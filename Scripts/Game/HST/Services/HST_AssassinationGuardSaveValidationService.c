// Schema-55 restore boundary for the exact guard force attached to the
// assassinate-officer mission. Historical missions and their aggregate guard
// groups remain contract-zero; this service never infers exact authority from
// a mission id, a mission-instance backlink, or a projection id alone.
class HST_AssassinationGuardSaveValidationService
{
	protected HST_CampaignSaveData m_SaveData;

	void Normalize(HST_CampaignSaveData saveData, int restoredSchemaVersion)
	{
		m_SaveData = saveData;
		if (!m_SaveData)
			return;
		if (restoredSchemaVersion < 55)
		{
			PreserveHistoricalMissionGuards();
			m_SaveData = null;
			return;
		}

		array<string> validatedOperationIds = {};
		int validatedCount;
		int quarantinedCount;
		foreach (HST_ActiveMissionState mission : m_SaveData.m_aActiveMissions)
		{
			if (!mission || !HST_MissionGuardOperationService.IsExactMission(mission))
				continue;

			HST_OperationRecordState operation = FindUniqueOperation(mission.m_sOperationId);
			HST_ForceManifestState manifest = FindUniqueManifest(mission.m_sManifestId);
			HST_ForceSpawnResultState batch = FindUniqueBatch(mission.m_sSpawnResultId);
			HST_ActiveGroupState group;
			if (operation)
				group = FindUniqueGroup(operation.m_sGroupId);
			HST_MissionObjectiveState objective = FindUniqueHVTObjective(mission.m_sInstanceId);
			HST_MissionAssetState hvt = FindUniqueHVTAsset(mission.m_sInstanceId);

			string failure = ValidateAggregate(
				mission,
				operation,
				manifest,
				batch,
				group,
				objective,
				hvt);
			if (!failure.IsEmpty())
			{
				QuarantineAggregate(mission, operation, batch, group, failure);
				quarantinedCount++;
				continue;
			}

			NormalizeValidAggregate(mission, operation, batch, group);
			validatedOperationIds.Insert(operation.m_sOperationId);
			validatedCount++;
		}

		quarantinedCount += PreserveOrphanClaimants(validatedOperationIds);
		if (quarantinedCount > 0 && !HasEvent("normalization_schema55_exact_mission_guard_conflict"))
		{
			HST_CampaignEventState eventState = new HST_CampaignEventState();
			eventState.m_sEventId = "normalization_schema55_exact_mission_guard_conflict";
			eventState.m_sCategory = "normalization";
			eventState.m_sAggregateType = "mission_guard_authority";
			eventState.m_sAggregateId = "schema55";
			eventState.m_sTransition = "corrupt_exact_mission_guards_quarantined";
			eventState.m_sReason = string.Format(
				"validated %1 exact assassination guard authorities and quarantined %2 incomplete, conflicting, unsupported, or non-unique graphs without claiming or changing their HVT assets",
				validatedCount,
				quarantinedCount);
			eventState.m_iCreatedAtSecond = Math.Max(0, m_SaveData.m_iElapsedSeconds);
			m_SaveData.m_aCampaignEvents.Insert(eventState);
		}
		m_SaveData = null;
	}

	static bool IsSchema55MissionGuardMissionClaimant(
		HST_CampaignSaveData saveData,
		HST_ActiveMissionState mission)
	{
		if (!saveData || !mission)
			return false;
		if (HST_MissionGuardOperationService.IsExactMission(mission)
			|| HST_MissionGuardOperationService.IsQuarantinedMission(mission))
			return true;
		foreach (HST_OperationRecordState operation : saveData.m_aOperations)
		{
			if (!IsSchema55MissionGuardOperationClaimant(saveData, operation)
				|| operation.m_sOperationId.IsEmpty()
				|| operation.m_sMissionInstanceId.IsEmpty())
				continue;
			if (mission.m_sOperationId == operation.m_sOperationId
				&& mission.m_sInstanceId == operation.m_sMissionInstanceId
				&& ((!mission.m_sManifestId.IsEmpty() && mission.m_sManifestId == operation.m_sManifestId)
					|| (!mission.m_sSpawnResultId.IsEmpty() && mission.m_sSpawnResultId == operation.m_sSpawnResultId)))
				return true;
		}
		return false;
	}

	static bool IsSchema55MissionGuardOperationClaimant(
		HST_CampaignSaveData saveData,
		HST_OperationRecordState operation)
	{
		return saveData && operation
			&& operation.m_eType == HST_EOperationType.HST_OPERATION_TYPE_MISSION_GUARD;
	}

	static bool IsSchema55MissionGuardManifestClaimant(
		HST_CampaignSaveData saveData,
		HST_ForceManifestState manifest)
	{
		if (!saveData || !manifest)
			return false;
		if (manifest.m_sPolicyId == HST_MissionGuardOperationService.EXACT_POLICY_ID)
			return true;
		foreach (HST_OperationRecordState operation : saveData.m_aOperations)
		{
			if (!IsSchema55MissionGuardOperationClaimant(saveData, operation)
				|| operation.m_sOperationId.IsEmpty())
				continue;
			if (manifest.m_sOperationId == operation.m_sOperationId
				&& !manifest.m_sManifestId.IsEmpty()
				&& manifest.m_sManifestId == operation.m_sManifestId)
				return true;
		}
		return false;
	}

	static bool IsSchema55MissionGuardBatchClaimant(
		HST_CampaignSaveData saveData,
		HST_ForceSpawnResultState batch)
	{
		if (!saveData || !batch)
			return false;
		foreach (HST_OperationRecordState operation : saveData.m_aOperations)
		{
			if (!IsSchema55MissionGuardOperationClaimant(saveData, operation)
				|| operation.m_sOperationId.IsEmpty()
				|| batch.m_sOperationId != operation.m_sOperationId)
				continue;
			if ((!batch.m_sResultId.IsEmpty() && batch.m_sResultId == operation.m_sSpawnResultId)
				|| (!batch.m_sManifestId.IsEmpty() && batch.m_sManifestId == operation.m_sManifestId)
				|| (!batch.m_sForceId.IsEmpty() && batch.m_sForceId == operation.m_sForceId)
				|| (!batch.m_sProjectionId.IsEmpty() && batch.m_sProjectionId == operation.m_sProjectionId))
				return true;
		}
		foreach (HST_ForceManifestState manifest : saveData.m_aForceManifests)
		{
			if (!IsSchema55MissionGuardManifestClaimant(saveData, manifest)
				|| manifest.m_sOperationId.IsEmpty()
				|| manifest.m_sManifestId.IsEmpty())
				continue;
			if (batch.m_sOperationId == manifest.m_sOperationId
				&& batch.m_sManifestId == manifest.m_sManifestId)
				return true;
		}
		return false;
	}

	static bool IsSchema55MissionGuardGroupClaimant(
		HST_CampaignSaveData saveData,
		HST_ActiveGroupState group)
	{
		if (!saveData || !group)
			return false;
		if (group.m_sSpawnFallbackMode == HST_MissionGuardOperationService.EXACT_GROUP_MODE
			|| group.m_sSpawnFallbackMode == HST_MissionGuardOperationService.QUARANTINE_STATUS
			|| group.m_sRuntimeStatus.StartsWith("exact_mission_guard_")
			|| group.m_sRuntimeStatus.StartsWith("mission_guard_virtual")
			|| group.m_sRuntimeStatus.StartsWith("mission_guard_materializing")
			|| group.m_sRuntimeStatus.StartsWith("mission_guard_physical")
			|| group.m_sRuntimeStatus.StartsWith("mission_guard_dematerializing"))
			return true;
		foreach (HST_OperationRecordState operation : saveData.m_aOperations)
		{
			if (!IsSchema55MissionGuardOperationClaimant(saveData, operation)
				|| operation.m_sOperationId.IsEmpty()
				|| group.m_sOperationId != operation.m_sOperationId)
				continue;
			if ((!group.m_sGroupId.IsEmpty() && group.m_sGroupId == operation.m_sGroupId)
				|| (!group.m_sManifestId.IsEmpty() && group.m_sManifestId == operation.m_sManifestId)
				|| (!group.m_sSpawnResultId.IsEmpty() && group.m_sSpawnResultId == operation.m_sSpawnResultId)
				|| (!group.m_sForceId.IsEmpty() && group.m_sForceId == operation.m_sForceId)
				|| (!group.m_sProjectionId.IsEmpty() && group.m_sProjectionId == operation.m_sProjectionId))
				return true;
		}
		return false;
	}

