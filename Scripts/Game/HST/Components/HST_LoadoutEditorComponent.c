class HST_LoadoutEditorWidgetHandler : ScriptedWidgetEventHandler
{
	protected HST_LoadoutEditorComponent m_Editor;

	void Bind(HST_LoadoutEditorComponent editor)
	{
		m_Editor = editor;
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (!m_Editor)
			return false;

		return m_Editor.OnWidgetClicked(w.GetUserID());
	}

	override bool OnMouseButtonUp(Widget w, int x, int y, int button)
	{
		if (!m_Editor)
			return false;

		return m_Editor.OnWidgetClicked(w.GetUserID());
	}
}

class HST_LoadoutEditorDrawCommandSet
{
	ref array<ref CanvasWidgetCommand> m_aCommands = {};
}

[ComponentEditorProps(category: "h-istasi", description: "Client-side fullscreen h-istasi loadout editor")]
class HST_LoadoutEditorComponentClass : ScriptComponentClass
{
}

class HST_LoadoutEditorComponent : ScriptComponent
{
	static const string EDITOR_INPUT_CONTEXT = "InGameMenuContext";
	static const string EDITOR_CURSOR_CONTEXT = "InventoryContext";
	static const int CLOSE_WIDGET_ID = 9200;
	static const int CANCEL_WIDGET_ID = 9201;
	static const int APPLY_WIDGET_ID = 9202;
	static const int SAVE_WIDGET_ID = 9203;

	protected static HST_LoadoutEditorComponent s_LocalInstance;

	protected IEntity m_OwnerEntity;
	protected bool m_bIsLocalOwner;
	protected bool m_bEditorOpen;
	protected string m_sStatusText = "Choose a saved template or build a draft from recovered arsenal gear.";
	protected string m_sLastResult;
	protected ref array<Widget> m_aWidgets = {};
	protected ref array<ref HST_LoadoutEditorDrawCommandSet> m_aCanvasCommandSets = {};
	protected ref HST_LoadoutEditorWidgetHandler m_WidgetHandler;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_OwnerEntity = owner;
		m_bIsLocalOwner = IsLocalOwner(owner);
		SetEventMask(owner, EntityEvent.INIT | EntityEvent.FRAME);

