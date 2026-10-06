//! "Operation" button in the pause menu while an AO runs (client): opens the operation screen, with the earnings so far
//! during the operation, or the result and the return to base during the loot time.
class CTR_PauseMenu
{
	static const string BUTTON_NAME = "CTR_Operation";
	static const string BUTTON_TEXT = "#CTR-Pause_Operation";
	static const string RETURN_HINT = "#CTR-Pause_ReturnHint";
	protected static const ResourceName BUTTON_LAYOUT = "{9ECCD201BCF07E95}UI/layouts/Menus/PauseMenu/PauseMenuButton.layout";
	protected static const ResourceName ICONS = "{2EFEA2AF1F38E7F0}UI/Textures/Icons/icons_wrapperUI-64.imageset";
	protected static const string ICON = "score";
	protected static const float BUTTON_HEIGHT = 54;

	//------------------------------------------------------------------------------------------------
	//! Adds the button right below "Continue". \return The button, or null when no AO runs.
	static SCR_ButtonTextComponent AddButton(notnull Widget menuRoot)
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode || gameMode.COE_GetState() == COE_EGameModeState.INTERMISSION || !COE_PlayerController.GetInstance())
			return null;

		Widget row = menuRoot.FindAnyWidget("ButtonRow");
		if (!row)
			return null;

		Widget buttonRoot = GetGame().GetWorkspace().CreateWidgets(BUTTON_LAYOUT, row);
		if (!buttonRoot)
			return null;

		buttonRoot.SetName(BUTTON_NAME);
		PlaceAfterContinue(row, buttonRoot);

		// Like the buttons placed in the vanilla pause menu layout.
		SizeLayoutWidget size = SizeLayoutWidget.Cast(buttonRoot.FindAnyWidget("SizeLayout"));
		if (size)
		{
			size.SetHeightOverride(BUTTON_HEIGHT);
			AlignableSlot.SetPadding(size, 7, 1, 0, 1);
		}

		ImageWidget icon = ImageWidget.Cast(buttonRoot.FindAnyWidget("Image0"));
		if (icon)
			icon.LoadImageFromSet(0, ICONS, ICON);

		SCR_ButtonTextComponent button = SCR_ButtonTextComponent.FindButtonTextComponent(buttonRoot);
		if (!button)
			return null;

		button.SetText(BUTTON_TEXT);
		return button;
	}

	//------------------------------------------------------------------------------------------------
	//! Layouts order their children by Z order, then by creation.
	protected static void PlaceAfterContinue(notnull Widget row, notnull Widget button)
	{
		Widget child = row.GetChildren();
		int zOrder = -100;
		while (child && child != button)
		{
			child.SetZOrder(zOrder++);
			if (child.GetName() == "Continue")
				break;

			child = child.GetSibling();
		}

		button.SetZOrder(zOrder);
	}
}

//------------------------------------------------------------------------------------------------
modded class PauseMenuUI
{
	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();
		SCR_ButtonTextComponent button = CTR_PauseMenu.AddButton(GetRootWidget());
		if (button)
			button.m_OnClicked.Insert(CTR_OnOperation);
	}

	//------------------------------------------------------------------------------------------------
	//! The operation screen opens on its own once the server answers.
	protected void CTR_OnOperation()
	{
		Close();
		COE_PlayerController controller = COE_PlayerController.GetInstance();
		if (controller)
			controller.CTR_RequestOperationStatus();
	}
}
