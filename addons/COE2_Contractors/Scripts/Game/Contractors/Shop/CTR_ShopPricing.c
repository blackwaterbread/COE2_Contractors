//! Shops and prices of the Contractors base, derived from the arsenal data of the vanilla and RHS item catalogs.
//! Two arsenal shops by kind of item (weapons; clothing, gear and supplies), each split into finer categories by
//! arsenal type and prefab folder. Prices are in USD at about real-world prices (reference prices per weapon family and
//! notable item, the rest from the arsenal supply cost); see the plan for the sources and the balance with the pay.
//! A price also covers the items the prefab comes with (default contents), so buying and selling back never pays.
class CTR_ShopPricing
{
	static const string SHOP_WEAPONS = "contractors_weapons";
	static const string SHOP_EQUIPMENT = "contractors_equipment";
	//! The quartermaster at the base: stash pages and other services (products, not items).
	static const string SHOP_SERVICES = "contractors_services";
	//! Price of the attachments mounted on a weapon variant, per supply cost point above the bare weapon.
	static const int VARIANT_PRICE_PER_SUPPLY = 80;

	// Weapons shop
	static const string CATEGORY_RIFLES = "Rifles";
	static const string CATEGORY_SNIPER_RIFLES = "Sniper rifles";
	static const string CATEGORY_MACHINE_GUNS = "Machine guns";
	static const string CATEGORY_PISTOLS = "Pistols";
	static const string CATEGORY_LAUNCHERS = "Launchers";
	static const string CATEGORY_AMMUNITION = "Ammunition";
	static const string CATEGORY_OPTICS = "Optics";
	static const string CATEGORY_MUZZLE = "Muzzle";
	static const string CATEGORY_ATTACHMENTS = "Attachments";
	static const string CATEGORY_EXPLOSIVES = "Explosives";
	static const string CATEGORY_HEAVY_WEAPONS = "Heavy weapons";

	// Equipment shop: clothing and gear
	static const string CATEGORY_HELMETS = "Helmets";
	static const string CATEGORY_HEADGEAR = "Headgear";
	static const string CATEGORY_TOPS = "Shirts & jackets";
	static const string CATEGORY_PANTS = "Pants";
	static const string CATEGORY_BOOTS_GLOVES = "Boots & gloves";
	static const string CATEGORY_VESTS = "Vests & armor";
	static const string CATEGORY_BACKPACKS = "Backpacks";

	// Equipment shop: supplies
	static const string CATEGORY_MEDICAL = "Medical";
	static const string CATEGORY_RADIOS = "Radios";
	static const string CATEGORY_VISION = "Binoculars & NVG";
	static const string CATEGORY_NAVIGATION = "Navigation";
	static const string CATEGORY_TOOLS = "Tools";
	static const string CATEGORY_ACCESSORIES = "Accessories";

	//! Vehicle and helicopter parts and ammunition are not personal gear.
	protected static const int EXCLUDED_TYPES = SCR_EArsenalItemType.HELICOPTER | SCR_EArsenalItemType.VEHICLE;
	protected static const int EXPLOSIVE_TYPES = SCR_EArsenalItemType.LETHAL_THROWABLE | SCR_EArsenalItemType.NON_LETHAL_THROWABLE | SCR_EArsenalItemType.EXPLOSIVES;
	protected static const int FOOTWEAR_TYPES = SCR_EArsenalItemType.FOOTWEAR | SCR_EArsenalItemType.HANDWEAR;

	protected static ref map<string, ref array<ref CTR_PriceRule>> s_mRules;

