class HST_BuildInfo
{
	static const string BUILD_SHA = "de3db34+convoyui";
	static const string BUILD_UTC = "2026-07-06T13:00:25Z";
	static const string BUILD_LABEL = "h-istasi-live-runtime-proof-r13-convoy-ui";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}
}
