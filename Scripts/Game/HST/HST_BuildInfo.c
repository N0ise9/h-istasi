class HST_BuildInfo
{
	static const string BUILD_SHA = "r120-population-income-scaling";
	static const string BUILD_UTC = "2026-07-09T03:28:18Z";
	static const string BUILD_LABEL = "h-istasi-live-runtime-proof-r120-population-income-scaling";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}
}
