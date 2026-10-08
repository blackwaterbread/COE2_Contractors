//! The gear a player spawns with, for the preview of the deploy menu: the gear they last wore if they left alive
//! (CTR_LastGear), else the starter kit. The vanilla preview shows the role's gear, which the spawn replaces. The server
//! sends each player the visible part of their gear; the client puts it on the previewed character, slot by slot like
//! the vanilla preview of arsenal loadouts.
class CTR_SpawnGear
{
	//------------------------------------------------------------------------------------------------
	//! Server: the visible part of a loadout snapshot (worn items, weapons, their attachments), small enough for an RPC.
	//! What is carried inside (magazines, medical items in pouches) is left out.
	static MRX_ItemSnapshot GetVisible(notnull MRX_ItemSnapshot loadout)
	{
		MRX_ItemSnapshot visible = MRX_ItemSnapshot.Create(loadout.m_sPrefab);
		foreach (MRX_ItemSnapshot item : loadout.m_aChildren)
		{
			visible.m_aChildren.Insert(CopyWithAttachments(item));
		}

		return visible;
	}

	//------------------------------------------------------------------------------------------------
	protected static MRX_ItemSnapshot CopyWithAttachments(notnull MRX_ItemSnapshot item)
	{
		MRX_ItemSnapshot copy = MRX_ItemSnapshot.Create(item.m_sPrefab);
		copy.m_iFormat = 0;
		copy.m_sStorage = item.m_sStorage;
		copy.m_iSlot = item.m_iSlot;
		foreach (MRX_ItemSnapshot child : item.m_aChildren)
		{
			if (IsAttachmentStorage(child.m_sStorage))
				copy.m_aChildren.Insert(CopyWithAttachments(child));
		}

		return copy;
	}

	//------------------------------------------------------------------------------------------------
	//! Storages whose items show on their item: weapon attachments and clothing parts (e.g. night vision on a helmet).
	//! \param storageId As MRX_EntitySnapshots.GetStorageId writes it: component class, ':', GUID.
	static bool IsAttachmentStorage(string storageId)
	{
		return storageId.Contains("WeaponAttachmentsStorageComponent") || storageId.StartsWith("ClothNodeStorageComponent");
	}

	//------------------------------------------------------------------------------------------------
	//! Client: puts the gear on a previewed character (a local entity of the item preview world) instead of what it
	//! came with, as the spawn does. \param gear Visible gear (GetVisible); null or empty for the starter kit.
	static void Dress(notnull IEntity character, MRX_ItemSnapshot gear)
	{
		set<BaseInventoryStorageComponent> storages = MRX_EntitySnapshots.GetLoadoutStorages(character);
		RemoveItems(storages);
		if (gear && !gear.m_aChildren.IsEmpty())
			DressSnapshot(character, gear, storages);
		else
			DressKit(character, CTR_Settings.Get().GetStarterKit(), storages);

		SelectWeapon(character);
	}

	//------------------------------------------------------------------------------------------------
	protected static void RemoveItems(notnull set<BaseInventoryStorageComponent> storages)
	{
		array<IEntity> items = {};
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			array<InventoryItemComponent> ownedItems = {};
			storage.GetOwnedItems(ownedItems, false);
			foreach (InventoryItemComponent item : ownedItems)
			{
				if (item.GetOwner())
					items.Insert(item.GetOwner());
			}
		}

