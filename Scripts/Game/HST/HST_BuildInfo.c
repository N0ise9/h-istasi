class HST_BuildInfo
{
	static const string BUILD_SHA = "5cd7254d20d97d58d40c97fce64372b7639bb82d";
	static const string BUILD_UTC = "2026-07-10T00:36:37Z";
	static const string BUILD_LABEL = "campaign-runtime-integrity-workbench-proof-stability";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
