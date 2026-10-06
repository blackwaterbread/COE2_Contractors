//! Operations pay instead of ending with COE2's exfil task:
//! AO generated -> operation tracked -> all tasks finished -> pay and result screen -> loot time -> return to base.
//! During the loot time players can return on their own; the AO ends when it runs out or no living player is left in
//! the AO. An operation ended early (the commander cancels the AO) still pays its completed tasks.
modded class COE_GameMode
{
	protected ref CTR_Operation m_CTR_Operation;
	protected ref CTR_Settlement m_CTR_Settlement;
	//! Set while the AO ends: players in vehicles ride along and their vehicles are kept.
	protected bool m_bCTR_Returning;
	//! Vehicles kept when the AO ends (weak: engine entities).
	protected ref array<IEntity> m_aCTR_ReturningVehicles = {};
	//! Vehicles their drivers took back to base during the loot time (weak); kept when the AO ends.
	protected ref array<IEntity> m_aCTR_ReturnedVehicles = {};
	//! Tick when the loot time ends; 0 = no loot time.
	protected int m_iCTR_ReturnTick;

	//! How the pay currency is shown on every machine; storage and the ledger keep the Marx currency ID.
	protected static const string CTR_CURRENCY_FORMAT = "$%1";
	protected static const int CTR_LOOT_CHECK_MS = 2000;
	protected static const float CTR_RETURN_SEARCH_RADIUS = 10;

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		MRX_TextFormat.SetCurrencyFormat(CTR_Settings.Get().m_sCurrency, CTR_CURRENCY_FORMAT);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnGameStart()
	{
		super.OnGameStart();

		if (!Replication.IsServer())
			return;

		CTR_CheckSystems();
		Print(string.Format("[CTR] %1 base arsenals switched off", CTR_BaseArsenals.DisableAll()));
		Print(string.Format("[CTR] Loadout prices from %1 shops", CTR_LoadoutPrices.Setup()));
	}

	//------------------------------------------------------------------------------------------------
	//! Every role respawns with the same minimal kit (not when the Game Master possesses a character). Applied before
	//! the player takes control: replacing the role's weapon later leaves the character with nothing in hand.
	override bool PreparePlayerEntity_S(SCR_SpawnRequestComponent requestComponent, SCR_SpawnHandlerComponent handlerComponent, SCR_SpawnData data, IEntity entity)
	{
		if (!super.PreparePlayerEntity_S(requestComponent, handlerComponent, data, entity))
			return false;

		if (entity && !SCR_PossessSpawnData.Cast(data))
			CTR_StarterKit.Apply(entity, CTR_Settings.Get().GetStarterKit());

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! COE2 and Marx both register a system through the vanilla systems config; log whether both run.
	protected void CTR_CheckSystems()
	{
		ChimeraWorld world = GetGame().GetWorld();
		bool enemySupport = world && world.FindSystem(COE_EnemySupportSystem) != null;
		bool marx = MRX_MarxSystem.GetInstance() != null;
		Print(string.Format("[CTR] Systems config %1: COE_EnemySupportSystem=%2, MRX_MarxSystem=%3", GetGame().GetSystemsConfig(), enemySupport, marx));

		if (!enemySupport || !marx)
			Print("[CTR] A required system is missing; check the World Systems Config", LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	CTR_Operation CTR_GetOperation()
	{
		return m_CTR_Operation;
	}

	//------------------------------------------------------------------------------------------------
	CTR_Settlement CTR_GetSettlement()
	{
		return m_CTR_Settlement;
	}

	//------------------------------------------------------------------------------------------------
	bool CTR_IsReturning()
	{
		return m_bCTR_Returning;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: seconds of loot time left, 0 when there is none.
	int CTR_GetReturnSecondsLeft()
	{
		if (m_iCTR_ReturnTick == 0)
			return 0;

		return Math.Max(1, Math.Ceil((m_iCTR_ReturnTick - System.GetTickCount()) / 1000.0));
	}

	//------------------------------------------------------------------------------------------------
	override protected void GenerateAO()
	{
		super.GenerateAO();
		CTR_StartOperation();
	}

	//------------------------------------------------------------------------------------------------
	protected void CTR_StartOperation()
	{
		CTR_StopLootTime();
		if (m_CTR_Operation && !m_CTR_Operation.IsClosed())
		{
			Print(string.Format("[CTR] Operation %1 replaced by a new AO before it ended; it pays nothing", m_CTR_Operation.GetId()), LogLevel.WARNING);
			m_CTR_Operation.Close();
		}

		m_CTR_Operation = new CTR_Operation(MRX_Marx.NewId());
		m_CTR_Operation.StartTracking();
		Print(string.Format("[CTR] Operation %1 started (%2 AO)", m_CTR_Operation.GetId(), m_aCurrentAOs.Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces COE2's exfil task: the operation ends and pays when every task of every AO is finished.
	override protected void OnAOFinished(COE_AO completedAO)
	{
		if (!m_CTR_Operation || m_CTR_Operation.IsClosed())
		{
			super.OnAOFinished(completedAO);
			return;
		}

		foreach (COE_AO ao : m_aCurrentAOs)
		{
			if (ao && !ao.AreAllTasksFinished())
				return;
		}

		CTR_EndOperation(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Every end of the AO (loot time over, nobody left in it, the commander cancels it): vehicles carrying players come
	//! along with their crew and are kept, like the vehicles taken back during the loot time; COE2 deletes the others.
	//! Ending the AO before its tasks are finished ends the operation early: its completed tasks still pay.
	override protected void DeleteAO()
	{
		if (m_CTR_Operation && !m_CTR_Operation.IsClosed())
			CTR_EndOperation(false);

		CTR_StopLootTime();
		m_bCTR_Returning = true;
		m_aCTR_ReturningVehicles.Clear();
		CTR_ReturnTrip.CollectOccupiedVehicles(m_aCTR_ReturningVehicles);
		int moved = CTR_ReturnTrip.MoveVehicles(m_aCTR_ReturningVehicles, m_vMainBasePos);
		foreach (IEntity vehicle : m_aCTR_ReturnedVehicles)
		{
			if (vehicle && !m_aCTR_ReturningVehicles.Contains(vehicle))
				m_aCTR_ReturningVehicles.Insert(vehicle);
		}

		super.DeleteAO();
		m_bCTR_Returning = false;
		m_aCTR_ReturningVehicles.Clear();
		m_aCTR_ReturnedVehicles.Clear();
		if (moved > 0)
			Print(string.Format("[CTR] AO ended: %1 vehicles moved to the base with their crew", moved));
	}

	//------------------------------------------------------------------------------------------------
	protected void CTR_EndOperation(bool finished)
	{
		m_CTR_Operation.Close();

		array<ref CTR_AreaInfo> areas = {};
		array<ref CTR_TaskOutcome> tasks = {};
		CTR_CollectOutcomes(areas, tasks);

		m_CTR_Settlement = new CTR_Settlement(CTR_Settings.Get(), m_CTR_Operation.GetId(), finished, m_CTR_Operation.GetDurationSeconds(), areas, tasks);
		m_CTR_Settlement.AddParticipants(m_CTR_Operation.GetParticipants());
		m_CTR_Settlement.GetOnDone().Insert(CTR_OnSettled);
		m_CTR_Settlement.Pay(MRX_Marx.GetEconomy());

		Print(string.Format("[CTR] Operation %1 %2: %3 of %4 tasks completed", m_CTR_Operation.GetId(), CTR_GetEndWord(finished), CTR_CountCompleted(tasks), tasks.Count()));
	}

	//------------------------------------------------------------------------------------------------
	protected static string CTR_GetEndWord(bool finished)
	{
		if (finished)
			return "finished";

		return "ended early";
	}

	//------------------------------------------------------------------------------------------------
	protected static int CTR_CountCompleted(array<ref CTR_TaskOutcome> tasks)
	{
		int count;
		foreach (CTR_TaskOutcome task : tasks)
		{
			if (task.m_bCompleted)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected void CTR_CollectOutcomes(notnull array<ref CTR_AreaInfo> areas, notnull array<ref CTR_TaskOutcome> tasks)
	{
		foreach (COE_AO ao : m_aCurrentAOs)
		{
			if (!ao)
				continue;

			vector center = ao.GetOrigin();
			CTR_AreaInfo area = new CTR_AreaInfo();
			area.m_sName = ao.CTR_GetLocationName();
			area.m_fX = center[0];
			area.m_fZ = center[2];
			area.m_fRadius = m_fAORadius;
			areas.Insert(area);

			array<KSC_BaseTask> aoTasks = {};
			ao.CTR_GetTasks(aoTasks);
			foreach (KSC_BaseTask task : aoTasks)
			{
				vector pos = task.GetOrigin();
				CTR_TaskOutcome outcome = new CTR_TaskOutcome();
				outcome.m_sType = task.CTR_GetBuilderClass();
				outcome.m_sTaskPrefab = task.CTR_GetTaskPrefab();
				outcome.m_sName = task.CTR_GetTaskName();
				if (outcome.m_sName.IsEmpty())
					outcome.m_sName = task.ClassName();

				outcome.m_fX = pos[0];
				outcome.m_fZ = pos[2];
				outcome.m_bCompleted = task.GetTaskState() == SCR_ETaskState.COMPLETED;
				outcome.m_bFailed = task.GetTaskState() == SCR_ETaskState.FAILED;
				tasks.Insert(outcome);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CTR_OnSettled(CTR_Settlement settlement)
	{
		int returnDelay;
		if (settlement.IsFinished())
			returnDelay = CTR_Settings.Get().m_iReturnDelaySeconds;

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			COE_PlayerController controller = COE_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
			if (controller)
				controller.CTR_SendOperationResult(settlement.BuildResult(playerId, returnDelay).ToJson());
		}

		foreach (CTR_PayEntry entry : settlement.GetEntries())
		{
			Print(string.Format("[CTR] Operation %1 pay: %2 (%3) total=%4 status=%5 entered=%6", settlement.GetOperationId(), entry.m_Stats.m_sName, entry.m_sOwnerId, entry.m_Payout.m_iTotal, typename.EnumToString(CTR_EPayStatus, entry.m_eStatus), entry.m_Stats.m_bEnteredAO));
		}

		if (!settlement.IsFinished())
			return;

		// The commander may have ended the AO or started the next one while the pay was committed.
		if (!m_CTR_Operation || m_CTR_Operation.GetId() != settlement.GetOperationId() || m_eCOE_CurrentState != COE_EGameModeState.EXECUTION)
			return;

		if (returnDelay > 0)
			CTR_StartLootTime(returnDelay);
		else
			CTR_ReturnToBase("no loot time");
	}

	//------------------------------------------------------------------------------------------------
	protected void CTR_StartLootTime(int seconds)
	{
		m_iCTR_ReturnTick = System.GetTickCount() + seconds * 1000;
		GetGame().GetCallqueue().Remove(CTR_UpdateLootTime);
		GetGame().GetCallqueue().CallLater(CTR_UpdateLootTime, CTR_LOOT_CHECK_MS, true);
		Print(string.Format("[CTR] Operation %1: loot time %2 s", m_CTR_Operation.GetId(), seconds));
	}

	//------------------------------------------------------------------------------------------------
	protected void CTR_StopLootTime()
	{
		m_iCTR_ReturnTick = 0;
		GetGame().GetCallqueue().Remove(CTR_UpdateLootTime);
	}

	//------------------------------------------------------------------------------------------------
	protected void CTR_UpdateLootTime()
	{
		if (System.GetTickCount() >= m_iCTR_ReturnTick)
			CTR_ReturnToBase("loot time over");
		else if (!CTR_IsAnyPlayerInAO())
			CTR_ReturnToBase("nobody left in the AO");
	}

	//------------------------------------------------------------------------------------------------
	//! Ends the AO the way COE2 does after exfil; see DeleteAO for the vehicles.
	protected void CTR_ReturnToBase(string reason)
	{
		CTR_StopLootTime();
		if (m_eCOE_CurrentState != COE_EGameModeState.EXECUTION)
			return;

		Print(string.Format("[CTR] Returning everyone to base: %1", reason));
		ExecuteCommanderRequest(COE_ECommanderRequest.CANCEL_AO);
	}

	//------------------------------------------------------------------------------------------------
	//! Unconscious players count: nobody is left behind.
	protected bool CTR_IsAnyPlayerInAO()
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(playerManager.GetPlayerControlledEntity(playerId));
			if (!character || character.GetCharacterController().GetLifeState() == ECharacterLifeState.DEAD)
				continue;

			if (CTR_Operation.IsInAnyAO(character.GetOrigin(), m_aCurrentAOs, m_fAORadius))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: takes a player back to base during the loot time and heals them, as the end of the AO does. A driver takes
	//! the vehicle and everyone in it along.
	CTR_EReturnStatus CTR_ReturnNow(int playerId)
	{
		if (m_iCTR_ReturnTick == 0 || m_eCOE_CurrentState != COE_EGameModeState.EXECUTION)
			return CTR_EReturnStatus.NOT_NOW;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId));
		if (!character || character.GetCharacterController().GetLifeState() == ECharacterLifeState.DEAD)
			return CTR_EReturnStatus.DEAD;

		if (character.IsInVehicle())
			return CTR_ReturnVehicle(character);

		if (CTR_ReturnTrip.IsAtBase(character.GetOrigin(), m_vMainBasePos))
			return CTR_EReturnStatus.AT_BASE;

		// COE2's fast travel (fade to black) moves the main entity, which only a character spawned through the respawn
		// system has; others are moved directly.
		COE_PlayerController controller = COE_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (controller && controller.CTR_HasMainEntity())
			controller.RequestFastTravel(m_vMainBasePos, 0, CTR_RETURN_SEARCH_RADIUS);
		else if (!CTR_TeleportToBase(character))
			return CTR_EReturnStatus.FAILED;

		CTR_Heal(character);
		Print(string.Format("[CTR] %1 returned to base", GetGame().GetPlayerManager().GetPlayerName(playerId)));
		return CTR_EReturnStatus.OK;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CTR_TeleportToBase(notnull SCR_ChimeraCharacter character)
	{
		SCR_EditableCharacterComponent editable = SCR_EditableCharacterComponent.Cast(character.FindComponent(SCR_EditableCharacterComponent));
		if (!editable)
			return false;

		vector pos = m_vMainBasePos;
		SCR_WorldTools.FindEmptyTerrainPosition(pos, m_vMainBasePos, CTR_RETURN_SEARCH_RADIUS);
		vector transform[4];
		KSC_GameTools.GetTransformFromPosAndRot(transform, pos, character.GetYawPitchRoll()[0]);
		return editable.SetTransform(transform);
	}

	//------------------------------------------------------------------------------------------------
	protected static void CTR_Heal(notnull SCR_ChimeraCharacter character)
	{
		SCR_DamageManagerComponent damageManager = character.GetDamageManager();
		if (damageManager)
			damageManager.FullHeal();
	}

	//------------------------------------------------------------------------------------------------
	protected CTR_EReturnStatus CTR_ReturnVehicle(notnull SCR_ChimeraCharacter driver)
	{
		if (!CTR_ReturnTrip.IsDriver(driver))
			return CTR_EReturnStatus.NOT_DRIVER;

		IEntity vehicle = CTR_ReturnTrip.GetVehicle(driver);
		if (!vehicle)
			return CTR_EReturnStatus.FAILED;

		if (CTR_ReturnTrip.IsAtBase(vehicle.GetOrigin(), m_vMainBasePos))
			return CTR_EReturnStatus.AT_BASE;

		if (!CTR_ReturnTrip.MoveVehicle(vehicle, m_vMainBasePos))
			return CTR_EReturnStatus.FAILED;

		if (!m_aCTR_ReturnedVehicles.Contains(vehicle))
			m_aCTR_ReturnedVehicles.Insert(vehicle);

		array<SCR_ChimeraCharacter> crew = {};
		CTR_ReturnTrip.CollectPlayersIn(vehicle, crew);
		foreach (SCR_ChimeraCharacter member : crew)
		{
			CTR_Heal(member);
		}

		Print(string.Format("[CTR] %1 returned to base with %2 players", vehicle, crew.Count()));
		return CTR_EReturnStatus.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the operation screen of a player. While the operation runs: what they would get if it ended now. During
	//! the loot time: the result, with the loot time left. Null otherwise.
	CTR_OperationResult CTR_BuildStatus(int playerId)
	{
		if (m_CTR_Operation && !m_CTR_Operation.IsClosed())
		{
			array<ref CTR_AreaInfo> areas = {};
			array<ref CTR_TaskOutcome> tasks = {};
			CTR_CollectOutcomes(areas, tasks);
			CTR_Settlement status = CTR_Settlement.CreateInProgress(CTR_Settings.Get(), m_CTR_Operation.GetId(), m_CTR_Operation.GetDurationSeconds(), areas, tasks);
			status.AddParticipants(m_CTR_Operation.GetParticipants());
			return status.BuildResult(playerId, 0);
		}

		if (m_iCTR_ReturnTick == 0 || !m_CTR_Operation || !m_CTR_Settlement || !m_CTR_Settlement.IsDone())
			return null;

		if (m_CTR_Settlement.GetOperationId() != m_CTR_Operation.GetId())
			return null;

		return m_CTR_Settlement.BuildResult(playerId, CTR_GetReturnSecondsLeft());
	}

	//------------------------------------------------------------------------------------------------
	//! When the AO ends, vehicles carrying players and those taken back during the loot time stay; COE2 deletes the others.
	override void CollectBuiltEntitiesForCleanUp()
	{
		super.CollectBuiltEntitiesForCleanUp();
		if (!m_bCTR_Returning)
			return;

		foreach (IEntity vehicle : m_aCTR_ReturningVehicles)
		{
			m_aEntitiesToDelete.RemoveItem(vehicle);
		}
	}
}
