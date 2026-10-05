//! Pay of one owner (all sessions of a player in the operation).
class CTR_PayEntry : Managed
{
	string m_sOwnerId;
	ref array<int> m_aPlayerIds = {};
	ref CTR_PlayerStats m_Stats;
	ref CTR_Payout m_Payout;
	CTR_EPayStatus m_eStatus;
	bool m_bHasBalance;
	int m_iBalance;
	bool m_bRetried;
}

//------------------------------------------------------------------------------------------------
class CTR_PayCallback : MRX_TxCallback
{
	//! Weak: the settlement owns this callback.
	protected CTR_Settlement m_Settlement;
	protected CTR_PayEntry m_Entry;

	//------------------------------------------------------------------------------------------------
	void CTR_PayCallback(CTR_Settlement settlement, CTR_PayEntry entry)
	{
		m_Settlement = settlement;
		m_Entry = entry;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_TxResult result)
	{
		if (m_Settlement)
			m_Settlement.OnPaid(m_Entry, result);
	}
}

//------------------------------------------------------------------------------------------------
void CTR_SettlementDoneMethod(CTR_Settlement settlement);
typedef func CTR_SettlementDoneMethod;

//------------------------------------------------------------------------------------------------
//! Settles a closed operation once (server): prices the tasks, merges sessions per owner, credits every owner's
//! total through Marx under the key "op:<operation>:<owner>", then reports when all answers are in.
class CTR_Settlement : Managed
{
	protected static const int PAY_TIMEOUT_MS = 20000;
	protected static const int RETRY_DELAY_MS = 3000;

	protected ref CTR_Settings m_Settings;
	protected string m_sOperationId;
	protected bool m_bFinished;
	protected int m_iDurationSeconds;
	protected int m_iTaskPay;
	protected ref array<ref CTR_AreaInfo> m_aAreas;
	protected ref array<ref CTR_TaskOutcome> m_aTasks;
	protected ref array<ref CTR_PayEntry> m_aEntries = {};
	protected ref array<ref CTR_PayCallback> m_aCallbacks = {};
	protected MRX_EconomyService m_Economy;
	protected int m_iPending;
	protected bool m_bDone;
	protected ref ScriptInvokerBase<CTR_SettlementDoneMethod> m_OnDone = new ScriptInvokerBase<CTR_SettlementDoneMethod>();

	//------------------------------------------------------------------------------------------------
	void CTR_Settlement(notnull CTR_Settings settings, string operationId, bool finished, int durationSeconds, notnull array<ref CTR_AreaInfo> areas, notnull array<ref CTR_TaskOutcome> tasks)
	{
		m_Settings = settings;
		m_sOperationId = operationId;
		m_bFinished = finished;
		m_iDurationSeconds = durationSeconds;
		m_aAreas = areas;
		m_aTasks = tasks;
		m_iTaskPay = CTR_PayoutCalculator.PriceTasks(settings, tasks);
	}

	//------------------------------------------------------------------------------------------------
	void ~CTR_Settlement()
	{
		GetGame().GetCallqueue().Remove(OnTimeout);
		GetGame().GetCallqueue().Remove(Retry);
	}

	//------------------------------------------------------------------------------------------------
	ScriptInvokerBase<CTR_SettlementDoneMethod> GetOnDone()
	{
		return m_OnDone;
	}

	//------------------------------------------------------------------------------------------------
	string GetOperationId()
	{
		return m_sOperationId;
	}

	//------------------------------------------------------------------------------------------------
	bool IsFinished()
	{
		return m_bFinished;
	}

