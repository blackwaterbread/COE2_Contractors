void CTR_OperationResultMethod(CTR_OperationResult result);
typedef func CTR_OperationResultMethod;

//------------------------------------------------------------------------------------------------
modded class COE_PlayerController
{
	protected static ref ScriptInvokerBase<CTR_OperationResultMethod> s_CTR_OnOperationResult;
	protected static ref CTR_OperationResult s_CTR_LastResult;
	protected ref CTR_ReturnCountdownHud m_CTR_ReturnHud;
	//! Client: tick when the loot time of the last operation ends; 0 = no return pending.
	protected int m_iCTR_ReturnTick;
	//! Server: last operation screen or return request of this player, against floods.
	protected int m_iCTR_LastRequestTick;

	protected static const int CTR_REQUEST_INTERVAL_MS = 500;

	//------------------------------------------------------------------------------------------------
	//! Client: fires when the local player receives an operation result.
	static ScriptInvokerBase<CTR_OperationResultMethod> CTR_GetOnOperationResult()
	{
		if (!s_CTR_OnOperationResult)
			s_CTR_OnOperationResult = new ScriptInvokerBase<CTR_OperationResultMethod>();

		return s_CTR_OnOperationResult;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the last operation result of the local player, or null.
	static CTR_OperationResult CTR_GetLastResult()
	{
		return s_CTR_LastResult;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the return countdown of the last result, or null.
	CTR_ReturnCountdownHud CTR_GetReturnCountdown()
	{
		return m_CTR_ReturnHud;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: seconds of loot time left before everyone in the AO returns to base, 0 when no return is pending.
	int CTR_GetReturnSecondsLeft()
	{
		if (m_iCTR_ReturnTick == 0)
			return 0;

		// The AO also ends earlier: when nobody is left in it, or when the commander ends it.
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		int remaining = Math.Ceil((m_iCTR_ReturnTick - System.GetTickCount()) / 1000.0);
		if (remaining <= 0 || !gameMode || gameMode.COE_GetState() != COE_EGameModeState.EXECUTION)
		{
			m_iCTR_ReturnTick = 0;
			return 0;
		}

		return remaining;
	}

	//------------------------------------------------------------------------------------------------
	bool CTR_HasMainEntity()
	{
		return m_MainEntity != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sends the result screen data to the owner of this controller.
	void CTR_SendOperationResult(string json)
	{
		Rpc(CTR_RpcDo_OperationResult, json);
	}

	//------------------------------------------------------------------------------------------------
	//! Runs on the owning client, or directly on the server when the server owns this controller (host).
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void CTR_RpcDo_OperationResult(string json)
	{
		CTR_OperationResult result = CTR_OperationResult.FromJson(json);
		if (!result)
		{
			Print("[CTR] Could not read the operation result", LogLevel.ERROR);
			return;
		}

		s_CTR_LastResult = result;
		Print(string.Format("[CTR] Operation result received: %1, pay %2, status %3", result.m_sOperationId, result.m_Payout.m_iTotal, typename.EnumToString(CTR_EPayStatus, result.m_ePayStatus)));
		CTR_GetOnOperationResult().Invoke(result);
		CTR_ShowOperation(result);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: asks for the operation screen (pause menu).
	void CTR_RequestOperationStatus()
	{
		Rpc(CTR_RpcAsk_OperationStatus);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void CTR_RpcAsk_OperationStatus()
	{
		if (!CTR_CheckRequestRate())
			return;

		COE_GameMode gameMode = COE_GameMode.GetInstance();
		CTR_OperationResult status;
		if (gameMode)
			status = gameMode.CTR_BuildStatus(GetPlayerId());

		string json;
		if (status)
			json = status.ToJson();

		Rpc(CTR_RpcDo_OperationStatus, json);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void CTR_RpcDo_OperationStatus(string json)
	{
		if (json.IsEmpty())
		{
			SCR_HintManagerComponent.ShowCustomHint("No operation is running.", "Operation", 4);
			return;
		}

		CTR_OperationResult result = CTR_OperationResult.FromJson(json);
		if (result)
			CTR_ShowOperation(result);
	}

	//------------------------------------------------------------------------------------------------
	//! Opens the operation screen; a result starts or corrects the countdown of the loot time.
	protected void CTR_ShowOperation(notnull CTR_OperationResult result)
	{
		if (!result.m_bInProgress)
		{
			m_iCTR_ReturnTick = 0;
			if (result.m_iReturnDelaySeconds > 0)
				m_iCTR_ReturnTick = System.GetTickCount() + result.m_iReturnDelaySeconds * 1000;

			// Stays on the HUD when the result screen is closed.
			m_CTR_ReturnHud = CTR_ReturnCountdownHud.Create(this);
		}

		CTR_ResultDialog.Open(result);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: asks to return to base during the loot time.
	void CTR_RequestReturnNow()
	{
		Rpc(CTR_RpcAsk_ReturnNow);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void CTR_RpcAsk_ReturnNow()
	{
		if (!CTR_CheckRequestRate())
			return;

		CTR_EReturnStatus status = CTR_EReturnStatus.NOT_NOW;
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (gameMode)
			status = gameMode.CTR_ReturnNow(GetPlayerId());

		Rpc(CTR_RpcDo_ReturnStatus, status);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void CTR_RpcDo_ReturnStatus(int status)
	{
		CTR_ResultDialog dialog = CTR_ResultDialog.GetOpen();
		if (status == CTR_EReturnStatus.OK)
		{
			// Out of the way of the fade to base.
			if (dialog)
				dialog.Close();

			return;
		}

		string text = CTR_GetReturnStatusText(status);
		if (dialog)
			dialog.ShowReturnStatus(text);
		else
			SCR_HintManagerComponent.ShowCustomHint(text, "Return to base", 4);
	}

	//------------------------------------------------------------------------------------------------
	static string CTR_GetReturnStatusText(int status)
	{
		switch (status)
		{
			case CTR_EReturnStatus.NOT_NOW: return "You can only return once the operation is over.";
			case CTR_EReturnStatus.DEAD: return "You are dead: you respawn at the base.";
			case CTR_EReturnStatus.NOT_DRIVER: return "Only the driver takes the vehicle back. Get out to return on your own.";
			case CTR_EReturnStatus.AT_BASE: return "You are already at the base.";
		}

		return "The return to base failed.";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: false when the previous request of this player came too shortly before.
	protected bool CTR_CheckRequestRate()
	{
		int now = System.GetTickCount();
		if (m_iCTR_LastRequestTick != 0 && now - m_iCTR_LastRequestTick < CTR_REQUEST_INTERVAL_MS)
			return false;

		m_iCTR_LastRequestTick = now;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! When the AO ends, players in a vehicle travel with the vehicle, and players already at the base stay where they
	//! are (e.g. those who returned during the loot time).
	override void RequestFastTravel(vector pos, float rotation = 0, float searchRadius = 10)
	{
		// COE2 reads m_MainEntity without a check; it is empty until the player spawned through the respawn system.
		if (!m_MainEntity || CTR_StaysPut())
			return;

		super.RequestFastTravel(pos, rotation, searchRadius);
	}

	//------------------------------------------------------------------------------------------------
	protected bool CTR_StaysPut()
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode || !gameMode.CTR_IsReturning())
			return false;

		ChimeraCharacter character = ChimeraCharacter.Cast(m_MainEntity);
		if (!character)
			return false;

		return character.IsInVehicle() || CTR_ReturnTrip.IsAtBase(character.GetOrigin(), gameMode.GetMainBasePos());
	}
}
