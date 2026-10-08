//! Operation timer at the top left of the HUD while an operation runs (client), in the look of the Marx balance panel:
//! operation time, the exfil countdown, players at the exfil point, the hold before the return and the return after
//! the commander cancelled. Rows show only when they apply. Reads the replicated state of the game mode; times are
//! server timestamps, so every machine shows the same.
class CTR_OperationTimerHud : Managed
{
	protected static const int UPDATE_MS = 250;
	protected static const float MARGIN = 24;
	protected static const float WIDTH = 330;
	protected static const ResourceName BOLD_FONT = "{EABA4FE9D014CCEF}UI/Fonts/RobotoCondensed/RobotoCondensed_Bold.fnt";
	protected static const int TITLE_FONT_SIZE = 16;
	protected static const int VALUE_FONT_SIZE = 24;
	protected static const float ACCENT_WIDTH = 4;
	//! The exfil countdown turns red in its last minute.
	protected static const int URGENT_SECONDS = 60;

	protected Widget m_wRoot;
	protected ref CTR_TimerRow m_OperationRow;
	protected ref CTR_TimerRow m_ExfilRow;
	protected ref CTR_TimerRow m_PresentRow;
	protected ref CTR_TimerRow m_HoldRow;
	protected ref CTR_TimerRow m_CancelRow;

	//------------------------------------------------------------------------------------------------
	//! \return Null before the vanilla HUD exists.
	static CTR_OperationTimerHud Create()
	{
		SCR_HUDManagerComponent hudManager = SCR_HUDManagerComponent.GetHUDManager();
		if (!hudManager || !hudManager.GetHUDRootWidget())
			return null;

		CTR_OperationTimerHud hud = new CTR_OperationTimerHud();
		hud.Build(hudManager.GetHUDRootWidget());
		hud.Update();
		GetGame().GetCallqueue().CallLater(hud.Update, UPDATE_MS, true);
		return hud;
	}

	//------------------------------------------------------------------------------------------------
	void ~CTR_OperationTimerHud()
	{
		ScriptCallQueue callQueue = GetGame().GetCallqueue();
		if (callQueue)
			callQueue.Remove(Update);

		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();
	}

	//------------------------------------------------------------------------------------------------
	//! False once the HUD it was built on is gone.
	bool IsBuilt()
	{
		return m_wRoot != null;
	}

	//------------------------------------------------------------------------------------------------
	bool IsShown()
	{
		return m_wRoot && m_wRoot.IsVisible();
	}

	//------------------------------------------------------------------------------------------------
	//! Text of a row as shown, for tests: "operation", "exfil", "present", "hold", "cancel". Empty when hidden.
	string GetRowText(string row)
	{
		CTR_TimerRow timerRow;
		switch (row)
		{
			case "operation": timerRow = m_OperationRow; break;
			case "exfil": timerRow = m_ExfilRow; break;
			case "present": timerRow = m_PresentRow; break;
			case "hold": timerRow = m_HoldRow; break;
			case "cancel": timerRow = m_CancelRow; break;
		}

		if (!IsShown() || !timerRow || !timerRow.IsShown())
			return string.Empty;

		return timerRow.GetValue();
	}

	//------------------------------------------------------------------------------------------------
	protected void Update()
	{
		if (!m_wRoot)
			return;

		COE_GameMode gameMode = COE_GameMode.GetInstance();
		bool shown = gameMode && gameMode.CTR_HasOperation();
		m_wRoot.SetVisible(shown);
		if (!shown)
			return;

		m_OperationRow.Set(CTR_ResultDialog.FormatDuration(gameMode.CTR_GetOperationSeconds()), Color.FromInt(Color.WHITE));

		WorldTimestamp deadline = gameMode.CTR_GetExfilDeadline();
		m_ExfilRow.SetShown(deadline != null);
		m_PresentRow.SetShown(deadline != null);
		if (deadline)
		{
			int left = Math.Max(0, Math.Ceil(COE_GameMode.CTR_SecondsUntil(deadline)));
			Color exfilColor = Color.FromInt(Color.WHITE);
			if (left <= URGENT_SECONDS)
				exfilColor = GetAlarmColor();

			m_ExfilRow.Set(CTR_ResultDialog.FormatDuration(left), exfilColor);

			int present, outside, needed;
			gameMode.CTR_GetExfilCount(present, outside, needed);
			Color presentColor = GetMutedColor();
			if (CTR_ExfilRules.IsMet(present, outside, CTR_Settings.Get().m_fExfilPlayerRatio))
				presentColor = GetGoColor();

			m_PresentRow.Set(WidgetManager.Translate("#CTR-Timer_PresentValue", present, outside, needed), presentColor);
		}

		WorldTimestamp holdEnd = gameMode.CTR_GetExfilHoldEnd();
		m_HoldRow.SetShown(holdEnd != null);
		if (holdEnd)
			m_HoldRow.Set(CTR_ResultDialog.FormatDuration(Math.Max(0, Math.Ceil(COE_GameMode.CTR_SecondsUntil(holdEnd)))), GetGoColor());

		WorldTimestamp cancelReturn = gameMode.CTR_GetCancelReturn();
		m_CancelRow.SetShown(cancelReturn != null);
		if (cancelReturn)
			m_CancelRow.Set(CTR_ResultDialog.FormatDuration(Math.Max(0, Math.Ceil(COE_GameMode.CTR_SecondsUntil(cancelReturn)))), GetAccentColor());
	}

