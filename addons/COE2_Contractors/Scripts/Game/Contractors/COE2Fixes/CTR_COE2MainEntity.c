//! COE2 bug (fix proposed upstream: blackwaterbread/COE2_AR, branch fix-dedicated-client-issues): fast travel reads
//! m_MainEntity, which a client sets from a replicated ID. The ID can arrive after the character came under the
//! player's control, or before the character streamed in (then vanilla does not find it and m_MainEntity stays empty),
//! so Deploy did nothing on a dedicated server. Here an empty m_MainEntity is filled the way vanilla GetMainEntity()
//! answers: the replicated main entity, else the controlled character when not possessing. Harmless once COE2 is fixed
//! (it uses GetMainEntity()); remove then.
modded class COE_PlayerController
{
	//------------------------------------------------------------------------------------------------
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);

		if (to)
			CTR_ResolveMainEntity();
	}

	//------------------------------------------------------------------------------------------------
	override void RequestFastTravel(vector pos, float rotation = 0, float searchRadius = 10)
	{
		CTR_ResolveMainEntity();

		// COE2 reads it without a check; empty until the player spawned through the respawn system.
		if (!m_MainEntity)
			return;

		super.RequestFastTravel(pos, rotation, searchRadius);
	}

	//------------------------------------------------------------------------------------------------
	protected void CTR_ResolveMainEntity()
	{
		if (m_MainEntity)
			return;

		OnRplMainEntityFromID();
		if (!m_MainEntity && !IsPossessing())
			m_MainEntity = GetControlledEntity();
	}
}
