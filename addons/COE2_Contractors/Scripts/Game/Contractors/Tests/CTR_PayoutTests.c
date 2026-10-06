#ifdef WORKBENCH
class CTR_PayoutTests
{
	static const ResourceName CAPTIVE_TASK = "{DE9C612D13BF9B18}Prefabs/Tasks/KSC_TakeCaptiveTask.et";
	static const ResourceName KILL_TASK = "{5474FE33D55AB461}Prefabs/Tasks/KSC_KillTask.et";

	//------------------------------------------------------------------------------------------------
	static void Register(notnull CTR_TestRunner runner)
	{
		runner.Add(new CTR_Test_TaskPricing());
		runner.Add(new CTR_Test_PayoutRules());
		runner.Add(new CTR_Test_StatMapping());
		runner.Add(new CTR_Test_UnlistedTreatment());
		runner.Add(new CTR_Test_SettlementPaysOnce());
		runner.Add(new CTR_Test_ResultJson());
	}

	//------------------------------------------------------------------------------------------------
	static CTR_TaskOutcome CreateTask(string builderClass, bool completed, ResourceName prefab = string.Empty)
	{
		CTR_TaskOutcome task = new CTR_TaskOutcome();
		task.m_sType = builderClass;
		task.m_sTaskPrefab = prefab;
		task.m_sName = builderClass;
		task.m_bCompleted = completed;
		return task;
	}

	//------------------------------------------------------------------------------------------------
	static CTR_PlayerStats CreateStats(bool entered, int kills = 0, int teamKills = 0, int deaths = 0, int heals = 0)
	{
		CTR_PlayerStats stats = new CTR_PlayerStats();
		stats.m_bEnteredAO = entered;
		stats.m_iKills = kills;
		stats.m_iTeamKills = teamKills;
		stats.m_iDeaths = deaths;
		stats.m_iHeals = heals;
		return stats;
	}

	//------------------------------------------------------------------------------------------------
	static CTR_Participant CreateParticipant(int playerId, string ownerId, CTR_PlayerStats stats)
	{
		CTR_Participant participant = new CTR_Participant();
		participant.m_iPlayerId = playerId;
		participant.m_sOwnerId = ownerId;
		participant.m_Stats = stats;
		stats.m_sName = "player " + playerId;
		return participant;
	}

	//------------------------------------------------------------------------------------------------
	//! In-memory economy with one "cash" currency starting at 0.
	static MRX_EconomyService CreateEconomy()
	{
		MRX_CurrencyRegistry currencies = new MRX_CurrencyRegistry();
		currencies.Register(MRX_CurrencyDef.Create(MRX_Settings.DEFAULT_CURRENCY, 0, 1000000));
		MRX_EconomyService economy = new MRX_EconomyService(new MRX_InMemoryBackend(), MRX_StorageRules.Create(currencies, 50, 200));
		economy.Init();
		return economy;
	}
}

