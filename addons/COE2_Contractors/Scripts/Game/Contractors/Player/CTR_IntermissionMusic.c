//! COE2 checks 100 ms after the local player spawned whether to play the briefing music, reading the player's character
//! without a check. On a client the character can come under the player's control later than that (a script exception
//! on every spawn at the base); the check then waits until the player controls a character.
modded class COE_IntermissionMusic
{
	//------------------------------------------------------------------------------------------------
	override protected void CheckLocationAndPlay()
	{
		if (SCR_PlayerController.GetLocalMainEntity())
		{
			super.CheckLocationAndPlay();
			return;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
			return;

		controller.m_OnControlledEntityChanged.Remove(CTR_OnControlledEntityChanged);
		controller.m_OnControlledEntityChanged.Insert(CTR_OnControlledEntityChanged);
	}

	//------------------------------------------------------------------------------------------------
	protected void CTR_OnControlledEntityChanged(IEntity from, IEntity to)
	{
		if (!to)
			return;

		CTR_StopWaiting();
		CheckLocationAndPlay();
	}

	//------------------------------------------------------------------------------------------------
	protected void CTR_StopWaiting()
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
			controller.m_OnControlledEntityChanged.Remove(CTR_OnControlledEntityChanged);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete()
	{
		CTR_StopWaiting();
		super.OnDelete();
	}
}
