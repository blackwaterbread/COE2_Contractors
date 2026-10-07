//! Tells Contractors when a body kept for a reconnect is given up, before vanilla deletes it (see CTR_LastGear).
modded class SCR_ReconnectComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void HandleDataExpiery(notnull SCR_ReconnectData data)
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (gameMode)
			gameMode.CTR_OnReservedBodyExpired(data.m_ReservedEntity);

		super.HandleDataExpiery(data);
	}
}
