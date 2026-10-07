//! Keeps the gear a player last wore across sessions (server). The worn and carried items of a living player are saved
//! every minute and when they leave; their next spawn puts that gear on instead of the starter kit. Death clears it.
//! Saved with exact state (ammo, damage, issued marks) as a property of the owner's Marx stash, in the format of the
//! Marx loadouts.
//!
//! A body the vanilla reconnect keeps after a lost connection stays in the world: if the player does not come back in
//! time, what is left on it then is saved (or nothing, if it died), so gear looted from it meanwhile is not given twice.
class CTR_LastGear : Managed
{
	static const string PROPERTY_KEY = "ctr.lastgear";
	static const string LEDGER_SOURCE = "ctr_lastgear";
	protected static const int SAVE_INTERVAL_MS = 60000;
	protected static const int IDENTITY_POLL_MS = 500;
	protected static const int IDENTITY_TIMEOUT_MS = 60000;

	//! Gear by owner ID, loaded when the owner is ready: the spawn reads it synchronously.
	protected ref map<string, ref MRX_SavedLoadout> m_mGear = new map<string, ref MRX_SavedLoadout>();
	//! Last value written (or read) per owner, to skip unchanged saves; "" = cleared.
	protected ref map<string, string> m_mWritten = new map<string, string>();
	//! Bodies kept for a reconnect (weak) and their owners, same index.
	protected ref array<IEntity> m_aReservedBodies = {};
	protected ref array<string> m_aReservedOwners = {};
	protected int m_iIdentityWaitedMs;

