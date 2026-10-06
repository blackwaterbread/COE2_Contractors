//! The vanilla data collector counts treatments of others only for the medical items it knows (bandage, tourniquet,
//! saline, morphine). ACE Medical adds drugs with consumable types of its own (epinephrine, naloxone, ...), which it
//! skips. Reports those so operations pay them like the vanilla ones (server).
modded class SCR_DataCollectorHealingItemsModule
{
	protected static ref ScriptInvokerInt s_CTR_OnUnlistedTreatment;

	//------------------------------------------------------------------------------------------------
	//! Player ID of a player who treated another character with a medical item the data collector does not list.
	static ScriptInvokerInt CTR_GetOnUnlistedTreatment()
	{
		if (!s_CTR_OnUnlistedTreatment)
			s_CTR_OnUnlistedTreatment = new ScriptInvokerInt();

		return s_CTR_OnUnlistedTreatment;
	}

	//------------------------------------------------------------------------------------------------
	override protected void HealingItemUsed(IEntity item, bool ActionCompleted, ItemUseParameters animParams)
	{
		super.HealingItemUsed(item, ActionCompleted, animParams);
		if (!item || !ActionCompleted || !s_CTR_OnUnlistedTreatment)
			return;

		SCR_ConsumableItemComponent consumable = SCR_ConsumableItemComponent.Cast(item.FindComponent(SCR_ConsumableItemComponent));
		if (!consumable)
			return;

		IEntity user = consumable.GetCharacterOwner();
		IEntity target = consumable.GetTargetCharacter();
		if (!user || !target || target == user)
			return;

		SCR_EConsumableType type = consumable.GetConsumableType();
		if (m_aConsumableTypes)
		{
			foreach (ConsumableTypeStats known : m_aConsumableTypes)
			{
				if (known.GetConsumableType() == type)
					return;
			}
		}

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(user);
		if (playerId > 0)
			s_CTR_OnUnlistedTreatment.Invoke(playerId);
	}
}
