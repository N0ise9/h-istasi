class HST_GarrisonService
{
	HST_GarrisonState FindOrCreate(HST_CampaignState state, string zoneId, string factionKey)
	{
		if (!state || !state.FindZone(zoneId) || !state.FindFactionPool(factionKey))
			return null;

		HST_GarrisonState garrison = state.FindGarrison(zoneId, factionKey);
		if (garrison)
		{
			if (garrison.m_sGarrisonId.IsEmpty())
				garrison.m_sGarrisonId = HST_StableIdService.BuildGarrisonId(zoneId, factionKey);
			return garrison;
		}

		garrison = new HST_GarrisonState();
		garrison.m_sGarrisonId = HST_StableIdService.BuildGarrisonId(zoneId, factionKey);
		garrison.m_sZoneId = zoneId;
		garrison.m_sFactionKey = factionKey;
		state.m_aGarrisons.Insert(garrison);
		return garrison;
	}

	bool AddAbstractForces(HST_CampaignState state, string zoneId, string factionKey, int infantryCount, int vehicleCount)
	{
		if (infantryCount < 0 || vehicleCount < 0)
			return false;

		HST_GarrisonState garrison = FindOrCreate(state, zoneId, factionKey);
		HST_ZoneState zone = state.FindZone(zoneId);
		if (!garrison || !zone)
			return false;

		int nextInfantry = garrison.m_iInfantryCount + infantryCount;
		if (zone.m_iGarrisonSlots > 0)
		{
			int exactInfantry = CountExecutableManifestInfantry(state, garrison);
			int activeInfantry = Math.Max(0, zone.m_iActiveInfantryCount);
			int legacyCapacity = Math.Max(0, zone.m_iGarrisonSlots - exactInfantry - activeInfantry);
			nextInfantry = Math.Min(legacyCapacity, nextInfantry);
		}

		garrison.m_iInfantryCount = nextInfantry;
		garrison.m_iVehicleCount = Math.Max(0, garrison.m_iVehicleCount + vehicleCount);
		return true;
	}

	bool AddManifestForcesExact(HST_CampaignState state, string zoneId, string factionKey, HST_ForceManifestState manifest)
	{
		if (!state || !manifest || zoneId.IsEmpty() || factionKey.IsEmpty())
			return false;
		if (!manifest.m_bFrozen || manifest.m_sManifestId.IsEmpty() || manifest.m_sFactionKey != factionKey || manifest.m_sTargetZoneId != zoneId)
			return false;
		if (manifest.m_iAcceptedMemberCount <= 0 || manifest.m_iAcceptedMemberCount != manifest.m_aMembers.Count())
			return false;
		if (manifest.m_sPolicyId == HST_GarrisonPatrolOperationService.EXACT_POLICY_ID)
			return LinkExecutableManifestExact(state, zoneId, factionKey, manifest) != null;

		HST_GarrisonState existing = state.FindGarrison(zoneId, factionKey);
		if (existing && existing.m_aAcceptedManifestIds.Contains(manifest.m_sManifestId))
			return true;

		HST_ZoneState zone = state.FindZone(zoneId);
		if (!zone)
			return false;

		int beforeInfantry;
		if (existing)
			beforeInfantry = Math.Max(0, existing.m_iInfantryCount);
		int activeInfantry = Math.Max(0, zone.m_iActiveInfantryCount);
		int executableInfantry = CountExecutableManifestInfantry(state, existing);
		if (zone.m_iGarrisonSlots > 0 && beforeInfantry + executableInfantry + activeInfantry
			+ manifest.m_iAcceptedMemberCount > zone.m_iGarrisonSlots)
			return false;

		HST_GarrisonState garrison = FindOrCreate(state, zoneId, factionKey);
		if (!garrison)
			return false;

		garrison.m_iInfantryCount = beforeInfantry + manifest.m_iAcceptedMemberCount;
		garrison.m_aAcceptedManifestIds.Insert(manifest.m_sManifestId);
		return garrison.m_iInfantryCount - beforeInfantry == manifest.m_iAcceptedMemberCount;
	}

	bool RemoveManifestForcesExact(HST_CampaignState state, string zoneId, string factionKey, HST_ForceManifestState manifest)
	{
		if (!state || !manifest)
			return false;
		if (manifest.m_sPolicyId == HST_GarrisonPatrolOperationService.EXACT_POLICY_ID)
			return UnlinkExecutableManifestExact(state, zoneId, factionKey, manifest);

		HST_GarrisonState garrison = state.FindGarrison(zoneId, factionKey);
		if (!garrison)
			return false;

		int manifestIndex = garrison.m_aAcceptedManifestIds.Find(manifest.m_sManifestId);
		if (manifestIndex < 0)
			return false;

		garrison.m_aAcceptedManifestIds.Remove(manifestIndex);
		garrison.m_iInfantryCount = Math.Max(0, garrison.m_iInfantryCount - Math.Max(0, manifest.m_iAcceptedMemberCount));
		if (garrison.m_iInfantryCount <= 0 && garrison.m_iVehicleCount <= 0 && garrison.m_aAcceptedManifestIds.Count() == 0)
		{
			int garrisonIndex = state.m_aGarrisons.Find(garrison);
			if (garrisonIndex >= 0)
				state.m_aGarrisons.Remove(garrisonIndex);
		}
		return true;
	}

	HST_GarrisonState LinkExecutableManifestExact(
		HST_CampaignState state,
		string zoneId,
		string factionKey,
		HST_ForceManifestState manifest)
	{
		if (!state || !manifest || zoneId.IsEmpty() || factionKey.IsEmpty())
			return null;
		if (!manifest.m_bFrozen || manifest.m_sPolicyId != HST_GarrisonPatrolOperationService.EXACT_POLICY_ID
			|| manifest.m_sManifestId.IsEmpty() || manifest.m_sFactionKey != factionKey
			|| manifest.m_sTargetZoneId != zoneId || manifest.m_iAcceptedMemberCount <= 0
			|| manifest.m_iAcceptedMemberCount != manifest.m_aMembers.Count())
			return null;

		HST_GarrisonState existing = state.FindGarrison(zoneId, factionKey);
		if (existing && existing.m_aAcceptedManifestIds.Contains(manifest.m_sManifestId))
			return existing;
		HST_ZoneState zone = state.FindZone(zoneId);
		if (!zone)
			return null;
		int legacyInfantry;
		if (existing)
			legacyInfantry = Math.Max(0, existing.m_iInfantryCount);
		int exactInfantry = CountExecutableManifestInfantry(state, existing);
		int activeInfantry = Math.Max(0, zone.m_iActiveInfantryCount);
		if (zone.m_iGarrisonSlots > 0
			&& legacyInfantry + exactInfantry + activeInfantry + manifest.m_iAcceptedMemberCount > zone.m_iGarrisonSlots)
			return null;

		HST_GarrisonState garrison = FindOrCreate(state, zoneId, factionKey);
		if (!garrison)
			return null;
		garrison.m_aAcceptedManifestIds.Insert(manifest.m_sManifestId);
		return garrison;
	}

	bool UnlinkExecutableManifestExact(
		HST_CampaignState state,
		string zoneId,
		string factionKey,
		HST_ForceManifestState manifest)
	{
		if (!state || !manifest || manifest.m_sPolicyId != HST_GarrisonPatrolOperationService.EXACT_POLICY_ID)
			return false;
		HST_GarrisonState garrison = state.FindGarrison(zoneId, factionKey);
		if (!garrison)
			return false;
		int manifestIndex = garrison.m_aAcceptedManifestIds.Find(manifest.m_sManifestId);
		if (manifestIndex < 0)
			return false;
		garrison.m_aAcceptedManifestIds.Remove(manifestIndex);
		if (garrison.m_iInfantryCount <= 0 && garrison.m_iVehicleCount <= 0
			&& garrison.m_aAcceptedManifestIds.Count() == 0)
		{
			int garrisonIndex = state.m_aGarrisons.Find(garrison);
			if (garrisonIndex >= 0)
				state.m_aGarrisons.Remove(garrisonIndex);
		}
		return true;
	}

	int CountExecutableManifestInfantry(HST_CampaignState state, HST_GarrisonState garrison)
	{
		if (!state || !garrison)
			return 0;
		int infantryCount;
		array<string> countedManifestIds = {};
		foreach (string manifestId : garrison.m_aAcceptedManifestIds)
		{
			if (manifestId.IsEmpty() || countedManifestIds.Contains(manifestId))
				continue;
			HST_ForceManifestState manifest = state.FindForceManifest(manifestId);
			if (!manifest || manifest.m_sPolicyId != HST_GarrisonPatrolOperationService.EXACT_POLICY_ID)
				continue;
			countedManifestIds.Insert(manifestId);
			if (manifest.m_sTargetZoneId != garrison.m_sZoneId
				|| manifest.m_sFactionKey != garrison.m_sFactionKey)
			{
				// A misplaced exact backlink is corruption, not free capacity. Retain
				// its full accepted reservation until typed validation resolves it.
				infantryCount += Math.Max(0, manifest.m_iAcceptedMemberCount);
				continue;
			}
			HST_OperationRecordState operation = ResolveUniqueReciprocalExactPatrolOperation(
				state,
				manifest,
				garrison);
			bool validSettlement = operation
				&& operation.m_eType == HST_EOperationType.HST_OPERATION_TYPE_GARRISON_PATROL
				&& operation.m_iContractVersion == HST_GarrisonPatrolOperationService.EXACT_CONTRACT_VERSION
				&& operation.m_iProjectionContractVersion == HST_GarrisonPatrolOperationService.EXACT_PROJECTION_CONTRACT_VERSION
				&& operation.m_sAssignmentKind == HST_GarrisonPatrolOperationService.ASSIGNMENT_KIND
				&& operation.m_sSettlementPolicyId == HST_GarrisonPatrolOperationService.SETTLEMENT_POLICY_ID
				&& operation.m_eSettlementState == HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_SETTLED
				&& operation.m_eTerminalResult != HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_NONE
				&& operation.m_eTerminalResult != HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_UNKNOWN
				&& operation.m_eDutyState == HST_EOperationDutyState.HST_OPERATION_DUTY_SETTLED
				&& operation.m_eResumeDutyState == HST_EOperationDutyState.HST_OPERATION_DUTY_SETTLED
				&& operation.m_eEngagementMode == HST_EOperationEngagementMode.HST_OPERATION_ENGAGEMENT_CLEAR
				&& operation.m_eMaterializationState == HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_RETIRED
				&& operation.m_ePositionAuthority == HST_EOperationPositionAuthority.HST_OPERATION_POSITION_STRATEGIC
				&& operation.m_sSettlementId == HST_OperationService.BuildSettlementId(
					operation.m_sOperationId,
					HST_GarrisonPatrolOperationService.SETTLEMENT_KIND);
			if (validSettlement)
				continue;

			HST_ForceSpawnResultState matchedBatch;
			int batchMatches;
			foreach (HST_ForceSpawnResultState batch : state.m_aForceSpawnResults)
			{
				if (!batch || batch.m_sManifestId != manifest.m_sManifestId)
					continue;
				batchMatches++;
				matchedBatch = batch;
			}
			bool openExactOperation = operation
				&& operation.m_eType == HST_EOperationType.HST_OPERATION_TYPE_GARRISON_PATROL
				&& operation.m_iContractVersion == HST_GarrisonPatrolOperationService.EXACT_CONTRACT_VERSION
				&& operation.m_eSettlementState == HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_OPEN
				&& operation.m_eTerminalResult == HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_NONE;
			if (openExactOperation && batchMatches == 1 && matchedBatch
				&& matchedBatch.m_sOperationId == manifest.m_sOperationId)
			{
				HST_ForceSpawnQueueService queue = new HST_ForceSpawnQueueService();
				infantryCount += queue.CountStrategicLivingMemberSlots(matchedBatch);
			}
			else
			{
				// Missing or ambiguous open authority remains capacity-reserved until
				// the typed validator can reconcile it without inventing casualties.
				infantryCount += Math.Max(0, manifest.m_iAcceptedMemberCount);
			}
		}
		return infantryCount;
	}

	protected HST_OperationRecordState ResolveUniqueReciprocalExactPatrolOperation(
		HST_CampaignState state,
		HST_ForceManifestState manifest,
		HST_GarrisonState garrison)
	{
		if (!state || !manifest || !garrison || manifest.m_sOperationId.IsEmpty()
			|| manifest.m_sManifestId.IsEmpty() || manifest.m_sQuoteId.IsEmpty())
			return null;

		HST_OperationRecordState resolved;
		int authorityMatches;
		foreach (HST_OperationRecordState operation : state.m_aOperations)
		{
			if (!operation)
				continue;
			if (operation.m_sOperationId != manifest.m_sOperationId
				&& operation.m_sManifestId != manifest.m_sManifestId)
				continue;
			authorityMatches++;
			resolved = operation;
		}
		if (authorityMatches != 1 || !resolved)
			return null;
		if (resolved.m_sOperationId != manifest.m_sOperationId
			|| resolved.m_sManifestId != manifest.m_sManifestId
			|| resolved.m_sQuoteId != manifest.m_sQuoteId
			|| resolved.m_sOwnerFactionKey != garrison.m_sFactionKey
			|| resolved.m_sAssignmentZoneId != garrison.m_sZoneId)
			return null;
		return resolved;
	}

	bool RemoveAbstractForces(HST_CampaignState state, string zoneId, string factionKey, int infantryCount, int vehicleCount)
	{
		if (!state || zoneId.IsEmpty() || factionKey.IsEmpty())
			return false;

		if (infantryCount < 0 || vehicleCount < 0)
			return false;

		HST_GarrisonState garrison = state.FindGarrison(zoneId, factionKey);
		if (!garrison)
			return false;

		int beforeInfantry = garrison.m_iInfantryCount;
		int beforeVehicles = garrison.m_iVehicleCount;

		garrison.m_iInfantryCount = Math.Max(0, garrison.m_iInfantryCount - infantryCount);
		garrison.m_iVehicleCount = Math.Max(0, garrison.m_iVehicleCount - vehicleCount);

		return garrison.m_iInfantryCount != beforeInfantry || garrison.m_iVehicleCount != beforeVehicles;
	}

	bool FoldSurvivors(HST_CampaignState state, string zoneId, string factionKey, int infantryCount, int vehicleCount)
	{
		if (infantryCount < 0 || vehicleCount < 0)
			return false;

		HST_GarrisonState garrison = FindOrCreate(state, zoneId, factionKey);
		if (!garrison)
			return false;

		HST_ZoneState zone = state.FindZone(zoneId);
		if (zone && zone.m_iGarrisonSlots > 0)
		{
			int exactInfantry = CountExecutableManifestInfantry(state, garrison);
			infantryCount = Math.Min(Math.Max(0, zone.m_iGarrisonSlots - exactInfantry), infantryCount);
		}

		garrison.m_iInfantryCount = infantryCount;
		garrison.m_iVehicleCount = vehicleCount;
		return true;
	}

	HST_ForceCompositionResult ComposeGarrisonForce(HST_CampaignState state, HST_CampaignPreset preset, HST_ZoneState zone, HST_GarrisonState garrison, HST_ForceCompositionService forceCompositions = null)
	{
		HST_ForceCompositionService compositions = forceCompositions;
		if (!compositions)
			compositions = new HST_ForceCompositionService();

		return compositions.Compose(state, preset, BuildGarrisonForceRequest(state, preset, zone, garrison));
	}

	HST_ForceRequest BuildGarrisonForceRequest(HST_CampaignState state, HST_CampaignPreset preset, HST_ZoneState zone, HST_GarrisonState garrison)
	{
		HST_ForceRequest request = new HST_ForceRequest();
		if (!state || !zone || !garrison)
			return request;

		request.m_sRequestId = "garrison_" + garrison.m_sZoneId + "_" + garrison.m_sFactionKey;
		request.m_sFactionKey = garrison.m_sFactionKey;
		request.m_sIntentId = HST_ForceCompositionService.INTENT_GARRISON;
		if (zone.m_eType == HST_EZoneType.HST_ZONE_TOWN)
			request.m_sIntentId = HST_ForceCompositionService.INTENT_TOWN_POLICE;
		else if (zone.m_eType == HST_EZoneType.HST_ZONE_OUTPOST || zone.m_eType == HST_EZoneType.HST_ZONE_AIRFIELD || zone.m_eType == HST_EZoneType.HST_ZONE_SEAPORT)
			request.m_sIntentId = HST_ForceCompositionService.INTENT_CHECKPOINT;
		request.m_sTargetZoneId = zone.m_sZoneId;
		request.m_vTargetPosition = zone.m_vPosition;
		request.m_iWarLevel = Math.Max(1, state.m_iWarLevel);
		request.m_iMinManpower = Math.Max(1, Math.Min(4, garrison.m_iInfantryCount));
		request.m_iMaxManpower = Math.Max(request.m_iMinManpower, Math.Max(4, garrison.m_iInfantryCount));
		request.m_iBudget = Math.Max(16, request.m_iMaxManpower * 3 + Math.Max(0, garrison.m_iVehicleCount) * 12);
		request.m_bAllowInfantry = true;
		request.m_bAllowVehicles = garrison.m_iVehicleCount > 0;
		request.m_bAllowArmedVehicles = request.m_bAllowVehicles && state.m_iWarLevel >= 5;
		request.m_bAllowLightArmor = request.m_bAllowVehicles && state.m_iWarLevel >= 6;
		request.m_bExplain = true;
		request.m_sReason = "garrison activation";
		return request;
	}
}
