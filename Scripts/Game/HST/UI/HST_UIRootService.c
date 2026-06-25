class HST_UIRootService
{
	protected static ref HST_UIRootService s_Instance;

	protected ref HST_UIScreenBase m_CurrentScreen;
	protected ref HST_UIScreenBase m_ModalScreen;
	protected int m_iRevision;
	protected int m_iNotificationDepth;

	static HST_UIRootService Get()
	{
		if (!s_Instance)
			s_Instance = new HST_UIRootService();

		return s_Instance;
	}

	bool RequestOpen(HST_EUIScreenMode mode, string owner, Widget root = null, bool blocksGameplay = false, bool blocksMap = false, bool modal = false)
	{
		if (mode == HST_EUIScreenMode.NONE)
			return false;

		if (!CanOpen(mode))
		{
			Print(string.Format("h-istasi ui root | refused open mode=%1 owner=%2 current=%3", HST_UIConstants.ModeName(mode), owner, HST_UIConstants.ModeName(GetCurrentMode())), LogLevel.WARNING);
			return false;
		}

		if (modal || mode == HST_EUIScreenMode.MISSION_DIALOG)
		{
			if (!m_ModalScreen)
				m_ModalScreen = new HST_UIScreenBase();

			m_ModalScreen.Configure(mode, owner, root, blocksGameplay, blocksMap, true);
			m_iRevision++;
			return true;
		}

		if (!m_CurrentScreen)
			m_CurrentScreen = new HST_UIScreenBase();
		m_CurrentScreen.Configure(mode, owner, root, blocksGameplay, blocksMap, modal);
		m_iRevision++;
		return true;
	}

	void NotifyClosed(HST_EUIScreenMode mode, string owner = "")
	{
		if (m_ModalScreen && m_ModalScreen.Matches(mode, owner))
		{
			m_ModalScreen = null;
			m_iRevision++;
			return;
		}

		if (!m_CurrentScreen || !m_CurrentScreen.Matches(mode, owner))
			return;

		m_CurrentScreen = null;
		m_iRevision++;
	}

	void NotifyNotificationShown()
	{
		m_iNotificationDepth++;
	}

	void NotifyNotificationHidden()
	{
		if (m_iNotificationDepth > 0)
			m_iNotificationDepth--;
	}

	bool CanOpen(HST_EUIScreenMode mode)
	{
		HST_EUIScreenMode current = GetCurrentMode();
		if (mode == HST_EUIScreenMode.MISSION_DIALOG)
			return current != HST_EUIScreenMode.SETUP_MAP;

		if (current == HST_EUIScreenMode.NONE || current == mode)
			return true;

		if (current == HST_EUIScreenMode.SETUP_MAP)
			return false;

		if (mode == HST_EUIScreenMode.COMMAND_MENU && current == HST_EUIScreenMode.LOADOUT_EDITOR)
			return false;

		if (mode == HST_EUIScreenMode.LOADOUT_EDITOR && current == HST_EUIScreenMode.COMMAND_MENU)
			return true;

		return true;
	}

	HST_EUIScreenMode GetCurrentMode()
	{
		if (!m_CurrentScreen)
			return HST_EUIScreenMode.NONE;

		return m_CurrentScreen.m_eMode;
	}

	bool IsGameplayBlocked()
	{
		if (m_CurrentScreen && m_CurrentScreen.m_bBlocksGameplay)
			return true;

		return m_ModalScreen && m_ModalScreen.m_bBlocksGameplay;
	}

	bool IsMapBlocked()
	{
		if (m_CurrentScreen && m_CurrentScreen.m_bBlocksMap)
			return true;

		return m_ModalScreen && m_ModalScreen.m_bBlocksMap;
	}

	bool IsModalOpen()
	{
		return m_ModalScreen != null;
	}

	bool IsNotificationVisible()
	{
		return m_iNotificationDepth > 0;
	}

	int GetRevision()
	{
		return m_iRevision;
	}
}
