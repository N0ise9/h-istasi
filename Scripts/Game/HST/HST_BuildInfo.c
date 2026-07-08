class HST_BuildInfo
{
	static const string BUILD_SHA = "r111-town-police-prefabs";
	static const string BUILD_UTC = "2026-07-08T18:33:12Z";
	static const string BUILD_LABEL = "h-istasi-live-runtime-proof-r111-town-police-prefabs";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}
}
