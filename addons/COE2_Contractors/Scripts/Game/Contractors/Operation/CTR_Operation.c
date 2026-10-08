//! One player session in an operation (server).
class CTR_Participant : Managed
{
	int m_iPlayerId;
	string m_sOwnerId;
	ref CTR_PlayerStats m_Stats = new CTR_PlayerStats();
	int m_iFirstSeen;
	int m_iLastSeen;
	//! Seconds of CPR by patient player ID; every full reward interval counts as a treatment, up to a cap per patient.
	ref map<int, float> m_mCprSeconds = new map<int, float>();
}

//------------------------------------------------------------------------------------------------
//! An operation from AO generation until its tasks are finished or it is cancelled (server).
//! Tracks who enters an AO and counts the vanilla data collector stats that players gain meanwhile, plus time spent on
//! CPR (ACE Medical Circulation). AI kills are counted from the death events of the game mode instead of the vanilla
//! stats, which depend on faction friendliness and team kill settings: civilians could count as nothing or as paid kills.
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
	//! Civilians killed by players inside an AO, by the whole team: they raise the chance of an enemy pursuit.
	protected int m_iCivilianKills;
	protected ref ScriptInvokerVoid m_OnCivilianKilled = new ScriptInvokerVoid();

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
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (gameMode)
			gameMode.GetOnControllableDestroyed().Insert(OnControllableDestroyed);

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
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (gameMode)
			gameMode.GetOnControllableDestroyed().Remove(OnControllableDestroyed);

		// The call queue is gone when the game shuts down.
		ScriptCallQueue callQueue = GetGame().GetCallqueue();
		if (callQueue)
			callQueue.Remove(Track);
	}

	//------------------------------------------------------------------------------------------------
	int GetCivilianKills()
	{
		return m_iCivilianKills;
	}

	//------------------------------------------------------------------------------------------------
	//! Fires after a player killed a civilian inside an AO.
	ScriptInvokerVoid GetOnCivilianKilled()
	{
		return m_OnCivilianKilled;
	}

	//------------------------------------------------------------------------------------------------
	//! Counts a civilian killed by a player inside an AO (also for dev tools).
	void AddCivilianKill()
	{
		m_iCivilianKills++;
		m_OnCivilianKilled.Invoke();
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
	//! Updates owner, name and time of connected players, marks those inside an AO and adds CPR time.
	protected void Track()
	{
		CTR_Settings settings = CTR_Settings.Get();
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

			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(playerManager.GetPlayerControlledEntity(playerId));
			if (!character || character.GetCharacterController().GetLifeState() == ECharacterLifeState.DEAD)
				continue;

			int patientId = GetCprPatient(character);
			if (patientId > 0)
				AddCprSeconds(participant, patientId, TRACK_INTERVAL_MS * 0.001, settings.m_iCprRewardSeconds, settings.m_iCprMaxSecondsPerPatient);

			if (!participant.m_Stats.m_bEnteredAO && IsInAnyAO(character.GetOrigin(), aos, radius))
				participant.m_Stats.m_bEnteredAO = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! eturn Player ID of the patient while the character does CPR (ACE Medical Circulation) on another player whose
	//! heart stopped, else 0. CPR on someone who does not need it, or on AI, pays nothing.
	static int GetCprPatient(notnull SCR_ChimeraCharacter character)
	{
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access)
			return 0;

		BaseCompartmentSlot slot = access.GetCompartment();
		if (!slot)
			return 0;

		ACE_Medical_CPRHelperCompartment helper = ACE_Medical_CPRHelperCompartment.Cast(slot.GetOwner());
		if (!helper)
			return 0;

		ACE_Medical_VitalsComponent vitals = helper.CTR_GetPatientVitals();
		if (!vitals || !(vitals.GetVitalStateID() & (ACE_Medical_EVitalStateID.CARDIAC_ARREST | ACE_Medical_EVitalStateID.RESUSCITATION)))
			return 0;

		IEntity patient = vitals.GetOwner();
		if (!patient || patient == character)
			return 0;

		return GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(patient);
	}

	//------------------------------------------------------------------------------------------------
	//! Adds CPR time on a patient; each full interval counts as one treatment, up to maxSeconds per patient. The cap
	//! keeps CPR on someone who cannot be revived (too much blood lost) from paying forever.
	static void AddCprSeconds(notnull CTR_Participant participant, int patientId, float seconds, int interval, int maxSeconds)
	{
		if (interval <= 0)
			return;

		float previous = participant.m_mCprSeconds.Get(patientId);
		float total = Math.Min(previous + seconds, maxSeconds);
		participant.m_mCprSeconds.Set(patientId, total);
		int before = Math.Floor(previous / interval);
		int after = Math.Floor(total / interval);
		participant.m_Stats.m_iHeals += Math.Max(0, after - before);
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
	//! A player killed an AI character: counted here rather than from the vanilla AI kill stats.
	protected void OnControllableDestroyed(notnull SCR_InstigatorContextData context)
	{
		if (m_bClosed)
			return;

		int killerId = context.GetKillerPlayerID();
		ChimeraCharacter victim = ChimeraCharacter.Cast(context.GetVictimEntity());
		if (killerId <= 0 || context.GetVictimPlayerID() > 0 || !victim)
			return;

		bool civilian;
		COE_FactionManager factionManager = COE_FactionManager.Cast(GetGame().GetFactionManager());
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(victim.FindComponent(FactionAffiliationComponent));
		if (factionManager && affiliation)
		{
			Faction victimFaction = affiliation.GetAffiliatedFaction();
			civilian = victimFaction && victimFaction == factionManager.GetCivilianFaction();
		}

		bool inAO;
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (gameMode)
			inAO = IsInAnyAO(victim.GetOrigin(), gameMode.GetCurrentAOs(), gameMode.GetAORadius());

		CTR_Participant participant = GetOrAddParticipant(killerId, System.GetUnixTime());
		AddAIKill(participant.m_Stats, context.GetVictimKillerRelation(), civilian, inAO);
		if (civilian && inAO)
			AddCivilianKill();
	}

	//------------------------------------------------------------------------------------------------
	//! An AI character killed by a player: enemies pay, friendlies and civilians inside an AO cost like a team kill.
	static void AddAIKill(notnull CTR_PlayerStats stats, SCR_ECharacterDeathStatusRelations relation, bool civilian, bool inAO)
	{
		if (civilian)
		{
			if (inAO)
				stats.m_iTeamKills++;

			return;
		}

		if (relation == SCR_ECharacterDeathStatusRelations.KILLED_BY_ENEMY_PLAYER)
			stats.m_iKills++;
		else if (relation == SCR_ECharacterDeathStatusRelations.KILLED_BY_FRIENDLY_PLAYER)
			stats.m_iTeamKills++;
	}

	//------------------------------------------------------------------------------------------------
	//! Maps a vanilla data collector stat to the operation stats. AI kills are left out (see AddAIKill).
	static void AddStat(notnull CTR_PlayerStats stats, SCR_EDataStats stat, float amount)
	{
		int count = Math.Round(amount);
		if (stat == SCR_EDataStats.KILLS || stat == SCR_EDataStats.ROADKILLS)
			stats.m_iKills += count;
		else if (stat == SCR_EDataStats.FRIENDLY_KILLS || stat == SCR_EDataStats.FRIENDLY_ROADKILLS)
			stats.m_iTeamKills += count;
		else if (stat == SCR_EDataStats.DEATHS)
			stats.m_iDeaths += count;
		else if (stat == SCR_EDataStats.SHOTS)
			stats.m_iShots += count;
		else if (stat == SCR_EDataStats.BANDAGE_FRIENDLIES)
			stats.m_iHeals += count; // Drugs on others pay nothing: a morphine overdose is a prank, not a treatment.
		else if (stat == SCR_EDataStats.DISTANCE_WALKED || stat == SCR_EDataStats.DISTANCE_DRIVEN || stat == SCR_EDataStats.DISTANCE_AS_OCCUPANT)
			stats.m_fDistance += amount;
	}
}
