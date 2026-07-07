class HST_BuildInfo
{
	static const string BUILD_SHA = "r76-convoy-ai-vehicle-usage";
	static const string BUILD_UTC = "2026-07-07T22:58:53Z";
	static const string BUILD_LABEL = "h-istasi-live-runtime-proof-r76-convoy-ai-vehicle-usage";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}
}
