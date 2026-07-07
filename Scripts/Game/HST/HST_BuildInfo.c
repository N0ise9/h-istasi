class HST_BuildInfo
{
	static const string BUILD_SHA = "r81-relation-order-decision-proof";
	static const string BUILD_UTC = "2026-07-07T23:40:08Z";
	static const string BUILD_LABEL = "h-istasi-live-runtime-proof-r81-relation-order-decision-proof";

	static string BuildSummary()
	{
		return string.Format("sha %1 | utc %2 | label %3", BUILD_SHA, BUILD_UTC, BUILD_LABEL);
	}
}
