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
	static const int DELETE_TEMPLATE_WIDGET_ID = 9204;
	static const int ITEM_PAGE_PREV_WIDGET_ID = 9205;
	static const int ITEM_PAGE_NEXT_WIDGET_ID = 9206;
	static const int SLOT_PAGE_PREV_WIDGET_ID = 9207;
	static const int SLOT_PAGE_NEXT_WIDGET_ID = 9208;
	static const int TEMPLATE_PAGE_PREV_WIDGET_ID = 9209;
	static const int TEMPLATE_PAGE_NEXT_WIDGET_ID = 9210;
	static const int CLEAR_DRAFT_WIDGET_ID = 9211;
	static const int CATEGORY_WIDGET_ID_BASE = 9300;
	static const int ITEM_WIDGET_ID_BASE = 9400;
	static const int SLOT_REMOVE_WIDGET_ID_BASE = 9500;
	static const int SLOT_MINUS_WIDGET_ID_BASE = 9600;
	static const int SLOT_PLUS_WIDGET_ID_BASE = 9700;
	static const int TEMPLATE_WIDGET_ID_BASE = 9800;
	static const int ITEMS_PER_PAGE = 9;
	static const int SLOTS_PER_PAGE = 8;
	static const int TEMPLATES_PER_PAGE = 4;

	protected static HST_LoadoutEditorComponent s_LocalInstance;

	protected IEntity m_OwnerEntity;
	protected bool m_bIsLocalOwner;
	protected bool m_bEditorOpen;
	protected string m_sSelectedCategory = "weapon";
	protected string m_sSelectedTemplateId;
	protected string m_sStatusText = "Choose a saved template or build a draft from recovered arsenal gear.";
	protected string m_sPreviewStatus;
	protected string m_sPreviewPosition;
	protected string m_sLastResult;
	protected bool m_bPreviewSpawned;
	protected int m_iPreviewItemCount;
	protected int m_iDraftSlotCount;
	protected int m_iDraftItemCount;
	protected int m_iDraftFiniteCount;
	protected int m_iDraftInfiniteCount;
	protected int m_iItemPage;
	protected int m_iSlotPage;
	protected int m_iTemplatePage;
	protected ref array<string> m_aCategoryIds = {};
	protected ref array<string> m_aCategoryLabels = {};
	protected ref array<int> m_aCategoryCounts = {};
	protected ref array<string> m_aItemCategories = {};
	protected ref array<string> m_aItemPrefabs = {};
	protected ref array<string> m_aItemDisplays = {};
	protected ref array<string> m_aItemCounts = {};
	protected ref array<bool> m_aItemInfinite = {};
	protected ref array<int> m_aVisibleItemIndexes = {};
	protected ref array<string> m_aSlotIds = {};
	protected ref array<string> m_aSlotCategories = {};
	protected ref array<string> m_aSlotPrefabs = {};
	protected ref array<string> m_aSlotDisplays = {};
	protected ref array<int> m_aSlotQuantities = {};
	protected ref array<string> m_aTemplateIds = {};
	protected ref array<string> m_aTemplateDisplays = {};
	protected ref array<int> m_aTemplateSlotCounts = {};
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

		int categoryIndex = widgetId - CATEGORY_WIDGET_ID_BASE;
		if (categoryIndex >= 0 && categoryIndex < m_aCategoryIds.Count())
		{
			m_sSelectedCategory = m_aCategoryIds[categoryIndex];
			m_iItemPage = 0;
			RenderEditor();
			return true;
		}

		int itemVisibleIndex = widgetId - ITEM_WIDGET_ID_BASE;
		if (itemVisibleIndex >= 0 && itemVisibleIndex < m_aVisibleItemIndexes.Count())
		{
			int itemIndex = m_aVisibleItemIndexes[itemVisibleIndex];
			if (itemIndex >= 0 && itemIndex < m_aItemPrefabs.Count())
			{
				m_sLastResult = "requested add item";
				RequestServerAction("loadout_add_item", m_aItemPrefabs[itemIndex]);
				RenderEditor();
				return true;
			}
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
		CreateRectWidget(workspace, root, 0, 0, 1280, 76, 0xFF111920, 1.0, 0);
		CreateRectWidget(workspace, root, 0, 76, 1280, 3, 0xFFC4953B, 1.0, 0);
		CreateTextWidget(workspace, root, "h-istasi Loadout Editor", 32, 16, 430, 38, 30, 0xFFF2D18B, 0, true);
		CreateTextWidget(workspace, root, BuildDraftHeaderSummary(), 468, 26, 560, 26, 15, 0xFFB7C7D7, 0, false);
		CreateButton(workspace, root, "Close", 1160, 16, 88, 42, CLOSE_WIDGET_ID);

		RenderCategories(workspace, root);
		RenderPreviewStage(workspace, root);
		RenderItemBrowser(workspace, root);
		RenderLoadoutPanel(workspace, root);
		RenderFooter(workspace, root);
	}

	protected void RenderCategories(WorkspaceWidget workspace, Widget root)
	{
		CreateRectWidget(workspace, root, 24, 96, 230, 650, 0xFF111920, 0.98, 0);
		CreateTextWidget(workspace, root, "Categories", 46, 118, 180, 28, 21, 0xFFEFE2C4, 0, true);

		if (m_aCategoryIds.Count() == 0)
			BuildFallbackCategories();

		for (int i = 0; i < m_aCategoryIds.Count(); i++)
		{
			int top = 164 + i * 50;
			int color = 0xFF1B252E;
			if (m_aCategoryIds[i] == m_sSelectedCategory)
				color = 0xFF604A24;

			CreateRectWidget(workspace, root, 44, top, 188, 34, color, 0.96, CATEGORY_WIDGET_ID_BASE + i);
			string countLabel = string.Format(" (%1)", m_aCategoryCounts[i]);
			CreateTextWidget(workspace, root, ShortenText(m_aCategoryLabels[i] + countLabel, 22), 58, top + 7, 160, 22, 16, 0xFFE2E6E8, CATEGORY_WIDGET_ID_BASE + i, m_aCategoryIds[i] == m_sSelectedCategory);
		}
	}

	protected void RenderPreviewStage(WorkspaceWidget workspace, Widget root)
	{
		CreateRectWidget(workspace, root, 280, 96, 520, 320, 0xFF0E1216, 0.99, 0);
		CreateRectWidget(workspace, root, 300, 118, 480, 276, 0xFF172129, 1.0, 0);
		CreateTextWidget(workspace, root, "Preview Mannequin", 322, 132, 250, 28, 21, 0xFFEFE2C4, 0, true);
		CreateTextWidget(workspace, root, BuildPreviewStatusLabel(), 532, 136, 220, 24, 14, 0xFFB7E48F, 0, false);

		CreateRectWidget(workspace, root, 492, 176, 86, 38, 0xFF293643, 0.95, 0);
		CreateRectWidget(workspace, root, 470, 218, 130, 92, 0xFF31404D, 0.95, 0);
		CreateRectWidget(workspace, root, 438, 226, 28, 104, 0xFF263440, 0.95, 0);
		CreateRectWidget(workspace, root, 604, 226, 28, 104, 0xFF263440, 0.95, 0);
		CreateRectWidget(workspace, root, 486, 314, 32, 58, 0xFF263440, 0.95, 0);
		CreateRectWidget(workspace, root, 552, 314, 32, 58, 0xFF263440, 0.95, 0);
		CreateRectWidget(workspace, root, 474, 240, 122, 30, 0xFF586A3A, 0.98, 0);

		RenderMannequinCallout(workspace, root, 322, 178, "Head", FindFirstSlotDisplayForCategory("headgear"));
		RenderMannequinCallout(workspace, root, 322, 224, "Uniform", FindFirstSlotDisplayForCategory("clothing"));
		RenderMannequinCallout(workspace, root, 322, 270, "Vest", FindFirstSlotDisplayForCategory("vest"));
		RenderMannequinCallout(workspace, root, 626, 178, "Pack", FindFirstSlotDisplayForCategory("backpack"));
		RenderMannequinCallout(workspace, root, 626, 224, "Weapon", FindFirstWeaponSlotDisplay());
		RenderMannequinCallout(workspace, root, 626, 270, "Support", BuildSupportSlotSummary());

		CreateTextWidget(workspace, root, ShortenText(m_sPreviewPosition, 44), 322, 358, 420, 22, 13, 0xFF9DB4C8, 0, false);
	}

	protected void RenderItemBrowser(WorkspaceWidget workspace, Widget root)
	{
		CreateRectWidget(workspace, root, 280, 432, 520, 314, 0xFF111920, 0.98, 0);
		CreateTextWidget(workspace, root, ResolveSelectedCategoryLabel(), 302, 452, 250, 26, 21, 0xFFEFE2C4, 0, true);
		CreateTextWidget(workspace, root, string.Format("%1 item types", CountItemsForSelectedCategory()), 594, 456, 120, 22, 14, 0xFFB7C7D7, 0, false);

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

			int top = 492 + visibleIndex * 26;
			m_aVisibleItemIndexes.Insert(itemIndex);
			CreateRectWidget(workspace, root, 302, top, 456, 22, 0xFF202B34, 0.98, ITEM_WIDGET_ID_BASE + visibleIndex);
			CreateTextWidget(workspace, root, ShortenText(m_aItemDisplays[itemIndex], 40), 314, top + 3, 310, 17, 12, 0xFFE2E6E8, ITEM_WIDGET_ID_BASE + visibleIndex, false);
			CreateTextWidget(workspace, root, BuildCountLabel(itemIndex), 652, top + 3, 84, 17, 12, 0xFFFFD166, ITEM_WIDGET_ID_BASE + visibleIndex, true);
			visibleIndex++;
			categoryIndex++;
		}

		if (visibleIndex == 0)
			CreateTextWidget(workspace, root, "No recovered items.", 320, 540, 240, 24, 16, 0xFFB7C7D7, 0, false);

		CreateButton(workspace, root, "<", 302, 710, 42, 26, ITEM_PAGE_PREV_WIDGET_ID);
		CreateTextWidget(workspace, root, string.Format("%1 / %2", m_iItemPage + 1, maxPage + 1), 354, 714, 70, 18, 13, 0xFFE2E6E8, 0, true);
		CreateButton(workspace, root, ">", 432, 710, 42, 26, ITEM_PAGE_NEXT_WIDGET_ID);
	}

	protected void RenderLoadoutPanel(WorkspaceWidget workspace, Widget root)
	{
		CreateRectWidget(workspace, root, 826, 96, 430, 650, 0xFF111920, 0.98, 0);
		CreateTextWidget(workspace, root, "Current Loadout", 850, 118, 220, 28, 21, 0xFFEFE2C4, 0, true);
		CreateTextWidget(workspace, root, BuildDraftHeaderSummary(), 850, 146, 360, 20, 13, 0xFFB7C7D7, 0, false);

		int maxSlotPage = GetMaxPage(m_aSlotIds.Count(), SLOTS_PER_PAGE);
		m_iSlotPage = Math.Min(Math.Max(0, m_iSlotPage), maxSlotPage);
		int firstSlot = m_iSlotPage * SLOTS_PER_PAGE;
		int slotLimit = Math.Min(m_aSlotIds.Count(), firstSlot + SLOTS_PER_PAGE);
		for (int slotIndex = firstSlot; slotIndex < slotLimit; slotIndex++)
		{
			int visibleSlot = slotIndex - firstSlot;
			int top = 176 + visibleSlot * 34;
			CreateRectWidget(workspace, root, 850, top - 3, 374, 28, 0xFF1B252E, 0.96, 0);
			CreateTextWidget(workspace, root, ShortenText(BuildSlotCategoryTag(m_aSlotCategories[slotIndex]), 8), 858, top, 58, 20, 12, 0xFFFFD166, 0, true);
			CreateTextWidget(workspace, root, ShortenText(m_aSlotDisplays[slotIndex], 25), 918, top, 132, 20, 12, 0xFFE2E6E8, 0, false);
			CreateButton(workspace, root, "-", 1056, top - 3, 34, 28, SLOT_MINUS_WIDGET_ID_BASE + slotIndex);
			CreateTextWidget(workspace, root, string.Format("x%1", m_aSlotQuantities[slotIndex]), 1098, top, 42, 24, 14, 0xFFFFD166, 0, true);
			CreateButton(workspace, root, "+", 1140, top - 3, 34, 28, SLOT_PLUS_WIDGET_ID_BASE + slotIndex);
			CreateButton(workspace, root, "X", 1184, top - 3, 34, 28, SLOT_REMOVE_WIDGET_ID_BASE + slotIndex);
		}

		if (m_aSlotIds.Count() == 0)
			CreateTextWidget(workspace, root, "Draft is empty.", 852, 180, 180, 24, 15, 0xFFCAD2D7, 0, false);

		CreateButton(workspace, root, "<", 850, 456, 42, 26, SLOT_PAGE_PREV_WIDGET_ID);
		CreateTextWidget(workspace, root, string.Format("%1 / %2", m_iSlotPage + 1, maxSlotPage + 1), 902, 460, 70, 18, 13, 0xFFE2E6E8, 0, true);
		CreateButton(workspace, root, ">", 980, 456, 42, 26, SLOT_PAGE_NEXT_WIDGET_ID);
		CreateButton(workspace, root, "Clear", 1136, 456, 88, 26, CLEAR_DRAFT_WIDGET_ID);

		CreateRectWidget(workspace, root, 850, 504, 380, 4, 0xFFC4953B, 1.0, 0);
		CreateTextWidget(workspace, root, "Saved Templates", 850, 522, 220, 28, 21, 0xFFEFE2C4, 0, true);

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

			CreateRectWidget(workspace, root, 850, topTemplate, 298, 24, color, 0.96, TEMPLATE_WIDGET_ID_BASE + templateIndex);
			CreateTextWidget(workspace, root, ShortenText(m_aTemplateDisplays[templateIndex], 25), 862, topTemplate + 4, 204, 17, 12, 0xFFE2E6E8, TEMPLATE_WIDGET_ID_BASE + templateIndex, m_aTemplateIds[templateIndex] == m_sSelectedTemplateId);
			CreateTextWidget(workspace, root, string.Format("%1 slots", m_aTemplateSlotCounts[templateIndex]), 1080, topTemplate + 4, 70, 17, 12, 0xFFB7C7D7, TEMPLATE_WIDGET_ID_BASE + templateIndex, false);
		}

		CreateButton(workspace, root, "Del", 1158, 558, 70, 26, DELETE_TEMPLATE_WIDGET_ID);
		CreateButton(workspace, root, "<", 850, 682, 42, 26, TEMPLATE_PAGE_PREV_WIDGET_ID);
		CreateTextWidget(workspace, root, string.Format("%1 / %2", m_iTemplatePage + 1, maxTemplatePage + 1), 902, 686, 70, 18, 13, 0xFFE2E6E8, 0, true);
		CreateButton(workspace, root, ">", 980, 682, 42, 26, TEMPLATE_PAGE_NEXT_WIDGET_ID);

		CreateTextWidget(workspace, root, ShortenText(m_sStatusText, 86), 850, 720, 370, 20, 13, 0xFFB7E48F, 0, false);
	}

	protected void RenderFooter(WorkspaceWidget workspace, Widget root)
	{
		CreateRectWidget(workspace, root, 0, 768, 1280, 82, 0xFF111920, 1.0, 0);
		CreateButton(workspace, root, "Save", 802, 790, 106, 42, SAVE_WIDGET_ID);
		CreateButton(workspace, root, "Apply", 928, 790, 106, 42, APPLY_WIDGET_ID);
		CreateButton(workspace, root, "Cancel", 1054, 790, 106, 42, CANCEL_WIDGET_ID);
		CreateTextWidget(workspace, root, ShortenText(m_sLastResult, 104), 32, 790, 720, 22, 15, 0xFFFFD166, 0, false);
		CreateTextWidget(workspace, root, "Finite counts commit after inventory apply. INF unlocks stay reusable.", 32, 818, 720, 20, 13, 0xFFB7C7D7, 0, false);
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
				m_aItemDisplays.Insert(fields[3]);
				m_aItemCounts.Insert(fields[4]);
				m_aItemInfinite.Insert(fields[5] == "INF");
				continue;
			}

			if (fields[0] == "SLOT" && fields.Count() >= 6)
			{
				m_aSlotIds.Insert(fields[1]);
				m_aSlotCategories.Insert(fields[2]);
				m_aSlotPrefabs.Insert(fields[3]);
				m_aSlotDisplays.Insert(fields[4]);
				m_aSlotQuantities.Insert(Math.Max(1, fields[5].ToInt()));
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
		m_aItemCounts.Clear();
		m_aItemInfinite.Clear();
		m_aVisibleItemIndexes.Clear();
		m_aSlotIds.Clear();
		m_aSlotCategories.Clear();
		m_aSlotPrefabs.Clear();
		m_aSlotDisplays.Clear();
		m_aSlotQuantities.Clear();
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

	protected void RenderMannequinCallout(WorkspaceWidget workspace, Widget root, int left, int top, string label, string value)
	{
		CreateRectWidget(workspace, root, left, top, 128, 34, 0xFF101820, 0.94, 0);
		CreateTextWidget(workspace, root, label, left + 8, top + 4, 52, 14, 11, 0xFFFFD166, 0, true);
		CreateTextWidget(workspace, root, ShortenText(value, 16), left + 8, top + 18, 110, 14, 11, 0xFFE2E6E8, 0, false);
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
				return m_aSlotDisplays[i];
		}

		return "empty";
	}

	protected string FindFirstWeaponSlotDisplay()
	{
		for (int i = 0; i < m_aSlotCategories.Count(); i++)
		{
			if (m_aSlotCategories[i] == "weapon" || m_aSlotCategories[i] == "launcher")
				return m_aSlotDisplays[i];
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

	protected void CreateButton(WorkspaceWidget workspace, Widget root, string label, int left, int top, int width, int height, int userId)
	{
		CreateRectWidget(workspace, root, left, top, width, height, 0xFF5F6C76, 0.96, userId);
		int inset = 16;
		if (width < 64)
			inset = 8;

		CreateTextWidget(workspace, root, label, left + inset, top + 8, Math.Max(8, width - inset * 2), Math.Max(8, height - 12), 17, 0xFFF4EBD6, userId, true);
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