	protected void PreserveHistoricalMissionGuards()
	{
		int legacyMissionCount;
		int legacyGroupCount;
		int unsupportedOperationCount;
		foreach (HST_ActiveMissionState mission : m_SaveData.m_aActiveMissions)
		{
			if (!mission || mission.m_sMissionId != HST_MissionGuardOperationService.EXACT_MISSION_ID)
				continue;
			mission.m_iOperationContractVersion = 0;
			legacyMissionCount++;
			foreach (HST_ActiveGroupState group : m_SaveData.m_aActiveGroups)
			{
				if (!group)
					continue;
				if (group.m_sMissionInstanceId == mission.m_sInstanceId
					|| group.m_sGroupId == "mission_group_" + mission.m_sInstanceId)
					legacyGroupCount++;
			}
		}
		foreach (HST_OperationRecordState operation : m_SaveData.m_aOperations)
		{
			if (!operation || operation.m_eType != HST_EOperationType.HST_OPERATION_TYPE_MISSION_GUARD)
				continue;
			QuarantineAggregate(
				null,
				operation,
				FindUniqueBatch(operation.m_sSpawnResultId),
				FindUniqueGroup(operation.m_sGroupId),
				"pre-schema-55 save carried an unsupported exact assassination guard operation");
			unsupportedOperationCount++;
		}
		if (HasEvent("migration_schema55_exact_mission_guard"))
			return;
		HST_CampaignEventState eventState = new HST_CampaignEventState();
		eventState.m_sEventId = "migration_schema55_exact_mission_guard";
		eventState.m_sCategory = "migration";
		eventState.m_sAggregateType = "mission_guard_authority";
		eventState.m_sAggregateId = "schema55";
		eventState.m_sTransition = "legacy_assassination_guards_preserved";
		eventState.m_sReason = string.Format(
			"preserved %1 historical officer missions and %2 historical mission groups on contract zero; quarantined %3 unsupported exact-operation rows and inferred no manifest, roster, batch, casualty, projection, or settlement authority",
			legacyMissionCount,
			legacyGroupCount,
			unsupportedOperationCount);
		eventState.m_iCreatedAtSecond = Math.Max(0, m_SaveData.m_iElapsedSeconds);
		m_SaveData.m_aCampaignEvents.Insert(eventState);
	}

	protected string ValidateAggregate(
		HST_ActiveMissionState mission,
		HST_OperationRecordState operation,
		HST_ForceManifestState manifest,
		HST_ForceSpawnResultState batch,
		HST_ActiveGroupState group,
		HST_MissionObjectiveState objective,
		HST_MissionAssetState hvt)
	{
		if (!mission || !operation || !manifest)
			return "exact assassination guard restore authority is incomplete or ambiguous";
		string failure = ValidateIdentity(mission, operation, manifest, batch, group);
		if (!failure.IsEmpty())
			return failure;
		failure = ValidateHVTBoundary(mission, operation, objective, hvt);
		if (!failure.IsEmpty())
			return failure;
		failure = ValidateFrozenManifest(mission, manifest, group);
		if (!failure.IsEmpty())
			return failure;
		failure = ValidateLifecycle(mission, operation, group);
		if (!failure.IsEmpty())
			return failure;
		if (operation.m_eSettlementState == HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_SETTLED)
			return ValidateSettlement(mission, operation, batch, group);
		if (!batch || !group)
			return "open exact assassination guard restore authority is incomplete or ambiguous";
		failure = ValidateBatchSlotBijection(manifest, batch);
		if (!failure.IsEmpty())
			return failure;
		failure = ValidateRuntime(mission, operation, manifest, batch, group);
		if (!failure.IsEmpty())
			return failure;
		return ValidateSettlement(mission, operation, batch, group);
	}

	protected string ValidateIdentity(
		HST_ActiveMissionState mission,
		HST_OperationRecordState operation,
		HST_ForceManifestState manifest,
		HST_ForceSpawnResultState batch,
		HST_ActiveGroupState group)
	{
		if (!HST_MissionGuardOperationService.IsExactMission(mission)
			|| mission.m_sRuntimePrimitive != "kill_hvt"
			|| operation.m_eType != HST_EOperationType.HST_OPERATION_TYPE_MISSION_GUARD
			|| operation.m_iContractVersion != HST_MissionGuardOperationService.EXACT_CONTRACT_VERSION
			|| mission.m_iOperationContractVersion != HST_MissionGuardOperationService.EXACT_CONTRACT_VERSION)
			return "exact assassination guard mission or operation contract conflicts";
		if (mission.m_sInstanceId.IsEmpty() || mission.m_sOperationId.IsEmpty()
			|| mission.m_sManifestId.IsEmpty() || mission.m_sSpawnResultId.IsEmpty()
			|| operation.m_sGroupId.IsEmpty() || operation.m_sForceId.IsEmpty()
			|| operation.m_sProjectionId.IsEmpty())
			return "exact assassination guard reciprocal identity is incomplete";
		if (operation.m_sOperationId != mission.m_sOperationId
			|| operation.m_sMissionInstanceId != mission.m_sInstanceId
			|| operation.m_sManifestId != mission.m_sManifestId
			|| operation.m_sSpawnResultId != mission.m_sSpawnResultId)
			return "exact assassination guard mission and operation backlinks conflict";
		if (!operation.m_sSupportRequestId.IsEmpty() || !operation.m_sEnemyOrderId.IsEmpty()
			|| !operation.m_sQuoteId.IsEmpty())
			return "exact assassination guard operation contains foreign authority backlinks";
		if (operation.m_sOperationId != HST_MissionGuardOperationService.BuildOperationId(mission.m_sInstanceId)
			|| manifest.m_sManifestId != HST_MissionGuardOperationService.BuildManifestId(mission.m_sInstanceId)
			|| operation.m_sSpawnResultId != HST_MissionGuardOperationService.BuildSpawnResultId(mission.m_sInstanceId)
			|| operation.m_sForceId != HST_MissionGuardOperationService.BuildForceId(mission.m_sInstanceId)
			|| operation.m_sProjectionId != HST_MissionGuardOperationService.BuildProjectionId(mission.m_sInstanceId)
			|| operation.m_sGroupId != operation.m_sProjectionId)
			return "exact assassination guard deterministic identity conflicts";
		if (manifest.m_sManifestId != operation.m_sManifestId
			|| manifest.m_sOperationId != operation.m_sOperationId)
			return "exact assassination guard manifest backlinks conflict";
		if (batch)
		{
			if (batch.m_sResultId != operation.m_sSpawnResultId
				|| batch.m_sRequestId != "mission_guard_" + mission.m_sInstanceId
				|| batch.m_sOperationId != operation.m_sOperationId
				|| batch.m_sManifestId != operation.m_sManifestId
				|| batch.m_sForceId != operation.m_sForceId
				|| batch.m_sProjectionId != operation.m_sProjectionId
				|| batch.m_iPriority != HST_MissionGuardOperationService.EXACT_PRIORITY
				|| batch.m_iMaxRetries != HST_MissionGuardOperationService.EXACT_MAX_RETRIES)
				return "exact assassination guard batch backlinks conflict";
			if (batch.m_sManifestHash.IsEmpty() || batch.m_sManifestHash != manifest.m_sManifestHash)
				return "exact assassination guard immutable manifest receipt conflicts";
		}
		if (group)
		{
			if (group.m_sGroupId != operation.m_sProjectionId
				|| group.m_sGroupId != operation.m_sGroupId
				|| group.m_sOperationId != operation.m_sOperationId
				|| group.m_sMissionInstanceId != operation.m_sMissionInstanceId
				|| group.m_sManifestId != operation.m_sManifestId
				|| group.m_sSpawnResultId != operation.m_sSpawnResultId
				|| group.m_sForceId != operation.m_sForceId
				|| group.m_sProjectionId != operation.m_sProjectionId)
				return "exact assassination guard group backlinks conflict";
		}
		if (CountMissionsByAnyIdentity(mission, operation) != 1
			|| CountOperationsByAnyIdentity(mission, operation) != 1
			|| CountManifestsByAnyIdentity(operation, manifest) != 1)
			return "exact assassination guard authority identity is ambiguous";
		if ((batch && CountBatchesByAnyIdentity(operation, batch) != 1)
			|| (!batch && CountBatchesClaimingOperation(operation) != 0)
			|| (group && CountGroupsByAnyIdentity(operation, group) != 1)
			|| (!group && CountGroupsClaimingOperation(operation) != 0))
			return "exact assassination guard runtime authority identity is ambiguous";
		if (mission.m_sTargetZoneId.IsEmpty() || IsZeroVector(mission.m_vTargetPosition)
			|| operation.m_sOwnerFactionKey.IsEmpty()
			|| operation.m_sAssignmentZoneId != mission.m_sTargetZoneId
			|| operation.m_sTacticalTargetZoneId != mission.m_sTargetZoneId
			|| (group && group.m_sZoneId != mission.m_sTargetZoneId)
			|| (group && group.m_sFactionKey != operation.m_sOwnerFactionKey))
			return "exact assassination guard owner or assignment conflicts";
		if (manifest.m_sFactionKey != operation.m_sOwnerFactionKey
			|| manifest.m_sTargetZoneId != operation.m_sAssignmentZoneId)
			return "exact assassination guard manifest owner or assignment conflicts";
		return "";
	}