//------------------------------------------------------------------------------------------------
class CTR_Test_TaskPricing : CTR_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		CTR_Settings settings = CTR_Settings.CreateDefault();
		array<ref CTR_TaskOutcome> tasks = {
			CTR_PayoutTests.CreateTask("COE_ClearAreaTaskBuilder", true),
			CTR_PayoutTests.CreateTask("COE_EnemyOfficerTaskBuilder", true, CTR_PayoutTests.CAPTIVE_TASK),
			CTR_PayoutTests.CreateTask("COE_EnemyOfficerTaskBuilder", true, CTR_PayoutTests.KILL_TASK),
			CTR_PayoutTests.CreateTask("COE_FreeHostageTaskBuilder", false),
			CTR_PayoutTests.CreateTask("SomeModTaskBuilder", true)
		};

		int sum = CTR_PayoutCalculator.PriceTasks(settings, tasks);
		CheckInt(tasks[0].m_iAmount, 6000, "clear area");
		CheckInt(tasks[1].m_iAmount, 18000, "capture officer (prefab entry before the builder entry)");
		CheckInt(tasks[2].m_iAmount, 12000, "kill officer (builder entry)");
		CheckInt(tasks[3].m_iAmount, 0, "failed task pays nothing");
		CheckInt(tasks[4].m_iAmount, settings.m_iDefaultTaskReward, "unknown builder pays the default");
		CheckInt(sum, 6000 + 18000 + 12000 + 6000, "sum of completed tasks");

		// The shipped config must match the built-in defaults.
		CTR_Settings loaded = CTR_Settings.Get();
		CheckString(loaded.m_sCurrency, MRX_Settings.DEFAULT_CURRENCY, "config currency");
		CheckInt(loaded.GetTaskReward("COE_EnemyOfficerTaskBuilder", CTR_PayoutTests.CAPTIVE_TASK), 18000, "config capture officer");
		CheckInt(loaded.GetTaskReward("COE_EnemyOfficerTaskBuilder", CTR_PayoutTests.KILL_TASK), 12000, "config kill officer");
		CheckInt(loaded.m_iReturnDelaySeconds, 10, "config return delay");
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
class CTR_Test_PayoutRules : CTR_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		CTR_Settings settings = CTR_Settings.CreateDefault();

		// A typical operation: clear area, destroy cache, kill officer.
		int tasks = 6000 + 8000 + 12000;
		CTR_Payout payout = CTR_PayoutCalculator.Calculate(settings, true, tasks, CTR_PayoutTests.CreateStats(true, 10, 1, 1, 2));
		CheckInt(payout.m_iTasks, tasks, "task line");
		CheckInt(payout.m_iKills, 10 * 250, "kill line");
		CheckInt(payout.m_iHeals, 2 * 300, "heal line");
		CheckInt(payout.m_iTeamKills, -10000, "team kill line");
		CheckInt(payout.m_iDeaths, -2500, "death line");
		CheckInt(payout.m_iTotal, tasks + 2500 + 600 - 10000 - 2500, "total");

		CheckInt(CTR_PayoutCalculator.Calculate(settings, true, tasks, CTR_PayoutTests.CreateStats(false, 10)).m_iTotal, 0, "never entered the AO");
		CheckInt(CTR_PayoutCalculator.Calculate(settings, false, tasks, CTR_PayoutTests.CreateStats(true, 10)).m_iTotal, 0, "cancelled operation");
		CheckInt(CTR_PayoutCalculator.Calculate(settings, true, 0, CTR_PayoutTests.CreateStats(true, 10, 0, 0, 5)).m_iTotal, 0, "no completed task, no personal pay");

		CTR_Payout negative = CTR_PayoutCalculator.Calculate(settings, true, 6000, CTR_PayoutTests.CreateStats(true, 0, 3));
		CheckInt(negative.m_iTeamKills, -30000, "team kill line keeps its full value");
		CheckInt(negative.m_iTotal, 0, "total is not negative");

		CheckString(CTR_PayoutCalculator.GetIdempotencyKey("op1", "owner1"), "op:op1:owner1", "idempotency key");
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
class CTR_Test_StatMapping : CTR_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		CTR_PlayerStats stats = new CTR_PlayerStats();
		CTR_Operation.AddStat(stats, SCR_EDataStats.AI_KILLS, 1);
		CTR_Operation.AddStat(stats, SCR_EDataStats.KILLS, 1);
		CTR_Operation.AddStat(stats, SCR_EDataStats.AI_ROADKILLS, 1);
		CTR_Operation.AddStat(stats, SCR_EDataStats.FRIENDLY_AI_KILLS, 1);
		CTR_Operation.AddStat(stats, SCR_EDataStats.FRIENDLY_KILLS, 1);
		CTR_Operation.AddStat(stats, SCR_EDataStats.DEATHS, 1);
		CTR_Operation.AddStat(stats, SCR_EDataStats.SHOTS, 30);
		CTR_Operation.AddStat(stats, SCR_EDataStats.BANDAGE_FRIENDLIES, 1);
		CTR_Operation.AddStat(stats, SCR_EDataStats.MORPHINE_FRIENDLIES, 1);
		CTR_Operation.AddStat(stats, SCR_EDataStats.BANDAGE_SELF, 1);
		CTR_Operation.AddStat(stats, SCR_EDataStats.DISTANCE_WALKED, 120.5);
		CTR_Operation.AddStat(stats, SCR_EDataStats.DISTANCE_AS_OCCUPANT, 79.5);
		CTR_Operation.AddStat(stats, SCR_EDataStats.WARCRIMES, 5);

		CheckInt(stats.m_iKills, 3, "kills");
		CheckInt(stats.m_iTeamKills, 2, "team kills");
		CheckInt(stats.m_iDeaths, 1, "deaths");
		CheckInt(stats.m_iShots, 30, "shots");
		CheckInt(stats.m_iHeals, 2, "heals of others only");
		CheckInt(Math.Round(stats.m_fDistance), 200, "distance");
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! Treatments with medical items the data collector does not list (ACE drugs) count as heals while the operation runs.
class CTR_Test_UnlistedTreatment : CTR_TestCase
{
	protected static const int PLAYER_ID = 77;

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		CTR_Operation operation = new CTR_Operation("test-treatment");
		operation.StartTracking();
		SCR_DataCollectorHealingItemsModule.CTR_GetOnUnlistedTreatment().Invoke(PLAYER_ID);
		SCR_DataCollectorHealingItemsModule.CTR_GetOnUnlistedTreatment().Invoke(PLAYER_ID);
		CTR_Participant participant = operation.GetParticipants().Get(PLAYER_ID);
		Check(participant != null, "treating player becomes a participant");
		if (participant)
			CheckInt(participant.m_Stats.m_iHeals, 2, "two treatments");

