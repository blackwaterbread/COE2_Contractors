//! Contractors debug actions (Marx debug panel, $profile:mrx_cmd.txt in Workbench, "#mrxdbg <id>" in the chat or over
//! RCON): try an operation without playing it through, move around the base and open its windows. The server runs them
//! only in developer builds (MRX_DebugRunner.IsAllowed). Money: wallet.give.

//------------------------------------------------------------------------------------------------
modded class MRX_DebugRegistry
{
	//------------------------------------------------------------------------------------------------
	override protected void RegisterActions()
	{
		super.RegisterActions();
		Register(new CTR_DebugAO());
		Register(new CTR_DebugGo());
		Register(new CTR_DebugWin());
		Register(new CTR_DebugFail());
		Register(new CTR_DebugCancel());
		Register(new CTR_DebugEarly());
		Register(new CTR_DebugExfil());
		Register(new CTR_DebugExfilNow());
		Register(new CTR_DebugMia());
		Register(new CTR_DebugCountdown());
		Register(new CTR_DebugPursuit());
		Register(new CTR_DebugCivilian());
		Register(new CTR_DebugBase());
		Register(new CTR_DebugQuartermaster());
		Register(new CTR_DebugOut());
		Register(new CTR_DebugShop());
		Register(new CTR_DebugStash());
	}
}

//------------------------------------------------------------------------------------------------
//! A Contractors debug action with a synchronous server part (Run).
class CTR_DebugAction : MRX_DebugAction
{
	static const string PAGE_OPERATION = "CTR ops";
	static const string PAGE_BASE = "CTR base";
	static const string NO_EXFIL = "no exfil running (ctr.win or ctr.early)";
	//! Quartermaster and base stash are searched this far from the base position.
	static const float BASE_RADIUS = 60;

	//------------------------------------------------------------------------------------------------
	override bool HasServerPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void RunServer(notnull MRX_DebugContext context, notnull MRX_DebugReply reply)
	{
		reply.Done(Run(context));
	}

	//------------------------------------------------------------------------------------------------
	//! Override: the server part.
	protected MRX_DebugResult Run(notnull MRX_DebugContext context)
	{
		return MRX_DebugResult.Failed("no server part");
	}

	//------------------------------------------------------------------------------------------------
	//! \param error Empty on success.
	protected static MRX_DebugResult Answer(string error, string done)
	{
		if (!error.IsEmpty())
			return MRX_DebugResult.Failed(error);

		return MRX_DebugResult.Ok(done);
	}

	//------------------------------------------------------------------------------------------------
	protected static MRX_DebugResult Teleport(notnull MRX_DebugContext context, vector pos, string done)
	{
		return Answer(CTR_DevTools.Teleport(context.m_iPlayerId, pos), done);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The running game mode with an exfil, or null.
	protected static COE_GameMode GetExfilGameMode()
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode || !gameMode.CTR_GetExfil())
			return null;

		return gameMode;
	}

	//------------------------------------------------------------------------------------------------
	protected static MRX_DebugResult SetCountdown(int seconds)
	{
		COE_GameMode gameMode = GetExfilGameMode();
		if (!gameMode)
			return MRX_DebugResult.Failed(NO_EXFIL);

		if (seconds < 0)
			return MRX_DebugResult.Create(MRX_EDebugStatus.BAD_ARGS, "seconds must be 0 or more");

		gameMode.CTR_SetExfilSecondsLeft(seconds);
		return MRX_DebugResult.Ok(string.Format("exfil countdown: %1 s", seconds));
	}

	//------------------------------------------------------------------------------------------------
	//! The quartermaster's shop, or null.
	static MRX_ShopComponent FindQuartermaster()
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode)
			return null;

		IEntity entity = CTR_DevTools.FindNear(gameMode.GetMainBasePos(), BASE_RADIUS, MRX_ShopKeeperComponent);
		if (!entity)
			return null;

		return MRX_ShopComponent.Cast(entity.FindComponent(MRX_ShopComponent));
	}
}

//------------------------------------------------------------------------------------------------
//! Puts an exfil point and generates an AO with random tasks away from the base (0: only "Clear Area").
class CTR_DebugAO : CTR_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void CTR_DebugAO()
	{
		Setup("ctr.ao", PAGE_OPERATION, "Generate AO");
		AddArg("tasks", "2", true);
	}

	//------------------------------------------------------------------------------------------------
	override protected MRX_DebugResult Run(notnull MRX_DebugContext context)
	{
		string location;
		string error = CTR_DevTools.GenerateAO(context.GetInt(0), location);
		return Answer(error, string.Format("AO generated at %1; ctr.go takes you there", location));
	}
}

