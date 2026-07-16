class HST_BuildInfo
{
	static const string BUILD_SHA = "7eb0a98977c523f6713a9e2088eab7ba20a333fd";
	static const string BUILD_UTC = "2026-07-16T17:12:17Z";
	static const string BUILD_LABEL = "schema70-settings24-counterattack-owner-applied-restart";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
