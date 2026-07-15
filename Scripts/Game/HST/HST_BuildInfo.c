class HST_BuildInfo
{
	static const string BUILD_SHA = "e596d986a1c85ddffeee414fee4c6c3d3559c29f";
	static const string BUILD_UTC = "2026-07-15T22:28:37Z";
	static const string BUILD_LABEL = "schema70-settings24-counterattack-virtual-restart-proof";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