	protected string ValidateHVTBoundary(
		HST_ActiveMissionState mission,
		HST_OperationRecordState operation,
		HST_MissionObjectiveState objective,
		HST_MissionAssetState hvt)
	{
		if (!operation)
			return "exact assassination guard operation is unavailable at the HVT boundary";
		int objectiveCount = CountHVTObjectives(mission.m_sInstanceId);
		int assetCount = CountHVTAssets(mission.m_sInstanceId);
		bool activeDestroyedGuard = operation.m_eSettlementState
			== HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_SETTLED
			&& operation.m_eTerminalResult == HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_DESTROYED
			&& mission.m_eStatus == HST_EMissionStatus.HST_MISSION_ACTIVE;
		bool requiresLiveHVTBoundary = operation.m_eSettlementState
			== HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_OPEN
			|| activeDestroyedGuard;
		if (!requiresLiveHVTBoundary)
		{
			if (objectiveCount > 1 || assetCount > 1)
				return "settled exact assassination guard retains ambiguous HVT runtime residue";
			if (hvt && (hvt.m_sOperationId == mission.m_sOperationId
				|| hvt.m_sManifestId == mission.m_sManifestId
				|| !hvt.m_sManifestSlotId.IsEmpty()
				|| !hvt.m_sConvoyElementId.IsEmpty()))
				return "settled exact assassination guard improperly claims surviving HVT residue";
			return "";
		}
		if (!objective || !hvt || objectiveCount != 1 || assetCount != 1)
			return "exact assassination guard requires one unique mission-owned HVT objective and asset";
		if (activeDestroyedGuard && (hvt.m_bDestroyed || !hvt.m_bAlive
			|| objective.m_bComplete || objective.m_bFailed))
			return "destroyed exact guard receipt conflicts with the still-active HVT objective";
		if (objective.m_sMissionInstanceId != mission.m_sInstanceId
			|| objective.m_eType != HST_EMissionObjectiveType.HST_OBJECTIVE_KILL_TARGET
			|| objective.m_sTargetId != "hvt")
			return "exact assassination guard HVT objective conflicts";
		if (!objective.m_sRuntimePrimitive.IsEmpty()
			&& objective.m_sRuntimePrimitive != "kill_hvt")
			return "exact assassination guard HVT objective primitive conflicts";
		if (hvt.m_sMissionInstanceId != mission.m_sInstanceId
			|| hvt.m_sRole != "hvt" || hvt.m_sKind != "character"
			|| hvt.m_sAssetId.IsEmpty())
			return "exact assassination guard HVT asset conflicts";
		// The HVT is mission-objective authority, not a member or asset of the
		// guard projection. It is observed here but never claimed or normalized.
		if (hvt.m_sOperationId == mission.m_sOperationId
			|| hvt.m_sManifestId == mission.m_sManifestId
			|| !hvt.m_sManifestSlotId.IsEmpty()
			|| !hvt.m_sConvoyElementId.IsEmpty())
			return "exact assassination guard improperly claims the HVT asset";
		if (IsZeroVector(hvt.m_vSourcePosition)
			|| !PositionsMatch(operation.m_vTacticalTargetPosition, hvt.m_vSourcePosition, 1.0))
			return "exact assassination guard frozen HVT assignment anchor conflicts";
		return "";
	}

	protected string ValidateFrozenManifest(
		HST_ActiveMissionState mission,
		HST_ForceManifestState manifest,
		HST_ActiveGroupState group)
	{
		if (!mission || !manifest || !manifest.m_bFrozen || manifest.m_sManifestHash.IsEmpty())
			return "exact assassination guard frozen manifest is incomplete";
		HST_ForcePlanningIntegrityService integrity = new HST_ForcePlanningIntegrityService();
		if (manifest.m_sManifestHash != integrity.BuildManifestHash(manifest))
			return "exact assassination guard frozen manifest hash conflicts";
		if (manifest.m_sPolicyId != HST_MissionGuardOperationService.EXACT_POLICY_ID
			|| manifest.m_sForceKind != HST_MissionGuardOperationService.EXACT_FORCE_KIND
			|| manifest.m_sIntentId != HST_MissionGuardOperationService.EXACT_INTENT_ID
			|| manifest.m_sFactionRole != "enemy" || !manifest.m_sSourceZoneId.IsEmpty()
			|| !manifest.m_sQuoteId.IsEmpty() || !manifest.m_sCommandRequestId.IsEmpty()
			|| manifest.m_sCatalogVersion != HST_ForceCatalogService.CATALOG_VERSION)
			return "exact assassination guard manifest policy or force kind conflicts";
		if (manifest.m_iRequestedMemberCount < 1 || manifest.m_iRequestedMemberCount > 32
			|| manifest.m_iRequestedMemberCount != manifest.m_iAcceptedMemberCount
			|| manifest.m_iAcceptedMemberCount != manifest.m_aMembers.Count())
			return "exact assassination guard member roster shape conflicts";
		if (manifest.m_iRequestedVehicleCount != 0 || manifest.m_iAcceptedVehicleCount != 0
			|| manifest.m_aVehicles.Count() != 0 || manifest.m_aAssets.Count() != 0)
			return "exact assassination guard must contain no vehicle or projected asset authority";
		if (manifest.m_iMoneyCost != 0 || manifest.m_iHRCost != 0
			|| manifest.m_iEquipmentCost != 0 || manifest.m_iAttackResourceCost != 0
			|| manifest.m_iSupportResourceCost != 0)
			return "exact assassination guard must contain no resource cost authority";
		if (manifest.m_aGroups.Count() != 1 || !manifest.m_aGroups[0])
			return "exact assassination guard requires one frozen group root";

		HST_ForceManifestGroupState root = manifest.m_aGroups[0];
		if (root.m_sElementId != HST_MissionGuardOperationService.BuildGroupRootId(mission.m_sInstanceId)
			|| root.m_sCatalogEntryId.IsEmpty()
			|| root.m_sPrefab.IsEmpty() || !root.m_sPrefab.Contains("NotSpawned")
			|| root.m_sRole.IsEmpty() || root.m_iOrdinal != 0 || !root.m_bRequired
			|| root.m_iExpectedMemberCount != manifest.m_iAcceptedMemberCount
			|| manifest.m_sGroupPrefab != root.m_sPrefab
			|| (group && group.m_sPrefab != root.m_sPrefab))
			return "exact assassination guard one-root execution contract conflicts";
		for (int memberIndex = 0; memberIndex < manifest.m_aMembers.Count(); memberIndex++)
		{
			HST_ForceManifestMemberState member = manifest.m_aMembers[memberIndex];
			if (!member || member.m_sSlotId.IsEmpty() || member.m_sCatalogSlotId.IsEmpty()
				|| member.m_sSlotId != HST_MissionGuardOperationService.BuildMemberSlotId(
					mission.m_sInstanceId,
					memberIndex)
				|| member.m_sGroupElementId != root.m_sElementId
				|| member.m_sPrefab.IsEmpty() || member.m_sRole.IsEmpty()
				|| member.m_iOrdinal != memberIndex
				|| CountManifestMembers(manifest, member.m_sSlotId) != 1)
				return "exact assassination guard member roster conflicts";
			if (member.m_iMoneyCost != 0 || member.m_iHRCost != 0
				|| member.m_iEquipmentCost != 0 || !member.m_sAssignedVehicleSlotId.IsEmpty()
				|| !member.m_sSeatRole.IsEmpty() || member.m_iSeatIndex != -1)
				return "exact assassination guard member cost or vehicle assignment conflicts";
		}
		return ValidateCurrentCatalogRoster(mission, manifest);
	}

