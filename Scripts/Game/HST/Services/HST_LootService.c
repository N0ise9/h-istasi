class HST_LootResult
{
	int m_iSourcesScanned;
	int m_iItemsSeen;
	int m_iItemsDeposited;
	int m_iItemsRemoved;
	int m_iItemsSkippedUnlocked;
	int m_iItemsSkippedBlocked;
	int m_iItemsSkippedFriendly;
	string m_sVehicleRuntimeId;
	string m_sVehicleDisplayName;
	ref array<string> m_aDepositedLines = {};

	string BuildSummary()
	{
		string target = "arsenal";
		if (!m_sVehicleRuntimeId.IsEmpty())
			target = "vehicle " + m_sVehicleDisplayName;

		string summary = string.Format("h-istasi loot | target %1 | scanned %2 source(s) | seen %3 item(s) | deposited %4 | removed %5", target, m_iSourcesScanned, m_iItemsSeen, m_iItemsDeposited, m_iItemsRemoved);
		summary = summary + string.Format(" | skipped unlocked %1 | blocked %2 | friendly/player %3", m_iItemsSkippedUnlocked, m_iItemsSkippedBlocked, m_iItemsSkippedFriendly);
		foreach (string line : m_aDepositedLines)
			summary = summary + "\n" + line;

		return summary;
	}
}

class HST_VehicleRootScanResult
{
	IEntity m_SelectedRoot;
	int m_iCandidates;
	int m_iResolvedRoots;
	string m_sSelectedPrefab;
	string m_sRejectReason = "not scanned";
	float m_fSelectedDistanceMeters;
	int m_iCargoEntries;

	bool HasTarget()
	{
		return m_SelectedRoot != null;
	}
}

class HST_LootService
{
	static const int GARAGE_CAPTURE_RADIUS_METERS = 24;
	protected ref array<IEntity> m_aScanEntities = {};

	HST_LootResult LootNearbyToArsenal(HST_CampaignState state, HST_CampaignPreset preset, HST_BalanceConfig balance, HST_ArsenalService arsenal, int playerId)
	{
		HST_LootResult result = new HST_LootResult();
		if (!state || !preset || !balance || !arsenal || playerId <= 0)
			return result;

		IEntity playerEntity = ResolveLivingPlayerEntity(playerId);
		if (!playerEntity)
		{
			result.m_aDepositedLines.Insert("no living player entity found for area loot");
			return result;
		}

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return result;

		m_aScanEntities.Clear();
		world.QueryEntitiesBySphere(playerEntity.GetOrigin(), balance.m_iLootRadiusMeters, AddLootCandidate, null, EQueryEntitiesFlags.ALL);
		foreach (IEntity source : m_aScanEntities)
		{
			if (!source || source == playerEntity)
				continue;

			SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(source.FindComponent(SCR_InventoryStorageManagerComponent));
			if (inventory)
			{
				if (!IsEligibleLootSource(source, preset, result))
					continue;

				result.m_iSourcesScanned++;
				CollectInventoryItems(source, inventory, state, balance, arsenal, result);
				continue;
			}

			if (!IsEligibleLooseLootEntity(source, playerEntity, null))
				continue;

			result.m_iSourcesScanned++;
			CollectLooseItemToArsenal(source, state, balance, arsenal, result);
		}

		if (result.m_iItemsDeposited == 0 && result.m_aDepositedLines.Count() == 0)
			result.m_aDepositedLines.Insert("no eligible loot found nearby");

		return result;
	}

	string CaptureNearbyVehicleToGarage(HST_CampaignState state, HST_CampaignPreset preset, HST_ArsenalService arsenal, int playerId)
	{
		if (!state || !preset || !arsenal || playerId <= 0)
			return "h-istasi garage | failed: service not ready";

		IEntity playerEntity = ResolveLivingPlayerEntity(playerId);
		if (!playerEntity)
			return "h-istasi garage | failed: no living player entity found";

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return "h-istasi garage | failed: world not ready";

		HST_VehicleRootScanResult scan = FindNearestVehicleRoot(playerEntity, GARAGE_CAPTURE_RADIUS_METERS, state, "garage capture");
		IEntity selectedVehicle = scan.m_SelectedRoot;

		if (!selectedVehicle)
			return string.Format("h-istasi garage | failed: no safe root vehicle nearby | candidates %1 | reason %2", scan.m_iCandidates, scan.m_sRejectReason);

		HST_GarageVehicleState vehicle = new HST_GarageVehicleState();
		vehicle.m_sVehicleId = string.Format("garage_%1_%2", state.m_iElapsedSeconds, state.m_aGarageVehicles.Count());
		vehicle.m_sPrefab = ResolvePrefabName(selectedVehicle);
		if (vehicle.m_sPrefab.IsEmpty() || !IsEligibleVehicleRoot(selectedVehicle, vehicle.m_sPrefab))
			return "h-istasi garage | failed: nearest candidate was not a top-level vehicle";

		string selectedVehicleRuntimeId = ResolveVehicleRuntimeId(selectedVehicle);
		vehicle.m_sDisplayName = BuildVehicleDisplayName(selectedVehicle, vehicle.m_sPrefab);
		vehicle.m_sSourceZoneId = FindNearestZoneId(state, selectedVehicle.GetOrigin());
		vehicle.m_sSourceFactionKey = ResolveFactionKey(selectedVehicle);
		vehicle.m_iStoredAtSecond = state.m_iElapsedSeconds;
		vehicle.m_iRedeployCost = ResolveGarageRedeployCost(vehicle.m_sPrefab);
		vehicle.m_vPosition = selectedVehicle.GetOrigin();
		vehicle.m_vAngles = ResolveStoredVehicleAngles(selectedVehicle, vehicle.m_sPrefab);
		vehicle.m_fFuel = 1.0;
		vehicle.m_bArmed = IsLikelyArmedVehicle(vehicle.m_sPrefab);
		if (!arsenal.StoreVehicle(state, vehicle))
			return "h-istasi garage | failed: vehicle could not be stored";

		SCR_EntityHelper.DeleteEntityAndChildren(selectedVehicle);
		if (IsVehicleRootStillPresent(selectedVehicleRuntimeId, vehicle.m_vPosition))
		{
			arsenal.RemoveVehicle(state, vehicle.m_sVehicleId);
			state.m_sLastVehicleTargetStatus = "garage capture failed";
			state.m_sLastVehicleTargetReason = "selected root still present after delete";
			Print(string.Format("h-istasi garage | failed to despawn %1 after storing; removed pending garage record %2 | prefab %3", vehicle.m_sDisplayName, vehicle.m_sVehicleId, vehicle.m_sPrefab), LogLevel.WARNING);
			return string.Format("h-istasi garage | failed: %1 did not despawn, garage record was not kept", vehicle.m_sDisplayName);
		}

		state.m_sLastVehicleTargetStatus = string.Format("garage captured %1", vehicle.m_sDisplayName);
		state.m_sLastVehicleTargetReason = "stored then deleted verified root vehicle";
		Print(string.Format("h-istasi garage | captured %1 into %2 and despawned verified root vehicle | prefab %3 | distance %4m", vehicle.m_sDisplayName, vehicle.m_sVehicleId, vehicle.m_sPrefab, scan.m_fSelectedDistanceMeters));
		return string.Format("h-istasi garage | captured %1 | complete", vehicle.m_sDisplayName);
	}

