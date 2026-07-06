class HST_BuildInfo
{
	static const string BUILD_SHA = "r19-faction-visual-spawn-proof";
	static const string BUILD_UTC = "2026-07-06T14:26:45Z";
	static const string BUILD_LABEL = "h-istasi-live-runtime-proof-r19-faction-visual-spawn-proof";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}
}
