// Development tool: turns the two FIA arsenal boxes of the COE2 main base prefab used by the COE2 worlds
// (COE_Hideout_01.et) into the Contractors arsenal shops, named and marked like the vanilla weapons and equipment
// arsenal boxes, and puts the stash point and the quartermaster (a shop keeper selling stash pages, prefab created here
// once) next to them. The override file itself is created in Workbench ("Override in" COE2_Contractors); this adds the
// lines by a text edit, with entity and component IDs from Workbench.GenerateGloballyUniqueID64(). Only adds what is
// missing: arsenal shops by shop ID, the stash point and the quartermaster by prefab. Run "Create Contractors Configs"
// first, it creates the shop catalogs.

[WorkbenchPluginAttribute(name: "Add Contractors Base Points", category: "Contractors", wbModules: { "WorldEditor" })]
class CTR_BasePointsPlugin : WorldEditorPlugin
{
	static const string TAG = "[CTR_PLUGIN] ";
	static const string BASE_FILE = "$COE2_Contractors:Prefabs/Compositions/Misc/COE/COE_Hideout_01.et";
	static const ResourceName STASH_PREFAB = "{66BACE8BD545B8C2}Prefabs/Marx/Stash/MRX_StashWardrobe.et";
	static const string QUARTERMASTER_FILE = "$COE2_Contractors:Prefabs/Contractors/CTR_Quartermaster.et";
	static const ResourceName QUARTERMASTER_BASE = "{DE15FB5FAFC3E63F}Prefabs/Characters/Factions/BLUFOR/US_Army/Character_US_Officer.et";
	//! Next to the stash wardrobe, along the same wall, facing into the hall.
	static const string QUARTERMASTER_POSITION = "2.4 0 6.1";
	//! Entity IDs of the FIA arsenal boxes in COE_Hideout_01.et (inherited children of the override).
	static const string WEAPONS_BOX_ID = "60A042236B427BA4";
	static const string EQUIPMENT_BOX_ID = "61288E8539ECF97D";
	//! Entries of the vanilla arsenal boxes the shops change: arsenal, mesh with its decal and MLOD materials
	//! (ArsenalBox_FIA.et), and the storage's display name, which the box and the arsenal window show.
	static const string ARSENAL_COMPONENT_ID = "{56F2C6D1431AD9AF}";
	static const string MESH_COMPONENT_ID = "{56F2C6D1431AD85A}";
	static const string DECAL_MATERIAL_ID = "{6108C5B124553EEA}";
	static const string MLOD_MATERIAL_ID = "{6108C5B124553E0E}";
	static const ResourceName MLOD_MATERIAL = "{54890FD8AFC9D213}Assets/Props/Military/AmmoBox/ArsenalBox_01/Data/ArsenalBox_01_FIA_MLOD.emat";
	static const string STORAGE_COMPONENT_ID = "{56F2C6D15FE6C4CE}";
	static const string STORAGE_ATTRIBUTES_ID = "{56F2C6D6A36680FB}";
	static const string STORAGE_UI_INFO_ID = "{56F2C6D6A229E091}";
	//! Decals of the vanilla FIA weapons and equipment arsenal boxes.
	static const ResourceName WEAPONS_DECAL = "{846039F3F6E61316}Assets/Props/Military/AmmoBox/ArsenalBox_01/Data/ArsenalBox_01_Decal_FIA_weapons.emat";
	static const ResourceName EQUIPMENT_DECAL = "{3A02B3149F575BBC}Assets/Props/Military/AmmoBox/ArsenalBox_01/Data/ArsenalBox_01_Decal_FIA_equip.emat";

