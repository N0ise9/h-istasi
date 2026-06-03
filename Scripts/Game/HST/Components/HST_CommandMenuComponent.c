[ComponentEditorProps(category: "h-istasi", description: "Client-side h-istasi alpha command menu key listener")]
class HST_CommandMenuComponentClass : SCR_BaseGameModeComponentClass
{
}

class HST_CommandMenuComponent : SCR_BaseGameModeComponent
{
	static const string COMMAND_MENU_ACTION = "Inventory";
	static const string COMMAND_MENU_CUSTOM_ACTION = "HST_CommandMenu";
	static const string COMMAND_MENU_UP_ACTION = "MenuUp";
	static const string COMMAND_MENU_DOWN_ACTION = "MenuDown";
	static const string COMMAND_MENU_SELECT_ACTION = "MenuSelect";
	static const string COMMAND_MENU_BACK_ACTION = "MenuBack";

	protected bool m_bMenuOpen;
	protected int m_iSelectedAction;
	protected ref array<string> m_aActionLabels = {};
	protected ref array<string> m_aActionCommands = {};
	protected ref array<string> m_aActionArguments = {};

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		RegisterInputListeners();
		BuildActionList();
	}

	override void OnDelete(IEntity owner)
	{
		UnregisterInputListeners();
		super.OnDelete(owner);
	}

	protected void RegisterInputListeners()
	{
		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;

		inputManager.AddActionListener(COMMAND_MENU_ACTION, EActionTrigger.DOWN, ToggleMenu);
		inputManager.AddActionListener(COMMAND_MENU_CUSTOM_ACTION, EActionTrigger.DOWN, ToggleMenu);
		inputManager.AddActionListener(COMMAND_MENU_UP_ACTION, EActionTrigger.DOWN, SelectPreviousAction);
		inputManager.AddActionListener(COMMAND_MENU_DOWN_ACTION, EActionTrigger.DOWN, SelectNextAction);
		inputManager.AddActionListener(COMMAND_MENU_SELECT_ACTION, EActionTrigger.DOWN, ExecuteSelectedAction);
		inputManager.AddActionListener(COMMAND_MENU_BACK_ACTION, EActionTrigger.DOWN, CloseMenu);
	}

	protected void UnregisterInputListeners()
	{
		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;

		inputManager.RemoveActionListener(COMMAND_MENU_ACTION, EActionTrigger.DOWN, ToggleMenu);
		inputManager.RemoveActionListener(COMMAND_MENU_CUSTOM_ACTION, EActionTrigger.DOWN, ToggleMenu);
		inputManager.RemoveActionListener(COMMAND_MENU_UP_ACTION, EActionTrigger.DOWN, SelectPreviousAction);
		inputManager.RemoveActionListener(COMMAND_MENU_DOWN_ACTION, EActionTrigger.DOWN, SelectNextAction);
		inputManager.RemoveActionListener(COMMAND_MENU_SELECT_ACTION, EActionTrigger.DOWN, ExecuteSelectedAction);
		inputManager.RemoveActionListener(COMMAND_MENU_BACK_ACTION, EActionTrigger.DOWN, CloseMenu);
	}

	protected void BuildActionList()
	{
		m_aActionLabels.Clear();
		m_aActionCommands.Clear();
		m_aActionArguments.Clear();

		AddAction("Campaign overview", "inspect_campaign", "");
		AddAction("Marker status", "inspect_markers", "");
		AddAction("Economy report", "inspect_economy", "");
		AddAction("Zone report", "inspect_zones", "");
		AddAction("Mission report", "inspect_missions", "");
		AddAction("Manual checkpoint", "checkpoint", "");
		AddAction("Apply income tick", "income_now", "");
		AddAction("Train FIA troops", "train_troops", "");
		AddAction("Move HQ: north forest", "move_hq", "hideout_north_forest");
		AddAction("Move HQ: central hills", "move_hq", "hideout_central_hills");
		AddAction("Move HQ: south woods", "move_hq", "hideout_south_woods");
		AddAction("Recruit FIA at Morton", "recruit_zone", "town_morton");
		AddAction("Start mission at Morton", "mission_zone", "town_morton");
		AddAction("Debug capture Morton", "capture_zone", "town_morton");
		AddAction("Debug activate Morton", "activate_zone", "town_morton");
		AddAction("Debug award resources", "award_small", "");
	}

	protected void AddAction(string label, string commandId, string argument)
	{
		m_aActionLabels.Insert(label);
		m_aActionCommands.Insert(commandId);
		m_aActionArguments.Insert(argument);
	}

	protected void ToggleMenu()
	{
		if (m_bMenuOpen)
		{
			CloseMenu();
			return;
		}

		OpenMenu();
	}

	protected void OpenMenu()
	{
		m_bMenuOpen = true;
		if (m_iSelectedAction < 0 || m_iSelectedAction >= m_aActionLabels.Count())
			m_iSelectedAction = 0;

		Print(BuildMenuText());
	}

	protected void CloseMenu()
	{
		if (!m_bMenuOpen)
			return;

		m_bMenuOpen = false;
		Print("h-istasi menu | closed");
	}

	protected void SelectPreviousAction()
	{
		if (!m_bMenuOpen || m_aActionLabels.Count() == 0)
			return;

		m_iSelectedAction--;
		if (m_iSelectedAction < 0)
			m_iSelectedAction = m_aActionLabels.Count() - 1;

		Print(BuildMenuText());
	}

	protected void SelectNextAction()
	{
		if (!m_bMenuOpen || m_aActionLabels.Count() == 0)
			return;

		m_iSelectedAction++;
		if (m_iSelectedAction >= m_aActionLabels.Count())
			m_iSelectedAction = 0;

		Print(BuildMenuText());
	}

	protected void ExecuteSelectedAction()
	{
		if (!m_bMenuOpen || m_iSelectedAction < 0 || m_iSelectedAction >= m_aActionCommands.Count())
			return;

		HST_CampaignCoordinatorComponent coordinator = HST_CampaignCoordinatorComponent.GetInstance();
		if (!coordinator)
		{
			Print("h-istasi menu | campaign coordinator not ready", LogLevel.WARNING);
			return;
		}

		int playerId = ResolveLocalPlayerId();
		string commandId = m_aActionCommands[m_iSelectedAction];
		string argument = m_aActionArguments[m_iSelectedAction];
		bool success = coordinator.RequestAlphaUICommand(playerId, commandId, argument);
		Print(string.Format("h-istasi menu | %1 %2", m_aActionLabels[m_iSelectedAction], success));
		Print(BuildMenuText());
	}

	protected string BuildMenuText()
	{
		HST_CampaignCoordinatorComponent coordinator = HST_CampaignCoordinatorComponent.GetInstance();
		int playerId = ResolveLocalPlayerId();
		string header = "h-istasi menu | I close/open | MenuUp/MenuDown select | MenuSelect run | MenuBack close";
		string report;
		if (coordinator)
		{
			report = coordinator.GetAlphaAdminMenu(playerId);
			if (report.IsEmpty())
				report = coordinator.GetAlphaCommanderMenu(playerId);
			if (report.IsEmpty())
				report = coordinator.GetAlphaMemberMenu(playerId);
		}

		string actions = "";
		for (int i = 0; i < m_aActionLabels.Count(); i++)
		{
			string prefix = "  ";
			if (i == m_iSelectedAction)
				prefix = "> ";

			actions = actions + string.Format("\n%1%2", prefix, m_aActionLabels[i]);
		}

		if (report.IsEmpty())
			report = "h-istasi menu | player is not registered as a member/commander/admin yet";

		return header + "\n" + report + actions;
	}

	protected int ResolveLocalPlayerId()
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return 1;

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		if (playerIds.Count() == 0)
			return 1;

		return playerIds[0];
	}
}
