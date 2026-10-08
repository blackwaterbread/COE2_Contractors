//! Commander menu entry "Early exfil", right after COE2's "Cancel AO": starts the exfil although tasks are left, e.g.
//! when a task cannot be finished. Completed tasks then pay in full if the exfil succeeds.
//! Added by script on every machine, at the end of the command list (commands travel as their index), so no COE2 config
//! has to be overridden.
[BaseContainerProps()]
class CTR_EarlyExfilCommand : COE_BaseRadialCommanderCommand
{
	static const string NAME = "CTR_EarlyExfil";
	//! COE2's command it is shown after.
	static const string AFTER = "COE_CancelAO";

	//------------------------------------------------------------------------------------------------
	void CTR_EarlyExfilCommand()
	{
		m_sCommandName = NAME;
		m_sCommandDisplayName = "#CTR-Command_EarlyExfil";
		m_sImageset = "{2EFEA2AF1F38E7F0}UI/Textures/Icons/icons_wrapperUI-64.imageset";
		m_sIconName = "exit";
	}

	//------------------------------------------------------------------------------------------------
	//! The server runs it first, then every client; only the server acts.
	override bool Execute(IEntity cursorTarget, IEntity groupEnt, vector targetPosition, int playerID, bool isClient)
	{
		if (isClient)
			return true;

		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (gameMode && gameMode.IsCommander(playerID) && gameMode.CTR_StartEarlyExfil())
			gameMode.CTR_AlertAll(CTR_EAlert.EARLY_EXFIL, 0);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformed(notnull SCR_ChimeraCharacter user)
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode)
			return false;

		if (!gameMode.CTR_HasOperation())
		{
			m_sCannotPerformReason = "#COE-Reason_NoAOGenerated";
			return false;
		}

		if (gameMode.CTR_IsInExfil() || gameMode.CTR_IsOperationClosed())
		{
			m_sCannotPerformReason = "#CTR-Reason_ExfilStarted";
			return false;
		}

		if (!gameMode.HasExfilPoint())
		{
			m_sCannotPerformReason = "#COE-Reason_NoExfilPoint";
			return false;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_CommandingManagerComponent
{
	//------------------------------------------------------------------------------------------------
	override void InitiateCommandMaps()
	{
		super.InitiateCommandMaps();

		if (!m_aCommands || m_mNameCommand.Contains(CTR_EarlyExfilCommand.NAME))
			return;

		CTR_EarlyExfilCommand command = new CTR_EarlyExfilCommand();
		m_aCommands.Insert(command);
		m_mNameCommand.Insert(command.GetCommandName(), command);
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerControllerCommandingComponent
{
	//------------------------------------------------------------------------------------------------
	//! The menu config is loaded anew whenever the menu opens; the entry goes in after "Cancel AO" every time.
	override protected void UpdateRadialMenu(IEntity owner, bool isOpen)
	{
		if (m_CommandingMenuConfig && isOpen)
			CTR_AddAfter(m_CommandingMenuConfig.GetRootCategory(), CTR_EarlyExfilCommand.AFTER, CTR_EarlyExfilCommand.NAME);

		super.UpdateRadialMenu(owner, isOpen);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the category (or one inside it) holds the command named after.
	protected static bool CTR_AddAfter(SCR_PlayerCommandingMenuCategoryElement category, string after, string name)
	{
		if (!category || !category.GetCategoryElements())
			return false;

		array<ref SCR_PlayerCommandingMenuBaseElement> elements = category.GetCategoryElements();
		foreach (SCR_PlayerCommandingMenuBaseElement existing : elements)
		{
			SCR_PlayerCommandingMenuCommand existingCommand = SCR_PlayerCommandingMenuCommand.Cast(existing);
			if (existingCommand && existingCommand.GetCommandName() == name)
				return true;
		}

		foreach (int i, SCR_PlayerCommandingMenuBaseElement element : elements)
		{
			SCR_PlayerCommandingMenuCommand command = SCR_PlayerCommandingMenuCommand.Cast(element);
			if (command && command.GetCommandName() == after)
			{
				SCR_PlayerCommandingMenuCommand added = new SCR_PlayerCommandingMenuCommand();
				added.SetCommandName(name);
				elements.InsertAt(added, i + 1);
				return true;
			}

			if (CTR_AddAfter(SCR_PlayerCommandingMenuCategoryElement.Cast(element), after, name))
				return true;
		}

		return false;
	}
}
