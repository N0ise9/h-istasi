class HST_ProfilePathService
{
	static const string PROFILE_DIRECTORY = "$profile:Partisan";
	static const string SETTINGS_FILE = "$profile:Partisan/HST_Settings.json";
	static const string CAMPAIGN_SAVE_FILE = "$profile:Partisan/HST_CampaignSaveData.json";
	static const string VISUAL_SETTINGS_FILE = "$profile:Partisan/HST_LoadoutEditorSettings.json";
	static const string LOADOUT_DIRECTORY = "$profile:Partisan/loadouts";
	static const string LOADOUT_DIRECTORY_V2 = "$profile:Partisan/loadouts/v2";
	static const string DEBUG_DIRECTORY = "$profile:Partisan/debug";

	static const string LEGACY_PROFILE_DIRECTORY = "$profile:h-istasi";
	static const string LEGACY_SETTINGS_FILE = "$profile:h-istasi/HST_Settings.json";
	static const string LEGACY_CAMPAIGN_SAVE_FILE = "$profile:h-istasi/HST_CampaignSaveData.json";
	static const string LEGACY_VISUAL_SETTINGS_FILE = "$profile:h-istasi/HST_LoadoutEditorSettings.json";
	static const string LEGACY_LOADOUT_DIRECTORY = "$profile:h-istasi/loadouts";
	static const string LEGACY_LOADOUT_DIRECTORY_V2 = "$profile:h-istasi/loadouts/v2";

	static string ResolveReadableFile(string currentPath, string legacyPath)
	{
		if (FileIO.FileExists(currentPath))
			return currentPath;
		if (FileIO.FileExists(legacyPath))
			return legacyPath;
		return currentPath;
	}

	static bool IsLegacyPath(string path)
	{
		return !path.IsEmpty()
			&& (path == LEGACY_PROFILE_DIRECTORY
				|| path.StartsWith(LEGACY_PROFILE_DIRECTORY + "/"));
	}

	static void EnsureProfileDirectory()
	{
		FileIO.MakeDirectory(PROFILE_DIRECTORY);
	}

	static void EnsureLoadoutDirectories()
	{
		EnsureProfileDirectory();
		FileIO.MakeDirectory(LOADOUT_DIRECTORY);
		FileIO.MakeDirectory(LOADOUT_DIRECTORY_V2);
	}

	static void EnsureDebugDirectory()
	{
		EnsureProfileDirectory();
		FileIO.MakeDirectory(DEBUG_DIRECTORY);
	}
}