	protected string ValidateCurrentCatalogRoster(
		HST_ActiveMissionState mission,
		HST_ForceManifestState manifest)
	{
		if (!mission || !manifest || manifest.m_aGroups.Count() != 1 || !manifest.m_aGroups[0])
			return "exact assassination guard catalog root is missing";
		HST_ForceManifestGroupState root = manifest.m_aGroups[0];
		HST_ForceCatalogService catalog = new HST_ForceCatalogService();
		HST_ForceGroupCatalogEntry catalogGroup;
		int matches;
		foreach (HST_ForceGroupCatalogEntry candidate : catalog.BuildGroupCatalog(manifest.m_sFactionKey))
		{
			if (!candidate || candidate.m_sEntryId != root.m_sCatalogEntryId)
				continue;
			catalogGroup = candidate;
			matches++;
		}
		if (matches != 1 || !catalogGroup)
			return "exact assassination guard catalog execution root conflicts";
		int catalogMemberCount = catalogGroup.m_aMemberSlots.Count();
		bool rootIdentityExact = root.m_sElementId
			== HST_MissionGuardOperationService.BuildGroupRootId(mission.m_sInstanceId);
		bool rootCatalogExact = root.m_sPrefab == catalogGroup.m_sExecutionPrefab
			&& manifest.m_sGroupPrefab == catalogGroup.m_sExecutionPrefab
			&& root.m_sRole == catalogGroup.m_sRole;
		bool rootShapeExact = root.m_iOrdinal == 0 && root.m_bRequired
			&& root.m_iExpectedMemberCount == catalogMemberCount;
		bool manifestCountsExact = manifest.m_iRequestedMemberCount == catalogMemberCount
			&& manifest.m_iAcceptedMemberCount == catalogMemberCount
			&& manifest.m_aMembers.Count() == catalogMemberCount;
		if (!rootIdentityExact || !rootCatalogExact || !rootShapeExact || !manifestCountsExact)
			return "exact assassination guard catalog execution root conflicts";
		for (int memberIndex = 0; memberIndex < catalogMemberCount; memberIndex++)
		{
			HST_ForceGroupCatalogSlot catalogSlot = catalogGroup.m_aMemberSlots[memberIndex];
			HST_ForceManifestMemberState member = manifest.m_aMembers[memberIndex];
			if (!catalogSlot || !member)
				return "exact assassination guard ordered catalog member roster conflicts";
			bool memberIdentityExact = member.m_sSlotId
				== HST_MissionGuardOperationService.BuildMemberSlotId(mission.m_sInstanceId, memberIndex)
				&& member.m_sCatalogSlotId == catalogGroup.m_sEntryId + "/" + catalogSlot.m_sSlotId
				&& member.m_sGroupElementId == root.m_sElementId;
			bool memberCatalogExact = member.m_sPrefab == catalogSlot.m_sPrefab
				&& member.m_sRole == catalogSlot.m_sRole
				&& member.m_bRequired == catalogSlot.m_bRequired;
			bool memberSeatless = member.m_sAssignedVehicleSlotId.IsEmpty()
				&& member.m_sSeatRole.IsEmpty() && member.m_iSeatIndex == -1;
			bool memberOrderAndCostExact = member.m_iOrdinal == memberIndex
				&& member.m_iMoneyCost == 0 && member.m_iHRCost == 0
				&& member.m_iEquipmentCost == 0;
			if (!memberIdentityExact || !memberCatalogExact || !memberSeatless
				|| !memberOrderAndCostExact)
				return "exact assassination guard ordered catalog member roster conflicts";
		}
		return "";
	}

	protected string ValidateLifecycle(
		HST_ActiveMissionState mission,
		HST_OperationRecordState operation,
		HST_ActiveGroupState group)
	{
		if (operation.m_iProjectionContractVersion != HST_MissionGuardOperationService.EXACT_PROJECTION_CONTRACT_VERSION
			|| operation.m_sAssignmentKind != HST_MissionGuardOperationService.ASSIGNMENT_KIND
			|| operation.m_sRecallPolicyId != HST_MissionGuardOperationService.RECALL_POLICY_ID
			|| operation.m_sSettlementPolicyId != HST_MissionGuardOperationService.SETTLEMENT_POLICY_ID)
			return "exact assassination guard projection or assignment policy conflicts";
		if (operation.m_eDutyState != HST_EOperationDutyState.HST_OPERATION_DUTY_ON_STATION
			&& operation.m_eDutyState != HST_EOperationDutyState.HST_OPERATION_DUTY_SETTLED)
			return "exact assassination guard stationary duty conflicts";
		bool settledDuty = operation.m_eSettlementState
			== HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_SETTLED;
		if ((!settledDuty && operation.m_eResumeDutyState
				!= HST_EOperationDutyState.HST_OPERATION_DUTY_ON_STATION)
			|| (settledDuty && operation.m_eResumeDutyState
				!= HST_EOperationDutyState.HST_OPERATION_DUTY_SETTLED))
			return "exact assassination guard resume duty conflicts";
		if (operation.m_eEngagementMode == HST_EOperationEngagementMode.HST_OPERATION_ENGAGEMENT_UNKNOWN)
			return "exact assassination guard engagement authority is unknown";
		if (!operation.m_sCurrentRouteId.IsEmpty() || !operation.m_sRouteContractHash.IsEmpty()
			|| operation.m_iRouteVersion != 0 || operation.m_iRouteWaypointIndex != -1
			|| operation.m_iRouteLapCount != 0 || operation.m_iRouteLegSequence != 0
			|| operation.m_iRouteLoopStartedAtSecond != 0
			|| operation.m_iRouteLoopCompletedAtSecond != 0
			|| !IsZeroVector(operation.m_vRouteStartPosition)
			|| !IsZeroVector(operation.m_vRouteEndPosition)
			|| operation.m_fRouteTotalDistanceMeters != 0
			|| operation.m_fRouteProgressMeters != 0
			|| operation.m_fStrategicSpeedMetersPerSecond != 0
			|| (group && !group.m_sRouteId.IsEmpty()))
			return "exact assassination guard must remain stationary and route-free";
		if (IsZeroVector(operation.m_vAssignmentPosition)
			|| IsZeroVector(operation.m_vTacticalTargetPosition)
			|| Distance2D(operation.m_vAssignmentPosition, operation.m_vTacticalTargetPosition) < 6.0
			|| Distance2D(operation.m_vAssignmentPosition, operation.m_vTacticalTargetPosition) > 30.0)
			return "exact assassination guard stationary assignment position conflicts";
		if (operation.m_sOriginZoneId != operation.m_sAssignmentZoneId
			|| !PositionsMatch(operation.m_vOriginPosition, operation.m_vAssignmentPosition, 1.0))
			return "exact assassination guard immutable origin anchor conflicts";
		if (group && (!PositionsMatch(group.m_vSourcePosition, operation.m_vStrategicPosition, 1.0)
			|| !PositionsMatch(group.m_vTargetPosition, operation.m_vAssignmentPosition, 1.0)))
			return "exact assassination guard group anchor semantics conflict";
		if (group && operation.m_eMaterializationState == HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_VIRTUAL
			&& !PositionsMatch(group.m_vPosition, operation.m_vStrategicPosition, 1.0))
			return "virtual exact assassination guard position conflicts with its strategic anchor";
		if (operation.m_eSettlementState == HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_SETTLED)
			return "";
		if (operation.m_eSettlementState != HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_OPEN
			|| operation.m_eTerminalResult != HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_NONE
			|| !operation.m_sSettlementId.IsEmpty())
			return "open exact assassination guard contains terminal authority";
		if (mission.m_eStatus != HST_EMissionStatus.HST_MISSION_ACTIVE)
			return "open exact assassination guard mission is not active";
		if (operation.m_eMaterializationState == HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_UNKNOWN
			|| operation.m_eMaterializationState == HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_RETIRING
			|| operation.m_eMaterializationState == HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_RETIRED)
			return "open exact assassination guard materialization state conflicts";
		bool liveState = operation.m_eMaterializationState == HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_PHYSICAL
			|| operation.m_eMaterializationState == HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_DEMATERIALIZING;
		if (liveState && operation.m_ePositionAuthority != HST_EOperationPositionAuthority.HST_OPERATION_POSITION_LIVE)
			return "physical exact assassination guard does not own live position authority";
		if (!liveState && operation.m_ePositionAuthority != HST_EOperationPositionAuthority.HST_OPERATION_POSITION_STRATEGIC)
			return "virtual exact assassination guard does not own strategic position authority";
		return "";
	}

