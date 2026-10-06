// Development tool: puts the Contractors shop and stash point into the COE2 main base prefab used by the COE2 worlds
// (COE_Hideout_01.et), next to the base's arsenal boxes. The override file itself is created in Workbench
// ("Override in" COE2_Contractors); this adds the children by a text edit, with entity IDs from
// Workbench.GenerateGloballyUniqueID64(). Runs once: an override that already has a shop is left alone.

[WorkbenchPluginAttribute(name: "Add Contractors Base Points", category: "Contractors", wbModules: { "WorldEditor" })]
class CTR_BasePointsPlugin : WorldEditorPlugin
{
	static const string TAG = "[CTR_PLUGIN] ";
	static const string BASE_FILE = "$COE2_Contractors:Prefabs/Compositions/Misc/COE/COE_Hideout_01.et";
	static const ResourceName SHOP_PREFAB = "{10C12BB88B37C571}Prefabs/Marx/Shop/MRX_ShopTable.et";
	static const ResourceName STASH_PREFAB = "{66BACE8BD545B8C2}Prefabs/Marx/Stash/MRX_StashWardrobe.et";
	//! MRX_ShopComponent entry of MRX_ShopTable.et.
	static const string SHOP_COMPONENT_ID = "{6A89197001F4D092}";
	static const ResourceName SHOP_CATALOG = "{503CE5B0C4047B85}Configs/Contractors/Shop/CTR_ShopCatalog.conf";

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

		foreach (string text : lines)
		{
			if (text.Contains("MRX_ShopTable"))
			{
				Print(TAG + "base override already has a shop, not changed");
				return;
			}
		}

		// Expected: the bare override, "COE_MainBaseEntity {", " ID ...", "}".
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

		array<string> children = {
			" {",
			"  GenericEntity : \"" + SHOP_PREFAB + "\" {",
			"   ID \"" + NewEntityId() + "\"",
			"   components {",
			"    MRX_ShopComponent \"" + SHOP_COMPONENT_ID + "\" {",
			"     m_sShopId \"contractors\"",
			"     m_sDisplayName \"Contractor Supply\"",
			"     m_sCatalog \"" + SHOP_CATALOG + "\"",
			"    }",
			"   }",
			"   coords -2.6 0 6.3",
			"  }",
			"  GenericEntity : \"" + STASH_PREFAB + "\" {",
			"   ID \"" + NewEntityId() + "\"",
			"   coords -0.4 0 6.6",
			"   angles 0 180 0",
			"  }",
			" }"
		};

		for (int i = children.Count() - 1; i >= 0; i--)
		{
			lines.InsertAt(children[i], last);
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
		Print(TAG + "shop and stash point added to " + absPath);
	}

	//------------------------------------------------------------------------------------------------
	//! Entity IDs in prefabs are written without braces.
	protected string NewEntityId()
	{
		string id = Workbench.GenerateGloballyUniqueID64();
		id.Replace("{", string.Empty);
		id.Replace("}", string.Empty);
		return id;
	}
}