//------------------------------------------------------------------------------------------------
//! Moves to the edge of the running AO (counts as entering it).
class CTR_DebugGo : CTR_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void CTR_DebugGo()
	{
		Setup("ctr.go", PAGE_OPERATION, "Go to AO");
	}

	//------------------------------------------------------------------------------------------------
	override protected MRX_DebugResult Run(notnull MRX_DebugContext context)
	{
		vector pos;
		if (!CTR_DevTools.GetAOEntryPos(pos))
			return MRX_DebugResult.Failed("no AO is running (ctr.ao)");

		return Teleport(context, pos, "you are at the edge of the AO");
	}
}

//------------------------------------------------------------------------------------------------
//! Sets every open task to a state; the AO then finishes as if the players did it.
class CTR_DebugFinishTasks : CTR_DebugAction
{
	protected SCR_ETaskState m_eState;
	protected string m_sWord;

	//------------------------------------------------------------------------------------------------
	override protected MRX_DebugResult Run(notnull MRX_DebugContext context)
	{
		int count = CTR_DevTools.FinishTasks(m_eState);
		if (count == 0)
			return MRX_DebugResult.Failed("no open task (ctr.ao)");

		return MRX_DebugResult.Ok(string.Format("%1 tasks %2", count, m_sWord));
	}
}

//------------------------------------------------------------------------------------------------
//! Completes every task: the exfil starts.
class CTR_DebugWin : CTR_DebugFinishTasks
{
	//------------------------------------------------------------------------------------------------
	void CTR_DebugWin()
	{
		Setup("ctr.win", PAGE_OPERATION, "Complete tasks");
		m_eState = SCR_ETaskState.COMPLETED;
		m_sWord = "completed";
	}
}

//------------------------------------------------------------------------------------------------
//! Fails every task: the operation fails and everyone returns.
class CTR_DebugFail : CTR_DebugFinishTasks
{
	//------------------------------------------------------------------------------------------------
	void CTR_DebugFail()
	{
		Setup("ctr.fail", PAGE_OPERATION, "Fail tasks");
		m_eState = SCR_ETaskState.FAILED;
		m_sWord = "failed";
	}
}

//------------------------------------------------------------------------------------------------
//! Cancels the operation like the commander does (delayed return).
class CTR_DebugCancel : CTR_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void CTR_DebugCancel()
	{
		Setup("ctr.cancel", PAGE_OPERATION, "Cancel operation");
	}

	//------------------------------------------------------------------------------------------------
	override protected MRX_DebugResult Run(notnull MRX_DebugContext context)
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode || gameMode.COE_GetState() == COE_EGameModeState.INTERMISSION)
			return MRX_DebugResult.Failed("no AO is running");

		gameMode.CTR_RequestCancel();
		return MRX_DebugResult.Ok("operation cancelled");
	}
}

//------------------------------------------------------------------------------------------------
//! Orders the early exfil.
class CTR_DebugEarly : CTR_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void CTR_DebugEarly()
	{
		Setup("ctr.early", PAGE_OPERATION, "Early exfil");
	}

	//------------------------------------------------------------------------------------------------
	override protected MRX_DebugResult Run(notnull MRX_DebugContext context)
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode || !gameMode.CTR_StartEarlyExfil())
			return MRX_DebugResult.Failed("no running operation before its exfil, or no exfil point");

		gameMode.CTR_AlertAll(CTR_EAlert.EARLY_EXFIL, 0);
		return MRX_DebugResult.Ok("early exfil ordered");
	}
}

//------------------------------------------------------------------------------------------------
//! Moves next to the exfil point.
class CTR_DebugExfil : CTR_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void CTR_DebugExfil()
	{
		Setup("ctr.exfil", PAGE_OPERATION, "Go to exfil");
	}

	//------------------------------------------------------------------------------------------------
	override protected MRX_DebugResult Run(notnull MRX_DebugContext context)
	{
		vector pos;
		if (!CTR_DevTools.GetExfilPos(pos))
			return MRX_DebugResult.Failed("no exfil point");

		return Teleport(context, pos, "you are at the exfil point");
	}
}

