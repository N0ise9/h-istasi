class HST_BuildInfo
{
	static const string BUILD_SHA = "609add9eeadf73816764c497178e2d35081307d1";
	static const string BUILD_UTC = "2026-07-12T18:30:29Z";
	static const string BUILD_LABEL = "schema65-settings24-civilian-consequence-authority";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
