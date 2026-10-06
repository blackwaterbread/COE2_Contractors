#ifdef ENABLE_DIAG
//! Diag builds only: "#ctr <action>" in the chat (admin) to try an operation without playing it through.
//!   #ctr ao [n]    generate an AO with n random tasks (default 2) away from the base
//!   #ctr go        move to the edge of the running AO (counts as entering it)
//!   #ctr win       complete every task: pay, result screen, loot time, return to base
//!   #ctr fail      fail every task: the operation finishes without pay
//!   #ctr cancel    cancel the AO like the commander does
//!   #ctr base      move next to the base arsenal shops
//!   #ctr cash [n]  credit n cash to yourself (default 1000)
class CTR_DevCommand : ScrServerCommand
{
	static const string KEYWORD = "ctr";
	protected static const string HELP = "#ctr ao [tasks] | go | win | fail | cancel | base | cash [amount]";
	protected static const int DEFAULT_TASKS = 2;
	protected static const int DEFAULT_CASH = 1000;

	//------------------------------------------------------------------------------------------------
	override string GetKeyword()
	{
		return KEYWORD;
	}

	//------------------------------------------------------------------------------------------------
	override bool IsServerSide()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override int RequiredRCONPermission()
	{
		return ERCONPermissions.PERMISSIONS_ADMIN;
	}

	//------------------------------------------------------------------------------------------------
	override int RequiredChatPermission()
	{
		return EPlayerRole.ADMINISTRATOR;
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId)
	{
		return ScrServerCmdResult(string.Empty, EServerCmdResultType.OK);
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		if (argv.Count() < 2)
			return Result(HELP, false);

		string action = argv[1];
		action.ToLower();
		int amount = -1;
		if (argv.Count() >= 3)
			amount = argv[2].ToInt();

		switch (action)
		{
			case "ao": return GenerateAO(amount);
			case "go": return Go(playerId);
			case "win": return FinishTasks(SCR_ETaskState.COMPLETED, "completed");
			case "fail": return FinishTasks(SCR_ETaskState.FAILED, "failed");
			case "cancel": return Cancel();
			case "base": return Base(playerId);
			case "cash": return Cash(playerId, amount);
		}

		return Result(HELP, false);
	}

	//------------------------------------------------------------------------------------------------
	protected ScrServerCmdResult GenerateAO(int taskCount)
	{
		if (taskCount < 0)
			taskCount = DEFAULT_TASKS;

		string location;
		string error = CTR_DevTools.GenerateAO(taskCount, location);
		if (!error.IsEmpty())
			return Result(error, false);

		return Result(string.Format("AO generated at %1; #ctr go takes you there", location), true);
	}

	//------------------------------------------------------------------------------------------------
	protected ScrServerCmdResult Go(int playerId)
	{
		vector pos;
		if (!CTR_DevTools.GetAOEntryPos(pos))
			return Result("no AO is running (#ctr ao)", false);

		return TeleportResult(playerId, pos, "you are at the edge of the AO");
	}

	//------------------------------------------------------------------------------------------------
	protected ScrServerCmdResult FinishTasks(SCR_ETaskState state, string word)
	{
		int count = CTR_DevTools.FinishTasks(state);
		if (count == 0)
			return Result("no open task (#ctr ao)", false);

		return Result(string.Format("%1 tasks %2", count, word), true);
	}

	//------------------------------------------------------------------------------------------------
	protected ScrServerCmdResult Cancel()
	{
		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode || gameMode.COE_GetState() == COE_EGameModeState.INTERMISSION)
			return Result("no AO is running", false);

		gameMode.ExecuteCommanderRequest(COE_ECommanderRequest.CANCEL_AO);
		return Result("AO cancelled", true);
	}

	//------------------------------------------------------------------------------------------------
	protected ScrServerCmdResult Base(int playerId)
	{
		vector pos;
		if (!CTR_DevTools.GetShopPos(pos))
			return Result("no COE2 main base", false);

		return TeleportResult(playerId, pos, "you are at the base arsenal shops");
	}

	//------------------------------------------------------------------------------------------------
	protected ScrServerCmdResult Cash(int playerId, int amount)
	{
		if (amount <= 0)
			amount = DEFAULT_CASH;

		string ownerId = MRX_Marx.GetOwnerId(playerId);
		MRX_EconomyService economy = MRX_Marx.GetEconomy();
		if (!economy || ownerId.IsEmpty())
			return Result("no wallet for you (no player identity; start Workbench with -TestIdentity)", false);

		MRX_TxContext context = MRX_TxContext.Create("coe2_contractors_dev", "dev cash", "dev:" + MRX_Marx.NewId());
		economy.Credit(ownerId, CTR_Settings.Get().m_sCurrency, amount, context);
		return Result(string.Format("%1 %2 credited", amount, CTR_Settings.Get().m_sCurrency), true);
	}

	//------------------------------------------------------------------------------------------------
	protected ScrServerCmdResult TeleportResult(int playerId, vector pos, string done)
	{
		string error = CTR_DevTools.Teleport(playerId, pos);
		if (!error.IsEmpty())
			return Result(error, false);

		return Result(done, true);
	}

	//------------------------------------------------------------------------------------------------
	protected static ScrServerCmdResult Result(string text, bool ok)
	{
		if (ok)
			return ScrServerCmdResult(text, EServerCmdResultType.OK);

		return ScrServerCmdResult(text, EServerCmdResultType.ERR);
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)
	{
		return ScrServerCmdResult("Use it in the chat", EServerCmdResultType.ERR);
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnUpdate()
	{
		return ScrServerCmdResult(string.Empty, EServerCmdResultType.OK);
	}
}
#endif
