//! One player session in an operation (server).
class CTR_Participant : Managed
{
	int m_iPlayerId;
	string m_sOwnerId;
	ref CTR_PlayerStats m_Stats = new CTR_PlayerStats();
	int m_iFirstSeen;
	int m_iLastSeen;
}

//------------------------------------------------------------------------------------------------
//! An operation from AO generation until its tasks are finished or it is cancelled (server).
//! Tracks who enters an AO and counts the vanilla data collector stats that players gain meanwhile.
class CTR_Operation : Managed
{
	protected static const int TRACK_INTERVAL_MS = 2000;

	protected string m_sId;
	protected int m_iStartTime;
	protected int m_iEndTime;
	protected bool m_bClosed;
	//! With the crimes module, stats arrive twice: first as temporary, later again when committed. Count the first.
	protected bool m_bCountTemporaryStats;
	protected ref map<int, ref CTR_Participant> m_mParticipants = new map<int, ref CTR_Participant>();

	//------------------------------------------------------------------------------------------------
	void CTR_Operation(string id)
	{
		m_sId = id;
		m_iStartTime = System.GetUnixTime();
	}

	//------------------------------------------------------------------------------------------------
	void ~CTR_Operation()
	{
		StopTracking();
	}

	//------------------------------------------------------------------------------------------------
	string GetId()
	{
		return m_sId;
	}

	//------------------------------------------------------------------------------------------------
	bool IsClosed()
	{
		return m_bClosed;
	}

	//------------------------------------------------------------------------------------------------
	int GetDurationSeconds()
	{
		if (m_bClosed)
			return m_iEndTime - m_iStartTime;

		return System.GetUnixTime() - m_iStartTime;
	}

	//------------------------------------------------------------------------------------------------
	void StartTracking()
	{
		SCR_DataCollectorComponent collector = GetGame().GetDataCollector();
		m_bCountTemporaryStats = collector && collector.FindModule(SCR_DataCollectorCrimesModule) != null;
		SCR_PlayerData.s_OnStatAdded.Insert(OnStatAdded);
		GetGame().GetCallqueue().CallLater(Track, TRACK_INTERVAL_MS, true);
		Track();
	}

	//------------------------------------------------------------------------------------------------
	//! Stops counting; the stats stay as they are.
	void Close()
	{
		if (m_bClosed)
			return;

		Track();
		StopTracking();
		m_bClosed = true;
		m_iEndTime = System.GetUnixTime();
	}

	//------------------------------------------------------------------------------------------------
	protected void StopTracking()
	{
		SCR_PlayerData.s_OnStatAdded.Remove(OnStatAdded);
		GetGame().GetCallqueue().Remove(Track);
	}

	//------------------------------------------------------------------------------------------------
	//! Sessions by player ID; several may share an owner after reconnects.
	map<int, ref CTR_Participant> GetParticipants()
	{
		return m_mParticipants;
	}

	//------------------------------------------------------------------------------------------------
	CTR_Participant GetOrAddParticipant(int playerId, int now)
	{
		CTR_Participant participant = m_mParticipants.Get(playerId);
		if (participant)
			return participant;

		participant = new CTR_Participant();
		participant.m_iPlayerId = playerId;
		participant.m_iFirstSeen = now;
		participant.m_iLastSeen = now;
		m_mParticipants.Insert(playerId, participant);
		return participant;
	}

	//------------------------------------------------------------------------------------------------
	//! Updates owner, name and time of connected players and marks those inside an AO.
	protected void Track()
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode)
			return;

		array<COE_AO> aos = gameMode.GetCurrentAOs();
		float radius = gameMode.GetAORadius();
		int now = System.GetUnixTime();
		PlayerManager playerManager = GetGame().GetPlayerManager();
		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);

		foreach (int playerId : playerIds)
		{
			CTR_Participant participant = GetOrAddParticipant(playerId, now);
			participant.m_iLastSeen = now;
			participant.m_Stats.m_iSeconds = now - participant.m_iFirstSeen;
			participant.m_Stats.m_sName = playerManager.GetPlayerName(playerId);

			string ownerId = MRX_Marx.GetOwnerId(playerId);
			if (!ownerId.IsEmpty())
				participant.m_sOwnerId = ownerId;

			if (participant.m_Stats.m_bEnteredAO)
				continue;

			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(playerManager.GetPlayerControlledEntity(playerId));
			if (!character || character.GetCharacterController().GetLifeState() == ECharacterLifeState.DEAD)
				continue;

			if (IsInAnyAO(character.GetOrigin(), aos, radius))
				participant.m_Stats.m_bEnteredAO = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	static bool IsInAnyAO(vector pos, array<COE_AO> aos, float radius)
	{
		if (!aos)
			return false;

		foreach (COE_AO ao : aos)
		{
			if (ao && vector.DistanceXZ(pos, ao.GetOrigin()) <= radius)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnStatAdded(int playerId, SCR_EDataStats stat, float amount, bool temp)
	{
		if (m_bClosed || temp != m_bCountTemporaryStats)
			return;

		CTR_Participant participant = GetOrAddParticipant(playerId, System.GetUnixTime());
		AddStat(participant.m_Stats, stat, amount);
	}

	//------------------------------------------------------------------------------------------------
	//! Maps a vanilla data collector stat to the operation stats.
	static void AddStat(notnull CTR_PlayerStats stats, SCR_EDataStats stat, float amount)
	{
		int count = Math.Round(amount);
		if (stat == SCR_EDataStats.KILLS || stat == SCR_EDataStats.AI_KILLS || stat == SCR_EDataStats.ROADKILLS || stat == SCR_EDataStats.AI_ROADKILLS)
			stats.m_iKills += count;
		else if (stat == SCR_EDataStats.FRIENDLY_KILLS || stat == SCR_EDataStats.FRIENDLY_AI_KILLS || stat == SCR_EDataStats.FRIENDLY_ROADKILLS || stat == SCR_EDataStats.FRIENDLY_AI_ROADKILLS)
			stats.m_iTeamKills += count;
		else if (stat == SCR_EDataStats.DEATHS)
			stats.m_iDeaths += count;
		else if (stat == SCR_EDataStats.SHOTS)
			stats.m_iShots += count;
		else if (stat == SCR_EDataStats.BANDAGE_FRIENDLIES || stat == SCR_EDataStats.TOURNIQUET_FRIENDLIES || stat == SCR_EDataStats.SALINE_FRIENDLIES || stat == SCR_EDataStats.MORPHINE_FRIENDLIES)
			stats.m_iHeals += count;
		else if (stat == SCR_EDataStats.DISTANCE_WALKED || stat == SCR_EDataStats.DISTANCE_DRIVEN || stat == SCR_EDataStats.DISTANCE_AS_OCCUPANT)
			stats.m_fDistance += amount;
	}
}
