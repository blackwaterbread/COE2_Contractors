//! Operation result screen (client): earnings, personal stats, the operation's tasks with their grid position and
//! outcome, and team totals. A countdown shows when everyone returns to base; the screen stays until it is closed.
class CTR_ResultDialog : MRX_ScriptedDialog
{
	protected static const string DIALOG_TAG = "CTR_Result";
	protected static const float WINDOW_WIDTH = 900;
	//! Room for the title, header and footer; the list takes the rest of the screen height.
	protected static const float RESERVED_HEIGHT = 340;
	protected static const float MIN_LIST_HEIGHT = 240;
	protected static const float MAX_LIST_HEIGHT = 640;
	protected static const int SECTION_FONT_SIZE = 24;
	protected static const int COLOR_GAIN = 0xFF80D080;
	protected static const int COLOR_LOSS = 0xFFE06060;
	protected static const int COLOR_MUTED = 0xFFA0A0A0;
	protected static const int COLOR_SECTION = 0xFFE0C060;

	protected ref CTR_OperationResult m_Result;
	protected TextWidget m_wCountdown;
	protected VerticalLayoutWidget m_wList;
	protected int m_iReturnTick;

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
		dialog.m_iReturnTick = System.GetTickCount() + result.m_iReturnDelaySeconds * 1000;
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
		if (!result.m_bFinished)
			return "Operation cancelled";

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
		m_wCountdown = AddHeaderLine(string.Empty);
		UpdateCountdown();

		m_wList = CreateScrollList(m_wRows, Math.Clamp(GetScreenHeight() - RESERVED_HEIGHT, MIN_LIST_HEIGHT, MAX_LIST_HEIGHT));
		if (!m_wList)
			return;

		AddEarnings();
		AddPersonalStats();
		AddTasks();
		AddTeamTotals();

		if (m_Result.m_iReturnDelaySeconds > 0)
			GetGame().GetCallqueue().CallLater(UpdateCountdown, 250, true);
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

		int remaining = Math.Ceil((m_iReturnTick - System.GetTickCount()) / 1000.0);
		if (remaining > 0)
		{
			m_wCountdown.SetText(string.Format("Returning to base in %1 s", remaining));
			return;
		}

		m_wCountdown.SetText("Returned to base");
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
		AddSection("Earnings");
		CTR_Payout payout = m_Result.m_Payout;
		CTR_PlayerStats stats = m_Result.m_Stats;

		if (!m_Result.m_bFinished)
		{
			AddLine("The operation was cancelled before all tasks were finished.", string.Empty, COLOR_MUTED);
		}
		else if (!stats.m_bEnteredAO)
		{
			AddLine("You did not enter the AO.", string.Empty, COLOR_MUTED);
		}
		else if (payout.m_iTasks <= 0)
		{
			AddLine("No task was completed.", string.Empty, COLOR_MUTED);
		}
		else
		{
			foreach (CTR_TaskOutcome task : m_Result.m_aTasks)
			{
				if (task.m_bCompleted)
					AddLine(WidgetManager.Translate(task.m_sName), FormatAmount(task.m_iAmount), COLOR_GAIN);
			}

			AddCountLine("Kills", stats.m_iKills, payout.m_iKills);
			AddCountLine("Friendlies treated", stats.m_iHeals, payout.m_iHeals);
			AddCountLine("Friendly or civilian kills", stats.m_iTeamKills, payout.m_iTeamKills);
			AddCountLine("Deaths", stats.m_iDeaths, payout.m_iDeaths);
		}

		AddLine("Total", FormatAmount(payout.m_iTotal), GetAmountColor(payout.m_iTotal));
		AddLine(GetPayStatusText(), GetBalanceText(), COLOR_MUTED);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddCountLine(string label, int count, int amount)
	{
		if (count == 0)
			return;

		AddLine(string.Format("%1 x%2", label, count), FormatAmount(amount), GetAmountColor(amount));
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

		return string.Format("Balance %1 %2", m_Result.m_iBalance, m_Result.m_sCurrency);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddPersonalStats()
	{
		AddSection("Your operation");
		CTR_PlayerStats stats = m_Result.m_Stats;
		AddLine("Kills", stats.m_iKills.ToString());
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
			else if (!m_Result.m_bFinished)
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
		AddLine("Total earned", string.Format("%1 %2", m_Result.m_iTeamPay, m_Result.m_sCurrency));
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
	static string FormatAmount(int amount)
	{
		if (amount > 0)
			return "+" + amount;

		return amount.ToString();
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
