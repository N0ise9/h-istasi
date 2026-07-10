class HST_BuildInfo
{
	static const string BUILD_SHA = "456c73524200b45ce933cca783900813be510dbf";
	static const string BUILD_UTC = "2026-07-10T17:01:02Z";
	static const string BUILD_LABEL = "schema48-physical-support-route-truth";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
