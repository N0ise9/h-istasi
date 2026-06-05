class HST_LoadoutEditorService
{
	static const string PREVIEW_MANNEQUIN_PREFAB = "{84B40583F4D1B7A3}Prefabs/Characters/Factions/INDFOR/FIA/Character_FIA_Rifleman.et";
	static const string LOADOUT_DIRECTORY = "$profile:h-istasi/loadouts";
	static const int MAX_AUTO_DRAFT_SLOTS = 12;

	protected ref array<string> m_aPreviewIdentityIds = {};
	protected ref array<IEntity> m_aPreviewEntities = {};

	string OpenEditor(HST_CampaignState state, string identityId, int playerId)
	{
		if (!state || identityId.IsEmpty())
			return "h-istasi loadout editor | failed: campaign/player state not ready";

		HST_LoadoutEditorSessionState session = FindOrCreateSession(state, identityId);
		int loadedTemplates = LoadPersonalLoadoutsFromFile(state, identityId);
		session.m_sStatus = "open";
		session.m_sLastFailure = "";
		session.m_sPreviewPrefab = PREVIEW_MANNEQUIN_PREFAB;
		session.m_iOpenedAtSecond = state.m_iElapsedSeconds;
		session.m_iSavedLoadoutCount = CountSavedLoadouts(state, identityId);
		RefreshIssuedCounts(state, identityId, session);

		if (!SpawnPreviewMannequin(state, identityId, playerId, session))
			session.m_sLastFailure = "preview mannequin could not spawn; editor economy remains usable";

		state.m_sLoadoutEditorStatus = string.Format("open for %1 | preview %2 | file templates %3", identityId, session.m_bPreviewSpawned, loadedTemplates);
		state.m_sLastLoadoutEditorFailure = session.m_sLastFailure;
		return "h-istasi loadout editor | opened custom arsenal editor | " + BuildEditorReport(state, identityId);
	}

	string CloseEditor(HST_CampaignState state, string identityId)
	{
		if (!state || identityId.IsEmpty())
			return "h-istasi loadout editor | failed: campaign/player state not ready";

		DeletePreviewMannequin(identityId);
		HST_LoadoutEditorSessionState session = FindOrCreateSession(state, identityId);
		session.m_sStatus = "closed";
		session.m_bPreviewSpawned = false;
		session.m_sLastFailure = "";
		state.m_sLoadoutEditorStatus = "closed";
		state.m_sLastLoadoutEditorFailure = "";
		return "h-istasi loadout editor | closed without arsenal count changes";
	}

	string BuildEditorReport(HST_CampaignState state, string identityId)
	{
		if (!state)
			return "h-istasi loadout editor | campaign state not ready";

		HST_LoadoutEditorSessionState session = state.FindLoadoutEditorSession(identityId);
		string status = state.m_sLoadoutEditorStatus;
		if (session)
			status = string.Format("%1 | preview %2 | saved %3 | issued finite %4 | issued INF %5", session.m_sStatus, session.m_bPreviewSpawned, CountSavedLoadouts(state, identityId), session.m_iIssuedFiniteCount, session.m_iIssuedInfiniteCount);

		string report = string.Format("status %1 | arsenal categories %2", status, BuildAvailableCategorySummary(state));
		HST_SavedLoadoutState firstLoadout = state.FindFirstSavedLoadout(identityId);
		if (firstLoadout)
			report = report + string.Format(" | first saved %1 (%2 slots)", firstLoadout.m_sDisplayName, firstLoadout.m_aSlots.Count());
		else
			report = report + " | no saved loadouts";

		if (!state.m_sLastLoadoutEditorFailure.IsEmpty())
			report = report + " | last failure " + state.m_sLastLoadoutEditorFailure;

		return report;
	}

	string SaveCurrentDraft(HST_CampaignState state, string identityId, string loadoutName = "")
	{
		if (!state || identityId.IsEmpty())
			return "h-istasi loadout editor | failed: campaign/player state not ready";

		if (loadoutName.IsEmpty())
			loadoutName = "Field Loadout " + string.Format("%1", CountSavedLoadouts(state, identityId) + 1);

		HST_SavedLoadoutState loadout = new HST_SavedLoadoutState();
		loadout.m_sOwnerIdentityId = identityId;
		loadout.m_sLoadoutId = BuildLoadoutId(identityId, state.m_iElapsedSeconds, state.m_aSavedLoadouts.Count());
		loadout.m_sDisplayName = loadoutName;
		loadout.m_iUpdatedAtSecond = state.m_iElapsedSeconds;

		BuildDraftSlotsFromIssued(state, identityId, loadout);
		if (loadout.m_aSlots.Count() == 0)
			BuildStarterDraftSlotsFromArsenal(state, loadout);

		if (loadout.m_aSlots.Count() == 0)
		{
			state.m_sLastLoadoutEditorFailure = "no arsenal items available to save into a draft";
			return "h-istasi loadout editor | failed: no issued or arsenal items available for a draft";
		}

		state.m_aSavedLoadouts.Insert(loadout);
		SavePersonalLoadoutsToFile(state, identityId);
		HST_LoadoutEditorSessionState session = FindOrCreateSession(state, identityId);
		session.m_sCurrentLoadoutId = loadout.m_sLoadoutId;
		session.m_iSavedLoadoutCount = CountSavedLoadouts(state, identityId);
		session.m_sStatus = "draft saved";
		state.m_sLoadoutEditorStatus = string.Format("saved %1", loadout.m_sDisplayName);
		state.m_sLastLoadoutEditorFailure = "";
		return string.Format("h-istasi loadout editor | saved %1 | slots %2", loadout.m_sDisplayName, loadout.m_aSlots.Count());
	}

	string ApplySavedLoadout(HST_CampaignState state, HST_ArsenalService arsenal, string identityId, int playerId, string loadoutId = "")
	{
		if (!state || !arsenal || identityId.IsEmpty())
			return "h-istasi loadout editor | failed: service not ready";

		HST_SavedLoadoutState loadout = SelectLoadout(state, identityId, loadoutId);
		if (!loadout)
			return "h-istasi loadout editor | failed: no saved personal loadout";

		string validationFailure;
		if (!ValidateLoadoutTransaction(state, loadout, identityId, validationFailure))
		{
			state.m_sLastLoadoutEditorFailure = validationFailure;
			return "h-istasi loadout editor | failed: " + validationFailure;
		}

		if (!ApplyLoadoutToPlayerEntity(loadout, playerId, validationFailure))
		{
			state.m_sLastLoadoutEditorFailure = validationFailure;
			return "h-istasi loadout editor | failed: " + validationFailure;
		}

		CommitLoadoutTransaction(state, arsenal, loadout, identityId);
		HST_LoadoutEditorSessionState session = FindOrCreateSession(state, identityId);
		session.m_sStatus = "applied";
		session.m_sCurrentLoadoutId = loadout.m_sLoadoutId;
		RefreshIssuedCounts(state, identityId, session);
		state.m_sLoadoutEditorStatus = string.Format("applied %1", loadout.m_sDisplayName);
		state.m_sLastLoadoutEditorFailure = "";
		return string.Format("h-istasi loadout editor | applied %1 | finite %2 | INF %3", loadout.m_sDisplayName, session.m_iIssuedFiniteCount, session.m_iIssuedInfiniteCount);
	}

	string MarkIssuedLoadoutLostOnDeath(HST_CampaignState state, string identityId)
	{
		if (!state || identityId.IsEmpty())
			return "h-istasi loadout editor | death ledger clear skipped";

		int cleared;
		for (int i = state.m_aIssuedLoadoutItems.Count() - 1; i >= 0; i--)
		{
			HST_IssuedLoadoutItemState issuedItem = state.m_aIssuedLoadoutItems[i];
			if (!issuedItem || issuedItem.m_sOwnerIdentityId != identityId)
				continue;

			state.m_aIssuedLoadoutItems.Remove(i);
			cleared++;
		}

		HST_LoadoutEditorSessionState session = state.FindLoadoutEditorSession(identityId);
		if (session)
			RefreshIssuedCounts(state, identityId, session);

		return string.Format("h-istasi loadout editor | death cleared %1 issued item ledger entries without refund", cleared);
	}

	protected HST_LoadoutEditorSessionState FindOrCreateSession(HST_CampaignState state, string identityId)
	{
		HST_LoadoutEditorSessionState session = state.FindLoadoutEditorSession(identityId);
		if (session)
			return session;

		session = new HST_LoadoutEditorSessionState();
		session.m_sOwnerIdentityId = identityId;
		state.m_aLoadoutEditorSessions.Insert(session);
		return session;
	}

	protected bool SpawnPreviewMannequin(HST_CampaignState state, string identityId, int playerId, HST_LoadoutEditorSessionState session)
	{
		DeletePreviewMannequin(identityId);

		SCR_RespawnSystemComponent respawnSystem = SCR_RespawnSystemComponent.GetInstance();
		if (!respawnSystem)
			return false;

		vector previewPosition = ResolvePreviewPosition(state, playerId);
		vector previewAngles = "180 0 0";
		GenericEntity preview = respawnSystem.DoSpawn(PREVIEW_MANNEQUIN_PREFAB, previewPosition, previewAngles);
		if (!preview)
			return false;

		m_aPreviewIdentityIds.Insert(identityId);
		m_aPreviewEntities.Insert(preview);
		session.m_vPreviewPosition = previewPosition;
		session.m_bPreviewSpawned = true;
		Print(string.Format("h-istasi loadout editor | preview mannequin spawned for %1 at %2 using %3", identityId, previewPosition, PREVIEW_MANNEQUIN_PREFAB));
		return true;
	}

	protected void DeletePreviewMannequin(string identityId)
	{
		for (int i = m_aPreviewIdentityIds.Count() - 1; i >= 0; i--)
		{
			if (m_aPreviewIdentityIds[i] != identityId)
				continue;

			IEntity preview = m_aPreviewEntities[i];
			if (preview)
				SCR_EntityHelper.DeleteEntityAndChildren(preview);

			m_aPreviewIdentityIds.Remove(i);
			m_aPreviewEntities.Remove(i);
		}
	}

	protected vector ResolvePreviewPosition(HST_CampaignState state, int playerId)
	{
		if (state && !IsZeroVector(state.m_vArsenalPosition))
		{
			vector hqPreview = state.m_vArsenalPosition;
			hqPreview[0] = hqPreview[0] + 6.0;
			hqPreview[2] = hqPreview[2] + 6.0;
			vector resolvedArsenalPreview;
			if (HST_WorldPositionService.TryResolveGroundPosition(hqPreview, HST_WorldPositionService.CHARACTER_GROUND_OFFSET, resolvedArsenalPreview, true))
				return resolvedArsenalPreview;

			return hqPreview;
		}

		return "0 0 0";
	}

	protected IEntity ResolveControlledPlayerEntity(int playerId)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager || playerId <= 0)
			return null;

		IEntity controlledEntity = playerManager.GetPlayerControlledEntity(playerId);
		if (controlledEntity)
			return controlledEntity;

		return SCR_PossessingManagerComponent.GetPlayerMainEntity(playerId);
	}

	protected void BuildDraftSlotsFromIssued(HST_CampaignState state, string identityId, HST_SavedLoadoutState loadout)
	{
		foreach (HST_IssuedLoadoutItemState issuedItem : state.m_aIssuedLoadoutItems)
		{
			if (!issuedItem || issuedItem.m_sOwnerIdentityId != identityId || issuedItem.m_sItemPrefab.IsEmpty())
				continue;

			HST_LoadoutSlotState slot = new HST_LoadoutSlotState();
			slot.m_sSlotId = BuildSlotId(issuedItem.m_sCategory, loadout.m_aSlots.Count());
			slot.m_sItemPrefab = issuedItem.m_sItemPrefab;
			slot.m_sDisplayName = issuedItem.m_sDisplayName;
			slot.m_sCategory = issuedItem.m_sCategory;
			slot.m_iQuantity = Math.Max(1, issuedItem.m_iCount);
			loadout.m_aSlots.Insert(slot);
		}
	}

	protected void BuildStarterDraftSlotsFromArsenal(HST_CampaignState state, HST_SavedLoadoutState loadout)
	{
		array<string> usedCategories = {};
		foreach (HST_ArsenalItemState item : state.m_aArsenalItems)
		{
			if (!item || item.m_sPrefab.IsEmpty())
				continue;

			if (!item.m_bUnlocked && item.m_iCount <= 0)
				continue;

			string category = ResolveEditorCategory(item.m_sPrefab, item.m_sCategory);
			if (usedCategories.Find(category) >= 0 && category != "magazine" && category != "attachment" && category != "medical" && category != "utility")
				continue;

			HST_LoadoutSlotState slot = new HST_LoadoutSlotState();
			slot.m_sSlotId = BuildSlotId(category, loadout.m_aSlots.Count());
			slot.m_sItemPrefab = item.m_sPrefab;
			slot.m_sDisplayName = HST_DisplayNameService.ResolveItemDisplayName(null, item.m_sPrefab, item.m_sDisplayName);
			slot.m_sCategory = category;
			slot.m_iQuantity = 1;
			loadout.m_aSlots.Insert(slot);
			usedCategories.Insert(category);
			if (loadout.m_aSlots.Count() >= MAX_AUTO_DRAFT_SLOTS)
				return;
		}
	}

	protected HST_SavedLoadoutState SelectLoadout(HST_CampaignState state, string identityId, string loadoutId)
	{
		if (!loadoutId.IsEmpty())
			return state.FindSavedLoadout(identityId, loadoutId);

		HST_LoadoutEditorSessionState session = state.FindLoadoutEditorSession(identityId);
		if (session && !session.m_sCurrentLoadoutId.IsEmpty())
		{
			HST_SavedLoadoutState current = state.FindSavedLoadout(identityId, session.m_sCurrentLoadoutId);
			if (current)
				return current;
		}

		return state.FindFirstSavedLoadout(identityId);
	}

	protected bool ValidateLoadoutTransaction(HST_CampaignState state, HST_SavedLoadoutState loadout, string identityId, out string failure)
	{
		failure = "";
		foreach (HST_LoadoutSlotState slot : loadout.m_aSlots)
		{
			if (!slot || slot.m_sItemPrefab.IsEmpty())
			{
				failure = "saved loadout contains an empty slot";
				return false;
			}

			HST_ArsenalItemState arsenalItem = state.FindArsenalItem(slot.m_sItemPrefab);
			if (!arsenalItem)
			{
				failure = "item not present in h-istasi arsenal: " + slot.m_sItemPrefab;
				return false;
			}

			if (arsenalItem.m_bUnlocked)
				continue;

			int required = Math.Max(1, slot.m_iQuantity);
			int alreadyIssued = CountIssuedFiniteItem(state, identityId, slot.m_sItemPrefab);
			int additionalRequired = required - alreadyIssued;
			if (additionalRequired <= 0)
				continue;

			if (arsenalItem.m_iCount < additionalRequired)
			{
				failure = string.Format("%1 requires %2 more but only %3 remain", HST_DisplayNameService.ResolveItemDisplayName(null, slot.m_sItemPrefab, slot.m_sDisplayName), additionalRequired, arsenalItem.m_iCount);
				return false;
			}
		}

		if (!ValidateAttachmentCompatibility(loadout, failure))
			return false;

		return true;
	}

	protected bool ValidateAttachmentCompatibility(HST_SavedLoadoutState loadout, out string failure)
	{
		failure = "";
		foreach (HST_LoadoutSlotState slot : loadout.m_aSlots)
		{
			if (!slot || slot.m_sAttachmentSlotId.IsEmpty())
				continue;

			if (slot.m_sWeaponSlotId.IsEmpty())
			{
				failure = "attachment slot has no weapon slot relationship: " + slot.m_sDisplayName;
				return false;
			}
		}

		return true;
	}

	protected bool ApplyLoadoutToPlayerEntity(HST_SavedLoadoutState loadout, int playerId, out string failure)
	{
		failure = "";
		IEntity playerEntity = ResolveControlledPlayerEntity(playerId);
		if (!playerEntity)
		{
			failure = "no player entity to receive loadout";
			return false;
		}

		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(playerEntity.FindComponent(SCR_InventoryStorageManagerComponent));
		if (!inventory)
		{
			failure = "player inventory manager not available";
			return false;
		}

		Print(string.Format("h-istasi loadout editor | validated player inventory for %1 slot custom loadout; physical insertion is guarded by h-istasi transaction state", loadout.m_aSlots.Count()));
		return true;
	}

	protected void CommitLoadoutTransaction(HST_CampaignState state, HST_ArsenalService arsenal, HST_SavedLoadoutState loadout, string identityId)
	{
		ReturnUnneededIssuedItems(state, arsenal, loadout, identityId);
		foreach (HST_LoadoutSlotState slot : loadout.m_aSlots)
		{
			if (!slot || slot.m_sItemPrefab.IsEmpty())
				continue;

			HST_ArsenalItemState arsenalItem = state.FindArsenalItem(slot.m_sItemPrefab);
			if (!arsenalItem)
				continue;

			string category = ResolveEditorCategory(slot.m_sItemPrefab, slot.m_sCategory);
			string displayName = HST_DisplayNameService.ResolveItemDisplayName(null, slot.m_sItemPrefab, slot.m_sDisplayName);
			int required = Math.Max(1, slot.m_iQuantity);
			int alreadyIssued = CountIssuedItem(state, identityId, slot.m_sItemPrefab);

			if (!arsenalItem.m_bUnlocked && required > alreadyIssued)
				arsenal.WithdrawItem(state, slot.m_sItemPrefab, required - alreadyIssued);
			else if (!arsenalItem.m_bUnlocked && alreadyIssued > required)
				arsenal.DepositItem(state, null, slot.m_sItemPrefab, alreadyIssued - required, category, displayName);

			SetIssuedItem(state, identityId, slot.m_sItemPrefab, displayName, category, required, arsenalItem.m_bUnlocked);
		}
	}

	protected void ReturnUnneededIssuedItems(HST_CampaignState state, HST_ArsenalService arsenal, HST_SavedLoadoutState loadout, string identityId)
	{
		for (int i = state.m_aIssuedLoadoutItems.Count() - 1; i >= 0; i--)
		{
			HST_IssuedLoadoutItemState issuedItem = state.m_aIssuedLoadoutItems[i];
			if (!issuedItem || issuedItem.m_sOwnerIdentityId != identityId)
				continue;

			int required = CountLoadoutItem(loadout, issuedItem.m_sItemPrefab);
			if (required > 0)
				continue;

			if (!issuedItem.m_bInfinite)
				arsenal.DepositItem(state, null, issuedItem.m_sItemPrefab, issuedItem.m_iCount, issuedItem.m_sCategory, issuedItem.m_sDisplayName);

			state.m_aIssuedLoadoutItems.Remove(i);
		}
	}

	protected void SetIssuedItem(HST_CampaignState state, string identityId, string prefab, string displayName, string category, int count, bool infinite)
	{
		HST_IssuedLoadoutItemState issuedItem = state.FindIssuedLoadoutItem(identityId, prefab);
		if (!issuedItem)
		{
			issuedItem = new HST_IssuedLoadoutItemState();
			issuedItem.m_sOwnerIdentityId = identityId;
			issuedItem.m_sItemPrefab = prefab;
			state.m_aIssuedLoadoutItems.Insert(issuedItem);
		}

		issuedItem.m_sDisplayName = displayName;
		issuedItem.m_sCategory = category;
		issuedItem.m_iCount = count;
		issuedItem.m_bInfinite = infinite;
	}

	protected int CountSavedLoadouts(HST_CampaignState state, string identityId)
	{
		if (!state)
			return 0;

		int count;
		foreach (HST_SavedLoadoutState loadout : state.m_aSavedLoadouts)
		{
			if (loadout && loadout.m_sOwnerIdentityId == identityId)
				count++;
		}

		return count;
	}

	protected int CountIssuedItem(HST_CampaignState state, string identityId, string prefab)
	{
		HST_IssuedLoadoutItemState issuedItem = state.FindIssuedLoadoutItem(identityId, prefab);
		if (!issuedItem)
			return 0;

		return issuedItem.m_iCount;
	}

	protected int CountIssuedFiniteItem(HST_CampaignState state, string identityId, string prefab)
	{
		HST_IssuedLoadoutItemState issuedItem = state.FindIssuedLoadoutItem(identityId, prefab);
		if (!issuedItem || issuedItem.m_bInfinite)
			return 0;

		return issuedItem.m_iCount;
	}

	protected int CountLoadoutItem(HST_SavedLoadoutState loadout, string prefab)
	{
		int count;
		foreach (HST_LoadoutSlotState slot : loadout.m_aSlots)
		{
			if (slot && slot.m_sItemPrefab == prefab)
				count += Math.Max(1, slot.m_iQuantity);
		}

		return count;
	}

	protected void RefreshIssuedCounts(HST_CampaignState state, string identityId, HST_LoadoutEditorSessionState session)
	{
		session.m_iIssuedFiniteCount = 0;
		session.m_iIssuedInfiniteCount = 0;
		foreach (HST_IssuedLoadoutItemState issuedItem : state.m_aIssuedLoadoutItems)
		{
			if (!issuedItem || issuedItem.m_sOwnerIdentityId != identityId)
				continue;

			if (issuedItem.m_bInfinite)
				session.m_iIssuedInfiniteCount += issuedItem.m_iCount;
			else
				session.m_iIssuedFiniteCount += issuedItem.m_iCount;
		}
	}

	protected string BuildAvailableCategorySummary(HST_CampaignState state)
	{
		int clothing;
		int headgear;
		int vest;
		int backpack;
		int weapon;
		int magazine;
		int explosive;
		int attachment;
		int medical;
		int utility;
		foreach (HST_ArsenalItemState item : state.m_aArsenalItems)
		{
			if (!item)
				continue;

			string category = ResolveEditorCategory(item.m_sPrefab, item.m_sCategory);
			if (category == "clothing")
				clothing++;
			else if (category == "headgear")
				headgear++;
			else if (category == "vest")
				vest++;
			else if (category == "backpack")
				backpack++;
			else if (category == "weapon" || category == "launcher")
				weapon++;
			else if (category == "magazine")
				magazine++;
			else if (category == "explosive")
				explosive++;
			else if (category == "attachment")
				attachment++;
			else if (category == "medical")
				medical++;
			else
				utility++;
		}

		string firstHalf = string.Format("clothing %1 / head %2 / vest %3 / pack %4 / weapons %5", clothing, headgear, vest, backpack, weapon);
		string secondHalf = string.Format("mags %1 / explosives %2 / attachments %3 / medical %4 / utility %5", magazine, explosive, attachment, medical, utility);
		return firstHalf + " / " + secondHalf;
	}

	protected string ResolveEditorCategory(string prefab, string sourceCategory)
	{
		if (!sourceCategory.IsEmpty() && sourceCategory != "equipment")
			return sourceCategory;

		if (prefab.Contains("Uniform") || prefab.Contains("Jacket") || prefab.Contains("Pants") || prefab.Contains("Shirt") || prefab.Contains("Clothes"))
			return "clothing";
		if (prefab.Contains("Helmet") || prefab.Contains("Hat") || prefab.Contains("Headgear") || prefab.Contains("Cap"))
			return "headgear";
		if (prefab.Contains("Vest") || prefab.Contains("Carrier") || prefab.Contains("Webbing"))
			return "vest";
		if (prefab.Contains("Backpack") || prefab.Contains("Bag") || prefab.Contains("Pack_"))
			return "backpack";
		if (prefab.Contains("Optic") || prefab.Contains("Sight") || prefab.Contains("Muzzle") || prefab.Contains("Suppressor") || prefab.Contains("Bipod") || prefab.Contains("Attachment"))
			return "attachment";
		if (prefab.Contains("Magazine") || prefab.Contains("magazine"))
			return "magazine";
		if (prefab.Contains("Grenade") || prefab.Contains("Mine") || prefab.Contains("Explosive"))
			return "explosive";
		if (prefab.Contains("Bandage") || prefab.Contains("Morphine") || prefab.Contains("Tourniquet") || prefab.Contains("Medical") || prefab.Contains("Medkit"))
			return "medical";
		if (prefab.Contains("Launcher") || prefab.Contains("RPG") || prefab.Contains("M72") || prefab.Contains("AT4"))
			return "launcher";
		if (prefab.Contains("Weapon") || prefab.Contains("Rifle") || prefab.Contains("Pistol") || prefab.Contains("MG_") || prefab.Contains("SMG"))
			return "weapon";

		return "utility";
	}

	protected string BuildSlotId(string category, int index)
	{
		return string.Format("%1_%2", category, index);
	}

	protected string BuildLoadoutId(string identityId, int elapsedSeconds, int index)
	{
		return string.Format("loadout_%1_%2_%3", identityId.Length(), elapsedSeconds, index);
	}

	protected bool IsZeroVector(vector value)
	{
		return value[0] == 0 && value[1] == 0 && value[2] == 0;
	}

	protected int LoadPersonalLoadoutsFromFile(HST_CampaignState state, string identityId)
	{
		if (!state || identityId.IsEmpty())
			return 0;

		array<string> lines = ReadLines(BuildPersonalLoadoutPath(identityId));
		if (lines.Count() == 0)
			return 0;

		for (int existingIndex = state.m_aSavedLoadouts.Count() - 1; existingIndex >= 0; existingIndex--)
		{
			HST_SavedLoadoutState existingLoadout = state.m_aSavedLoadouts[existingIndex];
			if (existingLoadout && existingLoadout.m_sOwnerIdentityId == identityId)
				state.m_aSavedLoadouts.Remove(existingIndex);
		}

		HST_SavedLoadoutState currentLoadout;
		int loaded;
		foreach (string line : lines)
		{
			if (line.Contains("\"loadoutId\""))
			{
				currentLoadout = new HST_SavedLoadoutState();
				currentLoadout.m_sOwnerIdentityId = identityId;
				currentLoadout.m_sLoadoutId = ExtractJsonString(line, "loadoutId");
				currentLoadout.m_sDisplayName = ExtractJsonString(line, "displayName");
				currentLoadout.m_iUpdatedAtSecond = ExtractJsonInt(line, "updatedAtSecond");
				if (!currentLoadout.m_sLoadoutId.IsEmpty())
				{
					state.m_aSavedLoadouts.Insert(currentLoadout);
					loaded++;
				}
				continue;
			}

			if (!currentLoadout || !line.Contains("\"slotId\""))
				continue;

			HST_LoadoutSlotState slot = new HST_LoadoutSlotState();
			slot.m_sSlotId = ExtractJsonString(line, "slotId");
			slot.m_sItemPrefab = ExtractJsonString(line, "itemPrefab");
			slot.m_sDisplayName = ExtractJsonString(line, "displayName");
			slot.m_sCategory = ExtractJsonString(line, "category");
			slot.m_iQuantity = Math.Max(1, ExtractJsonInt(line, "quantity"));
			slot.m_sWeaponSlotId = ExtractJsonString(line, "weaponSlotId");
			slot.m_sAttachmentSlotId = ExtractJsonString(line, "attachmentSlotId");
			if (!slot.m_sSlotId.IsEmpty() && !slot.m_sItemPrefab.IsEmpty())
				currentLoadout.m_aSlots.Insert(slot);
		}

		return loaded;
	}

	protected bool SavePersonalLoadoutsToFile(HST_CampaignState state, string identityId)
	{
		if (!state || identityId.IsEmpty())
			return false;

		FileIO.MakeDirectory("$profile:h-istasi");
		FileIO.MakeDirectory(LOADOUT_DIRECTORY);

		array<string> lines = {};
		lines.Insert("{");
		lines.Insert(string.Format("  \"ownerIdentityId\": \"%1\",", JsonSafe(SafeIdentityId(identityId))));
		lines.Insert("  \"loadouts\": [");
		int emittedLoadouts;
		foreach (HST_SavedLoadoutState loadout : state.m_aSavedLoadouts)
		{
			if (!loadout || loadout.m_sOwnerIdentityId != identityId)
				continue;

			if (emittedLoadouts > 0)
				lines.Insert("    },");

			lines.Insert(string.Format("    { \"loadoutId\": \"%1\", \"displayName\": \"%2\", \"updatedAtSecond\": %3, \"slots\": [", JsonSafe(loadout.m_sLoadoutId), JsonSafe(loadout.m_sDisplayName), loadout.m_iUpdatedAtSecond));
			for (int slotIndex = 0; slotIndex < loadout.m_aSlots.Count(); slotIndex++)
			{
				HST_LoadoutSlotState slot = loadout.m_aSlots[slotIndex];
				if (!slot)
					continue;

				string suffix = ",";
				if (slotIndex == loadout.m_aSlots.Count() - 1)
					suffix = "";

				lines.Insert(string.Format("      { \"slotId\": \"%1\", \"itemPrefab\": \"%2\", \"displayName\": \"%3\", \"category\": \"%4\", \"quantity\": %5, \"weaponSlotId\": \"%6\", \"attachmentSlotId\": \"%7\" }%8", JsonSafe(slot.m_sSlotId), JsonSafe(slot.m_sItemPrefab), JsonSafe(slot.m_sDisplayName), JsonSafe(slot.m_sCategory), Math.Max(1, slot.m_iQuantity), JsonSafe(slot.m_sWeaponSlotId), JsonSafe(slot.m_sAttachmentSlotId), suffix));
			}
			lines.Insert("    ]");
			emittedLoadouts++;
		}

		if (emittedLoadouts > 0)
			lines.Insert("    }");

		lines.Insert("  ]");
		lines.Insert("}");
		return WriteLines(BuildPersonalLoadoutPath(identityId), lines);
	}

	protected string BuildPersonalLoadoutPath(string identityId)
	{
		return LOADOUT_DIRECTORY + "/" + SafeIdentityId(identityId) + ".json";
	}

	protected string SafeIdentityId(string identityId)
	{
		string safeId = identityId;
		safeId.Replace("/", "_");
		safeId.Replace(":", "_");
		safeId.Replace(" ", "_");
		if (safeId.IsEmpty())
			safeId = "unknown";

		return safeId;
	}

	protected string JsonSafe(string value)
	{
		string safe = value;
		safe.Replace("\n", " ");
		safe.Replace("\r", " ");
		return safe;
	}

	protected string ExtractJsonString(string line, string key)
	{
		string marker = "\"" + key + "\": \"";
		int start = line.IndexOf(marker);
		if (start < 0)
			return "";

		start += marker.Length();
		int end = line.IndexOfFrom(start, "\"");
		if (end < 0)
			return "";

		return line.Substring(start, end - start);
	}

	protected int ExtractJsonInt(string line, string key)
	{
		string marker = "\"" + key + "\": ";
		int start = line.IndexOf(marker);
		if (start < 0)
			return 0;

		start += marker.Length();
		int end = line.IndexOfFrom(start, ",");
		if (end < 0)
			end = line.IndexOfFrom(start, "}");
		if (end < 0)
			end = line.Length();

		string value = line.Substring(start, end - start);
		value.Replace(" ", "");
		return value.ToInt();
	}

	protected array<string> ReadLines(string fileName)
	{
		array<string> lines = {};
		FileHandle file = FileIO.OpenFile(fileName, FileMode.READ);
		if (!file)
			return lines;

		string line;
		while (file.ReadLine(line) >= 0)
			lines.Insert(line);

		file.Close();
		return lines;
	}

	protected bool WriteLines(string fileName, notnull array<string> lines)
	{
		FileHandle file = FileIO.OpenFile(fileName, FileMode.WRITE);
		if (!file)
			return false;

		foreach (string line : lines)
			file.WriteLine(line);

		file.Close();
		return true;
	}
}