		foreach (IEntity item : items)
		{
			if (item && !item.IsDeleted())
				delete item;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Each item into the slot it was saved in; a fitting worn or weapon slot when the character has no such slot.
	protected static void DressSnapshot(notnull IEntity character, notnull MRX_ItemSnapshot gear, notnull set<BaseInventoryStorageComponent> storages)
	{
		foreach (MRX_ItemSnapshot item : gear.m_aChildren)
		{
			IEntity entity = SpawnLocal(item.m_sPrefab, character);
			if (!entity)
				continue;

			InventoryStorageSlot slot = FindSavedSlot(character, storages, item.m_sStorage, item.m_iSlot);
			if (!slot)
				slot = FindFittingSlot(storages, entity);

			if (!slot)
			{
				delete entity;
				continue;
			}

			slot.AttachEntity(entity);
			DressAttachments(entity, item);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Worn items and weapons of the kit into fitting slots; what goes into pouches does not show.
	protected static void DressKit(notnull IEntity character, notnull array<ResourceName> kit, notnull set<BaseInventoryStorageComponent> storages)
	{
		foreach (ResourceName prefab : kit)
		{
			IEntity entity = SpawnLocal(prefab, character);
			if (!entity)
				continue;

			InventoryStorageSlot slot = FindFittingSlot(storages, entity);
			if (slot)
				slot.AttachEntity(entity);
			else
				delete entity;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Makes the item's attachments those of the snapshot: same ones kept, others replaced, missing ones removed.
	protected static void DressAttachments(notnull IEntity item, notnull MRX_ItemSnapshot snapshot)
	{
		set<BaseInventoryStorageComponent> storages = new set<BaseInventoryStorageComponent>();
		MRX_EntitySnapshots.FindStorages(item, storages);
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			string storageId = MRX_EntitySnapshots.GetStorageId(item, storage);
			if (!IsAttachmentStorage(storageId))
				continue;

			for (int i = 0, count = storage.GetSlotsCount(); i < count; i++)
			{
				InventoryStorageSlot slot = storage.GetSlot(i);
				if (!slot)
					continue;

				MRX_ItemSnapshot wanted = FindChild(snapshot, storageId, i);
				IEntity current = slot.GetAttachedEntity();
				if (current && wanted && SCR_ResourceNameUtils.GetPrefabName(current) == wanted.m_sPrefab)
				{
					DressAttachments(current, wanted);
					continue;
				}

				if (current)
					delete current;

				if (!wanted)
					continue;

				IEntity attachment = SpawnLocal(wanted.m_sPrefab, item);
				if (!attachment)
					continue;

				slot.AttachEntity(attachment);
				DressAttachments(attachment, wanted);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static MRX_ItemSnapshot FindChild(notnull MRX_ItemSnapshot snapshot, string storageId, int slot)
	{
		foreach (MRX_ItemSnapshot child : snapshot.m_aChildren)
		{
			if (child.m_sStorage == storageId && child.m_iSlot == slot)
				return child;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The free slot an item was saved in: same storage (another role's character may have it under another GUID, so
	//! the same class does too) and slot.
	protected static InventoryStorageSlot FindSavedSlot(notnull IEntity character, notnull set<BaseInventoryStorageComponent> storages, string storageId, int slotId)
	{
		int separator = storageId.IndexOf(":");
		if (separator < 0 || slotId < 0)
			return null;

		string storageClass = storageId.Substring(0, separator);
		BaseInventoryStorageComponent sameClass;
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (MRX_EntitySnapshots.GetStorageId(character, storage) == storageId)
				return GetFreeSlot(storage, slotId);

			if (!sameClass && storage.ClassName() == storageClass)
				sameClass = storage;
		}

		if (!sameClass)
			return null;

		return GetFreeSlot(sameClass, slotId);
	}

	//------------------------------------------------------------------------------------------------
	protected static InventoryStorageSlot GetFreeSlot(notnull BaseInventoryStorageComponent storage, int slotId)
	{
		if (slotId >= storage.GetSlotsCount())
			return null;

		InventoryStorageSlot slot = storage.GetSlot(slotId);
		if (!slot || slot.GetAttachedEntity())
			return null;

		return slot;
	}

	//------------------------------------------------------------------------------------------------
	//! A free worn or weapon slot that takes the item.
	protected static InventoryStorageSlot FindFittingSlot(notnull set<BaseInventoryStorageComponent> storages, notnull IEntity item)
	{
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (!EquipedLoadoutStorageComponent.Cast(storage) && !EquipedWeaponStorageComponent.Cast(storage))
				continue;

			InventoryStorageSlot slot = storage.FindSuitableSlotForItem(item);
			if (slot && !slot.GetAttachedEntity())
				return slot;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity SpawnLocal(ResourceName prefab, notnull IEntity nextTo)
	{
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return null;

		return GetGame().SpawnEntityPrefabLocal(resource, nextTo.GetWorld());
	}

	//------------------------------------------------------------------------------------------------
	//! The first weapon in hand, as after the spawn (CTR_StarterKit.EquipWeapon).
	protected static void SelectWeapon(notnull IEntity character)
	{
		BaseWeaponManagerComponent weaponManager = BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
		if (!weaponManager)
			return;

		array<WeaponSlotComponent> slots = {};
		weaponManager.GetWeaponsSlots(slots);
		WeaponSlotComponent best;
		foreach (WeaponSlotComponent slot : slots)
		{
			if (slot.GetWeaponEntity() && (!best || slot.GetWeaponSlotIndex() < best.GetWeaponSlotIndex()))
				best = slot;
		}

		if (best)
			weaponManager.SelectWeapon(best);
	}
}
