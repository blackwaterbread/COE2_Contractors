modded class COE_GameMode
{
	//------------------------------------------------------------------------------------------------
	override protected void OnGameStart()
	{
		super.OnGameStart();

		if (Replication.IsServer())
			CTR_CheckSystems();
	}

	//------------------------------------------------------------------------------------------------
	//! COE2 and Marx both register a system through the vanilla systems config; log whether both run.
	protected void CTR_CheckSystems()
	{
		ChimeraWorld world = GetGame().GetWorld();
		bool enemySupport = world && world.FindSystem(COE_EnemySupportSystem) != null;
		bool marx = MRX_MarxSystem.GetInstance() != null;
		Print(string.Format("[CTR] Systems config %1: COE_EnemySupportSystem=%2, MRX_MarxSystem=%3", GetGame().GetSystemsConfig(), enemySupport, marx));

		if (!enemySupport || !marx)
			Print("[CTR] A required system is missing; check the World Systems Config", LogLevel.ERROR);
	}
}