	HST_LootResult CollectNearbyLootToVehicle(HST_CampaignState state, HST_CampaignPreset preset, HST_BalanceConfig balance, HST_ArsenalService arsenal, int playerId, string vehicleRuntimeId = "")
	{
		HST_LootResult result = new HST_LootResult();
		if (!state || !preset || !balance || !arsenal || playerId <= 0)
			return result;

		if (!balance.m_bVehicleLootEnabled)
		{
			result.m_aDepositedLines.Insert("vehicle loot disabled by config");
			return result;
		}

		IEntity playerEntity = ResolveLivingPlayerEntity(playerId);
		if (!playerEntity)
		{
			result.m_aDepositedLines.Insert("no living player entity found for vehicle loot");
			return result;
		}

		IEntity vehicle;
		if (!vehicleRuntimeId.IsEmpty())
			vehicle = FindLootVehicleByRuntimeId(playerEntity, balance.m_iVehicleLootRadiusMeters, vehicleRuntimeId, state);
		else
			vehicle = FindNearestLootVehicle(playerEntity, balance.m_iVehicleLootRadiusMeters, state);

		if (!vehicle)
		{
			if (!vehicleRuntimeId.IsEmpty())
				result.m_aDepositedLines.Insert(string.Format("target vehicle not nearby or invalid | %1", state.m_sLastVehicleTargetReason));
			else
				result.m_aDepositedLines.Insert(string.Format("no eligible vehicle nearby | candidates %1 | reason %2", state.m_iLastVehicleTargetCandidates, state.m_sLastVehicleTargetReason));
			return result;
		}

		if (!IsPlayerAtVehicleRear(playerEntity, vehicle, balance.m_iVehicleLootRadiusMeters))
		{
			result.m_aDepositedLines.Insert("stand near the rear/load area of the vehicle to load loot");
			return result;
		}

		string vehicleId = ResolveVehicleRuntimeId(vehicle);
		string vehiclePrefab = ResolvePrefabName(vehicle);
		string vehicleName = BuildVehicleDisplayName(vehicle, vehiclePrefab);
		result.m_sVehicleRuntimeId = vehicleId;
		result.m_sVehicleDisplayName = vehicleName;
		state.m_sLastVehicleTargetStatus = string.Format("vehicle loot target %1", vehicleName);
		state.m_sLastVehicleTargetPrefab = vehiclePrefab;
		state.m_sLastVehicleTargetReason = "target selected; scanning nearby loot";

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return result;

		m_aScanEntities.Clear();
		world.QueryEntitiesBySphere(vehicle.GetOrigin(), balance.m_iVehicleLootRadiusMeters, AddLootCandidate, null, EQueryEntitiesFlags.ALL);
		foreach (IEntity source : m_aScanEntities)
		{
			if (!source || source == playerEntity || source == vehicle)
				continue;

			if (balance.m_iVehicleLootMaxItemsPerAction > 0 && result.m_iItemsDeposited >= balance.m_iVehicleLootMaxItemsPerAction)
				break;

			SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(source.FindComponent(SCR_InventoryStorageManagerComponent));
			if (inventory)
			{
				if (!IsEligibleLootSource(source, preset, result))
					continue;

				result.m_iSourcesScanned++;
				CollectInventoryItemsToVehicle(source, inventory, vehicle, vehicleId, vehiclePrefab, vehicleName, state, balance, arsenal, result);
				continue;
			}

			if (!IsEligibleLooseLootEntity(source, playerEntity, vehicle))
				continue;

			result.m_iSourcesScanned++;
			CollectLooseItemToVehicle(source, vehicle, vehicleId, vehiclePrefab, vehicleName, state, balance, arsenal, result);
		}

		if (result.m_iItemsDeposited == 0 && result.m_aDepositedLines.Count() == 0)
			result.m_aDepositedLines.Insert("no eligible loot found near vehicle");

		state.m_iLastVehicleTargetCargoEntries = CountVehicleCargoEntries(state, vehicleId);
		Print(string.Format("h-istasi vehicle loot | target %1 | prefab %2 | scanned %3 | deposited %4 | cargo entries for vehicle %5", vehicleName, vehiclePrefab, result.m_iSourcesScanned, result.m_iItemsDeposited, state.m_iLastVehicleTargetCargoEntries));

		return result;
	}

