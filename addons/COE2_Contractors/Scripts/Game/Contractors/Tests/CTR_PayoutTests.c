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
		runner.Add(new CTR_Test_PayPercent());
		runner.Add(new CTR_Test_StatMapping());
		runner.Add(new CTR_Test_AIKills());
		runner.Add(new CTR_Test_CprTime());
		runner.Add(new CTR_Test_SettlementPaysOnce());
		runner.Add(new CTR_Test_SettlementInProgress());
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
		CheckInt(tasks[3].m_iReward, 20000, "reward known for a task not completed");
		CheckInt(tasks[4].m_iAmount, settings.m_iDefaultTaskReward, "unknown builder pays the default");
		CheckInt(sum, 6000 + 18000 + 12000 + 6000, "sum of completed tasks");

		// The shipped config must match the built-in defaults.
		CTR_Settings loaded = CTR_Settings.Get();
		CheckString(loaded.m_sCurrency, MRX_Settings.DEFAULT_CURRENCY, "config currency");
		CheckInt(loaded.GetTaskReward("COE_EnemyOfficerTaskBuilder", CTR_PayoutTests.CAPTIVE_TASK), 18000, "config capture officer");
		CheckInt(loaded.GetTaskReward("COE_EnemyOfficerTaskBuilder", CTR_PayoutTests.KILL_TASK), 12000, "config kill officer");

		// Not in the shipped config: the attribute defaults must match the built-in defaults.
		CheckInt(loaded.m_iExfilCountdownSeconds, settings.m_iExfilCountdownSeconds, "config exfil countdown");
		CheckInt(loaded.m_iCancelPayPercent, settings.m_iCancelPayPercent, "config cancel percent");
		CheckInt(loaded.m_iCancelReturnSeconds, settings.m_iCancelReturnSeconds, "config cancel return");
		CheckInt(loaded.m_iExfilEnemyWaves, settings.m_iExfilEnemyWaves, "config pursuit waves");
		Check(loaded.m_fExfilPlayerRatio == settings.m_fExfilPlayerRatio, "config exfil ratio");
		Check(loaded.m_fExfilMaxDistance == settings.m_fExfilMaxDistance, "config exfil max distance");
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
		CTR_Payout payout = CTR_PayoutCalculator.Calculate(settings, tasks, CTR_PayoutTests.CreateStats(true, 10, 1, 1, 2));
		CheckInt(payout.m_iTasks, tasks, "task line");
		CheckInt(payout.m_iKills, 10 * 250, "kill line");
		CheckInt(payout.m_iHeals, 2 * 300, "heal line");
		CheckInt(payout.m_iTeamKills, -10000, "team kill line");
		CheckInt(payout.m_iDeaths, -2500, "death line");
		CheckInt(payout.m_iTotal, tasks + 2500 + 600 - 10000 - 2500, "total");

		CheckInt(CTR_PayoutCalculator.Calculate(settings, tasks, CTR_PayoutTests.CreateStats(false, 10)).m_iTotal, 0, "never entered the AO");
		CheckInt(CTR_PayoutCalculator.Calculate(settings, 0, CTR_PayoutTests.CreateStats(true, 10, 0, 0, 5)).m_iTotal, 0, "no completed task, no personal pay");

		CTR_Payout negative = CTR_PayoutCalculator.Calculate(settings, 6000, CTR_PayoutTests.CreateStats(true, 0, 3));
		CheckInt(negative.m_iTeamKills, -30000, "team kill line keeps its full value");
		CheckInt(negative.m_iTotal, 0, "total is not negative");

		CheckString(CTR_PayoutCalculator.GetIdempotencyKey("op1", "owner1"), "op:op1:owner1", "idempotency key");

		// Live view of a running operation.
		CTR_PlayerStats active = CTR_PayoutTests.CreateStats(true, 4, 0, 1, 2);
		CTR_Payout soFar = CTR_PayoutCalculator.CalculateSoFar(settings, 0, active);
		CheckInt(soFar.m_iKills, 4 * 250, "so far: kill line before a task is completed");
		CheckInt(soFar.m_iHeals, 2 * 300, "so far: heal line before a task is completed");
		CheckInt(soFar.m_iDeaths, -2500, "so far: death line before a task is completed");
		CheckInt(soFar.m_iTotal, 0, "so far: nothing paid before a task is completed");
		CheckInt(CTR_PayoutCalculator.CalculateSoFar(settings, 6000, active).m_iTotal, CTR_PayoutCalculator.Calculate(settings, 6000, active).m_iTotal, "so far: the pay once a task is completed");
		CheckInt(CTR_PayoutCalculator.CalculateSoFar(settings, 0, CTR_PayoutTests.CreateStats(false, 4)).m_iKills, 0, "so far: no lines outside the AO");

		// If the exfil succeeds.
		CheckInt(CTR_PayoutCalculator.CalculateIfSuccess(settings, 14000, active), 14000 + 1000 + 600 - 2500, "if success: tasks and personal lines");
		CheckInt(CTR_PayoutCalculator.CalculateIfSuccess(settings, 14000, CTR_PayoutTests.CreateStats(false, 4)), 14000 + 1000, "if success: counts as entering the AO");
		CheckInt(CTR_PayoutCalculator.CalculateIfSuccess(settings, 0, active), 0, "if success: nothing without task pay");
		CheckInt(CTR_PayoutCalculator.CalculateIfSuccess(settings, 6000, CTR_PayoutTests.CreateStats(true, 0, 1)), 0, "if success: not below 0");
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! How the operation ended sets the share of the earnings; deductions stay whole and the total is never below 0.
class CTR_Test_PayPercent : CTR_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		CTR_Settings settings = CTR_Settings.CreateDefault();
		CheckInt(settings.GetPayPercent(CTR_EOperationEnd.COMPLETE), 100, "complete");
		CheckInt(settings.GetPayPercent(CTR_EOperationEnd.EARLY_EXFIL), 100, "early exfil");
		CheckInt(settings.GetPayPercent(CTR_EOperationEnd.FAILED), 100, "failed (nothing completed anyway)");
		CheckInt(settings.GetPayPercent(CTR_EOperationEnd.CANCELLED), 25, "cancelled before the exfil");
		CheckInt(settings.GetPayPercent(CTR_EOperationEnd.ABANDONED), 0, "cancelled during the exfil");
		CheckInt(settings.GetPayPercent(CTR_EOperationEnd.MISSING), 0, "missing in action");

		int tasks = 6000 + 8000 + 12000;
		CTR_Payout quarter = CTR_PayoutCalculator.Calculate(settings, tasks, CTR_PayoutTests.CreateStats(true, 10, 0, 0, 2), 25);
		CheckInt(quarter.m_iTasks, tasks / 4, "25%: task line");
		CheckInt(quarter.m_iKills, 10 * 250 / 4, "25%: kill line");
		CheckInt(quarter.m_iHeals, 2 * 300 / 4, "25%: heal line");
		CheckInt(quarter.m_iTotal, tasks / 4 + 625 + 150, "25%: total");

		CTR_Payout odd = CTR_PayoutCalculator.Calculate(settings, 6000, CTR_PayoutTests.CreateStats(true, 3), 25);
		CheckInt(odd.m_iKills, 187, "25%: rounded down (750 -> 187)");

		CTR_Payout deducted = CTR_PayoutCalculator.Calculate(settings, tasks, CTR_PayoutTests.CreateStats(true, 0, 1, 1), 25);
		CheckInt(deducted.m_iTeamKills, -10000, "25%: team kill deduction whole");
		CheckInt(deducted.m_iDeaths, -2500, "25%: death deduction whole");
		CheckInt(deducted.m_iTotal, 0, "25%: total not below 0");

		CTR_Payout nothing = CTR_PayoutCalculator.Calculate(settings, tasks, CTR_PayoutTests.CreateStats(true, 10, 0, 1, 2), 0);
		CheckInt(nothing.m_iTasks + nothing.m_iKills + nothing.m_iHeals, 0, "0%: no earnings");
		CheckInt(nothing.m_iDeaths, -2500, "0%: deduction still shown");
		CheckInt(nothing.m_iTotal, 0, "0%: total 0");

		CheckInt(CTR_PayoutCalculator.Calculate(settings, tasks, CTR_PayoutTests.CreateStats(true, 10), 100).m_iTotal, CTR_PayoutCalculator.Calculate(settings, tasks, CTR_PayoutTests.CreateStats(true, 10)).m_iTotal, "100% is the full pay");
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! The operation screen of a running operation: the same merging as the pay, the live lines, and never paid. Before the
//! exfil the pay if it succeeds counts every task that has not failed; after it starts, only the completed ones.
class CTR_Test_SettlementInProgress : CTR_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		array<ref CTR_AreaInfo> areas = {};
		CTR_TaskOutcome failed = CTR_PayoutTests.CreateTask("COE_FindIntelTaskBuilder", false);
		failed.m_bFailed = true;
		array<ref CTR_TaskOutcome> tasks = {CTR_PayoutTests.CreateTask("COE_ClearAreaTaskBuilder", false), failed, CTR_PayoutTests.CreateTask("COE_DestroyCacheTaskBuilder", true)};

		CTR_Settlement status = CTR_Settlement.CreateInProgress(CTR_Settings.CreateDefault(), "op-live", false, 90, areas, tasks);
		map<int, ref CTR_Participant> participants = new map<int, ref CTR_Participant>();
		participants.Insert(1, CTR_PayoutTests.CreateParticipant(1, "owner-a", CTR_PayoutTests.CreateStats(true, 2)));
		participants.Insert(4, CTR_PayoutTests.CreateParticipant(4, "owner-a", CTR_PayoutTests.CreateStats(false, 1)));
		participants.Insert(2, CTR_PayoutTests.CreateParticipant(2, "owner-b", CTR_PayoutTests.CreateStats(false)));
		status.AddParticipants(participants);

		CTR_OperationResult result = status.BuildResult(4);
		Check(result.m_bInProgress, "in progress");
		Check(!result.m_bExfil, "before the exfil");
		CheckInt(result.m_Stats.m_iKills, 3, "sessions merged");
		CheckInt(result.m_Payout.m_iKills, 3 * 250, "kill line shown");
		CheckInt(result.m_Payout.m_iTotal, 8000 + 3 * 250, "lines so far");
		CheckInt(result.m_ePayStatus, CTR_EPayStatus.NONE, "not paid");
		CheckInt(result.m_aTasks[0].m_iReward, 6000, "reward of the open task");
		CheckInt(result.m_iTotalIfSuccess, 6000 + 8000 + 3 * 250, "if success: every task still possible (the failed one does not count)");
		CheckInt(result.m_iTeamPay, result.m_iTotalIfSuccess, "team if success: only those who entered");

		CTR_Settlement exfil = CTR_Settlement.CreateInProgress(CTR_Settings.CreateDefault(), "op-live", true, 90, areas, tasks);
		exfil.AddParticipants(participants);
		CTR_OperationResult inExfil = exfil.BuildResult(1);
		Check(inExfil.m_bExfil, "during the exfil");
		CheckInt(inExfil.m_iTotalIfSuccess, 8000 + 3 * 250, "if success during the exfil: completed tasks only");

		CTR_OperationResult notEntered = exfil.BuildResult(2);
		CheckInt(notEntered.m_iTotalIfSuccess, 8000, "if success: counts as entering the AO");
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

		CheckInt(stats.m_iKills, 1, "player kills only (AI kills come from death events)");
		CheckInt(stats.m_iTeamKills, 1, "player team kills only");
		CheckInt(stats.m_iDeaths, 1, "deaths");
		CheckInt(stats.m_iShots, 30, "shots");
		CheckInt(stats.m_iHeals, 1, "bandages on others only (no drugs, not on oneself)");
		CheckInt(Math.Round(stats.m_fDistance), 200, "distance");
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! AI killed by a player: enemies are kills, friendlies team kills, civilians team kills only inside an AO.
class CTR_Test_AIKills : CTR_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		CTR_PlayerStats stats = new CTR_PlayerStats();
		CTR_Operation.AddAIKill(stats, SCR_ECharacterDeathStatusRelations.KILLED_BY_ENEMY_PLAYER, false, true);
		CTR_Operation.AddAIKill(stats, SCR_ECharacterDeathStatusRelations.KILLED_BY_ENEMY_PLAYER, false, false);
		CTR_Operation.AddAIKill(stats, SCR_ECharacterDeathStatusRelations.KILLED_BY_FRIENDLY_PLAYER, false, false);
		CheckInt(stats.m_iKills, 2, "enemy kills, inside and outside the AO");
		CheckInt(stats.m_iTeamKills, 1, "friendly AI");

		CTR_Operation.AddAIKill(stats, SCR_ECharacterDeathStatusRelations.KILLED_BY_ENEMY_PLAYER, true, true);
		CTR_Operation.AddAIKill(stats, SCR_ECharacterDeathStatusRelations.KILLED_BY_FRIENDLY_PLAYER, true, true);
		CheckInt(stats.m_iKills, 2, "a civilian never counts as a kill, whatever the faction relation");
		CheckInt(stats.m_iTeamKills, 3, "civilians inside the AO cost like team kills");

		CTR_Operation.AddAIKill(stats, SCR_ECharacterDeathStatusRelations.KILLED_BY_ENEMY_PLAYER, true, false);
		CheckInt(stats.m_iTeamKills, 3, "civilians outside the AO cost nothing");
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! Every full interval of CPR counts as one treatment, also when the time comes in pieces, up to a cap per patient.
class CTR_Test_CprTime : CTR_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		CTR_Participant participant = new CTR_Participant();
		CTR_Operation.AddCprSeconds(participant, 7, 14, 15, 120);
		CheckInt(participant.m_Stats.m_iHeals, 0, "less than one interval");
		CTR_Operation.AddCprSeconds(participant, 7, 2, 15, 120);
		CheckInt(participant.m_Stats.m_iHeals, 1, "first interval complete");
		CTR_Operation.AddCprSeconds(participant, 7, 30, 15, 120);
		CheckInt(participant.m_Stats.m_iHeals, 3, "two more intervals");
		CTR_Operation.AddCprSeconds(participant, 7, 300, 15, 120);
		CheckInt(participant.m_Stats.m_iHeals, 8, "capped at two minutes per patient");
		CTR_Operation.AddCprSeconds(participant, 7, 60, 15, 120);
		CheckInt(participant.m_Stats.m_iHeals, 8, "nothing more for that patient");
		CTR_Operation.AddCprSeconds(participant, 9, 30, 15, 120);
		CheckInt(participant.m_Stats.m_iHeals, 10, "another patient pays again");
		CTR_Operation.AddCprSeconds(participant, 9, 30, 0, 120);
		CheckInt(participant.m_Stats.m_iHeals, 10, "no interval configured");
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! Sessions merge per owner, only owners who entered get paid, settling the same operation again pays nothing, an
//! early exfil pays its completed tasks in full, a cancelled operation a quarter and one missing in action nothing.
class CTR_Test_SettlementPaysOnce : CTR_TestCase
{
	protected ref MRX_EconomyService m_Economy;
	protected ref CTR_Settlement m_First;
	protected ref CTR_Settlement m_Second;
	protected ref CTR_Settlement m_Cancelled;
	protected ref CTR_Settlement m_Missing;

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		m_Economy = CTR_PayoutTests.CreateEconomy();
		m_First = CreateSettlement(CTR_EOperationEnd.EARLY_EXFIL);
		m_First.GetOnDone().Insert(OnFirstDone);
		m_First.Pay(m_Economy);
	}

	//------------------------------------------------------------------------------------------------
	protected CTR_Settlement CreateSettlement(CTR_EOperationEnd end, string operationId = "test-op")
	{
		array<ref CTR_AreaInfo> areas = {};
		array<ref CTR_TaskOutcome> tasks = {
			CTR_PayoutTests.CreateTask("COE_ClearAreaTaskBuilder", true),
			CTR_PayoutTests.CreateTask("COE_FindIntelTaskBuilder", false)
		};

		CTR_Settlement settlement = new CTR_Settlement(CTR_Settings.CreateDefault(), operationId, end, 60, areas, tasks);
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
	//! The intel task was left by the early exfil; the completed area clearing pays in full.
	protected void OnFirstDone(CTR_Settlement settlement)
	{
		CTR_OperationResult a = settlement.BuildResult(5);
		CheckInt(a.m_eEnd, CTR_EOperationEnd.EARLY_EXFIL, "owner-a end");
		CheckInt(a.m_iPayPercent, 100, "owner-a share");
		CheckInt(a.m_Stats.m_iKills, 3, "sessions of owner-a merged");
		Check(a.m_Stats.m_bEnteredAO, "owner-a entered in one session");
		CheckInt(a.m_Payout.m_iTotal, 6000 + 3 * 250, "owner-a pay");
		CheckInt(a.m_ePayStatus, CTR_EPayStatus.PAID, "owner-a status");
		Check(a.m_bHasBalance, "owner-a balance known");
		CheckInt(a.m_iBalance, 6750, "owner-a balance");

		CTR_OperationResult b = settlement.BuildResult(2);
		CheckInt(b.m_Payout.m_iTotal, 0, "owner-b did not enter");
		CheckInt(b.m_ePayStatus, CTR_EPayStatus.NONE, "owner-b status");

		CTR_OperationResult noOwner = settlement.BuildResult(3);
		CheckInt(noOwner.m_Payout.m_iTotal, 6000, "player without owner earned pay");
		CheckInt(noOwner.m_ePayStatus, CTR_EPayStatus.NO_OWNER, "player without owner is not paid");

		CTR_OperationResult stranger = settlement.BuildResult(99);
		CheckInt(stranger.m_Payout.m_iTotal, 0, "player not in the operation");
		CheckInt(stranger.m_iParticipants, 2, "participants");
		CheckInt(stranger.m_iTeamPay, 6750 + 6000, "team pay");
		CheckInt(stranger.CountCompletedTasks(), 1, "completed tasks");

		m_Second = CreateSettlement(CTR_EOperationEnd.EARLY_EXFIL);
		m_Second.GetOnDone().Insert(OnSecondDone);
		m_Second.Pay(m_Economy);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSecondDone(CTR_Settlement settlement)
	{
		CTR_OperationResult a = settlement.BuildResult(1);
		CheckInt(a.m_ePayStatus, CTR_EPayStatus.ALREADY_PAID, "second settlement of the same operation");
		CheckInt(a.m_iBalance, 6750, "no second pay");

		m_Cancelled = CreateSettlement(CTR_EOperationEnd.CANCELLED, "test-op-cancelled");
		m_Cancelled.GetOnDone().Insert(OnCancelledDone);
		m_Cancelled.Pay(m_Economy);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCancelledDone(CTR_Settlement settlement)
	{
		CTR_OperationResult a = settlement.BuildResult(1);
		CheckInt(a.m_iPayPercent, 25, "cancelled: share");
		CheckInt(a.m_Payout.m_iTasks, 1500, "cancelled: a quarter of the task");
		CheckInt(a.m_Payout.m_iTotal, 1500 + 187, "cancelled: a quarter of the earnings");
		CheckInt(a.m_ePayStatus, CTR_EPayStatus.PAID, "cancelled: owner-a paid");
		CheckInt(a.m_iBalance, 6750 + 1687, "cancelled: balance");

		m_Missing = CreateSettlement(CTR_EOperationEnd.MISSING, "test-op-missing");
		m_Missing.GetOnDone().Insert(OnMissingDone);
		m_Missing.Pay(m_Economy);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMissingDone(CTR_Settlement settlement)
	{
		CTR_OperationResult a = settlement.BuildResult(1);
		CheckInt(a.m_eEnd, CTR_EOperationEnd.MISSING, "missing: end");
		CheckInt(a.m_iPayPercent, 0, "missing: share");
		CheckInt(a.m_Payout.m_iTotal, 0, "missing: nothing");
		CheckInt(a.m_ePayStatus, CTR_EPayStatus.NONE, "missing: nothing to pay");
		Check(!a.m_bHasBalance || a.m_iBalance == 6750 + 1687, "missing: balance unchanged");

		CTR_OperationResult b = settlement.BuildResult(2);
		CheckInt(b.m_Payout.m_iTotal, 0, "owner-b did not enter");
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
		result.m_bInProgress = true;
		result.m_bExfil = true;
		result.m_eEnd = CTR_EOperationEnd.ABANDONED;
		result.m_iPayPercent = 25;
		result.m_iDurationSeconds = 1234;
		CTR_AreaInfo area = new CTR_AreaInfo();
		area.m_sName = "Morton";
		area.m_fX = 4000;
		area.m_fZ = 5000;
		area.m_fRadius = 250;
		result.m_aAreas.Insert(area);
		CTR_TaskOutcome task = CTR_PayoutTests.CreateTask("COE_ClearAreaTaskBuilder", true);
		task.m_iAmount = 150;
		task.m_iReward = 150;
		task.m_fX = 4010;
		result.m_aTasks.Insert(task);
		CTR_TaskOutcome failed = CTR_PayoutTests.CreateTask("COE_FindIntelTaskBuilder", false);
		failed.m_bFailed = true;
		result.m_aTasks.Insert(failed);
		result.m_Stats = CTR_PayoutTests.CreateStats(true, 7, 1, 2, 3);
		result.m_Stats.m_fDistance = 1500.5;
		result.m_Payout = CTR_PayoutCalculator.Calculate(CTR_Settings.CreateDefault(), 150, result.m_Stats);
		result.m_iTotalIfSuccess = 9000;
		result.m_ePayStatus = CTR_EPayStatus.PAID;
		result.m_bHasBalance = true;
		result.m_iBalance = 420;
		result.m_sCurrency = "cash";

		string json = result.ToJson();
		CTR_OperationResult loaded = CTR_OperationResult.FromJson(json);
		Check(loaded != null, "parsed");
		if (loaded)
		{
			CheckString(loaded.m_sOperationId, "op-json", "operation id");
			Check(loaded.m_bInProgress, "in progress");
			Check(loaded.m_bExfil, "exfil");
			CheckInt(loaded.m_eEnd, CTR_EOperationEnd.ABANDONED, "end");
			CheckInt(loaded.m_iPayPercent, 25, "share");
			CheckInt(loaded.m_iDurationSeconds, 1234, "duration");
			CheckInt(loaded.m_aAreas.Count(), 1, "areas");
			CheckInt(loaded.m_aTasks.Count(), 2, "tasks");
			if (loaded.m_aTasks.Count() == 2)
			{
				CheckInt(loaded.m_aTasks[0].m_iAmount, 150, "task amount");
				CheckInt(loaded.m_aTasks[0].m_iReward, 150, "task reward");
				Check(loaded.m_aTasks[0].m_bCompleted, "task completed");
				Check(loaded.m_aTasks[1].m_bFailed && !loaded.m_aTasks[1].m_bCompleted, "task failed");
			}

			CheckInt(loaded.m_Stats.m_iKills, 7, "kills");
			CheckInt(loaded.m_Payout.m_iTotal, result.m_Payout.m_iTotal, "total");
			CheckInt(loaded.m_iTotalIfSuccess, 9000, "total if the exfil succeeds");
			CheckInt(loaded.m_ePayStatus, CTR_EPayStatus.PAID, "status");
			CheckInt(loaded.m_iBalance, 420, "balance");
		}

		Check(CTR_OperationResult.FromJson("not json") == null, "bad text is refused");
		Finish();
	}
}
#endif
