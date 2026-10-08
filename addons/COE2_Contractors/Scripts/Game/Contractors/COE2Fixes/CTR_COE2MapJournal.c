//! Vanilla bug that COE2's scenarios hit (not proposed upstream: vanilla code): the map journal registers its tool menu
//! entry only for a scenario with a journal config, which COE2's have not, yet SetJournalVisibility uses that entry and
//! the journal frame without a check. Clicking a task icon on the map calls it (SCR_TaskManagerUIComponent) and throws a
//! script exception. Without a journal there is nothing to show or hide. Remove once vanilla checks them.
//! A modded config class repeats the attributes of the original (here those of SCR_MapUIBaseComponent).
[BaseContainerProps()]
modded class SCR_MapJournalUI
{
	//------------------------------------------------------------------------------------------------
	override void SetJournalVisibility(bool visibility)
	{
		if (!m_ToolMenuEntry || !m_wJournalFrame)
			return;

		super.SetJournalVisibility(visibility);
	}
}
