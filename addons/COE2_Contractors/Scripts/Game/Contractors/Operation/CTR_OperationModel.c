// Plain data of an operation. Everything here is serialized to JSON for the result screen, so fields stay primitive.

//------------------------------------------------------------------------------------------------
//! A task of the operation.
class CTR_TaskOutcome : Managed
{
	//! Builder task name (may be a localization key).
	string m_sName;
	//! Builder class and task prefab: the reward type.
	string m_sType;
	ResourceName m_sTaskPrefab;
	float m_fX;
	float m_fZ;
	bool m_bCompleted;
	bool m_bFailed;
	//! Pay per participant; 0 unless completed.
	int m_iAmount;
	//! What the task pays per participant when it is completed, whatever its state.
	int m_iReward;
}

//------------------------------------------------------------------------------------------------
//! An AO of the operation.
class CTR_AreaInfo : Managed
{
	string m_sName;
	float m_fX;
	float m_fZ;
	float m_fRadius;
}

//------------------------------------------------------------------------------------------------
//! What one player did during the operation.
class CTR_PlayerStats : Managed
{
	string m_sName;
	bool m_bEnteredAO;
	//! Enemies killed: players, AI, run over.
	int m_iKills;
	//! Friendlies killed.
	int m_iTeamKills;
	//! Civilians killed inside an AO.
	int m_iCivilianKills;
	int m_iDeaths;
	//! Bandages, tourniquets, saline and morphine applied to others.
	int m_iHeals;
	int m_iShots;
	//! Meters walked, driven and driven as a passenger.
	float m_fDistance;
	//! Seconds in the operation.
	int m_iSeconds;

	//------------------------------------------------------------------------------------------------
	//! Adds another session of the same owner (e.g. after a reconnect).
	void Merge(notnull CTR_PlayerStats other)
	{
		m_bEnteredAO = m_bEnteredAO || other.m_bEnteredAO;
		m_iKills += other.m_iKills;
		m_iTeamKills += other.m_iTeamKills;
		m_iCivilianKills += other.m_iCivilianKills;
		m_iDeaths += other.m_iDeaths;
		m_iHeals += other.m_iHeals;
		m_iShots += other.m_iShots;
		m_fDistance += other.m_fDistance;
		m_iSeconds += other.m_iSeconds;
	}

	//------------------------------------------------------------------------------------------------
	CTR_PlayerStats Copy()
	{
		CTR_PlayerStats copy = new CTR_PlayerStats();
		copy.m_sName = m_sName;
		copy.Merge(this);
		return copy;
	}
}

//------------------------------------------------------------------------------------------------
//! Pay of one player, by line. Deductions are negative. m_iTotal is never below 0.
class CTR_Payout : Managed
{
	int m_iTasks;
	int m_iKills;
	int m_iTeamKills;
	int m_iCivilianKills;
	int m_iDeaths;
	int m_iHeals;
	int m_iTotal;
}

//------------------------------------------------------------------------------------------------
enum CTR_EPayStatus
{
	//! Nothing to pay (aborted, not in the AO, or a total of 0).
	NONE,
	PAID,
	//! The pay of this operation was already committed earlier.
	ALREADY_PAID,
	//! The player has no owner ID (no backend identity), so Marx cannot pay.
	NO_OWNER,
	FAILED,
	//! No answer from storage in time; the pay may still arrive.
	PENDING
}

//------------------------------------------------------------------------------------------------
//! How an operation ended.
enum CTR_EOperationEnd
{
	//! The players reached the exfil point after every task was finished.
	COMPLETE,
	//! The players reached the exfil point after the commander ordered an early exfil, with tasks left.
	EARLY_EXFIL,
	//! Every task failed: no exfil, everyone returns at once.
	FAILED,
	//! The exfil countdown ran out.
	MISSING,
	//! The commander cancelled during the exfil.
	ABANDONED,
	//! The commander cancelled before the exfil.
	CANCELLED
}

//------------------------------------------------------------------------------------------------
//! The result screen of one player, or the state of a running operation so far.
class CTR_OperationResult : Managed
{
	string m_sOperationId;
	//! The operation still runs: nothing is paid yet.
	bool m_bInProgress;
	//! In progress: the exfil has started.
	bool m_bExfil;
	//! After the operation: how it ended (CTR_EOperationEnd).
	int m_eEnd;
	//! After the operation: percent of the earnings paid (deductions stay whole).
	int m_iPayPercent = 100;
	//! Missing in action: this player is held behind the result screen and dies in that many seconds.
	bool m_bMissing;
	float m_fMissingSeconds;
	int m_iDurationSeconds;
	ref array<ref CTR_AreaInfo> m_aAreas = {};
	ref array<ref CTR_TaskOutcome> m_aTasks = {};

	//! Players who entered an AO.
	int m_iParticipants;
	//! Sum of all totals; in progress, of the totals if the exfil succeeds of the players who entered an AO.
	int m_iTeamPay;

	ref CTR_PlayerStats m_Stats;
	ref CTR_Payout m_Payout;
	//! In progress: the total if the exfil succeeds (CTR_PayoutCalculator.CalculateIfSuccess).
	int m_iTotalIfSuccess;
	CTR_EPayStatus m_ePayStatus;
	string m_sCurrency;
	bool m_bHasBalance;
	int m_iBalance;

	//------------------------------------------------------------------------------------------------
	int CountCompletedTasks()
	{
		int count;
		foreach (CTR_TaskOutcome task : m_aTasks)
		{
			if (task.m_bCompleted)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	string ToJson()
	{
		return Serialize(this);
	}

	//------------------------------------------------------------------------------------------------
	static string Serialize(notnull CTR_OperationResult result)
	{
		JsonSaveContext context = new JsonSaveContext();
		context.EnableTypeDiscriminator(false);
		context.WriteValue(string.Empty, result);
		return context.SaveToString();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Null when the text cannot be read.
	static CTR_OperationResult FromJson(string json)
	{
		JsonLoadContext context = new JsonLoadContext();
		context.EnableTypeDiscriminator(false);
		if (!context.LoadFromString(json))
			return null;

		CTR_OperationResult result = new CTR_OperationResult();
		if (!context.ReadValue(string.Empty, result))
			return null;

		return result;
	}
}
