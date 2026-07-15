class HST_BuildInfo
{
	static const string BUILD_SHA = "4757bc86ffbc7a5fa08e64a9abf7ef74ddc1c003";
	static const string BUILD_UTC = "2026-07-15T05:21:40Z";
	static const string BUILD_LABEL = "schema70-settings24-native-counterattack-projection";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
