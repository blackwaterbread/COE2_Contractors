//! Shops and prices of the Contractors base, derived from the arsenal data of the vanilla and RHS item catalogs.
//! Three shops by kind of item (weapons, clothing and gear, supplies), each split into finer categories by arsenal
//! type and prefab folder. Price: a base price per kind of item plus a factor of its arsenal supply cost (supply
//! costs rank items within a kind, e.g. M16A2 8, M249 40). Scale: a basic rifle (M16A2) costs about one operation's
//! pay; see the plan for the reasoning.
class CTR_ShopPricing
{
	static const string SHOP_WEAPONS = "contractors_weapons";
	static const string SHOP_GEAR = "contractors_gear";
	static const string SHOP_SUPPLIES = "contractors_supplies";

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

	// Clothing and gear shop
	static const string CATEGORY_HELMETS = "Helmets";
	static const string CATEGORY_HEADGEAR = "Headgear";
	static const string CATEGORY_TOPS = "Shirts & jackets";
	static const string CATEGORY_PANTS = "Pants";
	static const string CATEGORY_BOOTS_GLOVES = "Boots & gloves";
	static const string CATEGORY_VESTS = "Vests & armor";
	static const string CATEGORY_BACKPACKS = "Backpacks";

	// Supplies shop
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
	//! \return Price in cash, 0 when the item is not sold.
	static int GetPrice(ResourceName prefab, SCR_EArsenalItemType type, SCR_EArsenalItemMode mode, int supplyCost)
	{
		string category = GetCategory(prefab, type, mode);
		if (category.IsEmpty())
			return 0;

		string path = prefab;
		path.ToLower();
		bool ammunition = IsAmmunition(path, mode);
		supplyCost = Math.Max(GetMinSupplyCost(category, ammunition), supplyCost);
		if (ammunition)
			return 5 + 5 * supplyCost;

		if (category == CATEGORY_PISTOLS)
			return 100 + 30 * supplyCost;

		array<string> weapons = {CATEGORY_RIFLES, CATEGORY_SNIPER_RIFLES, CATEGORY_MACHINE_GUNS, CATEGORY_LAUNCHERS};
		if (weapons.Contains(category))
			return 200 + 40 * supplyCost;

		array<string> attachments = {CATEGORY_OPTICS, CATEGORY_MUZZLE, CATEGORY_ATTACHMENTS};
		if (attachments.Contains(category))
			return 10 + 3 * supplyCost;

		if (category == CATEGORY_EXPLOSIVES)
			return 10 + 7 * supplyCost;

		if (category == CATEGORY_MEDICAL)
			return 10 + 10 * supplyCost;

		if (category == CATEGORY_VESTS || category == CATEGORY_BACKPACKS)
			return 20 + 5 * supplyCost;

		array<string> clothing = {CATEGORY_HELMETS, CATEGORY_HEADGEAR, CATEGORY_TOPS, CATEGORY_PANTS, CATEGORY_BOOTS_GLOVES};
		if (clothing.Contains(category))
			return 10 + 5 * supplyCost;

		// Heavy weapon parts, radios, binoculars and night vision, navigation, tools, accessories.
		return 15 + 5 * supplyCost;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Shop that sells a category, empty for none.
	static string GetShop(string category)
	{
		array<string> weapons = {CATEGORY_RIFLES, CATEGORY_SNIPER_RIFLES, CATEGORY_MACHINE_GUNS, CATEGORY_PISTOLS, CATEGORY_LAUNCHERS, CATEGORY_AMMUNITION, CATEGORY_OPTICS, CATEGORY_MUZZLE, CATEGORY_ATTACHMENTS, CATEGORY_EXPLOSIVES, CATEGORY_HEAVY_WEAPONS};
		if (weapons.Contains(category))
			return SHOP_WEAPONS;

		array<string> gear = {CATEGORY_HELMETS, CATEGORY_HEADGEAR, CATEGORY_TOPS, CATEGORY_PANTS, CATEGORY_BOOTS_GLOVES, CATEGORY_VESTS, CATEGORY_BACKPACKS};
		if (gear.Contains(category))
			return SHOP_GEAR;

		array<string> supplies = {CATEGORY_MEDICAL, CATEGORY_RADIOS, CATEGORY_VISION, CATEGORY_NAVIGATION, CATEGORY_TOOLS, CATEGORY_ACCESSORIES};
		if (supplies.Contains(category))
			return SHOP_SUPPLIES;

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
	//! RHS gives some weapon variants a supply cost of 1 (the arsenal adds their attachments at runtime), which would
	//! sell a scoped SVD below a bare one. Weapons cost at least the cheapest regular weapon of their kind.
	protected static int GetMinSupplyCost(string category, bool ammunition)
	{
		if (ammunition)
			return 0;

		if (category == CATEGORY_RIFLES)
			return 8;

		if (category == CATEGORY_SNIPER_RIFLES)
			return 20;

		if (category == CATEGORY_MACHINE_GUNS)
			return 25;

		if (category == CATEGORY_PISTOLS)
			return 5;

		if (category == CATEGORY_LAUNCHERS)
			return 30;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! \param path Lowercase prefab path.
	protected static bool IsAmmunition(string path, SCR_EArsenalItemMode mode)
	{
		return mode == SCR_EArsenalItemMode.AMMUNITION || path.Contains("/weapons/magazines/") || path.Contains("/weapons/ammo/");
	}
}
