void CTR_OperationResultMethod(CTR_OperationResult result);
typedef func CTR_OperationResultMethod;

//------------------------------------------------------------------------------------------------
modded class COE_PlayerController
{
	protected static ref ScriptInvokerBase<CTR_OperationResultMethod> s_CTR_OnOperationResult;
	protected static ref CTR_OperationResult s_CTR_LastResult;

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
	}

	//------------------------------------------------------------------------------------------------
	//! On the Contractors return to base, players in a vehicle travel with the vehicle instead.
	override void RequestFastTravel(vector pos, float rotation = 0, float searchRadius = 10)
	{
		// COE2 reads m_MainEntity without a check; it is empty until the player spawned through the respawn system.
		if (!m_MainEntity || CTR_RidesHome())
			return;

		super.RequestFastTravel(pos, rotation, searchRadius);
	}

	//------------------------------------------------------------------------------------------------
	protected bool CTR_RidesHome()
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode || !gameMode.CTR_IsReturning())
			return false;

		ChimeraCharacter character = ChimeraCharacter.Cast(m_MainEntity);
		return character && character.IsInVehicle();
	}
}
