class HST_BuildInfo
{
	static const string BUILD_SHA = "695caf46ce6b4146e5407711b76d5e0c578d7392";
	static const string BUILD_UTC = "2026-07-13T14:44:37Z";
	static const string BUILD_LABEL = "schema68-settings24-commitment-aware-enemy-planning";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
