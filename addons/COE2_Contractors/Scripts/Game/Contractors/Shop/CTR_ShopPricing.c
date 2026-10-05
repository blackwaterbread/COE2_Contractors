//! Prices of the Contractors shop, derived from the vanilla arsenal data of every faction: a base price per kind of
//! item plus a factor of its arsenal supply cost (supply costs rank items within a kind, e.g. M16A2 8, M249 40).
//! Scale: a basic rifle (M16A2) costs about one operation's pay; see the plan for the reasoning.
class CTR_ShopPricing
{
	static const string CATEGORY_WEAPONS = "Weapons";
	static const string CATEGORY_AMMUNITION = "Ammunition";
	static const string CATEGORY_ATTACHMENTS = "Attachments";
	static const string CATEGORY_EXPLOSIVES = "Explosives";
	static const string CATEGORY_MEDICAL = "Medical";
	static const string CATEGORY_GEAR = "Backpacks and vests";
	static const string CATEGORY_CLOTHING = "Clothing";
	static const string CATEGORY_EQUIPMENT = "Equipment";

	protected static const int WEAPON_TYPES = SCR_EArsenalItemType.RIFLE | SCR_EArsenalItemType.PISTOL | SCR_EArsenalItemType.SNIPER_RIFLE | SCR_EArsenalItemType.MACHINE_GUN | SCR_EArsenalItemType.ROCKET_LAUNCHER;
	protected static const int EXPLOSIVE_TYPES = SCR_EArsenalItemType.LETHAL_THROWABLE | SCR_EArsenalItemType.NON_LETHAL_THROWABLE | SCR_EArsenalItemType.EXPLOSIVES;
	protected static const int GEAR_TYPES = SCR_EArsenalItemType.BACKPACK | SCR_EArsenalItemType.RADIO_BACKPACK | SCR_EArsenalItemType.VEST_AND_WAIST;
	protected static const int CLOTHING_TYPES = SCR_EArsenalItemType.HEADWEAR | SCR_EArsenalItemType.TORSO | SCR_EArsenalItemType.LEGS | SCR_EArsenalItemType.FOOTWEAR | SCR_EArsenalItemType.HANDWEAR;
	//! Vehicle and helicopter parts and ammunition are not personal gear.
	protected static const int EXCLUDED_TYPES = SCR_EArsenalItemType.HELICOPTER | SCR_EArsenalItemType.VEHICLE;

	//------------------------------------------------------------------------------------------------
	//! \return Shop category, or empty when the item is not sold.
	static string GetCategory(SCR_EArsenalItemType type, SCR_EArsenalItemMode mode)
	{
		if ((type & EXCLUDED_TYPES) || mode == SCR_EArsenalItemMode.PYLON)
			return string.Empty;

		if (mode == SCR_EArsenalItemMode.AMMUNITION)
			return CATEGORY_AMMUNITION;

		if (type & WEAPON_TYPES)
		{
			if (mode == SCR_EArsenalItemMode.WEAPON || mode == SCR_EArsenalItemMode.WEAPON_VARIANTS)
				return CATEGORY_WEAPONS;

			// Entries without arsenal data default to a rifle in DEFAULT mode: underbarrel launchers and the like.
			return CATEGORY_ATTACHMENTS;
		}

		if (type == SCR_EArsenalItemType.WEAPON_ATTACHMENT || mode == SCR_EArsenalItemMode.ATTACHMENT)
			return CATEGORY_ATTACHMENTS;

		if (type & EXPLOSIVE_TYPES)
			return CATEGORY_EXPLOSIVES;

		if (type == SCR_EArsenalItemType.HEAL)
			return CATEGORY_MEDICAL;

		if (type & GEAR_TYPES)
			return CATEGORY_GEAR;

		if (type & CLOTHING_TYPES)
			return CATEGORY_CLOTHING;

		return CATEGORY_EQUIPMENT;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Price in cash, 0 when the item is not sold.
	static int GetPrice(SCR_EArsenalItemType type, SCR_EArsenalItemMode mode, int supplyCost)
	{
		string category = GetCategory(type, mode);
		supplyCost = Math.Max(0, supplyCost);
		if (category == CATEGORY_WEAPONS)
		{
			if (type == SCR_EArsenalItemType.PISTOL)
				return 100 + 30 * supplyCost;

			return 200 + 40 * supplyCost;
		}

		if (category == CATEGORY_AMMUNITION)
			return 5 + 5 * supplyCost;

		if (category == CATEGORY_ATTACHMENTS)
			return 10 + 3 * supplyCost;

		if (category == CATEGORY_EXPLOSIVES)
			return 10 + 7 * supplyCost;

		if (category == CATEGORY_MEDICAL)
			return 10 + 10 * supplyCost;

		if (category == CATEGORY_GEAR)
			return 20 + 5 * supplyCost;

		if (category == CATEGORY_CLOTHING)
			return 10 + 5 * supplyCost;

		if (category == CATEGORY_EQUIPMENT)
			return 15 + 5 * supplyCost;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Display order of the categories in the shop.
	static int GetCategoryOrder(string category)
	{
		array<string> order = {CATEGORY_WEAPONS, CATEGORY_AMMUNITION, CATEGORY_ATTACHMENTS, CATEGORY_EXPLOSIVES, CATEGORY_MEDICAL, CATEGORY_GEAR, CATEGORY_CLOTHING, CATEGORY_EQUIPMENT};
		return order.Find(category);
	}
}