	protected string ValidateBatchSlotBijection(
		HST_ForceManifestState manifest,
		HST_ForceSpawnResultState batch)
	{
		if (!manifest || !batch || manifest.m_aGroups.Count() != 1 || !manifest.m_aGroups[0])
			return "exact assassination guard batch or manifest is incomplete";
		int expectedSlotCount = 1 + manifest.m_aMembers.Count();
		if (batch.m_iExpectedSlotCount != expectedSlotCount
			|| batch.m_aSlotResults.Count() != expectedSlotCount)
			return "exact assassination guard batch slot cardinality conflicts";
		HST_ForceManifestGroupState root = manifest.m_aGroups[0];
		if (CountBatchSlots(batch, root.m_sElementId, HST_ForceSpawnQueueService.SLOT_KIND_GROUP) != 1)
			return "exact assassination guard root slot conflicts";
		foreach (HST_ForceManifestMemberState member : manifest.m_aMembers)
		{
			if (!member || CountBatchSlots(batch, member.m_sSlotId, HST_ForceSpawnQueueService.SLOT_KIND_MEMBER) != 1)
				return "exact assassination guard member slot bijection conflicts";
		}
		foreach (HST_ForceSpawnSlotResultState slot : batch.m_aSlotResults)
		{
			if (!slot || slot.m_sSlotId.IsEmpty()
				|| (slot.m_sSlotKind != HST_ForceSpawnQueueService.SLOT_KIND_GROUP
					&& slot.m_sSlotKind != HST_ForceSpawnQueueService.SLOT_KIND_MEMBER)
				|| slot.m_sProjectionId != batch.m_sProjectionId)
				return "exact assassination guard batch contains a foreign or malformed slot";
			if (slot.m_sSlotKind == HST_ForceSpawnQueueService.SLOT_KIND_GROUP
				&& slot.m_sSlotId != root.m_sElementId)
				return "exact assassination guard batch contains a foreign group root";
			if (slot.m_sSlotKind == HST_ForceSpawnQueueService.SLOT_KIND_MEMBER
				&& !manifest.FindMemberSlot(slot.m_sSlotId))
				return "exact assassination guard batch contains a foreign member slot";
			bool memberSlot = slot.m_sSlotKind == HST_ForceSpawnQueueService.SLOT_KIND_MEMBER;
			if (slot.m_bCasualtyConfirmed)
			{
				if (!memberSlot || slot.m_eStatus != HST_EForceSpawnSlotStatus.HST_FORCE_SLOT_RETIRED
					|| !slot.m_bEverAlive || slot.m_iCasualtyAtSecond < batch.m_iCreatedAtSecond)
					return "exact assassination guard casualty tombstone conflicts";
				continue;
			}
			bool terminalBatch = batch.m_eStatus == HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_FAILED_FINAL
				|| batch.m_eStatus == HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_CANCELLED;
			if (!terminalBatch && (slot.m_eStatus == HST_EForceSpawnSlotStatus.HST_FORCE_SLOT_RETIRED
				|| slot.m_eStatus == HST_EForceSpawnSlotStatus.HST_FORCE_SLOT_FAILED_FINAL
				|| slot.m_eStatus == HST_EForceSpawnSlotStatus.HST_FORCE_SLOT_CANCELLED))
				return "open exact assassination guard contains an unproven terminal slot";
			if (memberSlot && slot.m_eStatus == HST_EForceSpawnSlotStatus.HST_FORCE_SLOT_REGISTERED
				&& !slot.m_bEverAlive)
				return "registered exact assassination guard member lacks living evidence";
		}
		return "";
	}

	protected string ValidateRuntime(
		HST_ActiveMissionState mission,
		HST_OperationRecordState operation,
		HST_ForceManifestState manifest,
		HST_ForceSpawnResultState batch,
		HST_ActiveGroupState group)
	{
		if (group.m_sSpawnFallbackMode != HST_MissionGuardOperationService.EXACT_GROUP_MODE
			|| group.m_sCompositionRequestId != manifest.m_sManifestId
			|| group.m_sCompositionIntentId != HST_MissionGuardOperationService.EXACT_INTENT_ID
			|| group.m_sMissionAssetId != "" || group.m_bQRF
			|| !group.m_sSupportRequestId.IsEmpty() || !group.m_sEnemyOrderId.IsEmpty()
			|| !group.m_sConvoyElementId.IsEmpty() || !group.m_sGarrisonZoneId.IsEmpty()
			|| !group.m_sQRFInstanceId.IsEmpty())
			return "exact assassination guard group ownership conflicts";
		if (group.m_iVehicleCount != 0 || group.m_iOriginalVehicleCount != 0
			|| group.m_iSurvivorVehicleCount != 0
			|| group.m_iCompositionVehicleCount != 0
			|| group.m_iCompositionArmedVehicleCount != 0
			|| !group.m_sVehiclePrefab.IsEmpty())
			return "exact assassination guard group must contain no vehicles";
		if (group.m_iCompositionCost != 0 || group.m_iCompositionManpower != manifest.m_iAcceptedMemberCount)
			return "exact assassination guard group frozen composition conflicts";
		if (group.m_iOriginalInfantryCount != manifest.m_iAcceptedMemberCount)
			return "exact assassination guard original roster count conflicts";

		HST_ForceSpawnQueueService queue = new HST_ForceSpawnQueueService();
		int living;
		bool processLocal = operation.m_eMaterializationState == HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_PHYSICAL
			|| operation.m_eMaterializationState == HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_DEMATERIALIZING;
		if (processLocal)
		{
			if (batch.m_eStatus != HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_SUCCEEDED
				|| batch.m_bStrategicProjectionHeld)
				return "physical exact assassination guard batch lifecycle conflicts";
			living = queue.CountDurableLivingMemberSlots(batch);
		}
		else if (operation.m_eSettlementState == HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_OPEN)
		{
			if (batch.m_eStatus != HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_PENDING
				&& batch.m_eStatus != HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_SUCCEEDED)
				return "virtual exact assassination guard batch lifecycle conflicts";
			if (batch.m_eStatus == HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_PENDING
				&& !batch.m_bStrategicProjectionHeld)
				return "virtual exact assassination guard lacks strategic roster hold";
			if (batch.m_bStrategicProjectionHeld)
				living = queue.CountStrategicLivingMemberSlots(batch);
			else
				living = queue.CountDurableLivingMemberSlots(batch);
		}
		else
		{
			living = Math.Max(0, group.m_iDurableLivingInfantryCount);
		}
		if (living < 0 || living > manifest.m_iAcceptedMemberCount)
			return "exact assassination guard survivor roster conflicts";
		if (operation.m_eSettlementState == HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_OPEN
			&& (group.m_iInfantryCount != living
				|| group.m_iLastSeenAliveCount != living
				|| group.m_iSurvivorInfantryCount != living
				|| group.m_iDurableLivingInfantryCount != living
				|| operation.m_iLastVirtualFriendlyCount != living))
			return "exact assassination guard survivor counts conflict with durable slots";
		return "";
	}

	protected string ValidateSettlement(
		HST_ActiveMissionState mission,
		HST_OperationRecordState operation,
		HST_ForceSpawnResultState batch,
		HST_ActiveGroupState group)
	{
		if (operation.m_eSettlementState == HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_OPEN)
		{
			if (!mission.m_sSettlementId.IsEmpty())
				return "open exact assassination guard contains a mission settlement receipt";
			return "";
		}
		if (operation.m_eSettlementState != HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_SETTLED
			|| operation.m_eTerminalResult == HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_NONE
			|| operation.m_eTerminalResult == HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_UNKNOWN)
			return "exact assassination guard settlement state conflicts";
		string expectedSettlementId = HST_OperationService.BuildSettlementId(
			operation.m_sOperationId,
			HST_MissionGuardOperationService.SETTLEMENT_KIND);
		if (operation.m_sSettlementId.IsEmpty()
			|| operation.m_sSettlementId != expectedSettlementId
			|| mission.m_sSettlementId != operation.m_sSettlementId)
			return "exact assassination guard terminal receipt conflicts";
		if (operation.m_eDutyState != HST_EOperationDutyState.HST_OPERATION_DUTY_SETTLED
			|| operation.m_eMaterializationState != HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_RETIRED
			|| operation.m_ePositionAuthority != HST_EOperationPositionAuthority.HST_OPERATION_POSITION_STRATEGIC)
			return "settled exact assassination guard lifecycle residue conflicts";
		bool activeGuardDestroyed = mission.m_eStatus == HST_EMissionStatus.HST_MISSION_ACTIVE
			&& operation.m_eTerminalResult == HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_DESTROYED;
		if (activeGuardDestroyed && operation.m_iLastVirtualFriendlyCount != 0)
			return "destroyed exact assassination guard terminal receipt retains survivors";
		if (mission.m_eStatus == HST_EMissionStatus.HST_MISSION_ACTIVE && !activeGuardDestroyed)
			return "settled exact assassination guard terminal result conflicts with an active HVT mission";
		if (batch && batch.m_eStatus != HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_CANCELLED
			&& batch.m_eStatus != HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_FAILED_FINAL)
			return "settled exact assassination guard batch is not terminal";
		if (group && (group.m_bSpawnedEntity || !group.m_sRuntimeEntityId.IsEmpty()
			|| group.m_iSpawnedAgentCount > 0 || group.m_iAssignedWaypointCount > 0))
			return "settled exact assassination guard retains process-local group authority";
		return "";
	}

	protected void NormalizeValidAggregate(
		HST_ActiveMissionState mission,
		HST_OperationRecordState operation,
		HST_ForceSpawnResultState batch,
		HST_ActiveGroupState group)
	{
		if (!mission || !operation)
			return;
		if (operation.m_eSettlementState == HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_SETTLED)
		{
			NormalizeSettledProcessResidue(batch, group);
			return;
		}
		if (!batch || !group)
			return;

		int restoreSecond = Math.Max(0, m_SaveData.m_iElapsedSeconds);
		bool processLocal = operation.m_eMaterializationState == HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_PHYSICAL
			|| operation.m_eMaterializationState == HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_DEMATERIALIZING
			|| operation.m_eMaterializationState == HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_MATERIALIZING;
		if (processLocal && !IsZeroVector(group.m_vPosition))
			operation.m_vStrategicPosition = group.m_vPosition;
		if (!processLocal && operation.m_eMaterializationState == HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_VIRTUAL
			&& batch.m_bStrategicProjectionHeld
			&& !BatchHasProcessResidue(batch)
			&& !GroupHasProcessResidue(group))
			return;

		operation.m_eMaterializationState = HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_VIRTUAL;
		operation.m_ePositionAuthority = HST_EOperationPositionAuthority.HST_OPERATION_POSITION_STRATEGIC;
		operation.m_iMaterializationStateEnteredAtSecond = restoreSecond;
		operation.m_iStrategicLastUpdateSecond = restoreSecond;
		operation.m_iVirtualCombatLastStepSecond = restoreSecond;
		operation.m_iLastProgressAtSecond = restoreSecond;
		operation.m_sLastProjectionReason = "schema-55 restore folded exact mission guard into held strategic survivor authority";
		NormalizeBatchForStrategicHold(batch, restoreSecond);
		HST_ForceSpawnQueueService queue = new HST_ForceSpawnQueueService();
		int living = queue.CountStrategicLivingMemberSlots(batch);
		operation.m_iLastVirtualFriendlyCount = living;
		NormalizeGroupForStrategicHold(operation, group, living);
		operation.m_iRevision++;
	}