	string UnloadNearestVehicleCargoToArsenal(HST_CampaignState state, HST_CampaignPreset preset, HST_BalanceConfig balance, HST_ArsenalService arsenal, int playerId, string vehicleRuntimeId = "")
	{
		if (!state || !preset || !balance || !arsenal || playerId <= 0)
			return "h-istasi vehicle loot | service not ready";

		IEntity playerEntity = ResolveLivingPlayerEntity(playerId);
		if (!playerEntity)
			return "h-istasi vehicle loot | no living player entity found";

		IEntity vehicle;
		if (!vehicleRuntimeId.IsEmpty())
			vehicle = FindLootVehicleByRuntimeId(playerEntity, balance.m_iVehicleLootRadiusMeters, vehicleRuntimeId, state);
		else
			vehicle = FindNearestLootVehicle(playerEntity, balance.m_iVehicleLootRadiusMeters, state);

		if (!vehicle)
		{
			if (!vehicleRuntimeId.IsEmpty())
				return string.Format("h-istasi vehicle loot | target vehicle not nearby or invalid | %1", state.m_sLastVehicleTargetReason);

			return string.Format("h-istasi vehicle loot | no eligible vehicle nearby | candidates %1 | reason %2", state.m_iLastVehicleTargetCandidates, state.m_sLastVehicleTargetReason);
		}

		if (!IsPlayerAtVehicleRear(playerEntity, vehicle, balance.m_iVehicleLootRadiusMeters))
			return "h-istasi vehicle loot | stand near the rear/load area of the vehicle to unload cargo";

		string vehicleId = ResolveVehicleRuntimeId(vehicle);
		string vehiclePrefab = ResolvePrefabName(vehicle);
		string vehicleName = BuildVehicleDisplayName(vehicle, vehiclePrefab);
		int recoveredLegacyEntries = RekeyLegacyVehiclePartCargo(state, vehicle, vehicleId, vehiclePrefab, vehicleName);
		int deposited;
		int entries;
		for (int i = state.m_aVehicleCargoItems.Count() - 1; i >= 0; i--)
		{
			HST_VehicleCargoItemState cargoItem = state.m_aVehicleCargoItems[i];
			if (!cargoItem || cargoItem.m_sVehicleRuntimeId != vehicleId)
				continue;

			string cargoDisplayName = HST_DisplayNameService.ResolveItemDisplayName(null, cargoItem.m_sItemPrefab, cargoItem.m_sDisplayName);
			HST_ArsenalItemState depositedItem = arsenal.DepositItem(state, balance, cargoItem.m_sItemPrefab, cargoItem.m_iCount, cargoItem.m_sCategory, cargoDisplayName);
			if (depositedItem)
			{
				deposited += cargoItem.m_iCount;
				entries++;
				state.m_aVehicleCargoItems.Remove(i);
			}
		}

		if (deposited <= 0)
			return string.Format("h-istasi vehicle loot | no stored cargo found in nearest vehicle | recovered legacy entries %1", recoveredLegacyEntries);

		state.m_iLastVehicleTargetCargoEntries = CountVehicleCargoEntriesIncludingLegacy(state, vehicle);
		Print(string.Format("h-istasi vehicle loot | unloaded %1 item(s) from %2 cargo entries to arsenal | target %3 | prefab %4 | recovered legacy entries %5 | remaining cargo entries %6", deposited, entries, vehicleName, vehiclePrefab, recoveredLegacyEntries, state.m_iLastVehicleTargetCargoEntries));
		return string.Format("h-istasi vehicle loot | unloaded %1 item(s) from %2 cargo entries to arsenal | recovered legacy entries %3", deposited, entries, recoveredLegacyEntries);
	}

	string BuildVehicleCargoReport(HST_CampaignState state)
	{
		if (!state)
			return "h-istasi vehicle cargo | campaign state not ready";

		string report = string.Format("h-istasi vehicle cargo | entries %1", state.m_aVehicleCargoItems.Count());
		foreach (HST_VehicleCargoItemState cargoItem : state.m_aVehicleCargoItems)
		{
			if (!cargoItem)
				continue;

			string vehicleLabel = HST_DisplayNameService.ResolveVehicleDisplayName(cargoItem.m_sVehiclePrefab, cargoItem.m_sVehicleDisplayName);
			string itemLabel = HST_DisplayNameService.ResolveItemDisplayName(null, cargoItem.m_sItemPrefab, cargoItem.m_sDisplayName);
			report = report + string.Format("\n%1 | %2 | %3 | count %4", vehicleLabel, itemLabel, cargoItem.m_sCategory, cargoItem.m_iCount);
		}

		return report;
	}

	protected int CountVehicleCargoEntries(HST_CampaignState state, string vehicleRuntimeId)
	{
		if (!state || vehicleRuntimeId.IsEmpty())
			return 0;

		int entries;
		foreach (HST_VehicleCargoItemState cargoItem : state.m_aVehicleCargoItems)
		{
			if (cargoItem && cargoItem.m_sVehicleRuntimeId == vehicleRuntimeId)
				entries++;
		}

		return entries;
	}

	protected int CountVehicleCargoEntriesIncludingLegacy(HST_CampaignState state, IEntity vehicle)
	{
		if (!state || !vehicle)
			return 0;

		string vehicleId = ResolveVehicleRuntimeId(vehicle);
		int entries = CountVehicleCargoEntries(state, vehicleId);
		entries += CountLegacyVehiclePartCargoNear(state, vehicle, vehicleId);
		return entries;
	}

	protected int CountLegacyVehiclePartCargoNear(HST_CampaignState state, IEntity vehicle, string vehicleId)
	{
		if (!state || !vehicle)
			return 0;

		int entries;
		foreach (HST_VehicleCargoItemState cargoItem : state.m_aVehicleCargoItems)
		{
			if (!cargoItem || cargoItem.m_sVehicleRuntimeId == vehicleId)
				continue;

			if (!IsLegacyVehiclePartCargoNear(cargoItem, vehicle))
				continue;

			entries++;
		}

		return entries;
	}

