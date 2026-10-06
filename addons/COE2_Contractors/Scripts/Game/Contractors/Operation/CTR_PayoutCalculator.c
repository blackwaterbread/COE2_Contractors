//! Pay rules, without engine dependencies:
//! - An operation ended early (the commander cancels the AO) pays its completed tasks like a finished one.
//! - Only players who entered an AO are paid.
//! - Every completed task pays its amount to every participant; failed tasks pay nothing.
//! - Without a completed task there is no pay at all, so personal lines cannot pay on their own.
//! - Personal lines: kills and heals add, team kills and deaths deduct. The total is never below 0.
class CTR_PayoutCalculator
{
	static const string LEDGER_SOURCE = "coe2_contractors";

	//------------------------------------------------------------------------------------------------
	//! Sets m_iAmount of every task from the settings and returns the sum of the completed ones.
	static int PriceTasks(notnull CTR_Settings settings, notnull array<ref CTR_TaskOutcome> tasks)
	{
		int sum;
		foreach (CTR_TaskOutcome task : tasks)
		{
			task.m_iAmount = 0;
			if (!task.m_bCompleted)
				continue;

			task.m_iAmount = settings.GetTaskReward(task.m_sType, task.m_sTaskPrefab);
			sum += task.m_iAmount;
		}

		return sum;
	}

	//------------------------------------------------------------------------------------------------
	static CTR_Payout Calculate(notnull CTR_Settings settings, int taskPay, notnull CTR_PlayerStats stats)
	{
		CTR_Payout payout = new CTR_Payout();
		if (!stats.m_bEnteredAO || taskPay <= 0)
			return payout;

		payout.m_iTasks = taskPay;
		SetPersonalLines(settings, stats, payout);
		payout.m_iTotal = Math.Max(0, payout.m_iTasks + payout.m_iKills + payout.m_iHeals + payout.m_iTeamKills + payout.m_iDeaths);
		return payout;
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
