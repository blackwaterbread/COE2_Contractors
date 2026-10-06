//! Prices of saved loadouts (Marx_Stash): the catalogs of the base's arsenal shops, in the pay currency. Server.
class CTR_LoadoutPrices
{
	//------------------------------------------------------------------------------------------------
	//! Sets the Marx price list from the shops in every COE2 main base of the world. \return Shops added.
	static int Setup()
	{
		MRX_ShopPriceList prices = new MRX_ShopPriceList(CTR_Settings.Get().m_sCurrency);
		array<IEntity> bases = {};
		KSC_WorldTools.GetEntitiesByType(bases, COE_MainBaseEntity);
		array<string> added = {};
		foreach (IEntity base : bases)
		{
			AddShopsUnder(base, prices, added);
		}

		MRX_Marx.SetPriceList(prices);
		return added.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Every main base has the same shops: each shop ID counts once.
	protected static void AddShopsUnder(notnull IEntity parent, notnull MRX_ShopPriceList prices, notnull array<string> added)
	{
		IEntity child = parent.GetChildren();
		while (child)
		{
			MRX_ShopComponent shop = MRX_ShopComponent.Cast(child.FindComponent(MRX_ShopComponent));
			if (shop && shop.GetDefinition() && !added.Contains(shop.GetShopId()))
			{
				prices.AddShop(shop.GetDefinition());
				added.Insert(shop.GetShopId());
			}

			AddShopsUnder(child, prices, added);
			child = child.GetSibling();
		}
	}
}
