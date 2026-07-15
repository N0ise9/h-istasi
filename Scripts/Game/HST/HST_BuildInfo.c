class HST_BuildInfo
{
	static const string BUILD_SHA = "ad54861f639b020627492f122f39f7a6cbc5a929";
	static const string BUILD_UTC = "2026-07-15T08:26:21Z";
	static const string BUILD_LABEL = "schema70-settings24-native-counterattack-physical-settle";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
