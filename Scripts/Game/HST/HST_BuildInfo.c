class HST_BuildInfo
{
	static const string BUILD_SHA = "b5eaa91a081a41d9460b543ac8c87380d78cca6c";
	static const string BUILD_UTC = "2026-07-11T01:30:41Z";
	static const string BUILD_LABEL = "schema51-exact-enemy-defensive-qrf";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
