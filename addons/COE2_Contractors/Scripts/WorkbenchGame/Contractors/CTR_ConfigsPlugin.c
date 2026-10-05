// Development tool: creates the Contractors persistence, Marx settings and systems configs and the mission headers.
// Object IDs come from Workbench.GenerateGloballyUniqueID64(); resource GUIDs from resource registration.
// Existing files are never overwritten.

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

	static const int STARTING_CASH = 150;

	//! Keeps created container resources alive until the plugin finishes.
	protected ref array<ref Resource> m_aHolders = {};

	//------------------------------------------------------------------------------------------------
	override void Run()
	{
		Print(TAG + "start");
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
		Log(mapName + " m_sDescription", root.Set("m_sDescription", "Unofficial COE2 variant: completed operations pay, gear comes from the base shop, and a personal stash keeps it."));

		string fileName = FilePath.StripExtension(FilePath.StripPath(parent));
		SaveAndRegister(root, "$COE2_Contractors:Missions/CTR_" + fileName + ".conf");
	}

	//------------------------------------------------------------------------------------------------
	protected ResourceName SaveAndRegister(BaseContainer root, string file)
	{
		string absPath;
		Workbench.GetAbsolutePath(file, absPath, false);
		if (FileIO.FileExists(absPath))
		{
			Print(TAG + "already exists, not overwritten: " + absPath, LogLevel.WARNING);
			return GetResourceName(absPath);
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
