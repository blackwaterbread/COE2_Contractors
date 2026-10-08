//! Moves vehicles with players inside to the main base, crew included (server).
//! Players on foot are moved by COE2's own fast travel when the AO ends.
class CTR_ReturnTrip
{
	//! Vehicles and players closer than this to the base stay where they are.
	protected static const float AT_BASE_DISTANCE = 150;
	protected static const float PARKING_SEARCH_RADIUS = 80;
	protected static const float PARKING_CLEARANCE = 7;
	protected static const float PARKING_HEIGHT = 4;

	//------------------------------------------------------------------------------------------------
	static bool IsAtBase(vector pos, vector basePos)
	{
		return vector.DistanceXZ(pos, basePos) < AT_BASE_DISTANCE;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the vehicles that are not at the base yet. \return Number of vehicles moved.
	static int MoveVehicles(notnull array<IEntity> vehicles, vector basePos)
	{
		int moved;
		foreach (IEntity vehicle : vehicles)
		{
			if (!vehicle || IsAtBase(vehicle.GetOrigin(), basePos))
				continue;

			if (MoveVehicle(vehicle, basePos))
				moved++;
		}

		return moved;
	}

	//------------------------------------------------------------------------------------------------
	//! Root vehicles that carry at least one living player.
	static void CollectOccupiedVehicles(notnull array<IEntity> outVehicles)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);

		foreach (int playerId : playerIds)
		{
			IEntity vehicle = GetVehicle(SCR_ChimeraCharacter.Cast(playerManager.GetPlayerControlledEntity(playerId)));
			if (vehicle && !outVehicles.Contains(vehicle))
				outVehicles.Insert(vehicle);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return The vehicle a living character sits in, or null.
	static IEntity GetVehicle(SCR_ChimeraCharacter character)
	{
		if (!character || !character.IsInVehicle())
			return null;

		if (character.GetCharacterController().GetLifeState() == ECharacterLifeState.DEAD)
			return null;

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(character.GetCompartmentAccessComponent());
		if (!access)
			return null;

		return access.GetVehicle();
	}

	//------------------------------------------------------------------------------------------------
	static bool MoveVehicle(notnull IEntity vehicle, vector basePos)
	{
		SCR_EditableVehicleComponent editable = SCR_EditableVehicleComponent.Cast(vehicle.FindComponent(SCR_EditableVehicleComponent));
		if (!editable)
		{
			Print(string.Format("[CTR] Cannot move %1 to the base: not an editable vehicle", vehicle), LogLevel.WARNING);
			return false;
		}

		vector parking;
		if (!SCR_WorldTools.FindEmptyTerrainPosition(parking, basePos, PARKING_SEARCH_RADIUS, PARKING_CLEARANCE, PARKING_HEIGHT))
		{
			Print(string.Format("[CTR] No free spot near the base for %1", vehicle), LogLevel.WARNING);
			return false;
		}

		vector transform[4];
		KSC_GameTools.GetTransformFromPosAndRot(transform, parking, vehicle.GetYawPitchRoll()[0]);

		Physics physics = vehicle.GetPhysics();
		if (physics)
		{
			physics.SetVelocity(vector.Zero);
			physics.SetAngularVelocity(vector.Zero);
		}

		return editable.SetTransform(transform);
	}
}
