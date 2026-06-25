enum HST_EUIScreenMode
{
	NONE,
	SETUP_MAP,
	COMMAND_MENU,
	LOADOUT_EDITOR,
	MISSION_DIALOG,
	GAMEPLAY_MAP_OVERLAY
}

class HST_UIConstants
{
	static const int Z_SETUP_MAP = 2200;
	static const int Z_COMMAND_MENU = 2500;
	static const int Z_NOTIFICATION = 2850;
	static const int Z_LOADOUT_EDITOR = 3600;
	static const int Z_MISSION_DIALOG = 4300;

	static string ModeName(HST_EUIScreenMode mode)
	{
		switch (mode)
		{
			case HST_EUIScreenMode.SETUP_MAP:
				return "setup_map";
			case HST_EUIScreenMode.COMMAND_MENU:
				return "command_menu";
			case HST_EUIScreenMode.LOADOUT_EDITOR:
				return "loadout_editor";
			case HST_EUIScreenMode.MISSION_DIALOG:
				return "mission_dialog";
			case HST_EUIScreenMode.GAMEPLAY_MAP_OVERLAY:
				return "gameplay_map_overlay";
		}

		return "none";
	}
}