		operation.Close();
		SCR_DataCollectorHealingItemsModule.CTR_GetOnUnlistedTreatment().Invoke(PLAYER_ID);
		if (participant)
			CheckInt(participant.m_Stats.m_iHeals, 2, "no counting after the operation closed");

		Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! Sessions merge per owner, only owners who entered get paid, and settling the same operation again pays nothing.
class CTR_Test_SettlementPaysOnce : CTR_TestCase
{
	protected ref MRX_EconomyService m_Economy;
	protected ref CTR_Settlement m_First;
	protected ref CTR_Settlement m_Second;
	protected ref CTR_Settlement m_Cancelled;

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		m_Economy = CTR_PayoutTests.CreateEconomy();
		m_First = CreateSettlement(true);
		m_First.GetOnDone().Insert(OnFirstDone);
		m_First.Pay(m_Economy);
	}

	//------------------------------------------------------------------------------------------------
	protected CTR_Settlement CreateSettlement(bool finished)
	{
		array<ref CTR_AreaInfo> areas = {};
		array<ref CTR_TaskOutcome> tasks = {
			CTR_PayoutTests.CreateTask("COE_ClearAreaTaskBuilder", true),
			CTR_PayoutTests.CreateTask("COE_FindIntelTaskBuilder", false)
		};

		CTR_Settlement settlement = new CTR_Settlement(CTR_Settings.CreateDefault(), "test-op", finished, 60, areas, tasks);
		map<int, ref CTR_Participant> participants = new map<int, ref CTR_Participant>();
		participants.Insert(1, CTR_PayoutTests.CreateParticipant(1, "owner-a", CTR_PayoutTests.CreateStats(true, 2)));
		// owner-a again after a reconnect, without entering the AO in this session
		participants.Insert(5, CTR_PayoutTests.CreateParticipant(5, "owner-a", CTR_PayoutTests.CreateStats(false, 1)));
		participants.Insert(2, CTR_PayoutTests.CreateParticipant(2, "owner-b", CTR_PayoutTests.CreateStats(false, 4)));
		participants.Insert(3, CTR_PayoutTests.CreateParticipant(3, string.Empty, CTR_PayoutTests.CreateStats(true)));
		settlement.AddParticipants(participants);
		return settlement;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnFirstDone(CTR_Settlement settlement)
	{
		CTR_OperationResult a = settlement.BuildResult(5, 10);
		CheckInt(a.m_Stats.m_iKills, 3, "sessions of owner-a merged");
		Check(a.m_Stats.m_bEnteredAO, "owner-a entered in one session");
		CheckInt(a.m_Payout.m_iTotal, 6000 + 3 * 250, "owner-a pay");
		CheckInt(a.m_ePayStatus, CTR_EPayStatus.PAID, "owner-a status");
		Check(a.m_bHasBalance, "owner-a balance known");
		CheckInt(a.m_iBalance, 6750, "owner-a balance");

		CTR_OperationResult b = settlement.BuildResult(2, 10);
		CheckInt(b.m_Payout.m_iTotal, 0, "owner-b did not enter");
		CheckInt(b.m_ePayStatus, CTR_EPayStatus.NONE, "owner-b status");

		CTR_OperationResult noOwner = settlement.BuildResult(3, 10);
		CheckInt(noOwner.m_Payout.m_iTotal, 6000, "player without owner earned pay");
		CheckInt(noOwner.m_ePayStatus, CTR_EPayStatus.NO_OWNER, "player without owner is not paid");

		CTR_OperationResult stranger = settlement.BuildResult(99, 10);
		CheckInt(stranger.m_Payout.m_iTotal, 0, "player not in the operation");
		CheckInt(stranger.m_iParticipants, 2, "participants");
		CheckInt(stranger.m_iTeamPay, 6750 + 6000, "team pay");
		CheckInt(stranger.CountCompletedTasks(), 1, "completed tasks");

		m_Second = CreateSettlement(true);
		m_Second.GetOnDone().Insert(OnSecondDone);
		m_Second.Pay(m_Economy);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSecondDone(CTR_Settlement settlement)
	{
		CTR_OperationResult a = settlement.BuildResult(1, 10);
		CheckInt(a.m_ePayStatus, CTR_EPayStatus.ALREADY_PAID, "second settlement of the same operation");
		CheckInt(a.m_iBalance, 6750, "no second pay");

		m_Cancelled = CreateSettlement(false);
		m_Cancelled.GetOnDone().Insert(OnCancelledDone);
		m_Cancelled.Pay(m_Economy);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCancelledDone(CTR_Settlement settlement)
	{
		foreach (CTR_PayEntry entry : settlement.GetEntries())
		{
			CheckInt(entry.m_Payout.m_iTotal, 0, "cancelled pays nothing: " + entry.m_sOwnerId);
			CheckInt(entry.m_eStatus, CTR_EPayStatus.NONE, "cancelled status: " + entry.m_sOwnerId);
		}

		int balance;
		Check(!m_Economy.TryGetCachedBalance("owner-b", MRX_Settings.DEFAULT_CURRENCY, balance) || balance == 0, "owner-b unpaid");
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
class CTR_Test_ResultJson : CTR_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		CTR_OperationResult result = new CTR_OperationResult();
		result.m_sOperationId = "op-json";
		result.m_bFinished = true;
		result.m_iDurationSeconds = 1234;
		CTR_AreaInfo area = new CTR_AreaInfo();
		area.m_sName = "Morton";
		area.m_fX = 4000;
		area.m_fZ = 5000;
		area.m_fRadius = 250;
		result.m_aAreas.Insert(area);
		CTR_TaskOutcome task = CTR_PayoutTests.CreateTask("COE_ClearAreaTaskBuilder", true);
		task.m_iAmount = 150;
		task.m_fX = 4010;
		result.m_aTasks.Insert(task);
		result.m_Stats = CTR_PayoutTests.CreateStats(true, 7, 1, 2, 3);
		result.m_Stats.m_fDistance = 1500.5;
		result.m_Payout = CTR_PayoutCalculator.Calculate(CTR_Settings.CreateDefault(), true, 150, result.m_Stats);
		result.m_ePayStatus = CTR_EPayStatus.PAID;
		result.m_bHasBalance = true;
		result.m_iBalance = 420;
		result.m_sCurrency = "cash";
		result.m_iReturnDelaySeconds = 10;

		string json = result.ToJson();
		CTR_OperationResult loaded = CTR_OperationResult.FromJson(json);
		Check(loaded != null, "parsed");
		if (loaded)
		{
			CheckString(loaded.m_sOperationId, "op-json", "operation id");
			Check(loaded.m_bFinished, "finished");
			CheckInt(loaded.m_iDurationSeconds, 1234, "duration");
			CheckInt(loaded.m_aAreas.Count(), 1, "areas");
			CheckInt(loaded.m_aTasks.Count(), 1, "tasks");
			if (loaded.m_aTasks.Count() == 1)
			{
				CheckInt(loaded.m_aTasks[0].m_iAmount, 150, "task amount");
				Check(loaded.m_aTasks[0].m_bCompleted, "task completed");
			}

			CheckInt(loaded.m_Stats.m_iKills, 7, "kills");
			CheckInt(loaded.m_Payout.m_iTotal, result.m_Payout.m_iTotal, "total");
			CheckInt(loaded.m_ePayStatus, CTR_EPayStatus.PAID, "status");
			CheckInt(loaded.m_iBalance, 420, "balance");
			CheckInt(loaded.m_iReturnDelaySeconds, 10, "return delay");
		}

		Check(CTR_OperationResult.FromJson("not json") == null, "bad text is refused");
		Finish();
	}
}
#endif
