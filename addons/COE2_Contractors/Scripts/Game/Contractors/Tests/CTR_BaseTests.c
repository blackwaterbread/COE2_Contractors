#ifdef WORKBENCH
class CTR_BaseTests
{
	//------------------------------------------------------------------------------------------------
	static void Register(notnull CTR_TestRunner runner)
	{
		runner.Add(new CTR_Test_ShopPricing());
		runner.Add(new CTR_Test_BaseSetup());
		runner.Add(new CTR_Test_ShopContents());
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
		CheckInt(CTR_ShopPricing.GetPrice("Prefabs/Weapons/Rifles/M16/Rifle_M16A2.et", SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.WEAPON, 8), 900, "M16A2 reference price");
		CheckInt(CTR_ShopPricing.GetPrice("Prefabs/Weapons/Handguns/M9/Handgun_M9.et", SCR_EArsenalItemType.PISTOL, SCR_EArsenalItemMode.WEAPON, 5), 700, "pistol reference price");
		CheckInt(CTR_ShopPricing.GetPrice("Prefabs/Items/Medicine/FieldDressing_01/FieldDressing_US_01.et", SCR_EArsenalItemType.HEAL, SCR_EArsenalItemMode.CONSUMABLE, 0), 8, "field dressing");
		ResourceName epinephrine = "Prefabs/Items/Medicine/EpinephrineInjection/ACE_Medical_EpinephrineInjection.et";
		CheckString(CTR_ShopPricing.GetCategory(epinephrine, SCR_EArsenalItemType.HEAL, SCR_EArsenalItemMode.CONSUMABLE), CTR_ShopPricing.CATEGORY_MEDICAL, "ACE epinephrine category");
		CheckInt(CTR_ShopPricing.GetPrice(epinephrine, SCR_EArsenalItemType.HEAL, SCR_EArsenalItemMode.CONSUMABLE, 3), 100, "ACE epinephrine price");

		// RHS rifles keep the default type and some the default mode.
		ResourceName rhsRifle = "Prefabs/Weapons/Rifles/M4A1/Rifle_M4A1_BLOCK_1.et";
		CheckString(CTR_ShopPricing.GetCategory(rhsRifle, SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.DEFAULT), CTR_ShopPricing.CATEGORY_RIFLES, "RHS rifle in default mode");
		CheckInt(CTR_ShopPricing.GetPrice(rhsRifle, SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.DEFAULT, 12), 1200, "RHS rifle price");
		ResourceName rhsMagazine = "Prefabs/Weapons/Magazines/6l23_plastic/Magazine_545x39_plastic_AK_30rnd_Ball_camo.et";
		CheckString(CTR_ShopPricing.GetCategory(rhsMagazine, SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.DEFAULT), CTR_ShopPricing.CATEGORY_AMMUNITION, "RHS magazine in default mode");
		CheckInt(CTR_ShopPricing.GetPrice(rhsMagazine, SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.DEFAULT, 2), 27, "magazine priced from its supply cost");

		// Weapon variants: mounted attachments (a higher supply cost) add to the family price; RHS variants with supply
		// cost 1 cost the bare weapon, and unknown rifles count at least a bare rifle's supply cost.
		CheckInt(CTR_ShopPricing.GetPrice("Prefabs/Weapons/Rifles/M4A1/Rifle_M4A1_BLOCK_0_NT4.et", SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.WEAPON_VARIANTS, 42), 1200 + 30 * CTR_ShopPricing.VARIANT_PRICE_PER_SUPPLY, "suppressed M4A1 variant");
		CheckInt(CTR_ShopPricing.GetPrice("Prefabs/Weapons/Rifles/SVD/Rifle_SVD_PSO1.et", SCR_EArsenalItemType.SNIPER_RIFLE, SCR_EArsenalItemMode.WEAPON_VARIANTS, 1), 2000, "scoped SVD variant (supply 1)");
		CheckInt(CTR_ShopPricing.GetPrice("Prefabs/Weapons/Rifles/AKS74UN/Rifle_AKS74UN_x.et", SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.WEAPON, 1), 800, "AKS-74UN variant (supply 1)");
		CheckInt(CTR_ShopPricing.GetPrice("Prefabs/Weapons/Rifles/Unknown/Rifle_Unknown.et", SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.WEAPON, 1), 400 + 80 * 8, "unknown rifle (supply 1)");

		// Launcher rockets are listed with the launchers, with their own prices.
		ResourceName rocket = "Prefabs/Weapons/Ammo/Ammo_Rocket_PG7VM.et";
		CheckString(CTR_ShopPricing.GetCategory(rocket, SCR_EArsenalItemType.ROCKET_LAUNCHER, SCR_EArsenalItemMode.AMMUNITION), CTR_ShopPricing.CATEGORY_LAUNCHERS, "rocket category");
		CheckInt(CTR_ShopPricing.GetPrice(rocket, SCR_EArsenalItemType.ROCKET_LAUNCHER, SCR_EArsenalItemMode.AMMUNITION, 10), 400, "rocket price");
		CheckInt(CTR_ShopPricing.GetPrice("Prefabs/Weapons/Launchers/RPG7/Launcher_RPG7.et", SCR_EArsenalItemType.ROCKET_LAUNCHER, SCR_EArsenalItemMode.WEAPON, 55), 1000, "launcher price");

		// Mortar parts and shells.
		ResourceName mortarBarrel = "Prefabs/Items/Equipment/Mortars/M252/Part_M252_Barrel.et";
		CheckString(CTR_ShopPricing.GetCategory(mortarBarrel, SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.SUPPORT_STATION), CTR_ShopPricing.CATEGORY_HEAVY_WEAPONS, "mortar part");
		CheckInt(CTR_ShopPricing.GetPrice(mortarBarrel, SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.SUPPORT_STATION, 90), 5000, "mortar part price");
		CheckInt(CTR_ShopPricing.GetPrice("Prefabs/Weapons/Ammo/Ammo_Shell_81mm_HE_M821.et", SCR_EArsenalItemType.MORTARS, SCR_EArsenalItemMode.AMMUNITION, 20), 600, "mortar shell price");

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
		CheckString(CTR_ShopPricing.GetShop(CTR_ShopPricing.CATEGORY_VESTS), CTR_ShopPricing.SHOP_EQUIPMENT, "vests in the equipment shop");
		CheckString(CTR_ShopPricing.GetShop(CTR_ShopPricing.CATEGORY_MEDICAL), CTR_ShopPricing.SHOP_EQUIPMENT, "medical in the equipment shop");
		CheckString(CTR_ShopPricing.GetShop(string.Empty), string.Empty, "no shop for unsold items");

		// Default contents come on top of the bare item. They replace the supply cost share of a weapon variant's
		// attachments; elsewhere a higher supply cost already prices built-in armor and plates, so the contents raise
		// such a price only when they are worth more.
		ResourceName medicalKit = "Prefabs/Items/Medicine/MedicalKit_01/MedicalKit_01_US.et";
		CheckInt(CTR_ShopPricing.GetPrice(medicalKit, SCR_EArsenalItemType.HEAL, SCR_EArsenalItemMode.CONSUMABLE, 5, 1989), 500 + 1989, "medical kit with its contents");
		ResourceName suppressedRifle = "Prefabs/Weapons/Rifles/M4A1/Rifle_M4A1_BLOCK_0_NT4.et";
		CheckInt(CTR_ShopPricing.GetPrice(suppressedRifle, SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.WEAPON_VARIANTS, 42, 1600), 1200 + 1600, "variant priced from its measured attachments");
		ResourceName carrier = "Prefabs/Characters/Vests/Vest_JPC/Vest_JPC.et";
		CheckInt(CTR_ShopPricing.GetPrice(carrier, SCR_EArsenalItemType.VEST_AND_WAIST, SCR_EArsenalItemMode.DEFAULT, 25, 1200), 100 + 60 * 25, "armored vest whose supply cost covers its plates");
		CheckInt(CTR_ShopPricing.GetPrice(carrier, SCR_EArsenalItemType.VEST_AND_WAIST, SCR_EArsenalItemMode.DEFAULT, 25, 1600), 100 + 1600, "armored vest with plates worth more");

		// Balance: a typical operation (clear area, destroy cache, kill officer) buys a professional kit (carbine, ACOG,
		// six magazines, armored plate carrier, FAST helmet, PVS-14) with money left, but not two of them.
		int kit = CTR_ShopPricing.GetPrice("Prefabs/Weapons/Rifles/M4A1/Rifle_M4A1_BLOCK_II.et", SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.WEAPON, 12);
		kit += CTR_ShopPricing.GetPrice("Prefabs/Weapons/Attachments/Optics/ta31rco/Optic_TA31RCO.et", SCR_EArsenalItemType.WEAPON_ATTACHMENT, SCR_EArsenalItemMode.ATTACHMENT, 32);
		kit += 6 * CTR_ShopPricing.GetPrice("Prefabs/Weapons/Magazines/Pmag/Magazine_556x45_Pmag.et", SCR_EArsenalItemType.RIFLE, SCR_EArsenalItemMode.AMMUNITION, 2);
		kit += CTR_ShopPricing.GetPrice("Prefabs/Characters/Vests/Vest_JPC/Vest_JPC.et", SCR_EArsenalItemType.VEST_AND_WAIST, SCR_EArsenalItemMode.DEFAULT, 25);
		kit += CTR_ShopPricing.GetPrice("Prefabs/Characters/HeadGear/Helmet_OPSCORE/Helmet_OPSCORE.et", SCR_EArsenalItemType.HEADWEAR, SCR_EArsenalItemMode.DEFAULT, 2);
		kit += CTR_ShopPricing.GetPrice("Prefabs/Items/Equipment/Nightvision/PVS14/NVG_PVS14_Base.et", SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.DEFAULT, 10);
		CTR_Settings settings = CTR_Settings.CreateDefault();
		int operation = settings.GetTaskReward("COE_ClearAreaTaskBuilder", string.Empty) + settings.GetTaskReward("COE_DestroyCacheTaskBuilder", string.Empty) + settings.GetTaskReward("COE_EnemyOfficerTaskBuilder", string.Empty);
		Check(kit * 4 >= operation && kit * 4 <= operation * 3, string.Format("professional kit %1 is a quarter to three quarters of an operation's pay %2", kit, operation));
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! The main base of the running world: two of its arsenal boxes are the Contractors arsenal shops, the other arsenals
//! are off, and a stash point stands next to them.
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
		array<string> shopIds = {};
		foreach (Managed managed : arsenals)
		{
			SCR_ArsenalComponent arsenal = SCR_ArsenalComponent.Cast(managed);
			CheckInt(arsenal.GetArsenalSaveType(), SCR_EArsenalSaveType.SAVING_DISABLED, "arsenal loadout saving");
			MRX_ArsenalShopComponent arsenalShop = MRX_ArsenalShopComponent.Find(arsenal.GetOwner());
			if (!arsenalShop)
			{
				Check(!arsenal.IsArsenalEnabled(), "arsenal disabled");
				CheckInt(arsenal.GetSupportedArsenalItemModes(), 0, "arsenal offers no items");
				continue;
			}

			Check(arsenal.IsArsenalEnabled(), "arsenal shop enabled");
			MRX_ShopDefinition shop = arsenalShop.GetShop();
			Check(shop != null, "arsenal shop catalog loads");
			if (!shop)
				continue;

			shopIds.Insert(shop.m_sShopId);
			CheckShop(shop, arsenalShop);
		}

		CheckInt(shopIds.Count(), 2, "arsenal shops at the base");
		Check(shopIds.Contains(CTR_ShopPricing.SHOP_WEAPONS) && shopIds.Contains(CTR_ShopPricing.SHOP_EQUIPMENT), "weapons and equipment arsenal shops");

		// The stash prefab has no Hierarchy component, so it is not a child of the base but stands next to it.
		GetGame().GetWorld().QueryEntitiesBySphere(base.GetOrigin(), NEARBY_RADIUS, CollectNearby);
		int stashPoints;
		foreach (IEntity entity : m_aNearby)
		{
			if (entity.FindComponent(MRX_ShopComponent))
				Check(MRX_ArsenalShopComponent.Find(entity) != null, "every shop at the base is an arsenal shop");

			if (entity.FindComponent(MRX_StashPointComponent))
				stashPoints++;
		}

		CheckInt(stashPoints, 1, "one stash point at the base");
		Finish();
	}

