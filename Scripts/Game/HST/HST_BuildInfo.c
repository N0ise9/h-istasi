class HST_BuildInfo
{
	static const string BUILD_SHA = "e2549e423c6b13dbad5040f6907065e0b0f64f10";
	static const string BUILD_UTC = "2026-07-09T22:52:47Z";
	static const string BUILD_LABEL = "campaign-runtime-integrity-marker-readiness";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
