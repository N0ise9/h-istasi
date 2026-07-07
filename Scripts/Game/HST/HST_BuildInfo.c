class HST_BuildInfo
{
	static const string BUILD_SHA = "r47-enemy-support-spend";
	static const string BUILD_UTC = "2026-07-07T12:15:00Z";
	static const string BUILD_LABEL = "h-istasi-live-runtime-proof-r47-enemy-support-spend";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}
}
