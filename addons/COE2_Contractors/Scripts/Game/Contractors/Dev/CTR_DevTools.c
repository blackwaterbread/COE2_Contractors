#ifdef ENABLE_DIAG
//! Shortcuts for trying Contractors by hand and in the tests (diag builds only: Workbench, PeerTool, diag server).
class CTR_DevTools
{
	//! AOs closer than this to the main base are not picked.
	static const float MIN_AO_BASE_DISTANCE = 800;

	//------------------------------------------------------------------------------------------------
	//! Server. Generates an AO the way the commander does, at a random location away from the base.
	//! \param taskCount Different random task builders; 0 or less uses only "Clear Area".
	//! \return Error text, empty on success.
	static string GenerateAO(int taskCount, out string locationName)
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode)
			return "not a COE2 world";

		if (gameMode.COE_GetState() != COE_EGameModeState.INTERMISSION)
			return "an AO is already running (#ctr cancel ends it)";

		COE_FactionManager factionManager = COE_FactionManager.Cast(GetGame().GetFactionManager());
		if (!factionManager || !factionManager.GetPlayerFaction() || !factionManager.GetEnemyFaction())
			return "player or enemy faction not set (scenario attributes)";

		array<KSC_Location> candidates = {};
		foreach (KSC_Location location : gameMode.GetAvailableLocations())
		{
			if (location && vector.DistanceXZ(location.m_vCenter, gameMode.GetMainBasePos()) > MIN_AO_BASE_DISTANCE)
				candidates.Insert(location);
		}

		if (candidates.IsEmpty())
			return "no AO location far enough from the base";

		KSC_Location chosen = candidates.GetRandomElement();
		array<COE_BaseTaskBuilder> builders = {};
		PickBuilders(gameMode, taskCount, builders);

		COE_AOParams params = new COE_AOParams();
		params.SetLocation(chosen);
		params.SetTaskBuilders(builders);
		array<ref COE_AOParams> nextParams = {params};
		gameMode.SetNextAOParams(nextParams);
		gameMode.ExecuteCommanderRequest(COE_ECommanderRequest.GENERATE_AO);

		locationName = WidgetManager.Translate(chosen.m_sName);
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	protected static void PickBuilders(notnull COE_GameMode gameMode, int count, notnull array<COE_BaseTaskBuilder> outBuilders)
	{
		array<COE_BaseTaskBuilder> available = {};
		foreach (COE_BaseTaskBuilder builder : gameMode.GetAvailableTaskBuilders())
		{
			if (count <= 0 && builder.ClassName() != "COE_ClearAreaTaskBuilder")
				continue;

			available.Insert(builder);
		}

		if (count <= 0)
		{
			outBuilders.Copy(available);
			return;
		}

		while (outBuilders.Count() < count && !available.IsEmpty())
		{
			int index = available.GetRandomIndex();
			outBuilders.Insert(available[index]);
			available.Remove(index);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Sets every task of the running AOs to the state; the AO then finishes as if the players did it.
	//! \return Number of tasks changed.
	static int FinishTasks(SCR_ETaskState state)
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode)
			return 0;

		int count;
		foreach (COE_AO ao : gameMode.GetCurrentAOs())
		{
			if (!ao)
				continue;

			array<KSC_BaseTask> tasks = {};
			ao.CTR_GetTasks(tasks);
			foreach (KSC_BaseTask task : tasks)
			{
				SCR_ETaskState current = task.GetTaskState();
				if (current == SCR_ETaskState.COMPLETED || current == SCR_ETaskState.FAILED)
					continue;

				task.SetTaskState(state);
				count++;
			}
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! \return A spot inside the first running AO, at its edge; false when no AO runs.
	static bool GetAOEntryPos(out vector pos)
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode)
			return false;

		foreach (COE_AO ao : gameMode.GetCurrentAOs())
		{
			if (!ao)
				continue;

			pos = ao.GetOrigin() + Vector(gameMode.GetAORadius() - 40, 0, 0);
			pos[1] = SCR_TerrainHelper.GetTerrainY(pos);
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return A free spot next to the base shop, or the base itself; false without a main base.
	static bool GetShopPos(out vector pos)
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode || !gameMode.GetMainBase())
			return false;

		pos = gameMode.GetMainBasePos();
		IEntity shop = FindNear(pos, 40, MRX_ShopComponent);
		if (shop)
			pos = shop.GetOrigin();

		vector free;
		if (SCR_WorldTools.FindEmptyTerrainPosition(free, pos, 6))
			pos = free;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! First entity with the component within the radius, or null.
	static IEntity FindNear(vector center, float radius, typename componentType)
	{
		CTR_DevQuery query = new CTR_DevQuery(componentType);
		GetGame().GetWorld().QueryEntitiesBySphere(center, radius, query.OnEntity);
		return query.GetFound();
	}

	//------------------------------------------------------------------------------------------------
	//! Server. Moves a player, or the vehicle the player sits in. \return Error text, empty on success.
	static string Teleport(int playerId, vector pos)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId));
		if (!character)
			return "you have no character (deploy first)";

		if (character.IsInVehicle())
		{
			SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(character.GetCompartmentAccessComponent());
			if (access && access.GetVehicle() && CTR_ReturnTrip.MoveVehicle(access.GetVehicle(), pos))
				return string.Empty;

			return "could not move your vehicle";
		}

		SCR_EditableCharacterComponent editable = SCR_EditableCharacterComponent.Cast(character.FindComponent(SCR_EditableCharacterComponent));
		if (!editable)
			return "your character cannot be moved";

		vector transform[4];
		KSC_GameTools.GetTransformFromPosAndRot(transform, pos, character.GetYawPitchRoll()[0]);
		editable.SetTransform(transform);
		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
class CTR_DevQuery : Managed
{
	protected typename m_ComponentType;
	protected IEntity m_Found;

	//------------------------------------------------------------------------------------------------
	void CTR_DevQuery(typename componentType)
	{
		m_ComponentType = componentType;
	}

	//------------------------------------------------------------------------------------------------
	bool OnEntity(IEntity entity)
	{
		if (!entity.FindComponent(m_ComponentType))
			return true;

		m_Found = entity;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	IEntity GetFound()
	{
		return m_Found;
	}
}
#endif
