class HST_BuildInfo
{
	static const string BUILD_SHA = "3d1f2a7720c7a8fd93a80680044230b5d41b7f13";
	static const string BUILD_UTC = "2026-07-10T03:51:06Z";
	static const string BUILD_LABEL = "campaign-runtime-integrity-exact-force-spawn-adapter";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
