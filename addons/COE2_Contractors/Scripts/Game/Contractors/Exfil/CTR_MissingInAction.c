//! Missing in action (server): the exfil countdown ran out. Every living player outside the base is held behind the
//! result screen (it cannot be closed, see CTR_ResultDialog) while the enemies around them stand down, then dies where
//! they are. Players who left the game outside the base during the exfil or meanwhile lose their last gear too, so
//! leaving does not save it.
class CTR_MissingInAction : Managed
{
	//! Meters around a missing player in which enemy AI stands down.
	static const float ENEMY_RADIUS = 1000;

	//! Characters of the missing players (weak) and their players, same index.
	protected ref array<IEntity> m_aCharacters = {};
	protected ref array<int> m_aPlayerIds = {};
	//! Owners who left the game outside the base, and the bodies they left (weak, same index).
	protected ref array<string> m_aLeftOwners = {};
	protected ref array<IEntity> m_aLeftBodies = {};
	//! AI that stood down (weak), to wake what is left of it when the AO has ended.
	protected ref array<AIAgent> m_aStoodDown = {};

	//------------------------------------------------------------------------------------------------
	//! The players who are alive outside the base now: they are missing. Unconscious players count.
	void CollectPlayers(vector basePos)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(playerManager.GetPlayerControlledEntity(playerId));
			if (!CTR_Exfil.IsAliveOutsideBase(character, basePos))
				continue;

