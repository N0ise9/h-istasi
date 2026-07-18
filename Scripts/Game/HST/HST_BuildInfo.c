class HST_BuildInfo
{
	static const string BUILD_SHA = "32727238d74b29905c68e5a80bb5897dfdc783c0";
	static const string BUILD_UTC = "2026-07-18T16:34:38Z";
	static const string BUILD_LABEL = "schema71-settings24-focused-force-authority";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
