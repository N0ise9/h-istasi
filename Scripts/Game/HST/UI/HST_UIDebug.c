class HST_UIDebug
{
	static const bool LAYOUT_DEBUG_ENABLED = true;
	static const bool LAYOUT_WIDGET_DEBUG_ENABLED = true;
	static const bool LAYOUT_GEOMETRY_DEBUG_ENABLED = true;
	static const bool LAYOUT_POPULATION_DEBUG_ENABLED = true;
	static const bool LAYOUT_ROW_SAMPLE_DEBUG_ENABLED = true;
	static const int LAYOUT_ROW_SAMPLE_LIMIT = 8;

	static void LogLayoutCreate(string owner, ResourceName layout, Widget root, Widget parent = null)
	{
		if (!LAYOUT_DEBUG_ENABLED)
			return;

		if (!root)
		{
			Print(string.Format("h-istasi ui layout debug | %1 | create failed | layout=%2 parent=%3", owner, layout, WidgetSummary(parent)), LogLevel.WARNING);
			return;
		}

		Print(string.Format("h-istasi ui layout debug | %1 | created | layout=%2 root=%3 parent=%4", owner, layout, WidgetSummary(root), WidgetSummary(parent)));
	}

	static void LogLayoutRejected(string owner, ResourceName layout, Widget root, string reason)
	{
		if (!LAYOUT_DEBUG_ENABLED)
			return;

		Print(string.Format("h-istasi ui layout debug | %1 | rejected | layout=%2 root=%3 reason=%4", owner, layout, WidgetSummary(root), reason), LogLevel.WARNING);
	}

	static void LogExpectedWidgets(string owner, Widget root, array<string> widgetNames)
	{
		if (!LAYOUT_WIDGET_DEBUG_ENABLED)
			return;
		if (!widgetNames)
			return;

		if (!root)
		{
			Print(string.Format("h-istasi ui layout debug | %1 | widget check skipped | root=null expected=%2", owner, widgetNames.Count()), LogLevel.WARNING);
			return;
		}

		string missing = "";
		int found;
		foreach (string widgetName : widgetNames)
		{
			if (widgetName.IsEmpty())
				continue;

			Widget widget = root.FindAnyWidget(widgetName);
			if (!widget && root.GetName() == widgetName)
				widget = root;
			if (widget)
			{
				found++;
				LogWidgetGeometry(owner, widgetName, widget);
				continue;
			}

			if (!missing.IsEmpty())
				missing = missing + ",";
			missing = missing + widgetName;
		}

		if (missing.IsEmpty())
		{
			Print(string.Format("h-istasi ui layout debug | %1 | widget check ok | root=%2 found=%3/%4", owner, WidgetSummary(root), found, widgetNames.Count()));
			return;
		}

		Print(string.Format("h-istasi ui layout debug | %1 | widget check missing | root=%2 found=%3/%4 missing=%5", owner, WidgetSummary(root), found, widgetNames.Count(), missing), LogLevel.WARNING);
	}

	static void LogExpectedWidgetsCsv(string owner, Widget root, string widgetNames)
	{
		if (!LAYOUT_WIDGET_DEBUG_ENABLED)
			return;
		if (widgetNames.IsEmpty())
			return;

		array<string> names = {};
		widgetNames.Split("|", names, true);
		LogExpectedWidgets(owner, root, names);
	}

	static void LogWidgetBound(string owner, Widget root, string widgetName, int userId)
	{
		if (!LAYOUT_WIDGET_DEBUG_ENABLED)
			return;
		if (!root || widgetName.IsEmpty())
			return;

		Widget widget = root.FindAnyWidget(widgetName);
		if (!widget)
		{
			Print(string.Format("h-istasi ui layout debug | %1 | bind missing | widget=%2 userId=%3 root=%4", owner, widgetName, userId, WidgetSummary(root)), LogLevel.WARNING);
			return;
		}

		Print(string.Format("h-istasi ui layout debug | %1 | bind ok | widget=%2 userId=%3 target=%4", owner, widgetName, userId, WidgetSummary(widget)));
	}

	static void LogWidgetGeometryCsv(string owner, Widget root, string widgetNames)
	{
		if (!LAYOUT_GEOMETRY_DEBUG_ENABLED)
			return;
		if (!root || widgetNames.IsEmpty())
			return;

		array<string> names = {};
		widgetNames.Split("|", names, true);
		foreach (string widgetName : names)
		{
			if (widgetName.IsEmpty())
				continue;

			Widget widget = root.FindAnyWidget(widgetName);
			if (!widget && root.GetName() == widgetName)
				widget = root;
			if (!widget)
				continue;

			LogWidgetGeometry(owner, widgetName, widget);
		}
	}

	static void LogReadyWidgetsCsv(string owner, Widget root, string widgetNames)
	{
		if (!LAYOUT_GEOMETRY_DEBUG_ENABLED)
			return;
		if (widgetNames.IsEmpty())
			return;

		array<string> names = {};
		widgetNames.Split("|", names, true);
		LogReadyWidgets(owner, root, names);
	}

	static void LogReadyWidgets(string owner, Widget root, array<string> widgetNames)
	{
		if (!LAYOUT_GEOMETRY_DEBUG_ENABLED)
			return;
		if (!widgetNames)
			return;

		if (!root)
		{
			Print(string.Format("h-istasi ui layout debug | %1 | ready incomplete | root=null expected=%2", owner, widgetNames.Count()), LogLevel.WARNING);
			return;
		}

		int expected;
		int ready;
		string missing = "";
		string hidden = "";
		string zero = "";

		foreach (string widgetName : widgetNames)
		{
			if (widgetName.IsEmpty())
				continue;

			expected++;
			Widget widget = root.FindAnyWidget(widgetName);
			if (!widget && root.GetName() == widgetName)
				widget = root;

			if (!widget)
			{
				missing = AppendCsvValue(missing, widgetName);
				continue;
			}

			bool visible = widget.IsVisibleInHierarchy();
			bool hasBounds = WidgetHasUsableBounds(widget);
			if (visible && hasBounds)
			{
				ready++;
				continue;
			}

			if (!visible)
				hidden = AppendCsvValue(hidden, FormatWidgetProblem(widgetName, widget));
			if (!hasBounds)
				zero = AppendCsvValue(zero, FormatWidgetProblem(widgetName, widget));
		}

		if (!missing.IsEmpty() || !hidden.IsEmpty() || !zero.IsEmpty())
		{
			Print(string.Format("h-istasi ui layout debug | %1 | ready incomplete | root=%2 ready=%3/%4 missing=%5 hidden=%6 zero=%7", owner, WidgetSummary(root), ready, expected, EmptyListAsNone(missing), EmptyListAsNone(hidden), EmptyListAsNone(zero)), LogLevel.WARNING);
			return;
		}

		Print(string.Format("h-istasi ui layout debug | %1 | ready ok | root=%2 ready=%3/%4", owner, WidgetSummary(root), ready, expected));
	}

	static void LogWidgetGeometry(string owner, string widgetName, Widget widget)
	{
		if (!LAYOUT_GEOMETRY_DEBUG_ENABLED)
			return;
		if (!widget)
			return;

		Print(string.Format("h-istasi ui layout debug | %1 | widget geometry | widget=%2 target=%3", owner, widgetName, WidgetSummary(widget)));
	}

	static void LogPopulation(string owner, string summary)
	{
		if (!LAYOUT_POPULATION_DEBUG_ENABLED)
			return;

		Print(string.Format("h-istasi ui layout debug | %1 | populate | %2", owner, summary));
	}

	static void LogRowSample(string owner, ResourceName layout, int index, string summary)
	{
		if (!LAYOUT_ROW_SAMPLE_DEBUG_ENABLED)
			return;
		if (index >= LAYOUT_ROW_SAMPLE_LIMIT)
			return;

		Print(string.Format("h-istasi ui layout debug | %1 | row sample #%2 | layout=%3 | %4", owner, index + 1, layout, summary));
	}

	static void LogRowSummary(string owner, ResourceName layout, int count, string summary)
	{
		if (!LAYOUT_POPULATION_DEBUG_ENABLED)
			return;

		Print(string.Format("h-istasi ui layout debug | %1 | row summary | layout=%2 count=%3 | %4", owner, layout, count, summary));
	}

	static string WidgetSummary(Widget widget)
	{
		if (!widget)
			return "null";

		float x;
		float y;
		float w;
		float h;
		widget.GetScreenPos(x, y);
		widget.GetScreenSize(w, h);
		string bounds = string.Format("screen=%1,%2 size=%3x%4", Math.Round(x), Math.Round(y), Math.Round(w), Math.Round(h));

		return string.Format("%1 type=%2 visible=%3 hierarchy=%4 z=%5 opacity=%6 flags=%7 %8", widget.GetName(), widget.GetTypeName(), widget.IsVisible(), widget.IsVisibleInHierarchy(), widget.GetZOrder(), widget.GetOpacity(), widget.GetFlags(), bounds);
	}

	protected static bool WidgetHasUsableBounds(Widget widget)
	{
		if (!widget)
			return false;

		float w;
		float h;
		widget.GetScreenSize(w, h);
		return w >= 1.0 && h >= 1.0;
	}

	protected static string FormatWidgetProblem(string widgetName, Widget widget)
	{
		if (!widget)
			return widgetName + "[null]";

		float x;
		float y;
		float w;
		float h;
		widget.GetScreenPos(x, y);
		widget.GetScreenSize(w, h);
		return string.Format("%1[vis=%2;hier=%3;z=%4;pos=%5:%6;size=%7x%8]", widgetName, widget.IsVisible(), widget.IsVisibleInHierarchy(), widget.GetZOrder(), Math.Round(x), Math.Round(y), Math.Round(w), Math.Round(h));
	}

	protected static string AppendCsvValue(string values, string value)
	{
		if (value.IsEmpty())
			return values;

		if (values.IsEmpty())
			return value;

		return values + "," + value;
	}

	protected static string EmptyListAsNone(string values)
	{
		if (values.IsEmpty())
			return "none";

		return values;
	}
}
