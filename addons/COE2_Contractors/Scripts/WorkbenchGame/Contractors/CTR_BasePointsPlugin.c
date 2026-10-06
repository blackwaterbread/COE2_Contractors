// Development tool: turns the two FIA arsenal boxes of the COE2 main base prefab used by the COE2 worlds
// (COE_Hideout_01.et) into the Contractors arsenal shops and puts the stash point next to them. The override file
// itself is created in Workbench ("Override in" COE2_Contractors); this adds the lines by a text edit, with entity and
// component IDs from Workbench.GenerateGloballyUniqueID64(). Only adds what is missing: arsenal shops by shop ID, the
// stash point by prefab. Run "Create Contractors Configs" first, it creates the shop catalogs.

[WorkbenchPluginAttribute(name: "Add Contractors Base Points", category: "Contractors", wbModules: { "WorldEditor" })]
class CTR_BasePointsPlugin : WorldEditorPlugin
{
	static const string TAG = "[CTR_PLUGIN] ";
	static const string BASE_FILE = "$COE2_Contractors:Prefabs/Compositions/Misc/COE/COE_Hideout_01.et";
	static const ResourceName STASH_PREFAB = "{66BACE8BD545B8C2}Prefabs/Marx/Stash/MRX_StashWardrobe.et";
	//! Entity IDs of the FIA arsenal boxes in COE_Hideout_01.et (inherited children of the override).
	static const string WEAPONS_BOX_ID = "60A042236B427BA4";
	static const string EQUIPMENT_BOX_ID = "61288E8539ECF97D";
	//! SCR_ArsenalComponent entry of the vanilla arsenal boxes.
	static const string ARSENAL_COMPONENT_ID = "{56F2C6D1431AD9AF}";

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
		if (!AddArsenalShop(children, text, WEAPONS_BOX_ID, CTR_ShopPricing.SHOP_WEAPONS, "Contractor Armory")
			|| !AddArsenalShop(children, text, EQUIPMENT_BOX_ID, CTR_ShopPricing.SHOP_EQUIPMENT, "Contractor Outfitter"))
			return;

		if (!text.Contains(STASH_PREFAB))
		{
			children.Insert("  GenericEntity : \"" + STASH_PREFAB + "\" {");
			children.Insert("   ID \"" + NewEntityId() + "\"");
			children.Insert("   coords -0.4 0 6.6");
			children.Insert("   angles 0 180 0");
			children.Insert("  }");
		}

		if (children.IsEmpty())
		{
			Print(TAG + "base override already has the arsenal shops and the stash point, not changed");
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
	//! Adds the lines that make an inherited arsenal box a Marx arsenal shop (saved loadouts off) unless the override
	//! already has a shop with this ID.
	//! \return False when the shop's catalog does not exist.
	protected bool AddArsenalShop(notnull array<string> children, string text, string boxId, string shopId, string displayName)
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
		children.Insert("   }");
		children.Insert("  }");
		return true;
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
