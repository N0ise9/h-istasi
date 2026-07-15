class HST_BuildInfo
{
	static const string BUILD_SHA = "78db295a02936aa66899203cb33e50462b5fd557";
	static const string BUILD_UTC = "2026-07-15T00:08:27Z";
	static const string BUILD_LABEL = "schema70-settings24-exact-qrf-prepared-recovery";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
