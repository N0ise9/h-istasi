class HST_BuildInfo
{
	static const string BUILD_SHA = "691134ec3345fb7350dcc5aaf1be720e29f41a62";
	static const string BUILD_UTC = "2026-07-10T15:32:07Z";
	static const string BUILD_LABEL = "schema48-civilian-group-projection";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
