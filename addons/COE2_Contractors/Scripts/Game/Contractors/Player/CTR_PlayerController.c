void CTR_OperationResultMethod(CTR_OperationResult result);
typedef func CTR_OperationResultMethod;

//------------------------------------------------------------------------------------------------
modded class COE_PlayerController
{
	protected static ref ScriptInvokerBase<CTR_OperationResultMethod> s_CTR_OnOperationResult;
	protected static ref CTR_OperationResult s_CTR_LastResult;
	//! Client: the operation timer on the HUD.
	protected ref CTR_OperationTimerHud m_CTR_TimerHud;
	//! Server: last operation screen request of this player, against floods.
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
	//! Client: the operation timer of the local player, or null before their first spawn.
	CTR_OperationTimerHud CTR_GetTimerHud()
	{
		return m_CTR_TimerHud;
	}

	//------------------------------------------------------------------------------------------------
	bool CTR_HasMainEntity()
	{
		return m_MainEntity != null;
	}

	//------------------------------------------------------------------------------------------------
	//! The operation timer is built on the HUD once the local player controls a character.
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);

		if (to && this == GetGame().GetPlayerController() && (!m_CTR_TimerHud || !m_CTR_TimerHud.IsBuilt()))
			m_CTR_TimerHud = CTR_OperationTimerHud.Create();
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
		CTR_ResultDialog.Open(result);
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
			SCR_HintManagerComponent.ShowCustomHint("#CTR-Hint_NoOperation", "#CTR-Pause_Operation", 4);
			return;
		}

		CTR_OperationResult result = CTR_OperationResult.FromJson(json);
		if (result)
			CTR_ResultDialog.Open(result);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: shows an alert at the top of the owner's screen.
	void CTR_SendAlert(CTR_EAlert alert, int param)
	{
		Rpc(CTR_RpcDo_Alert, alert, param);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void CTR_RpcDo_Alert(int alert, int param)
	{
		CTR_AlertHud.Show(alert, param);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: shows a short hint to the owner (a localization key).
	void CTR_SendHint(string text)
	{
		Rpc(CTR_RpcDo_Hint, text);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void CTR_RpcDo_Hint(string text)
	{
		SCR_HintManagerComponent.ShowCustomHint(text, "#COE-Action_SetExfilPoint", 5);
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
	//! are.
	override void RequestFastTravel(vector pos, float rotation = 0, float searchRadius = 10)
	{
		// An empty m_MainEntity is handled in COE2Fixes/CTR_COE2MainEntity.c.
		if (CTR_StaysPut())
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
