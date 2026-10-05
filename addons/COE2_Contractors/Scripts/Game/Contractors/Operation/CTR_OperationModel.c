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
	//! Pay per participant; 0 unless completed.
	int m_iAmount;
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
	//! Friendlies and civilians killed.
	int m_iTeamKills;
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
//! The result screen of one player.
class CTR_OperationResult : Managed
{
	string m_sOperationId;
	//! All tasks finished (false: cancelled before).
	bool m_bFinished;
	int m_iDurationSeconds;
	ref array<ref CTR_AreaInfo> m_aAreas = {};
	ref array<ref CTR_TaskOutcome> m_aTasks = {};

	//! Players who entered an AO.
	int m_iParticipants;
	//! Sum of all totals.
	int m_iTeamPay;

	ref CTR_PlayerStats m_Stats;
	ref CTR_Payout m_Payout;
	CTR_EPayStatus m_ePayStatus;
	string m_sCurrency;
	bool m_bHasBalance;
	int m_iBalance;
	//! Seconds until everyone returns to base; 0 = no return.
	int m_iReturnDelaySeconds;

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
	//! Paid at least one completed task.
	bool IsSuccess()
	{
		return m_bFinished && CountCompletedTasks() > 0;
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
