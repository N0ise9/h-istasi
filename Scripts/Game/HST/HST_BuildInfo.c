class HST_BuildInfo
{
	static const string BUILD_SHA = "r50-undercover-vehicle-heat";
	static const string BUILD_UTC = "2026-07-07T15:20:00Z";
	static const string BUILD_LABEL = "h-istasi-live-runtime-proof-r50-undercover-vehicle-heat";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}
}
