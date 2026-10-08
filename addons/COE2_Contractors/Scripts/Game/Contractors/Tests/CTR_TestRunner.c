#ifdef WORKBENCH
// Contractors test harness (Workbench only). Runs on server game start when Workbench was started with -ctrTests.
// Results are logged as "[CTR_TEST] PASS|FAIL|SKIP <test>" followed by "[CTR_TEST] DONE".

//------------------------------------------------------------------------------------------------
class CTR_TestCase : Managed
{
	protected CTR_TestRunner m_Runner;
	protected ref array<string> m_aFailures = {};
	protected bool m_bFinished;
	protected string m_sSkipReason;

	//------------------------------------------------------------------------------------------------
	void Start(notnull CTR_TestRunner runner)
	{
		m_Runner = runner;
		Run();
	}

	//------------------------------------------------------------------------------------------------
	bool IsFinished()
	{
		return m_bFinished;
	}

	//------------------------------------------------------------------------------------------------
	array<string> GetFailures()
	{
		return m_aFailures;
	}

	//------------------------------------------------------------------------------------------------
	//! Override for tests that wait for players or storage.
	int GetTimeoutMs()
	{
		return CTR_TestRunner.TIMEOUT_MS;
	}

	//------------------------------------------------------------------------------------------------
	string GetSkipReason()
	{
		return m_sSkipReason;
	}

	//------------------------------------------------------------------------------------------------
	//! Override. Call Finish() when done, synchronously or from a callback.
	protected void Run()
	{
		Finish();
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(bool condition, string message)
	{
		if (!condition)
			m_aFailures.Insert(message);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckInt(int actual, int expected, string message)
	{
		if (actual != expected)
			m_aFailures.Insert(string.Format("%1: expected %2, got %3", message, expected, actual));
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckString(string actual, string expected, string message)
	{
		if (actual != expected)
			m_aFailures.Insert(string.Format("%1: expected '%2', got '%3'", message, expected, actual));
	}

	//------------------------------------------------------------------------------------------------
	protected void Skip(string reason)
	{
		m_sSkipReason = reason;
		Finish();
	}

	//------------------------------------------------------------------------------------------------
	protected void Finish()
	{
		if (m_bFinished)
			return;

		m_bFinished = true;
		m_Runner.OnTestFinished(this);
	}
}

//------------------------------------------------------------------------------------------------
class CTR_TestRunner : Managed
{
	static const string TAG = "[CTR_TEST] ";
	static const int TIMEOUT_MS = 5000;
	static const string RUN_PARAM = "ctrTests";
	static const string AUTO_CLOSE_PARAM = "ctrTestsAutoClose";
	//! Players without a backend identity get a name-based test owner ID (see CTR_TestIdentity.c).
	static const string TEST_IDENTITY_PARAM = "ctrTestIdentity";
	static const int AUTO_CLOSE_DELAY_MS = 2000;

	protected static ref CTR_TestRunner s_Instance;

	protected ref array<ref CTR_TestCase> m_aTests = {};
	protected int m_iCurrent = -1;
	protected int m_iPassed;
	protected int m_iFailed;
	protected int m_iSkipped;

	//------------------------------------------------------------------------------------------------
	static void RunAll()
	{
		s_Instance = new CTR_TestRunner();
		CTR_StorageTests.Register(s_Instance);
		CTR_PayoutTests.Register(s_Instance);
		CTR_ExfilTests.Register(s_Instance);
		CTR_BaseTests.Register(s_Instance);
		CTR_GearTests.Register(s_Instance);
		// Last: it changes the world (AO, host position).
		CTR_OperationFlowTests.Register(s_Instance);
		Print(TAG + string.Format("START tests=%1", s_Instance.m_aTests.Count()));
		s_Instance.RunNext();
	}

	//------------------------------------------------------------------------------------------------
	void Add(notnull CTR_TestCase test)
	{
		m_aTests.Insert(test);
	}

	//------------------------------------------------------------------------------------------------
	void OnTestFinished(notnull CTR_TestCase test)
	{
		// Ignore tests that finish after their timeout was reported.
		if (m_aTests.Find(test) != m_iCurrent)
			return;

		Report(test, false);
		GetGame().GetCallqueue().CallLater(RunNext);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunNext()
	{
		m_iCurrent++;
		if (m_iCurrent >= m_aTests.Count())
		{
			Print(TAG + string.Format("DONE passed=%1 failed=%2 skipped=%3", m_iPassed, m_iFailed, m_iSkipped));
			GetGame().GetCallqueue().CallLater(Release);

			if (System.IsCLIParam(AUTO_CLOSE_PARAM))
				GetGame().GetCallqueue().CallLater(CloseGame, AUTO_CLOSE_DELAY_MS);

			return;
		}

		GetGame().GetCallqueue().CallLater(CheckTimeout, m_aTests[m_iCurrent].GetTimeoutMs(), false, m_iCurrent);
		m_aTests[m_iCurrent].Start(this);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Release()
	{
		s_Instance = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static void CloseGame()
	{
		Print(TAG + "closing the game (" + AUTO_CLOSE_PARAM + ")");
		GetGame().RequestClose();
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckTimeout(int index)
	{
		if (index != m_iCurrent || m_aTests[index].IsFinished())
			return;

		Report(m_aTests[index], true);
		RunNext();
	}

	//------------------------------------------------------------------------------------------------
	protected void Report(notnull CTR_TestCase test, bool timedOut)
	{
		array<string> failures = test.GetFailures();
		if (!timedOut && failures.IsEmpty() && !test.GetSkipReason().IsEmpty())
		{
			m_iSkipped++;
			Print(TAG + "SKIP " + test.ClassName() + ": " + test.GetSkipReason());
			return;
		}

		if (!timedOut && failures.IsEmpty())
		{
			m_iPassed++;
			Print(TAG + "PASS " + test.ClassName());
			return;
		}

		m_iFailed++;
		Print(TAG + "FAIL " + test.ClassName(), LogLevel.ERROR);
		foreach (string failure : failures)
		{
			Print(TAG + "  " + failure, LogLevel.ERROR);
		}

		if (timedOut)
			Print(TAG + string.Format("  timed out after %1 ms", test.GetTimeoutMs()), LogLevel.ERROR);
	}
}

//------------------------------------------------------------------------------------------------
modded class COE_GameMode
{
	//------------------------------------------------------------------------------------------------
	override protected void OnGameStart()
	{
		super.OnGameStart();
		if (Replication.IsServer() && System.IsCLIParam(CTR_TestRunner.RUN_PARAM))
			GetGame().GetCallqueue().CallLater(CTR_TestRunner.RunAll, 1000);
	}
}

//------------------------------------------------------------------------------------------------
//! Workbench sessions with -ctrTestIdentity (the launch script's default): a player without a backend identity gets a
//! stable owner ID derived from the player name. Workbench started without the launcher has no backend identity, and
//! Marx keeps wallet, shop and stash off for players without an owner.
modded class MRX_IdentityService
{
	//------------------------------------------------------------------------------------------------
	override protected string ResolveIdentity(int playerId)
	{
		string ownerId = super.ResolveIdentity(playerId);
		if (!ownerId.IsEmpty() || !System.IsCLIParam(CTR_TestRunner.TEST_IDENTITY_PARAM))
			return ownerId;

		ownerId = PersistenceIdUtils.FromString("ctr-test-identity:" + GetGame().GetPlayerManager().GetPlayerName(playerId));
		Print(string.Format("[CTR_TEST] player %1 has no backend identity, using test owner %2", playerId, ownerId), LogLevel.WARNING);
		return ownerId;
	}
}
#endif
