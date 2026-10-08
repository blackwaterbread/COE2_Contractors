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

	[Attribute("900", desc: "Exfil countdown: seconds from the end of the last task (or the early exfil order) to reach the exfil point. When it runs out the operation ends missing in action.", params: "60 3600", category: "Exfil")]
	int m_iExfilCountdownSeconds;

	[Attribute("30", desc: "Meters around the exfil point that count as being there (3D, vehicles included)", params: "5 200", category: "Exfil")]
	float m_fExfilRadius;

	[Attribute("0.75", desc: "Share of the living players outside the base who must be at the exfil point", params: "0.01 1", category: "Exfil")]
	float m_fExfilPlayerRatio;

	[Attribute("10", desc: "Seconds the players must hold the exfil point", params: "0 120", category: "Exfil")]
	int m_iExfilHoldSeconds;

	[Attribute("1000", desc: "Least distance from the edge of every AO to the exfil point, in meters", params: "0 10000", category: "Exfil")]
	float m_fExfilMinDistance;

	[Attribute("2000", desc: "Most distance from the edge of the nearest AO to the exfil point, in meters. 0 = no limit.", params: "0 20000", category: "Exfil")]
	float m_fExfilMaxDistance;

	[Attribute("25", desc: "Percent of the earnings (tasks, kills, heals) paid when the commander cancels before the exfil. Deductions stay whole.", params: "0 100", category: "Exfil")]
	int m_iCancelPayPercent;

	[Attribute("0", desc: "Percent of the earnings paid when the commander cancels during the exfil", params: "0 100", category: "Exfil")]
	int m_iExfilCancelPayPercent;

	[Attribute("0", desc: "Percent of the earnings paid when the exfil countdown runs out (missing in action)", params: "0 100", category: "Exfil")]
	int m_iMiaPayPercent;

	[Attribute("15", desc: "Seconds the players missing in action are held behind the result screen, with the enemies around them standing down, before they die", params: "0 120", category: "Exfil")]
	int m_iMiaDeathSeconds;

	[Attribute("15", desc: "Seconds between the commander cancelling the operation and the return to base", params: "0 120", category: "Exfil")]
	int m_iCancelReturnSeconds;

	[Attribute("0.17", desc: "Chance of an enemy pursuit when the exfil starts", params: "0 1", category: "Pursuit")]
	float m_fExfilEnemyChance;

	[Attribute("0.17", desc: "Chance added per civilian killed by players inside an AO during the operation (up to 1)", params: "0 1", category: "Pursuit")]
	float m_fExfilEnemyChancePerCivilian;

	[Attribute("1.5", desc: "Pursuers per living player outside the base, per wave", params: "0 10", category: "Pursuit")]
	float m_fExfilEnemyPerPlayer;

	[Attribute("8", desc: "Least pursuers per wave", params: "0 100", category: "Pursuit")]
	int m_iExfilEnemyMin;

	[Attribute("16", desc: "Most pursuers per wave", params: "0 100", category: "Pursuit")]
	int m_iExfilEnemyMax;

	[Attribute("3", desc: "Most waves of one pursuit", params: "1 10", category: "Pursuit")]
	int m_iExfilEnemyWaves;

	[Attribute("180", desc: "Seconds between pursuit waves", params: "10 1800", category: "Pursuit")]
	int m_iExfilEnemyWaveSeconds;

	[Attribute("200", desc: "Least distance of a wave from the nearest player, in meters", params: "50 2000", category: "Pursuit")]
	float m_fExfilEnemyMinDistance;

	[Attribute("350", desc: "Most distance of a wave from the nearest player, in meters", params: "50 2000", category: "Pursuit")]
	float m_fExfilEnemyMaxDistance;

	[Attribute("20", desc: "Seconds between pursuers turning towards the nearest player", params: "5 120", category: "Pursuit")]
	int m_iExfilEnemyRetargetSeconds;

	[Attribute(desc: "Gear every player respawns with, whatever the role: clothing, weapons, then the rest. It is issued: shops and loadouts give it no value. Empty = the built-in kit (CTR_StarterKit).", params: "et")]
	ref array<ResourceName> m_aStarterKit;

	[Attribute("75", desc: "Meters around the main base in which players cannot fire weapons, throw grenades or fire turrets. 0 = off.", params: "0 500")]
	float m_fSafeZoneRadius;

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
		settings.m_iExfilCountdownSeconds = 900;
		settings.m_fExfilRadius = 30;
		settings.m_fExfilPlayerRatio = 0.75;
		settings.m_iExfilHoldSeconds = 10;
		settings.m_fExfilMinDistance = 1000;
		settings.m_fExfilMaxDistance = 2000;
		settings.m_iCancelPayPercent = 25;
		settings.m_iExfilCancelPayPercent = 0;
		settings.m_iMiaPayPercent = 0;
		settings.m_iMiaDeathSeconds = 15;
		settings.m_iCancelReturnSeconds = 15;
		settings.m_fExfilEnemyChance = 0.17;
		settings.m_fExfilEnemyChancePerCivilian = 0.17;
		settings.m_fExfilEnemyPerPlayer = 1.5;
		settings.m_iExfilEnemyMin = 8;
		settings.m_iExfilEnemyMax = 16;
		settings.m_iExfilEnemyWaves = 3;
		settings.m_iExfilEnemyWaveSeconds = 180;
		settings.m_fExfilEnemyMinDistance = 200;
		settings.m_fExfilEnemyMaxDistance = 350;
		settings.m_iExfilEnemyRetargetSeconds = 20;
		settings.m_fSafeZoneRadius = 75;
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
	//! \return The configured respawn kit, or the built-in one.
	array<ResourceName> GetStarterKit()
	{
		if (m_aStarterKit && !m_aStarterKit.IsEmpty())
			return m_aStarterKit;

		return CTR_StarterKit.GetDefault();
	}

	//------------------------------------------------------------------------------------------------
	//! Percent of the earnings paid for an operation that ended this way.
	int GetPayPercent(CTR_EOperationEnd end)
	{
		switch (end)
		{
			case CTR_EOperationEnd.CANCELLED: return m_iCancelPayPercent;
			case CTR_EOperationEnd.ABANDONED: return m_iExfilCancelPayPercent;
			case CTR_EOperationEnd.MISSING: return m_iMiaPayPercent;
		}

		return 100;
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
