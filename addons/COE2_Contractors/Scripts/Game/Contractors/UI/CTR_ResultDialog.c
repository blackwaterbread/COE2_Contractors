//! Operation screen (client): earnings, personal stats, the operation's tasks with their grid position and outcome,
//! and team totals. While the operation runs it shows what the player would get if it ended now. After it, a countdown
//! shows when everyone left in the AO returns to base, with a button to return now; the screen stays until it is closed.
class CTR_ResultDialog : MRX_ScriptedDialog
{
	protected static const string DIALOG_TAG = "CTR_Result";
	protected static const string ACTION_RETURN = "return";
	protected static const float WINDOW_WIDTH = 900;
	//! Room for the title, header and footer; the list takes the rest of the screen height.
	protected static const float RESERVED_HEIGHT = 370;
	protected static const float MIN_LIST_HEIGHT = 240;
	protected static const float MAX_LIST_HEIGHT = 640;
	protected static const int SECTION_FONT_SIZE = 24;
	//! The return countdown must be noticed: everyone is moved when it runs out.
	protected static const int COUNTDOWN_FONT_SIZE = 34;
	protected static const int RETURN_BUTTON_FONT_SIZE = 24;
	protected static const int COLOR_GAIN = 0xFF80D080;
	protected static const int COLOR_LOSS = 0xFFE06060;
	protected static const int COLOR_MUTED = 0xFFA0A0A0;
	protected static const int COLOR_SECTION = 0xFFE0C060;

	protected ref CTR_OperationResult m_Result;
	protected TextWidget m_wCountdown;
	protected SCR_ButtonTextComponent m_ReturnButton;
	protected TextWidget m_wReturnStatus;
	protected VerticalLayoutWidget m_wList;

	//! The open result screen, if any (weak).
	protected static CTR_ResultDialog s_Instance;

	//------------------------------------------------------------------------------------------------
	//! Opens the result screen, replacing an open one.
	static CTR_ResultDialog Open(notnull CTR_OperationResult result)
	{
		if (s_Instance)
			s_Instance.Close();

		CTR_ResultDialog dialog = new CTR_ResultDialog();
		dialog.m_Result = result;
		OpenDialog(dialog, GetTitle(result), DIALOG_TAG, DIALOG_LAYOUT_MEDIUM);
		s_Instance = dialog;
		return dialog;
	}

