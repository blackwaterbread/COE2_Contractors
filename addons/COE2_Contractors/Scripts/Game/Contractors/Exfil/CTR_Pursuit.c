//! Enemy pursuit during an exfil (server): waves of enemy infantry spawn right behind the players and chase them, so the
//! players feel hunted and have to run. Rolled when the exfil starts, and again for every civilian killed in an AO until
//! a pursuit comes; its chance grows with the civilians killed. One pursuit per operation, up to a few waves.
class CTR_Pursuit : Managed
{
	//! Not closer than this to any player.
	protected static const float MIN_PLAYER_DISTANCE = 150;
	protected static const int SPAWN_TRIES = 12;
	//! Between the alert and the wave.
	protected static const int WARNING_MS = 5000;
	protected static const float GROUP_SPREAD = 12;

	protected vector m_vExfil;
	protected ref CTR_Settings m_Settings;
	protected bool m_bStarted;
	protected int m_iWave;
	protected vector m_vNextSpawn;
	//! Spawned groups (weak).
	protected ref array<AIGroup> m_aGroups = {};

	//------------------------------------------------------------------------------------------------
	void CTR_Pursuit(vector exfil, notnull CTR_Settings settings)
	{
		m_vExfil = exfil;
		m_Settings = settings;
	}

	//------------------------------------------------------------------------------------------------
	void ~CTR_Pursuit()
	{
		Stop();
	}

	//------------------------------------------------------------------------------------------------
	bool IsStarted()
	{
		return m_bStarted;
	}

	//------------------------------------------------------------------------------------------------
	int GetWave()
	{
		return m_iWave;
	}

