//! Respawn gear (server): whatever role a player picks, the character spawns with the same minimal kit, a basic rifle
//! and no armor, enough for a player who went broke. The kit is issued (MRX_IssuedItems): shops buy it back for nothing
//! and loadouts do not count it, so respawning never makes money. Players buy everything else, or put on a saved loadout
//! at the stash.
class CTR_StarterKit
{
	static const ResourceName JACKET = "{E95480D14AEE3EF4}Prefabs/Characters/Uniforms/Crye_Shirt/Jacket_Crye_Combat_Shirt_Rolled_mcblack.et";
	static const ResourceName PANTS = "{7824F6BA0A92A8DC}Prefabs/Characters/Uniforms/Pants_FROG_Trousers_MC.et";
	static const ResourceName BOOTS = "{057E63449502EBCA}Prefabs/Characters/Footwear/Boots_Salomon/Footwear_Salomon.et";
	static const ResourceName GLOVES = "{1E991CD19C6F7659}Prefabs/Characters/Handwear/Gloves_MechanixMpact/Gloves_mpack_Brown.et";
	//! Comes loaded (a tracer magazine).
	static const ResourceName RIFLE = "{3E413771E1834D2F}Prefabs/Weapons/Rifles/M16/Rifle_M16A2.et";
	static const ResourceName MAGAZINE = "{2EBF60EF24B108FC}Prefabs/Weapons/Magazines/Magazine_556x45_STANAG_30rnd_M855_Ball.et";
	static const ResourceName BANDAGE = "{A81F501D3EF6F38E}Prefabs/Items/Medicine/FieldDressing_01/FieldDressing_US_01.et";
	static const ResourceName TOURNIQUET = "{D70216B1B2889129}Prefabs/Items/Medicine/Tourniquet_01/Tourniquet_US_01.et";
	static const ResourceName COMPASS = "{61D4F80E49BF9B12}Prefabs/Items/Equipment/Compass/Compass_SY183.et";
	static const ResourceName MAP = "{13772C903CB5E4F7}Prefabs/Items/Equipment/Maps/Map_Paper_01/PaperMap_01_folded.et";
	static const ResourceName RADIO = "{D69684663E89D6EA}Prefabs/Items/Equipment/Radios/Radio_ANPRC152A_OLD.et";

	//------------------------------------------------------------------------------------------------
	//! RHS ION base clothing without helmet and armor, a loaded M16A2 with three more magazines, two bandages and a
	//! tourniquet, compass, map and radio.
	static array<ResourceName> GetDefault()
	{
		return {JACKET, PANTS, BOOTS, GLOVES, RIFLE, MAGAZINE, MAGAZINE, MAGAZINE, BANDAGE, BANDAGE, TOURNIQUET, COMPASS, MAP, RADIO};
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces what the character wears and carries (its loadout storages, see MRX_EntitySnapshots) with the kit, in
	//! order: clothing to its slot, weapons to their slot, magazines and attachments into a weapon given before when
	//! they fit (e.g. a magazine into an empty rifle), the rest wherever it fits. Items that do not fit are left out.
	//! Marks everything the character carries afterwards as issued.
	//! \return Kit items given.
	static int Apply(notnull IEntity character, notnull array<ResourceName> kit)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera || !chimera.GetCharacterController())
			return 0;

		InventoryStorageManagerComponent manager = chimera.GetCharacterController().GetInventoryStorageManager();
		if (!manager)
			return 0;

		set<BaseInventoryStorageComponent> storages = MRX_EntitySnapshots.GetLoadoutStorages(character);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			array<InventoryItemComponent> items = {};
			storage.GetOwnedItems(items, false);
			foreach (InventoryItemComponent item : items)
			{
				// The role's current weapon cannot be removed through the inventory: delete it directly.
				IEntity owner = item.GetOwner();
				if (owner && !manager.TryDeleteItem(owner))
					RplComponent.DeleteRplEntity(owner, false);
			}
		}

		BaseInventoryStorageComponent clothing = BaseInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
		BaseInventoryStorageComponent weapons = BaseInventoryStorageComponent.Cast(character.FindComponent(EquipedWeaponStorageComponent));
		int given;
		foreach (ResourceName prefab : kit)
		{
			bool spawned;
			if (clothing && manager.CanInsertResourceInStorage(prefab, clothing))
				spawned = manager.TrySpawnPrefabToStorage(prefab, clothing);
			else if (weapons && manager.CanInsertResourceInStorage(prefab, weapons))
				spawned = manager.TrySpawnPrefabToStorage(prefab, weapons);
			else
				spawned = SpawnIntoWeapon(chimera, manager, prefab) || manager.TrySpawnPrefabToStorage(prefab, null, -1, EStoragePurpose.PURPOSE_DEPOSIT);

			if (spawned)
				given++;
			else
				Print(string.Format("[CTR] Starter kit: no room for %1", prefab), LogLevel.WARNING);
		}

		MRX_IssuedItems.MarkCarried(character);
		EquipWeapon(chimera);
		return given;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the prefab into a free slot of a weapon the character has, e.g. a magazine into an empty rifle.
	//! \return False when no weapon takes it.
	protected static bool SpawnIntoWeapon(notnull ChimeraCharacter character, notnull InventoryStorageManagerComponent manager, ResourceName prefab)
	{
		BaseWeaponManagerComponent weaponManager = BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
		if (!weaponManager)
			return false;

		array<WeaponSlotComponent> slots = {};
		weaponManager.GetWeaponsSlots(slots);
		foreach (WeaponSlotComponent slot : slots)
		{
			IEntity weapon = slot.GetWeaponEntity();
			if (!weapon)
				continue;

			set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
			MRX_EntitySnapshots.FindStorages(weapon, storages);
			foreach (BaseInventoryStorageComponent storage : storages)
			{
				if (manager.CanInsertResourceInStorage(prefab, storage) && manager.TrySpawnPrefabToStorage(prefab, storage))
					return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the first weapon (primary first) in hand, as the vanilla arsenal loadout does after putting gear on.
	static void EquipWeapon(notnull ChimeraCharacter character)
	{
		BaseWeaponManagerComponent weaponManager = BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
		if (!weaponManager)
			return;

		array<WeaponSlotComponent> slots = {};
		weaponManager.GetWeaponsSlots(slots);
		IEntity weapon;
		int bestIndex = int.MAX;
		foreach (WeaponSlotComponent slot : slots)
		{
			if (slot.GetWeaponEntity() && slot.GetWeaponSlotIndex() < bestIndex)
			{
				weapon = slot.GetWeaponEntity();
				bestIndex = slot.GetWeaponSlotIndex();
			}
		}

		if (weapon)
			character.GetCharacterController().TryEquipRightHandItem(weapon, EEquipItemType.EEquipTypeWeapon, true);
	}
}