		if (m_bIsLocalOwner)
			s_LocalInstance = this;
	}

	override void OnDelete(IEntity owner)
	{
		if (s_LocalInstance == this)
			s_LocalInstance = null;

		CloseEditor(false);
		super.OnDelete(owner);
	}

	override void EOnInit(IEntity owner)
	{
		RefreshLocalOwner(owner);
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!m_bEditorOpen)
			return;

		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;

		inputManager.ActivateContext(EDITOR_INPUT_CONTEXT);
		inputManager.ActivateContext(EDITOR_CURSOR_CONTEXT);
	}

	static HST_LoadoutEditorComponent GetLocalInstance()
	{
		return s_LocalInstance;
	}

	void OpenFromArsenal(IEntity userEntity)
	{
		if (!m_bIsLocalOwner)
			return;

		HST_CommandMenuComponent commandMenu = HST_CommandMenuComponent.GetLocalInstance();
		if (commandMenu)
			commandMenu.CloseMenuFromExternal();

		m_bEditorOpen = true;
		m_sStatusText = "Opening h-istasi arsenal editor. Counts, INF unlocks, and apply validation stay server-authoritative.";
		m_sLastResult = "requested editor session";
		RequestServerAction("loadout_editor_open", "");
		RenderEditor();
		ShowHint("Loadout editor opened");
	}

	bool OnWidgetClicked(int widgetId)
	{
		if (!m_bEditorOpen)
			return false;

		if (widgetId == CLOSE_WIDGET_ID || widgetId == CANCEL_WIDGET_ID)
		{
			CloseEditor(true);
			return true;
		}

		if (widgetId == SAVE_WIDGET_ID)
		{
			m_sLastResult = "requested save draft";
			RequestServerAction("loadout_save", "Arsenal Template");
			RenderEditor();
			ShowHint("Saved current draft request sent");
			return true;
		}

		if (widgetId == APPLY_WIDGET_ID)
		{
			m_sLastResult = "requested apply saved loadout";
			RequestServerAction("loadout_apply", "");
			RenderEditor();
			ShowHint("Apply request sent");
			return true;
		}

		return false;
	}

	void OnServerActionResult(string payload, string lastResult)
	{
		if (!m_bEditorOpen)
			return;

		if (!lastResult.IsEmpty())
			m_sLastResult = lastResult;

		RenderEditor();
	}

	void CloseEditor(bool requestServer)
	{
		if (!m_bEditorOpen)
			return;

		if (requestServer)
			RequestServerAction("loadout_editor_close", "");

		m_bEditorOpen = false;
		ClearWidgets();
		ShowHint("Loadout editor closed");
	}

	protected void RequestServerAction(string commandId, string argument)
	{
		HST_CommandMenuRequestComponent request = HST_CommandMenuRequestComponent.GetLocalOwner();
		if (request)
		{
			request.RequestAction("arsenal", commandId, argument);
			return;
		}

		HST_CampaignCoordinatorComponent coordinator = HST_CampaignCoordinatorComponent.GetInstance();
		if (coordinator && Replication.IsServer())
		{
			m_sLastResult = coordinator.RequestVisibleMenuCommand(ResolveLocalPlayerId(), "arsenal", commandId, argument);
			return;
		}

		m_sLastResult = "player request bridge not ready";
	}

	protected void RenderEditor()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		ClearWidgets();
		if (!m_WidgetHandler)
		{
			m_WidgetHandler = new HST_LoadoutEditorWidgetHandler();
			m_WidgetHandler.Bind(this);
		}

		Widget root = workspace.CreateWidgetInWorkspace(WidgetType.FrameWidgetTypeID, 0, 0, 1280, 850, WidgetFlags.VISIBLE, null, 3600);
		if (!root)
			return;

		root.SetZOrder(3600);
		m_aWidgets.Insert(root);
		CreateRectWidget(workspace, root, 0, 0, 1280, 850, 0xFA070A0D, 1.0, 0);
		CreateRectWidget(workspace, root, 0, 0, 1280, 74, 0xFF111920, 1.0, 0);
		CreateTextWidget(workspace, root, "h-istasi Loadout Editor", 32, 18, 440, 38, 30, 0xFFF2D18B, 0, true);
		CreateTextWidget(workspace, root, "Custom FIA arsenal. h-istasi issues gear.", 474, 28, 420, 28, 16, 0xFFB7C7D7, 0, false);
		CreateButton(workspace, root, "Close", 1160, 16, 88, 42, CLOSE_WIDGET_ID);

		RenderCategories(workspace, root);
		RenderPreviewStage(workspace, root);
		RenderLoadoutPanel(workspace, root);
		RenderFooter(workspace, root);
	}

	protected void RenderCategories(WorkspaceWidget workspace, Widget root)
	{
		CreateRectWidget(workspace, root, 24, 96, 230, 650, 0xFF111920, 0.98, 0);
		CreateTextWidget(workspace, root, "Categories", 46, 118, 180, 28, 21, 0xFFEFE2C4, 0, true);

		array<string> categories = {};
		categories.Insert("Clothing");
		categories.Insert("Headgear");
		categories.Insert("Vest");
		categories.Insert("Backpack");
		categories.Insert("Weapons");
		categories.Insert("Magazines");
		categories.Insert("Explosives");
		categories.Insert("Attachments");
		categories.Insert("Medical");
		categories.Insert("Utility");
		for (int i = 0; i < categories.Count(); i++)
		{
			int top = 164 + i * 50;
			int color = 0xFF1B252E;
			if (i == 4)
				color = 0xFF604A24;

			CreateRectWidget(workspace, root, 44, top, 188, 34, color, 0.96, 0);
			CreateTextWidget(workspace, root, categories[i], 58, top + 7, 160, 22, 16, 0xFFE2E6E8, 0, i == 4);
		}
	}

	protected void RenderPreviewStage(WorkspaceWidget workspace, Widget root)
	{
		CreateRectWidget(workspace, root, 280, 96, 520, 650, 0xFF0E1216, 0.99, 0);
		CreateRectWidget(workspace, root, 300, 118, 480, 520, 0xFF18232B, 1.0, 0);
		CreateRectWidget(workspace, root, 340, 170, 400, 390, 0xFF222C34, 1.0, 0);
		CreateTextWidget(workspace, root, "Preview Stage", 322, 132, 210, 28, 21, 0xFFEFE2C4, 0, true);
		CreateTextWidget(workspace, root, "Draft gear is preview-only until Apply succeeds.", 322, 602, 420, 24, 15, 0xFFB7C7D7, 0, false);

		CreateRectWidget(workspace, root, 520, 228, 42, 210, 0xFF5E6B72, 1.0, 0);
		CreateRectWidget(workspace, root, 496, 202, 90, 64, 0xFF9AA3AA, 1.0, 0);
		CreateRectWidget(workspace, root, 468, 456, 44, 122, 0xFF48545C, 1.0, 0);
		CreateRectWidget(workspace, root, 570, 456, 44, 122, 0xFF48545C, 1.0, 0);
		CreateRectWidget(workspace, root, 454, 300, 64, 26, 0xFF7B8678, 1.0, 0);
		CreateRectWidget(workspace, root, 562, 300, 64, 26, 0xFF7B8678, 1.0, 0);
		CreateTextWidget(workspace, root, "MANNEQUIN", 476, 366, 130, 30, 15, 0xFFFFD166, 0, true);
	}

	protected void RenderLoadoutPanel(WorkspaceWidget workspace, Widget root)
	{
		CreateRectWidget(workspace, root, 826, 96, 430, 650, 0xFF111920, 0.98, 0);
		CreateTextWidget(workspace, root, "Current Loadout", 850, 118, 220, 28, 21, 0xFFEFE2C4, 0, true);
		CreateTextWidget(workspace, root, "Primary weapon", 852, 166, 150, 24, 15, 0xFFCAD2D7, 0, false);
		CreateTextWidget(workspace, root, "Select a weapon from recovered items", 1010, 166, 224, 24, 15, 0xFFFFD166, 0, false);
		CreateTextWidget(workspace, root, "Uniform", 852, 204, 150, 24, 15, 0xFFCAD2D7, 0, false);
		CreateTextWidget(workspace, root, "Draft clothing slot", 1010, 204, 224, 24, 15, 0xFFE2E6E8, 0, false);
		CreateTextWidget(workspace, root, "Attachments", 852, 242, 150, 24, 15, 0xFFCAD2D7, 0, false);
		CreateTextWidget(workspace, root, "Compatible mounts only on apply", 1010, 242, 224, 24, 15, 0xFFE2E6E8, 0, false);

		CreateRectWidget(workspace, root, 850, 310, 380, 4, 0xFFC4953B, 1.0, 0);
		CreateTextWidget(workspace, root, "Saved Templates", 850, 332, 220, 28, 21, 0xFFEFE2C4, 0, true);
		CreateTextWidget(workspace, root, "Personal templates are stored under $profile:h-istasi/loadouts and validated against this campaign arsenal.", 850, 374, 370, 70, 15, 0xFFB7C7D7, 0, false);

		CreateRectWidget(workspace, root, 850, 482, 380, 4, 0xFF50704A, 1.0, 0);
		CreateTextWidget(workspace, root, "Status", 850, 504, 120, 28, 21, 0xFFEFE2C4, 0, true);
		CreateTextWidget(workspace, root, ShortenText(m_sStatusText, 92), 850, 544, 370, 58, 15, 0xFFB7E48F, 0, false);
		CreateTextWidget(workspace, root, ShortenText(m_sLastResult, 86), 850, 618, 370, 44, 15, 0xFFFFD166, 0, false);
	}

	protected void RenderFooter(WorkspaceWidget workspace, Widget root)
	{
		CreateRectWidget(workspace, root, 0, 768, 1280, 82, 0xFF111920, 1.0, 0);
		CreateButton(workspace, root, "Save", 802, 790, 106, 42, SAVE_WIDGET_ID);
		CreateButton(workspace, root, "Apply", 928, 790, 106, 42, APPLY_WIDGET_ID);
		CreateButton(workspace, root, "Cancel", 1054, 790, 106, 42, CANCEL_WIDGET_ID);
		CreateTextWidget(workspace, root, "Apply commits finite counts only after server validation and player inventory application.", 32, 794, 650, 32, 16, 0xFFB7C7D7, 0, false);
	}

	protected void CreateButton(WorkspaceWidget workspace, Widget root, string label, int left, int top, int width, int height, int userId)
	{
		CreateRectWidget(workspace, root, left, top, width, height, 0xFF5F6C76, 0.96, userId);
		CreateTextWidget(workspace, root, label, left + 16, top + 10, width - 32, height - 16, 17, 0xFFF4EBD6, userId, true);
	}

	protected Widget CreateRectWidget(WorkspaceWidget workspace, Widget parent, int left, int top, int width, int height, int color, float opacity, int userId)
	{
		Widget widget = workspace.CreateWidget(WidgetType.CanvasWidgetTypeID, WidgetFlags.VISIBLE, null, 3650, parent);
		if (!widget)
			return null;

		FrameSlot.SetPos(widget, left, top);
		FrameSlot.SetSize(widget, width, height);
		SetupCanvasRect(widget, width, height, color);
		widget.SetOpacity(opacity);
		if (userId > 0)
		{
			widget.SetUserID(userId);
			widget.AddHandler(m_WidgetHandler);
		}

		return widget;
	}

	protected bool SetupCanvasRect(Widget widget, int width, int height, int color)
	{
		CanvasWidget canvas = CanvasWidget.Cast(widget);
		if (!canvas)
			return false;

		HST_LoadoutEditorDrawCommandSet commandSet = new HST_LoadoutEditorDrawCommandSet();
		PolygonDrawCommand rectCommand = new PolygonDrawCommand();
		rectCommand.m_iColor = color;
		rectCommand.m_Vertices = BuildRectVertices(width, height);
		commandSet.m_aCommands.Insert(rectCommand);
		canvas.SetDrawCommands(commandSet.m_aCommands);
		m_aCanvasCommandSets.Insert(commandSet);
		return true;
	}

	protected ref array<float> BuildRectVertices(int width, int height)
	{
		ref array<float> vertices = {};
		float rectWidth = width;
		float rectHeight = height;
		vertices.Insert(0.0);
		vertices.Insert(0.0);
		vertices.Insert(rectWidth);
		vertices.Insert(0.0);
		vertices.Insert(rectWidth);
		vertices.Insert(rectHeight);
		vertices.Insert(0.0);
		vertices.Insert(rectHeight);
		return vertices;
	}

	protected TextWidget CreateTextWidget(WorkspaceWidget workspace, Widget parent, string text, int left, int top, int width, int height, int fontSize, int color, int userId, bool bold)
	{
		Widget widget = workspace.CreateWidget(WidgetType.TextWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.NO_LOCALIZATION, null, 3700, parent);
		if (!widget)
			return null;

		FrameSlot.SetPos(widget, left, top);
		FrameSlot.SetSize(widget, width, height);
		TextWidget textWidget = TextWidget.Cast(widget);
		if (textWidget)
		{
			textWidget.SetText(text);
			textWidget.SetTextWrapping(false);
			textWidget.SetExactFontSize(fontSize);
			textWidget.SetLineSpacing(1.1);
			textWidget.SetBold(bold);
			textWidget.SetOutline(1, 0xDD000000);
			textWidget.SetShadow(2, 0xEE000000, 1, 1, 1);
		}

		widget.SetColorInt(color);
		if (userId > 0)
		{
			widget.SetUserID(userId);
			widget.AddHandler(m_WidgetHandler);
		}

		return textWidget;
	}

	protected void ClearWidgets()
	{
		foreach (Widget widget : m_aWidgets)
		{
			if (widget)
				widget.RemoveFromHierarchy();
		}

		m_aWidgets.Clear();
		m_aCanvasCommandSets.Clear();
	}

	protected void ShowHint(string text)
	{
		SCR_HintManagerComponent hintManager = SCR_HintManagerComponent.GetInstance();
		if (hintManager)
			hintManager.ShowCustomHint(text, "h-istasi", 2.0);
	}

	protected string ShortenText(string text, int maxCharacters)
	{
		if (text.IsEmpty() || maxCharacters <= 0)
			return "";

		if (text.Length() <= maxCharacters)
			return text;

		if (maxCharacters <= 3)
			return text.Substring(0, maxCharacters);

		return text.Substring(0, maxCharacters - 3) + "...";
	}

	protected int ResolveLocalPlayerId()
	{
		HST_CommandMenuRequestComponent request = HST_CommandMenuRequestComponent.GetLocalOwner();
		if (request && request.ResolveLocalPlayerId() > 0)
			return request.ResolveLocalPlayerId();

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return 1;

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		if (playerIds.Count() == 0)
			return 1;

		return playerIds[0];
	}

	protected void RefreshLocalOwner(IEntity owner)
	{
		if (m_bIsLocalOwner)
			return;

		if (!IsLocalOwner(owner))
			return;

		m_bIsLocalOwner = true;
		s_LocalInstance = this;
	}

	protected bool IsLocalOwner(IEntity owner)
	{
		if (!owner)
			return false;

		BaseRplComponent rpl = BaseRplComponent.Cast(owner.FindComponent(BaseRplComponent));
		return !rpl || rpl.IsOwner();
	}
}