	//------------------------------------------------------------------------------------------------
	protected void Build(notnull Widget parent)
	{
		m_wRoot = CreateWidget(WidgetType.OverlayWidgetTypeID, Color.FromInt(Color.WHITE), parent);
		FrameSlot.SetAnchorMin(m_wRoot, 0, 0);
		FrameSlot.SetAnchorMax(m_wRoot, 0, 0);
		FrameSlot.SetAlignment(m_wRoot, 0, 0);
		FrameSlot.SetPos(m_wRoot, MARGIN, MARGIN);
		FrameSlot.SetSizeToContent(m_wRoot, true);

		Stretch(CreateWidget(WidgetType.ImageWidgetTypeID, Color.FromSRGBA(62, 66, 72, 255), m_wRoot));
		Widget fill = CreateWidget(WidgetType.ImageWidgetTypeID, Color.FromSRGBA(20, 22, 25, 240), m_wRoot);
		Stretch(fill);
		AlignableSlot.SetPadding(fill, 1, 1, 1, 1);
		ImageWidget accent = ImageWidget.Cast(CreateWidget(WidgetType.ImageWidgetTypeID, GetAccentColor(), m_wRoot));
		AlignableSlot.SetHorizontalAlign(accent, LayoutHorizontalAlign.Left);
		AlignableSlot.SetVerticalAlign(accent, LayoutVerticalAlign.Stretch);
		accent.SetSize(ACCENT_WIDTH, 1);

		SizeLayoutWidget size = SizeLayoutWidget.Cast(CreateWidget(WidgetType.SizeLayoutWidgetTypeID, Color.FromInt(Color.WHITE), m_wRoot));
		size.EnableWidthOverride(true);
		size.SetWidthOverride(WIDTH);
		Widget column = CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), size);
		AlignableSlot.SetPadding(column, 14 + ACCENT_WIDTH, 6, 14, 8);

		m_OperationRow = AddRow(column, "#CTR-Timer_Operation", GetAccentColor());
		m_ExfilRow = AddRow(column, "#CTR-Timer_Exfil", GetAccentColor());
		m_PresentRow = AddRow(column, "#CTR-Timer_Present", GetAccentColor());
		m_HoldRow = AddRow(column, "#CTR-Timer_Hold", GetGoColor());
		m_CancelRow = AddRow(column, "#CTR-Timer_Cancel", GetAccentColor());
	}

	//------------------------------------------------------------------------------------------------
	protected CTR_TimerRow AddRow(notnull Widget column, string title, Color titleColor)
	{
		Widget row = CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), column);
		AlignableSlot.SetHorizontalAlign(row, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(row, 0, 2, 0, 2);

		TextWidget titleText = CreateText(row, TITLE_FONT_SIZE, titleColor);
		titleText.SetText(title);
		LayoutSlot.SetSizeMode(titleText, LayoutSizeMode.Fill);
		AlignableSlot.SetVerticalAlign(titleText, LayoutVerticalAlign.Center);

		TextWidget valueText = CreateText(row, VALUE_FONT_SIZE, Color.FromInt(Color.WHITE));
		AlignableSlot.SetVerticalAlign(valueText, LayoutVerticalAlign.Center);
		AlignableSlot.SetPadding(valueText, 12, 0, 0, 0);

		CTR_TimerRow timerRow = new CTR_TimerRow();
		timerRow.m_wRow = row;
		timerRow.m_wValue = valueText;
		return timerRow;
	}

	//------------------------------------------------------------------------------------------------
	protected static Widget CreateWidget(WidgetType type, Color color, Widget parent)
	{
		int flags = WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS;
		return GetGame().GetWorkspace().CreateWidget(type, flags, color, 0, parent);
	}

	//------------------------------------------------------------------------------------------------
	protected static TextWidget CreateText(Widget parent, int size, Color color)
	{
		TextWidget text = TextWidget.Cast(CreateWidget(WidgetType.TextWidgetTypeID, color, parent));
		text.SetFont(BOLD_FONT);
		text.SetExactFontSize(size);
		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Stretch(notnull Widget widget)
	{
		AlignableSlot.SetHorizontalAlign(widget, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetVerticalAlign(widget, LayoutVerticalAlign.Stretch);
	}

	//------------------------------------------------------------------------------------------------
	static Color GetAccentColor()
	{
		return Color.FromSRGBA(226, 167, 79, 255);
	}

	//------------------------------------------------------------------------------------------------
	static Color GetGoColor()
	{
		return Color.FromSRGBA(120, 205, 100, 255);
	}

	//------------------------------------------------------------------------------------------------
	static Color GetAlarmColor()
	{
		return Color.FromSRGBA(235, 90, 75, 255);
	}

	//------------------------------------------------------------------------------------------------
	static Color GetMutedColor()
	{
		return Color.FromSRGBA(200, 204, 210, 255);
	}
}

//------------------------------------------------------------------------------------------------
//! One row of the operation timer: a title and a value.
class CTR_TimerRow : Managed
{
	Widget m_wRow;
	TextWidget m_wValue;

	//------------------------------------------------------------------------------------------------
	void Set(string value, Color color)
	{
		if (!m_wValue)
			return;

		m_wValue.SetText(value);
		m_wValue.SetColor(color);
	}

	//------------------------------------------------------------------------------------------------
	void SetShown(bool shown)
	{
		if (m_wRow)
			m_wRow.SetVisible(shown);
	}

	//------------------------------------------------------------------------------------------------
	bool IsShown()
	{
		return m_wRow && m_wRow.IsVisible();
	}

	//------------------------------------------------------------------------------------------------
	string GetValue()
	{
		if (!m_wValue)
			return string.Empty;

		return m_wValue.GetText();
	}
}
