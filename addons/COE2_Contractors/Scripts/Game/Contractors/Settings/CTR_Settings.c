//! Reward for one completed task type: a COE2 task builder, optionally narrowed to one of its task prefabs
//! (COE_EnemyOfficerTaskBuilder is used for both "Kill Officer" and "Capture Officer").
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sBuilderClass")]
class CTR_TaskReward
{
	[Attribute(desc: "Class of the COE2 task builder, e.g. COE_DestroyCacheTaskBuilder")]
	string m_sBuilderClass;

	[Attribute(desc: "Task prefab spawned by the builder. Empty matches every prefab of the builder.", params: "et")]
	ResourceName m_sTaskPrefab;

	[Attribute("0", desc: "Paid to every participant when the task is completed", params: "0 inf")]
	int m_iAmount;

	//------------------------------------------------------------------------------------------------
	static CTR_TaskReward Create(string builderClass, int amount, ResourceName taskPrefab = string.Empty)
	{
		CTR_TaskReward reward = new CTR_TaskReward();
		reward.m_sBuilderClass = builderClass;
		reward.m_sTaskPrefab = taskPrefab;
		reward.m_iAmount = amount;
		return reward;
	}

	//------------------------------------------------------------------------------------------------
	bool Matches(string builderClass, ResourceName taskPrefab)
	{
		if (m_sBuilderClass != builderClass)
			return false;

		return m_sTaskPrefab.IsEmpty() || m_sTaskPrefab == taskPrefab;
	}
}

//! Operation pay. Amounts are in the Marx currency m_sCurrency. The default file is
//! Configs/Contractors/CTR_Settings.conf; server owners change it with Override in addon.
[BaseContainerProps(configRoot: true)]
class CTR_Settings
{
	static const ResourceName CONFIG = "{AFC310C32D25F1DC}Configs/Contractors/CTR_Settings.conf";

	[Attribute("cash", desc: "Marx currency of the pay")]
	string m_sCurrency;

	[Attribute(desc: "Pay per completed task type. The first matching entry counts, so entries with a task prefab go before the builder's entry without one.")]
	ref array<ref CTR_TaskReward> m_aTaskRewards;

	[Attribute("6000", desc: "Pay for a completed task whose builder has no entry", params: "0 inf")]
	int m_iDefaultTaskReward;

	[Attribute("250", desc: "Per enemy killed (players, AI, run over)", params: "0 inf")]
	int m_iKillReward;

	[Attribute("10000", desc: "Deducted per friendly or civilian killed", params: "0 inf")]
	int m_iTeamKillPenalty;

	[Attribute("2500", desc: "Deducted per death", params: "0 inf")]
	int m_iDeathPenalty;

	[Attribute("300", desc: "Per bandage applied to someone else and per CPR interval on a player whose heart stopped. Drugs pay nothing.", params: "0 inf")]
	int m_iFriendlyHealReward;

	[Attribute("15", desc: "Seconds of CPR (ACE Medical Circulation) that count as one treatment", params: "1 120")]
	int m_iCprRewardSeconds;

	[Attribute("120", desc: "Most seconds of CPR on one patient that pay in an operation (ACE revives in about a minute when the patient has enough blood)", params: "0 600")]
	int m_iCprMaxSecondsPerPatient;

	[Attribute("30", desc: "Seconds between the result screen and the return to base", params: "0 120")]
	int m_iReturnDelaySeconds;

	protected static ref CTR_Settings s_Instance;

	//------------------------------------------------------------------------------------------------
	//! Loaded once from CONFIG; built-in defaults when it cannot be loaded.
	static CTR_Settings Get()
	{
		if (s_Instance)
			return s_Instance;

		if (!CONFIG.IsEmpty())
			s_Instance = SCR_ConfigHelperT<CTR_Settings>.GetConfigObject(CONFIG);

		if (!s_Instance)
		{
			Print(string.Format("[CTR] Could not load %1, using built-in pay settings", CONFIG), LogLevel.ERROR);
			s_Instance = CreateDefault();
		}

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Defaults; see the plan for how the amounts were chosen. Attribute defaults only apply to configs.
	static CTR_Settings CreateDefault()
	{
		CTR_Settings settings = new CTR_Settings();
		settings.m_sCurrency = MRX_Settings.DEFAULT_CURRENCY;
		settings.m_iDefaultTaskReward = 6000;
		settings.m_iKillReward = 250;
		settings.m_iTeamKillPenalty = 10000;
		settings.m_iDeathPenalty = 2500;
		settings.m_iFriendlyHealReward = 300;
		settings.m_iCprRewardSeconds = 15;
		settings.m_iCprMaxSecondsPerPatient = 120;
		settings.m_iReturnDelaySeconds = 30;
		settings.m_aTaskRewards = {
			CTR_TaskReward.Create("COE_ClearAreaTaskBuilder", 6000),
			CTR_TaskReward.Create("COE_FindIntelTaskBuilder", 8000),
			CTR_TaskReward.Create("COE_DestroyCacheTaskBuilder", 8000),
			CTR_TaskReward.Create("COE_DemineEffectModuleTaskBuilder", 7000),
			CTR_TaskReward.Create("COE_DestroyInstallationTaskBuilder", 10000),
			CTR_TaskReward.Create("COE_DestroyVehicleTaskBuilder", 10000),
			CTR_TaskReward.Create("COE_EnemyOfficerTaskBuilder", 18000, "{DE9C612D13BF9B18}Prefabs/Tasks/KSC_TakeCaptiveTask.et"),
			CTR_TaskReward.Create("COE_EnemyOfficerTaskBuilder", 12000),
			CTR_TaskReward.Create("COE_FreeHostageTaskBuilder", 20000)
		};
		return settings;
	}

	//------------------------------------------------------------------------------------------------
	int GetTaskReward(string builderClass, ResourceName taskPrefab)
	{
		if (m_aTaskRewards)
		{
			foreach (CTR_TaskReward reward : m_aTaskRewards)
			{
				if (reward && reward.Matches(builderClass, taskPrefab))
					return reward.m_iAmount;
			}
		}

		return m_iDefaultTaskReward;
	}
}
