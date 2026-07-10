class HST_BuildInfo
{
	static const string BUILD_SHA = "1ce502a3840dfa2b19adea1309c0ffefa675a60b";
	static const string BUILD_UTC = "2026-07-10T20:50:35Z";
	static const string BUILD_LABEL = "schema49-exact-qrf-operation-authority";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
