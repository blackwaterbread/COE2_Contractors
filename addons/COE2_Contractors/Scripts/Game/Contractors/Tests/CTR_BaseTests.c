#ifdef WORKBENCH
class CTR_BaseTests
{
	//------------------------------------------------------------------------------------------------
	static void Register(notnull CTR_TestRunner runner)
	{
		runner.Add(new CTR_Test_ShopPricing());
		runner.Add(new CTR_Test_BaseSetup());
	}

	//------------------------------------------------------------------------------------------------
	static void FindComponents(notnull IEntity parent, typename componentType, notnull array<Managed> outComponents)
	{
		IEntity child = parent.GetChildren();
		while (child)
		{
			Managed component = child.FindComponent(componentType);
			if (component)
				outComponents.Insert(component);

			FindComponents(child, componentType, outComponents);
			child = child.GetSibling();
		}
	}
}

//------------------------------------------------------------------------------------------------
class CTR_Test_ShopPricing : CTR_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		CheckInt(CTR_ShopPricing.GetPrice(SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.WEAPON, 8), 520, "M16A2 (supply 8)");
		CheckInt(CTR_ShopPricing.GetPrice(SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.WEAPON_VARIANTS, 12), 680, "carbine variant (supply 12)");
		CheckInt(CTR_ShopPricing.GetPrice(SCR_EArsenalItemType.PISTOL, SCR_EArsenalItemMode.WEAPON, 5), 250, "pistol");
		CheckInt(CTR_ShopPricing.GetPrice(SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.AMMUNITION, 2), 15, "rifle magazine");
		CheckInt(CTR_ShopPricing.GetPrice(SCR_EArsenalItemType.HEAL, SCR_EArsenalItemMode.CONSUMABLE, 0), 10, "field dressing");
		CheckInt(CTR_ShopPricing.GetPrice(SCR_EArsenalItemType.HELICOPTER, SCR_EArsenalItemMode.AMMUNITION, 35), 0, "helicopter rockets are not sold");
		CheckInt(CTR_ShopPricing.GetPrice(SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.PYLON, 250), 0, "pylons are not sold");
		CheckString(CTR_ShopPricing.GetCategory(SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.DEFAULT), CTR_ShopPricing.CATEGORY_ATTACHMENTS, "entry without arsenal mode (underbarrel launcher)");
		CheckString(CTR_ShopPricing.GetCategory(SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.WEAPON), CTR_ShopPricing.CATEGORY_EQUIPMENT, "hand flare");
		CheckString(CTR_ShopPricing.GetCategory(SCR_EArsenalItemType.VEST_AND_WAIST, SCR_EArsenalItemMode.DEFAULT), CTR_ShopPricing.CATEGORY_GEAR, "vest");
		Check(CTR_ShopPricing.GetCategoryOrder(CTR_ShopPricing.CATEGORY_WEAPONS) < CTR_ShopPricing.GetCategoryOrder(CTR_ShopPricing.CATEGORY_EQUIPMENT), "category order");

		// One operation (about 650) buys a basic kit: rifle, six magazines, four dressings, a tourniquet.
		int kit = 520 + 6 * 15 + 4 * 10 + 10;
		Check(kit <= 700 && kit >= 600, "basic kit near one operation's pay: " + kit);
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! The main base of the running world: arsenals off, the Contractors shop and a stash point in it.
class CTR_Test_BaseSetup : CTR_TestCase
{
	protected static const float NEARBY_RADIUS = 40;
	protected ref array<IEntity> m_aNearby = {};

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode || !gameMode.GetMainBase())
		{
			Skip("no COE2 main base");
			return;
		}

		IEntity base = gameMode.GetMainBase();
		array<Managed> arsenals = {};
		CTR_BaseTests.FindComponents(base, SCR_ArsenalComponent, arsenals);
		Check(!arsenals.IsEmpty(), "base has its arsenal boxes");
		foreach (Managed managed : arsenals)
		{
			SCR_ArsenalComponent arsenal = SCR_ArsenalComponent.Cast(managed);
			Check(!arsenal.IsArsenalEnabled(), "arsenal disabled");
			CheckInt(arsenal.GetArsenalSaveType(), SCR_EArsenalSaveType.SAVING_DISABLED, "arsenal loadout saving");
			CheckInt(arsenal.GetSupportedArsenalItemModes(), 0, "arsenal offers no items");
		}

		// The shop and stash prefabs have no Hierarchy component, so they are not children of the base but stand next to it.
		GetGame().GetWorld().QueryEntitiesBySphere(base.GetOrigin(), NEARBY_RADIUS, CollectNearby);
		array<Managed> shops = {};
		array<Managed> stashPoints = {};
		foreach (IEntity entity : m_aNearby)
		{
			Managed shopComponent = entity.FindComponent(MRX_ShopComponent);
			if (shopComponent)
				shops.Insert(shopComponent);

			Managed stashComponent = entity.FindComponent(MRX_StashPointComponent);
			if (stashComponent)
				stashPoints.Insert(stashComponent);
		}

		CheckInt(shops.Count(), 1, "one shop at the base");
		if (shops.Count() == 1)
		{
			MRX_ShopComponent shop = MRX_ShopComponent.Cast(shops[0]);
			CheckString(shop.GetShopId(), "contractors", "shop ID");
			MRX_ShopDefinition definition = shop.GetDefinition();
			Check(definition != null, "catalog loads");
			if (definition)
			{
				Check(definition.m_Catalog.m_aItems.Count() > 300, "all-faction catalog, items: " + definition.m_Catalog.m_aItems.Count());
				Check(definition.m_bAllowSell, "buys items back");
				CheckInt(definition.m_iSellPercent, MRX_ShopDefinition.DEFAULT_SELL_PERCENT, "buy-back percentage");
				MRX_ShopItem rifle = definition.m_Catalog.FindItem("rifle_m16a2");
				Check(rifle && rifle.m_iPrice == 520, "M16A2 for sale at 520");
				Check(definition.m_Catalog.FindItem("rifle_ak74") != null, "USSR rifle for sale");
			}
		}

		CheckInt(stashPoints.Count(), 1, "one stash point at the base");
		Finish();
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectNearby(IEntity entity)
	{
		m_aNearby.Insert(entity);
		return true;
	}
}
#endif
