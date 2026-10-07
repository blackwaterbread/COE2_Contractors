//! COE2 adds its commander editor mode to the vanilla editor core config without UI info. Vanilla sorts the modes by
//! that info when the editor core starts and throws a script exception on the missing one (a blocking dialog in
//! Workbench). Modes without UI info get one here: COE2's own name for the mode, sorted after the vanilla modes.
modded class SCR_EditorModePrefab
{
	protected ref SCR_EditorModeUIInfo m_CTR_FallbackInfo;

	//------------------------------------------------------------------------------------------------
	override SCR_EditorModeUIInfo GetInfo()
	{
		SCR_EditorModeUIInfo info = super.GetInfo();
		if (info)
			return info;

		if (!m_CTR_FallbackInfo)
		{
			m_CTR_FallbackInfo = new SCR_EditorModeUIInfo();
			m_CTR_FallbackInfo.CTR_InitFallback(GetMode());
		}

		return m_CTR_FallbackInfo;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_EditorModeUIInfo
{
	protected static const int CTR_FALLBACK_ORDER = 100;

	//------------------------------------------------------------------------------------------------
	//! Fills an info created for a mode that has none (see SCR_EditorModePrefab.GetInfo).
	void CTR_InitFallback(EEditorMode mode)
	{
		if (mode == EEditorMode.COE_COMMANDER)
			SetName("#COE-Editor_CommanderMode_Name");
		else
			SetName(typename.EnumToString(EEditorMode, mode));

		m_ModeColor = Color.FromRGBA(255, 255, 255, 255);
		m_iOrder = CTR_FALLBACK_ORDER;
	}
}
