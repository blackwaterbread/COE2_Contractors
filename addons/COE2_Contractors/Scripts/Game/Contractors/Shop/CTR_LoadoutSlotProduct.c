//! Shop product: unlocks one more loadout slot (Marx_Stash MRX_LoadoutSlots), up to the slots the loadout window shows
//! (MRX_Settings.m_iMaxLoadoutSlots). Server.
[BaseContainerProps()]
class CTR_LoadoutSlotProduct : MRX_ShopProduct
{
	//------------------------------------------------------------------------------------------------
	override void Check(int playerId, string ownerId, notnull MRX_ShopItem item, notnull MRX_ShopProductCallback callback)
	{
		ListStash(ownerId, new CTR_LoadoutSlotCheck(callback));
	}

	//------------------------------------------------------------------------------------------------
	override void Deliver(int playerId, string ownerId, notnull MRX_ShopItem item, notnull MRX_ShopProductCallback callback)
	{
		MRX_TxContext context = MRX_TxContext.Create(CTR_StashPageProduct.LEDGER_SOURCE, "loadout_slot", "loadout_slot:" + MRX_Marx.NewId());
		MRX_LoadoutSlots.AddSlots(ownerId, 1, context, new CTR_LoadoutSlotDelivery(callback));
	}

	//------------------------------------------------------------------------------------------------
	override void GetState(int playerId, string ownerId, notnull MRX_ShopItem item, notnull MRX_ShopProductStateCallback callback)
	{
		ListStash(ownerId, new CTR_LoadoutSlotState(callback));
	}

	//------------------------------------------------------------------------------------------------
	protected static void ListStash(string ownerId, notnull MRX_StashCallback callback)
	{
		MRX_StashService stash = MRX_Marx.GetStash();
		if (!stash)
		{
			callback.OnResult(MRX_EStashStatus.STORAGE_ERROR, null);
			return;
		}

		stash.List(ownerId, callback);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while the owner can unlock another slot.
	static bool CanUnlock(int slots)
	{
		return slots > 0 && slots < MRX_LoadoutSlots.GetMaxSlots();
	}

	//------------------------------------------------------------------------------------------------
	//! Text of the shop window, e.g. "Loadouts: 3 / 10", translated on each client.
	static string FormatState(int slots, int maxSlots)
	{
		return MRX_TextFormat.PackLocalized("#CTR-Shop_LoadoutSlots", {slots.ToString(), maxSlots.ToString()});
	}
}

//------------------------------------------------------------------------------------------------
//! Internal.
class CTR_LoadoutSlotCheck : MRX_StashCallback
{
	protected ref MRX_ShopProductCallback m_Callback;

	//------------------------------------------------------------------------------------------------
	void CTR_LoadoutSlotCheck(MRX_ShopProductCallback callback)
	{
		m_Callback = callback;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_EStashStatus status, MRX_StashRecord record)
	{
		if (status != MRX_EStashStatus.OK || !record)
		{
			m_Callback.OnResult(MRX_EShopStatus.REJECTED);
			return;
		}

		if (CTR_LoadoutSlotProduct.CanUnlock(MRX_LoadoutSlots.GetSlots(record)))
			m_Callback.OnResult(MRX_EShopStatus.OK);
		else
			m_Callback.OnResult(MRX_EShopStatus.LIMIT_REACHED);
	}
}

//------------------------------------------------------------------------------------------------
//! Internal.
class CTR_LoadoutSlotState : MRX_StashCallback
{
	protected ref MRX_ShopProductStateCallback m_Callback;

	//------------------------------------------------------------------------------------------------
	void CTR_LoadoutSlotState(MRX_ShopProductStateCallback callback)
	{
		m_Callback = callback;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_EStashStatus status, MRX_StashRecord record)
	{
		if (status != MRX_EStashStatus.OK || !record)
		{
			m_Callback.OnResult(MRX_ShopProductState.Create(false, string.Empty));
			return;
		}

		int slots = MRX_LoadoutSlots.GetSlots(record);
		string text = CTR_LoadoutSlotProduct.FormatState(slots, MRX_LoadoutSlots.GetMaxSlots());
		m_Callback.OnResult(MRX_ShopProductState.Create(CTR_LoadoutSlotProduct.CanUnlock(slots), text));
	}
}

//------------------------------------------------------------------------------------------------
//! Internal.
class CTR_LoadoutSlotDelivery : MRX_LoadoutSlotsCallback
{
	protected ref MRX_ShopProductCallback m_Callback;

	//------------------------------------------------------------------------------------------------
	void CTR_LoadoutSlotDelivery(MRX_ShopProductCallback callback)
	{
		m_Callback = callback;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_EStashStatus status, bool limitReached, int slots)
	{
		if (limitReached)
			m_Callback.OnResult(MRX_EShopStatus.LIMIT_REACHED);
		else if (status == MRX_EStashStatus.OK)
			m_Callback.OnResult(MRX_EShopStatus.OK);
		else
			m_Callback.OnResult(MRX_EShopStatus.DELIVERY_FAILED);
	}
}
