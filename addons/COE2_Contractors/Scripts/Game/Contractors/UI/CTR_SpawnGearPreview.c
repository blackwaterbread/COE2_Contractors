//! The deploy menu's character preview shows the gear the player spawns with (CTR_SpawnGear) instead of the role's.
//! Arsenal loadouts stay as they are: they are put on after the spawn gear, so they are what the player gets.
modded class SCR_LoadoutPreviewComponent
{
	//! Previews showing spawn gear (weak), to dress again when the gear changes.
	protected static ref array<SCR_LoadoutPreviewComponent> s_aCTR_Previews = {};

	//! Weak: an entity of the item preview world.
	protected IEntity m_CTR_Previewed;
	protected PreviewRenderAttributes m_CTR_Attributes;

	//------------------------------------------------------------------------------------------------
	override IEntity SetPreviewedLoadout(notnull SCR_BasePlayerLoadout loadout, PreviewRenderAttributes attributes = null)
	{
		IEntity previewed = super.SetPreviewedLoadout(loadout, attributes);
		if (!previewed || SCR_PlayerArsenalLoadout.Cast(loadout) || !COE_GameMode.GetInstance())
			return previewed;

		m_CTR_Previewed = previewed;
		m_CTR_Attributes = attributes;
		if (!s_aCTR_Previews.Contains(this))
			s_aCTR_Previews.Insert(this);

		CTR_Dress();
		return previewed;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the local player's spawn gear on the previewed character.
	void CTR_Dress()
	{
		if (!m_CTR_Previewed || !m_PreviewManager || !m_wPreview)
			return;

		CTR_SpawnGear.Dress(m_CTR_Previewed, COE_PlayerController.CTR_GetSpawnGear());
		m_PreviewManager.SetPreviewItem(m_wPreview, m_CTR_Previewed, m_CTR_Attributes, true);
	}

	//------------------------------------------------------------------------------------------------
	//! The spawn gear changed (e.g. it arrived after the menu opened).
	static void CTR_RedressAll()
	{
		for (int i = s_aCTR_Previews.Count() - 1; i >= 0; i--)
		{
			SCR_LoadoutPreviewComponent preview = s_aCTR_Previews[i];
			if (preview)
				preview.CTR_Dress();
			else
				s_aCTR_Previews.Remove(i);
		}
	}
}