	//------------------------------------------------------------------------------------------------
	override void Run()
	{
		string absPath;
		Workbench.GetAbsolutePath(BASE_FILE, absPath, true);
		FileHandle reader = FileIO.OpenFile(absPath, FileMode.READ);
		if (!reader)
		{
			Print(TAG + "cannot read " + absPath + " (create it with Override in COE2_Contractors first)", LogLevel.ERROR);
			return;
		}

		array<string> lines = {};
		string line;
		while (reader.ReadLine(line) >= 0)
		{
			lines.Insert(line);
		}

		reader.Close();

		// Expected: "COE_MainBaseEntity {", " ID ...", optionally the children " {" ... " }", and "}".
		int last = lines.Count() - 1;
		while (last >= 0 && lines[last].Trim().IsEmpty())
		{
			last--;
		}

		if (last < 2 || lines[last].Trim() != "}" || !lines[1].Contains("ID "))
		{
			Print(TAG + "unexpected base override layout, not changed", LogLevel.ERROR);
			return;
		}

		bool hasChildren = lines[2].Trim() == "{";
		if (hasChildren && lines[last - 1].Trim() != "}")
		{
			Print(TAG + "unexpected end of the base override children, not changed", LogLevel.ERROR);
			return;
		}

		string text;
		foreach (string fileLine : lines)
		{
			text += fileLine + "\n";
		}

		array<string> children = {};
		if (!AddArsenalShop(children, text, WEAPONS_BOX_ID, CTR_ShopPricing.SHOP_WEAPONS, "Weapon Shop", WEAPONS_DECAL)
			|| !AddArsenalShop(children, text, EQUIPMENT_BOX_ID, CTR_ShopPricing.SHOP_EQUIPMENT, "Equipment Shop", EQUIPMENT_DECAL))
			return;

		if (!text.Contains(STASH_PREFAB))
		{
			children.Insert("  GenericEntity : \"" + STASH_PREFAB + "\" {");
			children.Insert("   ID \"" + NewEntityId() + "\"");
			children.Insert("   coords -0.4 0 6.6");
			children.Insert("   angles 0 180 0");
			children.Insert("  }");
		}

		ResourceName quartermaster = CreateQuartermasterPrefab();
		if (!quartermaster.IsEmpty() && !text.Contains(SCR_ResourceNameUtils.GetPrefabGUID(quartermaster)))
		{
			children.Insert("  SCR_ChimeraCharacter : \"" + quartermaster + "\" {");
			children.Insert("   ID \"" + NewEntityId() + "\"");
			children.Insert("   coords " + QUARTERMASTER_POSITION);
			children.Insert("   angles 0 180 0");
			children.Insert("  }");
		}

		if (children.IsEmpty())
		{
			Print(TAG + "base override already has the arsenal shops, the stash point and the quartermaster, not changed");
			return;
		}

		int insertAt = last - 1;
		if (!hasChildren)
		{
			children.InsertAt(" {", 0);
			children.Insert(" }");
			insertAt = last;
		}

		for (int i = children.Count() - 1; i >= 0; i--)
		{
			lines.InsertAt(children[i], insertAt);
		}

		FileHandle writer = FileIO.OpenFile(absPath, FileMode.WRITE);
		if (!writer)
		{
			Print(TAG + "cannot write " + absPath, LogLevel.ERROR);
			return;
		}

		foreach (string outLine : lines)
		{
			writer.WriteLine(outLine);
		}

		writer.Close();
		Print(TAG + "arsenal shops and stash point added to " + absPath);
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the lines that make an inherited arsenal box a Marx arsenal shop (saved loadouts off, own name and decal)
	//! unless the override already has a shop with this ID.
	//! \return False when the shop's catalog does not exist.
	protected bool AddArsenalShop(notnull array<string> children, string text, string boxId, string shopId, string displayName, ResourceName decal)
	{
		if (text.Contains("m_sShopId \"" + shopId + "\""))
			return true;

		string catalogPath;
		Workbench.GetAbsolutePath(CTR_ConfigsPlugin.GetShopCatalogFile(shopId), catalogPath, false);
		ResourceManager resourceManager = Workbench.GetModule(ResourceManager);
		MetaFile meta = resourceManager.GetMetaFile(catalogPath);
		if (!meta)
		{
			Print(TAG + "no catalog for " + shopId + " (run Create Contractors Configs first), not changed", LogLevel.ERROR);
			return false;
		}

		children.Insert("  GenericEntity {");
		children.Insert("   ID \"" + boxId + "\"");
		children.Insert("   components {");
		children.Insert("    MRX_ArsenalShopComponent \"{" + NewEntityId() + "}\" {");
		children.Insert("    }");
		children.Insert("    MRX_ShopComponent \"{" + NewEntityId() + "}\" {");
		children.Insert("     m_sShopId \"" + shopId + "\"");
		children.Insert("     m_sDisplayName \"" + displayName + "\"");
		children.Insert("     m_sCatalog \"" + meta.GetResourceID() + "\"");
		children.Insert("    }");
		children.Insert("    SCR_ArsenalComponent \"" + ARSENAL_COMPONENT_ID + "\" {");
		children.Insert("     m_eArsenalSaveType SAVING_DISABLED");
		children.Insert("    }");
		children.Insert("    MeshObject \"" + MESH_COMPONENT_ID + "\" {");
		children.Insert("     Materials {");
		children.Insert("      MaterialAssignClass \"" + DECAL_MATERIAL_ID + "\" {");
		children.Insert("       SourceMaterial \"ArsenalBox_01_Decal\"");
		children.Insert("       AssignedMaterial \"" + decal + "\"");
		children.Insert("      }");
		children.Insert("      MaterialAssignClass \"" + MLOD_MATERIAL_ID + "\" {");
		children.Insert("       SourceMaterial \"ArsenalBox_01_MLOD\"");
		children.Insert("       AssignedMaterial \"" + MLOD_MATERIAL + "\"");
		children.Insert("      }");
		children.Insert("     }");
		children.Insert("    }");
		children.Insert("    UniversalInventoryStorageComponent \"" + STORAGE_COMPONENT_ID + "\" {");
		children.Insert("     Attributes SCR_ItemAttributeCollection \"" + STORAGE_ATTRIBUTES_ID + "\" {");
		children.Insert("      ItemDisplayName SCR_InventoryUIInfo \"" + STORAGE_UI_INFO_ID + "\" {");
		children.Insert("       Name \"" + displayName + "\"");
		children.Insert("      }");
		children.Insert("     }");
		children.Insert("    }");
		children.Insert("   }");
		children.Insert("  }");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The quartermaster prefab: the vanilla US officer as a shop keeper without AI that sells the services catalog
	//! (stash pages) in the shop window. Created once.
	//! \return Its resource name, empty when it cannot be created.
	protected ResourceName CreateQuartermasterPrefab()
	{
		string absPath;
		Workbench.GetAbsolutePath(QUARTERMASTER_FILE, absPath, false);
		if (FileIO.FileExists(absPath))
			return GetResourceName(absPath);

		string catalogPath;
		Workbench.GetAbsolutePath(CTR_ConfigsPlugin.GetShopCatalogFile(CTR_ShopPricing.SHOP_SERVICES), catalogPath, false);
		ResourceName catalog = GetResourceName(catalogPath);
		if (catalog.IsEmpty())
		{
			Print(TAG + "no services catalog (run Create Contractors Configs first), no quartermaster", LogLevel.ERROR);
			return ResourceName.Empty;
		}

		WorldEditorAPI api = SCR_WorldEditorToolHelper.GetWorldEditorAPI();
		if (!api || !api.GetWorld())
		{
			Print(TAG + "open a world first, no quartermaster", LogLevel.ERROR);
			return ResourceName.Empty;
		}

		FileIO.MakeDirectory(FilePath.StripFileName(absPath));
		bool manageAction = !api.IsDoingEditAction();
		if (manageAction)
			api.BeginEntityAction("Contractors quartermaster");

		IEntitySource source = api.CreateEntity(QUARTERMASTER_BASE, string.Empty, api.GetCurrentEntityLayerId(), null, vector.Zero, vector.Zero);
		if (!source)
		{
			Print(TAG + "CreateEntity failed: " + QUARTERMASTER_BASE, LogLevel.ERROR);
			if (manageAction)
				api.EndEntityAction();

			return ResourceName.Empty;
		}

		source.ClearVariable("coords");
		array<ref ContainerIdPathEntry> shopPath = { new ContainerIdPathEntry("MRX_ShopComponent") };
		Log("MRX_ShopComponent", api.CreateComponent(source, "MRX_ShopComponent") != null);
		Log("m_sShopId", api.SetVariableValue(source, shopPath, "m_sShopId", CTR_ShopPricing.SHOP_SERVICES));
		Log("m_sDisplayName", api.SetVariableValue(source, shopPath, "m_sDisplayName", "Quartermaster"));
		Log("m_sCatalog", api.SetVariableValue(source, shopPath, "m_sCatalog", catalog));
		Log("m_bAllowSell", api.SetVariableValue(source, shopPath, "m_bAllowSell", "0"));
		Log("m_fMaxDistance", api.SetVariableValue(source, shopPath, "m_fMaxDistance", "3"));
		Log("MRX_ShopKeeperComponent", api.CreateComponent(source, "MRX_ShopKeeperComponent") != null);

		// The character's own "default" action context (on the chest) gets the trade action.
		array<ref ContainerIdPathEntry> actionsPath = { new ContainerIdPathEntry("ActionsManagerComponent") };
		Log("additionalActions", api.CreateObjectArrayVariableMember(source, actionsPath, "additionalActions", "MRX_OpenShopAction", 0));
		array<ref ContainerIdPathEntry> actionPath = { new ContainerIdPathEntry("ActionsManagerComponent"), new ContainerIdPathEntry("additionalActions", 0) };
		Log("ParentContextList", api.SetVariableValue(source, actionPath, "ParentContextList", "default"));
		Log("UIInfo", api.CreateObjectVariableMember(source, actionPath, "UIInfo", "UIInfo"));
		array<ref ContainerIdPathEntry> uiInfoPath = { new ContainerIdPathEntry("ActionsManagerComponent"), new ContainerIdPathEntry("additionalActions", 0), new ContainerIdPathEntry("UIInfo") };
		Log("UIInfo.Name", api.SetVariableValue(source, uiInfoPath, "Name", "Talk to the Quartermaster"));

		Log("CreateEntityTemplate", api.CreateEntityTemplate(source, absPath));
		api.DeleteEntity(source);
		if (manageAction)
			api.EndEntityAction();

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
	protected void Log(string step, bool result)
	{
		if (result)
			Print(TAG + step + " ok");
		else
			Print(TAG + step + " FAILED", LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	//! Entity IDs in prefabs are written without braces, component IDs with them.
	protected string NewEntityId()
	{
		string id = Workbench.GenerateGloballyUniqueID64();
		id.Replace("{", string.Empty);
		id.Replace("}", string.Empty);
		return id;
	}
}
