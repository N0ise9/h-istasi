class HST_BuildInfo
{
	static const string BUILD_SHA = "40f1c9f+convoypost";
	static const string BUILD_UTC = "2026-07-06T05:36:24Z";
	static const string BUILD_LABEL = "h-istasi-live-runtime-proof-r9-convoypost";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}
}