			m_aCharacters.Insert(character);
			m_aPlayerIds.Insert(playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! An owner who left the game outside the base, with the body they left.
	void AddLeft(string ownerId, IEntity body)
	{
		if (ownerId.IsEmpty() || m_aLeftOwners.Contains(ownerId))
			return;

		m_aLeftOwners.Insert(ownerId);
		m_aLeftBodies.Insert(body);
	}

	//------------------------------------------------------------------------------------------------
	bool IsMissing(int playerId)
	{
		return m_aPlayerIds.Contains(playerId);
	}

	//------------------------------------------------------------------------------------------------
	int CountMissing()
	{
		return m_aPlayerIds.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! A missing player leaves before they die: they lose their gear like those who left during the exfil. Call before
	//! the game mode handles the disconnect.
	void OnPlayerLeaving(int playerId)
	{
		int index = m_aPlayerIds.Find(playerId);
		if (index >= 0)
			AddLeft(MRX_Marx.GetOwnerId(playerId), m_aCharacters[index]);
	}

	//------------------------------------------------------------------------------------------------
	//! Enemy AI near the missing players stands down: nobody fights players who are held behind the result screen.
	//! \return AI that stood down.
	int StandDownEnemies()
	{
		AIWorld aiWorld = GetGame().GetAIWorld();
		if (!aiWorld)
			return 0;

		array<AIAgent> agents = {};
		aiWorld.GetAIAgents(agents);
		int count;
		foreach (AIAgent agent : agents)
		{
			AIGroup group = AIGroup.Cast(agent);
			if (!group)
			{
				count += StandDown(agent);
				continue;
			}

			int before = count;
			array<AIAgent> members = {};
			group.GetAgents(members);
			foreach (AIAgent member : members)
			{
				count += StandDown(member);
			}

			if (count > before && group.IsAIActivated())
			{
				group.DeactivateAI();
				m_aStoodDown.Insert(group);
			}
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! \return 1 when the agent stood down.
	protected int StandDown(AIAgent agent)
	{
		if (!agent || !agent.IsAIActivated() || !IsThreat(SCR_ChimeraCharacter.Cast(agent.GetControlledEntity())))
			return 0;

		agent.DeactivateAI();
		m_aStoodDown.Insert(agent);
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	//! A living AI character of a faction hostile to a missing player near it.
	bool IsThreat(SCR_ChimeraCharacter character)
	{
		if (!character || !character.GetCharacterController() || character.GetCharacterController().GetLifeState() == ECharacterLifeState.DEAD)
			return false;

		Faction faction = character.GetFaction();
		if (!faction || GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(character) != 0)
			return false;

		foreach (IEntity entity : m_aCharacters)
		{
			SCR_ChimeraCharacter missing = SCR_ChimeraCharacter.Cast(entity);
			if (!missing || !IsHostile(missing.GetFaction(), faction))
				continue;

			if (vector.Distance(missing.GetOrigin(), character.GetOrigin()) <= ENEMY_RADIUS)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The COE2 enemy faction, and any faction hostile to the player's.
	static bool IsHostile(Faction playerFaction, Faction faction)
	{
		if (!faction)
			return false;

		COE_FactionManager factionManager = COE_FactionManager.Cast(GetGame().GetFactionManager());
		if (factionManager && faction == factionManager.GetEnemyFaction())
			return true;

		return playerFaction && playerFaction.IsFactionEnemy(faction);
	}

	//------------------------------------------------------------------------------------------------
	//! The AO has ended (COE2 deleted its AI): what stood down elsewhere wakes up.
	void Release()
	{
		foreach (AIAgent agent : m_aStoodDown)
		{
			if (agent && !agent.IsAIActivated())
				agent.ActivateAI();
		}

		m_aStoodDown.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Kills the missing players wherever they are now, the bodies left by players who left outside the base, and those
	//! players if they came back; those players lose their last gear (the others lose it by dying).
	//! \param[out] outKilled Characters killed (to wait until they are dead).
	void Kill(vector basePos, CTR_LastGear lastGear, notnull array<IEntity> outKilled)
	{
		foreach (IEntity character : m_aCharacters)
		{
			KillCharacter(SCR_ChimeraCharacter.Cast(character), outKilled);
		}

		PlayerManager playerManager = GetGame().GetPlayerManager();
		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		foreach (int i, string ownerId : m_aLeftOwners)
		{
			if (lastGear)
				lastGear.Clear(ownerId);

			SCR_ChimeraCharacter body = SCR_ChimeraCharacter.Cast(m_aLeftBodies[i]);
			if (CTR_Exfil.IsAliveOutsideBase(body, basePos))
				KillCharacter(body, outKilled);

			foreach (int playerId : playerIds)
			{
				if (MRX_Marx.GetOwnerId(playerId) == ownerId)
					KillCharacter(SCR_ChimeraCharacter.Cast(playerManager.GetPlayerControlledEntity(playerId)), outKilled);
			}
		}

		Print(string.Format("[CTR] Missing in action: %1 characters killed, %2 players who left lose their gear", outKilled.Count(), m_aLeftOwners.Count()));
	}

	//------------------------------------------------------------------------------------------------
	protected static void KillCharacter(SCR_ChimeraCharacter character, notnull array<IEntity> outKilled)
	{
		if (!character || outKilled.Contains(character) || !character.GetCharacterController())
			return;

		if (character.GetCharacterController().GetLifeState() == ECharacterLifeState.DEAD)
			return;

		character.GetCharacterController().ForceDeath();
		outKilled.Insert(character);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: a missing player who drives stops their vehicle, since they cannot steer behind the result screen. Not
	//! a helicopter: it would fall. The driver's machine simulates the vehicle, as for the vanilla engine action.
	//! \return True when a vehicle was stopped.
	static bool StopDrivenVehicle(IEntity character)
	{
		Vehicle vehicle = Vehicle.Cast(CompartmentAccessComponent.GetVehicleIn(character));
		if (!vehicle || vehicle.GetPilot() != character)
			return false;

		VehicleControllerComponent controller = vehicle.GetVehicleController();
		if (!controller || HelicopterControllerComponent.Cast(controller))
			return false;

		controller.StopEngine(false);
		CarControllerComponent car = CarControllerComponent.Cast(controller);
		if (car)
		{
			// As the vanilla handbrake action does.
			car.SetPersistentHandBrake(true);
			VehicleWheeledSimulation simulation = car.GetSimulation();
			if (simulation)
				simulation.SetBreak(true, true);
		}

		return true;
	}
}
