class HST_BuildInfo
{
	static const string BUILD_SHA = "f0ba07ff2bc295d12542a3ea34b4c913e99b1869";
	static const string BUILD_UTC = "2026-07-11T16:43:51Z";
	static const string BUILD_LABEL = "schema58-exact-rescue-pows";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
