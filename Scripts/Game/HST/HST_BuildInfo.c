class HST_BuildInfo
{
	static const string BUILD_SHA = "r44-editable-group-membership";
	static const string BUILD_UTC = "2026-07-07T09:35:00Z";
	static const string BUILD_LABEL = "h-istasi-live-runtime-proof-r44-editable-group-membership";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}
}
