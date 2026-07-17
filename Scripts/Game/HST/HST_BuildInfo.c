class HST_BuildInfo
{
	static const string BUILD_SHA = "85572fca9340074c3c198c758f857c4f57b600d9";
	static const string BUILD_UTC = "2026-07-17T09:37:00Z";
	static const string BUILD_LABEL = "schema71-settings24-campaign-recovery-journal";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