	//------------------------------------------------------------------------------------------------
	array<ref CTR_PayEntry> GetEntries()
	{
		return m_aEntries;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges the sessions per owner and calculates every pay. Call once, before Pay().
	void AddParticipants(notnull map<int, ref CTR_Participant> participants)
	{
		map<string, CTR_PayEntry> byOwner = new map<string, CTR_PayEntry>();
		foreach (int playerId, CTR_Participant participant : participants)
		{
			string key = participant.m_sOwnerId;
			if (key.IsEmpty())
				key = "player:" + playerId;

			CTR_PayEntry entry = byOwner.Get(key);
			if (entry)
			{
				entry.m_Stats.Merge(participant.m_Stats);
			}
			else
			{
				entry = new CTR_PayEntry();
				entry.m_sOwnerId = participant.m_sOwnerId;
				entry.m_Stats = participant.m_Stats.Copy();
				m_aEntries.Insert(entry);
				byOwner.Insert(key, entry);
			}

			entry.m_aPlayerIds.Insert(playerId);
		}

		foreach (CTR_PayEntry entry : m_aEntries)
		{
			entry.m_Payout = CTR_PayoutCalculator.Calculate(m_Settings, m_bFinished, m_iTaskPay, entry.m_Stats);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Credits every total above 0. GetOnDone() fires on a later frame when all answers are in, or after a timeout.
	void Pay(MRX_EconomyService economy)
	{
		m_Economy = economy;
		foreach (CTR_PayEntry entry : m_aEntries)
		{
			if (entry.m_Payout.m_iTotal <= 0)
			{
				entry.m_eStatus = CTR_EPayStatus.NONE;
				ReadCachedBalance(entry);
				continue;
			}

			if (entry.m_sOwnerId.IsEmpty() || !m_Economy)
			{
				entry.m_eStatus = CTR_EPayStatus.NO_OWNER;
				continue;
			}

			entry.m_eStatus = CTR_EPayStatus.PENDING;
			m_iPending++;
			Credit(entry);
		}

		GetGame().GetCallqueue().CallLater(OnTimeout, PAY_TIMEOUT_MS);
		GetGame().GetCallqueue().CallLater(CheckDone);
	}

	//------------------------------------------------------------------------------------------------
	protected void Credit(CTR_PayEntry entry)
	{
		CTR_PayCallback callback = new CTR_PayCallback(this, entry);
		m_aCallbacks.Insert(callback);
		string reason = string.Format("operation %1", m_sOperationId);
		MRX_TxContext context = MRX_TxContext.Create(CTR_PayoutCalculator.LEDGER_SOURCE, reason, CTR_PayoutCalculator.GetIdempotencyKey(m_sOperationId, entry.m_sOwnerId));
		m_Economy.Credit(entry.m_sOwnerId, m_Settings.m_sCurrency, entry.m_Payout.m_iTotal, context, callback);
	}

	//------------------------------------------------------------------------------------------------
	void OnPaid(CTR_PayEntry entry, MRX_TxResult result)
	{
		if (!entry || entry.m_eStatus != CTR_EPayStatus.PENDING)
			return;

		// A storage error may still have committed; retrying with the same key answers DUPLICATE in that case.
		if (result.m_eStatus == MRX_ETxStatus.STORAGE_ERROR && !entry.m_bRetried)
		{
			entry.m_bRetried = true;
			GetGame().GetCallqueue().CallLater(Retry, RETRY_DELAY_MS, false, entry);
			return;
		}

		if (result.m_eStatus == MRX_ETxStatus.OK)
			entry.m_eStatus = CTR_EPayStatus.PAID;
		else if (result.m_eStatus == MRX_ETxStatus.DUPLICATE)
			entry.m_eStatus = CTR_EPayStatus.ALREADY_PAID;
		else
			entry.m_eStatus = CTR_EPayStatus.FAILED;

		if (result.IsCommitted() && !result.m_aEntries.IsEmpty())
		{
			entry.m_bHasBalance = true;
			entry.m_iBalance = result.m_aEntries[0].m_iBalanceAfter;
		}
		else
		{
			ReadCachedBalance(entry);
		}

		if (entry.m_eStatus == CTR_EPayStatus.FAILED)
			Print(string.Format("[CTR] Pay of %1 for operation %2 failed: %3", entry.m_sOwnerId, m_sOperationId, typename.EnumToString(MRX_ETxStatus, result.m_eStatus)), LogLevel.ERROR);

		m_iPending--;
		CheckDone();
	}

	//------------------------------------------------------------------------------------------------
	protected void Retry(CTR_PayEntry entry)
	{
		if (entry && entry.m_eStatus == CTR_EPayStatus.PENDING && m_Economy)
			Credit(entry);
	}

	//------------------------------------------------------------------------------------------------
	protected void ReadCachedBalance(CTR_PayEntry entry)
	{
		if (!m_Economy || entry.m_sOwnerId.IsEmpty())
			return;

		int balance;
		if (m_Economy.TryGetCachedBalance(entry.m_sOwnerId, m_Settings.m_sCurrency, balance))
		{
			entry.m_bHasBalance = true;
			entry.m_iBalance = balance;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnTimeout()
	{
		if (m_bDone)
			return;

		Print(string.Format("[CTR] Pay of operation %1: %2 answers missing after %3 ms", m_sOperationId, m_iPending, PAY_TIMEOUT_MS), LogLevel.WARNING);
		m_iPending = 0;
		CheckDone();
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckDone()
	{
		if (m_bDone || m_iPending > 0)
			return;

		m_bDone = true;
		GetGame().GetCallqueue().Remove(OnTimeout);
		m_OnDone.Invoke(this);
	}

	//------------------------------------------------------------------------------------------------
	bool IsDone()
	{
		return m_bDone;
	}

	//------------------------------------------------------------------------------------------------
	//! The result screen of a player; also for players who joined after the operation ended.
	CTR_OperationResult BuildResult(int playerId, int returnDelaySeconds)
	{
		CTR_OperationResult result = new CTR_OperationResult();
		result.m_sOperationId = m_sOperationId;
		result.m_bFinished = m_bFinished;
		result.m_iDurationSeconds = m_iDurationSeconds;
		result.m_sCurrency = m_Settings.m_sCurrency;
		result.m_iReturnDelaySeconds = returnDelaySeconds;

		foreach (CTR_AreaInfo area : m_aAreas)
		{
			result.m_aAreas.Insert(area);
		}

		foreach (CTR_TaskOutcome task : m_aTasks)
		{
			result.m_aTasks.Insert(task);
		}

		foreach (CTR_PayEntry entry : m_aEntries)
		{
			if (entry.m_Stats.m_bEnteredAO)
				result.m_iParticipants++;

			result.m_iTeamPay += entry.m_Payout.m_iTotal;

			if (!entry.m_aPlayerIds.Contains(playerId))
				continue;

			result.m_Stats = entry.m_Stats;
			result.m_Payout = entry.m_Payout;
			result.m_ePayStatus = entry.m_eStatus;
			result.m_bHasBalance = entry.m_bHasBalance;
			result.m_iBalance = entry.m_iBalance;
		}

		if (!result.m_Stats)
		{
			result.m_Stats = new CTR_PlayerStats();
			result.m_Payout = new CTR_Payout();
		}

		return result;
	}
}
