class HST_BuildInfo
{
	static const string BUILD_SHA = "fa5e7e45dbd8741269e614e60c51d4edee6bf223";
	static const string BUILD_UTC = "2026-07-11T05:41:21Z";
	static const string BUILD_LABEL = "schema52-exact-mission-convoy";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
