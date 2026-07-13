class HST_BuildInfo
{
	static const string BUILD_SHA = "5bdcda938840ab769b41ff3e1856d908572a8c45";
	static const string BUILD_UTC = "2026-07-13T19:40:35Z";
	static const string BUILD_LABEL = "schema69-settings24-exact-enemy-counterattack-engine-proof";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