	protected int RekeyLegacyVehiclePartCargo(HST_CampaignState state, IEntity vehicle, string vehicleId, string vehiclePrefab, string vehicleName)
	{
		if (!state || !vehicle || vehicleId.IsEmpty())
			return 0;

		int recoveredEntries;
		for (int i = state.m_aVehicleCargoItems.Count() - 1; i >= 0; i--)
		{
			HST_VehicleCargoItemState cargoItem = state.m_aVehicleCargoItems[i];
			if (!cargoItem || cargoItem.m_sVehicleRuntimeId == vehicleId)
				continue;

			if (!IsLegacyVehiclePartCargoNear(cargoItem, vehicle))
				continue;

			HST_VehicleCargoItemState existing = state.FindVehicleCargoItem(vehicleId, cargoItem.m_sItemPrefab);
			if (existing && existing != cargoItem)
			{
				existing.m_iCount += cargoItem.m_iCount;
				existing.m_iLastStoredAtSecond = Math.Max(existing.m_iLastStoredAtSecond, cargoItem.m_iLastStoredAtSecond);
				existing.m_vLastVehiclePosition = vehicle.GetOrigin();
				if (existing.m_sDisplayName.IsEmpty())
					existing.m_sDisplayName = cargoItem.m_sDisplayName;
				if (existing.m_sCategory.IsEmpty())
					existing.m_sCategory = cargoItem.m_sCategory;
				state.m_aVehicleCargoItems.Remove(i);
			}
			else
			{
				cargoItem.m_sVehicleRuntimeId = vehicleId;
				cargoItem.m_sVehiclePrefab = vehiclePrefab;
				cargoItem.m_sVehicleDisplayName = HST_DisplayNameService.ResolveVehicleDisplayName(vehiclePrefab, vehicleName);
				cargoItem.m_vLastVehiclePosition = vehicle.GetOrigin();
			}

			recoveredEntries++;
		}

		if (recoveredEntries > 0)
			Print(string.Format("h-istasi vehicle loot | rekeyed %1 legacy vehicle-part cargo entries to root %2 | prefab %3", recoveredEntries, vehicleId, vehiclePrefab));

		return recoveredEntries;
	}

	protected bool IsLegacyVehiclePartCargoNear(HST_VehicleCargoItemState cargoItem, IEntity vehicle)
	{
		if (!cargoItem || !vehicle)
			return false;

		if (!HST_VehicleRootPolicy.IsVehiclePartPrefab(cargoItem.m_sVehiclePrefab) && !HST_VehicleRootPolicy.IsVehiclePartName(cargoItem.m_sVehicleDisplayName))
			return false;

		vector cargoPosition = cargoItem.m_vLastVehiclePosition;
		if (IsZeroVector(cargoPosition))
			return false;

		return DistanceSq2D(cargoPosition, vehicle.GetOrigin()) <= 12 * 12;
	}

	protected bool IsZeroVector(vector value)
	{
		return value[0] == 0 && value[1] == 0 && value[2] == 0;
	}

	protected bool IsVehicleRootStillPresent(string vehicleRuntimeId, vector position)
	{
		if (vehicleRuntimeId.IsEmpty())
			return false;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		m_aScanEntities.Clear();
		world.QueryEntitiesBySphere(position, 8, AddLootCandidate, null, EQueryEntitiesFlags.ALL);
		foreach (IEntity candidate : m_aScanEntities)
		{
			string rejectReason;
			IEntity rootVehicle = ResolveVehicleRoot(candidate, rejectReason);
			if (!rootVehicle)
				continue;

			if (ResolveVehicleRuntimeId(rootVehicle) == vehicleRuntimeId)
				return true;
		}

		return false;
	}

	protected bool AddLootCandidate(IEntity entity)
	{
		if (entity)
			m_aScanEntities.Insert(entity);

		return true;
	}

	protected IEntity ResolveLivingPlayerEntity(int playerId)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return null;

		IEntity playerEntity = playerManager.GetPlayerControlledEntity(playerId);
		if (!playerEntity)
			playerEntity = SCR_PossessingManagerComponent.GetPlayerMainEntity(playerId);

		if (!IsLivingEntity(playerEntity))
			return null;

