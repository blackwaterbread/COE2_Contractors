//! COE2 bug (fix proposed upstream: blackwaterbread/COE2_AR, branch fix-dedicated-client-issues): the deploy and building
//! mode actions and commands use the insertion point, the player controller and the building provider without checks.
//! A command comes back from the server after a round trip, by when the insertion point can be gone, and on clients
//! these can be missing for a moment. Here they are checked before COE2's code runs. Harmless once COE2 is fixed;
//! remove then.
class CTR_COE2Commands
{
	//------------------------------------------------------------------------------------------------
	//! The insertion point's building provider, or null.
	static COE_CampaignBuildingProviderComponent GetBuildingProvider()
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode || !gameMode.GetInsertionPoint())
			return null;

		return COE_CampaignBuildingProviderComponent.Cast(gameMode.GetInsertionPoint().FindComponent(COE_CampaignBuildingProviderComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! True when a deploy can reach COE2's fast travel without empty references.
	static bool CanDeploy()
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		return gameMode && gameMode.GetInsertionPoint() && COE_PlayerController.GetInstance();
	}
}

//------------------------------------------------------------------------------------------------
modded class COE_DeployUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (CTR_COE2Commands.CanDeploy())
			super.PerformAction(pOwnerEntity, pUserEntity);
	}
}

//------------------------------------------------------------------------------------------------
modded class COE_DeployCommand
{
	//------------------------------------------------------------------------------------------------
	override bool Execute(IEntity cursorTarget, IEntity groupEnt, vector targetPosition, int playerID, bool isClient)
	{
		if (playerID == SCR_PlayerController.GetLocalPlayerId() && !CTR_COE2Commands.CanDeploy())
			return false;

		return super.Execute(cursorTarget, groupEnt, targetPosition, playerID, isClient);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformed(notnull SCR_ChimeraCharacter user)
	{
		if (!SCR_PlayerController.GetLocalControlledEntity())
			return false;

		return super.CanBePerformed(user);
	}
}

//------------------------------------------------------------------------------------------------
modded class COE_OpenBuildingModeUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (CTR_COE2Commands.GetBuildingProvider())
			super.PerformAction(pOwnerEntity, pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		// Without an insertion point COE2 gives its own reason; with one but no provider it would throw.
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (gameMode && gameMode.GetInsertionPoint() && !CTR_COE2Commands.GetBuildingProvider())
			return false;

		return super.CanBePerformedScript(user);
	}
}

//------------------------------------------------------------------------------------------------
modded class COE_OpenBuildingModeCommand
{
	//------------------------------------------------------------------------------------------------
	override bool Execute(IEntity cursorTarget, IEntity groupEnt, vector targetPosition, int playerID, bool isClient)
	{
		if (playerID == SCR_PlayerController.GetLocalPlayerId() && !CTR_COE2Commands.GetBuildingProvider())
			return false;

		return super.Execute(cursorTarget, groupEnt, targetPosition, playerID, isClient);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformed(notnull SCR_ChimeraCharacter user)
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (gameMode && gameMode.GetInsertionPoint() && !CTR_COE2Commands.GetBuildingProvider())
			return false;

		return super.CanBePerformed(user);
	}
}
