class HST_BuildInfo
{
	static const string BUILD_SHA = "r20-direct-member-visual-proof";
	static const string BUILD_UTC = "2026-07-06T14:29:35Z";
	static const string BUILD_LABEL = "h-istasi-live-runtime-proof-r20-direct-member-visual-proof";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}
}
