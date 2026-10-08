//! The exfil of a running operation (server): from the end of the last task, or the commander's early exfil order,
//! until enough players hold the exfil point, the countdown runs out or the commander cancels.
//! Judged here instead of by the trigger of COE2's exfil task, which counts everyone connected: dead players, players at
//! the base and AFK players could keep the others from leaving. Here only living players outside the base count.
class CTR_Exfil : Managed
{
	protected static const int CHECK_INTERVAL_MS = 2000;

	protected vector m_vPos;
	protected float m_fRadius;
	protected float m_fRatio;
	protected int m_iHoldSeconds;
	protected bool m_bHolding;
	protected WorldTimestamp m_HoldEnd;
	//! Owners who left the game outside the base during the exfil, and the bodies they left (weak, same index).
	protected ref array<string> m_aLeftOwners = {};
	protected ref array<IEntity> m_aLeftBodies = {};
	protected ref CTR_Pursuit m_Pursuit;

	//------------------------------------------------------------------------------------------------
	void CTR_Exfil(vector pos, notnull CTR_Settings settings)
	{
		m_vPos = pos;
		m_fRadius = settings.m_fExfilRadius;
		m_fRatio = settings.m_fExfilPlayerRatio;
		m_iHoldSeconds = settings.m_iExfilHoldSeconds;
		m_Pursuit = new CTR_Pursuit(pos, settings);
	}

	//------------------------------------------------------------------------------------------------
	CTR_Pursuit GetPursuit()
	{
		return m_Pursuit;
	}

	//------------------------------------------------------------------------------------------------
	void ~CTR_Exfil()
	{
		Stop();
	}

	//------------------------------------------------------------------------------------------------
	vector GetPos()
	{
		return m_vPos;
	}

	//------------------------------------------------------------------------------------------------
	void Start()
	{
		GetGame().GetCallqueue().CallLater(Check, CHECK_INTERVAL_MS, true);
		Check();
	}

	//------------------------------------------------------------------------------------------------
	//! Stops the checks and the pursuit's waves.
	void Stop()
	{
		if (m_Pursuit)
			m_Pursuit.Stop();

		// The call queue is gone when the game shuts down.
		ScriptCallQueue callQueue = GetGame().GetCallqueue();
		if (callQueue)
			callQueue.Remove(Check);
	}

	//------------------------------------------------------------------------------------------------
	//! Counts the players at the exfil point; reports when they held it long enough, or when the countdown ran out.
	void Check()
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		ChimeraWorld world = GetGame().GetWorld();
		if (!gameMode || !world)
			return;

		array<SCR_ChimeraCharacter> outside = {};
		CollectOutsideBase(gameMode.GetMainBasePos(), outside);
		int present;
		foreach (SCR_ChimeraCharacter character : outside)
		{
			if (vector.Distance(character.GetOrigin(), m_vPos) <= m_fRadius)
				present++;
		}

		gameMode.CTR_SetExfilCount(present, outside.Count(), CTR_ExfilRules.GetNeeded(outside.Count(), m_fRatio));

		WorldTimestamp now = world.GetServerTimestamp();
		if (CTR_ExfilRules.IsMet(present, outside.Count(), m_fRatio))
		{
			if (!m_bHolding)
			{
				m_bHolding = true;
				m_HoldEnd = now.PlusSeconds(m_iHoldSeconds);
				gameMode.CTR_SetExfilHold(true, m_HoldEnd);
			}

			if (!m_HoldEnd.Greater(now))
			{
				Stop();
				gameMode.CTR_OnExfilReached();
				return;
			}
		}
		else if (m_bHolding)
		{
			m_bHolding = false;
			gameMode.CTR_SetExfilHold(false, null);
		}

		WorldTimestamp deadline = gameMode.CTR_GetExfilDeadline();
		if (deadline && !deadline.Greater(now))
		{
			Stop();
			gameMode.CTR_OnExfilTimeout();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Remembers a player who leaves the game outside the base during the exfil: missing in action costs them their gear
	//! too, so leaving does not save it. Call before the game mode handles the disconnect.
	void OnPlayerLeaving(int playerId, vector basePos)
	{
		string ownerId = MRX_Marx.GetOwnerId(playerId);
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId));
		if (ownerId.IsEmpty() || !IsAliveOutsideBase(character, basePos) || m_aLeftOwners.Contains(ownerId))
			return;

		m_aLeftOwners.Insert(ownerId);
		m_aLeftBodies.Insert(character);
		Print(string.Format("[CTR] %1 (%2) left the game during the exfil, outside the base", GetGame().GetPlayerManager().GetPlayerName(playerId), ownerId));
	}

	//------------------------------------------------------------------------------------------------
	//! Missing in action: kills every living player outside the base, the bodies left by players who left during the
	//! exfil, and those players if they came back (wherever they are), and deletes those players' last gear.
	//! \param[out] outKilled Characters killed (to wait until they are dead).
	void KillMissing(vector basePos, CTR_LastGear lastGear, notnull array<IEntity> outKilled)
	{
		array<SCR_ChimeraCharacter> outside = {};
		CollectOutsideBase(basePos, outside);
		foreach (SCR_ChimeraCharacter character : outside)
		{
			Kill(character, outKilled);
		}

		PlayerManager playerManager = GetGame().GetPlayerManager();
		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		foreach (int i, string ownerId : m_aLeftOwners)
		{
			if (lastGear)
				lastGear.Clear(ownerId);

			SCR_ChimeraCharacter body = SCR_ChimeraCharacter.Cast(m_aLeftBodies[i]);
			if (IsAliveOutsideBase(body, basePos))
				Kill(body, outKilled);

			foreach (int playerId : playerIds)
			{
				if (MRX_Marx.GetOwnerId(playerId) == ownerId)
					Kill(SCR_ChimeraCharacter.Cast(playerManager.GetPlayerControlledEntity(playerId)), outKilled);
			}
		}

		Print(string.Format("[CTR] Missing in action: %1 characters killed, %2 players who left lose their gear", outKilled.Count(), m_aLeftOwners.Count()));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Kill(SCR_ChimeraCharacter character, notnull array<IEntity> outKilled)
	{
		if (!character || outKilled.Contains(character) || !character.GetCharacterController())
			return;

		if (character.GetCharacterController().GetLifeState() == ECharacterLifeState.DEAD)
			return;

		character.GetCharacterController().ForceDeath();
		outKilled.Insert(character);
	}

	//------------------------------------------------------------------------------------------------
	//! Unconscious players count: nobody is left behind.
	static bool IsAliveOutsideBase(SCR_ChimeraCharacter character, vector basePos)
	{
		if (!character || !character.GetCharacterController())
			return false;

		if (character.GetCharacterController().GetLifeState() == ECharacterLifeState.DEAD)
			return false;

		return !CTR_ReturnTrip.IsAtBase(character.GetOrigin(), basePos);
	}

	//------------------------------------------------------------------------------------------------
	//! Characters of the connected players who are alive outside the base: those who must exfil.
	static void CollectOutsideBase(vector basePos, notnull array<SCR_ChimeraCharacter> outCharacters)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(playerManager.GetPlayerControlledEntity(playerId));
			if (IsAliveOutsideBase(character, basePos))
				outCharacters.Insert(character);
		}
	}
}
