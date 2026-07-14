class HST_BuildInfo
{
	static const string BUILD_SHA = "6303e5817a924091258c9cf0dbccdd2e0731c1e3";
	static const string BUILD_UTC = "2026-07-14T19:33:16Z";
	static const string BUILD_LABEL = "schema70-settings24-exact-qrf-refund-replay";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
