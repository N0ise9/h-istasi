class HST_BuildInfo
{
	static const string BUILD_SHA = "a8ebe54fca7260075813e65920960bb21b1fd47f";
	static const string BUILD_UTC = "2026-07-14T11:41:04Z";
	static const string BUILD_LABEL = "schema70-settings24-radio-lifecycle-fixture-source";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}

	static string BuildRuntimeSummary()
	{
		return BuildSummary() + string.Format(" | campaign schema %1 | settings schema %2", HST_CampaignState.SCHEMA_VERSION, HST_RuntimeSettings.SCHEMA_VERSION);
	}
}
