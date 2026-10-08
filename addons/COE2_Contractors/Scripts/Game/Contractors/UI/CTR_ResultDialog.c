//! Operation screen (client): personal stats, the operation's tasks with their grid position and outcome, team totals,
//! and the earnings at the bottom. While the operation runs it shows the earnings so far and the pay if the exfil
//! succeeds; after it, how it ended and what was paid. The screen stays until it is closed.
//! Missing in action it holds the player: it cannot be closed and counts down to their death, then closes.
class CTR_ResultDialog : MRX_ScriptedDialog
{
	protected static const string DIALOG_TAG = "CTR_Result";
	protected static const float WINDOW_WIDTH = 900;
	//! Room for the title, header and footer; the list takes the rest of the screen height.
	protected static const float RESERVED_HEIGHT = 370;
	protected static const float MIN_LIST_HEIGHT = 240;
	protected static const float MAX_LIST_HEIGHT = 640;
	protected static const int SECTION_FONT_SIZE = 24;
	protected static const int COLOR_GAIN = 0xFF80D080;
	protected static const int COLOR_LOSS = 0xFFE06060;
	protected static const int COLOR_MUTED = 0xFFA0A0A0;
	protected static const int COLOR_SECTION = 0xFFE0C060;
	protected static const int COLOR_RULE = 0xFF5A5F66;
	protected static const float TITLE_RULE_HEIGHT = 2;
	protected static const float TOTAL_RULE_HEIGHT = 1;
	//! Taken off the room the dialog layout leaves above its content ("Content"), so the line under the title sits as
	//! far from the title as from the line below it.
	protected static const float TITLE_GAP_CUT = 12;
	protected static const int HELD_UPDATE_MS = 200;
	//! Missing in action: the screen lets go when the player is still alive this long after their time was up.
	protected static const int HELD_GRACE_MS = 10000;

	protected ref CTR_OperationResult m_Result;
	protected VerticalLayoutWidget m_wList;
	//! In progress: the times count up while the screen is open.
	protected int m_iOpenTick;
	protected TextWidget m_wAreas;
	protected TextWidget m_wPersonalTime;
	protected TextWidget m_wOperationTime;
	//! Missing in action: the screen cannot be closed until the player died, which is due at this tick.
	protected bool m_bHeld;
	protected int m_iHeldUntilTick;
	protected TextWidget m_wHeldLine;

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
		dialog.m_iOpenTick = System.GetTickCount();
		dialog.m_bHeld = result.m_bMissing;
		dialog.m_iHeldUntilTick = dialog.m_iOpenTick + result.m_fMissingSeconds * 1000;
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
	CTR_OperationResult GetResult()
	{
		return m_Result;
	}

	//------------------------------------------------------------------------------------------------
	//! Missing in action: the screen cannot be closed yet.
	bool IsHeld()
	{
		return m_bHeld;
	}

