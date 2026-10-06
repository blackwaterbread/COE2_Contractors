//! Safehouse safe zone: within CTR_Settings.m_fSafeZoneRadius of the main base, players cannot fire weapons, throw
//! grenades or fire vehicle turrets (against griefing at the base). Enforced on the machine of the controlling player,
//! which handles its input; COE2 replicates the main base position.
class CTR_SafeZone
{
	protected static const int HINT_INTERVAL_MS = 4000;
	protected static int s_iLastHintTick;

	//------------------------------------------------------------------------------------------------
	//! True when the position is inside the safe zone of the current main base.
	static bool IsInside(vector position)
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		float radius = CTR_Settings.Get().m_fSafeZoneRadius;
		if (!gameMode || radius <= 0)
			return false;

		vector basePosition = gameMode.GetMainBasePos();
		if (basePosition == vector.Zero)
			return false;

		float dx = position[0] - basePosition[0];
		float dz = position[2] - basePosition[2];
		return dx * dx + dz * dz <= radius * radius;
	}

	//------------------------------------------------------------------------------------------------
	//! Tells the player why nothing fired, at most every few seconds.
	static void ShowBlockedHint()
	{
		int now = System.GetTickCount();
		if (s_iLastHintTick != 0 && now - s_iLastHintTick < HINT_INTERVAL_MS)
			return;

		s_iLastHintTick = now;
		SCR_HintManagerComponent.ShowCustomHint("#CTR-Hint_SafeZone", "#CTR-Hint_SafeZoneTitle", 4);
	}
}

//------------------------------------------------------------------------------------------------
//! No firing and no grenade throwing for the player's character in the safe zone.
modded class SCR_CharacterControllerComponent
{
	//------------------------------------------------------------------------------------------------
	override void OnPrepareControls(IEntity owner, ActionManager am, float dt, bool player)
	{
		super.OnPrepareControls(owner, am, dt, player);
		if (!player || !owner || !CTR_SafeZone.IsInside(owner.GetOrigin()))
			return;

		if (am.GetActionValue("CharacterFire") > 0 || am.GetActionTriggered("CharacterThrowGrenade"))
			CTR_SafeZone.ShowBlockedHint();

		am.SetActionValue("CharacterFire", 0);
		am.SetActionValue("CharacterThrowGrenade", 0);
	}
}

//------------------------------------------------------------------------------------------------
//! No turret fire for the player in the safe zone.
modded class SCR_TurretControllerComponent
{
	//------------------------------------------------------------------------------------------------
	override void OnPrepareControls(IEntity owner, ActionManager am, float dt, bool player)
	{
		super.OnPrepareControls(owner, am, dt, player);
		if (!player || !owner || !CTR_SafeZone.IsInside(owner.GetOrigin()))
			return;

		if (am.GetActionValue("TurretFire") > 0)
			CTR_SafeZone.ShowBlockedHint();

		am.SetActionValue("TurretFire", 0);
		SetFireWeaponWanted(false);
	}
}
