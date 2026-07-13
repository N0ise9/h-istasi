class HST_BuildInfo
{
	static const string BUILD_SHA = "fdf262637e74a70c12454f6c1d3789c2cd0a0f05";
	static const string BUILD_UTC = "2026-07-13T13:19:22Z";
	static const string BUILD_LABEL = "schema68-settings24-bootstrap-profile-marker-hardening";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
