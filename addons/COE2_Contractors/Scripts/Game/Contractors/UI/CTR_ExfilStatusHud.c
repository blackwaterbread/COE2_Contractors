//! What CTR_ExfilStatusHud shows.
enum CTR_EExfilStatus
{
	NONE,
	//! Players at the exfil point.
	PRESENT,
	//! Enough players hold the exfil point: everyone returns soon.
	HOLD,
	//! The commander cancelled: everyone returns soon.
	CANCEL
}

//------------------------------------------------------------------------------------------------
//! What the return depends on, at the top centre of the HUD (client), in the look of the operation timer:
//! - after the commander cancelled: the return countdown;
//! - while enough players hold the exfil point: the return countdown, for everyone;
//! - inside the exfil point: players there out of those who must exfil, and how many are needed.
//! Below the alerts (CTR_AlertHud), so both can show at once.
class CTR_ExfilStatusHud : Managed
{
	protected static const int UPDATE_MS = 250;
	protected static const float TOP_MARGIN = 225;
	protected static const float WIDTH = 360;
	protected static const int FONT_SIZE = 24;

	protected Widget m_wRoot;
	protected ref CTR_TimerRow m_Row;
	protected CTR_EExfilStatus m_eShown;

	//------------------------------------------------------------------------------------------------
	//! \return Null before the vanilla HUD exists.
	static CTR_ExfilStatusHud Create()
	{
		SCR_HUDManagerComponent hudManager = SCR_HUDManagerComponent.GetHUDManager();
		if (!hudManager || !hudManager.GetHUDRootWidget())
			return null;

		CTR_ExfilStatusHud hud = new CTR_ExfilStatusHud();
		hud.Build(hudManager.GetHUDRootWidget());
		hud.Update();
		GetGame().GetCallqueue().CallLater(hud.Update, UPDATE_MS, true);
		return hud;
	}

	//------------------------------------------------------------------------------------------------
	void ~CTR_ExfilStatusHud()
	{
		ScriptCallQueue callQueue = GetGame().GetCallqueue();
		if (callQueue)
			callQueue.Remove(Update);

		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();
	}

	//------------------------------------------------------------------------------------------------
	bool IsBuilt()
	{
		return m_wRoot != null;
	}

	//------------------------------------------------------------------------------------------------
	//! What is shown, for tests.
	CTR_EExfilStatus GetShown()
	{
		if (!m_wRoot || !m_wRoot.IsVisible())
			return CTR_EExfilStatus.NONE;

		return m_eShown;
	}

	//------------------------------------------------------------------------------------------------
	protected void Update()
	{
		if (!m_wRoot)
			return;

		m_eShown = CTR_EExfilStatus.NONE;
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (gameMode && gameMode.CTR_HasOperation())
			m_eShown = Show(gameMode);

		m_wRoot.SetVisible(m_eShown != CTR_EExfilStatus.NONE);
	}

	//------------------------------------------------------------------------------------------------
	protected CTR_EExfilStatus Show(notnull COE_GameMode gameMode)
	{
		WorldTimestamp cancelReturn = gameMode.CTR_GetCancelReturn();
		if (cancelReturn)
		{
			SetRow("#CTR-Timer_Cancel", CTR_OperationTimerHud.GetAccentColor(), FormatSecondsLeft(cancelReturn), CTR_OperationTimerHud.GetAccentColor());
			return CTR_EExfilStatus.CANCEL;
		}

		WorldTimestamp holdEnd = gameMode.CTR_GetExfilHoldEnd();
		if (holdEnd)
		{
			SetRow("#CTR-Timer_Hold", CTR_OperationTimerHud.GetGoColor(), FormatSecondsLeft(holdEnd), CTR_OperationTimerHud.GetGoColor());
			return CTR_EExfilStatus.HOLD;
		}

		if (!gameMode.CTR_IsInExfil() || !IsLocalPlayerAtExfil(gameMode))
			return CTR_EExfilStatus.NONE;

		int present, outside, needed;
		gameMode.CTR_GetExfilCount(present, outside, needed);
		Color color = CTR_OperationTimerHud.GetMutedColor();
		if (CTR_ExfilRules.IsMet(present, outside, CTR_Settings.Get().m_fExfilPlayerRatio))
			color = CTR_OperationTimerHud.GetGoColor();

		SetRow("#CTR-Timer_Present", CTR_OperationTimerHud.GetAccentColor(), WidgetManager.Translate("#CTR-Timer_PresentValue", present, outside, needed), color);
		return CTR_EExfilStatus.PRESENT;
	}

	//------------------------------------------------------------------------------------------------
	//! Inside the radius the server counts players at the exfil point with.
	protected static bool IsLocalPlayerAtExfil(notnull COE_GameMode gameMode)
	{
		vector exfil;
		IEntity character = SCR_PlayerController.GetLocalControlledEntity();
		if (!character || !gameMode.CTR_GetReplicatedExfilPoint(exfil))
			return false;

		return vector.Distance(character.GetOrigin(), exfil) <= CTR_Settings.Get().m_fExfilRadius;
	}

	//------------------------------------------------------------------------------------------------
	protected static string FormatSecondsLeft(WorldTimestamp time)
	{
		return CTR_ResultDialog.FormatDuration(Math.Max(0, Math.Ceil(COE_GameMode.CTR_SecondsUntil(time))));
	}

	//------------------------------------------------------------------------------------------------
	protected void SetRow(string title, Color titleColor, string value, Color valueColor)
	{
		m_Row.SetTitle(title);
		m_Row.SetTitleColor(titleColor);
		m_Row.Set(value, valueColor);
	}

	//------------------------------------------------------------------------------------------------
	protected void Build(notnull Widget parent)
	{
		m_wRoot = CTR_OperationTimerHud.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), parent);
		FrameSlot.SetAnchorMin(m_wRoot, 0.5, 0);
		FrameSlot.SetAnchorMax(m_wRoot, 0.5, 0);
		FrameSlot.SetAlignment(m_wRoot, 0.5, 0);
		FrameSlot.SetPos(m_wRoot, 0, TOP_MARGIN);
		FrameSlot.SetSizeToContent(m_wRoot, true);

		Widget column = CTR_OperationTimerHud.AddBox(m_wRoot, CTR_OperationTimerHud.GetAccentColor(), 0, WIDTH);
		AlignableSlot.SetHorizontalAlign(column, LayoutHorizontalAlign.Center);
		m_Row = CTR_OperationTimerHud.AddRow(column, "#CTR-Timer_Present", CTR_OperationTimerHud.GetAccentColor(), string.Empty, FONT_SIZE);
	}
}