	//------------------------------------------------------------------------------------------------
	//! Rolls for a pursuit unless one came already. \return True when it starts now.
	bool Roll(int civilians)
	{
		if (m_bStarted)
			return false;

		float chance = CTR_ExfilRules.GetPursuitChance(m_Settings.m_fExfilEnemyChance, m_Settings.m_fExfilEnemyChancePerCivilian, civilians);
		bool hit = Math.RandomFloat01() < chance;
		Print(string.Format("[CTR] Pursuit roll: %1 civilians, chance %2, %3", civilians, chance, hit));
		if (hit)
			Start();

		return hit;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the pursuit now: the first wave at once, the next ones at the wave interval.
	void Start()
	{
		if (m_bStarted)
			return;

		m_bStarted = true;
		WarnNextWave();
		GetGame().GetCallqueue().CallLater(Retarget, m_Settings.m_iExfilEnemyRetargetSeconds * 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	//! No more waves and no more chasing; the pursuers stay until the AO ends.
	void Stop()
	{
		ScriptCallQueue callQueue = GetGame().GetCallqueue();
		if (!callQueue)
			return;

		callQueue.Remove(WarnNextWave);
		callQueue.Remove(SpawnWave);
		callQueue.Remove(Retarget);
	}

	//------------------------------------------------------------------------------------------------
	//! Picks where the next wave comes from, alerts everyone, spawns it shortly after and plans the one after it.
	protected void WarnNextWave()
	{
		m_iWave++;
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode)
			return;

		bool more = m_iWave < m_Settings.m_iExfilEnemyWaves;
		gameMode.CTR_SetPursuitWave(m_iWave, more, m_Settings.m_iExfilEnemyWaveSeconds);
		if (more)
			GetGame().GetCallqueue().CallLater(WarnNextWave, m_Settings.m_iExfilEnemyWaveSeconds * 1000);

		array<SCR_ChimeraCharacter> players = {};
		CTR_Exfil.CollectOutsideBase(gameMode.GetMainBasePos(), players);
		vector center;
		if (players.IsEmpty() || !FindSpawnPos(players, m_vNextSpawn, center))
		{
			// Nobody to chase, or nowhere to come from: the wave is lost.
			Print(string.Format("[CTR] Pursuit wave %1 skipped: %2 players outside the base", m_iWave, players.Count()), LogLevel.WARNING);
			return;
		}

		gameMode.CTR_AlertAll(CTR_EAlert.PURSUIT, CTR_ExfilRules.GetCompassOctant(center, m_vNextSpawn));
		GetGame().GetCallqueue().CallLater(SpawnWave, WARNING_MS, false, players.Count());
	}

	//------------------------------------------------------------------------------------------------
	//! Behind the players: from the exfil point through their centre, 200-350 m beyond the rearmost of them, on land and
	//! away from everyone. Any direction when nothing fits behind them.
	protected bool FindSpawnPos(notnull array<SCR_ChimeraCharacter> players, out vector pos, out vector center)
	{
		center = vector.Zero;
		foreach (SCR_ChimeraCharacter player : players)
		{
			center = center + player.GetOrigin();
		}

		center = center * (1.0 / players.Count());
		vector back = center - m_vExfil;
		float backYaw = Math.Atan2(back[0], back[2]);
		if (vector.DistanceXZ(center, m_vExfil) < 1)
			backYaw = Math.RandomFloat(0, Math.PI2);

		// The rearmost player: furthest along the way back from the exfil point.
		vector backDir = Vector(Math.Sin(backYaw), 0, Math.Cos(backYaw));
		vector rear = players[0].GetOrigin();
		foreach (SCR_ChimeraCharacter other : players)
		{
			if (vector.Dot(other.GetOrigin() - rear, backDir) > 0)
				rear = other.GetOrigin();
		}

		for (int i = 0; i < SPAWN_TRIES * 2; i++)
		{
			// Half the tries straight behind (with some spread), the rest anywhere.
			float spread = 35;
			if (i >= SPAWN_TRIES)
				spread = 180;

			float yaw = backYaw + Math.RandomFloat(-spread, spread) * Math.DEG2RAD;
			vector dir = Vector(Math.Sin(yaw), 0, Math.Cos(yaw));
			vector candidate = rear + dir * Math.RandomFloat(m_Settings.m_fExfilEnemyMinDistance, m_Settings.m_fExfilEnemyMaxDistance);
			if (IsSpawnPos(candidate, players, pos))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsSpawnPos(vector candidate, notnull array<SCR_ChimeraCharacter> players, out vector pos)
	{
		vector mins, maxs;
		GetGame().GetWorld().GetBoundBox(mins, maxs);
		if (candidate[0] < mins[0] || candidate[2] < mins[2] || candidate[0] > maxs[0] || candidate[2] > maxs[2])
			return false;

		if (KSC_TerrainHelper.SurfaceIsWater(candidate) || !SCR_WorldTools.FindEmptyTerrainPosition(pos, candidate, 30, 2, 2))
			return false;

		if (KSC_TerrainHelper.SurfaceIsWater(pos))
			return false;

		foreach (SCR_ChimeraCharacter player : players)
		{
			if (vector.DistanceXZ(player.GetOrigin(), pos) < MIN_PLAYER_DISTANCE)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Enemy groups (medium and large, else any) until the wave has its size, chasing the nearest player.
	protected void SpawnWave(int playersOutside)
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		COE_FactionManager factionManager = COE_FactionManager.Cast(GetGame().GetFactionManager());
		if (!gameMode || !factionManager || !factionManager.GetEnemyFaction())
			return;

		array<ResourceName> prefabs = {};
		factionManager.GetFactionEntityListWithLabel(factionManager.GetEnemyFaction(), EEntityCatalogType.GROUP, EEditableEntityLabel.GROUPSIZE_MEDIUM, prefabs);
		factionManager.GetFactionEntityListWithLabel(factionManager.GetEnemyFaction(), EEntityCatalogType.GROUP, EEditableEntityLabel.GROUPSIZE_LARGE, prefabs);
		if (prefabs.IsEmpty())
			factionManager.GetFactionEntityListWithLabel(factionManager.GetEnemyFaction(), EEntityCatalogType.GROUP, EEditableEntityLabel.ENTITYTYPE_GROUP, prefabs);

		if (prefabs.IsEmpty())
		{
			Print("[CTR] Pursuit: the enemy faction has no groups", LogLevel.ERROR);
			return;
		}

		COE_AO ao = FindNearestAO(gameMode, m_vNextSpawn);
		int size = CTR_ExfilRules.GetWaveSize(playersOutside, m_Settings.m_fExfilEnemyPerPlayer, m_Settings.m_iExfilEnemyMin, m_Settings.m_iExfilEnemyMax);
		int spawned;
		int groups;
		while (spawned < size && groups < size)
		{
			vector pos = m_vNextSpawn;
			SCR_WorldTools.FindEmptyTerrainPosition(pos, m_vNextSpawn + Vector(Math.RandomFloat(-GROUP_SPREAD, GROUP_SPREAD), 0, Math.RandomFloat(-GROUP_SPREAD, GROUP_SPREAD)), GROUP_SPREAD);
			SCR_AIGroup group = SCR_AIGroup.Cast(KSC_GameTools.SpawnGroupPrefab(prefabs.GetRandomElement(), pos));
			groups++;
			if (!group)
				continue;

			spawned += CountUnits(group);
			// The AO deletes the group when it ends; the waypoints are handled here.
			if (ao)
				ao.AddGroup(group);

			m_aGroups.Insert(group);
			Chase(group, ao);
		}

		Print(string.Format("[CTR] Pursuit wave %1: %2 groups, %3 AI at %4 (%5 players outside the base)", m_iWave, groups, spawned, m_vNextSpawn, playersOutside));
	}

	//------------------------------------------------------------------------------------------------
	protected static int CountUnits(notnull SCR_AIGroup group)
	{
		// What the group spawns (a prefab may have more slots than it fills).
		if (group.GetNumberOfMembersToSpawn() > 0)
			return group.GetNumberOfMembersToSpawn();

		array<ResourceName> units;
		if (group.GetPrefabData() && group.GetPrefabData().GetPrefab())
			group.GetPrefabData().GetPrefab().Get("m_aUnitPrefabSlots", units);

		if (!units)
			return 1;

		return Math.Max(1, units.Count());
	}

	//------------------------------------------------------------------------------------------------
	protected static COE_AO FindNearestAO(notnull COE_GameMode gameMode, vector pos)
	{
		COE_AO nearest;
		float distance = float.MAX;
		foreach (COE_AO ao : gameMode.GetCurrentAOs())
		{
			if (ao && vector.DistanceSqXZ(ao.GetOrigin(), pos) < distance)
			{
				nearest = ao;
				distance = vector.DistanceSqXZ(ao.GetOrigin(), pos);
			}
		}

		return nearest;
	}

	//------------------------------------------------------------------------------------------------
	//! Every group heads for the nearest player again, wherever they went.
	protected void Retarget()
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode)
			return;

		foreach (AIGroup group : m_aGroups)
		{
			if (group)
				Chase(group, FindNearestAO(gameMode, group.GetOrigin()));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the group's waypoint with a search and destroy at the nearest living player outside the base. A waypoint
	//! added on top of the old one would be walked only after it, so the old one goes first.
	protected void Chase(AIGroup group, COE_AO ao)
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!group || !gameMode)
			return;

		vector from = group.GetOrigin();
		IEntity leader = group.GetLeaderEntity();
		if (leader)
			from = leader.GetOrigin();

		array<SCR_ChimeraCharacter> players = {};
		CTR_Exfil.CollectOutsideBase(gameMode.GetMainBasePos(), players);
		SCR_ChimeraCharacter target;
		float distance = float.MAX;
		foreach (SCR_ChimeraCharacter player : players)
		{
			if (vector.DistanceSqXZ(player.GetOrigin(), from) < distance)
			{
				target = player;
				distance = vector.DistanceSqXZ(player.GetOrigin(), from);
			}
		}

		if (!target)
			return;

		array<AIWaypoint> waypoints = {};
		group.GetWaypoints(waypoints);
		foreach (AIWaypoint waypoint : waypoints)
		{
			group.RemoveWaypoint(waypoint);
			if (ao)
				ao.CTR_ForgetEntity(waypoint);

			SCR_EntityHelper.DeleteEntityAndChildren(waypoint);
		}

		KSC_AITasks.SearchAndDestroy(group, target.GetOrigin(), 30);
		waypoints.Clear();
		group.GetWaypoints(waypoints);
		if (ao && !waypoints.IsEmpty())
			ao.AddEntity(waypoints[waypoints.Count() - 1]);
	}

	//------------------------------------------------------------------------------------------------
	//! AI of the pursuit still alive. For tests.
	int CountAlive()
	{
		int count;
		foreach (AIGroup group : m_aGroups)
		{
			if (group)
				count += group.GetAgentsCount();
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Where the last wave came from. For tests.
	vector GetLastSpawn()
	{
		return m_vNextSpawn;
	}

	//------------------------------------------------------------------------------------------------
	//! Groups of the pursuit (weak). For tests.
	array<AIGroup> GetGroups()
	{
		return m_aGroups;
	}
}

//------------------------------------------------------------------------------------------------
modded class COE_AO
{
	//------------------------------------------------------------------------------------------------
	//! Removes an entity the AO would delete when it ends, because it was deleted already.
	void CTR_ForgetEntity(IEntity entity)
	{
		m_aEntities.RemoveItem(entity);
	}
}
