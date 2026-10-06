// Development tool: creates the Contractors persistence, Marx settings and systems configs, the mission headers, the
// pay settings and the shop catalogs.
// Object IDs come from Workbench.GenerateGloballyUniqueID64(); resource GUIDs from resource registration.
// Existing files are never overwritten, except the generated shop catalogs.

[WorkbenchPluginAttribute(name: "Create Contractors Configs", category: "Contractors", wbModules: { "WorldEditor" })]
class CTR_ConfigsPlugin : WorldEditorPlugin
{
	static const string TAG = "[CTR_PLUGIN] ";

	//! Vanilla persistence base with only the "Main" database, its session storage and the "System" collection: no world state.
	static const ResourceName PARENT_PERSISTENCE = "{BEFFBD9A87620F23}Configs/Systems/Persistence/BaseSetup.conf";
	//! Vanilla mission systems: BaseGameModeSystems (= ChimeraSystemsConfig with the COE2 and Marx entries) plus the persistence system.
	static const ResourceName PARENT_SYSTEMS = "{C50579A2EC48E8EF}Configs/Systems/MissionSystems.conf";
	static const ResourceName PARENT_MARX_SETTINGS = "{A520D015366C7F35}Configs/Marx/MRX_Settings.conf";
	//! "Main" save game database declared in vanilla BaseSetup.conf.
	static const string MAIN_DATABASE_ID = "{6624ADA9A88DA0F6}";
	//! SCR_PersistenceSystem entry declared in vanilla MissionSystems.conf.
	static const string PARENT_PERSISTENCE_SYSTEM_ID = "{65DC893A5A752E89}";
	//! MRX_MarxSystem entry declared in Marx_Core's ChimeraSystemsConfig.conf override.
	static const string MARX_SYSTEM_ID = "{6A88FD9099EB0B30}";

	static const string PERSISTENCE_FILE = "$COE2_Contractors:Configs/Contractors/Persistence/CTR_Persistence.conf";
	static const string MARX_SETTINGS_FILE = "$COE2_Contractors:Configs/Contractors/CTR_MarxSettings.conf";
	static const string SYSTEMS_FILE = "$COE2_Contractors:Configs/Contractors/Systems/CTR_Systems.conf";
	static const string PAY_SETTINGS_FILE = "$COE2_Contractors:Configs/Contractors/CTR_Settings.conf";
	static const string SHOP_DIR = "$COE2_Contractors:Configs/Contractors/Shop/";
	//! Default contents of the shop items (MRX_ShopContentsCheck report written by the test CTR_Test_ShopContents),
	//! relative to the addon directory: kept in the repo's tools folder, outside the packed addon.
	static const string DEFAULT_CONTENTS_FILE = "../../tools/shop-default-contents.csv";
	//! Columns of the report: prefab and the semicolon-separated content prefabs.
	protected static const int CONTENTS_COLUMN_PREFAB = 1;
	protected static const int CONTENTS_COLUMN_CONTENTS = 11;

	static const int STARTING_CASH = 5000;
	//! A full stash of 8 pages holds up to 384 one-cell items.
	static const int MAX_STASH_ASSETS = 400;

	//! Keeps created container resources alive until the plugin finishes.
	protected ref array<ref Resource> m_aHolders = {};
	protected ref map<string, int> m_mCategoryOrder = new map<string, int>();

	//------------------------------------------------------------------------------------------------
	override void Run()
	{
		Print(TAG + "start");
		CreatePaySettings();
		CreateShopCatalogs();
		CreateServicesCatalog();
		ResourceName persistence = CreatePersistenceConfig();
		ResourceName settings = CreateMarxSettings();
		if (persistence.IsEmpty() || settings.IsEmpty())
			return;

		ResourceName systems = CreateSystemsConfig(persistence, settings);
		if (systems.IsEmpty())
			return;

		CreateMissionHeader("{F239FD0036BD2C1E}Missions/COE2_Arland.conf", "Arland", systems);
		CreateMissionHeader("{4D4780DB775BFF64}Missions/COE2_Cain.conf", "Kolguyev", systems);
		CreateMissionHeader("{EE676FAB9DFA4CF7}Missions/COE2_Eden.conf", "Everon", systems);
		Print(TAG + "done");
	}

