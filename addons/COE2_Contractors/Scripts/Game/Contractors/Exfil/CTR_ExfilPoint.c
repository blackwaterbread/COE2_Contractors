//! Where the commander may put the exfil point (every machine): far enough from the AOs that the exfil is a real
//! move, near enough to reach it in time (CTR_ExfilRules.CheckDistance), and not moved once the exfil started.
class CTR_ExfilPoint
{
	//------------------------------------------------------------------------------------------------
	//! Distance check against the AOs the commander picked (or that run: COE2 keeps them in the next AO params).
	static CTR_EExfilDistance Check(vector pos)
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode)
			return CTR_EExfilDistance.OK;

		array<vector> centers = {};
		foreach (COE_AOParams params : gameMode.GetNextAOParams())
		{
			if (params && params.GetLocation())
				centers.Insert(params.GetLocation().m_vCenter);
		}

		CTR_Settings settings = CTR_Settings.Get();
		return CTR_ExfilRules.CheckDistance(pos, centers, gameMode.GetAORadius(), settings.m_fExfilMinDistance, settings.m_fExfilMaxDistance);
	}

	//------------------------------------------------------------------------------------------------
	//! The exfil started in the running AO: the point stays where it is.
	static bool IsLocked()
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		return gameMode && gameMode.CTR_IsExfilPointLocked() && gameMode.COE_GetState() == COE_EGameModeState.EXECUTION;
	}

	//------------------------------------------------------------------------------------------------
	//! Reason text for a distance that is not OK.
	static string GetReason(CTR_EExfilDistance distance)
	{
		if (distance == CTR_EExfilDistance.TOO_FAR)
			return "#CTR-Reason_ExfilTooFar";

		return "#CTR-Reason_ExfilTooClose";
	}

	//------------------------------------------------------------------------------------------------
	//! Reason the AO cannot be generated with the current exfil point, or empty.
	static string GetGenerateReason()
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		vector pos;
		if (!gameMode || !gameMode.CTR_GetReplicatedExfilPoint(pos))
			return string.Empty;

		CTR_EExfilDistance distance = Check(pos);
		if (distance == CTR_EExfilDistance.OK)
			return string.Empty;

		return GetReason(distance);
	}
}

//------------------------------------------------------------------------------------------------
//! The map menu entry is off where the exfil point is not allowed, and once the exfil started. Modded config classes
//! repeat the attributes of the original: without them configs cannot create them.
[BaseContainerProps(configRoot: true), SCR_BaseContainerCustomTitleUIInfo("Name")]
modded class COE_SetExfilPointRadialMenuEntry
{
	protected bool m_bCTR_DescriptionRead;
	protected string m_sCTR_Description;

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedOnMap(vector position, COE_MapUIElement selectedMapElement)
	{
		if (!m_bCTR_DescriptionRead)
		{
			m_sCTR_Description = Description;
			m_bCTR_DescriptionRead = true;
		}

		string reason;
		if (CTR_ExfilPoint.IsLocked())
		{
			reason = "#CTR-Reason_ExfilLocked";
		}
		else
		{
			CTR_EExfilDistance distance = CTR_ExfilPoint.Check(position);
			if (distance != CTR_EExfilDistance.OK)
				reason = CTR_ExfilPoint.GetReason(distance);
		}

		if (reason.IsEmpty())
		{
			SetDescription(m_sCTR_Description);
			return super.CanBePerformedOnMap(position, selectedMapElement);
		}

		SetDescription(reason);
		return false;
	}
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
modded class COE_GenerateAOCommand
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformed(notnull SCR_ChimeraCharacter user)
	{
		if (!super.CanBePerformed(user))
			return false;

		string reason = CTR_ExfilPoint.GetGenerateReason();
		if (reason.IsEmpty())
			return true;

		m_sCannotPerformReason = reason;
		return false;
	}
}

//------------------------------------------------------------------------------------------------
modded class COE_GenerateAOUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!super.CanBePerformedScript(user))
			return false;

		string reason = CTR_ExfilPoint.GetGenerateReason();
		if (reason.IsEmpty())
			return true;

		m_sCannotPerformReason = reason;
		return false;
	}
}