	//------------------------------------------------------------------------------------------------
	//! \return Shop category, or empty when the item is not sold.
	static string GetCategory(ResourceName prefab, SCR_EArsenalItemType type, SCR_EArsenalItemMode mode)
	{
		string path = prefab;
		path.ToLower();
		if ((type & EXCLUDED_TYPES) || mode == SCR_EArsenalItemMode.PYLON)
			return string.Empty;

		// Corpse belongings, Conflict cache notes and the Conflict deployable tent have no use here.
		if (path.Contains("/personalbelongings/") || path.Contains("/misc/caches/") || path.Contains("/equipment/tents/"))
			return string.Empty;

		// Mortars, static machine guns on tripods, their parts and shells.
		if ((type & SCR_EArsenalItemType.MORTARS) || path.Contains("/equipment/mortars/") || path.Contains("/equipment/tripods/"))
			return CATEGORY_HEAVY_WEAPONS;

		if (IsAmmunition(path, mode))
		{
			if (type & SCR_EArsenalItemType.ROCKET_LAUNCHER)
				return CATEGORY_LAUNCHERS;

			return CATEGORY_AMMUNITION;
		}

		if (path.Contains("/weapons/attachments/") || type == SCR_EArsenalItemType.WEAPON_ATTACHMENT || (mode == SCR_EArsenalItemMode.ATTACHMENT && path.Contains("/weapons/")))
		{
			if (path.Contains("/attachments/optics/"))
				return CATEGORY_OPTICS;

			if (path.Contains("/attachments/muzzle/"))
				return CATEGORY_MUZZLE;

			return CATEGORY_ATTACHMENTS;
		}

		if ((type & EXPLOSIVE_TYPES) || path.Contains("/weapons/grenades/") || path.Contains("/weapons/explosives/") || path.Contains("/weapons/flares/"))
			return CATEGORY_EXPLOSIVES;

		// The spectrum device is held like a weapon.
		if (path.Contains("/weapons/misc/"))
			return CATEGORY_TOOLS;

		// RHS leaves the type of its rifles at the default and some in the default mode, so the folder decides too.
		if (path.Contains("/weapons/") || mode == SCR_EArsenalItemMode.WEAPON || mode == SCR_EArsenalItemMode.WEAPON_VARIANTS)
		{
			if (type & SCR_EArsenalItemType.SNIPER_RIFLE)
				return CATEGORY_SNIPER_RIFLES;

			if (type & SCR_EArsenalItemType.MACHINE_GUN)
				return CATEGORY_MACHINE_GUNS;

			if (type & SCR_EArsenalItemType.PISTOL)
				return CATEGORY_PISTOLS;

			if (type & SCR_EArsenalItemType.ROCKET_LAUNCHER)
				return CATEGORY_LAUNCHERS;

			if (type & SCR_EArsenalItemType.RIFLE)
				return CATEGORY_RIFLES;
		}

		if ((type & SCR_EArsenalItemType.HEAL) || path.Contains("/items/medicine/"))
			return CATEGORY_MEDICAL;

		// Helmet attachments and armor plates have the equipment type, so the folder decides.
		if (path.Contains("/characters/headgear/") || path.Contains("/characters/eyewear/") || (type & SCR_EArsenalItemType.HEADWEAR))
		{
			if (path.Contains("/headgear/helmet_") || FilePath.StripPath(path).StartsWith("helmet_"))
				return CATEGORY_HELMETS;

			return CATEGORY_HEADGEAR;
		}

		if (path.Contains("/characters/vests/") || (type & SCR_EArsenalItemType.VEST_AND_WAIST))
			return CATEGORY_VESTS;

		if (type & SCR_EArsenalItemType.LEGS)
			return CATEGORY_PANTS;

		if ((type & FOOTWEAR_TYPES) || path.Contains("/characters/footwear/") || path.Contains("/characters/handwear/"))
			return CATEGORY_BOOTS_GLOVES;

		if ((type & SCR_EArsenalItemType.TORSO) || path.Contains("/characters/uniforms/"))
			return CATEGORY_TOPS;

		if ((type & SCR_EArsenalItemType.RADIO_BACKPACK) || path.Contains("/equipment/radios/"))
			return CATEGORY_RADIOS;

		if ((type & SCR_EArsenalItemType.BACKPACK) || path.Contains("/equipment/backpacks/"))
			return CATEGORY_BACKPACKS;

		if (path.Contains("/equipment/binoculars/") || path.Contains("/equipment/nightvision/") || path.Contains("/equipment/thermals/"))
			return CATEGORY_VISION;

		if (path.Contains("/equipment/maps/") || path.Contains("/equipment/compass/") || path.Contains("/equipment/navigation/") || path.Contains("/equipment/watches/"))
			return CATEGORY_NAVIGATION;

		// Entrenching tools sit among the accessories but build fortifications.
		if ((path.Contains("/equipment/patches/") || path.Contains("/equipment/accessories/")) && !path.Contains("/etool_"))
			return CATEGORY_ACCESSORIES;

		return CATEGORY_TOOLS;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Price in USD, 0 when the item is not sold. Known items and weapon families have a reference price
	//! (real-world unit cost or street price, rounded); the rest is priced from their arsenal supply cost.
	//! \param contentsPrice Price of the items the prefab comes with (mounted attachments, a loaded magazine, armor
	//! plates, the contents of a kit). The price covers them: it is at least the price at supply cost 0 (the bare
	//! weapon, the empty carrier) plus the contents. For a weapon family with a reference price they replace the share
	//! a variant's supply cost adds for its mounted attachments. Otherwise a higher supply cost already prices built-in
	//! armor, mounted attachments and plates, so the contents only raise the price when they are worth more.
	static int GetPrice(ResourceName prefab, SCR_EArsenalItemType type, SCR_EArsenalItemMode mode, int supplyCost, int contentsPrice = 0)
	{
		string category = GetCategory(prefab, type, mode);
		if (category.IsEmpty())
			return 0;

		string path = prefab;
		path.ToLower();
		supplyCost = Math.Max(0, supplyCost);
		bool ammunition = IsAmmunition(path, mode);
		int price, ownPrice;
		CTR_PriceRule rule = FindRule(category, path);
		if (rule)
		{
			price = rule.GetPrice(supplyCost);
			ownPrice = rule.GetPrice(0);
			if (contentsPrice > 0 && rule.m_iBaseCost > 0)
				return ownPrice + contentsPrice;
		}
		else
		{
			price = GetFallbackPrice(category, ammunition, supplyCost);
			ownPrice = GetFallbackPrice(category, ammunition, 0);
		}

		if (contentsPrice > 0)
			price = Math.Max(price, ownPrice + contentsPrice);

		return price;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Shop that sells a category, empty for none.
	static string GetShop(string category)
	{
		array<string> weapons = {CATEGORY_RIFLES, CATEGORY_SNIPER_RIFLES, CATEGORY_MACHINE_GUNS, CATEGORY_PISTOLS, CATEGORY_LAUNCHERS, CATEGORY_AMMUNITION, CATEGORY_OPTICS, CATEGORY_MUZZLE, CATEGORY_ATTACHMENTS, CATEGORY_EXPLOSIVES, CATEGORY_HEAVY_WEAPONS};
		if (weapons.Contains(category))
			return SHOP_WEAPONS;

		array<string> equipment = {
			CATEGORY_HELMETS, CATEGORY_HEADGEAR, CATEGORY_TOPS, CATEGORY_PANTS, CATEGORY_BOOTS_GLOVES, CATEGORY_VESTS, CATEGORY_BACKPACKS,
			CATEGORY_MEDICAL, CATEGORY_RADIOS, CATEGORY_VISION, CATEGORY_NAVIGATION, CATEGORY_TOOLS, CATEGORY_ACCESSORIES
		};
		if (equipment.Contains(category))
			return SHOP_EQUIPMENT;

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Display order of the categories in their shop.
	static int GetCategoryOrder(string category)
	{
		array<string> order = {
			CATEGORY_RIFLES, CATEGORY_SNIPER_RIFLES, CATEGORY_MACHINE_GUNS, CATEGORY_PISTOLS, CATEGORY_LAUNCHERS, CATEGORY_AMMUNITION,
			CATEGORY_OPTICS, CATEGORY_MUZZLE, CATEGORY_ATTACHMENTS, CATEGORY_EXPLOSIVES, CATEGORY_HEAVY_WEAPONS,
			CATEGORY_HELMETS, CATEGORY_HEADGEAR, CATEGORY_TOPS, CATEGORY_PANTS, CATEGORY_BOOTS_GLOVES, CATEGORY_VESTS, CATEGORY_BACKPACKS,
			CATEGORY_MEDICAL, CATEGORY_RADIOS, CATEGORY_VISION, CATEGORY_NAVIGATION, CATEGORY_TOOLS, CATEGORY_ACCESSORIES
		};
		return order.Find(category);
	}

	//------------------------------------------------------------------------------------------------
	//! First rule of the category whose keyword is in the lowercase prefab path.
	protected static CTR_PriceRule FindRule(string category, string path)
	{
		if (!s_mRules)
			s_mRules = CreateRules();

		array<ref CTR_PriceRule> rules = s_mRules.Get(category);
		if (!rules)
			return null;

		foreach (CTR_PriceRule rule : rules)
		{
			if (path.Contains(rule.m_sKeyword))
				return rule;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Items without a reference price: a base price per category plus a factor of the supply cost. RHS gives some
	//! weapon variants a supply cost of 1, so weapons count at least the cost of a bare weapon of their kind.
	protected static int GetFallbackPrice(string category, bool ammunition, int supplyCost)
	{
		if (ammunition)
		{
			if (category == CATEGORY_LAUNCHERS)
				return 100 + 50 * supplyCost;

			if (category == CATEGORY_HEAVY_WEAPONS)
				return 100 + 30 * supplyCost;

			return 15 + 6 * supplyCost;
		}

		if (category == CATEGORY_RIFLES)
			return 400 + 80 * Math.Max(8, supplyCost);

		if (category == CATEGORY_SNIPER_RIFLES)
			return 1000 + 100 * Math.Max(20, supplyCost);

		if (category == CATEGORY_MACHINE_GUNS)
			return 1000 + 80 * Math.Max(25, supplyCost);

		if (category == CATEGORY_PISTOLS)
			return 300 + 60 * Math.Max(5, supplyCost);

		if (category == CATEGORY_LAUNCHERS)
			return 500 + 50 * Math.Max(30, supplyCost);

		if (category == CATEGORY_OPTICS)
			return 100 + 40 * supplyCost;

		// Suppressors have supply cost 10, flash hiders and brakes 0.
		if (category == CATEGORY_MUZZLE)
			return 100 + 110 * supplyCost;

		if (category == CATEGORY_ATTACHMENTS)
			return 30 + 30 * supplyCost;

		if (category == CATEGORY_EXPLOSIVES)
			return 20 + 10 * supplyCost;

		if (category == CATEGORY_HEAVY_WEAPONS)
			return 500 + 100 * supplyCost;

		if (category == CATEGORY_MEDICAL)
			return 10 + 10 * supplyCost;

		if (category == CATEGORY_HELMETS)
			return 400 + 100 * supplyCost;

		if (category == CATEGORY_HEADGEAR)
			return 30 + 20 * supplyCost;

		if (category == CATEGORY_TOPS)
			return 50 + 30 * supplyCost;

		if (category == CATEGORY_PANTS)
			return 50 + 30 * supplyCost;

		if (category == CATEGORY_BOOTS_GLOVES)
			return 120 + 20 * supplyCost;

		// Supply cost 25 for armored vests (carrier and plates), 2 to 3 for chest rigs.
		if (category == CATEGORY_VESTS)
			return 100 + 60 * supplyCost;

		if (category == CATEGORY_BACKPACKS)
			return 80 + 20 * supplyCost;

		if (category == CATEGORY_RADIOS || category == CATEGORY_VISION)
			return 200 + 100 * supplyCost;

		if (category == CATEGORY_ACCESSORIES)
			return 15 + 20 * supplyCost;

		// Navigation, tools.
		return 50 + 50 * supplyCost;
	}

	//------------------------------------------------------------------------------------------------
	//! Reference prices in USD, rounded: military unit cost where public, otherwise civilian, surplus or black market
	//! prices (see the plan for the sources). Where the game use is far below the real cost (radios, cosmetic parts,
	//! electronics) or the real cost differs far from the other side's equivalent (Western weapons, night vision), the
	//! game use sets the price instead. Keywords are matched against the lowercase prefab path in order, so specific
	//! ones go first. A weapon rule with a base supply cost prices variants with mounted attachments (a higher supply
	//! cost) above the bare weapon.
	protected static map<string, ref array<ref CTR_PriceRule>> CreateRules()
	{
		map<string, ref array<ref CTR_PriceRule>> rules = new map<string, ref array<ref CTR_PriceRule>>();

		array<ref CTR_PriceRule> rulesRifles = {
			new CTR_PriceRule("/rifles/m16/", 900, 8),
			new CTR_PriceRule("/rifles/m4a1/", 1200, 12),
			new CTR_PriceRule("/rifles/hk416a5/", 2000, 42),
			new CTR_PriceRule("/rifles/m27iar/", 2000, 10),
			new CTR_PriceRule("/rifles/ak74m/", 800, 10),
			new CTR_PriceRule("/rifles/ak74/", 600, 10),
			new CTR_PriceRule("/rifles/aks74un/", 800, 12),
			new CTR_PriceRule("/rifles/aks74u/", 700, 10),
			new CTR_PriceRule("/rifles/ak200/", 1100, 12),
			new CTR_PriceRule("/rifles/an94/", 2000, 25),
			new CTR_PriceRule("/rifles/sr3m/", 3000, 25),
			new CTR_PriceRule("/rifles/vz58/", 600, 10)
		};
		rules.Insert(CATEGORY_RIFLES, rulesRifles);

		array<ref CTR_PriceRule> rulesSniperRifles = {
			new CTR_PriceRule("/rifles/m14/", 2000, 20),
			new CTR_PriceRule("/rifles/m40/", 2500, 10),
			new CTR_PriceRule("/rifles/svd/", 2000, 20),
			new CTR_PriceRule("/rifles/m16/", 1500, 10)
		};
		rules.Insert(CATEGORY_SNIPER_RIFLES, rulesSniperRifles);

		array<ref CTR_PriceRule> rulesMachineGuns = {
			new CTR_PriceRule("/machineguns/m249/", 3000, 40),
			new CTR_PriceRule("/machineguns/m240/", 4000, 50),
			new CTR_PriceRule("/machineguns/m60/", 3800, 50),
			new CTR_PriceRule("/machineguns/pkm/", 3000, 50),
			new CTR_PriceRule("/machineguns/rpk74", 1200, 25),
			new CTR_PriceRule("/machineguns/uk59/", 4000, 50)
		};
		rules.Insert(CATEGORY_MACHINE_GUNS, rulesMachineGuns);

		array<ref CTR_PriceRule> rulesPistols = {
			new CTR_PriceRule("/handguns/m9/", 700, 5),
			new CTR_PriceRule("/handguns/glock/", 550, 5),
			new CTR_PriceRule("/handguns/m17/", 650, 5),
			new CTR_PriceRule("/handguns/pm/", 450, 5),
			new CTR_PriceRule("/handguns/aps/", 1000, 7),
			new CTR_PriceRule("/handguns/mp443/", 700, 5)
		};
		rules.Insert(CATEGORY_PISTOLS, rulesPistols);

		array<ref CTR_PriceRule> rulesLaunchers = {
			new CTR_PriceRule("pg7vl", 500),
			new CTR_PriceRule("pg7vr", 2000),
			new CTR_PriceRule("pg7vm", 400),
			new CTR_PriceRule("tbg7v", 1800),
			new CTR_PriceRule("og7v", 300),
			new CTR_PriceRule("pg7v", 300),
			new CTR_PriceRule("container_mk153_hedp", 700),
			new CTR_PriceRule("container_mk153_heaa", 900),
			new CTR_PriceRule("container_mk153_ne", 1200),
			new CTR_PriceRule("/launchers/m72/", 1500),
			new CTR_PriceRule("/launchers/rpg7/", 1000, 55),
			new CTR_PriceRule("/launchers/rpg22/", 800),
			new CTR_PriceRule("/launchers/rpg75/", 600),
			new CTR_PriceRule("/launchers/rpoa/", 3500, 75),
			new CTR_PriceRule("/launchers/mk153/", 3000, 75),
			new CTR_PriceRule("/grenadelaunchers/gm94/", 3000, 90)
		};
		rules.Insert(CATEGORY_LAUNCHERS, rulesLaunchers);

		array<ref CTR_PriceRule> rulesOptics = {
			new CTR_PriceRule("pas13g", 6000),
			new CTR_PriceRule("reap_ir", 5000),
			new CTR_PriceRule("infratech1tws", 4500),
			new CTR_PriceRule("/mbs/", 800),
			new CTR_PriceRule("m8541", 3500),
			new CTR_PriceRule("dedal_nv", 3000),
			new CTR_PriceRule("vc18dsco", 2700),
			new CTR_PriceRule("1pn93", 2500),
			new CTR_PriceRule("ta648mdo", 2200),
			new CTR_PriceRule("su230", 2000),
			new CTR_PriceRule("optic_spp", 2000),
			new CTR_PriceRule("ta31rco", 1500),
			new CTR_PriceRule("optic_artii", 1500),
			new CTR_PriceRule("optic_leupoldmk4", 1500),
			new CTR_PriceRule("aimpoint_t1", 900),
			new CTR_PriceRule("exps", 900),
			new CTR_PriceRule("1p87", 900),
			new CTR_PriceRule("1p86", 800),
			new CTR_PriceRule("bravo4", 600),
			new CTR_PriceRule("rmr", 600),
			new CTR_PriceRule("1p21", 600),
			new CTR_PriceRule("optic_ap2k", 500),
			new CTR_PriceRule("1p78", 450),
			new CTR_PriceRule("zenit_vzor3", 400),
			new CTR_PriceRule("1p63", 350),
			new CTR_PriceRule("optic_1p29", 350),
			new CTR_PriceRule("uk59_4x8", 300),
			new CTR_PriceRule("pgo7v3", 300),
			new CTR_PriceRule("po4x24", 300),
			new CTR_PriceRule("optic_pso1", 250),
			new CTR_PriceRule("optic_pgo7", 250),
			new CTR_PriceRule("optic_4x20", 250),
			new CTR_PriceRule("kac_folding_sights", 250),
			new CTR_PriceRule("m4a1_carry_handle", 150),
			new CTR_PriceRule("lmt_l8a", 150),
			new CTR_PriceRule("mbus", 120)
		};
		rules.Insert(CATEGORY_OPTICS, rulesOptics);

		array<ref CTR_PriceRule> rulesMuzzle = {
			new CTR_PriceRule("kac_nt4", 1600)
		};
		rules.Insert(CATEGORY_MUZZLE, rulesMuzzle);

		array<ref CTR_PriceRule> rulesAttachments = {
			new CTR_PriceRule("psq23", 1500),
			new CTR_PriceRule("anpeq16", 1200),
			new CTR_PriceRule("anpeq15", 1000),
			new CTR_PriceRule("perst", 900),
			new CTR_PriceRule("ugl_gp25", 500),
			new CTR_PriceRule("/lights/", 300),
			new CTR_PriceRule("harris_bipod", 130),
			new CTR_PriceRule("bayonet", 80),
			new CTR_PriceRule("grip", 60)
		};
		rules.Insert(CATEGORY_ATTACHMENTS, rulesAttachments);

		array<ref CTR_PriceRule> rulesExplosives = {
			new CTR_PriceRule("grenade_m67", 60),
			new CTR_PriceRule("grenade_rgd5", 25),
			new CTR_PriceRule("smoke_m18", 40),
			new CTR_PriceRule("smoke_anm8hc", 40),
			new CTR_PriceRule("smoke_rdg2", 20),
			new CTR_PriceRule("flarestarparachute", 60),
			new CTR_PriceRule("flare_rsp30", 40),
			new CTR_PriceRule("demoblock_m112", 40),
			new CTR_PriceRule("demoblock_tsh400g", 20),
			new CTR_PriceRule("mine_m14", 30),
			new CTR_PriceRule("mine_m15at", 150),
			new CTR_PriceRule("mine_pmn4", 20),
			new CTR_PriceRule("mine_tm62m", 100),
			new CTR_PriceRule("blastingmachine", 150)
		};
		rules.Insert(CATEGORY_EXPLOSIVES, rulesExplosives);

		array<ref CTR_PriceRule> rulesHeavyWeapons = {
			new CTR_PriceRule("part_m252_", 5000),
			new CTR_PriceRule("part_2b14_", 3500),
			new CTR_PriceRule("part_m2_gun", 13000),
			new CTR_PriceRule("part_m3_tripod", 1500),
			new CTR_PriceRule("part_nsv_gun", 10000),
			new CTR_PriceRule("part_nsv_tripod", 1200),
			new CTR_PriceRule("part_tripod_m122", 500),
			new CTR_PriceRule("part_tripod_6t5", 400),
			new CTR_PriceRule("_practice_", 150),
			new CTR_PriceRule("81mm_he_", 600),
			new CTR_PriceRule("82mm_he_", 400),
			new CTR_PriceRule("_illum_", 500),
			new CTR_PriceRule("_smoke_", 350),
			new CTR_PriceRule("ammobox_81mm", 1800),
			new CTR_PriceRule("ammobox_82mm", 1200),
			new CTR_PriceRule("ballistictable", 20),
			new CTR_PriceRule("balistictable", 20)
		};
		rules.Insert(CATEGORY_HEAVY_WEAPONS, rulesHeavyWeapons);

		array<ref CTR_PriceRule> rulesMedical = {
			new CTR_PriceRule("fielddressing", 8),
			new CTR_PriceRule("tourniquet", 40),
			new CTR_PriceRule("salinebag", 15),
			new CTR_PriceRule("morphine", 30),
			new CTR_PriceRule("medicalkit", 500),
			new CTR_PriceRule("epinephrine", 100),
			new CTR_PriceRule("naloxone", 50),
			new CTR_PriceRule("phenylephrine", 30),
			new CTR_PriceRule("metoprolol", 20),
			new CTR_PriceRule("ammoniumcarbonate", 5)
		};
		rules.Insert(CATEGORY_MEDICAL, rulesMedical);

		// Covers and bands sit in the helmet folders but protect nothing; the tank helmet's folder looks like theirs.
		array<ref CTR_PriceRule> rulesHelmets = {
			new CTR_PriceRule("counterweight", 60),
			new CTR_PriceRule("tsh4", 80),
			new CTR_PriceRule("/headgear_", 40),
			new CTR_PriceRule("helmet_opscore", 1900),
			new CTR_PriceRule("helmet_caiman", 2000),
			new CTR_PriceRule("helmet_spartan", 1800),
			new CTR_PriceRule("helmet_exfil", 1550),
			new CTR_PriceRule("helmet_triada", 1500),
			new CTR_PriceRule("helmet_sph4", 1200),
			new CTR_PriceRule("helmet_dh132", 1200),
			new CTR_PriceRule("helmet_zsh", 1000),
			new CTR_PriceRule("helmet_tor2", 1000),
			new CTR_PriceRule("helmet_ech", 900),
			new CTR_PriceRule("helmet_tor", 800),
			new CTR_PriceRule("helmet_lshz", 700),
			new CTR_PriceRule("helmet_kiver", 600),
			new CTR_PriceRule("helmet_cvc", 600),
			new CTR_PriceRule("helmet_mich_ach", 500),
			new CTR_PriceRule("helmet_tbh3a", 500),
			new CTR_PriceRule("helmet_lwh", 400),
			new CTR_PriceRule("helmet_6b47", 300),
			new CTR_PriceRule("helmet_6b7", 180),
			new CTR_PriceRule("helmet_pasgt", 150),
			new CTR_PriceRule("helmet_m1_", 120),
			new CTR_PriceRule("helmet_ssh68", 60)
		};
		rules.Insert(CATEGORY_HELMETS, rulesHelmets);

		array<ref CTR_PriceRule> rulesHeadgear = {
			new CTR_PriceRule("headphones_peltor", 80),
			new CTR_PriceRule("sordin", 60),
			new CTR_PriceRule("headphones_6m2", 40),
			new CTR_PriceRule("headphones_impact", 70),
			new CTR_PriceRule("eyewear_6b50", 150),
			new CTR_PriceRule("mask_6b49", 150),
			new CTR_PriceRule("crossbow", 120),
			new CTR_PriceRule("npp_kondor", 100),
			new CTR_PriceRule("npp_strelok", 100),
			new CTR_PriceRule("gascan", 60),
			new CTR_PriceRule("balaclava", 25),
			new CTR_PriceRule("hat_", 25)
		};
		rules.Insert(CATEGORY_HEADGEAR, rulesHeadgear);

		array<ref CTR_PriceRule> rulesTops = {
			new CTR_PriceRule("shirt_cp_g3", 230),
			new CTR_PriceRule("crye_shirt", 230),
			new CTR_PriceRule("suit_pilot", 400),
			new CTR_PriceRule("suit_tanker", 300),
			new CTR_PriceRule("jacket_suit", 300),
			new CTR_PriceRule("parka", 200),
			new CTR_PriceRule("ion_shirt", 150),
			new CTR_PriceRule("vkpo", 120),
			new CTR_PriceRule("suit_klmk", 80),
			new CTR_PriceRule("jacket_", 80)
		};
		rules.Insert(CATEGORY_TOPS, rulesTops);

		array<ref CTR_PriceRule> rulesPants = {
			new CTR_PriceRule("pants_cp_g3", 320),
			new CTR_PriceRule("pants_suit", 150),
			new CTR_PriceRule("vkpo", 100)
		};
		rules.Insert(CATEGORY_PANTS, rulesPants);

		array<ref CTR_PriceRule> rulesBootsGloves = {
			new CTR_PriceRule("salomon", 250),
			new CTR_PriceRule("solomon", 250),
			new CTR_PriceRule("faradey", 200),
			new CTR_PriceRule("rocky", 190),
			new CTR_PriceRule("usmc_combat", 180),
			new CTR_PriceRule("altama", 140),
			new CTR_PriceRule("gloves_pilot", 70),
			new CTR_PriceRule("gloves_leather", 40),
			new CTR_PriceRule("gloves_mechanix", 35),
			new CTR_PriceRule("gloves_wool", 15)
		};
		rules.Insert(CATEGORY_BOOTS_GLOVES, rulesBootsGloves);

		// Armored carriers are priced from their supply cost (carrier and plates); old soft armor and rigs are cheap.
		array<ref CTR_PriceRule> rulesVests = {
			new CTR_PriceRule("esapi", 800),
			new CTR_PriceRule("granit", 300),
			new CTR_PriceRule("taktika_br4", 250),
			new CTR_PriceRule("vest_6b3", 300),
			new CTR_PriceRule("vest_pasgt", 300),
			new CTR_PriceRule("vest_6b2", 200),
			new CTR_PriceRule("vest_m69", 150),
			new CTR_PriceRule("vest_alice", 50),
			new CTR_PriceRule("vest_sovietharness", 40),
			new CTR_PriceRule("vest_lifchik", 40),
			new CTR_PriceRule("vest_naz", 40),
			new CTR_PriceRule("vest_type56", 30)
		};
		rules.Insert(CATEGORY_VESTS, rulesVests);

		array<ref CTR_PriceRule> rulesBackpacks = {
			new CTR_PriceRule("filbe", 200),
			new CTR_PriceRule("wartech", 150),
			new CTR_PriceRule("iifs", 150),
			new CTR_PriceRule("medical", 150),
			new CTR_PriceRule("rush12", 140),
			new CTR_PriceRule("ratnik", 120),
			new CTR_PriceRule("alice", 80),
			new CTR_PriceRule("hydration", 80),
			new CTR_PriceRule("kolobok", 60),
			new CTR_PriceRule("rpg_", 60),
			new CTR_PriceRule("m70_swiss", 40),
			new CTR_PriceRule("veshmeshok", 30),
			new CTR_PriceRule("suharka", 30),
			new CTR_PriceRule("touristseat", 15)
		};
		rules.Insert(CATEGORY_BACKPACKS, rulesBackpacks);

		// Radios only carry voice, so game use prices them, not their real cost: handhelds by transmitting range (1.3 to
		// 4 km), backpack radios (2 km, deployable as a respawn point) alike. RHS radio batteries run down.
		array<ref CTR_PriceRule> rulesRadios = {
			new CTR_PriceRule("_battery", 30),
			new CTR_PriceRule("attachment_", 100),
			new CTR_PriceRule("filbe_backpack_radio", 1200),
			new CTR_PriceRule("radio_rf10", 1000),
			new CTR_PriceRule("r107m", 800),
			new CTR_PriceRule("anprc77", 800),
			new CTR_PriceRule("r187p1", 600),
			new CTR_PriceRule("anprc152", 500),
			new CTR_PriceRule("radio_r148", 250),
			new CTR_PriceRule("anprc68", 200)
		};
		rules.Insert(CATEGORY_RADIOS, rulesRadios);

		array<ref CTR_PriceRule> rulesVision = {
			new CTR_PriceRule("vector21", 2000),
			new CTR_PriceRule("pvs_31_batpack", 300),
			new CTR_PriceRule("nvg_pvs31", 5000),
			new CTR_PriceRule("nvg_pvs14dual", 4000),
			new CTR_PriceRule("nvg_pvs14", 3000),
			new CTR_PriceRule("1pn138", 3000),
			new CTR_PriceRule("thermal_patrolir", 4000),
			new CTR_PriceRule("geo-onv1", 2500),
			new CTR_PriceRule("pdu4", 2000),
			new CTR_PriceRule("binoculars_m22", 800),
			new CTR_PriceRule("binoculars_b12", 400),
			new CTR_PriceRule("binoculars_b8", 200),
			new CTR_PriceRule("helstar", 200),
			new CTR_PriceRule("ms2000", 150),
			new CTR_PriceRule("chargepro", 150),
			new CTR_PriceRule("viplight", 100),
			new CTR_PriceRule("switch", 100),
			new CTR_PriceRule("vlite", 80)
		};
		rules.Insert(CATEGORY_VISION, rulesVision);

		array<ref CTR_PriceRule> rulesNavigation = {
			new CTR_PriceRule("dagr", 500),
			new CTR_PriceRule("orion", 400),
			new CTR_PriceRule("garmintactix", 200),
			new CTR_PriceRule("compass", 120),
			new CTR_PriceRule("watch_", 150),
			new CTR_PriceRule("map_", 15)
		};
		rules.Insert(CATEGORY_NAVIGATION, rulesNavigation);

		array<ref CTR_PriceRule> rulesTools = {
			new CTR_PriceRule("spectrumdevice", 500),
			new CTR_PriceRule("rearmingkit", 1500),
			new CTR_PriceRule("repairkit", 1000),
			new CTR_PriceRule("barbed_tape", 150),
			new CTR_PriceRule("jerrycan", 80),
			new CTR_PriceRule("flashlight", 80),
			new CTR_PriceRule("sandbag", 50),
			new CTR_PriceRule("etool", 50),
			new CTR_PriceRule("barbedtape_stake", 20),
			new CTR_PriceRule("mineflag", 5)
		};
		rules.Insert(CATEGORY_TOOLS, rulesTools);

		array<ref CTR_PriceRule> rulesAccessories = {
			new CTR_PriceRule("gpsradiobeacon", 300),
			new CTR_PriceRule("emptymount", 150),
			new CTR_PriceRule("patch", 15)
		};
		rules.Insert(CATEGORY_ACCESSORIES, rulesAccessories);

		return rules;
	}

	//------------------------------------------------------------------------------------------------
	//! \param path Lowercase prefab path.
	protected static bool IsAmmunition(string path, SCR_EArsenalItemMode mode)
	{
		return mode == SCR_EArsenalItemMode.AMMUNITION || path.Contains("/weapons/magazines/") || path.Contains("/weapons/ammo/");
	}
}

//------------------------------------------------------------------------------------------------
//! Reference price of the shop items whose lowercase prefab path contains a keyword.
class CTR_PriceRule
{
	string m_sKeyword;
	int m_iPrice;
	//! Supply cost of the bare weapon; 0 for a fixed price.
	int m_iBaseCost;

	//------------------------------------------------------------------------------------------------
	void CTR_PriceRule(string keyword, int price, int baseCost = 0)
	{
		m_sKeyword = keyword;
		m_iPrice = price;
		m_iBaseCost = baseCost;
	}

	//------------------------------------------------------------------------------------------------
	//! Variants with mounted attachments have a higher supply cost than the bare weapon; each point adds a share of
	//! the attachments' price.
	int GetPrice(int supplyCost)
	{
		if (m_iBaseCost > 0 && supplyCost > m_iBaseCost)
			return m_iPrice + (supplyCost - m_iBaseCost) * CTR_ShopPricing.VARIANT_PRICE_PER_SUPPLY;

		return m_iPrice;
	}
}
