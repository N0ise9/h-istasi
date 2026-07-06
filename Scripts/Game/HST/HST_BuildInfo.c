class HST_BuildInfo
{
	static const string BUILD_SHA = "e105448+convoyproof";
	static const string BUILD_UTC = "2026-07-06T04:57:06Z";
	static const string BUILD_LABEL = "h-istasi-live-runtime-proof-r4";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}
}
