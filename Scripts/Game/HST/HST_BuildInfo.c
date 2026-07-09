class HST_BuildInfo
{
	static const string BUILD_SHA = "r121-training-war-cap";
	static const string BUILD_UTC = "2026-07-09T03:31:00Z";
	static const string BUILD_LABEL = "h-istasi-live-runtime-proof-r121-training-war-cap";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}
}