	//------------------------------------------------------------------------------------------------
	//! CTR_Settings with the built-in defaults; CTR_Settings.CONFIG must then be set to the printed resource name.
	protected ResourceName CreatePaySettings()
	{
		Resource holder = BaseContainerTools.CreateContainerFromInstance(CTR_Settings.CreateDefault());
		if (!holder || !holder.IsValid())
		{
			Print(TAG + "CreateContainerFromInstance failed: CTR_Settings", LogLevel.ERROR);
			return ResourceName.Empty;
		}

		m_aHolders.Insert(holder);
		return SaveAndRegister(holder.GetResource().ToBaseContainer(), PAY_SETTINGS_FILE);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Catalog file of a Contractors shop (CTR_ShopPricing.SHOP_*).
	static string GetShopCatalogFile(string shopId)
	{
		if (shopId == CTR_ShopPricing.SHOP_WEAPONS)
			return SHOP_DIR + "CTR_ShopWeapons.conf";

		if (shopId == CTR_ShopPricing.SHOP_SERVICES)
			return SHOP_DIR + "CTR_ShopServices.conf";

		return SHOP_DIR + "CTR_ShopEquipment.conf";
	}

	//------------------------------------------------------------------------------------------------
	//! Catalog of the quartermaster: stash pages for $1,000,000 each, up to 8 pages. Not rewritten once it exists.
	protected ResourceName CreateServicesCatalog()
	{
		CTR_StashPageProduct product = new CTR_StashPageProduct();
		product.m_iMaxPages = 8;
		MRX_ShopItem item = MRX_ShopItem.Create("stash_page", "{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et", 1000000, CTR_Settings.Get().m_sCurrency, 0);
		item.m_sName = "Stash Expansion";
		item.m_sCategory = "Stash";
		item.m_sDescription = "One more page of 6 x 8 cells in your stash, up to 8 pages";
		item.m_Product = product;

		MRX_ShopCatalog catalog = new MRX_ShopCatalog();
		catalog.m_aItems = {item};
		Resource holder = BaseContainerTools.CreateContainerFromInstance(catalog);
		if (!holder || !holder.IsValid())
		{
			Print(TAG + "CreateContainerFromInstance failed: services catalog", LogLevel.ERROR);
			return ResourceName.Empty;
		}

		m_aHolders.Insert(holder);
		return SaveAndRegister(holder.GetResource().ToBaseContainer(), GetShopCatalogFile(CTR_ShopPricing.SHOP_SERVICES));
	}

	//------------------------------------------------------------------------------------------------
	//! One catalog per shop with every item that has arsenal data in the vanilla and RHS faction item catalogs, priced
	//! and sorted into categories by CTR_ShopPricing. Prices cover the default contents (DEFAULT_CONTENTS_FILE), priced
	//! from the catalogs of both shops. Generated catalogs are rewritten in place (same resource GUID).
	protected void CreateShopCatalogs()
	{
		array<ResourceName> sources = {
			"{5F7EC52FC40A03E2}Configs/EntityCatalog/US/InventoryItems_EntityCatalog_US.conf",
			"{C53421647C3D0D2E}Configs/EntityCatalog/USSR/InventoryItems_EntityCatalog_USSR.conf",
			"{E908001749419691}Configs/EntityCatalog/FIA/InventoryItems_EntityCatalog_FIA.conf",
			"{9D7E5804BB2E9B28}Configs/EntityCatalog/CIV/InventoryItems_EntityCatalog_CIV.conf",
			"{3BFC57A750823093}Configs/EntityCatalog/USMC/USMC_InventoryItems.conf",
			"{4AB9A3D2B81A7855}Configs/EntityCatalog/RHS_MSV/MSV_InventoryItems.conf",
			"{07394ECD0D03534C}Configs/EntityCatalog/ION/ION_InventoryItems.conf",
			"{1FFB8A9964E1F4E4}Configs/EntityCatalog/FFA/FFA_InventoryItems.conf"
		};

		array<ref MRX_ShopItem> items = {};
		array<ref CTR_ShopItemSource> itemSources = {};
		array<string> prefabKeys = {};
		array<string> ids = {};
		foreach (ResourceName source : sources)
		{
			SCR_EntityCatalogMultiList catalog = SCR_ConfigHelperT<SCR_EntityCatalogMultiList>.GetConfigObject(source);
			if (!catalog)
			{
				Print(TAG + "cannot load " + source, LogLevel.ERROR);
				continue;
			}

			array<SCR_EntityCatalogMultiListEntry> lists = {};
			catalog.GetMultiList(lists);
			foreach (SCR_EntityCatalogMultiListEntry list : lists)
			{
				if (!list.m_aEntities)
					continue;

				foreach (SCR_EntityCatalogEntry entry : list.m_aEntities)
				{
					CTR_ShopItemSource itemSource = CreateShopItem(entry, prefabKeys, ids);
					if (!itemSource)
						continue;

					items.Insert(itemSource.m_Item);
					itemSources.Insert(itemSource);
				}
			}
		}

		AddDefaultContents(itemSources);

		array<string> shops = {CTR_ShopPricing.SHOP_WEAPONS, CTR_ShopPricing.SHOP_EQUIPMENT};
		foreach (string shop : shops)
		{
			array<ref MRX_ShopItem> shopItems = {};
			foreach (MRX_ShopItem item : items)
			{
				if (CTR_ShopPricing.GetShop(item.m_sCategory) == shop)
					shopItems.Insert(item);
			}

			MRX_ShopCatalog shopCatalog = new MRX_ShopCatalog();
			shopCatalog.m_aItems = SortShopItems(shopItems);
			Print(TAG + string.Format("%1 catalog: %2 items", shop, shopCatalog.m_aItems.Count()));

			Resource holder = BaseContainerTools.CreateContainerFromInstance(shopCatalog);
			if (!holder || !holder.IsValid())
			{
				Print(TAG + "CreateContainerFromInstance failed: MRX_ShopCatalog " + shop, LogLevel.ERROR);
				continue;
			}

			m_aHolders.Insert(holder);
			SaveAndRegister(holder.GetResource().ToBaseContainer(), GetShopCatalogFile(shop), true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Raises the prices of the items that come with other items (DEFAULT_CONTENTS_FILE) so that they cover them. The
	//! contents count at their price without contents, so contents of contents count once.
	protected void AddDefaultContents(notnull array<ref CTR_ShopItemSource> itemSources)
	{
		map<string, ref array<ResourceName>> contents = LoadDefaultContents();
		if (!contents)
			return;

		map<string, int> prices = new map<string, int>();
		foreach (CTR_ShopItemSource itemSource : itemSources)
		{
			prices.Insert(MRX_ShopCatalog.GetPrefabKey(itemSource.m_Item.m_sPrefab), itemSource.m_Item.m_iPrice);
		}

		int raised;
		foreach (CTR_ShopItemSource itemSource : itemSources)
		{
			MRX_ShopItem item = itemSource.m_Item;
			array<ResourceName> itemContents = contents.Get(MRX_ShopCatalog.GetPrefabKey(item.m_sPrefab));
			if (!itemContents)
				continue;

			int contentsPrice;
			foreach (ResourceName content : itemContents)
			{
				contentsPrice += prices.Get(MRX_ShopCatalog.GetPrefabKey(content));
			}

			int price = CTR_ShopPricing.GetPrice(item.m_sPrefab, itemSource.m_eType, itemSource.m_eMode, itemSource.m_iSupplyCost, contentsPrice);
			if (price > item.m_iPrice)
				raised++;

			item.m_iPrice = price;
		}

		Print(TAG + string.Format("default contents: %1 items listed, %2 prices raised", contents.Count(), raised));
	}

	//------------------------------------------------------------------------------------------------
	//! \return Content prefabs by prefab key (MRX_ShopCatalog.GetPrefabKey) of the items that have contents, or null
	//! when the file is missing.
	protected map<string, ref array<ResourceName>> LoadDefaultContents()
	{
		string gproj;
		Workbench.GetAbsolutePath("$COE2_Contractors:addon.gproj", gproj, true);
		string path = FilePath.Concat(FilePath.StripFileName(gproj), DEFAULT_CONTENTS_FILE);
		FileHandle reader = FileIO.OpenFile(path, FileMode.READ);
		if (!reader)
		{
			Print(TAG + "no default contents file " + path + ", prices do not cover default contents", LogLevel.WARNING);
			return null;
		}

		map<string, ref array<ResourceName>> contents = new map<string, ref array<ResourceName>>();
		string line;
		bool header = true;
		while (reader.ReadLine(line) >= 0)
		{
			if (header)
			{
				header = false;
				continue;
			}

			array<string> columns = {};
			line.Split(",", columns, false);
			if (columns.Count() <= CONTENTS_COLUMN_CONTENTS || columns[CONTENTS_COLUMN_CONTENTS].IsEmpty())
				continue;

			array<string> prefabs = {};
			columns[CONTENTS_COLUMN_CONTENTS].Split(";", prefabs, true);
			array<ResourceName> itemContents = {};
			foreach (string prefab : prefabs)
			{
				itemContents.Insert(prefab);
			}

			contents.Set(MRX_ShopCatalog.GetPrefabKey(columns[CONTENTS_COLUMN_PREFAB]), itemContents);
		}

		reader.Close();
		return contents;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Null for disabled entries, items without arsenal data, items not sold, and prefabs already added.
	protected CTR_ShopItemSource CreateShopItem(SCR_EntityCatalogEntry entry, notnull array<string> prefabKeys, notnull array<string> ids)
	{
		if (!entry || !entry.IsEnabled())
			return null;

		SCR_ArsenalItem arsenal = SCR_ArsenalItem.Cast(entry.GetEntityDataOfType(SCR_ArsenalItem));
		if (!arsenal)
			return null;

		ResourceName prefab = entry.GetPrefab();
		string key = MRX_ShopCatalog.GetPrefabKey(prefab);
		if (prefab.IsEmpty() || prefabKeys.Contains(key))
			return null;

		SCR_EArsenalItemType type = arsenal.GetItemType();
		SCR_EArsenalItemMode mode = arsenal.GetItemMode();
		int supplyCost = arsenal.GetSupplyCost(SCR_EArsenalSupplyCostType.DEFAULT, false);
		int price = CTR_ShopPricing.GetPrice(prefab, type, mode, supplyCost);
		if (price <= 0)
			return null;

		prefabKeys.Insert(key);
		string id = FilePath.StripExtension(FilePath.StripPath(prefab));
		id.ToLower();
		if (ids.Contains(id))
			id = id + "_" + key.Substring(1, 8);

		ids.Insert(id);

		MRX_ShopItem item = new MRX_ShopItem();
		item.m_sId = id;
		item.m_sPrefab = prefab;
		item.m_sCategory = CTR_ShopPricing.GetCategory(prefab, type, mode);
		item.m_sCurrency = MRX_Settings.DEFAULT_CURRENCY;
		item.m_iPrice = price;
		item.m_iSellPrice = -1;
		item.m_iStock = -1;

		CTR_ShopItemSource itemSource = new CTR_ShopItemSource();
		itemSource.m_Item = item;
		itemSource.m_eType = type;
		itemSource.m_eMode = mode;
		itemSource.m_iSupplyCost = supplyCost;
		return itemSource;
	}

	//------------------------------------------------------------------------------------------------
	//! By category, then ID, so the variants of a weapon or a piece of clothing stand together. Items move into the
	//! sorted array before leaving the source, so they stay referenced.
	protected array<ref MRX_ShopItem> SortShopItems(notnull array<ref MRX_ShopItem> items)
	{
		array<ref MRX_ShopItem> sorted = {};
		while (!items.IsEmpty())
		{
			int first;
			for (int i = 1; i < items.Count(); i++)
			{
				if (CompareShopItems(items[i], items[first]) < 0)
					first = i;
			}

			sorted.Insert(items[first]);
			items.RemoveOrdered(first);
		}

		return sorted;
	}

	//------------------------------------------------------------------------------------------------
	protected int CompareShopItems(MRX_ShopItem a, MRX_ShopItem b)
	{
		if (a.m_sCategory != b.m_sCategory)
			return GetCategoryOrder(a.m_sCategory) - GetCategoryOrder(b.m_sCategory);

		return a.m_sId.Compare(b.m_sId);
	}

	//------------------------------------------------------------------------------------------------
	//! CTR_ShopPricing.GetCategoryOrder, cached: sorting a catalog compares items hundreds of thousands of times.
	protected int GetCategoryOrder(string category)
	{
		int order;
		if (m_mCategoryOrder.Find(category, order))
			return order;

		order = CTR_ShopPricing.GetCategoryOrder(category);
		m_mCategoryOrder.Insert(category, order);
		return order;
	}

	//------------------------------------------------------------------------------------------------
	protected ResourceName CreatePersistenceConfig()
	{
		BaseContainer root = CreateContainer("PersistenceSystemConfig", string.Empty);
		root.SetAncestor(PARENT_PERSISTENCE);

		string storageId = NewId();
		BaseContainer storage = CreateContainer("GamemodeStorage", storageId);
		Log("storage.Database", storage.Set("Database", MAIN_DATABASE_ID));
		Log("Storages", root.SetObjectArray("Storages").Insert(storage));

		BaseContainerList collections = root.SetObjectArray("Collections");
		BaseContainer gameplay = CreateContainer("PersistenceConfigGroup", "Gameplay");
		BaseContainerList states = gameplay.SetObjectArray("Configurations");

		AddStateCollection(collections, states, storageId, MRX_NativeBackend.COLLECTION_NAME, "MRX_WalletStateSerializer");
		AddStateCollection(collections, states, storageId, MRX_NativeBackend.STASH_COLLECTION_NAME, "MRX_StashStateSerializer");

		BaseContainer scriptedStates = CreateContainer("PersistenceConfigGroup", "ScriptedStates");
		Log("ScriptedStates.Configurations", scriptedStates.SetObjectArray("Configurations").Insert(gameplay));
		Log("Configurations", root.SetObjectArray("Configurations").Insert(scriptedStates));

		return SaveAndRegister(root, PERSISTENCE_FILE);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddStateCollection(BaseContainerList collections, BaseContainerList states, string storageId, string name, string serializerClass)
	{
		string collectionId = NewId();
		BaseContainer collection = CreateContainer("PersistenceCollection", collectionId);
		Log(name + ".Name", collection.Set("Name", name));
		Log(name + ".Storage", collection.Set("Storage", storageId));
		Log(name + " collection", collections.Insert(collection));

		BaseContainer stateConfig = CreateContainer("StatePersistenceConfig", NewId());
		Log(name + " state.Collection", stateConfig.Set("Collection", collectionId));
		Log(name + " state.Serializer", stateConfig.SetObject("Serializer", CreateContainer(serializerClass, NewId())));
		Log(name + " state", states.Insert(stateConfig));
	}

	//------------------------------------------------------------------------------------------------
	protected ResourceName CreateMarxSettings()
	{
		BaseContainer root = CreateContainer("MRX_Settings", string.Empty);
		root.SetAncestor(PARENT_MARX_SETTINGS);

		BaseContainer cash = CreateContainer("MRX_CurrencyDef", NewId());
		Log("cash.m_sId", cash.Set("m_sId", MRX_Settings.DEFAULT_CURRENCY));
		Log("cash.m_iInitialBalance", cash.Set("m_iInitialBalance", STARTING_CASH));
		Log("m_aCurrencies", root.SetObjectArray("m_aCurrencies").Insert(cash));
		Log("m_iMaxStashAssets", root.Set("m_iMaxStashAssets", MAX_STASH_ASSETS));

		return SaveAndRegister(root, MARX_SETTINGS_FILE);
	}

	//------------------------------------------------------------------------------------------------
	protected ResourceName CreateSystemsConfig(ResourceName persistence, ResourceName settings)
	{
		BaseContainer root = CreateContainer("SystemSettings", string.Empty);
		root.SetAncestor(PARENT_SYSTEMS);
		BaseContainerList systems = root.SetObjectArray("Systems");

		BaseContainer persistenceSystem = CreateContainer("SCR_PersistenceSystem", PARENT_PERSISTENCE_SYSTEM_ID);
		Log("persistenceSystem.Config", persistenceSystem.Set("Config", persistence));
		Log("Systems persistence", systems.Insert(persistenceSystem));

		BaseContainer marxSystem = CreateContainer("MRX_MarxSystem", MARX_SYSTEM_ID);
		Log("marxSystem.m_sSettingsConfig", marxSystem.Set("m_sSettingsConfig", settings));
		Log("Systems marx", systems.Insert(marxSystem));

		return SaveAndRegister(root, SYSTEMS_FILE);
	}

	//------------------------------------------------------------------------------------------------
	protected void CreateMissionHeader(ResourceName parent, string mapName, ResourceName systems)
	{
		BaseContainer root = CreateContainer("COE_MissionHeader", string.Empty);
		root.SetAncestor(parent);
		Log(mapName + " SystemsConfig", root.Set("SystemsConfig", systems));
		Log(mapName + " m_sName", root.Set("m_sName", "COE2: Contractors - " + mapName));
		Log(mapName + " m_sDescription", root.Set("m_sDescription", "Unofficial COE2 variant: completed operations pay, gear comes from the base shops, and a personal stash keeps it."));
		Log(mapName + " player faction", root.Set("m_sCOE_DefaultPlayerFactionKey", CTR_Factions.PLAYER));
		Log(mapName + " enemy faction", root.Set("m_sCOE_DefaultEnemyFactionKey", CTR_Factions.ENEMY));
		Log(mapName + " civilian faction", root.Set("m_sCOE_DefaultCivilianFactionKey", CTR_Factions.CIVILIAN));

		string fileName = FilePath.StripExtension(FilePath.StripPath(parent));
		SaveAndRegister(root, "$COE2_Contractors:Missions/CTR_" + fileName + ".conf");
	}

	//------------------------------------------------------------------------------------------------
	//! \param overwrite Rewrites an existing file in place; its .meta file and so its resource GUID stay.
	protected ResourceName SaveAndRegister(BaseContainer root, string file, bool overwrite = false)
	{
		string absPath;
		Workbench.GetAbsolutePath(file, absPath, false);
		if (FileIO.FileExists(absPath))
		{
			if (!overwrite)
			{
				Print(TAG + "already exists, not overwritten: " + absPath, LogLevel.WARNING);
				return GetResourceName(absPath);
			}

			if (!BaseContainerTools.SaveContainer(root, ResourceName.Empty, file))
			{
				Print(TAG + "SaveContainer failed: " + file, LogLevel.ERROR);
				return ResourceName.Empty;
			}

			ResourceName rewritten = GetResourceName(absPath);
			Print(TAG + "rewritten " + rewritten);
			return rewritten;
		}

		FileIO.MakeDirectory(FilePath.StripFileName(absPath));
		if (!BaseContainerTools.SaveContainer(root, ResourceName.Empty, file))
		{
			Print(TAG + "SaveContainer failed: " + file, LogLevel.ERROR);
			return ResourceName.Empty;
		}

		ResourceManager resourceManager = Workbench.GetModule(ResourceManager);
		if (!resourceManager.RegisterResourceFile(absPath, false))
			Print(TAG + "RegisterResourceFile failed: " + absPath, LogLevel.ERROR);

		ResourceName resourceName = GetResourceName(absPath);
		Print(TAG + "saved " + resourceName);
		return resourceName;
	}

	//------------------------------------------------------------------------------------------------
	protected ResourceName GetResourceName(string absPath)
	{
		ResourceManager resourceManager = Workbench.GetModule(ResourceManager);
		MetaFile meta = resourceManager.GetMetaFile(absPath);
		if (!meta)
			return ResourceName.Empty;

		return meta.GetResourceID();
	}

	//------------------------------------------------------------------------------------------------
	protected BaseContainer CreateContainer(string className, string name)
	{
		Resource holder = BaseContainerTools.CreateContainer(className);
		if (!holder || !holder.IsValid())
		{
			Print(TAG + "CreateContainer failed: " + className, LogLevel.ERROR);
			return null;
		}

		BaseContainer container = holder.GetResource().ToBaseContainer();
		if (!name.IsEmpty())
			container.SetName(name);

		m_aHolders.Insert(holder);
		return container;
	}

	//------------------------------------------------------------------------------------------------
	protected string NewId()
	{
		string id = Workbench.GenerateGloballyUniqueID64();
		if (!id.StartsWith("{"))
			id = "{" + id + "}";

		return id;
	}

	//------------------------------------------------------------------------------------------------
	protected void Log(string step, bool result)
	{
		if (result)
			Print(TAG + step + " ok");
		else
			Print(TAG + step + " FAILED", LogLevel.ERROR);
	}
}

//------------------------------------------------------------------------------------------------
//! A generated shop item with the arsenal data it was priced from.
class CTR_ShopItemSource
{
	ref MRX_ShopItem m_Item;
	SCR_EArsenalItemType m_eType;
	SCR_EArsenalItemMode m_eMode;
	int m_iSupplyCost;
}
