//! Operations end with an exfil judged by Contractors, and pay:
//! AO generated -> operation tracked -> every task finished (or the commander orders an early exfil) -> exfil: hold the
//! exfil point before the countdown runs out -> pay -> everyone returns to base -> result screen.
//! - The countdown running out is missing in action: no pay, and every living player outside the base dies.
//! - The commander can cancel: part of the pay before the exfil, nothing during it; everyone returns after a short delay.
//! - When every task failed there is nothing to exfil for: everyone returns at once.
modded class COE_GameMode
{
	protected ref CTR_Operation m_CTR_Operation;
	protected ref CTR_Settlement m_CTR_Settlement;
	//! Server: gear players keep across sessions.
	protected ref CTR_LastGear m_CTR_LastGear;
	//! Server: the exfil of the running operation; null before it starts and after the operation ends.
	protected ref CTR_Exfil m_CTR_Exfil;
	//! Set while the AO ends: players in vehicles ride along and their vehicles are kept.
	protected bool m_bCTR_Returning;
	//! Vehicles kept when the AO ends (weak: engine entities).
	protected ref array<IEntity> m_aCTR_ReturningVehicles = {};
	//! Server: the result screens are sent once the pay is done and the AO has ended.
	protected bool m_bCTR_ResultPending;
	protected bool m_bCTR_AOEnded;
	//! Server: characters killed missing in action (weak); the AO ends once they are dead.
	protected ref array<IEntity> m_aCTR_Dying = {};
	protected int m_iCTR_DyingWaitedMs;

	// Replicated for the operation timer of every player. Times are server timestamps, the same on every machine.
	[RplProp()]
	protected bool m_bCTR_HasOperation;
	[RplProp()]
	protected bool m_bCTR_OperationClosed;
	[RplProp()]
	protected WorldTimestamp m_CTR_OperationStart;
	[RplProp()]
	protected WorldTimestamp m_CTR_OperationEnd;
	[RplProp()]
	protected bool m_bCTR_InExfil;
	[RplProp()]
	protected WorldTimestamp m_CTR_ExfilDeadline;
	[RplProp()]
	protected int m_iCTR_ExfilPresent;
	[RplProp()]
	protected int m_iCTR_ExfilOutside;
	[RplProp()]
	protected int m_iCTR_ExfilNeeded;
	[RplProp()]
	protected bool m_bCTR_ExfilHolding;
	[RplProp()]
	protected WorldTimestamp m_CTR_ExfilHoldEnd;
	//! The exfil point cannot move once the exfil started, until the AO ends.
	[RplProp()]
	protected bool m_bCTR_ExfilPointLocked;
	//! Where the exfil point is (COE2 keeps it in server-only map markers); valid while COE2's m_bHasExfilPoint.
	[RplProp()]
	protected vector m_vCTR_ExfilPoint;
	[RplProp()]
	protected bool m_bCTR_Cancelling;
	[RplProp()]
	protected WorldTimestamp m_CTR_CancelReturn;
	//! Waves of the enemy pursuit so far (0 = none came), and when the next one comes if one more is due.
	[RplProp()]
	protected int m_iCTR_PursuitWave;
	[RplProp()]
	protected bool m_bCTR_PursuitMore;
	[RplProp()]
	protected WorldTimestamp m_CTR_NextWave;

	//! How the pay currency is shown on every machine; storage and the ledger keep the Marx currency ID.
	protected static const string CTR_CURRENCY_FORMAT = "$%1";
	//! The result screen opens after the fast travel to the base.
	protected static const int CTR_RESULT_DELAY_MS = 3000;
	protected static const int CTR_DYING_CHECK_MS = 500;
	protected static const int CTR_DYING_MAX_WAIT_MS = 5000;

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

		m_CTR_LastGear = new CTR_LastGear();
		m_CTR_LastGear.Start(this);
	}

	//------------------------------------------------------------------------------------------------
	//! Every role spawns with the gear the player last wore if they left alive, else with the same minimal kit (not when
	//! the Game Master possesses a character). Applied before the player takes control: replacing the role's weapon
	//! later leaves the character with nothing in hand.
	override bool PreparePlayerEntity_S(SCR_SpawnRequestComponent requestComponent, SCR_SpawnHandlerComponent handlerComponent, SCR_SpawnData data, IEntity entity)
	{
		if (!super.PreparePlayerEntity_S(requestComponent, handlerComponent, data, entity))
			return false;

		if (!entity || SCR_PossessSpawnData.Cast(data))
			return true;

		if (!m_CTR_LastGear || !requestComponent || !m_CTR_LastGear.Apply(entity, requestComponent.GetPlayerId()))
			CTR_StarterKit.Apply(entity, CTR_Settings.Get().GetStarterKit());

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Saves the leaving player's gear while their owner ID and character are still known.
	override protected void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		if (m_CTR_Exfil)
			m_CTR_Exfil.OnPlayerLeaving(playerId, m_vMainBasePos);

		if (m_CTR_LastGear)
			m_CTR_LastGear.OnPlayerLeaving(playerId);

		super.OnPlayerDisconnected(playerId, cause, timeout);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the vanilla reconnect gave up on a kept body (see CTR_LastGear).
	void CTR_OnReservedBodyExpired(IEntity body)
	{
		if (m_CTR_LastGear)
			m_CTR_LastGear.OnReservedBodyExpired(body);
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
	//! Server: the running exfil, or null.
	CTR_Exfil CTR_GetExfil()
	{
		return m_CTR_Exfil;
	}

	//------------------------------------------------------------------------------------------------
	CTR_LastGear CTR_GetLastGear()
	{
		return m_CTR_LastGear;
	}

	//------------------------------------------------------------------------------------------------
	bool CTR_IsReturning()
	{
		return m_bCTR_Returning;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: an operation runs or just ended in the current AO.
	bool CTR_HasOperation()
	{
		return m_bCTR_HasOperation && m_eCOE_CurrentState == COE_EGameModeState.EXECUTION;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: the operation of the current AO ended (cancelled and waiting for the return, or missing in action).
	bool CTR_IsOperationClosed()
	{
		return m_bCTR_OperationClosed;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: seconds since the operation started; stops when it ends.
	int CTR_GetOperationSeconds()
	{
		if (!m_bCTR_HasOperation || !m_CTR_OperationStart)
			return 0;

		ChimeraWorld world = GetGame().GetWorld();
		if (m_bCTR_OperationClosed && m_CTR_OperationEnd)
			return m_CTR_OperationEnd.DiffSeconds(m_CTR_OperationStart);

		if (!world)
			return 0;

		return world.GetServerTimestamp().DiffSeconds(m_CTR_OperationStart);
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: the exfil of the running operation has started.
	bool CTR_IsInExfil()
	{
		return m_bCTR_InExfil;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: the exfil point cannot be moved (the exfil started in this AO).
	bool CTR_IsExfilPointLocked()
	{
		return m_bCTR_ExfilPointLocked;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: where the exfil point is. \return False without one.
	bool CTR_GetReplicatedExfilPoint(out vector pos)
	{
		pos = m_vCTR_ExfilPoint;
		return m_bHasExfilPoint;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: when the exfil countdown runs out; null outside the exfil.
	WorldTimestamp CTR_GetExfilDeadline()
	{
		if (!m_bCTR_InExfil)
			return null;

		return m_CTR_ExfilDeadline;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: players at the exfil point, living players outside the base and how many of them must be there.
	void CTR_GetExfilCount(out int present, out int outside, out int needed)
	{
		present = m_iCTR_ExfilPresent;
		outside = m_iCTR_ExfilOutside;
		needed = m_iCTR_ExfilNeeded;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: when enough players are held at the exfil point, when they will have held it long enough.
	WorldTimestamp CTR_GetExfilHoldEnd()
	{
		if (!m_bCTR_InExfil || !m_bCTR_ExfilHolding)
			return null;

		return m_CTR_ExfilHoldEnd;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: after the commander cancelled, when everyone returns; null otherwise.
	WorldTimestamp CTR_GetCancelReturn()
	{
		if (!m_bCTR_Cancelling)
			return null;

		return m_CTR_CancelReturn;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: waves of the enemy pursuit so far during the exfil; 0 when none came.
	int CTR_GetPursuitWave()
	{
		if (!m_bCTR_InExfil)
			return 0;

		return m_iCTR_PursuitWave;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: when the next pursuit wave comes; null when no more are due.
	WorldTimestamp CTR_GetNextWave()
	{
		if (!m_bCTR_InExfil || !m_bCTR_PursuitMore)
			return null;

		return m_CTR_NextWave;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from CTR_Pursuit.
	void CTR_SetPursuitWave(int wave, bool more, int waveSeconds)
	{
		m_iCTR_PursuitWave = wave;
		m_bCTR_PursuitMore = more;
		if (more)
			m_CTR_NextWave = CTR_GetTimeIn(waveSeconds);

		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a player killed a civilian inside an AO. During the exfil the pursuit is rolled again, with the higher
	//! chance, until one came.
	protected void CTR_OnCivilianKilled()
	{
		if (m_CTR_Exfil && m_CTR_Operation && m_CTR_Exfil.GetPursuit())
			m_CTR_Exfil.GetPursuit().Roll(m_CTR_Operation.GetCivilianKills());
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: seconds from now until the time, negative when it passed. 0 without a time.
	static float CTR_SecondsUntil(WorldTimestamp time)
	{
		ChimeraWorld world = GetGame().GetWorld();
		if (!time || !world)
			return 0;

		return time.DiffSeconds(world.GetServerTimestamp());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a timestamp seconds from now.
	protected static WorldTimestamp CTR_GetTimeIn(float seconds)
	{
		ChimeraWorld world = GetGame().GetWorld();
		return world.GetServerTimestamp().PlusSeconds(seconds);
	}

	//------------------------------------------------------------------------------------------------
	override protected void GenerateAO()
	{
		// The commander's menu checks it too, but only on the commander's machine.
		string reason = CTR_ExfilPoint.GetGenerateReason();
		if (!reason.IsEmpty())
		{
			Print(string.Format("[CTR] AO not generated: %1", reason), LogLevel.WARNING);
			CTR_HintCommanders(reason);
			return;
		}

		super.GenerateAO();
		CTR_StartOperation();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a short hint for every commander, e.g. why their order was refused.
	void CTR_HintCommanders(string text)
	{
		foreach (int playerId : m_aCommanderPlayerIDs)
		{
			COE_PlayerController controller = COE_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
			if (controller)
				controller.CTR_SendHint(text);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CTR_StartOperation()
	{
		if (m_CTR_Operation && !m_CTR_Operation.IsClosed())
		{
			Print(string.Format("[CTR] Operation %1 replaced by a new AO before it ended; it pays nothing", m_CTR_Operation.GetId()), LogLevel.WARNING);
			m_CTR_Operation.Close();
		}

		CTR_StopExfil();
		CTR_StopCancel();
		m_bCTR_ResultPending = false;
		m_bCTR_AOEnded = false;
		m_bCTR_ExfilPointLocked = false;

		m_CTR_Operation = new CTR_Operation(MRX_Marx.NewId());
		m_CTR_Operation.GetOnCivilianKilled().Insert(CTR_OnCivilianKilled);
		m_CTR_Operation.StartTracking();

		m_bCTR_HasOperation = true;
		m_bCTR_OperationClosed = false;
		m_CTR_OperationStart = CTR_GetTimeIn(0);
		Replication.BumpMe();
		Print(string.Format("[CTR] Operation %1 started (%2 AO)", m_CTR_Operation.GetId(), m_aCurrentAOs.Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces COE2's exfil task: when every task of every AO is finished, the exfil starts (or, when every task failed,
	//! everyone returns at once).
	override protected void OnAOFinished(COE_AO completedAO)
	{
		// Called while the task system announces the last task's state: the exfil task cannot be created from in there
		// (its own state change would re-enter that announcement).
		GetGame().GetCallqueue().Remove(CTR_OnAllTasksFinished);
		GetGame().GetCallqueue().CallLater(CTR_OnAllTasksFinished);
	}

	//------------------------------------------------------------------------------------------------
	protected void CTR_OnAllTasksFinished()
	{
		// Ended or cancelled (waiting for the return), or in the exfil already (tasks left by an early exfil finishing).
		if (!m_CTR_Operation || m_CTR_Operation.IsClosed() || m_CTR_Exfil || m_eCOE_CurrentState != COE_EGameModeState.EXECUTION)
			return;

		if (!CTR_AreAllTasksFinished())
			return;

		array<ref CTR_AreaInfo> areas = {};
		array<ref CTR_TaskOutcome> tasks = {};
		CTR_CollectOutcomes(areas, tasks);
		if (CTR_CountCompleted(tasks) == 0)
		{
			// Nothing to be paid for, so nothing to risk an exfil for.
			CTR_EndOperation(CTR_EOperationEnd.FAILED);
			CTR_ReturnToBase("every task failed");
			return;
		}

		CTR_StartExfil(false);
	}

	//------------------------------------------------------------------------------------------------
	protected bool CTR_AreAllTasksFinished()
	{
		foreach (COE_AO ao : m_aCurrentAOs)
		{
			if (ao && !ao.AreAllTasksFinished())
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the commander orders the exfil although tasks are left. \return False when it cannot start now.
	bool CTR_StartEarlyExfil()
	{
		if (m_eCOE_CurrentState != COE_EGameModeState.EXECUTION || !m_CTR_Operation || m_CTR_Operation.IsClosed())
			return false;

		if (m_CTR_Exfil || m_bCTR_Cancelling || !HasExfilPoint())
			return false;

		CTR_StartExfil(true);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! COE2's exfil task stays for the map marker and the task list; its trigger is switched off (see CTR_Exfil).
	protected void CTR_StartExfil(bool early)
	{
		vector pos;
		if (!CTR_GetExfilPointPos(pos))
		{
			// Only an AO generated without the commander (dev tools, tests) can lack one.
			Print("[CTR] No exfil point: the operation ends as if the players had reached it", LogLevel.ERROR);
			CTR_OnExfilReached();
			return;
		}

		if (!m_ExfilTask)
			CreateExfilTask(pos);

		if (m_ExfilTask)
			m_ExfilTask.CTR_DisableTrigger();

		CTR_Settings settings = CTR_Settings.Get();
		m_bCTR_InExfil = true;
		m_bCTR_ExfilPointLocked = true;
		m_bCTR_ExfilHolding = false;
		m_CTR_ExfilDeadline = CTR_GetTimeIn(settings.m_iExfilCountdownSeconds);
		Replication.BumpMe();

		m_CTR_Exfil = new CTR_Exfil(pos, settings);
		m_CTR_Exfil.Start();
		Print(string.Format("[CTR] Operation %1: exfil started (%2) at %3, %4 s", m_CTR_Operation.GetId(), CTR_GetExfilWord(early), pos, settings.m_iExfilCountdownSeconds));
		m_CTR_Exfil.GetPursuit().Roll(m_CTR_Operation.GetCivilianKills());
	}

	//------------------------------------------------------------------------------------------------
	protected static string CTR_GetExfilWord(bool early)
	{
		if (early)
			return "early";

		return "every task finished";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: where the commander put the exfil point. \return False without one.
	bool CTR_GetExfilPointPos(out vector pos)
	{
		if (!m_aExfilMarkers || m_aExfilMarkers.IsEmpty() || !m_aExfilMarkers[0])
			return false;

		int markerPos[2];
		m_aExfilMarkers[0].GetWorldPos(markerPos);
		pos = Vector(markerPos[0], 0, markerPos[1]);
		if (KSC_TerrainHelper.SurfaceIsWater(pos))
		{
			EWaterSurfaceType surfaceType;
			float area;
			pos[1] = SCR_WorldTools.GetWaterSurfaceY(GetWorld(), pos, surfaceType, area);
		}
		else
		{
			pos[1] = SCR_TerrainHelper.GetTerrainY(pos);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from CTR_Exfil.
	void CTR_SetExfilCount(int present, int outside, int needed)
	{
		if (present == m_iCTR_ExfilPresent && outside == m_iCTR_ExfilOutside && needed == m_iCTR_ExfilNeeded)
			return;

		m_iCTR_ExfilPresent = present;
		m_iCTR_ExfilOutside = outside;
		m_iCTR_ExfilNeeded = needed;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from CTR_Exfil.
	void CTR_SetExfilHold(bool holding, WorldTimestamp end)
	{
		m_bCTR_ExfilHolding = holding;
		m_CTR_ExfilHoldEnd = end;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: moves the end of the exfil countdown (dev tools).
	void CTR_SetExfilSecondsLeft(int seconds)
	{
		if (!m_CTR_Exfil)
			return;

		m_CTR_ExfilDeadline = CTR_GetTimeIn(seconds);
		Replication.BumpMe();
		m_CTR_Exfil.Check();
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from CTR_Exfil: enough players held the exfil point. The operation pays in full and everyone returns.
	void CTR_OnExfilReached()
	{
		CTR_EOperationEnd end = CTR_EOperationEnd.COMPLETE;
		if (!CTR_AreAllTasksFinished())
			end = CTR_EOperationEnd.EARLY_EXFIL;

		CTR_EndOperation(end);
		if (m_ExfilTask)
			m_ExfilTask.SetTaskState(SCR_ETaskState.COMPLETED);

		CTR_ReturnToBase("exfil reached");
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from CTR_Exfil: the exfil countdown ran out. Missing in action: every living player outside the base dies,
	//! then the AO ends.
	void CTR_OnExfilTimeout()
	{
		CTR_Exfil exfil = m_CTR_Exfil;
		CTR_EndOperation(CTR_EOperationEnd.MISSING);
		if (m_ExfilTask)
			m_ExfilTask.SetTaskState(SCR_ETaskState.FAILED);

		m_aCTR_Dying.Clear();
		if (exfil)
			exfil.KillMissing(m_vMainBasePos, m_CTR_LastGear, m_aCTR_Dying);

		// COE2 moves and heals everyone who is not dead when the AO ends; ACE may take a moment.
		m_iCTR_DyingWaitedMs = 0;
		CTR_WaitForDeaths();
	}

	//------------------------------------------------------------------------------------------------
	protected void CTR_WaitForDeaths()
	{
		bool waiting;
		foreach (IEntity entity : m_aCTR_Dying)
		{
			ChimeraCharacter character = ChimeraCharacter.Cast(entity);
			if (character && character.GetCharacterController() && character.GetCharacterController().GetLifeState() != ECharacterLifeState.DEAD)
				waiting = true;
		}

		if (waiting && m_iCTR_DyingWaitedMs < CTR_DYING_MAX_WAIT_MS)
		{
			m_iCTR_DyingWaitedMs += CTR_DYING_CHECK_MS;
			GetGame().GetCallqueue().CallLater(CTR_WaitForDeaths, CTR_DYING_CHECK_MS);
			return;
		}

		if (waiting)
			Print("[CTR] Missing in action: some characters are still not dead", LogLevel.WARNING);

		m_aCTR_Dying.Clear();
		CTR_ReturnToBase("missing in action");
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the commander cancels the operation (commander menu, base board, #ctr cancel). The pay is settled now and
	//! everyone returns after a short delay. Ignored while a cancel waits or after the operation ended.
	void CTR_RequestCancel()
	{
		if (m_eCOE_CurrentState != COE_EGameModeState.EXECUTION || m_bCTR_Cancelling)
			return;

		if (!m_CTR_Operation)
		{
			ExecuteCommanderRequest(COE_ECommanderRequest.CANCEL_AO);
			return;
		}

		if (m_CTR_Operation.IsClosed())
			return;

		CTR_EndOperation(CTR_GetCancelEnd());
		int seconds = CTR_Settings.Get().m_iCancelReturnSeconds;
		if (seconds <= 0)
		{
			CTR_ReturnToBase("cancelled");
			return;
		}

		m_bCTR_Cancelling = true;
		m_CTR_CancelReturn = CTR_GetTimeIn(seconds);
		Replication.BumpMe();
		CTR_AlertAll(CTR_EAlert.CANCELLED, 0);
		GetGame().GetCallqueue().CallLater(CTR_FinishCancel, seconds * 1000);
		Print(string.Format("[CTR] Operation %1 cancelled; everyone returns in %2 s", m_CTR_Operation.GetId(), seconds));
	}

	//------------------------------------------------------------------------------------------------
	protected CTR_EOperationEnd CTR_GetCancelEnd()
	{
		if (m_CTR_Exfil)
			return CTR_EOperationEnd.ABANDONED;

		return CTR_EOperationEnd.CANCELLED;
	}

	//------------------------------------------------------------------------------------------------
	protected void CTR_FinishCancel()
	{
		CTR_StopCancel();
		CTR_ReturnToBase("cancelled");
	}

	//------------------------------------------------------------------------------------------------
	protected void CTR_StopCancel()
	{
		GetGame().GetCallqueue().Remove(CTR_FinishCancel);
		if (!m_bCTR_Cancelling)
			return;

		m_bCTR_Cancelling = false;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: shows an alert at the top of every player's screen.
	void CTR_AlertAll(CTR_EAlert alert, int param)
	{
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			COE_PlayerController controller = COE_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
			if (controller)
				controller.CTR_SendAlert(alert, param);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The exfil is judged by Contractors (CTR_Exfil), which ends the operation itself when it succeeds; COE2 would end
	//! the AO a second time.
	override void OnExfilStateChanged(SCR_Task task, SCR_ETaskState newState)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! The exfil point must be within the allowed distance of the AOs, and stays where it is once the exfil started:
	//! moving it away at the last moment would leave everyone missing in action. The commander's map menu checks the
	//! same on their machine; refused requests only come from a race (the exfil started meanwhile).
	override protected void CreateExfilPoint(vector pos)
	{
		string reason;
		if (CTR_ExfilPoint.IsLocked())
		{
			reason = "#CTR-Reason_ExfilLocked";
		}
		else
		{
			CTR_EExfilDistance distance = CTR_ExfilPoint.Check(pos);
			if (distance != CTR_EExfilDistance.OK)
				reason = CTR_ExfilPoint.GetReason(distance);
		}

		if (!reason.IsEmpty())
		{
			Print(string.Format("[CTR] Exfil point not set at %1: %2", pos, reason), LogLevel.WARNING);
			CTR_HintCommanders(reason);
			return;
		}

		super.CreateExfilPoint(pos);
		m_vCTR_ExfilPoint = pos;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Every end of the AO: vehicles carrying players come along with their crew and are kept; COE2 deletes the others.
	//! Ending the AO while the operation still runs (COE2's own cancel, tests) settles it as cancelled without the delay.
	override protected void DeleteAO()
	{
		if (m_CTR_Operation && !m_CTR_Operation.IsClosed())
			CTR_EndOperation(CTR_GetCancelEnd());

		CTR_StopCancel();
		GetGame().GetCallqueue().Remove(CTR_WaitForDeaths);
		m_aCTR_Dying.Clear();

		m_bCTR_Returning = true;
		m_aCTR_ReturningVehicles.Clear();
		CTR_ReturnTrip.CollectOccupiedVehicles(m_aCTR_ReturningVehicles);
		int moved = CTR_ReturnTrip.MoveVehicles(m_aCTR_ReturningVehicles, m_vMainBasePos);

		super.DeleteAO();
		m_bCTR_Returning = false;
		m_aCTR_ReturningVehicles.Clear();
		if (moved > 0)
			Print(string.Format("[CTR] AO ended: %1 vehicles moved to the base with their crew", moved));

		m_bCTR_HasOperation = false;
		m_bCTR_ExfilPointLocked = false;
		Replication.BumpMe();
		m_bCTR_AOEnded = true;
		CTR_TrySendResults();
	}

	//------------------------------------------------------------------------------------------------
	//! Closes the operation and settles it as it is now; the pay is committed in the background.
	protected void CTR_EndOperation(CTR_EOperationEnd end)
	{
		CTR_StopExfil();
		m_CTR_Operation.Close();

		array<ref CTR_AreaInfo> areas = {};
		array<ref CTR_TaskOutcome> tasks = {};
		CTR_CollectOutcomes(areas, tasks);

		m_CTR_Settlement = new CTR_Settlement(CTR_Settings.Get(), m_CTR_Operation.GetId(), end, m_CTR_Operation.GetDurationSeconds(), areas, tasks);
		m_CTR_Settlement.AddParticipants(m_CTR_Operation.GetParticipants());
		m_CTR_Settlement.GetOnDone().Insert(CTR_OnSettled);
		m_bCTR_ResultPending = true;
		m_CTR_Settlement.Pay(MRX_Marx.GetEconomy());

		m_bCTR_OperationClosed = true;
		m_CTR_OperationEnd = CTR_GetTimeIn(0);
		Replication.BumpMe();

		Print(string.Format("[CTR] Operation %1 ended %2: %3 of %4 tasks completed", m_CTR_Operation.GetId(), typename.EnumToString(CTR_EOperationEnd, end), CTR_CountCompleted(tasks), tasks.Count()));
	}

	//------------------------------------------------------------------------------------------------
	protected void CTR_StopExfil()
	{
		if (m_CTR_Exfil)
			m_CTR_Exfil.Stop();

		m_CTR_Exfil = null;
		if (!m_bCTR_InExfil)
			return;

		m_bCTR_InExfil = false;
		m_bCTR_ExfilHolding = false;
		m_iCTR_PursuitWave = 0;
		m_bCTR_PursuitMore = false;
		Replication.BumpMe();
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
		foreach (CTR_PayEntry entry : settlement.GetEntries())
		{
			Print(string.Format("[CTR] Operation %1 pay: %2 (%3) total=%4 status=%5 entered=%6", settlement.GetOperationId(), entry.m_Stats.m_sName, entry.m_sOwnerId, entry.m_Payout.m_iTotal, typename.EnumToString(CTR_EPayStatus, entry.m_eStatus), entry.m_Stats.m_bEnteredAO));
		}

		CTR_TrySendResults();
	}

	//------------------------------------------------------------------------------------------------
	//! The result screens wait for both the pay and the end of the AO: a cancelled operation is paid before everyone
	//! returns, a finished one after.
	protected void CTR_TrySendResults()
	{
		if (!m_bCTR_ResultPending || !m_bCTR_AOEnded || !m_CTR_Settlement || !m_CTR_Settlement.IsDone())
			return;

		m_bCTR_ResultPending = false;
		GetGame().GetCallqueue().CallLater(CTR_SendResults, CTR_RESULT_DELAY_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void CTR_SendResults()
	{
		if (!m_CTR_Settlement)
			return;

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			COE_PlayerController controller = COE_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
			if (controller)
				controller.CTR_SendOperationResult(m_CTR_Settlement.BuildResult(playerId).ToJson());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Ends the AO the way COE2 does after exfil; see DeleteAO for the vehicles.
	protected void CTR_ReturnToBase(string reason)
	{
		if (m_eCOE_CurrentState != COE_EGameModeState.EXECUTION)
			return;

		Print(string.Format("[CTR] Returning everyone to base: %1", reason));
		ExecuteCommanderRequest(COE_ECommanderRequest.CANCEL_AO);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the operation screen of a player. While the operation runs: the lines so far and the pay if the exfil
	//! succeeds. After it ends, until the AO ends (cancel delay): the result once paid. Null otherwise.
	CTR_OperationResult CTR_BuildStatus(int playerId)
	{
		if (m_CTR_Operation && !m_CTR_Operation.IsClosed())
		{
			array<ref CTR_AreaInfo> areas = {};
			array<ref CTR_TaskOutcome> tasks = {};
			CTR_CollectOutcomes(areas, tasks);
			CTR_Settlement status = CTR_Settlement.CreateInProgress(CTR_Settings.Get(), m_CTR_Operation.GetId(), m_CTR_Exfil != null, m_CTR_Operation.GetDurationSeconds(), areas, tasks);
			status.AddParticipants(m_CTR_Operation.GetParticipants());
			return status.BuildResult(playerId);
		}

		if (m_eCOE_CurrentState != COE_EGameModeState.EXECUTION || !m_CTR_Operation || !m_CTR_Settlement || !m_CTR_Settlement.IsDone())
			return null;

		if (m_CTR_Settlement.GetOperationId() != m_CTR_Operation.GetId())
			return null;

		return m_CTR_Settlement.BuildResult(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! When the AO ends, vehicles carrying players stay; COE2 deletes the others.
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

//------------------------------------------------------------------------------------------------
//! The commander's cancel (commander menu and base board both send it here) goes through Contractors: the pay is settled
//! at once and everyone returns after a short delay. Other code ending the AO (COE2's exfil, tests) is not delayed.
modded class COE_EditorModeCommanderEntity
{
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	override protected void RequestServer(COE_ECommanderRequest request, vector pos)
	{
		if (request == COE_ECommanderRequest.CANCEL_AO && m_pGameMode)
		{
			m_pGameMode.CTR_RequestCancel();
			return;
		}

		super.RequestServer(request, pos);
	}
}

//------------------------------------------------------------------------------------------------
//! The trigger of COE2's exfil task would show its own countdown and play music to whoever stands in it; Contractors
//! judges the exfil itself (CTR_Exfil).
modded class KSC_AreaTriggerTask
{
	//------------------------------------------------------------------------------------------------
	void CTR_DisableTrigger()
	{
		if (!m_pTrigger)
			return;

		m_pTrigger.GetOnActivate().Remove(OnTriggerActivate);
		// Its queries start later on their own; nobody fits in a sphere of radius 0.
		m_pTrigger.SetSphereRadius(0);
		m_pTrigger.EnablePeriodicQueries(false);
	}
}
