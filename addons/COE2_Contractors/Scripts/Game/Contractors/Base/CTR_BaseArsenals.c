//! COE2's main base has unrestricted arsenals. Contractors buy their gear for money instead: two of the boxes are Marx
//! arsenal shops (base override prefab), the others are switched off (no items, no saved loadouts; role loadouts on
//! respawn are unaffected). The boxes stay: they also hold the base's supplies (SCR_ResourceComponent), and an override
//! prefab cannot remove inherited children anyway.
class CTR_BaseArsenals
{
	//------------------------------------------------------------------------------------------------
	//! Server. \return Number of arsenals switched off in every COE2 main base of the world (Marx arsenal shops stay).
	static int DisableAll()
	{
		array<IEntity> bases = {};
		KSC_WorldTools.GetEntitiesByType(bases, COE_MainBaseEntity);

		int count;
		foreach (IEntity base : bases)
		{
			count += DisableUnder(base);
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	static int DisableUnder(notnull IEntity parent)
	{
		int count;
		IEntity child = parent.GetChildren();
		while (child)
		{
			SCR_ArsenalComponent arsenal = SCR_ArsenalComponent.Cast(child.FindComponent(SCR_ArsenalComponent));
			if (arsenal && !MRX_ArsenalShopComponent.Find(child))
			{
				arsenal.SetArsenalEnabled(false);
				arsenal.SetArsenalSaveType(SCR_EArsenalSaveType.SAVING_DISABLED);
				count++;
			}

			count += DisableUnder(child);
			child = child.GetSibling();
		}

		return count;
	}
}
