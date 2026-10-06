//! Countdown of the loot time on the HUD of the local player: shown while the result screen is closed, until everyone
//! left in the AO is moved back to base. Same look as the balance panel of the Marx arsenal.
class CTR_ReturnCountdownHud : Managed
{
	protected static const int UPDATE_MS = 250;
	protected static const float TOP_MARGIN = 140;
	protected static const ResourceName BOLD_FONT = "{EABA4FE9D014CCEF}UI/Fonts/RobotoCondensed/RobotoCondensed_Bold.fnt";
	protected static const int TITLE_FONT_SIZE = 15;
	protected static const int TIME_FONT_SIZE = 30;
	protected static const int HINT_FONT_SIZE = 14;
	protected static const float ACCENT_WIDTH = 4;

	protected Widget m_wRoot;
	protected TextWidget m_wTime;
	//! Weak: the controller owns this countdown.
	protected COE_PlayerController m_Controller;

	//------------------------------------------------------------------------------------------------
	//! \return Null without loot time left or before the vanilla HUD exists.
	static CTR_ReturnCountdownHud Create(notnull COE_PlayerController controller)
	{
		if (controller.CTR_GetReturnSecondsLeft() <= 0)
			return null;

		SCR_HUDManagerComponent hudManager = SCR_HUDManagerComponent.GetHUDManager();
		if (!hudManager || !hudManager.GetHUDRootWidget())
			return null;

		CTR_ReturnCountdownHud hud = new CTR_ReturnCountdownHud();
		hud.m_Controller = controller;
		hud.Build(hudManager.GetHUDRootWidget());
		hud.Update();
		GetGame().GetCallqueue().CallLater(hud.Update, UPDATE_MS, true);
		return hud;
	}

	//------------------------------------------------------------------------------------------------
	void ~CTR_ReturnCountdownHud()
	{
		ScriptCallQueue callQueue = GetGame().GetCallqueue();
		if (callQueue)
			callQueue.Remove(Update);

		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();
	}

	//------------------------------------------------------------------------------------------------
	bool IsShown()
	{
		return m_wRoot && m_wRoot.IsVisible();
	}

	//------------------------------------------------------------------------------------------------
	protected void Update()
	{
		if (!m_wRoot)
			return;

		int remaining;
		if (m_Controller)
			remaining = m_Controller.CTR_GetReturnSecondsLeft();

		if (remaining <= 0)
		{
			m_wRoot.SetVisible(false);
			GetGame().GetCallqueue().Remove(Update);
			return;
		}

		m_wTime.SetText(CTR_ResultDialog.FormatDuration(remaining));
		// The result screen shows its own countdown.
		m_wRoot.SetVisible(!CTR_ResultDialog.GetOpen());
	}

	//------------------------------------------------------------------------------------------------
	protected void Build(notnull Widget parent)
	{
		m_wRoot = CreateWidget(WidgetType.OverlayWidgetTypeID, Color.FromInt(Color.WHITE), parent);
		FrameSlot.SetAnchorMin(m_wRoot, 0.5, 0);
		FrameSlot.SetAnchorMax(m_wRoot, 0.5, 0);
		FrameSlot.SetAlignment(m_wRoot, 0.5, 0);
		FrameSlot.SetPos(m_wRoot, 0, TOP_MARGIN);
		FrameSlot.SetSizeToContent(m_wRoot, true);

		Stretch(CreateWidget(WidgetType.ImageWidgetTypeID, Color.FromSRGBA(62, 66, 72, 255), m_wRoot));
		Widget fill = CreateWidget(WidgetType.ImageWidgetTypeID, Color.FromSRGBA(20, 22, 25, 240), m_wRoot);
		Stretch(fill);
		AlignableSlot.SetPadding(fill, 1, 1, 1, 1);
		ImageWidget accent = ImageWidget.Cast(CreateWidget(WidgetType.ImageWidgetTypeID, GetAccentColor(), m_wRoot));
		AlignableSlot.SetHorizontalAlign(accent, LayoutHorizontalAlign.Left);
		AlignableSlot.SetVerticalAlign(accent, LayoutVerticalAlign.Stretch);
		accent.SetSize(ACCENT_WIDTH, 1);

		Widget column = CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, Color.FromInt(Color.WHITE), m_wRoot);
		AlignableSlot.SetPadding(column, 20 + ACCENT_WIDTH, 8, 20, 10);

		TextWidget title = CreateText(column, TITLE_FONT_SIZE, GetAccentColor());
		title.SetText("RETURNING TO BASE");
		AlignableSlot.SetHorizontalAlign(title, LayoutHorizontalAlign.Center);
		m_wTime = CreateText(column, TIME_FONT_SIZE, Color.FromInt(Color.WHITE));
		AlignableSlot.SetHorizontalAlign(m_wTime, LayoutHorizontalAlign.Center);
		TextWidget hint = CreateText(column, HINT_FONT_SIZE, Color.FromSRGBA(160, 160, 160, 255));
		hint.SetText(CTR_PauseMenu.RETURN_HINT);
		AlignableSlot.SetHorizontalAlign(hint, LayoutHorizontalAlign.Center);
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
	protected static Color GetAccentColor()
	{
		return Color.FromSRGBA(226, 167, 79, 255);
	}
}
