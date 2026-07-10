class HST_BuildInfo
{
	static const string BUILD_SHA = "36fc8e1f075eaad2164ad71f14ddb951a68550b4";
	static const string BUILD_UTC = "2026-07-10T01:51:42Z";
	static const string BUILD_LABEL = "campaign-runtime-integrity-spawn-queue-kernel";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
