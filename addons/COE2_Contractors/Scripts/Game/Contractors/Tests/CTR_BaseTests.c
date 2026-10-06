#ifdef WORKBENCH
class CTR_BaseTests
{
	//------------------------------------------------------------------------------------------------
	static void Register(notnull CTR_TestRunner runner)
	{
		runner.Add(new CTR_Test_ShopPricing());
		runner.Add(new CTR_Test_BaseSetup());
		runner.Add(new CTR_Test_ShopWindowPages());
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
		// Arsenal data as the vanilla and RHS catalogs have it.
		CheckInt(CTR_ShopPricing.GetPrice("Prefabs/Weapons/Rifles/M16/Rifle_M16A2.et", SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.WEAPON, 8), 520, "M16A2 (supply 8)");
		CheckInt(CTR_ShopPricing.GetPrice("Prefabs/Weapons/Handguns/M9/Handgun_M9.et", SCR_EArsenalItemType.PISTOL, SCR_EArsenalItemMode.WEAPON, 5), 250, "pistol");
		CheckInt(CTR_ShopPricing.GetPrice("Prefabs/Items/Medicine/FieldDressing_01/FieldDressing_US_01.et", SCR_EArsenalItemType.HEAL, SCR_EArsenalItemMode.CONSUMABLE, 0), 10, "field dressing");

		// RHS rifles keep the default type and some the default mode.
		ResourceName rhsRifle = "Prefabs/Weapons/Rifles/M4A1/Rifle_M4A1_BLOCK_1.et";
		CheckString(CTR_ShopPricing.GetCategory(rhsRifle, SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.DEFAULT), CTR_ShopPricing.CATEGORY_RIFLES, "RHS rifle in default mode");
		CheckInt(CTR_ShopPricing.GetPrice(rhsRifle, SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.DEFAULT, 12), 680, "RHS rifle price");
		ResourceName rhsMagazine = "Prefabs/Weapons/Magazines/6l23_plastic/Magazine_545x39_plastic_AK_30rnd_Ball_camo.et";
		CheckString(CTR_ShopPricing.GetCategory(rhsMagazine, SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.DEFAULT), CTR_ShopPricing.CATEGORY_AMMUNITION, "RHS magazine in default mode");
		CheckInt(CTR_ShopPricing.GetPrice(rhsMagazine, SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.DEFAULT, 2), 15, "RHS magazine price");

		// RHS weapon variants with supply cost 1 cost at least a bare weapon of their kind.
		CheckInt(CTR_ShopPricing.GetPrice("Prefabs/Weapons/Rifles/SVD/Rifle_SVD_PSO1.et", SCR_EArsenalItemType.SNIPER_RIFLE, SCR_EArsenalItemMode.WEAPON_VARIANTS, 1), 1000, "scoped SVD variant (supply 1)");
		CheckInt(CTR_ShopPricing.GetPrice("Prefabs/Weapons/Rifles/AKS74UN/Rifle_AKS74UN_x.et", SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.WEAPON, 1), 520, "rifle (supply 1)");

		// Launcher rockets are listed with the launchers but priced as ammunition.
		ResourceName rocket = "Prefabs/Weapons/Ammo/Ammo_Rocket_PG7VM.et";
		CheckString(CTR_ShopPricing.GetCategory(rocket, SCR_EArsenalItemType.ROCKET_LAUNCHER, SCR_EArsenalItemMode.AMMUNITION), CTR_ShopPricing.CATEGORY_LAUNCHERS, "rocket category");
		CheckInt(CTR_ShopPricing.GetPrice(rocket, SCR_EArsenalItemType.ROCKET_LAUNCHER, SCR_EArsenalItemMode.AMMUNITION, 10), 55, "rocket price");
		CheckInt(CTR_ShopPricing.GetPrice("Prefabs/Weapons/Launchers/RPG7/Launcher_RPG7.et", SCR_EArsenalItemType.ROCKET_LAUNCHER, SCR_EArsenalItemMode.WEAPON, 55), 2400, "launcher price");

		// Mortar parts and shells.
		ResourceName mortarBarrel = "Prefabs/Items/Equipment/Mortars/M252/Part_M252_Barrel.et";
		CheckString(CTR_ShopPricing.GetCategory(mortarBarrel, SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.SUPPORT_STATION), CTR_ShopPricing.CATEGORY_HEAVY_WEAPONS, "mortar part");
		CheckInt(CTR_ShopPricing.GetPrice(mortarBarrel, SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.SUPPORT_STATION, 90), 465, "mortar part price");
		CheckInt(CTR_ShopPricing.GetPrice("Prefabs/Weapons/Ammo/Ammo_Shell_81mm_HE_M821.et", SCR_EArsenalItemType.MORTARS, SCR_EArsenalItemMode.AMMUNITION, 20), 105, "mortar shell price");

		// Attachments by folder.
		CheckString(CTR_ShopPricing.GetCategory("Prefabs/Weapons/Attachments/Optics/Optic_SPP/Optic_SPP.et", SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.ATTACHMENT), CTR_ShopPricing.CATEGORY_OPTICS, "night sight typed as equipment");
		CheckString(CTR_ShopPricing.GetCategory("Prefabs/Weapons/Attachments/Muzzle/Suppressor.et", SCR_EArsenalItemType.WEAPON_ATTACHMENT, SCR_EArsenalItemMode.ATTACHMENT), CTR_ShopPricing.CATEGORY_MUZZLE, "suppressor");
		CheckString(CTR_ShopPricing.GetCategory("Prefabs/Weapons/Attachments/Underbarrel/UGL_GP25.et", SCR_EArsenalItemType.WEAPON_ATTACHMENT, SCR_EArsenalItemMode.ATTACHMENT), CTR_ShopPricing.CATEGORY_ATTACHMENTS, "underbarrel launcher");
		CheckString(CTR_ShopPricing.GetCategory("Prefabs/Items/Equipment/Accessories/XmasLights/XmasLights.et", SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.ATTACHMENT), CTR_ShopPricing.CATEGORY_ACCESSORIES, "attachment that is not a weapon part");

		// Odd types: hand flares are weapons, the spectrum device is a launcher, helmet parts and armor plates equipment.
		CheckString(CTR_ShopPricing.GetCategory("Prefabs/Weapons/Flares/Flare_RSP30_red.et", SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.WEAPON), CTR_ShopPricing.CATEGORY_EXPLOSIVES, "hand flare");
		CheckString(CTR_ShopPricing.GetCategory("Prefabs/Weapons/Misc/SpectrumDevice/Device_SpectrumDevice_ru.et", SCR_EArsenalItemType.ROCKET_LAUNCHER, SCR_EArsenalItemMode.WEAPON), CTR_ShopPricing.CATEGORY_TOOLS, "spectrum device");
		CheckString(CTR_ShopPricing.GetCategory("Prefabs/Characters/HeadGear/Helmet_TOR/counterweight/TOR_counterweight_Black.et", SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.DEFAULT), CTR_ShopPricing.CATEGORY_HELMETS, "helmet counterweight");
		CheckString(CTR_ShopPricing.GetCategory("Prefabs/Characters/Vests/Vest_PCGen_III/Plates/DestructibleArmorPlate_PCGenIII_ESAPIrevJ_Back.et", SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.DEFAULT), CTR_ShopPricing.CATEGORY_VESTS, "armor plate");
		CheckString(CTR_ShopPricing.GetCategory("Prefabs/Characters/HeadGear/Hat_BallCap/Hat_BallCap_01.et", SCR_EArsenalItemType.HEADWEAR, SCR_EArsenalItemMode.DEFAULT), CTR_ShopPricing.CATEGORY_HEADGEAR, "cap");
		CheckString(CTR_ShopPricing.GetCategory("Prefabs/Items/Equipment/Radios/Radio_ANPRC77.et", SCR_EArsenalItemType.RADIO_BACKPACK, SCR_EArsenalItemMode.DEFAULT), CTR_ShopPricing.CATEGORY_RADIOS, "radio backpack");
		CheckString(CTR_ShopPricing.GetCategory("Prefabs/Items/Equipment/Accessories/ETool_ALICE/ETool_ALICE_FreeRoamBuilding_Gadget.et", SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.DEFAULT), CTR_ShopPricing.CATEGORY_TOOLS, "entrenching tool");

		// Not sold.
		CheckInt(CTR_ShopPricing.GetPrice("Prefabs/Weapons/Ammo/Ammo_Rocket_S5.et", SCR_EArsenalItemType.HELICOPTER, SCR_EArsenalItemMode.AMMUNITION, 35), 0, "helicopter rockets");
		CheckInt(CTR_ShopPricing.GetPrice("Prefabs/Weapons/AircraftWeapons/RocketPods/Pod.et", SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.PYLON, 250), 0, "pylons");
		CheckInt(CTR_ShopPricing.GetPrice("Prefabs/Items/PersonalBelongings/PersonalBelongings_US.et", SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.DEFAULT, 0), 0, "personal belongings");

		CheckString(CTR_ShopPricing.GetShop(CTR_ShopPricing.CATEGORY_LAUNCHERS), CTR_ShopPricing.SHOP_WEAPONS, "launchers in the weapons shop");
		CheckString(CTR_ShopPricing.GetShop(CTR_ShopPricing.CATEGORY_VESTS), CTR_ShopPricing.SHOP_GEAR, "vests in the gear shop");
		CheckString(CTR_ShopPricing.GetShop(CTR_ShopPricing.CATEGORY_MEDICAL), CTR_ShopPricing.SHOP_SUPPLIES, "medical in the supplies shop");
		CheckString(CTR_ShopPricing.GetShop(string.Empty), string.Empty, "no shop for unsold items");

		// One operation (about 650) buys a basic kit: rifle, six magazines, four dressings, a tourniquet.
		int kit = 520 + 6 * 15 + 4 * 10 + 10;
		Check(kit <= 700 && kit >= 600, "basic kit near one operation's pay: " + kit);
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! The main base of the running world: arsenals off, the three Contractors shops and a stash point in it.
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

		// One shop per kind of item; every item of a catalog belongs to its shop.
		CheckInt(shops.Count(), 3, "three shops at the base");
		array<string> shopIds = {};
		foreach (Managed shopManaged : shops)
		{
			MRX_ShopComponent shop = MRX_ShopComponent.Cast(shopManaged);
			string shopId = shop.GetShopId();
			shopIds.Insert(shopId);
			MRX_ShopDefinition definition = shop.GetDefinition();
			Check(definition != null, "catalog loads: " + shopId);
			if (!definition)
				continue;

			Check(definition.m_bAllowSell, "buys items back: " + shopId);
			CheckInt(definition.m_iSellPercent, MRX_ShopDefinition.DEFAULT_SELL_PERCENT, "buy-back percentage: " + shopId);
			int foreign;
			foreach (MRX_ShopItem item : definition.m_Catalog.m_aItems)
			{
				if (CTR_ShopPricing.GetShop(item.m_sCategory) != shopId)
					foreign++;
			}

			CheckInt(foreign, 0, "items of other shops in " + shopId);
			if (shopId == CTR_ShopPricing.SHOP_WEAPONS)
			{
				MRX_ShopItem rifle = definition.m_Catalog.FindItem("rifle_m16a2");
				Check(rifle && rifle.m_iPrice == 520, "M16A2 for sale at 520");
				Check(definition.m_Catalog.FindItem("rifle_m4a1_block_ii") != null, "RHS rifle for sale");
			}
			else if (shopId == CTR_ShopPricing.SHOP_GEAR)
			{
				Check(definition.m_Catalog.m_aItems.Count() > 500, "RHS clothing and gear, items: " + definition.m_Catalog.m_aItems.Count());
			}
			else if (shopId == CTR_ShopPricing.SHOP_SUPPLIES)
			{
				Check(definition.m_Catalog.FindItem("fielddressing_us_01") != null, "field dressing for sale");
			}
		}

		Check(shopIds.Contains(CTR_ShopPricing.SHOP_WEAPONS) && shopIds.Contains(CTR_ShopPricing.SHOP_GEAR) && shopIds.Contains(CTR_ShopPricing.SHOP_SUPPLIES), "weapons, gear and supplies shops");

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

//------------------------------------------------------------------------------------------------
//! A base shop window pages its whole catalog (client UI of the Workbench host). It stays open for a few
//! seconds so it can be looked at.
class CTR_Test_ShopWindowPages : CTR_TestCase
{
	protected static const int SHOW_MS = 4000;
	//! Weak: the menu system owns the dialog.
	protected MRX_ShopMenu m_Menu;

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return SHOW_MS + 10000;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		IEntity shopEntity;
		if (gameMode)
			shopEntity = CTR_DevTools.FindNear(gameMode.GetMainBasePos(), 40, MRX_ShopComponent);

		if (!shopEntity)
		{
			Skip("no base shop");
			return;
		}

		MRX_ShopComponent shop = MRX_ShopComponent.Cast(shopEntity.FindComponent(MRX_ShopComponent));
		MRX_ShopDefinition definition = shop.GetDefinition();
		m_Menu = MRX_ShopMenu.Open(shop);
		if (!m_Menu || !definition)
		{
			Check(false, "shop window opens");
			Finish();
			return;
		}

		int items;
		foreach (MRX_ShopItem item : definition.m_Catalog.m_aItems)
		{
			if (item.m_iPrice > 0)
				items++;
		}

		int pages = (items + MRX_ShopMenu.PAGE_SIZE - 1) / MRX_ShopMenu.PAGE_SIZE;
		CheckInt(m_Menu.GetPageCount(), pages, "pages of the whole catalog");
		CheckInt(m_Menu.GetRowCount(), MRX_ShopMenu.PAGE_SIZE, "first page is full");

		m_Menu.ShowPage(pages - 1);
		CheckInt(m_Menu.GetRowCount(), items - (pages - 1) * MRX_ShopMenu.PAGE_SIZE, "last page holds the rest");

		m_Menu.ShowPage(pages + 5);
		CheckInt(m_Menu.GetRowCount(), items - (pages - 1) * MRX_ShopMenu.PAGE_SIZE, "pages beyond the end show the last one");

		m_Menu.ShowCategory(1);
		Check(m_Menu.GetRowCount() > 0 && m_Menu.GetRowCount() <= MRX_ShopMenu.PAGE_SIZE, "a category starts on its first page");

		m_Menu.ShowCategory(0);
		GetGame().GetCallqueue().CallLater(CloseMenu, SHOW_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void CloseMenu()
	{
		if (m_Menu)
			m_Menu.Close();

		Finish();
	}
}
#endif