	//------------------------------------------------------------------------------------------------
	static CTR_ResultDialog GetOpen()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetTitle(CTR_OperationResult result)
	{
		if (result.m_bInProgress)
			return "Operation in progress";

		if (!result.m_bFinished)
		{
			if (result.IsSuccess())
				return "Operation ended early";

			return "Operation cancelled";
		}

		if (!result.IsSuccess())
			return "Operation failed";

		return "Operation complete";
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnDialogOpened()
	{
		SetDialogWidth(WINDOW_WIDTH);

		TextWidget areas = AddHeaderLine(GetAreaNames());
		SetColor(areas, COLOR_MUTED);
		if (m_Result.m_bInProgress)
		{
			TextWidget note = AddHeaderLine("Paid when the operation ends. Amounts as if it ended now.");
			SetColor(note, COLOR_MUTED);
		}
		else
		{
			AddReturnHeader();
		}

		m_wList = CreateScrollList(m_wRows, Math.Clamp(GetScreenHeight() - RESERVED_HEIGHT, MIN_LIST_HEIGHT, MAX_LIST_HEIGHT));
		if (!m_wList)
			return;

		AddEarnings();
		AddPersonalStats();
		AddTasks();
		AddTeamTotals();
	}

	//------------------------------------------------------------------------------------------------
	//! Countdown of the loot time and the button to return now.
	protected void AddReturnHeader()
	{
		m_wCountdown = AddHeaderLine(string.Empty);
		if (!m_wCountdown)
			return;

		m_wCountdown.SetExactFontSize(COUNTDOWN_FONT_SIZE);
		SetColor(m_wCountdown, COLOR_SECTION);
		AlignableSlot.SetPadding(m_wCountdown, 0, 6, 0, 6);

		if (GetReturnSecondsLeft() > 0)
		{
			Widget row = CreateLayout(WidgetType.HorizontalLayoutWidgetTypeID, m_wHeader);
			m_ReturnButton = AddButton(row, "Return to base now", ACTION_RETURN);
			if (m_ReturnButton)
			{
				TextWidget buttonText = TextWidget.Cast(m_ReturnButton.GetRootWidget().FindAnyWidget("Text"));
				if (buttonText)
					buttonText.SetExactFontSize(RETURN_BUTTON_FONT_SIZE);
			}

			m_wReturnStatus = CreateText(row, string.Empty);
			SetColor(m_wReturnStatus, COLOR_LOSS);
			AlignableSlot.SetPadding(m_wReturnStatus, 16, 0, 0, 0);
			AlignableSlot.SetVerticalAlign(m_wReturnStatus, LayoutVerticalAlign.Center);
			GetGame().GetCallqueue().CallLater(UpdateCountdown, 250, true);
		}

		UpdateCountdown();
	}

	//------------------------------------------------------------------------------------------------
	protected static int GetReturnSecondsLeft()
	{
		COE_PlayerController controller = COE_PlayerController.GetInstance();
		if (!controller)
			return 0;

		return controller.CTR_GetReturnSecondsLeft();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnRowAction(string action)
	{
		if (action != ACTION_RETURN)
			return;

		COE_PlayerController controller = COE_PlayerController.GetInstance();
		if (controller)
			controller.CTR_RequestReturnNow();
	}

	//------------------------------------------------------------------------------------------------
	//! Why the return to base was refused.
	void ShowReturnStatus(string text)
	{
		if (m_wReturnStatus)
			m_wReturnStatus.SetText(text);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		GetGame().GetCallqueue().Remove(UpdateCountdown);
		if (s_Instance == this)
			s_Instance = null;

		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateCountdown()
	{
		if (!m_wCountdown)
			return;

		if (m_Result.m_iReturnDelaySeconds <= 0)
		{
			m_wCountdown.SetText(string.Empty);
			return;
		}

		int remaining = GetReturnSecondsLeft();
		if (remaining > 0)
		{
			m_wCountdown.SetText(string.Format("Everyone in the AO returns to base in %1", FormatDuration(remaining)));
			return;
		}

		m_wCountdown.SetText("Returned to base");
		if (m_ReturnButton)
			m_ReturnButton.GetRootWidget().SetVisible(false);

		if (m_wReturnStatus)
			m_wReturnStatus.SetText(string.Empty);

		GetGame().GetCallqueue().Remove(UpdateCountdown);
	}

	//------------------------------------------------------------------------------------------------
	protected string GetAreaNames()
	{
		string names;
		foreach (CTR_AreaInfo area : m_Result.m_aAreas)
		{
			if (!names.IsEmpty())
				names += ", ";

			names += WidgetManager.Translate(area.m_sName);
		}

		return string.Format("%1  |  %2", names, FormatDuration(m_Result.m_iDurationSeconds));
	}

	//------------------------------------------------------------------------------------------------
	protected void AddEarnings()
	{
		CTR_Payout payout = m_Result.m_Payout;
		CTR_PlayerStats stats = m_Result.m_Stats;
		if (m_Result.m_bInProgress)
		{
			AddSection("Earnings so far");
			if (!stats.m_bEnteredAO)
				AddLine("You have not entered the AO yet. Only contractors who enter it are paid.", string.Empty, COLOR_MUTED);
			else if (payout.m_iTasks <= 0)
				AddLine("No task completed yet. Nothing is paid until one is.", string.Empty, COLOR_MUTED);

			if (stats.m_bEnteredAO)
				AddPayLines();

			AddLine("If it ended now", FormatAmount(payout.m_iTotal, m_Result.m_sCurrency), GetAmountColor(payout.m_iTotal));
			return;
		}

		AddSection("Earnings");
		if (!m_Result.m_bFinished)
			AddLine("Ended before all tasks were finished: completed tasks still pay.", string.Empty, COLOR_MUTED);

		if (!stats.m_bEnteredAO)
			AddLine("You did not enter the AO.", string.Empty, COLOR_MUTED);
		else if (payout.m_iTasks <= 0)
			AddLine("No task was completed.", string.Empty, COLOR_MUTED);
		else
			AddPayLines();

		AddLine("Total", FormatAmount(payout.m_iTotal, m_Result.m_sCurrency), GetAmountColor(payout.m_iTotal));
		AddLine(GetPayStatusText(), GetBalanceText(), COLOR_MUTED);
	}

	//------------------------------------------------------------------------------------------------
	//! Completed tasks and the personal lines.
	protected void AddPayLines()
	{
		CTR_Payout payout = m_Result.m_Payout;
		CTR_PlayerStats stats = m_Result.m_Stats;
		foreach (CTR_TaskOutcome task : m_Result.m_aTasks)
		{
			if (task.m_bCompleted)
				AddLine(WidgetManager.Translate(task.m_sName), FormatAmount(task.m_iAmount, m_Result.m_sCurrency), COLOR_GAIN);
		}

		AddCountLine("Kills", stats.m_iKills, payout.m_iKills);
		AddCountLine("Bandages and CPR", stats.m_iHeals, payout.m_iHeals);
		AddCountLine("Friendly or civilian kills", stats.m_iTeamKills, payout.m_iTeamKills);
		AddCountLine("Deaths", stats.m_iDeaths, payout.m_iDeaths);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddCountLine(string label, int count, int amount)
	{
		if (count == 0)
			return;

		AddLine(string.Format("%1 x%2", label, count), FormatAmount(amount, m_Result.m_sCurrency), GetAmountColor(amount));
	}

	//------------------------------------------------------------------------------------------------
	protected string GetPayStatusText()
	{
		switch (m_Result.m_ePayStatus)
		{
			case CTR_EPayStatus.PAID: return "Paid";
			case CTR_EPayStatus.ALREADY_PAID: return "Already paid";
			case CTR_EPayStatus.NO_OWNER: return "Not paid: no player identity";
			case CTR_EPayStatus.FAILED: return "Not paid: storage error";
			case CTR_EPayStatus.PENDING: return "Payment pending";
		}

		return "Nothing to pay";
	}

	//------------------------------------------------------------------------------------------------
	protected string GetBalanceText()
	{
		if (!m_Result.m_bHasBalance)
			return string.Empty;

		return "Balance " + MRX_TextFormat.Money(m_Result.m_iBalance, m_Result.m_sCurrency);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddPersonalStats()
	{
		AddSection("Your operation");
		CTR_PlayerStats stats = m_Result.m_Stats;
		AddLine("Kills", stats.m_iKills.ToString());
		AddLine("Bandages and CPR", stats.m_iHeals.ToString());
		AddLine("Deaths", stats.m_iDeaths.ToString());
		AddLine("Shots fired", stats.m_iShots.ToString());
		AddLine("Distance", FormatDistance(stats.m_fDistance));
		AddLine("Time in the operation", FormatDuration(stats.m_iSeconds));
	}

	//------------------------------------------------------------------------------------------------
	protected void AddTasks()
	{
		AddSection("Tasks");
		foreach (CTR_TaskOutcome task : m_Result.m_aTasks)
		{
			string outcome = "Failed";
			int color = COLOR_LOSS;
			if (task.m_bCompleted)
			{
				outcome = "Completed";
				color = COLOR_GAIN;
			}
			else if (m_Result.m_bInProgress && !task.m_bFailed)
			{
				outcome = "In progress";
				color = COLOR_MUTED;
			}
			else if (!m_Result.m_bFinished && !task.m_bFailed)
			{
				outcome = "Not finished";
				color = COLOR_MUTED;
			}

			AddLine(string.Format("%1  (grid %2)", WidgetManager.Translate(task.m_sName), FormatGrid(task.m_fX, task.m_fZ)), outcome, color);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void AddTeamTotals()
	{
		AddSection("Team");
		AddLine("Tasks completed", string.Format("%1 / %2", m_Result.CountCompletedTasks(), m_Result.m_aTasks.Count()));
		AddLine("Contractors in the AO", m_Result.m_iParticipants.ToString());
		if (m_Result.m_bInProgress)
			AddLine("Team total if it ended now", MRX_TextFormat.Money(m_Result.m_iTeamPay, m_Result.m_sCurrency));
		else
			AddLine("Total earned", MRX_TextFormat.Money(m_Result.m_iTeamPay, m_Result.m_sCurrency));
		AddLine("Operation time", FormatDuration(m_Result.m_iDurationSeconds));
	}

	//------------------------------------------------------------------------------------------------
	protected void AddSection(string title)
	{
		TextWidget text = CreateText(m_wList, title);
		text.SetExactFontSize(SECTION_FONT_SIZE);
		SetColor(text, COLOR_SECTION);
		AlignableSlot.SetPadding(text, 0, 12, 0, 4);
	}

	//------------------------------------------------------------------------------------------------
	//! Label on the left, value on the right.
	protected void AddLine(string label, string value, int valueColor = Color.WHITE)
	{
		Widget row = CreateLayout(WidgetType.HorizontalLayoutWidgetTypeID, m_wList);
		AlignableSlot.SetHorizontalAlign(row, LayoutHorizontalAlign.Stretch);
		TextWidget labelText = CreateText(row, label);
		LayoutSlot.SetSizeMode(labelText, LayoutSizeMode.Fill);
		TextWidget valueText = CreateText(row, value);
		SetColor(valueText, valueColor);
		AlignableSlot.SetPadding(valueText, 16, 0, 8, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetColor(Widget widget, int color)
	{
		if (widget)
			widget.SetColor(Color.FromInt(color));
	}

	//------------------------------------------------------------------------------------------------
	protected static int GetAmountColor(int amount)
	{
		if (amount > 0)
			return COLOR_GAIN;

		if (amount < 0)
			return COLOR_LOSS;

		return COLOR_MUTED;
	}

	//------------------------------------------------------------------------------------------------
	//! Signed amount, e.g. "+$6,000" or "-$2,500".
	static string FormatAmount(int amount, string currency)
	{
		if (amount > 0)
			return "+" + MRX_TextFormat.Money(amount, currency);

		return MRX_TextFormat.Money(amount, currency);
	}

	//------------------------------------------------------------------------------------------------
	static string FormatDuration(int seconds)
	{
		seconds = Math.Max(0, seconds);
		int hours = seconds / 3600;
		int minutes = (seconds % 3600) / 60;
		int rest = seconds % 60;
		if (hours > 0)
			return string.Format("%1:%2:%3", hours, minutes.ToString(2), rest.ToString(2));

		return string.Format("%1:%2", minutes, rest.ToString(2));
	}

	//------------------------------------------------------------------------------------------------
	static string FormatDistance(float meters)
	{
		if (meters < 1000)
			return string.Format("%1 m", Math.Round(meters));

		int tenths = Math.Round(meters / 100);
		return string.Format("%1.%2 km", tenths / 10, tenths % 10);
	}

	//------------------------------------------------------------------------------------------------
	//! Six-figure grid reference (100 m squares), as on the map.
	static string FormatGrid(float x, float z)
	{
		int column = Math.Floor(x / 100);
		int row = Math.Floor(z / 100);
		return string.Format("%1 %2", column.ToString(3), row.ToString(3));
	}
}
