//! Alerts the server shows on every player's screen.
enum CTR_EAlert
{
	//! The commander cancelled the operation; everyone returns shortly.
	CANCELLED,
	//! Every task is finished; the exfil starts.
	EXFIL,
	//! The commander ordered the early exfil.
	EARLY_EXFIL,
	//! A wave of enemy pursuers is coming; param = compass octant they come from (0 = north, clockwise).
	PURSUIT
}

//------------------------------------------------------------------------------------------------
//! A short alert at the top centre of the HUD (client): a large title and a small line, gone after a few seconds.
class CTR_AlertHud
{
	//! How long every alert stays on the screen.
	static const int SHOW_MS = 8000;

	protected static const float TOP_MARGIN = 110;
	protected static const ResourceName BOLD_FONT = "{EABA4FE9D014CCEF}UI/Fonts/RobotoCondensed/RobotoCondensed_Bold.fnt";
	protected static const int TITLE_FONT_SIZE = 34;
	protected static const int LINE_FONT_SIZE = 18;
	protected static const float ACCENT_HEIGHT = 3;

	protected static Widget s_wRoot;
	protected static int s_iLastAlert = -1;

	//------------------------------------------------------------------------------------------------
	//! Replaces an alert still shown.
	static void Show(CTR_EAlert alert, int param)
	{
		SCR_HUDManagerComponent hudManager = SCR_HUDManagerComponent.GetHUDManager();
		if (!hudManager || !hudManager.GetHUDRootWidget())
			return;

		Hide();
		Color color = CTR_OperationTimerHud.GetAccentColor();
		string line;
		string title;
		switch (alert)
		{
			case CTR_EAlert.CANCELLED:
				title = "#CTR-Alert_Cancelled";
				line = "#CTR-Alert_CancelledLine";
				break;
			case CTR_EAlert.EXFIL:
				title = "#CTR-Alert_Exfil";
				line = "#CTR-Alert_ExfilLine";
				break;
			case CTR_EAlert.EARLY_EXFIL:
				title = "#CTR-Alert_EarlyExfil";
				line = "#CTR-Alert_EarlyExfilLine";
				break;
			case CTR_EAlert.PURSUIT:
				title = "#CTR-Alert_Pursuit";
				line = WidgetManager.Translate("#CTR-Alert_PursuitLine", WidgetManager.Translate(GetDirectionKey(param)));
				color = CTR_OperationTimerHud.GetAlarmColor();
				break;
		}

		string icon;
		if (alert == CTR_EAlert.PURSUIT)
			icon = CTR_OperationTimerHud.WARNING_ICON;

		Build(hudManager.GetHUDRootWidget(), title, line, color, icon);
		s_iLastAlert = alert;
		if (alert == CTR_EAlert.PURSUIT)
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.TASK_FAILED);
		else if (alert == CTR_EAlert.EXFIL)
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.TASK_SUCCEED);
		else
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.HINT);

		GetGame().GetCallqueue().CallLater(Hide, SHOW_MS);
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		GetGame().GetCallqueue().Remove(Hide);
		if (s_wRoot)
			s_wRoot.RemoveFromHierarchy();

		s_wRoot = null;
	}

	//------------------------------------------------------------------------------------------------
	//! The alert on the screen, or -1. For tests.
	static int GetShownAlert()
	{
		if (!s_wRoot)
			return -1;

		return s_iLastAlert;
	}

	//------------------------------------------------------------------------------------------------
	static string GetDirectionKey(int octant)
	{
		switch (octant)
		{
			case 0: return "#CTR-Alert_North";
			case 1: return "#CTR-Alert_NorthEast";
			case 2: return "#CTR-Alert_East";
			case 3: return "#CTR-Alert_SouthEast";
			case 4: return "#CTR-Alert_South";
			case 5: return "#CTR-Alert_SouthWest";
			case 6: return "#CTR-Alert_West";
		}

		return "#CTR-Alert_NorthWest";
	}

	//------------------------------------------------------------------------------------------------
	protected static void Build(notnull Widget parent, string title, string line, Color color, string icon)
	{
		s_wRoot = CreateWidget(WidgetType.OverlayWidgetTypeID, Color.FromInt(Color.WHITE), parent);
		FrameSlot.SetAnchorMin(s_wRoot, 0.5, 0);
		FrameSlot.SetAnchorMax(s_wRoot, 0.5, 0);
		FrameSlot.SetAlignment(s_wRoot, 0.5, 0);
		FrameSlot.SetPos(s_wRoot, 0, TOP_MARGIN);
		FrameSlot.SetSizeToContent(s_wRoot, true);

		Widget fill = CreateWidget(WidgetType.ImageWidgetTypeID, Color.FromSRGBA(20, 22, 25, 220), s_wRoot);
		AlignableSlot.SetHorizontalAlign(fill, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetVerticalAlign(fill, LayoutVerticalAlign.Stretch);
		ImageWidget accent = ImageWidget.Cast(CreateWidget(WidgetType.ImageWidgetTypeID, color, s_wRoot));
		AlignableSlot.SetHorizontalAlign(accent, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetVerticalAlign(accent, LayoutVerticalAlign.Top);
		accent.SetSize(1, ACCENT_HEIGHT);

		Widget column = CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), s_wRoot);
		AlignableSlot.SetPadding(column, 32, 10 + ACCENT_HEIGHT, 32, 12);

		Widget titleRow = CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), column);
		AlignableSlot.SetHorizontalAlign(titleRow, LayoutHorizontalAlign.Center);
		if (!icon.IsEmpty())
			CTR_OperationTimerHud.AddIcon(titleRow, icon, color, TITLE_FONT_SIZE);

		TextWidget titleText = CreateText(titleRow, TITLE_FONT_SIZE, color);
		titleText.SetText(title);

		TextWidget lineText = CreateText(column, LINE_FONT_SIZE, Color.FromSRGBA(220, 222, 226, 255));
		lineText.SetText(line);
		AlignableSlot.SetHorizontalAlign(lineText, LayoutHorizontalAlign.Center);
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
}
