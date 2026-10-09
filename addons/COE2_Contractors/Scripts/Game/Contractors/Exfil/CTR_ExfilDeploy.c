//! While enough players hold the exfil point, nobody deploys from the base (CTR_Exfil.IsDeployBlocked): one more player
//! outside the base raises the players needed and breaks the hold. The base board action and the radial command are
//! refused with a reason; a deploy sent just before the hold started is stopped when its fast travel would move the
//! player, and the player stays at the base. The return to the base when the AO ends is never stopped: it comes after
//! the exfil ended and goes to the base.
modded class COE_DeployUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (CTR_Exfil.IsDeployBlocked())
		{
			m_sCannotPerformReason = CTR_Exfil.HOLDING_REASON;
			return false;
		}

		return super.CanBePerformedScript(user);
	}
}

//------------------------------------------------------------------------------------------------
//! Modded config classes repeat the attributes of the original: without them configs cannot create them.
[BaseContainerProps()]
modded class COE_DeployCommand
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformed(notnull SCR_ChimeraCharacter user)
	{
		if (CTR_Exfil.IsDeployBlocked())
		{
			m_sCannotPerformReason = CTR_Exfil.HOLDING_REASON;
			return false;
		}

		return super.CanBePerformed(user);
	}
}

//------------------------------------------------------------------------------------------------
modded class COE_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Owner, after the screen faded out.
	override protected void FastTravelTeleport(vector pos, float rotation)
	{
		if (CTR_IsDeployRefused(pos))
		{
			CTR_RpcDo_DeployRefused();
			return;
		}

		super.FastTravelTeleport(pos, rotation);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the owner may have sent it before it learned that the hold started.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	override protected void RpcAsk_Server_FastTravelTeleport(vector transform[4])
	{
		if (CTR_IsDeployRefused(transform[3]))
		{
			Rpc(CTR_RpcDo_DeployRefused);
			return;
		}

		super.RpcAsk_Server_FastTravelTeleport(transform);
	}

	//------------------------------------------------------------------------------------------------
	//! A fast travel out of the base while the exfil point is held.
	protected bool CTR_IsDeployRefused(vector pos)
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		return gameMode && CTR_Exfil.IsDeployBlocked() && !CTR_ReturnTrip.IsAtBase(pos, gameMode.GetMainBasePos());
	}

	//------------------------------------------------------------------------------------------------
	//! Owner: the screen comes back as after a fast travel, with the reason.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void CTR_RpcDo_DeployRefused()
	{
		SCR_ScreenEffectsManager manager = SCR_ScreenEffectsManager.GetScreenEffectsDisplay();
		if (manager)
		{
			SCR_FadeInOutEffect fade = SCR_FadeInOutEffect.Cast(manager.GetEffect(SCR_FadeInOutEffect));
			if (fade)
				fade.FadeOutEffect(false, FAST_TRAVEL_FADE_DURATION);
		}

		SCR_HintManagerComponent.ShowCustomHint(CTR_Exfil.HOLDING_REASON, "#AR-ButtonSelectDeploy", 5);
	}
}
