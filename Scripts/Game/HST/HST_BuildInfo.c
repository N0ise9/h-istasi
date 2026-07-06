class HST_BuildInfo
{
	static const string BUILD_SHA = "45a9508+strict-faction-proof";
	static const string BUILD_UTC = "2026-07-06T13:24:00Z";
	static const string BUILD_LABEL = "h-istasi-live-runtime-proof-r16-strict-faction-proof";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}
}
