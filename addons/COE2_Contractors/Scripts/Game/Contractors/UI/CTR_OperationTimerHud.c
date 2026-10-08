//! Operation timer at the top left of the HUD while an operation runs (client), in the look of the Marx balance panel.
//! Two boxes: the operation (its time stops when the tasks end, the title then tells how), and during the exfil the
//! countdown, with hundredths running fast for urgency, and the enemy pursuit once it came. What concerns the return
//! (players at the exfil point, the hold, the return after a cancel) shows at the top centre (CTR_ExfilStatusHud).
//! Reads the replicated state of the game mode; times are server timestamps, so every machine shows the same.
class CTR_OperationTimerHud : Managed
{
	protected static const int UPDATE_MS = 250;
	protected static const float MARGIN = 24;
	//! Both boxes; their rows fill it (title left, value right).
	protected static const float WIDTH = 300;
	protected static const float BOX_GAP = 6;
	protected static const ResourceName BOLD_FONT = "{EABA4FE9D014CCEF}UI/Fonts/RobotoCondensed/RobotoCondensed_Bold.fnt";
	static const int TEXT_FONT_SIZE = 20;
	protected static const int FRACTION_FONT_SIZE = 14;
	protected static const float ACCENT_WIDTH = 4;
	//! The exfil countdown turns red in its last minute.
	protected static const int URGENT_SECONDS = 60;
	static const ResourceName ICONS = "{2EFEA2AF1F38E7F0}UI/Textures/Icons/icons_wrapperUI-64.imageset";
	static const string WARNING_ICON = "warning";
	protected static const float ICON_SIZE = 18;

	protected Widget m_wRoot;
	protected Widget m_wExfilBox;
	protected ref CTR_TimerRow m_OperationRow;
	protected ref CTR_TimerRow m_ExfilRow;
	protected TextWidget m_wExfilFraction;
	protected ref CTR_TimerRow m_PursuitRow;
	protected bool m_bFastUpdate;

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
		{
			callQueue.Remove(Update);
			callQueue.Remove(UpdateExfil);
		}

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
	//! For tests: "operation", "exfil" or "pursuit". Empty when hidden.
	string GetRowText(string row)
	{
		CTR_TimerRow timerRow = GetRow(row);
		if (!IsShown() || !timerRow || !timerRow.IsShown())
			return string.Empty;

		if (row != "operation" && !m_wExfilBox.IsVisible())
			return string.Empty;

		return timerRow.GetValue();
	}

