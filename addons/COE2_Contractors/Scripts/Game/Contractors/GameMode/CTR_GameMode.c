//! Operations pay instead of ending with COE2's exfil task:
//! AO generated -> operation tracked -> all tasks finished -> pay and result screen -> return to base after a delay.
//! A cancelled operation (commander, wipe) pays nothing and shows a failed result; COE2 returns everyone itself.
modded class COE_GameMode
{
	protected ref CTR_Operation m_CTR_Operation;
	protected ref CTR_Settlement m_CTR_Settlement;
	//! Set while the return to base ends the AO: players in vehicles ride along and vehicles are kept.
	protected bool m_bCTR_Returning;

	//------------------------------------------------------------------------------------------------
	override protected void OnGameStart()
	{
		super.OnGameStart();

		if (Replication.IsServer())
			CTR_CheckSystems();
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
	override protected void GenerateAO()
	{
		super.GenerateAO();
		CTR_StartOperation();
	}

	//------------------------------------------------------------------------------------------------
	protected void CTR_StartOperation()
	{
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
	//! Ending the AO before its tasks are finished cancels the operation.
	override protected void DeleteAO()
	{
		if (m_CTR_Operation && !m_CTR_Operation.IsClosed())
			CTR_EndOperation(false);

		super.DeleteAO();
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

		return "cancelled";
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

		if (settlement.IsFinished())
			GetGame().GetCallqueue().CallLater(CTR_ReturnToBase, returnDelay * 1000, false, settlement.GetOperationId());
	}

	//------------------------------------------------------------------------------------------------
	//! Ends the AO the way COE2 does after exfil, but vehicles with players come along and no vehicle is deleted.
	protected void CTR_ReturnToBase(string operationId)
	{
		// The commander may have ended the AO or started the next one meanwhile.
		if (!m_CTR_Operation || m_CTR_Operation.GetId() != operationId || m_eCOE_CurrentState != COE_EGameModeState.EXECUTION)
			return;

		m_bCTR_Returning = true;
		int moved = CTR_ReturnTrip.MoveOccupiedVehicles(m_vMainBasePos);
		ExecuteCommanderRequest(COE_ECommanderRequest.CANCEL_AO);
		m_bCTR_Returning = false;
		Print(string.Format("[CTR] Operation %1: returned to base, %2 vehicles moved", operationId, moved));
	}

	//------------------------------------------------------------------------------------------------
	//! On the return to base, vehicles stay: those that came along and those left in the AO.
	override void CollectBuiltEntitiesForCleanUp()
	{
		super.CollectBuiltEntitiesForCleanUp();
		if (!m_bCTR_Returning)
			return;

		for (int i = m_aEntitiesToDelete.Count() - 1; i >= 0; i--)
		{
			IEntity entity = m_aEntitiesToDelete[i];
			if (entity && entity.FindComponent(SCR_EditableVehicleComponent))
				m_aEntitiesToDelete.Remove(i);
		}
	}
}