	protected void NormalizeBatchForStrategicHold(
		HST_ForceSpawnResultState batch,
		int restoreSecond)
	{
		if (!batch)
			return;
		bool wasProcessLocal = batch.m_eStatus != HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_PENDING
			|| !batch.m_bStrategicProjectionHeld;
		batch.m_sNativeGroupId = "";
		batch.m_eStatus = HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_PENDING;
		batch.m_bStrategicProjectionHeld = true;
		batch.m_bCancelRequested = false;
		batch.m_sTerminalReason = "";
		batch.m_sLastFailureReason = "";
		if (wasProcessLocal)
			batch.m_iReprojectionCount++;
		batch.m_iStrategicHoldSinceSecond = restoreSecond;
		batch.m_iNextAttemptSecond = 0;
		batch.m_iUpdatedAtSecond = restoreSecond;
		batch.m_iCompletedAtSecond = 0;
		batch.m_iAttemptGeneration++;
		batch.m_iLifecycleRevision++;
		batch.m_iLastLifecycleSecond = restoreSecond;
		foreach (HST_ForceSpawnSlotResultState slot : batch.m_aSlotResults)
		{
			if (!slot)
				continue;
			slot.m_sSpawnedPrefab = "";
			slot.m_sEntityId = "";
			slot.m_sAssignedVehicleEntityId = "";
			slot.m_sNativeGroupId = "";
			slot.m_bAliveVerified = false;
			slot.m_bFactionVerified = false;
			slot.m_bGroupVerified = false;
			slot.m_bGameMasterVerified = false;
			slot.m_bProjectionVerified = false;
			slot.m_bSeatVerified = false;
			slot.m_iUpdatedAtSecond = restoreSecond;
			if (slot.m_eStatus != HST_EForceSpawnSlotStatus.HST_FORCE_SLOT_RETIRED
				|| !slot.m_bCasualtyConfirmed)
				slot.m_eStatus = HST_EForceSpawnSlotStatus.HST_FORCE_SLOT_QUEUED;
		}
	}

	protected void NormalizeGroupForStrategicHold(
		HST_OperationRecordState operation,
		HST_ActiveGroupState group,
		int living)
	{
		if (!operation || !group)
			return;
		group.m_bSpawnedEntity = false;
		group.m_bSpawnAttempted = false;
		group.m_bSpawnCompleted = false;
		group.m_sRuntimeEntityId = "";
		group.m_iSpawnedAgentCount = 0;
		group.m_iAssignedWaypointCount = 0;
		group.m_sRuntimeStatus = "mission_guard_virtual";
		group.m_sSpawnFallbackMode = HST_MissionGuardOperationService.EXACT_GROUP_MODE;
		group.m_vPosition = operation.m_vStrategicPosition;
		group.m_vSourcePosition = operation.m_vStrategicPosition;
		group.m_vTargetPosition = operation.m_vAssignmentPosition;
		group.m_iInfantryCount = Math.Max(0, living);
		group.m_iLastSeenAliveCount = Math.Max(0, living);
		group.m_iSurvivorInfantryCount = Math.Max(0, living);
		group.m_iDurableLivingInfantryCount = Math.Max(0, living);
		group.m_iLifecycleRevision++;
	}

	protected void NormalizeSettledProcessResidue(
		HST_ForceSpawnResultState batch,
		HST_ActiveGroupState group)
	{
		if (batch)
		{
			batch.m_sNativeGroupId = "";
			foreach (HST_ForceSpawnSlotResultState slot : batch.m_aSlotResults)
			{
				if (!slot)
					continue;
				slot.m_sEntityId = "";
				slot.m_sAssignedVehicleEntityId = "";
				slot.m_sNativeGroupId = "";
				slot.m_bAliveVerified = false;
				slot.m_bFactionVerified = false;
				slot.m_bGroupVerified = false;
				slot.m_bGameMasterVerified = false;
				slot.m_bProjectionVerified = false;
				slot.m_bSeatVerified = false;
			}
		}
		if (!group)
			return;
		group.m_bSpawnedEntity = false;
		group.m_bSpawnAttempted = false;
		group.m_sRuntimeEntityId = "";
		group.m_iSpawnedAgentCount = 0;
		group.m_iAssignedWaypointCount = 0;
	}

	protected bool BatchHasProcessResidue(HST_ForceSpawnResultState batch)
	{
		if (!batch)
			return false;
		if (!batch.m_sNativeGroupId.IsEmpty())
			return true;
		foreach (HST_ForceSpawnSlotResultState slot : batch.m_aSlotResults)
		{
			if (!slot)
				continue;
			if (!slot.m_sEntityId.IsEmpty() || !slot.m_sAssignedVehicleEntityId.IsEmpty()
				|| !slot.m_sNativeGroupId.IsEmpty() || slot.m_bAliveVerified
				|| slot.m_bFactionVerified || slot.m_bGroupVerified
				|| slot.m_bGameMasterVerified || slot.m_bProjectionVerified
				|| slot.m_bSeatVerified)
				return true;
		}
		return false;
	}

	protected bool GroupHasProcessResidue(HST_ActiveGroupState group)
	{
		return group && (group.m_bSpawnedEntity || group.m_bSpawnAttempted
			|| !group.m_sRuntimeEntityId.IsEmpty() || group.m_iSpawnedAgentCount > 0
			|| group.m_iAssignedWaypointCount > 0);
	}

	protected void QuarantineAggregate(
		HST_ActiveMissionState mission,
		HST_OperationRecordState operation,
		HST_ForceSpawnResultState batch,
		HST_ActiveGroupState group,
		string reason)
	{
		if (reason.IsEmpty())
			reason = "schema-55 exact assassination guard authority conflict";
		if (mission && (HST_MissionGuardOperationService.IsExactMission(mission)
			|| HST_MissionGuardOperationService.IsQuarantinedMission(mission)
			|| MissionStronglyClaimsOperation(mission, operation)))
		{
			mission.m_iOperationContractVersion = HST_MissionGuardOperationService.QUARANTINED_CONTRACT_VERSION;
			mission.m_sRuntimeFailureReason = reason;
		}
		if (operation && operation.m_eType == HST_EOperationType.HST_OPERATION_TYPE_MISSION_GUARD)
		{
			operation.m_iContractVersion = HST_MissionGuardOperationService.QUARANTINED_CONTRACT_VERSION;
			if (operation.m_eSettlementState == HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_OPEN)
			{
				operation.m_eMaterializationState = HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_VIRTUAL;
				operation.m_ePositionAuthority = HST_EOperationPositionAuthority.HST_OPERATION_POSITION_STRATEGIC;
			}
			operation.m_sLastProjectionReason = reason;
			operation.m_iRevision++;
		}
		if (BatchClaimsAuthority(batch, mission, operation))
			HoldBatch(batch, reason);
		if (GroupClaimsAuthority(group, mission, operation))
			HoldGroup(group, reason);
		HoldStrongClaimants(mission, operation, reason);
	}