	//------------------------------------------------------------------------------------------------
	protected CTR_TimerRow GetRow(string row)
	{
		switch (row)
		{
			case "operation": return m_OperationRow;
			case "exfil": return m_ExfilRow;
			case "pursuit": return m_PursuitRow;
		}

		return null;
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
		{
			SetFastUpdate(false);
			return;
		}

		m_OperationRow.SetTitle(GetOperationTitle(gameMode.CTR_GetTasksEnd()));
		m_OperationRow.Set(CTR_ResultDialog.FormatDuration(gameMode.CTR_GetTasksSeconds()), Color.FromInt(Color.WHITE));

		bool exfil = gameMode.CTR_GetExfilDeadline() != null;
		m_wExfilBox.SetVisible(exfil);
		SetFastUpdate(exfil);
		if (exfil)
			UpdateExfil();

		UpdatePursuit(gameMode);
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetOperationTitle(int tasksEnd)
	{
		switch (tasksEnd)
		{
			case COE_GameMode.CTR_TASKS_COMPLETE: return "#CTR-Timer_OperationComplete";
			case COE_GameMode.CTR_TASKS_EARLY_EXFIL: return "#CTR-Timer_EarlyExfil";
			case COE_GameMode.CTR_TASKS_CANCELLED: return "#CTR-Timer_OperationCancelled";
		}

		return "#CTR-Timer_Operation";
	}

	//------------------------------------------------------------------------------------------------
	//! The exfil countdown runs every frame: its hundredths must move.
	protected void SetFastUpdate(bool fast)
	{
		if (fast == m_bFastUpdate)
			return;

		m_bFastUpdate = fast;
		GetGame().GetCallqueue().Remove(UpdateExfil);
		if (fast)
			GetGame().GetCallqueue().CallLater(UpdateExfil, 0, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateExfil()
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode || !m_ExfilRow)
			return;

		WorldTimestamp deadline = gameMode.CTR_GetExfilDeadline();
		if (!deadline)
			return;

		float left = Math.Max(0, COE_GameMode.CTR_SecondsUntil(deadline));
		int seconds = left;
		int hundredths = (left - seconds) * 100;
		Color color = Color.FromInt(Color.WHITE);
		if (left <= URGENT_SECONDS)
			color = GetAlarmColor();

		m_ExfilRow.Set(CTR_ResultDialog.FormatDuration(seconds), color);
		if (m_wExfilFraction)
		{
			m_wExfilFraction.SetText("." + hundredths.ToString(2));
			m_wExfilFraction.SetColor(color);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Shows only once the first wave came: the chance of a pursuit is never shown.
	protected void UpdatePursuit(notnull COE_GameMode gameMode)
	{
		int wave = gameMode.CTR_GetPursuitWave();
		m_PursuitRow.SetShown(wave > 0);
		if (wave <= 0)
			return;

		WorldTimestamp next = gameMode.CTR_GetNextWave();
		string text = WidgetManager.Translate("#CTR-Timer_PursuitLast", wave);
		if (next)
			text = WidgetManager.Translate("#CTR-Timer_PursuitValue", wave, CTR_ResultDialog.FormatDuration(Math.Max(0, Math.Ceil(COE_GameMode.CTR_SecondsUntil(next)))));

		m_PursuitRow.Set(text, GetAlarmColor());
	}

	//------------------------------------------------------------------------------------------------
	protected void Build(notnull Widget parent)
	{
		m_wRoot = CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), parent);
		FrameSlot.SetAnchorMin(m_wRoot, 0, 0);
		FrameSlot.SetAnchorMax(m_wRoot, 0, 0);
		FrameSlot.SetAlignment(m_wRoot, 0, 0);
		FrameSlot.SetPos(m_wRoot, MARGIN, MARGIN);
		FrameSlot.SetSizeToContent(m_wRoot, true);

		Widget operation = AddBox(m_wRoot, GetAccentColor(), 0);
		m_OperationRow = AddRow(operation, "#CTR-Timer_Operation", GetAccentColor());

		Widget exfil = AddBox(m_wRoot, GetAccentColor(), BOX_GAP);
		m_wExfilBox = exfil.GetParent().GetParent();
		m_ExfilRow = AddRow(exfil, "#CTR-Timer_Exfil", GetAccentColor());
		m_wExfilFraction = CreateText(m_ExfilRow.m_wRow, FRACTION_FONT_SIZE, Color.FromInt(Color.WHITE));
		AlignableSlot.SetVerticalAlign(m_wExfilFraction, LayoutVerticalAlign.Bottom);
		AlignableSlot.SetPadding(m_wExfilFraction, 1, 0, 0, 2);
		m_PursuitRow = AddRow(exfil, "#CTR-Timer_Pursuit", GetAlarmColor(), WARNING_ICON);
	}

	//------------------------------------------------------------------------------------------------
	//! A box in the panel's look. \return The vertical layout for its rows.
	static Widget AddBox(notnull Widget parent, Color accentColor, float topGap, float width = WIDTH)
	{
		Widget box = CreateWidget(WidgetType.OverlayWidgetTypeID, Color.FromInt(Color.WHITE), parent);
		AlignableSlot.SetHorizontalAlign(box, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(box, 0, topGap, 0, 0);

		Stretch(CreateWidget(WidgetType.ImageWidgetTypeID, Color.FromSRGBA(62, 66, 72, 255), box));
		Widget fill = CreateWidget(WidgetType.ImageWidgetTypeID, Color.FromSRGBA(20, 22, 25, 240), box);
		Stretch(fill);
		AlignableSlot.SetPadding(fill, 1, 1, 1, 1);
		ImageWidget accent = ImageWidget.Cast(CreateWidget(WidgetType.ImageWidgetTypeID, accentColor, box));
		AlignableSlot.SetHorizontalAlign(accent, LayoutHorizontalAlign.Left);
		AlignableSlot.SetVerticalAlign(accent, LayoutVerticalAlign.Stretch);
		accent.SetSize(ACCENT_WIDTH, 1);

		SizeLayoutWidget size = SizeLayoutWidget.Cast(CreateWidget(WidgetType.SizeLayoutWidgetTypeID, Color.FromInt(Color.WHITE), box));
		AlignableSlot.SetHorizontalAlign(size, LayoutHorizontalAlign.Stretch);
		// Here, not on the column: the box grows by this padding, a size layout ignores its child's. Less above than
		// below: the font leaves room above the letters.
		AlignableSlot.SetPadding(size, 0, 6, 0, 7);
		size.EnableWidthOverride(true);
		size.SetWidthOverride(width);
		Widget column = CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), size);
		AlignableSlot.SetHorizontalAlign(column, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(column, 12 + ACCENT_WIDTH, 0, 12, 0);
		return column;
	}

	//------------------------------------------------------------------------------------------------
	//! A title and a value of the same size, the value on the right.
	static CTR_TimerRow AddRow(notnull Widget column, string title, Color titleColor, string icon = string.Empty, int fontSize = TEXT_FONT_SIZE)
	{
		Widget row = CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), column);
		AlignableSlot.SetHorizontalAlign(row, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(row, 0, 1, 0, 1);

		if (!icon.IsEmpty())
			AddIcon(row, icon, titleColor, ICON_SIZE);

		TextWidget titleText = CreateText(row, fontSize, titleColor);
		titleText.SetText(title);
		LayoutSlot.SetSizeMode(titleText, LayoutSizeMode.Fill);
		AlignableSlot.SetVerticalAlign(titleText, LayoutVerticalAlign.Center);

		TextWidget valueText = CreateText(row, fontSize, Color.FromInt(Color.WHITE));
		AlignableSlot.SetVerticalAlign(valueText, LayoutVerticalAlign.Center);
		AlignableSlot.SetPadding(valueText, 16, 0, 0, 0);

		CTR_TimerRow timerRow = new CTR_TimerRow();
		timerRow.m_wRow = row;
		timerRow.m_wTitle = titleText;
		timerRow.m_wValue = valueText;
		return timerRow;
	}

