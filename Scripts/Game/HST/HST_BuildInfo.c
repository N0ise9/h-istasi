class HST_BuildInfo
{
	static const string BUILD_SHA = "02713ba432732cd09a8958cde5b499f88432dfa3";
	static const string BUILD_UTC = "2026-07-10T13:28:34Z";
	static const string BUILD_LABEL = "schema48-force-settlement-archive";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