	protected int PreserveOrphanClaimants(array<string> validatedOperationIds)
	{
		int quarantinedCount;
		foreach (HST_ActiveMissionState mission : m_SaveData.m_aActiveMissions)
		{
			if (!mission || (!HST_MissionGuardOperationService.IsExactMission(mission)
				&& !HST_MissionGuardOperationService.IsQuarantinedMission(mission)))
				continue;
			if (validatedOperationIds.Contains(mission.m_sOperationId))
				continue;
			HST_OperationRecordState operation = FindUniqueOperation(mission.m_sOperationId);
			HST_ForceSpawnResultState batch;
			HST_ActiveGroupState group;
			if (operation)
			{
				batch = FindUniqueBatch(operation.m_sSpawnResultId);
				group = FindUniqueGroup(operation.m_sGroupId);
			}
			string reason = "exact assassination guard mission has no validated reciprocal authority graph";
			if (HST_MissionGuardOperationService.IsQuarantinedMission(mission))
				reason = "schema-55 exact assassination guard remained quarantined after restore";
			QuarantineAggregate(
				mission,
				operation,
				batch,
				group,
				reason);
			quarantinedCount++;
		}
		foreach (HST_OperationRecordState operation : m_SaveData.m_aOperations)
		{
			if (!operation || operation.m_eType != HST_EOperationType.HST_OPERATION_TYPE_MISSION_GUARD
				|| validatedOperationIds.Contains(operation.m_sOperationId))
				continue;
			HST_ActiveMissionState mission = FindUniqueMission(operation.m_sMissionInstanceId);
			QuarantineAggregate(
				mission,
				operation,
				FindUniqueBatch(operation.m_sSpawnResultId),
				FindUniqueGroup(operation.m_sGroupId),
				"exact assassination guard operation has no validated reciprocal mission graph");
			quarantinedCount++;
		}
		foreach (HST_ActiveGroupState group : m_SaveData.m_aActiveGroups)
		{
			if (!IsSchema55MissionGuardGroupClaimant(m_SaveData, group))
				continue;
			HST_OperationRecordState operation = FindUniqueOperation(group.m_sOperationId);
			if (operation && validatedOperationIds.Contains(operation.m_sOperationId))
				continue;
			HoldGroup(group, "exact assassination guard group has no validated reciprocal authority graph");
		}
		return quarantinedCount;
	}

	protected void HoldStrongClaimants(
		HST_ActiveMissionState mission,
		HST_OperationRecordState operation,
		string reason)
	{
		foreach (HST_ForceSpawnResultState batch : m_SaveData.m_aForceSpawnResults)
		{
			if (BatchClaimsAuthority(batch, mission, operation))
				HoldBatch(batch, reason);
		}
		foreach (HST_ActiveGroupState group : m_SaveData.m_aActiveGroups)
		{
			if (GroupClaimsAuthority(group, mission, operation))
				HoldGroup(group, reason);
		}
	}

	protected void HoldBatch(HST_ForceSpawnResultState batch, string reason)
	{
		if (!batch)
			return;
		batch.m_bStrategicProjectionHeld = true;
		batch.m_bCancelRequested = true;
		batch.m_sLastFailureReason = reason;
	}

	protected void HoldGroup(HST_ActiveGroupState group, string reason)
	{
		if (!group)
			return;
		group.m_sSpawnFallbackMode = HST_MissionGuardOperationService.QUARANTINE_STATUS;
		group.m_sRuntimeStatus = HST_MissionGuardOperationService.QUARANTINE_STATUS;
		group.m_sSpawnFailureReason = reason;
		group.m_iLifecycleRevision++;
	}

	protected bool MissionStronglyClaimsOperation(
		HST_ActiveMissionState mission,
		HST_OperationRecordState operation)
	{
		if (!mission || !operation || operation.m_eType != HST_EOperationType.HST_OPERATION_TYPE_MISSION_GUARD
			|| operation.m_sOperationId.IsEmpty()
			|| mission.m_sOperationId != operation.m_sOperationId
			|| mission.m_sInstanceId != operation.m_sMissionInstanceId)
			return false;
		return (!mission.m_sManifestId.IsEmpty() && mission.m_sManifestId == operation.m_sManifestId)
			|| (!mission.m_sSpawnResultId.IsEmpty() && mission.m_sSpawnResultId == operation.m_sSpawnResultId);
	}

	protected bool BatchClaimsAuthority(
		HST_ForceSpawnResultState batch,
		HST_ActiveMissionState mission,
		HST_OperationRecordState operation)
	{
		if (!batch)
			return false;
		if (operation && operation.m_eType == HST_EOperationType.HST_OPERATION_TYPE_MISSION_GUARD
			&& !operation.m_sOperationId.IsEmpty()
			&& batch.m_sOperationId == operation.m_sOperationId)
		{
			return (!operation.m_sSpawnResultId.IsEmpty() && batch.m_sResultId == operation.m_sSpawnResultId)
				|| (!operation.m_sManifestId.IsEmpty() && batch.m_sManifestId == operation.m_sManifestId)
				|| (!operation.m_sForceId.IsEmpty() && batch.m_sForceId == operation.m_sForceId)
				|| (!operation.m_sProjectionId.IsEmpty() && batch.m_sProjectionId == operation.m_sProjectionId);
		}
		if (!mission || mission.m_sOperationId.IsEmpty()
			|| batch.m_sOperationId != mission.m_sOperationId)
			return false;
		return (!mission.m_sSpawnResultId.IsEmpty() && batch.m_sResultId == mission.m_sSpawnResultId)
			|| (!mission.m_sManifestId.IsEmpty() && batch.m_sManifestId == mission.m_sManifestId);
	}

	protected bool GroupClaimsAuthority(
		HST_ActiveGroupState group,
		HST_ActiveMissionState mission,
		HST_OperationRecordState operation)
	{
		if (!group)
			return false;
		if (operation && operation.m_eType == HST_EOperationType.HST_OPERATION_TYPE_MISSION_GUARD
			&& !operation.m_sOperationId.IsEmpty()
			&& group.m_sOperationId == operation.m_sOperationId)
		{
			return (!operation.m_sGroupId.IsEmpty() && group.m_sGroupId == operation.m_sGroupId)
				|| (!operation.m_sManifestId.IsEmpty() && group.m_sManifestId == operation.m_sManifestId)
				|| (!operation.m_sSpawnResultId.IsEmpty() && group.m_sSpawnResultId == operation.m_sSpawnResultId)
				|| (!operation.m_sForceId.IsEmpty() && group.m_sForceId == operation.m_sForceId)
				|| (!operation.m_sProjectionId.IsEmpty() && group.m_sProjectionId == operation.m_sProjectionId);
		}
		if (!mission || mission.m_sOperationId.IsEmpty()
			|| group.m_sOperationId != mission.m_sOperationId
			|| group.m_sMissionInstanceId != mission.m_sInstanceId)
			return false;
		return (!mission.m_sManifestId.IsEmpty() && group.m_sManifestId == mission.m_sManifestId)
			|| (!mission.m_sSpawnResultId.IsEmpty() && group.m_sSpawnResultId == mission.m_sSpawnResultId);
	}

	protected HST_ActiveMissionState FindUniqueMission(string instanceId)
	{
		HST_ActiveMissionState match;
		if (instanceId.IsEmpty())
			return null;
		foreach (HST_ActiveMissionState mission : m_SaveData.m_aActiveMissions)
		{
			if (!mission || mission.m_sInstanceId != instanceId)
				continue;
			if (match)
				return null;
			match = mission;
		}
		return match;
	}

	protected HST_OperationRecordState FindUniqueOperation(string operationId)
	{
		HST_OperationRecordState match;
		if (operationId.IsEmpty())
			return null;
		foreach (HST_OperationRecordState operation : m_SaveData.m_aOperations)
		{
			if (!operation || operation.m_sOperationId != operationId)
				continue;
			if (match)
				return null;
			match = operation;
		}
		return match;
	}

	protected HST_ForceManifestState FindUniqueManifest(string manifestId)
	{
		HST_ForceManifestState match;
		if (manifestId.IsEmpty())
			return null;
		foreach (HST_ForceManifestState manifest : m_SaveData.m_aForceManifests)
		{
			if (!manifest || manifest.m_sManifestId != manifestId)
				continue;
			if (match)
				return null;
			match = manifest;
		}
		return match;
	}

	protected HST_ForceSpawnResultState FindUniqueBatch(string resultId)
	{
		HST_ForceSpawnResultState match;
		if (resultId.IsEmpty())
			return null;
		foreach (HST_ForceSpawnResultState batch : m_SaveData.m_aForceSpawnResults)
		{
			if (!batch || batch.m_sResultId != resultId)
				continue;
			if (match)
				return null;
			match = batch;
		}
		return match;
	}

	protected HST_ActiveGroupState FindUniqueGroup(string groupId)
	{
		HST_ActiveGroupState match;
		if (groupId.IsEmpty())
			return null;
		foreach (HST_ActiveGroupState group : m_SaveData.m_aActiveGroups)
		{
			if (!group || group.m_sGroupId != groupId)
				continue;
			if (match)
				return null;
			match = group;
		}
		return match;
	}

	protected HST_MissionObjectiveState FindUniqueHVTObjective(string missionInstanceId)
	{
		HST_MissionObjectiveState match;
		foreach (HST_MissionObjectiveState objective : m_SaveData.m_aMissionObjectives)
		{
			if (!objective || objective.m_sMissionInstanceId != missionInstanceId
				|| objective.m_eType != HST_EMissionObjectiveType.HST_OBJECTIVE_KILL_TARGET
				|| objective.m_sTargetId != "hvt")
				continue;
			if (match)
				return null;
			match = objective;
		}
		return match;
	}

