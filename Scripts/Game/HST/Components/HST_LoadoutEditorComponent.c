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
	static const ResourceName EDITOR_LAYOUT = "{5AF2D86E07D44A51}UI/layouts/HST_LoadoutEditor.layout";
	static const ResourceName DEFAULT_PREVIEW_PREFAB = "{84B40583F4D1B7A3}Prefabs/Characters/Factions/INDFOR/FIA/Character_FIA_Rifleman.et";
	static const ResourceName PREVIEW_WORLD_RESOURCE = "{4391FE7994EE6FE2}worlds/Sandbox/InventoryPreviewWorld10.et";
	static const string EDITOR_INPUT_CONTEXT = "InGameMenuContext";
	static const string EDITOR_CURSOR_CONTEXT = "InventoryContext";
	static const int CLOSE_WIDGET_ID = 9200;
	static const int CANCEL_WIDGET_ID = 9201;
	static const int APPLY_WIDGET_ID = 9202;
	static const int SAVE_WIDGET_ID = 9203;
	static const int DELETE_TEMPLATE_WIDGET_ID = 9204;
	static const int ITEM_PAGE_PREV_WIDGET_ID = 9205;
	static const int ITEM_PAGE_NEXT_WIDGET_ID = 9206;
	static const int SLOT_PAGE_PREV_WIDGET_ID = 9207;
	static const int SLOT_PAGE_NEXT_WIDGET_ID = 9208;
	static const int TEMPLATE_PAGE_PREV_WIDGET_ID = 9209;
	static const int TEMPLATE_PAGE_NEXT_WIDGET_ID = 9210;
	static const int CLEAR_DRAFT_WIDGET_ID = 9211;
	static const int CAMERA_WIDGET_ID = 9212;
	static const int SPOTLIGHT_WIDGET_ID = 9213;
	static const int BACK_WIDGET_ID = 9214;
	static const int SWAP_WIDGET_ID = 9215;
	static const int MODE_WIDGET_ID_BASE = 9250;
	static const int CATEGORY_WIDGET_ID_BASE = 9300;
	static const int ITEM_WIDGET_ID_BASE = 9400;
	static const int SLOT_REMOVE_WIDGET_ID_BASE = 9500;
	static const int SLOT_MINUS_WIDGET_ID_BASE = 9600;
	static const int SLOT_PLUS_WIDGET_ID_BASE = 9700;
	static const int TEMPLATE_WIDGET_ID_BASE = 9800;
	static const int SLOT_SELECT_WIDGET_ID_BASE = 9900;
	static const int ITEMS_PER_PAGE = 6;
	static const int SLOTS_PER_PAGE = 5;
	static const int TEMPLATES_PER_PAGE = 6;
	static const int PREVIEW_CAMERA = 0;

	protected static HST_LoadoutEditorComponent s_LocalInstance;

	protected IEntity m_OwnerEntity;
	protected bool m_bIsLocalOwner;
	protected bool m_bEditorOpen;
	protected string m_sEditorMode = "clothing";
	protected string m_sSelectedCategory = "clothing";
	protected string m_sSelectedSlotId;
	protected string m_sSelectedTemplateId;
	protected string m_sPreviewPrefab = DEFAULT_PREVIEW_PREFAB;
	protected string m_sStatusText = "Choose a saved template or build a draft from recovered arsenal gear.";
	protected string m_sPreviewStatus;
	protected string m_sPreviewPosition;
	protected string m_sLastResult;
	protected bool m_bPreviewSpawned;
	protected bool m_bSpotlightEnabled = true;
	protected int m_iPreviewItemCount;
	protected int m_iDraftSlotCount;
	protected int m_iDraftItemCount;
	protected int m_iDraftFiniteCount;
	protected int m_iDraftInfiniteCount;
	protected int m_iItemPage;
	protected int m_iSlotPage;
	protected int m_iTemplatePage;
	protected int m_iCameraMode;
	protected ref array<string> m_aCategoryIds = {};
	protected ref array<string> m_aCategoryLabels = {};
	protected ref array<int> m_aCategoryCounts = {};
	protected ref array<string> m_aItemCategories = {};
	protected ref array<string> m_aItemPrefabs = {};
	protected ref array<string> m_aItemDisplays = {};
	protected ref array<string> m_aItemShortDisplays = {};
	protected ref array<string> m_aItemSlotLabels = {};
	protected ref array<string> m_aItemCounts = {};
	protected ref array<bool> m_aItemInfinite = {};
	protected ref array<bool> m_aItemPreviewEligible = {};
	protected ref array<int> m_aVisibleItemIndexes = {};
	protected ref array<string> m_aSlotIds = {};
	protected ref array<string> m_aSlotCategories = {};
	protected ref array<string> m_aSlotPrefabs = {};
	protected ref array<string> m_aSlotDisplays = {};
	protected ref array<string> m_aSlotShortDisplays = {};
	protected ref array<string> m_aSlotLabels = {};
	protected ref array<int> m_aSlotQuantities = {};
	protected ref array<bool> m_aSlotPreviewEligible = {};
	protected ref array<string> m_aTemplateIds = {};
	protected ref array<string> m_aTemplateDisplays = {};
	protected ref array<int> m_aTemplateSlotCounts = {};
	protected ref array<Widget> m_aWidgets = {};
	protected ref array<ref HST_LoadoutEditorDrawCommandSet> m_aCanvasCommandSets = {};
	protected ref HST_LoadoutEditorWidgetHandler m_WidgetHandler;
	protected RenderTargetWidget m_PreviewWidget;
	protected ref SharedItemRef m_PreviewWorldRef;
	protected BaseWorld m_PreviewWorld;
	protected IEntity m_PreviewCharacter;
	protected string m_sPreviewRenderKey;

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
		m_sEditorMode = "clothing";
		m_sSelectedCategory = "clothing";
		m_sSelectedSlotId = "";
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

		if (widgetId == BACK_WIDGET_ID)
		{
			m_sEditorMode = "clothing";
			m_sSelectedCategory = ResolveDefaultCategoryForMode(m_sEditorMode);
			m_sSelectedSlotId = "";
			m_iItemPage = 0;
			RenderEditor();
			return true;
		}

		if (widgetId == CAMERA_WIDGET_ID)
		{
			m_iCameraMode++;
			if (m_iCameraMode > 2)
				m_iCameraMode = 0;

			UpdatePreviewCamera(true);
			RenderEditor();
			return true;
		}

		if (widgetId == SPOTLIGHT_WIDGET_ID)
		{
			m_bSpotlightEnabled = !m_bSpotlightEnabled;
			RenderEditor();
			return true;
		}

		if (widgetId == SWAP_WIDGET_ID)
		{
			ShowHint(BuildSwapHint());
			m_sLastResult = BuildSwapHint();
			RenderEditor();
			return true;
		}

		if (widgetId == SAVE_WIDGET_ID)
		{
			m_sLastResult = "requested save draft";
			RequestServerAction("loadout_save", "");
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

		if (widgetId == DELETE_TEMPLATE_WIDGET_ID)
		{
			if (m_sSelectedTemplateId.IsEmpty())
				return true;

			m_sLastResult = "requested delete template";
			RequestServerAction("loadout_delete", m_sSelectedTemplateId);
			RenderEditor();
			ShowHint("Delete template request sent");
			return true;
		}

		if (widgetId == CLEAR_DRAFT_WIDGET_ID)
		{
			m_sLastResult = "requested clear draft";
			RequestServerAction("loadout_clear_draft", "");
			RenderEditor();
			return true;
		}

		if (widgetId == ITEM_PAGE_PREV_WIDGET_ID)
		{
			m_iItemPage = Math.Max(0, m_iItemPage - 1);
			RenderEditor();
			return true;
		}

		if (widgetId == ITEM_PAGE_NEXT_WIDGET_ID)
		{
			m_iItemPage++;
			RenderEditor();
			return true;
		}

		if (widgetId == SLOT_PAGE_PREV_WIDGET_ID)
		{
			m_iSlotPage = Math.Max(0, m_iSlotPage - 1);
			RenderEditor();
			return true;
		}

		if (widgetId == SLOT_PAGE_NEXT_WIDGET_ID)
		{
			m_iSlotPage++;
			RenderEditor();
			return true;
		}

		if (widgetId == TEMPLATE_PAGE_PREV_WIDGET_ID)
		{
			m_iTemplatePage = Math.Max(0, m_iTemplatePage - 1);
			RenderEditor();
			return true;
		}

		if (widgetId == TEMPLATE_PAGE_NEXT_WIDGET_ID)
		{
			m_iTemplatePage++;
			RenderEditor();
			return true;
		}

		int modeIndex = widgetId - MODE_WIDGET_ID_BASE;
		if (modeIndex >= 0 && modeIndex < GetEditorModeCount())
		{
			m_sEditorMode = GetEditorModeId(modeIndex);
			m_sSelectedCategory = ResolveDefaultCategoryForMode(m_sEditorMode);
			m_sSelectedSlotId = "";
			m_iItemPage = 0;
			m_iSlotPage = 0;
			m_iTemplatePage = 0;
			UpdatePreviewCamera(true);
			RenderEditor();
			return true;
		}

		int categoryIndex = widgetId - CATEGORY_WIDGET_ID_BASE;
		if (categoryIndex >= 0 && categoryIndex < m_aCategoryIds.Count())
		{
			m_sSelectedCategory = m_aCategoryIds[categoryIndex];
			m_sEditorMode = ResolveModeForCategory(m_sSelectedCategory);
			int selectedSlotIndex = FindSelectedSlotIndex();
			if (selectedSlotIndex < 0 || m_aSlotCategories[selectedSlotIndex] != m_sSelectedCategory)
				m_sSelectedSlotId = "";
			m_iItemPage = 0;
			UpdatePreviewCamera(true);
			RenderEditor();
			return true;
		}

		int itemVisibleIndex = widgetId - ITEM_WIDGET_ID_BASE;
		if (itemVisibleIndex >= 0 && itemVisibleIndex < m_aVisibleItemIndexes.Count())
		{
			int itemIndex = m_aVisibleItemIndexes[itemVisibleIndex];
			if (itemIndex >= 0 && itemIndex < m_aItemPrefabs.Count())
			{
				if (!m_sSelectedSlotId.IsEmpty())
				{
					m_sLastResult = "requested swap item";
					RequestServerAction("loadout_replace_slot", m_sSelectedSlotId + ":" + m_aItemPrefabs[itemIndex]);
				}
				else
				{
					m_sLastResult = "requested add item";
					RequestServerAction("loadout_add_item", m_aItemPrefabs[itemIndex]);
				}

				RenderEditor();
				return true;
			}
		}

		int slotSelectIndex = widgetId - SLOT_SELECT_WIDGET_ID_BASE;
		if (slotSelectIndex >= 0 && slotSelectIndex < m_aSlotIds.Count())
		{
			m_sSelectedSlotId = m_aSlotIds[slotSelectIndex];
			m_sSelectedCategory = m_aSlotCategories[slotSelectIndex];
			m_sEditorMode = ResolveModeForCategory(m_sSelectedCategory);
			m_iItemPage = 0;
			UpdatePreviewCamera(true);
			RenderEditor();
			return true;
		}

		int slotRemoveIndex = widgetId - SLOT_REMOVE_WIDGET_ID_BASE;
		if (slotRemoveIndex >= 0 && slotRemoveIndex < m_aSlotIds.Count())
		{
			m_sLastResult = "requested remove slot";
			RequestServerAction("loadout_remove_slot", m_aSlotIds[slotRemoveIndex]);
			RenderEditor();
			return true;
		}

		int slotMinusIndex = widgetId - SLOT_MINUS_WIDGET_ID_BASE;
		if (slotMinusIndex >= 0 && slotMinusIndex < m_aSlotIds.Count())
		{
			int quantity = Math.Max(1, m_aSlotQuantities[slotMinusIndex] - 1);
			m_sLastResult = "requested quantity change";
			RequestServerAction("loadout_set_quantity", m_aSlotIds[slotMinusIndex] + ":" + quantity);
			RenderEditor();
			return true;
		}

		int slotPlusIndex = widgetId - SLOT_PLUS_WIDGET_ID_BASE;
		if (slotPlusIndex >= 0 && slotPlusIndex < m_aSlotIds.Count())
		{
			int quantity = Math.Max(1, m_aSlotQuantities[slotPlusIndex] + 1);
			m_sLastResult = "requested quantity change";
			RequestServerAction("loadout_set_quantity", m_aSlotIds[slotPlusIndex] + ":" + quantity);
			RenderEditor();
			return true;
		}

		int templateIndex = widgetId - TEMPLATE_WIDGET_ID_BASE;
		if (templateIndex >= 0 && templateIndex < m_aTemplateIds.Count())
		{
			m_sSelectedTemplateId = m_aTemplateIds[templateIndex];
			m_sLastResult = "requested select template";
			RequestServerAction("loadout_select", m_sSelectedTemplateId);
			RenderEditor();
			return true;
		}

		return false;
	}

	void OnServerActionResult(string payload, string lastResult)
	{
		if (!m_bEditorOpen)
			return;

		if (!payload.Contains("HST_LOADOUT_EDITOR"))
			return;

		if (!lastResult.IsEmpty())
			m_sLastResult = lastResult;

		ParseEditorPayload(payload);
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
		DeletePreviewWorld();
		ShowHint("Loadout editor closed");
	}

	protected void RequestServerAction(string commandId, string argument)
	{
		HST_CommandMenuRequestComponent request = HST_CommandMenuRequestComponent.GetLocalOwner();
		if (request)
		{
			request.RequestLoadoutEditorAction(commandId, argument);
			return;
		}

		HST_CampaignCoordinatorComponent coordinator = HST_CampaignCoordinatorComponent.GetInstance();
		if (coordinator && Replication.IsServer())
		{
			int playerId = ResolveLocalPlayerId();
			m_sLastResult = coordinator.RequestLoadoutEditorCommand(playerId, commandId, argument);
			OnServerActionResult(coordinator.BuildLoadoutEditorPayload(playerId), m_sLastResult);
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

		Widget root = workspace.CreateWidgets(EDITOR_LAYOUT);
		bool layoutRoot = root != null;
		if (!root)
			root = workspace.CreateWidgetInWorkspace(WidgetType.FrameWidgetTypeID, 0, 0, 1280, 850, WidgetFlags.VISIBLE, null, 3600);

		if (!root)
			return;

		if (!layoutRoot)
		{
			FrameSlot.SetPos(root, 0, 0);
			FrameSlot.SetSize(root, 1280, 850);
		}

		root.SetZOrder(3600);
		m_aWidgets.Insert(root);

		m_PreviewWidget = RenderTargetWidget.Cast(root.FindAnyWidget("HST_LoadoutPreview"));
		ConfigurePreviewWidget();
		EnsurePreviewWorld();
		RefreshPreviewWorldLoadout();

		CreateRectWidget(workspace, root, 0, 0, 1280, 850, 0x66060A0D, 1.0, 0);
		CreateRectWidget(workspace, root, 0, 0, 1280, 48, 0xE911171B, 1.0, 0);
		CreateRectWidget(workspace, root, 0, 48, 1280, 2, 0xFFC4953B, 1.0, 0);
		CreateTextWidget(workspace, root, "h-istasi Arsenal", 58, 11, 250, 30, 23, 0xFFF2D18B, 0, true);
		CreateTextWidget(workspace, root, BuildDraftHeaderSummary(), 842, 15, 360, 22, 14, 0xFFCED6DA, 0, false);
		CreateButton(workspace, root, "ESC", 18, 334, 48, 36, CLOSE_WIDGET_ID);

		RenderModeTabs(workspace, root);
		RenderSlotRail(workspace, root);
		RenderCandidatePanel(workspace, root);
		RenderPreviewStage(workspace, root);
		RenderFooter(workspace, root);
	}

	protected void RenderModeTabs(WorkspaceWidget workspace, Widget root)
	{
		CreateRectWidget(workspace, root, 58, 58, 330, 44, 0xF3212528, 1.0, 0);
		for (int i = 0; i < GetEditorModeCount(); i++)
		{
			string modeId = GetEditorModeId(i);
			int color = 0xFF1A1F23;
			if (modeId == m_sEditorMode)
				color = 0xFFC4953B;

			int left = 62 + i * 64;
			CreateRectWidget(workspace, root, left, 62, 60, 34, color, 0.98, MODE_WIDGET_ID_BASE + i);
			CreateTextWidget(workspace, root, GetEditorModeLabel(modeId), left + 5, 70, 50, 18, 10, 0xFFF5E8CE, MODE_WIDGET_ID_BASE + i, modeId == m_sEditorMode);
		}
	}

	protected void RenderSlotRail(WorkspaceWidget workspace, Widget root)
	{
		CreateRectWidget(workspace, root, 58, 104, 330, 260, 0xF3212528, 1.0, 0);
		CreateRectWidget(workspace, root, 58, 104, 330, 2, 0xFFC4953B, 1.0, 0);
		CreateTextWidget(workspace, root, BuildModeTitle(), 76, 116, 210, 24, 18, 0xFFEFE2C4, 0, true);
		CreateTextWidget(workspace, root, BuildPreviewStatusLabel(), 236, 120, 134, 18, 11, 0xFFD7B66F, 0, false);

		int maxSlotPage = GetMaxPage(CountVisibleSlotsForCurrentMode(), SLOTS_PER_PAGE);
		m_iSlotPage = Math.Min(Math.Max(0, m_iSlotPage), maxSlotPage);
		int firstVisible = m_iSlotPage * SLOTS_PER_PAGE;
		int rendered;
		int visibleCursor;
		for (int slotIndex = 0; slotIndex < m_aSlotIds.Count(); slotIndex++)
		{
			if (!IsSlotVisibleInCurrentMode(slotIndex))
				continue;

			if (visibleCursor < firstVisible)
			{
				visibleCursor++;
				continue;
			}

			if (rendered >= SLOTS_PER_PAGE)
				break;

			int top = 146 + rendered * 38;
			int rowColor = 0xFF24282A;
			if (m_aSlotIds[slotIndex] == m_sSelectedSlotId)
				rowColor = 0xFF6B4C24;

			CreateRectWidget(workspace, root, 70, top, 250, 35, rowColor, 0.98, SLOT_SELECT_WIDGET_ID_BASE + slotIndex);
			CreateTextWidget(workspace, root, ShortenText(GetSlotLabel(slotIndex), 18), 82, top + 4, 150, 14, 10, 0xFFFFD166, SLOT_SELECT_WIDGET_ID_BASE + slotIndex, true);
			CreateTextWidget(workspace, root, ShortenText(GetSlotShortDisplay(slotIndex), 27), 82, top + 18, 196, 15, 11, 0xFFECEEEF, SLOT_SELECT_WIDGET_ID_BASE + slotIndex, false);
			CreateTextWidget(workspace, root, string.Format("x%1", m_aSlotQuantities[slotIndex]), 284, top + 5, 28, 14, 10, 0xFFFFD166, SLOT_SELECT_WIDGET_ID_BASE + slotIndex, true);
			CreateButton(workspace, root, "-", 324, top + 7, 18, 21, SLOT_MINUS_WIDGET_ID_BASE + slotIndex);
			CreateButton(workspace, root, "+", 346, top + 7, 18, 21, SLOT_PLUS_WIDGET_ID_BASE + slotIndex);
			CreateButton(workspace, root, "X", 368, top + 7, 18, 21, SLOT_REMOVE_WIDGET_ID_BASE + slotIndex);
			rendered++;
			visibleCursor++;
		}

		if (rendered == 0)
			CreateTextWidget(workspace, root, "No draft slots in this tab.", 76, 156, 240, 22, 13, 0xFFADB7BE, 0, false);

		CreateButton(workspace, root, "<", 70, 332, 32, 24, SLOT_PAGE_PREV_WIDGET_ID);
		CreateTextWidget(workspace, root, string.Format("%1 / %2", m_iSlotPage + 1, maxSlotPage + 1), 112, 336, 56, 16, 11, 0xFFE2E6E8, 0, true);
		CreateButton(workspace, root, ">", 174, 332, 32, 24, SLOT_PAGE_NEXT_WIDGET_ID);
	}

	protected void RenderPreviewStage(WorkspaceWidget workspace, Widget root)
	{
		if (!m_bSpotlightEnabled)
			CreateRectWidget(workspace, root, 398, 54, 852, 708, 0xDD020507, 0.68, 0);

		CreateRectWidget(workspace, root, 416, 58, 804, 2, 0x54C4953B, 1.0, 0);
		CreateTextWidget(workspace, root, ShortenText(BuildSelectedPreviewTitle(), 58), 416, 68, 520, 28, 18, 0xFFEFE2C4, 0, true);
		CreateTextWidget(workspace, root, ShortenText(BuildPreviewMetaLine(), 70), 416, 94, 560, 20, 13, 0xFFB7C7D7, 0, false);
		if (!m_PreviewWidget || !m_PreviewWorld)
			CreateTextWidget(workspace, root, "Preview target unavailable", 612, 348, 260, 24, 16, 0xFFFFD166, 0, true);
	}

	protected void RenderCandidatePanel(WorkspaceWidget workspace, Widget root)
	{
		CreateRectWidget(workspace, root, 58, 374, 330, 368, 0xF3212528, 1.0, 0);
		CreateRectWidget(workspace, root, 58, 374, 330, 2, 0xFFC4953B, 1.0, 0);

		if (m_sEditorMode == "templates")
		{
			RenderTemplatePanel(workspace, root);
			return;
		}

		if (m_sEditorMode == "settings")
		{
			RenderSettingsPanel(workspace, root);
			return;
		}

		RenderCategoryChips(workspace, root);
		CreateRectWidget(workspace, root, 70, 422, 306, 44, 0xFF151A1E, 0.96, SWAP_WIDGET_ID);
		CreateTextWidget(workspace, root, BuildSwapHeaderText(), 82, 428, 196, 16, 11, 0xFFFFD166, SWAP_WIDGET_ID, true);
		CreateTextWidget(workspace, root, ShortenText(BuildSwapTargetText(), 34), 82, 444, 224, 16, 11, 0xFFECEEEF, SWAP_WIDGET_ID, false);
		CreateTextWidget(workspace, root, BuildSwapActionLabel(), 314, 434, 48, 17, 11, 0xFFF5E8CE, SWAP_WIDGET_ID, true);
		CreateTextWidget(workspace, root, ResolveSelectedCategoryLabel(), 76, 474, 190, 22, 17, 0xFFEFE2C4, 0, true);
		CreateTextWidget(workspace, root, string.Format("%1", CountItemsForSelectedCategory()), 326, 478, 42, 18, 12, 0xFFFFD166, 0, true);

		m_aVisibleItemIndexes.Clear();
		int itemCount = CountItemsForSelectedCategory();
		int maxPage = GetMaxPage(itemCount, ITEMS_PER_PAGE);
		m_iItemPage = Math.Min(Math.Max(0, m_iItemPage), maxPage);
		int startIndex = m_iItemPage * ITEMS_PER_PAGE;
		int visibleIndex = 0;
		int categoryIndex = 0;
		for (int itemIndex = 0; itemIndex < m_aItemPrefabs.Count(); itemIndex++)
		{
			if (m_aItemCategories[itemIndex] != m_sSelectedCategory)
				continue;

			if (categoryIndex < startIndex)
			{
				categoryIndex++;
				continue;
			}

			if (visibleIndex >= ITEMS_PER_PAGE)
				break;

			int top = 502 + visibleIndex * 31;
			m_aVisibleItemIndexes.Insert(itemIndex);
			int itemRowColor = 0xFF282B2D;
			if (IsItemSelectedForSlot(itemIndex))
				itemRowColor = 0xFF604A24;
			int itemLabelColor = 0xFFFFD166;
			if (itemIndex < m_aItemPreviewEligible.Count() && !m_aItemPreviewEligible[itemIndex])
				itemLabelColor = 0xFFB7C7D7;

			CreateRectWidget(workspace, root, 70, top, 306, 29, itemRowColor, 0.98, ITEM_WIDGET_ID_BASE + visibleIndex);
			CreateTextWidget(workspace, root, ShortenText(GetItemSlotLabel(itemIndex), 20), 82, top + 4, 152, 13, 10, itemLabelColor, ITEM_WIDGET_ID_BASE + visibleIndex, true);
			CreateTextWidget(workspace, root, ShortenText(GetItemShortDisplay(itemIndex), 29), 82, top + 17, 204, 13, 10, 0xFFE2E6E8, ITEM_WIDGET_ID_BASE + visibleIndex, false);
			CreateTextWidget(workspace, root, BuildCountLabel(itemIndex), 322, top + 8, 44, 14, 10, 0xFFFFD166, ITEM_WIDGET_ID_BASE + visibleIndex, true);
			visibleIndex++;
			categoryIndex++;
		}

		if (visibleIndex == 0)
			CreateTextWidget(workspace, root, "No recovered items.", 76, 482, 230, 22, 13, 0xFFB7C7D7, 0, false);

		CreateButton(workspace, root, "<", 70, 706, 32, 24, ITEM_PAGE_PREV_WIDGET_ID);
		CreateTextWidget(workspace, root, string.Format("%1 / %2", m_iItemPage + 1, maxPage + 1), 112, 710, 56, 16, 11, 0xFFE2E6E8, 0, true);
		CreateButton(workspace, root, ">", 174, 706, 32, 24, ITEM_PAGE_NEXT_WIDGET_ID);
	}

	protected void RenderTemplatePanel(WorkspaceWidget workspace, Widget root)
	{
		CreateTextWidget(workspace, root, "Saved Templates", 76, 392, 190, 24, 18, 0xFFEFE2C4, 0, true);
		int maxTemplatePage = GetMaxPage(m_aTemplateIds.Count(), TEMPLATES_PER_PAGE);
		m_iTemplatePage = Math.Min(Math.Max(0, m_iTemplatePage), maxTemplatePage);
		int firstTemplate = m_iTemplatePage * TEMPLATES_PER_PAGE;
		int templateLimit = Math.Min(m_aTemplateIds.Count(), firstTemplate + TEMPLATES_PER_PAGE);
		for (int templateIndex = firstTemplate; templateIndex < templateLimit; templateIndex++)
		{
			int visibleTemplate = templateIndex - firstTemplate;
			int topTemplate = 558 + visibleTemplate * 30;
			int color = 0xFF1B252E;
			if (m_aTemplateIds[templateIndex] == m_sSelectedTemplateId)
				color = 0xFF604A24;

			CreateRectWidget(workspace, root, 70, topTemplate, 276, 28, color, 0.96, TEMPLATE_WIDGET_ID_BASE + templateIndex);
			CreateTextWidget(workspace, root, ShortenText(m_aTemplateDisplays[templateIndex], 25), 82, topTemplate + 6, 188, 17, 12, 0xFFE2E6E8, TEMPLATE_WIDGET_ID_BASE + templateIndex, m_aTemplateIds[templateIndex] == m_sSelectedTemplateId);
			CreateTextWidget(workspace, root, string.Format("%1 slots", m_aTemplateSlotCounts[templateIndex]), 276, topTemplate + 6, 60, 17, 10, 0xFFB7C7D7, TEMPLATE_WIDGET_ID_BASE + templateIndex, false);
		}

		if (m_aTemplateIds.Count() == 0)
			CreateTextWidget(workspace, root, "No saved templates.", 76, 452, 230, 22, 13, 0xFFB7C7D7, 0, false);

		CreateButton(workspace, root, "Del", 276, 392, 70, 26, DELETE_TEMPLATE_WIDGET_ID);
		CreateButton(workspace, root, "<", 70, 706, 32, 24, TEMPLATE_PAGE_PREV_WIDGET_ID);
		CreateTextWidget(workspace, root, string.Format("%1 / %2", m_iTemplatePage + 1, maxTemplatePage + 1), 112, 710, 56, 16, 11, 0xFFE2E6E8, 0, true);
		CreateButton(workspace, root, ">", 174, 706, 32, 24, TEMPLATE_PAGE_NEXT_WIDGET_ID);
	}

	protected void RenderSettingsPanel(WorkspaceWidget workspace, Widget root)
	{
		CreateTextWidget(workspace, root, "Editor Settings", 76, 392, 190, 24, 18, 0xFFEFE2C4, 0, true);
		CreateTextWidget(workspace, root, "Camera", 76, 440, 88, 20, 13, 0xFFFFD166, 0, true);
		CreateTextWidget(workspace, root, BuildCameraModeLabel(), 158, 440, 160, 20, 13, 0xFFE2E6E8, 0, false);
		CreateTextWidget(workspace, root, "Spotlight", 76, 468, 88, 20, 13, 0xFFFFD166, 0, true);
		CreateTextWidget(workspace, root, BuildSpotlightLabel(), 158, 468, 160, 20, 13, 0xFFE2E6E8, 0, false);
		CreateTextWidget(workspace, root, ShortenText(m_sLastResult, 48), 76, 520, 250, 54, 13, 0xFFD7B66F, 0, false);
		CreateButton(workspace, root, "Clear", 76, 638, 82, 32, CLEAR_DRAFT_WIDGET_ID);
	}

	protected void RenderFooter(WorkspaceWidget workspace, Widget root)
	{
		CreateRectWidget(workspace, root, 0, 768, 1280, 82, 0xE911171B, 1.0, 0);
		CreateTextWidget(workspace, root, ShortenText(m_sLastResult, 82), 78, 790, 560, 22, 14, 0xFFFFD166, 0, false);
		CreateButton(workspace, root, "< Back", 650, 790, 88, 38, BACK_WIDGET_ID);
		CreateButton(workspace, root, BuildSwapActionLabel(), 754, 790, 86, 38, SWAP_WIDGET_ID);
		CreateButton(workspace, root, "Save", 856, 790, 84, 38, SAVE_WIDGET_ID);
		CreateButton(workspace, root, "Apply", 956, 790, 88, 38, APPLY_WIDGET_ID);
		CreateButton(workspace, root, BuildSpotlightLabel(), 1060, 790, 104, 38, SPOTLIGHT_WIDGET_ID);
		CreateButton(workspace, root, BuildCameraModeLabel(), 1180, 790, 92, 38, CAMERA_WIDGET_ID);
	}

	protected void ParseEditorPayload(string payload)
	{
		if (payload.IsEmpty())
			return;

		ClearEditorPayload();
		array<string> lines = {};
		payload.Split("\n", lines, false);
		foreach (string line : lines)
		{
			array<string> fields = {};
			line.Split("|", fields, false);
			if (fields.Count() == 0)
				continue;

			if (fields[0] == "HST_LOADOUT_EDITOR")
			{
				if (fields.Count() > 1)
					m_sStatusText = "Editor " + fields[1];
				if (fields.Count() > 2)
					m_sSelectedTemplateId = fields[2];
				if (fields.Count() > 3)
					m_bPreviewSpawned = fields[3] == "true" || fields[3] == "1";
				if (fields.Count() > 4)
					m_iDraftSlotCount = fields[4].ToInt();
				if (fields.Count() > 5)
					m_iDraftItemCount = fields[5].ToInt();
				if (fields.Count() > 6)
					m_iDraftFiniteCount = fields[6].ToInt();
				if (fields.Count() > 7)
					m_iDraftInfiniteCount = fields[7].ToInt();
				continue;
			}

			if (fields[0] == "PREVIEW" && fields.Count() >= 5)
			{
				m_bPreviewSpawned = fields[1] == "true" || fields[1] == "1";
				m_sPreviewPosition = fields[2];
				m_iPreviewItemCount = fields[3].ToInt();
				m_sPreviewStatus = fields[4];
				continue;
			}

			if (fields[0] == "PREVIEW_PREFAB" && fields.Count() >= 2)
			{
				m_sPreviewPrefab = fields[1];
				continue;
			}

			if (fields[0] == "CATEGORY" && fields.Count() >= 4)
			{
				m_aCategoryIds.Insert(fields[1]);
				m_aCategoryLabels.Insert(fields[2]);
				m_aCategoryCounts.Insert(fields[3].ToInt());
				continue;
			}

			if (fields[0] == "ITEM" && fields.Count() >= 6)
			{
				m_aItemCategories.Insert(fields[1]);
				m_aItemPrefabs.Insert(fields[2]);
				string itemDisplay = ResolvePayloadDisplayText(fields[3]);
				string itemShortDisplay = itemDisplay;
				string itemSlotLabel = BuildSlotCategoryLabel(fields[1]);
				bool itemPreviewEligible = IsPreviewEligibleCategory(fields[1]);
				if (fields.Count() > 6)
					itemDisplay = ResolvePayloadDisplayText(fields[6]);
				if (fields.Count() > 7)
					itemShortDisplay = ResolvePayloadDisplayText(fields[7]);
				if (fields.Count() > 8)
					itemSlotLabel = fields[8];
				if (fields.Count() > 9)
					itemPreviewEligible = ParsePayloadBool(fields[9], itemPreviewEligible);

				itemDisplay = HST_DisplayNameService.ResolveItemDisplayName(null, fields[2], itemDisplay);
				itemShortDisplay = HST_DisplayNameService.ResolveShortItemDisplayName(itemShortDisplay, fields[2]);
				if (itemShortDisplay.IsEmpty())
					itemShortDisplay = itemDisplay;
				m_aItemDisplays.Insert(itemDisplay);
				m_aItemShortDisplays.Insert(itemShortDisplay);
				m_aItemSlotLabels.Insert(itemSlotLabel);
				m_aItemCounts.Insert(fields[4]);
				m_aItemInfinite.Insert(fields[5] == "INF");
				m_aItemPreviewEligible.Insert(itemPreviewEligible);
				continue;
			}

			if (fields[0] == "SLOT" && fields.Count() >= 6)
			{
				m_aSlotIds.Insert(fields[1]);
				m_aSlotCategories.Insert(fields[2]);
				m_aSlotPrefabs.Insert(fields[3]);
				string slotDisplay = ResolvePayloadDisplayText(fields[4]);
				string slotShortDisplay = slotDisplay;
				string slotLabel = BuildSlotCategoryLabel(fields[2]);
				bool slotPreviewEligible = IsPreviewEligibleCategory(fields[2]);
				if (fields.Count() > 6)
					slotDisplay = ResolvePayloadDisplayText(fields[6]);
				if (fields.Count() > 7)
					slotShortDisplay = ResolvePayloadDisplayText(fields[7]);
				if (fields.Count() > 8)
					slotLabel = fields[8];
				if (fields.Count() > 9)
					slotPreviewEligible = ParsePayloadBool(fields[9], slotPreviewEligible);

				slotDisplay = HST_DisplayNameService.ResolveItemDisplayName(null, fields[3], slotDisplay);
				slotShortDisplay = HST_DisplayNameService.ResolveShortItemDisplayName(slotShortDisplay, fields[3]);
				if (slotShortDisplay.IsEmpty())
					slotShortDisplay = slotDisplay;
				m_aSlotDisplays.Insert(slotDisplay);
				m_aSlotShortDisplays.Insert(slotShortDisplay);
				m_aSlotLabels.Insert(slotLabel);
				m_aSlotQuantities.Insert(Math.Max(1, fields[5].ToInt()));
				m_aSlotPreviewEligible.Insert(slotPreviewEligible);
				continue;
			}

			if (fields[0] == "TEMPLATE" && fields.Count() >= 4)
			{
				m_aTemplateIds.Insert(fields[1]);
				m_aTemplateDisplays.Insert(fields[2]);
				m_aTemplateSlotCounts.Insert(fields[3].ToInt());
			}
		}

		if (m_aCategoryIds.Count() == 0)
			BuildFallbackCategories();

		if (FindCategoryIndex(m_sSelectedCategory) < 0 && m_aCategoryIds.Count() > 0)
			m_sSelectedCategory = m_aCategoryIds[0];

		if (!m_sSelectedTemplateId.IsEmpty() && FindTemplateIndex(m_sSelectedTemplateId) < 0)
			m_sSelectedTemplateId = "";

		if (!m_sSelectedSlotId.IsEmpty() && FindSelectedSlotIndex() < 0)
			m_sSelectedSlotId = "";

		ClampPages();
	}

	protected void ClearEditorPayload()
	{
		m_aCategoryIds.Clear();
		m_aCategoryLabels.Clear();
		m_aCategoryCounts.Clear();
		m_aItemCategories.Clear();
		m_aItemPrefabs.Clear();
		m_aItemDisplays.Clear();
		m_aItemShortDisplays.Clear();
		m_aItemSlotLabels.Clear();
		m_aItemCounts.Clear();
		m_aItemInfinite.Clear();
		m_aItemPreviewEligible.Clear();
		m_aVisibleItemIndexes.Clear();
		m_aSlotIds.Clear();
		m_aSlotCategories.Clear();
		m_aSlotPrefabs.Clear();
		m_aSlotDisplays.Clear();
		m_aSlotShortDisplays.Clear();
		m_aSlotLabels.Clear();
		m_aSlotQuantities.Clear();
		m_aSlotPreviewEligible.Clear();
		m_aTemplateIds.Clear();
		m_aTemplateDisplays.Clear();
		m_aTemplateSlotCounts.Clear();
		m_bPreviewSpawned = false;
		m_iPreviewItemCount = 0;
		m_iDraftSlotCount = 0;
		m_iDraftItemCount = 0;
		m_iDraftFiniteCount = 0;
		m_iDraftInfiniteCount = 0;
		m_sPreviewStatus = "";
		m_sPreviewPosition = "";
		m_sPreviewPrefab = DEFAULT_PREVIEW_PREFAB;
	}

	protected void BuildFallbackCategories()
	{
		if (m_aCategoryIds.Count() > 0)
			return;

		InsertCategory("clothing", "Clothing", 0);
		InsertCategory("headgear", "Headgear", 0);
		InsertCategory("vest", "Vest", 0);
		InsertCategory("backpack", "Backpack", 0);
		InsertCategory("weapon", "Weapons", 0);
		InsertCategory("magazine", "Magazines", 0);
		InsertCategory("explosive", "Explosives", 0);
		InsertCategory("attachment", "Attachments", 0);
		InsertCategory("medical", "Medical", 0);
		InsertCategory("utility", "Utility", 0);
	}

	protected void InsertCategory(string categoryId, string label, int count)
	{
		m_aCategoryIds.Insert(categoryId);
		m_aCategoryLabels.Insert(label);
		m_aCategoryCounts.Insert(count);
	}

	protected int FindCategoryIndex(string categoryId)
	{
		for (int i = 0; i < m_aCategoryIds.Count(); i++)
		{
			if (m_aCategoryIds[i] == categoryId)
				return i;
		}

		return -1;
	}

	protected int FindTemplateIndex(string templateId)
	{
		for (int i = 0; i < m_aTemplateIds.Count(); i++)
		{
			if (m_aTemplateIds[i] == templateId)
				return i;
		}

		return -1;
	}

	protected string ResolveSelectedCategoryLabel()
	{
		int categoryIndex = FindCategoryIndex(m_sSelectedCategory);
		if (categoryIndex >= 0 && categoryIndex < m_aCategoryLabels.Count())
			return m_aCategoryLabels[categoryIndex];

		return "Recovered Items";
	}

	protected int CountItemsForSelectedCategory()
	{
		int count;
		foreach (string category : m_aItemCategories)
		{
			if (category == m_sSelectedCategory)
				count++;
		}

		return count;
	}

	protected string BuildCountLabel(int itemIndex)
	{
		if (itemIndex < 0 || itemIndex >= m_aItemCounts.Count())
			return "";

		if (itemIndex < m_aItemInfinite.Count() && m_aItemInfinite[itemIndex])
			return "INF";

		return "x" + m_aItemCounts[itemIndex];
	}

	protected string BuildPreviewStatusLabel()
	{
		if (!m_bPreviewSpawned)
			return "preview offline";

		if (!m_sPreviewStatus.IsEmpty())
			return ShortenText(m_sPreviewStatus, 28);

		return string.Format("%1 item(s)", m_iPreviewItemCount);
	}

	protected string BuildDraftHeaderSummary()
	{
		return string.Format("%1 slots / %2 items | finite %3 | INF %4", m_iDraftSlotCount, m_iDraftItemCount, m_iDraftFiniteCount, m_iDraftInfiniteCount);
	}

	protected string FindFirstSlotDisplayForCategory(string category)
	{
		for (int i = 0; i < m_aSlotCategories.Count(); i++)
		{
			if (m_aSlotCategories[i] == category)
				return GetSlotShortDisplay(i);
		}

		return "empty";
	}

	protected string FindFirstWeaponSlotDisplay()
	{
		for (int i = 0; i < m_aSlotCategories.Count(); i++)
		{
			if (m_aSlotCategories[i] == "weapon" || m_aSlotCategories[i] == "launcher")
				return GetSlotShortDisplay(i);
		}

		return "empty";
	}

	protected string BuildSupportSlotSummary()
	{
		int magazines = CountSlotsInCategory("magazine");
		int explosives = CountSlotsInCategory("explosive");
		int medical = CountSlotsInCategory("medical");
		int utility = CountSlotsInCategory("utility") + CountSlotsInCategory("attachment");
		if (magazines + explosives + medical + utility <= 0)
			return "empty";

		return string.Format("M%1 E%2 Med%3 U%4", magazines, explosives, medical, utility);
	}

	protected int CountSlotsInCategory(string category)
	{
		int count;
		for (int i = 0; i < m_aSlotCategories.Count(); i++)
		{
			if (m_aSlotCategories[i] == category)
				count += Math.Max(1, m_aSlotQuantities[i]);
		}

		return count;
	}

	protected string BuildSlotCategoryTag(string category)
	{
		if (category == "clothing")
			return "UNI";
		if (category == "headgear")
			return "HEAD";
		if (category == "vest")
			return "VEST";
		if (category == "backpack")
			return "PACK";
		if (category == "weapon")
			return "WPN";
		if (category == "launcher")
			return "AT";
		if (category == "magazine")
			return "MAG";
		if (category == "explosive")
			return "EXP";
		if (category == "attachment")
			return "ATT";
		if (category == "medical")
			return "MED";

		return "KIT";
	}

	protected string BuildSlotCategoryLabel(string category)
	{
		if (category == "clothing")
			return "Uniform";
		if (category == "headgear")
			return "Head";
		if (category == "vest")
			return "Vest";
		if (category == "backpack")
			return "Back";
		if (category == "weapon")
			return "Weapon";
		if (category == "launcher")
			return "Launcher";
		if (category == "magazine")
			return "Ammo";
		if (category == "explosive")
			return "Explosive";
		if (category == "attachment")
			return "Attachment";
		if (category == "medical")
			return "Medical";

		return "Gear";
	}

	protected bool IsPreviewEligibleCategory(string category)
	{
		return category == "clothing" || category == "headgear" || category == "vest" || category == "backpack" || category == "weapon" || category == "launcher" || category == "attachment";
	}

	protected bool ParsePayloadBool(string value, bool fallback)
	{
		if (value == "true" || value == "1" || value == "TRUE")
			return true;
		if (value == "false" || value == "0" || value == "FALSE")
			return false;

		return fallback;
	}

	protected string ResolvePayloadDisplayText(string value)
	{
		if (value.IsEmpty())
			return "";

		if (value.Length() > 0 && value.Substring(0, 1) == "#")
		{
			string translated = WidgetManager.Translate(value);
			if (!translated.IsEmpty() && translated != value && translated.Substring(0, 1) != "#")
				return translated;
		}

		return value;
	}

	protected int GetMaxPage(int itemCount, int pageSize)
	{
		if (pageSize <= 0 || itemCount <= pageSize)
			return 0;

		return (itemCount - 1) / pageSize;
	}

	protected void ClampPages()
	{
		m_iItemPage = Math.Min(Math.Max(0, m_iItemPage), GetMaxPage(CountItemsForSelectedCategory(), ITEMS_PER_PAGE));
		m_iSlotPage = Math.Min(Math.Max(0, m_iSlotPage), GetMaxPage(m_aSlotIds.Count(), SLOTS_PER_PAGE));
		m_iTemplatePage = Math.Min(Math.Max(0, m_iTemplatePage), GetMaxPage(m_aTemplateIds.Count(), TEMPLATES_PER_PAGE));
	}

	protected int GetEditorModeCount()
	{
		return 5;
	}

	protected string GetEditorModeId(int index)
	{
		if (index == 0)
			return "clothing";
		if (index == 1)
			return "weapons";
		if (index == 2)
			return "gear";
		if (index == 3)
			return "templates";

		return "settings";
	}

	protected string GetEditorModeLabel(string modeId)
	{
		if (modeId == "clothing")
			return "Cloth";
		if (modeId == "weapons")
			return "Weapon";
		if (modeId == "gear")
			return "Gear";
		if (modeId == "templates")
			return "Save";

		return "Cfg";
	}

	protected string ResolveDefaultCategoryForMode(string modeId)
	{
		if (modeId == "clothing")
			return "clothing";
		if (modeId == "weapons")
			return "weapon";
		if (modeId == "gear")
			return "magazine";

		return m_sSelectedCategory;
	}

	protected string ResolveModeForCategory(string category)
	{
		if (category == "clothing" || category == "headgear" || category == "vest" || category == "backpack")
			return "clothing";
		if (category == "weapon" || category == "launcher" || category == "attachment")
			return "weapons";
		if (category == "magazine" || category == "explosive" || category == "medical" || category == "utility")
			return "gear";

		return m_sEditorMode;
	}

	protected bool IsCategoryVisibleForMode(string category, string modeId)
	{
		if (modeId == "clothing")
			return category == "clothing" || category == "headgear" || category == "vest" || category == "backpack";
		if (modeId == "weapons")
			return category == "weapon" || category == "launcher" || category == "attachment";
		if (modeId == "gear")
			return category == "magazine" || category == "explosive" || category == "medical" || category == "utility";
		if (modeId == "templates" || modeId == "settings")
			return true;

		return false;
	}

	protected bool IsSlotVisibleInCurrentMode(int slotIndex)
	{
		if (slotIndex < 0 || slotIndex >= m_aSlotCategories.Count())
			return false;

		if (m_sEditorMode == "templates" || m_sEditorMode == "settings")
			return true;

		return IsCategoryVisibleForMode(m_aSlotCategories[slotIndex], m_sEditorMode);
	}

	protected int CountVisibleSlotsForCurrentMode()
	{
		int count;
		for (int i = 0; i < m_aSlotCategories.Count(); i++)
		{
			if (IsSlotVisibleInCurrentMode(i))
				count++;
		}

		return count;
	}

	protected string BuildModeTitle()
	{
		if (m_sEditorMode == "templates")
			return "Current Loadout";
		if (m_sEditorMode == "settings")
			return "Draft Overview";

		return ResolveSelectedCategoryLabel();
	}

	protected void RenderCategoryChips(WorkspaceWidget workspace, Widget root)
	{
		int visible;
		for (int i = 0; i < m_aCategoryIds.Count(); i++)
		{
			if (!IsCategoryVisibleForMode(m_aCategoryIds[i], m_sEditorMode))
				continue;

			if (visible >= 4)
				break;

			int left = 70 + visible * 68;
			int color = 0xFF1A1F23;
			if (m_aCategoryIds[i] == m_sSelectedCategory)
				color = 0xFF6B4C24;

			CreateRectWidget(workspace, root, left, 390, 62, 24, color, 0.98, CATEGORY_WIDGET_ID_BASE + i);
			CreateTextWidget(workspace, root, ShortenText(BuildSlotCategoryTag(m_aCategoryIds[i]), 6), left + 7, 396, 48, 13, 10, 0xFFF5E8CE, CATEGORY_WIDGET_ID_BASE + i, m_aCategoryIds[i] == m_sSelectedCategory);
			visible++;
		}
	}

	protected string BuildSelectedPreviewTitle()
	{
		int slotIndex = FindSelectedSlotIndex();
		if (slotIndex >= 0)
			return GetSlotLabel(slotIndex) + " / " + GetSlotShortDisplay(slotIndex);

		return BuildModeTitle();
	}

	protected string BuildPreviewMetaLine()
	{
		string light = "spotlight off";
		if (m_bSpotlightEnabled)
			light = "spotlight on";

		return string.Format("%1 | %2 | %3", BuildCameraModeLabel(), light, ShortenText(m_sPreviewStatus, 44));
	}

	protected string BuildCameraModeLabel()
	{
		if (m_iCameraMode == 1)
			return "Detail";
		if (m_iCameraMode == 2)
			return "Weapon";

		return "Camera";
	}

	protected string BuildSpotlightLabel()
	{
		if (m_bSpotlightEnabled)
			return "Spotlight";

		return "Light Off";
	}

	protected int FindSelectedSlotIndex()
	{
		if (m_sSelectedSlotId.IsEmpty())
			return -1;

		for (int i = 0; i < m_aSlotIds.Count(); i++)
		{
			if (m_aSlotIds[i] == m_sSelectedSlotId)
				return i;
		}

		return -1;
	}

	protected string GetSlotLabel(int slotIndex)
	{
		if (slotIndex >= 0 && slotIndex < m_aSlotLabels.Count() && !m_aSlotLabels[slotIndex].IsEmpty())
			return m_aSlotLabels[slotIndex];
		if (slotIndex >= 0 && slotIndex < m_aSlotCategories.Count())
			return BuildSlotCategoryLabel(m_aSlotCategories[slotIndex]);

		return "Slot";
	}

	protected string GetSlotShortDisplay(int slotIndex)
	{
		if (slotIndex >= 0 && slotIndex < m_aSlotShortDisplays.Count() && !m_aSlotShortDisplays[slotIndex].IsEmpty())
			return m_aSlotShortDisplays[slotIndex];
		if (slotIndex >= 0 && slotIndex < m_aSlotDisplays.Count() && !m_aSlotDisplays[slotIndex].IsEmpty())
			return m_aSlotDisplays[slotIndex];

		return "empty";
	}

	protected string GetItemSlotLabel(int itemIndex)
	{
		if (itemIndex >= 0 && itemIndex < m_aItemSlotLabels.Count() && !m_aItemSlotLabels[itemIndex].IsEmpty())
			return m_aItemSlotLabels[itemIndex];
		if (itemIndex >= 0 && itemIndex < m_aItemCategories.Count())
			return BuildSlotCategoryLabel(m_aItemCategories[itemIndex]);

		return "Item";
	}

	protected string GetItemShortDisplay(int itemIndex)
	{
		if (itemIndex >= 0 && itemIndex < m_aItemShortDisplays.Count() && !m_aItemShortDisplays[itemIndex].IsEmpty())
			return m_aItemShortDisplays[itemIndex];
		if (itemIndex >= 0 && itemIndex < m_aItemDisplays.Count() && !m_aItemDisplays[itemIndex].IsEmpty())
			return m_aItemDisplays[itemIndex];

		return "item";
	}

	protected bool IsItemSelectedForSlot(int itemIndex)
	{
		int slotIndex = FindSelectedSlotIndex();
		if (slotIndex < 0 || itemIndex < 0 || slotIndex >= m_aSlotPrefabs.Count() || itemIndex >= m_aItemPrefabs.Count())
			return false;

		return m_aSlotPrefabs[slotIndex] == m_aItemPrefabs[itemIndex];
	}

	protected string BuildSwapActionLabel()
	{
		if (FindSelectedSlotIndex() >= 0)
			return "Swap";

		return "Add";
	}

	protected string BuildSwapHeaderText()
	{
		int slotIndex = FindSelectedSlotIndex();
		if (slotIndex >= 0)
			return "Swap " + GetSlotLabel(slotIndex);

		return "Add " + ResolveSelectedCategoryLabel();
	}

	protected string BuildSwapTargetText()
	{
		int slotIndex = FindSelectedSlotIndex();
		if (slotIndex >= 0)
			return GetSlotShortDisplay(slotIndex);

		return "Select recovered gear below";
	}

	protected string BuildSwapHint()
	{
		int slotIndex = FindSelectedSlotIndex();
		if (slotIndex >= 0)
			return "Select a recovered item to swap " + GetSlotLabel(slotIndex);

		return "Select a recovered item to add it to the draft";
	}

	protected void ConfigurePreviewWidget()
	{
		if (!m_PreviewWidget)
			return;

		FrameSlot.SetPos(m_PreviewWidget, 398, 50);
		FrameSlot.SetSize(m_PreviewWidget, 882, 718);
		m_PreviewWidget.SetVisible(true);
	}

	protected void EnsurePreviewWorld()
	{
		if (!m_PreviewWidget)
			return;

		if (!m_PreviewWorldRef)
		{
			m_PreviewWorldRef = BaseWorld.CreateWorld("HSTLoadoutPreview", "HSTLoadoutPreview");
			if (m_PreviewWorldRef)
				m_PreviewWorld = m_PreviewWorldRef.GetRef();

			if (m_PreviewWorld)
			{
				m_PreviewWorld.SetCameraType(PREVIEW_CAMERA, CameraType.PERSPECTIVE);
				m_PreviewWorld.SetCameraNearPlane(PREVIEW_CAMERA, 0.001);
				m_PreviewWorld.SetCameraFarPlane(PREVIEW_CAMERA, 150);
				m_PreviewWorld.SetCameraVerticalFOV(PREVIEW_CAMERA, 38);

				Resource worldResource = Resource.Load(PREVIEW_WORLD_RESOURCE);
				if (worldResource && worldResource.IsValid())
					GetGame().SpawnEntityPrefab(worldResource, m_PreviewWorld);
			}
		}

		if (m_PreviewWorld)
		{
			m_PreviewWidget.SetWorld(m_PreviewWorld, PREVIEW_CAMERA);
			UpdatePreviewCamera(false);
		}
	}

	protected void RefreshPreviewWorldLoadout()
	{
		if (!m_PreviewWorld)
			return;

		string renderKey = BuildPreviewRenderKey();
		if (renderKey == m_sPreviewRenderKey && m_PreviewCharacter)
			return;

		m_sPreviewRenderKey = renderKey;
		if (m_PreviewCharacter)
			SCR_EntityHelper.DeleteEntityAndChildren(m_PreviewCharacter);

		m_PreviewCharacter = null;
		ResourceName previewPrefab = m_sPreviewPrefab;
		if (previewPrefab.IsEmpty())
			previewPrefab = DEFAULT_PREVIEW_PREFAB;

		Resource loaded = Resource.Load(previewPrefab);
		if (!loaded)
		{
			m_sPreviewStatus = "preview character resource failed";
			m_bPreviewSpawned = false;
			return;
		}

		EntitySpawnParams params = new EntitySpawnParams;
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = vector.Zero;
		m_PreviewCharacter = GetGame().SpawnEntityPrefabLocal(loaded, m_PreviewWorld, params);
		if (!m_PreviewCharacter)
		{
			m_sPreviewStatus = "preview character spawn failed";
			m_bPreviewSpawned = false;
			return;
		}

		m_PreviewCharacter.SetFixedLOD(0);
		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(m_PreviewCharacter.FindComponent(SCR_InventoryStorageManagerComponent));
		int inserted;
		int removed = ClearLocalPreviewInventory(inventory);
		string firstFailure;
		for (int slotIndex = 0; slotIndex < m_aSlotPrefabs.Count(); slotIndex++)
		{
			if (slotIndex < m_aSlotPreviewEligible.Count() && !m_aSlotPreviewEligible[slotIndex])
			{
				if (firstFailure.IsEmpty())
					firstFailure = "gear only";
				continue;
			}

			string slotPrefab = m_aSlotPrefabs[slotIndex];
			string failure;
			if (TryInsertLocalPreviewItem(inventory, slotPrefab, failure))
				inserted++;
			else if (firstFailure.IsEmpty())
				firstFailure = failure;
		}

		m_bPreviewSpawned = true;
		m_iPreviewItemCount = inserted;
		if (firstFailure.IsEmpty())
			m_sPreviewStatus = string.Format("preview %1 item(s), cleared %2", inserted, removed);
		else
			m_sPreviewStatus = string.Format("preview %1 item(s), skipped %2", inserted, ShortenText(firstFailure, 28));

		UpdatePreviewCamera(true);
	}

	protected int ClearLocalPreviewInventory(SCR_InventoryStorageManagerComponent inventory)
	{
		if (!inventory)
			return 0;

		array<IEntity> items = {};
		inventory.GetItems(items, EStoragePurpose.PURPOSE_ANY);
		int removed;
		foreach (IEntity item : items)
		{
			if (item && inventory.TryDeleteItem(item))
				removed++;
		}

		return removed;
	}

	protected bool TryInsertLocalPreviewItem(SCR_InventoryStorageManagerComponent inventory, string prefab, out string failure)
	{
		failure = "";
		if (!inventory || prefab.IsEmpty())
		{
			failure = "missing preview inventory";
			return false;
		}

		ResourceName resourceName = prefab;
		Resource loaded = Resource.Load(resourceName);
		if (!loaded)
		{
			failure = "resource failed";
			return false;
		}

		EntitySpawnParams params = new EntitySpawnParams;
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = vector.Zero;
		IEntity itemEntity = GetGame().SpawnEntityPrefabLocal(loaded, m_PreviewWorld, params);
		if (!itemEntity)
		{
			failure = "spawn failed";
			return false;
		}

		BaseInventoryStorageComponent storage = inventory.FindStorageForInsert(itemEntity, null, EStoragePurpose.PURPOSE_ANY);
		if (!storage)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
			failure = "no slot";
			return false;
		}

		HST_LoadoutEditorInsertCallback callback = new HST_LoadoutEditorInsertCallback();
		callback.m_TemporaryEntity = itemEntity;
		inventory.TryInsertItemInStorage(itemEntity, storage, -1, callback);
		return true;
	}

	protected string BuildPreviewRenderKey()
	{
		string key = m_sPreviewPrefab;
		for (int i = 0; i < m_aSlotPrefabs.Count(); i++)
			key = key + "|" + m_aSlotPrefabs[i] + ":" + m_aSlotQuantities[i];

		return key;
	}

	protected bool GetPreviewCharacterBounds(out vector mins, out vector maxs, out vector center, out float size)
	{
		mins = vector.Zero;
		maxs = vector.Zero;
		center = "0 1 0";
		size = 3.0;
		if (!m_PreviewCharacter)
			return false;

		m_PreviewCharacter.GetBounds(mins, maxs);
		center = vector.Lerp(mins, maxs, 0.5);
		size = vector.Distance(mins, maxs);
		return size > 0.1;
	}

	protected void UpdatePreviewCamera(bool allowSelectedFocus)
	{
		if (!m_PreviewWorld)
			return;

		vector mins;
		vector maxs;
		vector boundsCenter;
		float boundsSize;
		bool hasBounds = GetPreviewCharacterBounds(mins, maxs, boundsCenter, boundsSize);
		vector look = boundsCenter;
		if (!hasBounds)
		{
			look = "0 1.0 0";
			boundsSize = 3.0;
		}

		if (look[1] < 1.0)
			look[1] = 1.0;

		float distance = Math.Clamp(boundsSize * 1.18, 3.8, 6.2);
		vector cameraOffset = "0 0 0";
		cameraOffset[0] = distance * 0.55;
		cameraOffset[1] = distance * 0.18;
		cameraOffset[2] = distance;

		bool focusSelectedSlot = false;
		int selectedSlotIndex = FindSelectedSlotIndex();
		if (allowSelectedFocus && selectedSlotIndex >= 0)
		{
			focusSelectedSlot = true;
			if (selectedSlotIndex < m_aSlotPreviewEligible.Count() && !m_aSlotPreviewEligible[selectedSlotIndex])
				focusSelectedSlot = false;
		}

		if (m_iCameraMode == 1 || focusSelectedSlot)
		{
			string category = m_sSelectedCategory;
			if (selectedSlotIndex >= 0)
				category = m_aSlotCategories[selectedSlotIndex];

			if (category == "headgear")
			{
				look[1] = Math.Max(1.52, look[1] + 0.45);
				distance = Math.Clamp(boundsSize * 0.95, 3.4, 4.8);
				cameraOffset[0] = distance * 0.48;
				cameraOffset[1] = distance * 0.08;
				cameraOffset[2] = distance * 0.86;
			}
			else if (category == "vest" || category == "clothing")
			{
				look[1] = Math.Max(1.15, look[1] + 0.1);
				distance = Math.Clamp(boundsSize * 1.02, 3.6, 5.1);
				cameraOffset[0] = distance * 0.55;
				cameraOffset[1] = distance * 0.12;
				cameraOffset[2] = distance * 0.92;
			}
			else if (category == "backpack")
			{
				look[1] = Math.Max(1.15, look[1] + 0.08);
				distance = Math.Clamp(boundsSize * 1.05, 3.8, 5.2);
				cameraOffset[0] = -distance * 0.58;
				cameraOffset[1] = distance * 0.14;
				cameraOffset[2] = distance * 0.96;
			}
		}

		if (m_iCameraMode == 2 || m_sSelectedCategory == "weapon" || m_sSelectedCategory == "launcher")
		{
			look[1] = Math.Max(1.18, boundsCenter[1] + 0.08);
			distance = Math.Clamp(boundsSize * 1.0, 3.8, 5.3);
			cameraOffset[0] = distance * 0.7;
			cameraOffset[1] = distance * 0.1;
			cameraOffset[2] = distance * 0.86;
		}

		vector camera = look + cameraOffset;
		vector mat[4];
		SCR_Math3D.LookAt(camera, look, vector.Up, mat);
		m_PreviewWorld.SetCameraEx(PREVIEW_CAMERA, mat);
	}

	protected void DeletePreviewWorld()
	{
		if (m_PreviewCharacter)
			SCR_EntityHelper.DeleteEntityAndChildren(m_PreviewCharacter);

		m_PreviewCharacter = null;
		m_PreviewWidget = null;
		m_PreviewWorld = null;
		m_PreviewWorldRef = null;
		m_sPreviewRenderKey = "";
	}

	protected void CreateButton(WorkspaceWidget workspace, Widget root, string label, int left, int top, int width, int height, int userId)
	{
		CreateRectWidget(workspace, root, left, top, width, height, 0xFF5F6C76, 0.96, userId);
		int inset = 16;
		int fontSize = 17;
		int topInset = 8;
		if (width < 64)
			inset = 8;
		if (width < 32)
		{
			inset = 4;
			fontSize = 11;
			topInset = 4;
		}

		CreateTextWidget(workspace, root, label, left + inset, top + topInset, Math.Max(8, width - inset * 2), Math.Max(8, height - 8), fontSize, 0xFFF4EBD6, userId, true);
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
