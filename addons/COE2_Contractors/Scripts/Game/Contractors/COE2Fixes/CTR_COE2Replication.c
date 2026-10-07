//! COE2 bug (fix proposed upstream: blackwaterbread/COE2_AR, branch fix-dedicated-client-issues): on clients the factions
//! and the insertion point are only resolved when their replicated IDs change (the insertion point once, 1 s later). A
//! player who joins later, or whose insertion point has not streamed in yet, keeps them empty: no enemy color on the map,
//! Deploy and building mode unavailable. Here proxies resolve them from the replicated IDs when they are read. Harmless
//! once COE2 is fixed (they are found already); remove then.
modded class COE_FactionManager
{
	//------------------------------------------------------------------------------------------------
	override Faction GetPlayerFaction()
	{
		Faction faction = super.GetPlayerFaction();
		if (!faction && !Replication.IsServer())
		{
			faction = GetFactionByIndex(m_iPlayerFactionId);
			m_pPlayerFaction = faction;
		}

		return faction;
	}

	//------------------------------------------------------------------------------------------------
	override Faction GetEnemyFaction()
	{
		Faction faction = super.GetEnemyFaction();
		if (!faction && !Replication.IsServer())
		{
			faction = GetFactionByIndex(m_iEnemyFactionId);
			m_pEnemyFaction = faction;
		}

		return faction;
	}

	//------------------------------------------------------------------------------------------------
	override Faction GetCivilianFaction()
	{
		Faction faction = super.GetCivilianFaction();
		if (!faction && !Replication.IsServer())
		{
			faction = GetFactionByIndex(m_iCivilianFactionId);
			m_pCivilianFaction = faction;
		}

		return faction;
	}
}

//------------------------------------------------------------------------------------------------
modded class COE_GameMode
{
	//------------------------------------------------------------------------------------------------
	override SCR_SpawnPoint GetInsertionPoint()
	{
		SCR_SpawnPoint insertionPoint = super.GetInsertionPoint();
		if (insertionPoint || Replication.IsServer() || m_iInsertionPointId == Replication.INVALID_ID)
			return insertionPoint;

		RplComponent rpl = RplComponent.Cast(Replication.FindItem(m_iInsertionPointId));
		if (rpl)
			m_pInsertionPoint = SCR_SpawnPoint.Cast(rpl.GetEntity());

		return m_pInsertionPoint;
	}
}