	protected int CountHVTObjectives(string missionInstanceId)
	{
		int count;
		foreach (HST_MissionObjectiveState objective : m_SaveData.m_aMissionObjectives)
		{
			if (objective && objective.m_sMissionInstanceId == missionInstanceId
				&& objective.m_eType == HST_EMissionObjectiveType.HST_OBJECTIVE_KILL_TARGET
				&& objective.m_sTargetId == "hvt")
				count++;
		}
		return count;
	}

	protected HST_MissionAssetState FindUniqueHVTAsset(string missionInstanceId)
	{
		HST_MissionAssetState match;
		foreach (HST_MissionAssetState asset : m_SaveData.m_aMissionAssets)
		{
			if (!asset || asset.m_sMissionInstanceId != missionInstanceId
				|| asset.m_sRole != "hvt")
				continue;
			if (match)
				return null;
			match = asset;
		}
		return match;
	}

	protected int CountHVTAssets(string missionInstanceId)
	{
		int count;
		foreach (HST_MissionAssetState asset : m_SaveData.m_aMissionAssets)
		{
			if (asset && asset.m_sMissionInstanceId == missionInstanceId
				&& asset.m_sRole == "hvt")
				count++;
		}
		return count;
	}

	protected int CountMissionsByAnyIdentity(
		HST_ActiveMissionState expected,
		HST_OperationRecordState operation)
	{
		int count;
		foreach (HST_ActiveMissionState mission : m_SaveData.m_aActiveMissions)
		{
			if (!mission)
				continue;
			bool matches = mission.m_sInstanceId == expected.m_sInstanceId;
			if (!matches && !operation.m_sOperationId.IsEmpty())
				matches = mission.m_sOperationId == operation.m_sOperationId;
			if (!matches && !operation.m_sManifestId.IsEmpty())
				matches = mission.m_sManifestId == operation.m_sManifestId;
			if (!matches && !operation.m_sSpawnResultId.IsEmpty())
				matches = mission.m_sSpawnResultId == operation.m_sSpawnResultId;
			if (matches)
				count++;
		}
		return count;
	}

	protected int CountOperationsByAnyIdentity(
		HST_ActiveMissionState mission,
		HST_OperationRecordState expected)
	{
		int count;
		foreach (HST_OperationRecordState operation : m_SaveData.m_aOperations)
		{
			if (!operation)
				continue;
			bool matches = operation.m_sOperationId == expected.m_sOperationId;
			if (!matches && !mission.m_sInstanceId.IsEmpty())
				matches = operation.m_sMissionInstanceId == mission.m_sInstanceId;
			if (!matches && !mission.m_sManifestId.IsEmpty())
				matches = operation.m_sManifestId == mission.m_sManifestId;
			if (!matches && !mission.m_sSpawnResultId.IsEmpty())
				matches = operation.m_sSpawnResultId == mission.m_sSpawnResultId;
			if (!matches && !expected.m_sGroupId.IsEmpty())
				matches = operation.m_sGroupId == expected.m_sGroupId;
			if (!matches && !expected.m_sProjectionId.IsEmpty())
				matches = operation.m_sProjectionId == expected.m_sProjectionId;
			if (matches)
				count++;
		}
		return count;
	}

	protected int CountManifestsByAnyIdentity(
		HST_OperationRecordState operation,
		HST_ForceManifestState expected)
	{
		int count;
		foreach (HST_ForceManifestState manifest : m_SaveData.m_aForceManifests)
		{
			if (!manifest)
				continue;
			bool matches = manifest.m_sManifestId == expected.m_sManifestId;
			if (!matches && !operation.m_sOperationId.IsEmpty())
				matches = manifest.m_sOperationId == operation.m_sOperationId;
			if (matches)
				count++;
		}
		return count;
	}

	protected int CountBatchesByAnyIdentity(
		HST_OperationRecordState operation,
		HST_ForceSpawnResultState expected)
	{
		int count;
		foreach (HST_ForceSpawnResultState batch : m_SaveData.m_aForceSpawnResults)
		{
			if (!batch)
				continue;
			bool matches = batch.m_sResultId == expected.m_sResultId;
			if (!matches && !operation.m_sOperationId.IsEmpty())
				matches = batch.m_sOperationId == operation.m_sOperationId;
			if (!matches && !operation.m_sManifestId.IsEmpty())
				matches = batch.m_sManifestId == operation.m_sManifestId;
			if (!matches && !operation.m_sForceId.IsEmpty())
				matches = batch.m_sForceId == operation.m_sForceId;
			if (!matches && !operation.m_sProjectionId.IsEmpty())
				matches = batch.m_sProjectionId == operation.m_sProjectionId;
			if (matches)
				count++;
		}
		return count;
	}

	protected int CountBatchesClaimingOperation(HST_OperationRecordState operation)
	{
		int count;
		if (!operation)
			return count;
		foreach (HST_ForceSpawnResultState batch : m_SaveData.m_aForceSpawnResults)
		{
			if (!batch)
				continue;
			if ((!operation.m_sSpawnResultId.IsEmpty() && batch.m_sResultId == operation.m_sSpawnResultId)
				|| (!operation.m_sOperationId.IsEmpty() && batch.m_sOperationId == operation.m_sOperationId)
				|| (!operation.m_sManifestId.IsEmpty() && batch.m_sManifestId == operation.m_sManifestId)
				|| (!operation.m_sForceId.IsEmpty() && batch.m_sForceId == operation.m_sForceId)
				|| (!operation.m_sProjectionId.IsEmpty() && batch.m_sProjectionId == operation.m_sProjectionId))
				count++;
		}
		return count;
	}

	protected int CountGroupsByAnyIdentity(
		HST_OperationRecordState operation,
		HST_ActiveGroupState expected)
	{
		int count;
		foreach (HST_ActiveGroupState group : m_SaveData.m_aActiveGroups)
		{
			if (!group)
				continue;
			bool matches = group.m_sGroupId == expected.m_sGroupId;
			if (!matches && !operation.m_sOperationId.IsEmpty())
				matches = group.m_sOperationId == operation.m_sOperationId;
			if (!matches && !operation.m_sManifestId.IsEmpty())
				matches = group.m_sManifestId == operation.m_sManifestId;
			if (!matches && !operation.m_sSpawnResultId.IsEmpty())
				matches = group.m_sSpawnResultId == operation.m_sSpawnResultId;
			if (!matches && !operation.m_sForceId.IsEmpty())
				matches = group.m_sForceId == operation.m_sForceId;
			if (!matches && !operation.m_sProjectionId.IsEmpty())
				matches = group.m_sProjectionId == operation.m_sProjectionId;
			if (matches)
				count++;
		}
		return count;
	}

	protected int CountGroupsClaimingOperation(HST_OperationRecordState operation)
	{
		int count;
		if (!operation)
			return count;
		foreach (HST_ActiveGroupState group : m_SaveData.m_aActiveGroups)
		{
			if (!group)
				continue;
			if ((!operation.m_sGroupId.IsEmpty() && group.m_sGroupId == operation.m_sGroupId)
				|| (!operation.m_sOperationId.IsEmpty() && group.m_sOperationId == operation.m_sOperationId)
				|| (!operation.m_sManifestId.IsEmpty() && group.m_sManifestId == operation.m_sManifestId)
				|| (!operation.m_sSpawnResultId.IsEmpty() && group.m_sSpawnResultId == operation.m_sSpawnResultId)
				|| (!operation.m_sForceId.IsEmpty() && group.m_sForceId == operation.m_sForceId)
				|| (!operation.m_sProjectionId.IsEmpty() && group.m_sProjectionId == operation.m_sProjectionId))
				count++;
		}
		return count;
	}

	protected int CountManifestMembers(HST_ForceManifestState manifest, string slotId)
	{
		int count;
		foreach (HST_ForceManifestMemberState member : manifest.m_aMembers)
		{
			if (member && member.m_sSlotId == slotId)
				count++;
		}
		return count;
	}

	protected int CountBatchSlots(
		HST_ForceSpawnResultState batch,
		string slotId,
		string slotKind)
	{
		int count;
		foreach (HST_ForceSpawnSlotResultState slot : batch.m_aSlotResults)
		{
			if (slot && slot.m_sSlotId == slotId && slot.m_sSlotKind == slotKind)
				count++;
		}
		return count;
	}

	protected bool PositionsMatch(vector first, vector second, float toleranceMeters = 0.5)
	{
		float dx = first[0] - second[0];
		float dz = first[2] - second[2];
		return dx * dx + dz * dz <= toleranceMeters * toleranceMeters;
	}

	protected float Distance2D(vector first, vector second)
	{
		float dx = first[0] - second[0];
		float dz = first[2] - second[2];
		return Math.Sqrt(dx * dx + dz * dz);
	}

	protected bool IsZeroVector(vector value)
	{
		return value[0] == 0 && value[1] == 0 && value[2] == 0;
	}

	protected bool HasEvent(string eventId)
	{
		foreach (HST_CampaignEventState eventState : m_SaveData.m_aCampaignEvents)
		{
			if (eventState && eventState.m_sEventId == eventId)
				return true;
		}
		return false;
	}
}
