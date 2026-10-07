//! COE2 bug (fix proposed upstream: blackwaterbread/COE2_AR, branch fix-dedicated-client-issues): COE_GameMode.OnGameStart
//! runs on every machine and, on a client, resets the replicated game state to INTERMISSION, clears the next AO params
//! and picks a main base. A player joining during an operation could then see it as not started (e.g. Deploy refused)
//! until the state changed again. On clients the replicated values are put back after COE2's OnGameStart. Harmless once
//! COE2 is fixed (nothing changes them then); remove then.
modded class COE_GameMode
{
	//------------------------------------------------------------------------------------------------
	override protected void OnGameStart()
	{
		if (Replication.IsServer())
		{
			super.OnGameStart();
			return;
		}

		COE_EGameModeState state = m_eCOE_CurrentState;
		vector mainBasePos = m_vMainBasePos;
		array<ref COE_AOParams> nextAOParams = {};
		foreach (COE_AOParams params : m_aNextAOParams)
		{
			nextAOParams.Insert(params);
		}

		super.OnGameStart();

		m_eCOE_CurrentState = state;
		m_vMainBasePos = mainBasePos;
		m_aNextAOParams.Clear();
		foreach (COE_AOParams params : nextAOParams)
		{
			m_aNextAOParams.Insert(params);
		}
	}
}
