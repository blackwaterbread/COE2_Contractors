#ifdef WORKBENCH
//! End-to-end operations in the running COE2 world. Need a fresh game (intermission) and the host player.
class CTR_OperationFlowTests
{
	//------------------------------------------------------------------------------------------------
	static void Register(notnull CTR_TestRunner runner)
	{
		runner.Add(new CTR_Test_OperationFlow());
		runner.Add(new CTR_Test_VehicleExfil());
		runner.Add(new CTR_Test_CancelBeforeExfil());
		runner.Add(new CTR_Test_ExfilAbandoned());
		runner.Add(new CTR_Test_EarlyExfil());
		runner.Add(new CTR_Test_EarlyExfilTasksDone());
		runner.Add(new CTR_Test_AllTasksFailed());
		// Last: the host dies.
		runner.Add(new CTR_Test_ExfilMissing());
	}
}

//------------------------------------------------------------------------------------------------
//! Puts an exfil point, generates an AO, moves the host into it, checks the live operation screen, completes every task:
//! the exfil starts (countdown, timer, exfil point locked). The host goes to the exfil point and holds it: the operation
//! pays in full, everyone returns, then the result screen arrives.
class CTR_Test_OperationFlow : CTR_TestCase
{
	protected static const float MIN_BASE_DISTANCE = 800;
	protected static const int WAIT_MS = 500;
	protected static const int MAX_OWNER_WAIT_MS = 20000;
	//! Short, so the tests do not wait for the shipped values.
	protected static const int TEST_HOLD_SECONDS = 2;
	protected static const int TEST_CANCEL_SECONDS = 4;
	//! COE2 removes the AO, its insertion and exfil points 3 s after it ends; the next test must not start before.
	protected static const int AFTER_RETURN_MS = 4500;
	//! Shipped values, put back after each test.
	protected static int s_iConfiguredHoldSeconds = -1;
	protected static int s_iConfiguredCancelSeconds = -1;

