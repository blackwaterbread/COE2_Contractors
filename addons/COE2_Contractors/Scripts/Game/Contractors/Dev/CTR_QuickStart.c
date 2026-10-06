#ifdef WORKBENCH
//! Workbench Play only: when nothing sets the factions (Workbench Play ignores mission headers), the game starts with
//! the factions of the Contractors mission headers (RHS ION players against RHS AFRF, CIV civilians) through COE2's
//! own default faction path, and the host becomes commander. So a hand test deploys at the base right away; the
//! commander can still change the factions in the scenario attributes. Off for the test harness (-ctrTests), whose
//! tests need the host character that COE2 replaces.
modded class COE_GameMode
{
	protected static const int CTR_COMMANDER_RETRY_MS = 1000;
	protected static const int CTR_COMMANDER_MAX_TRIES = 30;
	protected int m_iCTR_CommanderTries;

	//------------------------------------------------------------------------------------------------
	override protected void OnGameStart()
	{
		if (Replication.IsServer() && !System.IsCLIParam(CTR_TestRunner.RUN_PARAM) && m_sDefaultPlayerFactionKey.IsEmpty())
		{
			m_sDefaultPlayerFactionKey = CTR_Factions.PLAYER;
			if (m_sDefaultEnemyFactionKey.IsEmpty())
				m_sDefaultEnemyFactionKey = CTR_Factions.ENEMY;

			if (m_sDefaultCivilianFactionKey.IsEmpty())
				m_sDefaultCivilianFactionKey = CTR_Factions.CIVILIAN;

			Print(string.Format("[CTR] Workbench quick start: %1 against %2; the host becomes commander", CTR_Factions.PLAYER, CTR_Factions.ENEMY));
			GetGame().GetCallqueue().CallLater(CTR_MakeHostCommander, CTR_COMMANDER_RETRY_MS, true);
		}

		super.OnGameStart();
	}

	//------------------------------------------------------------------------------------------------
	//! COE2 makes the host commander on game start, but in Workbench the host has no player ID yet at that point.
	protected void CTR_MakeHostCommander()
	{
		m_iCTR_CommanderTries++;
		int playerId = SCR_PlayerController.GetLocalPlayerId();
		if (playerId <= 0 && m_iCTR_CommanderTries < CTR_COMMANDER_MAX_TRIES)
			return;

		GetGame().GetCallqueue().Remove(CTR_MakeHostCommander);
		if (playerId > 0 && !IsCommander(playerId))
			GetGame().GetPlayerManager().GivePlayerRole(playerId, EPlayerRole.COE_COMMANDER);
	}
}
#endif
