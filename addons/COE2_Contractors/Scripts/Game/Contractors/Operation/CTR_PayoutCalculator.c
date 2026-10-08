//! Pay rules, without engine dependencies:
//! - Only players who entered an AO are paid.
//! - Every completed task pays its amount to every participant; failed tasks pay nothing.
//! - Without a completed task there is no pay at all, so personal lines cannot pay on their own.
//! - Personal lines: kills and heals add, team kills and deaths deduct. The total is never below 0.
//! - How the operation ended sets a percent of the earnings (tasks, kills, heals); deductions stay whole.
class CTR_PayoutCalculator
{
	static const string LEDGER_SOURCE = "coe2_contractors";

	//------------------------------------------------------------------------------------------------
	//! Sets m_iReward of every task and m_iAmount of the completed ones from the settings. \return Sum of the completed.
	static int PriceTasks(notnull CTR_Settings settings, notnull array<ref CTR_TaskOutcome> tasks)
	{
		int sum;
		foreach (CTR_TaskOutcome task : tasks)
		{
			task.m_iReward = settings.GetTaskReward(task.m_sType, task.m_sTaskPrefab);
			task.m_iAmount = 0;
			if (!task.m_bCompleted)
				continue;

			task.m_iAmount = task.m_iReward;
			sum += task.m_iAmount;
		}

		return sum;
	}

	//------------------------------------------------------------------------------------------------
	//! Task pay if every task that has not failed gets completed (after PriceTasks).
	static int SumRewardsStillPossible(notnull array<ref CTR_TaskOutcome> tasks)
	{
		int sum;
		foreach (CTR_TaskOutcome task : tasks)
		{
			if (!task.m_bFailed)
				sum += task.m_iReward;
		}

		return sum;
	}

	//------------------------------------------------------------------------------------------------
	//! Running operation: the total if the exfil succeeds with this task pay, with the personal lines so far. Counts as if
	//! the player enters the AO, which they must to be paid.
	static int CalculateIfSuccess(notnull CTR_Settings settings, int taskPay, notnull CTR_PlayerStats stats)
	{
		if (taskPay <= 0)
			return 0;

		CTR_Payout payout = new CTR_Payout();
		payout.m_iTasks = taskPay;
		SetPersonalLines(settings, stats, payout);
		return GetTotal(payout);
	}

	//------------------------------------------------------------------------------------------------
	//! \param percent Share of the earnings paid (how the operation ended); deductions stay whole.
	static CTR_Payout Calculate(notnull CTR_Settings settings, int taskPay, notnull CTR_PlayerStats stats, int percent = 100)
	{
		CTR_Payout payout = new CTR_Payout();
		if (!stats.m_bEnteredAO || taskPay <= 0)
			return payout;

		payout.m_iTasks = ApplyPercent(taskPay, percent);
		SetPersonalLines(settings, stats, payout);
		payout.m_iKills = ApplyPercent(payout.m_iKills, percent);
		payout.m_iHeals = ApplyPercent(payout.m_iHeals, percent);
		payout.m_iTotal = GetTotal(payout);
		return payout;
	}

	//------------------------------------------------------------------------------------------------
	//! Share of an amount, rounded down.
	static int ApplyPercent(int amount, int percent)
	{
		return amount * Math.Clamp(percent, 0, 100) / 100;
	}

	//------------------------------------------------------------------------------------------------
	//! Sum of the lines, never below 0.
	protected static int GetTotal(notnull CTR_Payout payout)
	{
		return Math.Max(0, payout.m_iTasks + payout.m_iKills + payout.m_iHeals + payout.m_iTeamKills + payout.m_iDeaths);
	}

	//------------------------------------------------------------------------------------------------
	//! Running operation, as if it ended now: like Calculate, but the personal lines of a player who entered an AO show
	//! before a task is completed too, so the player sees what they are worth. They do not pay yet (total 0).
	static CTR_Payout CalculateSoFar(notnull CTR_Settings settings, int taskPay, notnull CTR_PlayerStats stats)
	{
		CTR_Payout payout = Calculate(settings, taskPay, stats);
		if (stats.m_bEnteredAO && taskPay <= 0)
			SetPersonalLines(settings, stats, payout);

		return payout;
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetPersonalLines(notnull CTR_Settings settings, notnull CTR_PlayerStats stats, notnull CTR_Payout payout)
	{
		payout.m_iKills = stats.m_iKills * settings.m_iKillReward;
		payout.m_iHeals = stats.m_iHeals * settings.m_iFriendlyHealReward;
		payout.m_iTeamKills = -stats.m_iTeamKills * settings.m_iTeamKillPenalty;
		payout.m_iDeaths = -stats.m_iDeaths * settings.m_iDeathPenalty;
	}

	//------------------------------------------------------------------------------------------------
	//! One pay per operation and owner, whatever happens to the server in between.
	static string GetIdempotencyKey(string operationId, string ownerId)
	{
		return "op:" + operationId + ":" + ownerId;
	}
}