//------------------------------------------------------------------------------------------------
//! The exfil succeeds now.
class CTR_DebugExfilNow : CTR_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void CTR_DebugExfilNow()
	{
		Setup("ctr.exfilnow", PAGE_OPERATION, "Exfil now");
	}

	//------------------------------------------------------------------------------------------------
	override protected MRX_DebugResult Run(notnull MRX_DebugContext context)
	{
		COE_GameMode gameMode = GetExfilGameMode();
		if (!gameMode)
			return MRX_DebugResult.Failed(NO_EXFIL);

		gameMode.CTR_OnExfilReached();
		return MRX_DebugResult.Ok("exfil reached");
	}
}

//------------------------------------------------------------------------------------------------
//! Sets the exfil countdown.
class CTR_DebugCountdown : CTR_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void CTR_DebugCountdown()
	{
		Setup("ctr.cd", PAGE_OPERATION, "Exfil countdown");
		AddArg("seconds", "", true);
	}

	//------------------------------------------------------------------------------------------------
	override protected MRX_DebugResult Run(notnull MRX_DebugContext context)
	{
		return SetCountdown(context.GetInt(0));
	}
}

//------------------------------------------------------------------------------------------------
//! The exfil countdown runs out now (missing in action).
class CTR_DebugMia : CTR_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void CTR_DebugMia()
	{
		Setup("ctr.mia", PAGE_OPERATION, "Missing in action");
	}

	//------------------------------------------------------------------------------------------------
	override protected MRX_DebugResult Run(notnull MRX_DebugContext context)
	{
		return SetCountdown(0);
	}
}

//------------------------------------------------------------------------------------------------
//! The enemy pursuit starts now (first wave).
class CTR_DebugPursuit : CTR_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void CTR_DebugPursuit()
	{
		Setup("ctr.pursuit", PAGE_OPERATION, "Start pursuit");
	}

	//------------------------------------------------------------------------------------------------
	override protected MRX_DebugResult Run(notnull MRX_DebugContext context)
	{
		COE_GameMode gameMode = GetExfilGameMode();
		if (!gameMode)
			return MRX_DebugResult.Failed(NO_EXFIL);

		CTR_Pursuit pursuit = gameMode.CTR_GetExfil().GetPursuit();
		if (pursuit.IsStarted())
			return MRX_DebugResult.Failed(string.Format("the pursuit runs already (wave %1)", pursuit.GetWave()));

		pursuit.Start();
		return MRX_DebugResult.Ok("pursuit started");
	}
}

//------------------------------------------------------------------------------------------------
//! Counts a civilian killed in the AO (rolls the pursuit again during the exfil).
class CTR_DebugCivilian : CTR_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void CTR_DebugCivilian()
	{
		Setup("ctr.civ", PAGE_OPERATION, "Civilian kill");
	}

	//------------------------------------------------------------------------------------------------
	override protected MRX_DebugResult Run(notnull MRX_DebugContext context)
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode || !gameMode.CTR_GetOperation() || gameMode.CTR_GetOperation().IsClosed())
			return MRX_DebugResult.Failed("no operation running");

		gameMode.CTR_GetOperation().AddCivilianKill();
		return MRX_DebugResult.Ok(string.Format("%1 civilians killed in the AO", gameMode.CTR_GetOperation().GetCivilianKills()));
	}
}

//------------------------------------------------------------------------------------------------
//! Moves next to the base arsenal shops.
class CTR_DebugBase : CTR_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void CTR_DebugBase()
	{
		Setup("ctr.base", PAGE_BASE, "Go to base shops");
	}

	//------------------------------------------------------------------------------------------------
	override protected MRX_DebugResult Run(notnull MRX_DebugContext context)
	{
		vector pos;
		if (!CTR_DevTools.GetShopPos(pos))
			return MRX_DebugResult.Failed("no COE2 main base");

		return Teleport(context, pos, "you are at the base arsenal shops");
	}
}

//------------------------------------------------------------------------------------------------
//! Stands in front of the quartermaster, facing him.
class CTR_DebugQuartermaster : CTR_DebugAction
{
	protected static const float FRONT_DISTANCE = 2.5;

	//------------------------------------------------------------------------------------------------
	void CTR_DebugQuartermaster()
	{
		Setup("ctr.qm", PAGE_BASE, "Go to quartermaster");
	}

