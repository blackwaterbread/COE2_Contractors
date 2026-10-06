#ifdef WORKBENCH
//! End-to-end operation in the running COE2 world. Needs a fresh game (intermission) and the host player.
class CTR_OperationFlowTests
{
	//------------------------------------------------------------------------------------------------
	static void Register(notnull CTR_TestRunner runner)
	{
		runner.Add(new CTR_Test_OperationFlow());
		runner.Add(new CTR_Test_ReturnOnFoot());
		runner.Add(new CTR_Test_VehicleReturn());
		runner.Add(new CTR_Test_CancelInVehicle());
	}
}

//------------------------------------------------------------------------------------------------
//! Generates an AO, moves the host into it, checks the live operation screen, completes every task and checks pay,
//! result, the loot time and the return to base when it runs out.
class CTR_Test_OperationFlow : CTR_TestCase
{
	protected static const float MIN_BASE_DISTANCE = 800;
	protected static const int WAIT_MS = 500;
	protected static const int MAX_OWNER_WAIT_MS = 20000;
	//! Loot time of the shipped config, put back after each test.
	protected static int s_iConfiguredLootSeconds = -1;

	protected COE_GameMode m_GameMode;
	protected int m_iPlayerId;
	protected int m_iWaitedMs;
	protected int m_iExpectedPay;
	protected ref CTR_OperationResult m_Result;
	//! Weak: owned by the player controller.
	protected CTR_ReturnCountdownHud m_ReturnCountdown;

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return 90000;
	}

	//------------------------------------------------------------------------------------------------
	//! Short, so the test sees the AO end when the loot time runs out.
	protected int GetLootSeconds()
	{
		return 6;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		if (s_iConfiguredLootSeconds < 0)
			s_iConfiguredLootSeconds = CTR_Settings.Get().m_iReturnDelaySeconds;

		CTR_Settings.Get().m_iReturnDelaySeconds = GetLootSeconds();
		m_GameMode = COE_GameMode.GetInstance();
		if (!m_GameMode)
		{
			Skip("not a COE2 world");
			return;
		}

		if (m_GameMode.COE_GetState() != COE_EGameModeState.INTERMISSION)
		{
			Skip("an AO is already running");
			return;
		}

		m_iPlayerId = SCR_PlayerController.GetLocalPlayerId();
		WaitForOwner();
	}

	//------------------------------------------------------------------------------------------------
	protected void WaitForOwner()
	{
		if (!MRX_Marx.GetOwnerId(m_iPlayerId).IsEmpty() && GetCharacter())
		{
			// Workbench creates the host character directly; a real spawn sets it as the main entity.
			COE_PlayerController controller = COE_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(m_iPlayerId));
			if (controller && !controller.CTR_HasMainEntity())
				controller.SetInitialMainEntity(GetCharacter());

			GenerateAO();
			return;
		}

		m_iWaitedMs += WAIT_MS;
		if (m_iWaitedMs >= MAX_OWNER_WAIT_MS)
		{
			Skip("host has no owner ID or character");
			return;
		}

		GetGame().GetCallqueue().CallLater(WaitForOwner, WAIT_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_ChimeraCharacter GetCharacter()
	{
		return SCR_ChimeraCharacter.Cast(GetGame().GetPlayerManager().GetPlayerControlledEntity(m_iPlayerId));
	}

	//------------------------------------------------------------------------------------------------
	//! In a real game the commander picks the factions first; Workbench Play starts without them.
	protected void EnsureFactions()
	{
		COE_FactionManager factionManager = COE_FactionManager.Cast(GetGame().GetFactionManager());
		if (!factionManager)
			return;

		Faction player = factionManager.GetPlayerFaction();
		if (!player)
		{
			SCR_ChimeraCharacter character = GetCharacter();
			if (character)
				player = character.GetFaction();
		}

		if (!player)
			player = factionManager.GetFactionByKey(CTR_Factions.PLAYER);

		// The Contractors enemies, so the AO is built from RHS compositions.
		Faction enemy = factionManager.GetEnemyFaction();
		if (!enemy)
			enemy = factionManager.GetFactionByKey(CTR_Factions.ENEMY);

		if (enemy == player)
			enemy = factionManager.GetFactionByKey(CTR_Factions.PLAYER);

		Faction civilian = factionManager.GetCivilianFaction();
		if (!civilian)
			civilian = factionManager.GetFactionByKey(CTR_Factions.CIVILIAN);

		factionManager.CTR_TestSetFactions(player, enemy, civilian);
		Check(factionManager.GetPlayerFaction() && factionManager.GetEnemyFaction(), "factions set");
		if (factionManager.GetEnemyFaction())
			CheckString(factionManager.GetEnemyFaction().GetFactionKey(), CTR_Factions.ENEMY, "enemy faction");
	}

	//------------------------------------------------------------------------------------------------
	protected void GenerateAO()
	{
		EnsureFactions();

		KSC_Location location;
		foreach (KSC_Location candidate : m_GameMode.GetAvailableLocations())
		{
			if (candidate && vector.DistanceXZ(candidate.m_vCenter, m_GameMode.GetMainBasePos()) > MIN_BASE_DISTANCE)
			{
				location = candidate;
				break;
			}
		}

		if (!location)
		{
			Skip("no AO location far enough from the base");
			return;
		}

		array<COE_BaseTaskBuilder> builders = {};
		foreach (COE_BaseTaskBuilder builder : m_GameMode.GetAvailableTaskBuilders())
		{
			if (builder.ClassName() == "COE_ClearAreaTaskBuilder")
				builders.Insert(builder);
		}

		Check(!builders.IsEmpty(), "clear area builder available");
		COE_AOParams params = new COE_AOParams();
		params.SetLocation(location);
		params.SetTaskBuilders(builders);
		array<ref COE_AOParams> nextParams = {params};
		m_GameMode.SetNextAOParams(nextParams);

		COE_PlayerController.CTR_GetOnOperationResult().Insert(OnResult);
		m_GameMode.ExecuteCommanderRequest(COE_ECommanderRequest.GENERATE_AO);
		Print(CTR_TestRunner.TAG + "flow: AO generated at " + location.m_sName);
		GetGame().GetCallqueue().CallLater(EnterAO, 3000);
	}

	//------------------------------------------------------------------------------------------------
	protected void EnterAO()
	{
		CTR_Operation operation = m_GameMode.CTR_GetOperation();
		Check(operation && !operation.IsClosed(), "operation running");

		array<COE_AO> aos = m_GameMode.GetCurrentAOs();
		if (aos.IsEmpty() || !aos[0])
		{
			Check(false, "AO spawned");
			Cleanup();
			return;
		}

		array<KSC_BaseTask> tasks = {};
		aos[0].CTR_GetTasks(tasks);
		Check(!tasks.IsEmpty(), "tasks built");
		foreach (KSC_BaseTask task : tasks)
		{
			CheckString(task.CTR_GetBuilderClass(), "COE_ClearAreaTaskBuilder", "task knows its builder");
			Check(!task.CTR_GetTaskName().IsEmpty(), "task knows its name");
			m_iExpectedPay += CTR_Settings.Get().GetTaskReward(task.CTR_GetBuilderClass(), task.CTR_GetTaskPrefab());
		}

		// Inside the AO, but at its edge, away from the defenders.
		vector pos = aos[0].GetOrigin() + Vector(m_GameMode.GetAORadius() - 40, 0, 0);
		pos[1] = SCR_TerrainHelper.GetTerrainY(pos);
		Teleport(pos);
		OnEnteredAO(pos);
		GetGame().GetCallqueue().CallLater(CompleteTasks, 4500);
	}

	//------------------------------------------------------------------------------------------------
	//! Override to prepare more right after the host was moved into the AO.
	protected void OnEnteredAO(vector pos)
	{
	}

	//------------------------------------------------------------------------------------------------
	protected void Teleport(vector pos)
	{
		SCR_ChimeraCharacter character = GetCharacter();
		if (!character)
			return;

		SCR_EditableCharacterComponent editable = SCR_EditableCharacterComponent.Cast(character.FindComponent(SCR_EditableCharacterComponent));
		vector transform[4];
		KSC_GameTools.GetTransformFromPosAndRot(transform, pos, 0);
		if (editable)
			editable.SetTransform(transform);
	}

	//------------------------------------------------------------------------------------------------
	protected void CompleteTasks()
	{
		CTR_Operation operation = m_GameMode.CTR_GetOperation();
		CTR_Participant participant;
		if (operation)
			participant = operation.GetParticipants().Get(m_iPlayerId);

		Check(participant && participant.m_Stats.m_bEnteredAO, "entering the AO was tracked");

		CTR_OperationResult status = m_GameMode.CTR_BuildStatus(m_iPlayerId);
		Check(status && status.m_bInProgress, "live operation screen while it runs");
		if (status)
		{
			Check(status.m_Stats.m_bEnteredAO, "live: entered the AO");
			CheckInt(status.CountCompletedTasks(), 0, "live: no task completed yet");
			CheckInt(status.m_Payout.m_iTotal, 0, "live: nothing paid before a task is completed");
		}

		array<COE_AO> aos = m_GameMode.GetCurrentAOs();
		foreach (COE_AO ao : aos)
		{
			array<KSC_BaseTask> tasks = {};
			ao.CTR_GetTasks(tasks);
			foreach (KSC_BaseTask task : tasks)
			{
				task.SetTaskState(SCR_ETaskState.COMPLETED);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnResult(CTR_OperationResult result)
	{
		COE_PlayerController.CTR_GetOnOperationResult().Remove(OnResult);
		m_Result = result;

		Check(result.m_bFinished, "operation finished");
		Check(result.IsSuccess(), "operation succeeded");
		CheckInt(result.CountCompletedTasks(), result.m_aTasks.Count(), "all tasks completed");
		Check(result.m_Stats.m_bEnteredAO, "result: entered the AO");
		CheckInt(result.m_Payout.m_iTasks, m_iExpectedPay, "task pay");
		Check(result.m_Payout.m_iTotal > 0, "something paid");
		Check(result.m_ePayStatus == CTR_EPayStatus.PAID, "pay committed, status " + typename.EnumToString(CTR_EPayStatus, result.m_ePayStatus));
		Check(result.m_bHasBalance, "balance known");
		CheckInt(result.m_iReturnDelaySeconds, GetLootSeconds(), "loot time");
		CheckInt(result.m_aAreas.Count(), 1, "one AO in the result");
		Print(CTR_TestRunner.TAG + "flow: result " + result.ToJson());

		// The result screen and the countdown are created right after this event, the loot time starts after it.
		GetGame().GetCallqueue().CallLater(CloseResultScreen, WAIT_MS);
		GetGame().GetCallqueue().CallLater(DuringLootTime, WAIT_MS * 3);
	}

	//------------------------------------------------------------------------------------------------
	//! The host stays in the AO: everyone returns when the loot time runs out.
	protected void DuringLootTime()
	{
		CheckLootTime();
		GetGame().GetCallqueue().CallLater(CheckReturn, (GetLootSeconds() + 7) * 1000);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckLootTime()
	{
		Check(m_GameMode.COE_GetState() == COE_EGameModeState.EXECUTION, "AO stays during the loot time");
		CTR_OperationResult status = m_GameMode.CTR_BuildStatus(m_iPlayerId);
		Check(status && !status.m_bInProgress, "operation screen shows the result during the loot time");
		if (status)
			Check(status.m_iReturnDelaySeconds > 0 && status.m_iReturnDelaySeconds <= GetLootSeconds(), "result with the loot time left: " + status.m_iReturnDelaySeconds);
	}

	//------------------------------------------------------------------------------------------------
	protected void CloseResultScreen()
	{
		COE_PlayerController controller = COE_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
			m_ReturnCountdown = controller.CTR_GetReturnCountdown();

		Check(m_ReturnCountdown != null, "return countdown created");
		if (m_ReturnCountdown)
			Check(!m_ReturnCountdown.IsShown(), "countdown not on the HUD while the result screen is open");

		CTR_ResultDialog dialog = CTR_ResultDialog.GetOpen();
		Check(dialog != null, "result screen open");
		if (dialog)
			dialog.Close();

		// Closing the result screen moves the countdown to the HUD.
		GetGame().GetCallqueue().CallLater(CheckCountdownShown, WAIT_MS * 2);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckCountdownShown()
	{
		Check(m_ReturnCountdown && m_ReturnCountdown.IsShown(), "countdown on the HUD after the result screen closed");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckReturn()
	{
		Check(m_GameMode.COE_GetState() == COE_EGameModeState.INTERMISSION, "AO ended after the return");
		Check(!m_ReturnCountdown || !m_ReturnCountdown.IsShown(), "countdown gone after the return");
		CheckReturnedHost();
		Finish();
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckReturnedHost()
	{
		SCR_ChimeraCharacter character = GetCharacter();
		Check(character && vector.DistanceXZ(character.GetOrigin(), m_GameMode.GetMainBasePos()) < 60, "host back at the base");
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup()
	{
		COE_PlayerController.CTR_GetOnOperationResult().Remove(OnResult);
		if (m_GameMode.COE_GetState() != COE_EGameModeState.INTERMISSION)
			m_GameMode.ExecuteCommanderRequest(COE_ECommanderRequest.CANCEL_AO);

		Finish();
	}

	//------------------------------------------------------------------------------------------------
	override protected void Finish()
	{
		if (s_iConfiguredLootSeconds >= 0)
			CTR_Settings.Get().m_iReturnDelaySeconds = s_iConfiguredLootSeconds;

		super.Finish();
	}
}
//------------------------------------------------------------------------------------------------
//! The host returns on foot during the loot time: COE2's fast travel takes them to the base, and the AO ends right
//! away as nobody is left in it.
class CTR_Test_ReturnOnFoot : CTR_Test_OperationFlow
{
	//------------------------------------------------------------------------------------------------
	//! Long, so an end of the AO within the test comes from nobody being left in it.
	override protected int GetLootSeconds()
	{
		return 120;
	}

	//------------------------------------------------------------------------------------------------
	override protected void DuringLootTime()
	{
		CheckLootTime();
		COE_PlayerController controller = COE_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
			controller.CTR_RequestReturnNow();

		GetGame().GetCallqueue().CallLater(CheckReturn, 9000);
	}
}

//------------------------------------------------------------------------------------------------
//! Like the operation flow, but the host drives a vehicle in the AO and another vehicle stays empty there. During the
//! loot time the host returns now: the vehicle goes to the base with the host inside, the AO ends right away as nobody
//! is left in it, the driven vehicle is kept and the empty one is deleted as in COE2.
class CTR_Test_VehicleReturn : CTR_Test_OperationFlow
{
	protected static const ResourceName VEHICLE = "{259EE7B78C51B624}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et";

	protected IEntity m_Driven;
	protected IEntity m_LeftBehind;

	//------------------------------------------------------------------------------------------------
	//! Long, so an end of the AO within the test comes from nobody being left in it.
	override protected int GetLootSeconds()
	{
		return 120;
	}

	//------------------------------------------------------------------------------------------------
	override protected void DuringLootTime()
	{
		CheckLootTime();
		COE_PlayerController controller = COE_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
			controller.CTR_RequestReturnNow();

		GetGame().GetCallqueue().CallLater(CheckDrivenBack, WAIT_MS * 2);
		GetGame().GetCallqueue().CallLater(CheckReturn, 7000);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckDrivenBack()
	{
		Check(m_Driven && vector.DistanceXZ(m_Driven.GetOrigin(), m_GameMode.GetMainBasePos()) < 120, "returned now: vehicle at the base");
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnEnteredAO(vector pos)
	{
		m_Driven = KSC_GameTools.SpawnVehiclePrefab(VEHICLE, pos + Vector(0, 0, 8), 0);
		m_LeftBehind = KSC_GameTools.SpawnVehiclePrefab(VEHICLE, pos + Vector(0, 0, -20), 0);
		Check(m_Driven && m_LeftBehind, "vehicles spawned");

		GetGame().GetCallqueue().CallLater(Board, 1500);
	}

	//------------------------------------------------------------------------------------------------
	protected void Board()
	{
		SCR_ChimeraCharacter character = GetCharacter();
		if (!character || !m_Driven)
			return;

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(character.GetCompartmentAccessComponent());
		Check(access && access.MoveInVehicle(m_Driven, ECompartmentType.PILOT), "host gets in as driver");
	}

	//------------------------------------------------------------------------------------------------
	override protected void CompleteTasks()
	{
		SCR_ChimeraCharacter character = GetCharacter();
		Check(character && character.IsInVehicle(), "host in the vehicle before the end");
		super.CompleteTasks();
	}

	//------------------------------------------------------------------------------------------------
	override protected void CheckReturnedHost()
	{
		Check(m_Driven != null, "driven vehicle not deleted");
		Check(m_LeftBehind == null, "vehicle left in the AO deleted the COE2 way");
		if (m_Driven)
			Check(vector.DistanceXZ(m_Driven.GetOrigin(), m_GameMode.GetMainBasePos()) < 120, "driven vehicle at the base");

		SCR_ChimeraCharacter character = GetCharacter();
		Check(character && character.IsInVehicle(), "host still in the vehicle");
		if (character && m_Driven)
			Check(vector.DistanceXZ(character.GetOrigin(), m_Driven.GetOrigin()) < 10, "host came along with the vehicle");
	}
}

//------------------------------------------------------------------------------------------------
//! The commander ends the AO while the host drives a vehicle in it: the vehicle comes back with the host and is kept,
//! like at the end of a finished operation, and the empty one is deleted.
class CTR_Test_CancelInVehicle : CTR_Test_VehicleReturn
{
	//------------------------------------------------------------------------------------------------
	override protected void CompleteTasks()
	{
		SCR_ChimeraCharacter character = GetCharacter();
		Check(character && character.IsInVehicle(), "host in the vehicle before the end");
		m_GameMode.ExecuteCommanderRequest(COE_ECommanderRequest.CANCEL_AO);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnResult(CTR_OperationResult result)
	{
		COE_PlayerController.CTR_GetOnOperationResult().Remove(OnResult);
		Check(!result.m_bFinished, "operation ended early");
		CheckInt(result.m_iReturnDelaySeconds, 0, "no loot time when the commander ends the AO");

		CTR_ResultDialog dialog = CTR_ResultDialog.GetOpen();
		if (dialog)
			dialog.Close();

		GetGame().GetCallqueue().CallLater(CheckReturn, 6000);
	}
}

//------------------------------------------------------------------------------------------------
//! Test only: sets the factions without COE2's side effects (respawning every player, cancelling the AO).
modded class COE_FactionManager
{
	//------------------------------------------------------------------------------------------------
	void CTR_TestSetFactions(Faction player, Faction enemy, Faction civilian)
	{
		if (player)
		{
			m_pPlayerFaction = player;
			m_iPlayerFactionId = GetFactionIndex(player);
		}

		if (enemy)
		{
			m_pEnemyFaction = enemy;
			m_iEnemyFactionId = GetFactionIndex(enemy);
		}

		if (civilian)
		{
			m_pCivilianFaction = civilian;
			m_iCivilianFactionId = GetFactionIndex(civilian);
		}

		Replication.BumpMe();
	}
}
#endif
