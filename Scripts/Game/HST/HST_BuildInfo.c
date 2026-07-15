class HST_BuildInfo
{
	static const string BUILD_SHA = "b6395475b664b513ed371a7a97a13508722a438d";
	static const string BUILD_UTC = "2026-07-15T07:41:58Z";
	static const string BUILD_LABEL = "schema70-settings24-native-counterattack-casualty-settle";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