		return playerEntity;
	}

	protected bool IsEligibleLootSource(IEntity source, HST_CampaignPreset preset, HST_LootResult result)
	{
		if (!source)
			return false;

		FactionAffiliationComponent factionComponent = FactionAffiliationComponent.Cast(source.FindComponent(FactionAffiliationComponent));
		if (factionComponent)
		{
			string factionKey = factionComponent.GetAffiliatedFactionKey();
			if (factionKey == preset.m_sResistanceFactionKey || factionKey == "FIA")
			{
				result.m_iItemsSkippedFriendly++;
				return false;
			}

			if (IsLivingEntity(source))
				return false;
		}

		return true;
	}

	protected bool IsLivingEntity(IEntity entity)
	{
		if (!entity)
			return false;

		SCR_DamageManagerComponent damageManager = SCR_DamageManagerComponent.Cast(entity.FindComponent(SCR_DamageManagerComponent));
		return !damageManager || damageManager.GetState() != EDamageState.DESTROYED;
	}

	protected void CollectInventoryItems(IEntity source, SCR_InventoryStorageManagerComponent inventory, HST_CampaignState state, HST_BalanceConfig balance, HST_ArsenalService arsenal, HST_LootResult result)
	{
		array<IEntity> items = {};
		inventory.GetItems(items, EStoragePurpose.PURPOSE_ANY);
		foreach (IEntity item : items)
		{
			if (!item)
				continue;

			result.m_iItemsSeen++;
			string prefab = ResolvePrefabName(item);
			if (prefab.IsEmpty())
			{
				result.m_iItemsSkippedBlocked++;
				continue;
			}

			string category = ClassifyItem(item, prefab);
			if (!IsAllowedByLootRules(prefab, category, balance))
			{
				result.m_iItemsSkippedBlocked++;
				continue;
			}

			if (balance.m_bLootOnlyLockedItems && arsenal.IsItemUnlocked(state, prefab))
			{
				result.m_iItemsSkippedUnlocked++;
				continue;
			}

			HST_ArsenalItemState deposited = arsenal.DepositItem(state, balance, prefab, 1, category, BuildDisplayName(item, prefab));
			if (!deposited)
				continue;

			result.m_iItemsDeposited++;
			result.m_aDepositedLines.Insert(string.Format("+ %1 | %2 | count %3 | unlocked %4", deposited.m_sDisplayName, deposited.m_sCategory, deposited.m_iCount, deposited.m_bUnlocked));
			if (balance.m_bRemoveLootedItems && RemoveLootedItem(inventory, item))
				result.m_iItemsRemoved++;
		}
	}

	protected void CollectInventoryItemsToVehicle(IEntity source, SCR_InventoryStorageManagerComponent inventory, IEntity vehicle, string vehicleId, string vehiclePrefab, string vehicleName, HST_CampaignState state, HST_BalanceConfig balance, HST_ArsenalService arsenal, HST_LootResult result)
	{
		array<IEntity> items = {};
		inventory.GetItems(items, EStoragePurpose.PURPOSE_ANY);
		foreach (IEntity item : items)
		{
			if (!item)
				continue;

			if (balance.m_iVehicleLootMaxItemsPerAction > 0 && result.m_iItemsDeposited >= balance.m_iVehicleLootMaxItemsPerAction)
				return;

			result.m_iItemsSeen++;
			string prefab = ResolvePrefabName(item);
			if (prefab.IsEmpty())
			{
				result.m_iItemsSkippedBlocked++;
				continue;
			}

			string category = ClassifyItem(item, prefab);
			if (!IsAllowedByLootRules(prefab, category, balance))
			{
				result.m_iItemsSkippedBlocked++;
				continue;
			}

			if (balance.m_bVehicleLootOnlyLockedItems && arsenal.IsItemUnlocked(state, prefab))
			{
				result.m_iItemsSkippedUnlocked++;
				continue;
			}

			HST_VehicleCargoItemState cargoItem = DepositVehicleCargo(state, vehicle, vehicleId, vehiclePrefab, vehicleName, prefab, category, BuildDisplayName(item, prefab));
			if (!cargoItem)
				continue;

			result.m_iItemsDeposited++;
			result.m_aDepositedLines.Insert(string.Format("+ %1 | %2 | vehicle count %3", cargoItem.m_sDisplayName, cargoItem.m_sCategory, cargoItem.m_iCount));
			if (balance.m_bVehicleLootRemoveSource && RemoveLootedItem(inventory, item))
				result.m_iItemsRemoved++;
		}
	}

	protected void CollectLooseItemToArsenal(IEntity item, HST_CampaignState state, HST_BalanceConfig balance, HST_ArsenalService arsenal, HST_LootResult result)
	{
		result.m_iItemsSeen++;
		string prefab = ResolvePrefabName(item);
		if (prefab.IsEmpty())
		{
			result.m_iItemsSkippedBlocked++;
			return;
		}

		string category = ClassifyItem(item, prefab);
		if (!IsAllowedByLootRules(prefab, category, balance))
		{
			result.m_iItemsSkippedBlocked++;
			return;
		}

		if (balance.m_bLootOnlyLockedItems && arsenal.IsItemUnlocked(state, prefab))
		{
			result.m_iItemsSkippedUnlocked++;
			return;
		}

		HST_ArsenalItemState deposited = arsenal.DepositItem(state, balance, prefab, 1, category, BuildDisplayName(item, prefab));
		if (!deposited)
			return;

		result.m_iItemsDeposited++;
		result.m_aDepositedLines.Insert(string.Format("+ %1 | %2 | count %3 | unlocked %4", deposited.m_sDisplayName, deposited.m_sCategory, deposited.m_iCount, deposited.m_bUnlocked));
		if (balance.m_bRemoveLootedItems && RemoveLooseLootItem(item))
			result.m_iItemsRemoved++;
	}

	protected void CollectLooseItemToVehicle(IEntity item, IEntity vehicle, string vehicleId, string vehiclePrefab, string vehicleName, HST_CampaignState state, HST_BalanceConfig balance, HST_ArsenalService arsenal, HST_LootResult result)
	{
		if (balance.m_iVehicleLootMaxItemsPerAction > 0 && result.m_iItemsDeposited >= balance.m_iVehicleLootMaxItemsPerAction)
			return;

		result.m_iItemsSeen++;
		string prefab = ResolvePrefabName(item);
		if (prefab.IsEmpty())
		{
			result.m_iItemsSkippedBlocked++;
			return;
		}

		string category = ClassifyItem(item, prefab);
		if (!IsAllowedByLootRules(prefab, category, balance))
		{
			result.m_iItemsSkippedBlocked++;
			return;
		}

		if (balance.m_bVehicleLootOnlyLockedItems && arsenal.IsItemUnlocked(state, prefab))
		{
			result.m_iItemsSkippedUnlocked++;
			return;
		}

		HST_VehicleCargoItemState cargoItem = DepositVehicleCargo(state, vehicle, vehicleId, vehiclePrefab, vehicleName, prefab, category, BuildDisplayName(item, prefab));
		if (!cargoItem)
			return;

		result.m_iItemsDeposited++;
		result.m_aDepositedLines.Insert(string.Format("+ %1 | %2 | vehicle count %3", cargoItem.m_sDisplayName, cargoItem.m_sCategory, cargoItem.m_iCount));
		if (balance.m_bVehicleLootRemoveSource && RemoveLooseLootItem(item))
			result.m_iItemsRemoved++;
	}

	protected HST_VehicleCargoItemState DepositVehicleCargo(HST_CampaignState state, IEntity vehicle, string vehicleId, string vehiclePrefab, string vehicleName, string itemPrefab, string category, string displayName)
	{
		if (!state || vehicleId.IsEmpty() || itemPrefab.IsEmpty())
			return null;

		HST_VehicleCargoItemState cargoItem = state.FindVehicleCargoItem(vehicleId, itemPrefab);
		if (!cargoItem)
		{
			cargoItem = new HST_VehicleCargoItemState();
			cargoItem.m_sVehicleRuntimeId = vehicleId;
			cargoItem.m_sItemPrefab = itemPrefab;
			state.m_aVehicleCargoItems.Insert(cargoItem);
		}

		cargoItem.m_sVehiclePrefab = vehiclePrefab;
		cargoItem.m_sVehicleDisplayName = HST_DisplayNameService.ResolveVehicleDisplayName(vehiclePrefab, vehicleName);
		cargoItem.m_sCategory = category;
		cargoItem.m_sDisplayName = HST_DisplayNameService.ResolveItemDisplayName(null, itemPrefab, displayName);
		cargoItem.m_iCount++;
		cargoItem.m_iLastStoredAtSecond = state.m_iElapsedSeconds;
		if (vehicle)
			cargoItem.m_vLastVehiclePosition = vehicle.GetOrigin();

		return cargoItem;
	}

	protected string ResolvePrefabName(IEntity item)
	{
		if (!item || !item.GetPrefabData())
			return "";

		return item.GetPrefabData().GetPrefabName();
	}

	protected bool IsEligibleGarageVehicle(IEntity entity, IEntity playerEntity, HST_CampaignPreset preset)
	{
		if (!entity || entity == playerEntity)
			return false;

		string prefab = ResolvePrefabName(entity);
		if (prefab.IsEmpty())
			return false;

		if (prefab.Contains("Character") || prefab.Contains("Inventory") || prefab.Contains("Magazine"))
			return false;

		if (!IsEligibleVehicleRoot(entity, prefab))
			return false;

		return true;
	}

	protected bool IsEligibleLooseLootEntity(IEntity entity, IEntity playerEntity, IEntity targetVehicle)
	{
		if (!entity || entity == playerEntity || entity == targetVehicle)
			return false;

		string prefab = ResolvePrefabName(entity);
		if (prefab.IsEmpty())
			return false;

		if (prefab.Contains("Character") || prefab.Contains("Vehicle") || prefab.Contains("Vehicles"))
			return false;

		if (entity.GetParent())
			return false;

		if (entity.FindComponent(SCR_InventoryStorageManagerComponent))
			return false;

		if (entity.FindComponent(BaseWeaponComponent) || entity.FindComponent(BaseMagazineComponent) || entity.FindComponent(GrenadeMoveComponent) || entity.FindComponent(InventoryItemComponent))
			return true;

		return prefab.Contains("Weapons") || prefab.Contains("Weapon") || prefab.Contains("Magazines") || prefab.Contains("Magazine") || prefab.Contains("Grenade") || prefab.Contains("Items") || prefab.Contains("Equipment");
	}

	protected IEntity FindNearestLootVehicle(IEntity playerEntity, int radiusMeters, HST_CampaignState state)
	{
		HST_VehicleRootScanResult scan = FindNearestVehicleRoot(playerEntity, radiusMeters, state, "vehicle loot");
		return scan.m_SelectedRoot;
	}

	protected IEntity FindLootVehicleByRuntimeId(IEntity playerEntity, int radiusMeters, string vehicleRuntimeId, HST_CampaignState state)
	{
		if (!playerEntity || vehicleRuntimeId.IsEmpty())
			return null;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return null;

		HST_VehicleRootScanResult scan = FindNearestVehicleRoot(playerEntity, radiusMeters, state, "vehicle loot id " + vehicleRuntimeId);
		if (scan.m_SelectedRoot && ResolveVehicleRuntimeId(scan.m_SelectedRoot) == vehicleRuntimeId)
			return scan.m_SelectedRoot;

		m_aScanEntities.Clear();
		world.QueryEntitiesBySphere(playerEntity.GetOrigin(), radiusMeters, AddLootCandidate, null, EQueryEntitiesFlags.ALL);
		array<IEntity> checkedRoots = {};
		string lastReject = "target runtime id not found";
		foreach (IEntity candidate : m_aScanEntities)
		{
			string rejectReason;
			IEntity rootVehicle = ResolveVehicleRoot(candidate, rejectReason);
			if (!rootVehicle)
			{
				lastReject = rejectReason;
				continue;
			}

			if (checkedRoots.Find(rootVehicle) >= 0)
				continue;

			checkedRoots.Insert(rootVehicle);
			if (!IsEligibleLootVehicle(rootVehicle, playerEntity))
			{
				lastReject = "resolved root failed loot vehicle eligibility";
				continue;
			}

			if (ResolveVehicleRuntimeId(rootVehicle) == vehicleRuntimeId)
			{
				PublishVehicleTargetDiagnostics(state, "vehicle loot id target selected", rootVehicle, m_aScanEntities.Count(), checkedRoots.Count(), DistanceSq2D(playerEntity.GetOrigin(), rootVehicle.GetOrigin()), "matched runtime id");
				return rootVehicle;
			}
		}

		if (state)
		{
			state.m_iLastVehicleTargetCandidates = m_aScanEntities.Count();
			state.m_sLastVehicleTargetStatus = "vehicle target not found";
			state.m_sLastVehicleTargetPrefab = "";
			state.m_sLastVehicleTargetReason = lastReject;
			state.m_fLastVehicleTargetDistanceMeters = 0;
			state.m_iLastVehicleTargetCargoEntries = 0;
		}

		return null;
	}

	protected HST_VehicleRootScanResult FindNearestVehicleRoot(IEntity playerEntity, int radiusMeters, HST_CampaignState state, string purpose)
	{
		HST_VehicleRootScanResult result = new HST_VehicleRootScanResult();
		if (!playerEntity)
		{
			result.m_sRejectReason = "no player entity";
			PublishVehicleTargetDiagnostics(state, purpose + " failed", null, 0, 0, 0, result.m_sRejectReason);
			return result;
		}

		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			result.m_sRejectReason = "world not ready";
			PublishVehicleTargetDiagnostics(state, purpose + " failed", null, 0, 0, 0, result.m_sRejectReason);
			return result;
		}

		m_aScanEntities.Clear();
		world.QueryEntitiesBySphere(playerEntity.GetOrigin(), radiusMeters, AddLootCandidate, null, EQueryEntitiesFlags.ALL);

		float bestDistanceSq = 999999999.0;
		array<IEntity> checkedRoots = {};
		foreach (IEntity candidate : m_aScanEntities)
		{
			result.m_iCandidates++;
			string rejectReason;
			IEntity rootVehicle = ResolveVehicleRoot(candidate, rejectReason);
			if (!rootVehicle)
			{
				result.m_sRejectReason = rejectReason;
				continue;
			}

			if (checkedRoots.Find(rootVehicle) >= 0)
				continue;

			checkedRoots.Insert(rootVehicle);
			result.m_iResolvedRoots++;
			if (!IsEligibleLootVehicle(rootVehicle, playerEntity))
			{
				result.m_sRejectReason = "resolved root failed loot vehicle eligibility";
				continue;
			}

			float distanceSq = DistanceSq2D(playerEntity.GetOrigin(), rootVehicle.GetOrigin());
			if (distanceSq < bestDistanceSq)
			{
				result.m_SelectedRoot = rootVehicle;
				result.m_sSelectedPrefab = ResolvePrefabName(rootVehicle);
				result.m_sRejectReason = "selected nearest eligible root";
				result.m_fSelectedDistanceMeters = Math.Sqrt(distanceSq);
				bestDistanceSq = distanceSq;
			}
		}

		if (!result.m_SelectedRoot && result.m_sRejectReason.IsEmpty())
			result.m_sRejectReason = "no vehicle prefab roots found";

		PublishVehicleTargetDiagnostics(state, purpose, result.m_SelectedRoot, result.m_iCandidates, result.m_iResolvedRoots, bestDistanceSq, result.m_sRejectReason);
		return result;
	}

	protected void PublishVehicleTargetDiagnostics(HST_CampaignState state, string status, IEntity vehicle, int candidates, int resolvedRoots, float distanceSq, string reason)
	{
		if (!state)
			return;

		state.m_iLastVehicleTargetCandidates = candidates;
		state.m_sLastVehicleTargetStatus = status;
		state.m_sLastVehicleTargetReason = reason;
		state.m_iLastVehicleTargetCargoEntries = 0;
		state.m_fLastVehicleTargetDistanceMeters = 0;
		state.m_sLastVehicleTargetPrefab = "";

		if (vehicle)
		{
			string prefab = ResolvePrefabName(vehicle);
			string vehicleId = ResolveVehicleRuntimeId(vehicle);
			state.m_sLastVehicleTargetPrefab = prefab;
			state.m_fLastVehicleTargetDistanceMeters = Math.Sqrt(distanceSq);
			state.m_iLastVehicleTargetCargoEntries = CountVehicleCargoEntries(state, vehicleId) + CountLegacyVehiclePartCargoNear(state, vehicle, vehicleId);
			Print(string.Format("h-istasi vehicle target | %1 | selected %2 | prefab %3 | candidates %4 | roots %5 | distance %6m | reason %7", status, BuildVehicleDisplayName(vehicle, prefab), prefab, candidates, resolvedRoots, state.m_fLastVehicleTargetDistanceMeters, reason));
			return;
		}

		Print(string.Format("h-istasi vehicle target | %1 | no target | candidates %2 | roots %3 | reason %4", status, candidates, resolvedRoots, reason), LogLevel.WARNING);
	}

	protected bool IsPlayerAtVehicleRear(IEntity playerEntity, IEntity vehicle, int radiusMeters)
	{
		if (!playerEntity || !vehicle)
			return false;

		int gateMeters = Math.Max(3, Math.Min(radiusMeters, 8));
		return DistanceSq2D(playerEntity.GetOrigin(), vehicle.GetOrigin()) <= gateMeters * gateMeters;
	}

	protected bool IsEligibleLootVehicle(IEntity entity, IEntity playerEntity)
	{
		if (!entity || entity == playerEntity)
			return false;

		string prefab = ResolvePrefabName(entity);
		if (prefab.IsEmpty())
			return false;

		if (prefab.Contains("Character") || prefab.Contains("Inventory") || prefab.Contains("Magazine"))
			return false;

		return IsEligibleVehicleRoot(entity, prefab);
	}

	protected IEntity ResolveVehicleRoot(IEntity entity, out string rejectReason)
	{
		rejectReason = "entity missing";
		if (!entity)
			return null;

		array<IEntity> chain = {};
		IEntity cursor = entity;
		for (int i = 0; i < 16; i++)
		{
			if (!cursor)
				break;

			chain.Insert(cursor);
			cursor = cursor.GetParent();
		}

		for (int topDown = chain.Count() - 1; topDown >= 0; topDown--)
		{
			IEntity candidate = chain[topDown];
			if (!candidate)
				continue;

			string candidatePrefab = ResolvePrefabName(candidate);
			if (candidatePrefab.IsEmpty())
				continue;

			if (IsEligibleVehicleRoot(candidate, candidatePrefab))
			{
				rejectReason = "resolved canonical vehicle root";
				return candidate;
			}

			rejectReason = BuildVehicleRootRejectReason(candidate, candidatePrefab);
		}

		if (rejectReason.IsEmpty() || rejectReason == "entity missing")
			rejectReason = "candidate chain had no eligible vehicle root prefab";

		return null;
	}

	protected bool IsEligibleVehicleRoot(IEntity entity, string prefab)
	{
		if (!entity || prefab.IsEmpty())
			return false;

		if (!HasVehicleRootComponent(entity))
			return false;

		return HST_VehicleRootPolicy.IsEligibleVehicleRootPrefab(prefab) && !HST_VehicleRootPolicy.IsVehiclePartName(entity.GetName());
	}

	protected string BuildVehicleRootRejectReason(IEntity entity, string prefab)
	{
		if (!entity)
			return "candidate missing";

		if (prefab.IsEmpty())
			return "candidate had no prefab";

		string policyReason = HST_VehicleRootPolicy.BuildRejectReason(entity, prefab);
		if (policyReason != "candidate passed prefab checks")
			return policyReason;

		if (!HasVehicleRootComponent(entity))
			return "prefab passed checks but entity has no vehicle root component";

		return "candidate passed vehicle root checks";
	}

	protected bool HasVehicleRootComponent(IEntity entity)
	{
		if (!entity)
			return false;

		return entity.FindComponent(SCR_BaseCompartmentManagerComponent) != null;
	}

	protected bool IsLikelyVehicleRootPrefab(string prefab)
	{
		return HST_VehicleRootPolicy.IsEligibleVehicleRootPrefab(prefab);
	}

	protected bool IsRejectedVehicleRootPrefab(string prefab)
	{
		return HST_VehicleRootPolicy.IsRejectedVehicleRootPrefab(prefab);
	}

	protected bool IsVehiclePartEntity(IEntity entity, string prefab)
	{
		if (HST_VehicleRootPolicy.IsVehiclePartPrefab(prefab))
			return true;

		if (!entity)
			return false;

		return HST_VehicleRootPolicy.IsVehiclePartName(entity.GetName());
	}

	protected bool IsVehiclePartName(string name)
	{
		return HST_VehicleRootPolicy.IsVehiclePartName(name);
	}

	protected string ResolveVehicleRuntimeId(IEntity vehicle)
	{
		if (!vehicle)
			return "";

		BaseRplComponent rpl = BaseRplComponent.Cast(vehicle.FindComponent(BaseRplComponent));
		if (rpl)
			return string.Format("rpl_%1", rpl.Id());

		return string.Format("local_%1_%2", ResolvePrefabName(vehicle), vehicle.GetOrigin());
	}

	protected string ResolveFactionKey(IEntity entity)
	{
		if (!entity)
			return "";

		FactionAffiliationComponent factionComponent = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (!factionComponent)
			return "";

		return factionComponent.GetAffiliatedFactionKey();
	}

	protected string BuildVehicleDisplayName(IEntity entity, string prefab)
	{
		if (!prefab.IsEmpty())
			return HST_DisplayNameService.ResolveVehicleDisplayName(prefab);

		if (entity)
			return entity.GetName();

		return "captured vehicle";
	}

	protected string FindNearestZoneId(HST_CampaignState state, vector position)
	{
		HST_ZoneState bestZone;
		float bestDistanceSq = 999999999.0;
		foreach (HST_ZoneState zone : state.m_aZones)
		{
			if (!zone)
				continue;

			float distanceSq = DistanceSq2D(position, zone.m_vPosition);
			if (distanceSq < bestDistanceSq)
			{
				bestZone = zone;
				bestDistanceSq = distanceSq;
			}
		}

		if (bestZone)
			return bestZone.m_sZoneId;

		return "";
	}

	protected float DistanceSq2D(vector first, vector second)
	{
		float dx = first[0] - second[0];
		float dz = first[2] - second[2];
		return dx * dx + dz * dz;
	}

	protected vector ResolveStoredVehicleAngles(IEntity vehicle, string prefab)
	{
		vector angles;
		if (!vehicle)
			return angles;

		angles = vehicle.GetYawPitchRoll();
		if (angles[0] != 0 || angles[1] != 0 || angles[2] != 0)
			return angles;

		vector origin = vehicle.GetOrigin();
		int seed = Math.Round(origin[0]) * 13 + Math.Round(origin[2]) * 17 + prefab.Length() * 23;
		if (seed < 0)
			seed = -seed;

		angles[0] = seed - (seed / 360) * 360;
		return angles;
	}

	protected int ResolveGarageRedeployCost(string prefab)
	{
		if (IsLikelyArmedVehicle(prefab))
			return 250;

		return 100;
	}

	protected bool IsLikelyArmedVehicle(string prefab)
	{
		return prefab.Contains("APC") || prefab.Contains("BTR") || prefab.Contains("BMP") || prefab.Contains("M2") || prefab.Contains("DShK") || prefab.Contains("PKM") || prefab.Contains("Armed") || prefab.Contains("armed");
	}

	protected string BuildDisplayName(IEntity item, string prefab)
	{
		return HST_DisplayNameService.ResolveItemDisplayName(item, prefab);
	}

	protected string ClassifyItem(IEntity item, string prefab)
	{
		if (item.FindComponent(BaseMagazineComponent))
			return "magazine";

		if (item.FindComponent(BaseWeaponComponent))
			return "weapon";

		if (item.FindComponent(GrenadeMoveComponent))
			return "explosive";

		if (prefab.Contains("Magazine") || prefab.Contains("magazine"))
			return "magazine";

		if (prefab.Contains("Grenade") || prefab.Contains("Mine") || prefab.Contains("Explosive"))
			return "explosive";

		if (prefab.Contains("Launcher") || prefab.Contains("RPG") || prefab.Contains("M72") || prefab.Contains("AT4"))
			return "launcher";

		return "equipment";
	}

	protected bool IsAllowedByLootRules(string prefab, string category, HST_BalanceConfig balance)
	{
		if (category == "explosive" && !balance.m_bAllowExplosiveUnlocks)
			return false;

		if (IsGuidedLauncher(prefab) && !balance.m_bAllowGuidedLauncherUnlocks)
			return false;

		return true;
	}

	protected bool IsGuidedLauncher(string prefab)
	{
		return prefab.Contains("ATGM") || prefab.Contains("Javelin") || prefab.Contains("Stinger") || prefab.Contains("Igla") || prefab.Contains("Metis") || prefab.Contains("guided") || prefab.Contains("Guided");
	}

	protected bool RemoveLootedItem(SCR_InventoryStorageManagerComponent inventory, IEntity item)
	{
		if (!inventory || !item)
			return false;

		if (inventory.TryDeleteItem(item))
			return true;

		BaseInventoryStorageComponent storage = inventory.FindStorageForItem(item, EStoragePurpose.PURPOSE_ANY);
		return storage && inventory.TryRemoveItemFromStorage(item, storage);
	}

	protected bool RemoveLooseLootItem(IEntity item)
	{
		if (!item)
			return false;

		SCR_EntityHelper.DeleteEntityAndChildren(item);
		return true;
	}
}