	//------------------------------------------------------------------------------------------------
	static string GetTitle(notnull CTR_OperationResult result)
	{
		if (result.m_bInProgress)
			return "#CTR-Result_TitleInProgress";

		switch (result.m_eEnd)
		{
			case CTR_EOperationEnd.EARLY_EXFIL: return "#CTR-Result_TitleEarlyExfil";
			case CTR_EOperationEnd.FAILED: return "#CTR-Result_TitleFailed";
			case CTR_EOperationEnd.MISSING: return "#CTR-Result_TitleMissing";
			case CTR_EOperationEnd.ABANDONED: return "#CTR-Result_TitleAbandoned";
			case CTR_EOperationEnd.CANCELLED: return "#CTR-Result_TitleCancelled";
		}

		return "#CTR-Result_TitleComplete";
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnDialogOpened()
	{
		SetDialogWidth(WINDOW_WIDTH);

		// A line under the title, as far from it as from the line below.
		Widget content = OverlayWidget.Cast(GetRootWidget().FindAnyWidget("Content"));
		if (content)
		{
			float left, top, right, bottom;
			AlignableSlot.GetPadding(content, left, top, right, bottom);
			AlignableSlot.SetPadding(content, left, Math.Max(0, top - TITLE_GAP_CUT), right, bottom);
		}

		// In the gold of the section titles: an image takes its color as linear, so it is converted.
		Color gold = Color.FromSRGBA((COLOR_SECTION >> 16) & 0xFF, (COLOR_SECTION >> 8) & 0xFF, COLOR_SECTION & 0xFF, 255);
		AddRule(m_wHeader, gold, TITLE_RULE_HEIGHT, 0, 14);

		m_wAreas = AddHeaderLine(GetAreaNames());
		SetColor(m_wAreas, COLOR_MUTED);
		if (m_Result.m_bInProgress)
		{
			TextWidget note = AddHeaderLine("#CTR-Result_PaidAtEnd");
			SetColor(note, COLOR_MUTED);
		}

		if (m_bHeld)
		{
			m_wHeldLine = AddHeaderLine(GetHeldText());
			SetColor(m_wHeldLine, COLOR_LOSS);
			SetCloseShown(false);
			GetGame().GetCallqueue().CallLater(UpdateHeld, HELD_UPDATE_MS, true);
		}

		m_wList = CreateScrollList(m_wRows, Math.Clamp(GetScreenHeight() - RESERVED_HEIGHT, MIN_LIST_HEIGHT, MAX_LIST_HEIGHT));
		if (!m_wList)
			return;

		AddPersonalStats();
		AddTasks();
		AddTeamTotals();
		AddEarnings();

		if (m_Result.m_bInProgress)
			GetGame().GetCallqueue().CallLater(UpdateElapsed, 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	//! In progress: seconds since the screen opened, added to the times of the operation. 0 for a result.
	protected int GetElapsedSeconds()
	{
		if (!m_Result.m_bInProgress)
			return 0;

		return (System.GetTickCount() - m_iOpenTick) / 1000;
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateElapsed()
	{
		if (m_wAreas)
			m_wAreas.SetText(GetAreaNames());

		int elapsed = GetElapsedSeconds();
		if (m_wPersonalTime)
			m_wPersonalTime.SetText(FormatDuration(m_Result.m_Stats.m_iSeconds + elapsed));

		if (m_wOperationTime)
			m_wOperationTime.SetText(FormatDuration(m_Result.m_iDurationSeconds + elapsed));
	}

	//------------------------------------------------------------------------------------------------
	//! Missing in action: counts down, and closes once the player died. Lets go of them when they are still alive
	//! after the AO ended, or well after their time was up.
	protected void UpdateHeld()
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!character || !character.GetCharacterController() || character.GetCharacterController().GetLifeState() == ECharacterLifeState.DEAD)
		{
			Close();
			return;
		}

		COE_GameMode gameMode = COE_GameMode.GetInstance();
		if (!gameMode || !gameMode.CTR_HasOperation() || System.GetTickCount() - m_iHeldUntilTick > HELD_GRACE_MS)
		{
			Release();
			return;
		}

		if (m_wHeldLine)
			m_wHeldLine.SetText(GetHeldText());
	}

	//------------------------------------------------------------------------------------------------
	protected string GetHeldText()
	{
		int seconds = Math.Max(0, Math.Ceil((m_iHeldUntilTick - System.GetTickCount()) / 1000.0));
		return WidgetManager.Translate("#CTR-Result_MissingCountdown", seconds);
	}

	//------------------------------------------------------------------------------------------------
	protected void Release()
	{
		m_bHeld = false;
		GetGame().GetCallqueue().Remove(UpdateHeld);
		SetCloseShown(true);
		if (m_wHeldLine)
			m_wHeldLine.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetCloseShown(bool shown)
	{
		SCR_InputButtonComponent button = FindButton(BUTTON_CANCEL);
		if (button)
			button.SetVisible(shown, false);
	}

	//------------------------------------------------------------------------------------------------
	//! The Close button and its key (Esc) do nothing while the player is held.
	override protected void OnCancel()
	{
		if (m_bHeld)
			return;

		super.OnCancel();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		GetGame().GetCallqueue().Remove(UpdateElapsed);
		GetGame().GetCallqueue().Remove(UpdateHeld);
		if (s_Instance == this)
			s_Instance = null;

		super.OnMenuClose();
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

		return string.Format("%1  |  %2", names, FormatDuration(m_Result.m_iDurationSeconds + GetElapsedSeconds()));
	}

	//------------------------------------------------------------------------------------------------
	protected void AddEarnings()
	{
		CTR_Payout payout = m_Result.m_Payout;
		CTR_PlayerStats stats = m_Result.m_Stats;
		if (m_Result.m_bInProgress)
		{
			AddSection("#CTR-Result_EarningsSoFar");
			if (!stats.m_bEnteredAO)
				AddLine("#CTR-Result_NotEnteredYet", string.Empty, COLOR_MUTED);
			else if (payout.m_iTasks <= 0)
				AddLine("#CTR-Result_NoTaskYet", string.Empty, COLOR_MUTED);

			if (stats.m_bEnteredAO)
				AddPayLines();

			AddRule(m_wList, Color.FromInt(COLOR_RULE), TOTAL_RULE_HEIGHT, 6, 4);
			AddLine("#CTR-Result_IfSuccess", FormatAmount(m_Result.m_iTotalIfSuccess, m_Result.m_sCurrency), GetAmountColor(m_Result.m_iTotalIfSuccess), SECTION_FONT_SIZE);
			return;
		}

		AddSection("#CTR-Result_Earnings");
		if (m_Result.m_iPayPercent != 100)
			AddLine("#CTR-Result_PayShare", m_Result.m_iPayPercent.ToString() + "%", COLOR_LOSS);

		if (!stats.m_bEnteredAO)
			AddLine("#CTR-Result_NotEntered", string.Empty, COLOR_MUTED);
		else if (m_Result.CountCompletedTasks() == 0)
			AddLine("#CTR-Result_NoTask", string.Empty, COLOR_MUTED);
		else
			AddPayLines();

		AddRule(m_wList, Color.FromInt(COLOR_RULE), TOTAL_RULE_HEIGHT, 6, 4);
		AddLine("#CTR-Result_Total", FormatAmount(payout.m_iTotal, m_Result.m_sCurrency), GetAmountColor(payout.m_iTotal), SECTION_FONT_SIZE);
		string status = GetPayStatusText();
		if (!status.IsEmpty())
			AddLine(status, string.Empty, COLOR_MUTED);
	}

	//------------------------------------------------------------------------------------------------
	//! Completed tasks and the personal lines; after the operation the earnings at the share paid.
	protected void AddPayLines()
	{
		CTR_Payout payout = m_Result.m_Payout;
		CTR_PlayerStats stats = m_Result.m_Stats;
		foreach (CTR_TaskOutcome task : m_Result.m_aTasks)
		{
			if (!task.m_bCompleted)
				continue;

			int amount = CTR_PayoutCalculator.ApplyPercent(task.m_iAmount, m_Result.m_iPayPercent);
			AddLine(WidgetManager.Translate(task.m_sName), FormatAmount(amount, m_Result.m_sCurrency), GetAmountColor(amount));
		}

		AddCountLine("#CTR-Result_Kills", stats.m_iKills, payout.m_iKills);
		AddCountLine("#CTR-Result_Heals", stats.m_iHeals, payout.m_iHeals);
		AddCountLine("#CTR-Result_TeamKills", stats.m_iTeamKills, payout.m_iTeamKills);
		AddCountLine("#CTR-Result_CivilianKills", stats.m_iCivilianKills, payout.m_iCivilianKills);
		AddCountLine("#CTR-Result_Deaths", stats.m_iDeaths, payout.m_iDeaths);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddCountLine(string label, int count, int amount)
	{
		if (count == 0)
			return;

		AddLine(string.Format("%1 x%2", WidgetManager.Translate(label), count), FormatAmount(amount, m_Result.m_sCurrency), GetAmountColor(amount));
	}

	//------------------------------------------------------------------------------------------------
	//! Only when the pay did not go through as shown; empty when it was paid or there was nothing to pay.
	protected string GetPayStatusText()
	{
		switch (m_Result.m_ePayStatus)
		{
			case CTR_EPayStatus.ALREADY_PAID: return "#CTR-Result_AlreadyPaid";
			case CTR_EPayStatus.NO_OWNER: return "#CTR-Result_NoOwner";
			case CTR_EPayStatus.FAILED: return "#CTR-Result_PayFailed";
			case CTR_EPayStatus.PENDING: return "#CTR-Result_PayPending";
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	protected void AddPersonalStats()
	{
		AddSection("#CTR-Result_YourOperation");
		CTR_PlayerStats stats = m_Result.m_Stats;
		AddLine("#CTR-Result_Kills", stats.m_iKills.ToString());
		AddLine("#CTR-Result_Heals", stats.m_iHeals.ToString());
		AddLine("#CTR-Result_Deaths", stats.m_iDeaths.ToString());
		AddLine("#CTR-Result_Shots", stats.m_iShots.ToString());
		AddLine("#CTR-Result_Distance", FormatDistance(stats.m_fDistance));
		m_wPersonalTime = AddLine("#CTR-Result_PersonalTime", FormatDuration(stats.m_iSeconds + GetElapsedSeconds()));
	}

	//------------------------------------------------------------------------------------------------
	protected void AddTasks()
	{
		AddSection("#CTR-Result_Tasks");
		foreach (CTR_TaskOutcome task : m_Result.m_aTasks)
		{
			string outcome = "#CTR-Result_TaskFailed";
			int color = COLOR_LOSS;
			if (task.m_bCompleted)
			{
				outcome = "#CTR-Result_TaskCompleted";
				color = COLOR_GAIN;
			}
			else if (!task.m_bFailed)
			{
				outcome = "#CTR-Result_TaskNotFinished";
				if (m_Result.m_bInProgress)
					outcome = "#CTR-Result_TaskInProgress";

				color = COLOR_MUTED;
			}

			// While it runs, what each task is worth.
			if (m_Result.m_bInProgress && !task.m_bFailed)
				outcome = string.Format("%1   %2", FormatAmount(task.m_iReward, m_Result.m_sCurrency), WidgetManager.Translate(outcome));

			AddLine(WidgetManager.Translate("#CTR-Result_TaskGrid", WidgetManager.Translate(task.m_sName), FormatGrid(task.m_fX, task.m_fZ)), outcome, color);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void AddTeamTotals()
	{
		AddSection("#CTR-Result_Team");
		AddLine("#CTR-Result_TasksCompleted", string.Format("%1 / %2", m_Result.CountCompletedTasks(), m_Result.m_aTasks.Count()));
		AddLine("#CTR-Result_Participants", m_Result.m_iParticipants.ToString());
		if (m_Result.m_bInProgress)
			AddLine("#CTR-Result_TeamIfSuccess", MRX_TextFormat.Money(m_Result.m_iTeamPay, m_Result.m_sCurrency));
		else
			AddLine("#CTR-Result_TeamTotal", MRX_TextFormat.Money(m_Result.m_iTeamPay, m_Result.m_sCurrency));
		m_wOperationTime = AddLine("#CTR-Result_OperationTime", FormatDuration(m_Result.m_iDurationSeconds + GetElapsedSeconds()));
	}

	//------------------------------------------------------------------------------------------------
	//! A line across the window, between parts.
	protected void AddRule(Widget parent, Color color, float height, float top, float bottom)
	{
		if (!parent)
			return;

		ImageWidget rule = ImageWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.ImageWidgetTypeID, WIDGET_FLAGS, color, 0, parent));
		if (!rule)
			return;

		// At least one screen pixel: thinner, it is drawn or not depending on where it falls on a scaled-down screen.
		rule.SetSize(1, Math.Max(height, GetGame().GetWorkspace().DPIUnscale(1)));
		AlignableSlot.SetHorizontalAlign(rule, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetPadding(rule, 0, top, 0, bottom);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddSection(string title)
	{
		TextWidget text = CreateText(m_wList, title);
		text.SetExactFontSize(SECTION_FONT_SIZE);
		SetColor(text, COLOR_SECTION);
		AlignableSlot.SetPadding(text, 0, 20, 0, 4);
	}

	//------------------------------------------------------------------------------------------------
	//! Label on the left, value on the right. \param fontSize 0 keeps the dialog's size. \return The value text.
	protected TextWidget AddLine(string label, string value, int valueColor = Color.WHITE, int fontSize = 0)
	{
		Widget row = CreateLayout(WidgetType.HorizontalLayoutWidgetTypeID, m_wList);
		AlignableSlot.SetHorizontalAlign(row, LayoutHorizontalAlign.Stretch);
		TextWidget labelText = CreateText(row, label);
		LayoutSlot.SetSizeMode(labelText, LayoutSizeMode.Fill);
		TextWidget valueText = CreateText(row, value);
		SetColor(valueText, valueColor);
		AlignableSlot.SetPadding(valueText, 16, 0, 8, 0);
		if (fontSize > 0)
		{
			labelText.SetExactFontSize(fontSize);
			valueText.SetExactFontSize(fontSize);
		}

		return valueText;
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

		return string.Format("%1:%2", minutes.ToString(2), rest.ToString(2));
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
