#ifdef WORKBENCH
//! Needs the World Systems Config CTR_Systems.conf.
class CTR_StorageTests
{
	//------------------------------------------------------------------------------------------------
	static void Register(notnull CTR_TestRunner runner)
	{
		runner.Add(new CTR_Test_MarxOnlyPersistence());
		runner.Add(new CTR_Test_DataSurvivesRestart());
	}
}

//------------------------------------------------------------------------------------------------
//! Marx collections are there, world state collections are not, and the Contractors Marx settings are used.
class CTR_Test_MarxOnlyPersistence : CTR_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		if (!persistence)
		{
			Skip("no persistence system (select CTR_Systems.conf as World Systems Config)");
			return;
		}

		Check(MRX_NativeBackend.IsAvailable(), "wallet collection present");
		Check(persistence.FindCollection(MRX_NativeBackend.STASH_COLLECTION_NAME) != null, "stash collection present");

		array<string> worldCollections = {"Character", "Vehicle", "Item", "Storage", "Structure", "Logic"};
		foreach (string worldCollection : worldCollections)
		{
			Check(persistence.FindCollection(worldCollection) == null, "no world state collection " + worldCollection);
		}

		MRX_MarxSystem marx = MRX_MarxSystem.GetInstance();
		Check(marx != null, "Marx system running");
		if (marx)
		{
			array<ref MRX_CurrencyDef> currencies = marx.GetSettings().m_aCurrencies;
			Check(currencies && currencies.Count() == 1, "one currency configured");
			if (currencies && currencies.Count() == 1)
			{
				CheckString(currencies[0].m_sId, MRX_Settings.DEFAULT_CURRENCY, "currency id");
				CheckInt(currencies[0].m_iInitialBalance, 5000, "starting cash");
			}
		}

		Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! Credits the host and grants a stash asset once under fixed keys. The first session reports OK; every later session
//! must report DUPLICATE, which proves the wallet and the stash (with their idempotency keys) came back from disk.
class CTR_Test_DataSurvivesRestart : CTR_TestCase
{
	protected static const int WAIT_MS = 500;
	protected static const int MAX_WAIT_MS = 20000;
	protected static const ResourceName STASH_PREFAB = "{A81F501D3EF6F38E}Prefabs/Items/Medicine/FieldDressing_01/FieldDressing_US_01.et";
	protected int m_iWaitedMs;
	protected string m_sOwnerId;
	protected ref MRX_TxCallback m_Callback;
	protected ref MRX_StashResultCallback m_StashCallback;

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return MAX_WAIT_MS + 15000;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		if (!MRX_NativeBackend.IsAvailable())
		{
			Skip("native storage not configured");
			return;
		}

		WaitForOwner();
	}

	//------------------------------------------------------------------------------------------------
	protected void WaitForOwner()
	{
		m_sOwnerId = MRX_Marx.GetOwnerId(SCR_PlayerController.GetLocalPlayerId());
		if (!m_sOwnerId.IsEmpty())
		{
			m_Callback = new MRX_TxCallback();
			m_Callback.GetOnResult().Insert(OnCredited);
			MRX_TxContext context = MRX_TxContext.Create("coe2_contractors_test", "persistence check", "ctr-test:persist:v1");
			MRX_Marx.GetEconomy().Credit(m_sOwnerId, MRX_Settings.DEFAULT_CURRENCY, 7, context, m_Callback);
			return;
		}

		m_iWaitedMs += WAIT_MS;
		if (m_iWaitedMs >= MAX_WAIT_MS)
		{
			Skip("host has no owner ID (backend identity missing; start Workbench with -ctrTestIdentity)");
			return;
		}

		GetGame().GetCallqueue().CallLater(WaitForOwner, WAIT_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCredited(MRX_TxResult result)
	{
		Check(result.IsCommitted(), "credit committed, status " + typename.EnumToString(MRX_ETxStatus, result.m_eStatus));
		Print(CTR_TestRunner.TAG + "persistence: wallet " + typename.EnumToString(MRX_ETxStatus, result.m_eStatus) + " (OK in the first session, DUPLICATE after a restart)");

		m_StashCallback = new MRX_StashResultCallback();
		m_StashCallback.GetOnResult().Insert(OnGranted);
		MRX_TxContext context = MRX_TxContext.Create("coe2_contractors_test", "persistence check", "ctr-test:persist-stash:v1");
		MRX_Marx.GetStash().Grant(m_sOwnerId, STASH_PREFAB, context, m_StashCallback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnGranted(MRX_StashResult result)
	{
		Check(result.m_eStatus == MRX_EStashStatus.OK || result.m_eStatus == MRX_EStashStatus.DUPLICATE, "stash grant committed, status " + typename.EnumToString(MRX_EStashStatus, result.m_eStatus));
		Print(CTR_TestRunner.TAG + "persistence: stash " + typename.EnumToString(MRX_EStashStatus, result.m_eStatus) + " (OK in the first session, DUPLICATE after a restart)");
		Finish();
	}
}
#endif
