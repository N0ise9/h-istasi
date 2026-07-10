class HST_BuildInfo
{
	static const string BUILD_SHA = "fdfd1058f761704c2b2a05c947036e30afa34161";
	static const string BUILD_UTC = "2026-07-10T16:01:42Z";
	static const string BUILD_LABEL = "schema48-authoritative-convoy-seating";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