	protected COE_GameMode m_GameMode;
	protected int m_iPlayerId;
	protected int m_iWaitedMs;
	protected int m_iExpectedPay;
	protected int m_iResults;
	protected ref CTR_OperationResult m_Result;
	protected vector m_vExfil;

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return 90000;
	}

	//------------------------------------------------------------------------------------------------
	//! Clear Area tasks in the AO.
	protected int GetTaskCount()
	{
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		CTR_Settings settings = CTR_Settings.Get();
		if (s_iConfiguredHoldSeconds < 0)
		{
			s_iConfiguredHoldSeconds = settings.m_iExfilHoldSeconds;
			s_iConfiguredCancelSeconds = settings.m_iCancelReturnSeconds;
		}

		settings.m_iExfilHoldSeconds = TEST_HOLD_SECONDS;
		settings.m_iCancelReturnSeconds = TEST_CANCEL_SECONDS;
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
			vector exfilPos;
			if (candidate && vector.DistanceXZ(candidate.m_vCenter, m_GameMode.GetMainBasePos()) > MIN_BASE_DISTANCE && CTR_DevTools.FindExfilPos(candidate.m_vCenter, m_GameMode.GetAORadius(), exfilPos))
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
			if (builder.ClassName() != "COE_ClearAreaTaskBuilder")
				continue;

			for (int i = 0; i < GetTaskCount(); i++)
			{
				builders.Insert(builder);
			}
		}

		Check(!builders.IsEmpty(), "clear area builder available");
		COE_AOParams params = new COE_AOParams();
		params.SetLocation(location);
		params.SetTaskBuilders(builders);
		array<ref COE_AOParams> nextParams = {params};
		m_GameMode.SetNextAOParams(nextParams);
		Check(CTR_DevTools.PlaceExfilPoint(location.m_vCenter), "exfil point placed");
		Check(m_GameMode.CTR_GetExfilPointPos(m_vExfil), "exfil point known");

		COE_PlayerController.CTR_GetOnOperationResult().Insert(OnResult);
		m_GameMode.ExecuteCommanderRequest(COE_ECommanderRequest.GENERATE_AO);
		Print(CTR_TestRunner.TAG + ClassName() + ": AO generated at " + location.m_sName);
		GetGame().GetCallqueue().CallLater(EnterAO, 3000);
	}

	//------------------------------------------------------------------------------------------------
	protected void EnterAO()
	{
		CTR_Operation operation = m_GameMode.CTR_GetOperation();
		Check(operation && !operation.IsClosed(), "operation running");
		Check(m_GameMode.CTR_HasOperation() && !m_GameMode.CTR_IsInExfil(), "operation replicated, no exfil yet");

		array<KSC_BaseTask> tasks = {};
		GetTasks(tasks);
		if (tasks.IsEmpty())
		{
			Check(false, "tasks built");
			Cleanup();
			return;
		}

		if (tasks.Count() < GetTaskCount())
		{
			m_sSkipReason = string.Format("%1 tasks built, %2 needed", tasks.Count(), GetTaskCount());
			Cleanup();
			return;
		}

		foreach (KSC_BaseTask task : tasks)
		{
			CheckString(task.CTR_GetBuilderClass(), "COE_ClearAreaTaskBuilder", "task knows its builder");
			Check(!task.CTR_GetTaskName().IsEmpty(), "task knows its name");
			m_iExpectedPay += CTR_Settings.Get().GetTaskReward(task.CTR_GetBuilderClass(), task.CTR_GetTaskPrefab());
		}

		// Inside the AO, but at its edge, away from the defenders.
		vector pos = m_GameMode.GetCurrentAOs()[0].GetOrigin() + Vector(m_GameMode.GetAORadius() - 40, 0, 0);
		pos[1] = SCR_TerrainHelper.GetTerrainY(pos);
		Teleport(pos);
		OnEnteredAO(pos);
		GetGame().GetCallqueue().CallLater(InAO, 4500);
	}

	//------------------------------------------------------------------------------------------------
	protected void GetTasks(notnull array<KSC_BaseTask> outTasks)
	{
		foreach (COE_AO ao : m_GameMode.GetCurrentAOs())
		{
			if (ao)
				ao.CTR_GetTasks(outTasks);
		}
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
	//! The host is in the AO: checks the live screen, then ends the tasks.
	protected void InAO()
	{
		CTR_Operation operation = m_GameMode.CTR_GetOperation();
		CTR_Participant participant;
		if (operation)
			participant = operation.GetParticipants().Get(m_iPlayerId);

		Check(participant && participant.m_Stats.m_bEnteredAO, "entering the AO was tracked");

		CTR_OperationResult status = m_GameMode.CTR_BuildStatus(m_iPlayerId);
		Check(status && status.m_bInProgress && !status.m_bExfil, "live operation screen before the exfil");
		if (status)
		{
			Check(status.m_Stats.m_bEnteredAO, "live: entered the AO");
			CheckInt(status.CountCompletedTasks(), 0, "live: no task completed yet");
			CheckInt(status.m_Payout.m_iTotal, 0, "live: nothing so far before a task is completed");
			CheckInt(status.m_iTotalIfSuccess, m_iExpectedPay, "live: pay if the exfil succeeds");
		}

		CheckTimerRow("operation", true);
		CheckTimerRow("exfil", false);
		EndTasks();
	}

	//------------------------------------------------------------------------------------------------
	//! Completes every task: the exfil starts.
	protected void EndTasks()
	{
		SetTaskStates(SCR_ETaskState.COMPLETED, -1);
		GetGame().GetCallqueue().CallLater(OnExfilStarted, 1500);
	}

	//------------------------------------------------------------------------------------------------
	//! \param count Tasks to change, -1 for all.
	protected void SetTaskStates(SCR_ETaskState state, int count)
	{
		array<KSC_BaseTask> tasks = {};
		GetTasks(tasks);
		foreach (KSC_BaseTask task : tasks)
		{
			SCR_ETaskState current = task.GetTaskState();
			if (count == 0 || current == SCR_ETaskState.COMPLETED || current == SCR_ETaskState.FAILED)
				continue;

			task.SetTaskState(state);
			count--;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnExfilStarted()
	{
		CheckExfil();
		DuringExfil();
	}

	//------------------------------------------------------------------------------------------------
	//! The exfil runs: countdown, timer rows, the live screen counts only completed tasks, the exfil point is locked.
	protected void CheckExfil()
	{
		Check(m_GameMode.COE_GetState() == COE_EGameModeState.EXECUTION, "AO stays during the exfil");
		Check(m_GameMode.CTR_GetExfil() != null && m_GameMode.CTR_IsInExfil(), "exfil started");
		WorldTimestamp deadline = m_GameMode.CTR_GetExfilDeadline();
		float left = COE_GameMode.CTR_SecondsUntil(deadline);
		Check(deadline && left > CTR_Settings.Get().m_iExfilCountdownSeconds - 10 && left <= CTR_Settings.Get().m_iExfilCountdownSeconds, "exfil countdown: " + left);

		int present, outside, needed;
		m_GameMode.CTR_GetExfilCount(present, outside, needed);
		CheckInt(outside, 1, "host outside the base counts");
		CheckInt(needed, 1, "one player needed");

		CTR_OperationResult status = m_GameMode.CTR_BuildStatus(m_iPlayerId);
		Check(status && status.m_bInProgress && status.m_bExfil, "live operation screen during the exfil");

		CheckTimerRow("exfil", true);
		CheckTimerRow("present", true);

		// The commander cannot move the exfil point any more.
		m_GameMode.ExecuteCommanderRequest(COE_ECommanderRequest.EXFIL_POINT, m_vExfil + "300 0 300");
		vector exfil;
		m_GameMode.CTR_GetExfilPointPos(exfil);
		Check(vector.DistanceXZ(exfil, m_vExfil) < 1, "exfil point locked during the exfil");
	}

	//------------------------------------------------------------------------------------------------
	//! Goes to the exfil point and holds it.
	protected void DuringExfil()
	{
		GoToExfil();
		GetGame().GetCallqueue().CallLater(CheckHolding, 2600);
	}

	//------------------------------------------------------------------------------------------------
	protected void GoToExfil()
	{
		vector pos;
		Check(CTR_DevTools.GetExfilPos(pos), "exfil point to go to");
		Teleport(pos);
	}

	//------------------------------------------------------------------------------------------------
	//! Between the first count at the exfil point and the end of the hold.
	protected void CheckHolding()
	{
		if (m_GameMode.COE_GetState() != COE_EGameModeState.EXECUTION)
			return;

		int present, outside, needed;
		m_GameMode.CTR_GetExfilCount(present, outside, needed);
		CheckInt(present, 1, "host counted at the exfil point");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckTimerRow(string row, bool shown)
	{
		COE_PlayerController controller = COE_PlayerController.Cast(GetGame().GetPlayerController());
		CTR_OperationTimerHud hud;
		if (controller)
			hud = controller.CTR_GetTimerHud();

		Check(hud != null, "operation timer on the HUD");
		if (!hud)
			return;

		string text = hud.GetRowText(row);
		if (shown)
			Check(!text.IsEmpty(), "timer row shown: " + row);
		else
			Check(text.IsEmpty(), "timer row hidden: " + row + " = " + text);
	}

	//------------------------------------------------------------------------------------------------
	//! The result screen comes once after the AO ended.
	protected void OnResult(CTR_OperationResult result)
	{
		m_iResults++;
		if (m_iResults > 1)
		{
			Check(false, "only one result");
			return;
		}

		m_Result = result;
		Check(m_GameMode.COE_GetState() == COE_EGameModeState.INTERMISSION, "result after the AO ended");
		Check(!result.m_bInProgress, "result of an ended operation");
		CheckInt(result.m_aAreas.Count(), 1, "one AO in the result");
		Check(result.m_Stats.m_bEnteredAO, "result: entered the AO");
		CheckResult(result);
		Print(CTR_TestRunner.TAG + ClassName() + ": result " + result.ToJson());

		// The screen opens right after this event.
		GetGame().GetCallqueue().CallLater(CheckResultScreen, WAIT_MS);
		GetGame().GetCallqueue().CallLater(AfterReturn, AFTER_RETURN_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckResultScreen()
	{
		CTR_ResultDialog dialog = CTR_ResultDialog.GetOpen();
		Check(dialog && dialog.GetResult() == m_Result, "result screen open");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckResult(notnull CTR_OperationResult result)
	{
		CheckInt(result.m_eEnd, CTR_EOperationEnd.COMPLETE, "operation complete");
		CheckInt(result.m_iPayPercent, 100, "paid in full");
		CheckInt(result.CountCompletedTasks(), result.m_aTasks.Count(), "all tasks completed");
		CheckInt(result.m_Payout.m_iTasks, m_iExpectedPay, "task pay");
		Check(result.m_ePayStatus == CTR_EPayStatus.PAID, "pay committed, status " + typename.EnumToString(CTR_EPayStatus, result.m_ePayStatus));
		Check(result.m_bHasBalance, "balance known");
	}

	//------------------------------------------------------------------------------------------------
	protected void AfterReturn()
	{
		CTR_ResultDialog dialog = CTR_ResultDialog.GetOpen();
		if (dialog)
			dialog.Close();

		CheckReturnedHost();
		CheckTimerRow("operation", false);
		Cleanup();
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
		{
			m_GameMode.ExecuteCommanderRequest(COE_ECommanderRequest.CANCEL_AO);
			GetGame().GetCallqueue().CallLater(Finish, AFTER_RETURN_MS);
			return;
		}

		Finish();
	}

	//------------------------------------------------------------------------------------------------
	override protected void Finish()
	{
		if (s_iConfiguredHoldSeconds >= 0)
		{
			CTR_Settings.Get().m_iExfilHoldSeconds = s_iConfiguredHoldSeconds;
			CTR_Settings.Get().m_iCancelReturnSeconds = s_iConfiguredCancelSeconds;
		}

		super.Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! The host drives a vehicle to the exfil point; another vehicle stays empty in the AO. After the exfil the driven
//! vehicle is at the base with the host inside and kept; the empty one is deleted as in COE2.
class CTR_Test_VehicleExfil : CTR_Test_OperationFlow
{
	protected static const ResourceName VEHICLE = "{259EE7B78C51B624}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469.et";

	protected IEntity m_Driven;
	protected IEntity m_LeftBehind;

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
	override protected void GoToExfil()
	{
		SCR_ChimeraCharacter character = GetCharacter();
		Check(character && character.IsInVehicle(), "host in the vehicle before the exfil");

		// Close to the exfil point: CTR_ReturnTrip.MoveVehicle may park up to 80 m away.
		vector pos;
		if (!SCR_WorldTools.FindEmptyTerrainPosition(pos, m_vExfil, 15, 4, 3))
			pos = m_vExfil;

		SCR_EditableVehicleComponent editable;
		if (m_Driven)
			editable = SCR_EditableVehicleComponent.Cast(m_Driven.FindComponent(SCR_EditableVehicleComponent));

		vector transform[4];
		KSC_GameTools.GetTransformFromPosAndRot(transform, pos, 0);
		Check(editable && editable.SetTransform(transform), "vehicle at the exfil point");
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
//! The commander cancels before the exfil (through the commander's request, as the menu and the base board send it): the
//! pay is settled at once at a quarter, the AO stays for the return delay (a task finished meanwhile starts no exfil),
//! then everyone returns.
class CTR_Test_CancelBeforeExfil : CTR_Test_OperationFlow
{
	//------------------------------------------------------------------------------------------------
	override protected void EndTasks()
	{
		RequestCancel();
		GetGame().GetCallqueue().CallLater(DuringCancel, 1000);
	}

	//------------------------------------------------------------------------------------------------
	protected void RequestCancel()
	{
		if (COE_EditorModeCommanderEntity.GetInstance())
		{
			COE_EditorModeCommanderEntity.Request(COE_ECommanderRequest.CANCEL_AO);
			return;
		}

		Print(CTR_TestRunner.TAG + ClassName() + ": host has no commander editor mode, cancelling directly", LogLevel.WARNING);
		m_GameMode.CTR_RequestCancel();
	}

	//------------------------------------------------------------------------------------------------
	protected void DuringCancel()
	{
		Check(m_GameMode.COE_GetState() == COE_EGameModeState.EXECUTION, "AO stays during the cancel delay");
		Check(m_GameMode.CTR_IsOperationClosed(), "operation closed by the cancel");
		Check(m_GameMode.CTR_GetCancelReturn() != null, "return pending");
		CheckInt(CTR_AlertHud.GetShownAlert(), CTR_EAlert.CANCELLED, "cancel alert shown");
		CheckTimerRow("cancel", true);

		SetTaskStates(SCR_ETaskState.COMPLETED, -1);
		GetGame().GetCallqueue().CallLater(CheckNoExfil, 1000);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckNoExfil()
	{
		Check(!m_GameMode.CTR_IsInExfil(), "a task finished during the cancel delay starts no exfil");
	}

	//------------------------------------------------------------------------------------------------
	override protected void CheckResult(notnull CTR_OperationResult result)
	{
		CheckInt(result.m_eEnd, CTR_EOperationEnd.CANCELLED, "operation cancelled");
		CheckInt(result.m_iPayPercent, 25, "a quarter paid");
		CheckInt(result.CountCompletedTasks(), 0, "settled when cancelled, before the task was finished");
		CheckInt(result.m_Payout.m_iTotal, 0, "nothing completed, nothing paid");
	}
}

//------------------------------------------------------------------------------------------------
//! The commander cancels during the exfil: nothing is paid, nobody dies, everyone returns after the delay.
class CTR_Test_ExfilAbandoned : CTR_Test_CancelBeforeExfil
{
	//------------------------------------------------------------------------------------------------
	override protected void EndTasks()
	{
		SetTaskStates(SCR_ETaskState.COMPLETED, -1);
		GetGame().GetCallqueue().CallLater(OnExfilStarted, 1500);
	}

	//------------------------------------------------------------------------------------------------
	override protected void DuringExfil()
	{
		RequestCancel();
		GetGame().GetCallqueue().CallLater(CheckAbandoned, 1000);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckAbandoned()
	{
		Check(m_GameMode.CTR_IsOperationClosed() && !m_GameMode.CTR_IsInExfil(), "exfil stopped by the cancel");
		CheckTimerRow("exfil", false);
		CheckTimerRow("cancel", true);
	}

	//------------------------------------------------------------------------------------------------
	override protected void CheckResult(notnull CTR_OperationResult result)
	{
		CheckInt(result.m_eEnd, CTR_EOperationEnd.ABANDONED, "exfil abandoned");
		CheckInt(result.m_iPayPercent, 0, "nothing paid");
		CheckInt(result.CountCompletedTasks(), result.m_aTasks.Count(), "tasks completed");
		CheckInt(result.m_Payout.m_iTotal, 0, "total 0");
	}

	//------------------------------------------------------------------------------------------------
	override protected void CheckReturnedHost()
	{
		SCR_ChimeraCharacter character = GetCharacter();
		Check(character && character.GetCharacterController().GetLifeState() != ECharacterLifeState.DEAD, "nobody dies when the exfil is abandoned");
		super.CheckReturnedHost();
	}
}

//------------------------------------------------------------------------------------------------
//! The commander orders the early exfil (the injected commander menu command) with the task still open. Holding the
//! exfil point ends it as an early exfil; nothing was completed, so nothing is paid.
class CTR_Test_EarlyExfil : CTR_Test_OperationFlow
{
	//------------------------------------------------------------------------------------------------
	//! The task stays open.
	override protected void EndTasks()
	{
		OrderEarlyExfil();
	}

	//------------------------------------------------------------------------------------------------
	protected void OrderEarlyExfil()
	{
		Check(!m_GameMode.CTR_IsInExfil(), "no exfil while a task is open");
		SCR_CommandingManagerComponent commanding = SCR_CommandingManagerComponent.GetInstance();
		SCR_BaseRadialCommand command;
		if (commanding)
			command = commanding.FindCommand(CTR_EarlyExfilCommand.NAME);

		Check(command != null, "early exfil command added to the commander menu");
		SCR_ChimeraCharacter character = GetCharacter();
		if (command && character)
		{
			Check(command.CanBePerformed(character), "early exfil can be ordered");
			command.Execute(null, null, vector.Zero, m_iPlayerId, false);
		}
		else
		{
			m_GameMode.CTR_StartEarlyExfil();
		}

		GetGame().GetCallqueue().CallLater(OnExfilStarted, 500);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnExfilStarted()
	{
		CheckInt(CTR_AlertHud.GetShownAlert(), CTR_EAlert.EARLY_EXFIL, "early exfil alert shown");
		CTR_OperationResult status = m_GameMode.CTR_BuildStatus(m_iPlayerId);
		if (status)
			CheckInt(status.m_iTotalIfSuccess, 0, "live: pay if the exfil succeeds counts completed tasks only");

		super.OnExfilStarted();
	}

	//------------------------------------------------------------------------------------------------
	override protected void CheckResult(notnull CTR_OperationResult result)
	{
		CheckInt(result.m_eEnd, CTR_EOperationEnd.EARLY_EXFIL, "early exfil");
		CheckInt(result.m_iPayPercent, 100, "full share");
		CheckInt(result.CountCompletedTasks(), 0, "the task stayed open");
		CheckInt(result.m_Payout.m_iTotal, 0, "nothing completed, nothing paid");
	}
}

//------------------------------------------------------------------------------------------------
//! After the early exfil the open task is completed: the exfil goes on (no second start, same countdown), and holding
//! the exfil point completes the operation.
class CTR_Test_EarlyExfilTasksDone : CTR_Test_EarlyExfil
{
	protected WorldTimestamp m_Deadline;

	//------------------------------------------------------------------------------------------------
	override protected void DuringExfil()
	{
		m_Deadline = m_GameMode.CTR_GetExfilDeadline();
		SetTaskStates(SCR_ETaskState.COMPLETED, -1);
		GetGame().GetCallqueue().CallLater(AfterLastTask, 1000);
	}

	//------------------------------------------------------------------------------------------------
	protected void AfterLastTask()
	{
		WorldTimestamp deadline = m_GameMode.CTR_GetExfilDeadline();
		Check(m_GameMode.CTR_IsInExfil() && deadline && m_Deadline && deadline.Equals(m_Deadline), "the exfil goes on with the same countdown");
		super.DuringExfil();
	}

	//------------------------------------------------------------------------------------------------
	override protected void CheckResult(notnull CTR_OperationResult result)
	{
		CheckInt(result.m_eEnd, CTR_EOperationEnd.COMPLETE, "complete: every task finished at the exfil");
		CheckInt(result.CountCompletedTasks(), 1, "the task completed during the exfil");
		CheckInt(result.m_Payout.m_iTasks, m_iExpectedPay, "the task pays");
	}
}

//------------------------------------------------------------------------------------------------
//! Every task fails: no exfil, everyone returns at once, nothing is paid.
class CTR_Test_AllTasksFailed : CTR_Test_OperationFlow
{
	//------------------------------------------------------------------------------------------------
	override protected void EndTasks()
	{
		SetTaskStates(SCR_ETaskState.FAILED, -1);
		GetGame().GetCallqueue().CallLater(CheckNoExfil, WAIT_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckNoExfil()
	{
		Check(!m_GameMode.CTR_IsInExfil(), "no exfil when every task failed");
		Check(m_GameMode.CTR_GetOperation().IsClosed(), "operation over");
	}

	//------------------------------------------------------------------------------------------------
	override protected void CheckResult(notnull CTR_OperationResult result)
	{
		CheckInt(result.m_eEnd, CTR_EOperationEnd.FAILED, "operation failed");
		CheckInt(result.CountCompletedTasks(), 0, "no task completed");
		CheckInt(result.m_Payout.m_iTotal, 0, "nothing paid");
	}
}

//------------------------------------------------------------------------------------------------
//! The host stays in the AO and the exfil countdown runs out: missing in action, the host dies, nothing is paid, the AO
//! ends. Last test: the host is dead afterwards.
class CTR_Test_ExfilMissing : CTR_Test_OperationFlow
{
	//------------------------------------------------------------------------------------------------
	override protected void DuringExfil()
	{
		m_GameMode.CTR_SetExfilSecondsLeft(0);
		GetGame().GetCallqueue().CallLater(CheckKilled, 300);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckKilled()
	{
		SCR_ChimeraCharacter character = GetCharacter();
		Check(!character || character.GetCharacterController().GetLifeState() == ECharacterLifeState.DEAD, "host outside the base killed");
	}

	//------------------------------------------------------------------------------------------------
	override protected void CheckResult(notnull CTR_OperationResult result)
	{
		CheckInt(result.m_eEnd, CTR_EOperationEnd.MISSING, "missing in action");
		CheckInt(result.m_iPayPercent, 0, "nothing paid");
		CheckInt(result.m_Payout.m_iTotal, 0, "total 0");
		CheckInt(result.m_Stats.m_iDeaths, 0, "the death after the end does not count");
	}

	//------------------------------------------------------------------------------------------------
	override protected void CheckReturnedHost()
	{
		SCR_ChimeraCharacter character = GetCharacter();
		Check(!character || character.GetCharacterController().GetLifeState() == ECharacterLifeState.DEAD, "host stayed dead");
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
