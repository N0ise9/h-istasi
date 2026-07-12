class HST_BuildInfo
{
	static const string BUILD_SHA = "6f3c913eaed66926cce38b2ecafcff94084898a3";
	static const string BUILD_UTC = "2026-07-12T11:28:41Z";
	static const string BUILD_LABEL = "schema64-canonical-town-influence";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
