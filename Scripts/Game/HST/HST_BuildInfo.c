class HST_BuildInfo
{
	static const string BUILD_SHA = "7fdf3988797edeb747f5d6a6951ad0382bd93db3";
	static const string BUILD_UTC = "2026-07-21T19:36:22Z";
	static const string BUILD_LABEL = "schema71-settings24-gate1-release-surface";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
