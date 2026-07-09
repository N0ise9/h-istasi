class HST_BuildInfo
{
	static const string BUILD_SHA = "r123-marker-group-civilian-fixes";
	static const string BUILD_UTC = "2026-07-09T04:28:00Z";
	static const string BUILD_LABEL = "h-istasi-live-runtime-proof-r123-marker-group-civilian-fixes";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}
}
