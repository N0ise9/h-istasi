class HST_BuildInfo
{
	static const string BUILD_SHA = "402b3531a5a150dba51f6063b6936c76dd6db682";
	static const string BUILD_UTC = "2026-07-17T18:26:37Z";
	static const string BUILD_LABEL = "schema71-settings24-garrison-rebuild-restart";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
