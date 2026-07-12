class HST_BuildInfo
{
	static const string BUILD_SHA = "6afadc7c13681b78171939a740862e52328beffd";
	static const string BUILD_UTC = "2026-07-12T15:57:55Z";
	static const string BUILD_LABEL = "schema64-settings24-ambient-runtime-authority";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