	//------------------------------------------------------------------------------------------------
	override protected MRX_DebugResult Run(notnull MRX_DebugContext context)
	{
		MRX_ShopComponent shop = FindQuartermaster();
		if (!shop)
			return MRX_DebugResult.Failed("no quartermaster at the base");

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(context.m_Character);
		SCR_EditableCharacterComponent editable;
		if (character)
			editable = SCR_EditableCharacterComponent.Cast(character.FindComponent(SCR_EditableCharacterComponent));

		if (!editable)
			return MRX_DebugResult.Failed("you have no character that can be moved");

		IEntity quartermaster = shop.GetOwner();
		vector front = quartermaster.GetOrigin() + quartermaster.GetTransformAxis(2) * FRONT_DISTANCE;
		vector toQuartermaster = quartermaster.GetOrigin() - front;
		vector transform[4];
		KSC_GameTools.GetTransformFromPosAndRot(transform, front, toQuartermaster.ToYaw());
		editable.SetTransform(transform);
		return MRX_DebugResult.Ok(string.Format("quartermaster at %1 facing %2", quartermaster.GetOrigin(), quartermaster.GetYawPitchRoll()[0]));
	}
}

//------------------------------------------------------------------------------------------------
//! Moves 120 m away from the base, out of the safe zone.
class CTR_DebugOut : CTR_DebugAction
{
	protected static const float DISTANCE = 120;
	protected static const float FREE_RADIUS = 20;

	//------------------------------------------------------------------------------------------------
	void CTR_DebugOut()
	{
		Setup("ctr.out", PAGE_BASE, "Leave base");
	}

	//------------------------------------------------------------------------------------------------
	override protected MRX_DebugResult Run(notnull MRX_DebugContext context)
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode)
			return MRX_DebugResult.Failed("not a COE2 world");

		vector pos = gameMode.GetMainBasePos() + Vector(DISTANCE, 0, 0);
		vector free;
		if (SCR_WorldTools.FindEmptyTerrainPosition(free, pos, FREE_RADIUS))
			pos = free;

		return Teleport(context, pos, "you are 120 m from the base");
	}
}

//------------------------------------------------------------------------------------------------
//! Opens the quartermaster's shop window (the client waits until the quartermaster replicated to it).
class CTR_DebugShop : CTR_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void CTR_DebugShop()
	{
		Setup("ctr.shop", PAGE_BASE, "Quartermaster shop");
	}

	//------------------------------------------------------------------------------------------------
	override bool HasClientPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override protected MRX_DebugResult Run(notnull MRX_DebugContext context)
	{
		MRX_ShopComponent shop = FindQuartermaster();
		if (!shop)
			return MRX_DebugResult.Failed("no quartermaster at the base");

		return MRX_DebugResult.Ok().SetEntity(shop.GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	override MRX_DebugResult RunClient(notnull MRX_DebugContext context, notnull MRX_DebugResult serverResult)
	{
		MRX_ShopComponent shop;
		if (context.m_Entity)
			shop = MRX_ShopComponent.Cast(context.m_Entity.FindComponent(MRX_ShopComponent));

		if (!shop)
			return MRX_DebugResult.Failed("the quartermaster is not here (ctr.qm first)");

		MRX_ShopMenu.Open(shop);
		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! Opens the stash at the base (the stash point checks the distance: ctr.qm first when far away).
class CTR_DebugStash : CTR_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void CTR_DebugStash()
	{
		Setup("ctr.stash", PAGE_BASE, "Base stash");
	}

	//------------------------------------------------------------------------------------------------
	override bool HasClientPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override protected MRX_DebugResult Run(notnull MRX_DebugContext context)
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode)
			return MRX_DebugResult.Failed("not a COE2 world");

		IEntity stashPoint = CTR_DevTools.FindNear(gameMode.GetMainBasePos(), BASE_RADIUS, MRX_StashPointComponent);
		if (!stashPoint)
			return MRX_DebugResult.Failed("no stash at the base");

		return MRX_DebugResult.Ok().SetEntity(stashPoint);
	}

	//------------------------------------------------------------------------------------------------
	override MRX_DebugResult RunClient(notnull MRX_DebugContext context, notnull MRX_DebugResult serverResult)
	{
		if (!context.m_Controller || !context.m_Entity)
			return MRX_DebugResult.Failed("the base stash is not here (ctr.qm first)");

		context.m_Controller.MRX_RequestStashOpen(context.m_Entity);
		return null;
	}
}