	//------------------------------------------------------------------------------------------------
	//! Every item of a catalog belongs to its shop, and the arsenal lists all of them.
	protected void CheckShop(notnull MRX_ShopDefinition shop, notnull MRX_ArsenalShopComponent arsenalShop)
	{
		string shopId = shop.m_sShopId;
		Check(shop.m_bAllowSell, "buys items back: " + shopId);
		CheckInt(shop.m_iSellPercent, MRX_ShopDefinition.DEFAULT_SELL_PERCENT, "buy-back percentage: " + shopId);
		int foreign, priced;
		foreach (MRX_ShopItem item : shop.m_Catalog.m_aItems)
		{
			if (CTR_ShopPricing.GetShop(item.m_sCategory) != shopId)
				foreign++;

			if (item.m_iPrice > 0)
				priced++;
		}

		CheckInt(foreign, 0, "items of the other shop in " + shopId);
		array<SCR_ArsenalItem> listed = {};
		CheckInt(arsenalShop.GetArsenalItems(listed), priced, "arsenal lists the catalog of " + shopId);

		if (shopId == CTR_ShopPricing.SHOP_WEAPONS)
		{
			// Reference price plus the flash hider and magazine it comes with.
			MRX_ShopItem rifle = shop.m_Catalog.FindItem("rifle_m16a2");
			Check(rifle && rifle.m_iPrice > 900, "M16A2 for sale at 900 plus what it comes with");
			Check(shop.m_Catalog.FindItem("rifle_m4a1_block_ii") != null, "RHS rifle for sale");
		}
		else if (shopId == CTR_ShopPricing.SHOP_EQUIPMENT)
		{
			Check(shop.m_Catalog.m_aItems.Count() > 1000, "RHS clothing, gear and supplies, items: " + shop.m_Catalog.m_aItems.Count());
			Check(shop.m_Catalog.FindItem("fielddressing_us_01") != null, "field dressing for sale");
			Check(shop.m_Catalog.FindItem("ace_medical_epinephrineinjection") != null, "ACE epinephrine for sale");
			Check(shop.m_Catalog.FindItem("ace_medical_naloxoneinjection") != null, "ACE Circulation naloxone for sale");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectNearby(IEntity entity)
	{
		m_aNearby.Insert(entity);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Buying an item and selling it back never pays, in one piece or part by part at either arsenal shop: prices cover the
//! default contents. Spawns every item of both catalogs once (about 30 s) and writes the report the catalog generator
//! reads (REPORT_FILE; copy it to tools/shop-default-contents.csv, then run "Create Contractors Configs").
class CTR_Test_ShopContents : CTR_TestCase
{
	static const string REPORT_FILE = "$profile:ctr_shop_default_contents.csv";
	protected static const float SPAWN_HEIGHT = 500;
	protected static const int MAX_REPORTED = 10;

	protected ref MRX_ShopContentsCheck m_Check;

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return 180000;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode || !gameMode.GetMainBase())
		{
			Skip("no COE2 main base");
			return;
		}

		// Contents may be sold at the other shop, so the items of both price them.
		MRX_ShopCatalog catalog = new MRX_ShopCatalog();
		catalog.m_aItems = {};
		int sellPercent = MRX_ShopDefinition.DEFAULT_SELL_PERCENT;
		array<Managed> arsenals = {};
		CTR_BaseTests.FindComponents(gameMode.GetMainBase(), MRX_ArsenalShopComponent, arsenals);
		foreach (Managed managed : arsenals)
		{
			MRX_ShopDefinition shop = MRX_ArsenalShopComponent.Cast(managed).GetShop();
			if (!shop)
				continue;

			sellPercent = shop.m_iSellPercent;
			foreach (MRX_ShopItem item : shop.m_Catalog.m_aItems)
			{
				catalog.m_aItems.Insert(item);
			}
		}

		if (catalog.m_aItems.IsEmpty())
		{
			Skip("no arsenal shop catalogs");
			return;
		}

		m_Check = new MRX_ShopContentsCheck();
		m_Check.GetOnDone().Insert(OnDone);
		m_Check.Start(MRX_ShopDefinition.Create("contractors_all", catalog, sellPercent), gameMode.GetMainBasePos() + Vector(0, SPAWN_HEIGHT, 0));
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDone(array<ref MRX_ShopContentsEntry> entries)
	{
		Check(MRX_ShopContentsCheck.WriteReport(REPORT_FILE, entries), "report written: " + REPORT_FILE);
		int unmeasured, profitable;
		foreach (MRX_ShopContentsEntry entry : entries)
		{
			if (!entry.m_bMeasured)
			{
				unmeasured++;
				continue;
			}

			if (!entry.IsProfitable())
				continue;

			profitable++;
			if (profitable <= MAX_REPORTED)
				Check(false, string.Format("%1 costs %2, sells back for %3 with its contents", entry.m_sItemId, entry.m_iPrice, entry.m_iSellPrice + entry.m_iContentsSell));
		}

		CheckInt(unmeasured, 0, "items that could not be spawned");
		CheckInt(profitable, 0, "items that pay to buy and sell back");
		Finish();
	}
}
#endif