	//------------------------------------------------------------------------------------------------
	void Start(notnull SCR_BaseGameMode gameMode)
	{
		gameMode.GetOnPlayerKilled().Insert(OnPlayerKilled);
		AttachIdentity();
		GetGame().GetCallqueue().CallLater(SaveAll, SAVE_INTERVAL_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	void ~CTR_LastGear()
	{
		// The call queue is gone when the game shuts down.
		ScriptCallQueue callQueue = GetGame().GetCallqueue();
		if (callQueue)
		{
			callQueue.Remove(SaveAll);
			callQueue.Remove(AttachIdentity);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Loads the gear of each owner when it becomes known. The Marx identity service is created after the game mode
	//! starts: waits for it, then also loads the owners already known.
	protected void AttachIdentity()
	{
		MRX_IdentityService identity = MRX_Marx.GetIdentity();
		if (!identity)
		{
			m_iIdentityWaitedMs += IDENTITY_POLL_MS;
			if (m_iIdentityWaitedMs < IDENTITY_TIMEOUT_MS)
				GetGame().GetCallqueue().CallLater(AttachIdentity, IDENTITY_POLL_MS);
			else
				Print("[CTR] No Marx identity service: the last gear of players is not loaded", LogLevel.ERROR);

			return;
		}

		identity.GetOnOwnerReady().Insert(OnOwnerReady);
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			Load(MRX_Marx.GetOwnerId(playerId));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the player's last gear on a character that is about to be handed to them.
	//! \return False when there is none (or it holds nothing); the caller gives the starter kit then.
	bool Apply(notnull IEntity entity, int playerId)
	{
		return ApplyOwner(entity, MRX_Marx.GetOwnerId(playerId), playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the owner's last gear on the character (see Apply). \param playerId For the log only.
	bool ApplyOwner(notnull IEntity entity, string ownerId, int playerId = 0)
	{
		MRX_SavedLoadout saved = m_mGear.Get(ownerId);
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!saved || !saved.m_Loadout || saved.m_Loadout.m_aChildren.IsEmpty() || !character || !character.GetCharacterController())
			return false;

		InventoryStorageManagerComponent manager = character.GetCharacterController().GetInventoryStorageManager();
		if (!manager)
			return false;

		if (!MRX_EntitySnapshots.ApplyLoadout(character, saved.m_Loadout, manager))
			Print(string.Format("[CTR] Player %1 (%2): part of the last gear could not be put on", playerId, ownerId), LogLevel.WARNING);

		CTR_StarterKit.EquipWeapon(character);
		Print(string.Format("[CTR] Player %1 (%2) spawned with the gear they last wore", playerId, ownerId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Saves the gear of a player who is leaving. Call before the game mode handles the disconnect.
	void OnPlayerLeaving(int playerId)
	{
		string ownerId = MRX_Marx.GetOwnerId(playerId);
		IEntity character = GetLivingCharacter(playerId);
		if (ownerId.IsEmpty() || !character)
			return;

		Save(ownerId, character);
		m_aReservedBodies.Insert(character);
		m_aReservedOwners.Insert(ownerId);
	}

	//------------------------------------------------------------------------------------------------
	//! The vanilla reconnect gave up on a body: keep what is left on it, or nothing when it died.
	void OnReservedBodyExpired(IEntity body)
	{
		int index = m_aReservedBodies.Find(body);
		if (index < 0 || !body)
			return;

		string ownerId = m_aReservedOwners[index];
		m_aReservedBodies.Remove(index);
		m_aReservedOwners.Remove(index);

		ChimeraCharacter character = ChimeraCharacter.Cast(body);
		if (character && character.GetCharacterController() && character.GetCharacterController().GetLifeState() != ECharacterLifeState.DEAD)
			Save(ownerId, character);
		else
			Clear(ownerId);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnOwnerReady(int playerId, string ownerId)
	{
		Load(ownerId);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPlayerKilled(notnull SCR_InstigatorContextData context)
	{
		int playerId = context.GetVictimPlayerID();
		if (playerId > 0)
			Clear(MRX_Marx.GetOwnerId(playerId));
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the owner's last gear from storage, unless it was read or written before.
	void Load(string ownerId)
	{
		if (ownerId.IsEmpty() || m_mWritten.Contains(ownerId) || !MRX_Marx.GetStash())
			return;

		MRX_Marx.GetStash().List(ownerId, new CTR_LastGearListCallback(this, ownerId));
	}

	//------------------------------------------------------------------------------------------------
	bool HasGear(string ownerId)
	{
		return m_mGear.Contains(ownerId);
	}

	//------------------------------------------------------------------------------------------------
	void OnListed(string ownerId, MRX_EStashStatus status, MRX_StashRecord record)
	{
		// Saved or cleared in the meantime: that is newer than what was read.
		if (status != MRX_EStashStatus.OK || !record || m_mWritten.Contains(ownerId))
			return;

		string json = record.GetProperty(PROPERTY_KEY);
		m_mWritten.Set(ownerId, json);
		MRX_SavedLoadout saved = MRX_LoadoutService.Parse(json);
		if (saved)
			m_mGear.Set(ownerId, saved);
	}

	//------------------------------------------------------------------------------------------------
	protected void SaveAll()
	{
		for (int i = m_aReservedBodies.Count() - 1; i >= 0; i--)
		{
			if (!m_aReservedBodies[i])
			{
				m_aReservedBodies.Remove(i);
				m_aReservedOwners.Remove(i);
			}
		}

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			string ownerId = MRX_Marx.GetOwnerId(playerId);
			IEntity character = GetLivingCharacter(playerId);
			if (!ownerId.IsEmpty() && character)
				Save(ownerId, character);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Saves the character's gear as the owner's last gear.
	void Save(string ownerId, notnull IEntity character)
	{
		MRX_SavedLoadout saved = new MRX_SavedLoadout();
		saved.m_Loadout = MRX_EntitySnapshots.CaptureLoadoutState(character);
		saved.m_sMainItem = MRX_LoadoutService.GetMainItem(character, saved.m_Loadout);
		m_mGear.Set(ownerId, saved);
		Write(ownerId, MRX_LoadoutService.ToJson(saved));
	}

	//------------------------------------------------------------------------------------------------
	void Clear(string ownerId)
	{
		if (ownerId.IsEmpty())
			return;

		m_mGear.Remove(ownerId);
		Write(ownerId, string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	protected void Write(string ownerId, string json)
	{
		if (!MRX_Marx.GetStash() || (m_mWritten.Contains(ownerId) && m_mWritten.Get(ownerId) == json))
			return;

		m_mWritten.Set(ownerId, json);
		MRX_PropertyChange change = MRX_PropertyChange.Create(PROPERTY_KEY, json);
		MRX_TxContext context = MRX_TxContext.Create(LEDGER_SOURCE, "lastgear", "lastgear:" + MRX_Marx.NewId());
		MRX_Marx.GetStash().SetProperty(ownerId, change, context, new CTR_LastGearWriteCallback(this, ownerId));
	}

	//------------------------------------------------------------------------------------------------
	void OnWritten(string ownerId, MRX_StashResult result)
	{
		if (result.m_eStatus == MRX_EStashStatus.OK)
			return;

		// Write again on the next save.
		m_mWritten.Remove(ownerId);
		Print(string.Format("[CTR] Could not save the last gear of %1: %2", ownerId, typename.EnumToString(MRX_EStashStatus, result.m_eStatus)), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! The player's own living character; null while dead, not spawned or possessing another character.
	protected static IEntity GetLivingCharacter(int playerId)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (controller && controller.IsPossessing())
			return null;

		return MRX_LoadoutService.GetCharacter(playerId);
	}
}

//------------------------------------------------------------------------------------------------
class CTR_LastGearListCallback : MRX_StashCallback
{
	protected CTR_LastGear m_LastGear;
	protected string m_sOwnerId;

	//------------------------------------------------------------------------------------------------
	void CTR_LastGearListCallback(CTR_LastGear lastGear, string ownerId)
	{
		m_LastGear = lastGear;
		m_sOwnerId = ownerId;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_EStashStatus status, MRX_StashRecord record)
	{
		if (m_LastGear)
			m_LastGear.OnListed(m_sOwnerId, status, record);
	}
}

//------------------------------------------------------------------------------------------------
class CTR_LastGearWriteCallback : MRX_StashResultCallback
{
	protected CTR_LastGear m_LastGear;
	protected string m_sOwnerId;

	//------------------------------------------------------------------------------------------------
	void CTR_LastGearWriteCallback(CTR_LastGear lastGear, string ownerId)
	{
		m_LastGear = lastGear;
		m_sOwnerId = ownerId;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_StashResult result)
	{
		if (m_LastGear)
			m_LastGear.OnWritten(m_sOwnerId, result);
	}
}