	//------------------------------------------------------------------------------------------------
	//! An icon of the vanilla UI imageset before a text in a horizontal layout.
	static ImageWidget AddIcon(notnull Widget row, string icon, Color color, float size)
	{
		// Blended, or the transparent part of the icon shows black.
		int flags = WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS;
		ImageWidget image = ImageWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.ImageWidgetTypeID, flags, color, 0, row));
		image.LoadImageFromSet(0, ICONS, icon);
		image.SetImage(0);
		image.SetSize(size, size);
		AlignableSlot.SetVerticalAlign(image, LayoutVerticalAlign.Center);
		AlignableSlot.SetPadding(image, 0, 0, 6, 0);
		return image;
	}

	//------------------------------------------------------------------------------------------------
	static Widget CreateWidget(WidgetType type, Color color, Widget parent)
	{
		int flags = WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS;
		return GetGame().GetWorkspace().CreateWidget(type, flags, color, 0, parent);
	}

	//------------------------------------------------------------------------------------------------
	static TextWidget CreateText(Widget parent, int size, Color color)
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
	TextWidget m_wTitle;
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
	void SetTitle(string title)
	{
		if (m_wTitle && m_wTitle.GetText() != title)
			m_wTitle.SetText(title);
	}

	//------------------------------------------------------------------------------------------------
	void SetTitleColor(Color color)
	{
		if (m_wTitle)
			m_wTitle.SetColor(color);
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
