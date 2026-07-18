class HST_LocalSecurityOperationResult
{
	bool m_bSuccess;
	bool m_bAlreadyApplied;
	bool m_bStateChanged;
	string m_sFailureReason;
	ref HST_LocalSecurityPatrolState m_Patrol;
	ref HST_OperationRecordState m_Operation;
	ref HST_ForceManifestState m_Manifest;
	ref HST_ForceSpawnResultState m_Batch;
	ref HST_ActiveGroupState m_Group;
}

// Exact automatic police authority for canonical enemy towns. One persisted
// envelope owns one frozen roster epoch; ambient projection count is never
// political or casualty truth.
class HST_LocalSecurityOperationService
{
	static const float PERSISTENCE_POSITION_UPDATE_THRESHOLD_SQ_METERS = 4.0;
	static const int EXACT_CONTRACT_VERSION = 1;
	static const int QUARANTINED_CONTRACT_VERSION = -66;
	static const int EXACT_PROJECTION_CONTRACT_VERSION = 1;
	static const string EXACT_FORCE_KIND = "local_security_patrol";
	static const string EXACT_POLICY_ID = "exact_local_security_patrol_v1";
	static const string EXACT_MANIFEST_INTENT = "town_police";
	static const string EXACT_GROUP_MODE = "exact_local_security_patrol";
	static const string ASSIGNMENT_KIND = "local_security_patrol";
	static const string RECALL_POLICY_ID = "retire_without_refund";
	static const string SETTLEMENT_POLICY_ID = "exact_local_security_patrol_roster";
	static const string SETTLEMENT_KIND = "exact_local_security_patrol_terminal";
	static const string LOSS_EVENT_KIND = "local_security_patrol_destroyed";
	static const int EXACT_PRIORITY = 60;
	static const int EXACT_MAX_RETRIES = 3;
	static const int DEPLOYMENT_GRACE_SECONDS = 180;
	static const int MAX_ADMISSIONS_PER_TICK = 4;
	static const int MAX_ID_CHARACTERS = 180;

	protected ref HST_LocalSecurityCatalogService m_Catalog = new HST_LocalSecurityCatalogService();
	protected ref HST_ForcePlanningIntegrityService m_Integrity = new HST_ForcePlanningIntegrityService();
	protected ref HST_ForceSpawnQueueService m_SpawnQueue;
	protected ref HST_ForceSpawnAdapterService m_SpawnAdapter;
	protected ref HST_PhysicalWarService m_PhysicalWar;
	protected ref HST_TownInfluenceService m_TownInfluence;

	void SetRuntimeServices(
		HST_ForceSpawnQueueService spawnQueue,
		HST_ForceSpawnAdapterService spawnAdapter,
		HST_PhysicalWarService physicalWar,
		HST_TownInfluenceService townInfluence)
	{
		m_SpawnQueue = spawnQueue;
		m_SpawnAdapter = spawnAdapter;
		m_PhysicalWar = physicalWar;
		m_TownInfluence = townInfluence;
	}

	static bool IsLocalSecurityPatrolOperation(HST_OperationRecordState operation)
	{
		return operation
			&& operation.m_eType == HST_EOperationType.HST_OPERATION_TYPE_LOCAL_SECURITY_PATROL
			&& (operation.m_iContractVersion == EXACT_CONTRACT_VERSION
				|| operation.m_iContractVersion == QUARANTINED_CONTRACT_VERSION);
	}

	static bool IsOpenLocalSecurityPatrolOperation(HST_OperationRecordState operation)
	{
		return operation
			&& operation.m_eType == HST_EOperationType.HST_OPERATION_TYPE_LOCAL_SECURITY_PATROL
			&& operation.m_iContractVersion == EXACT_CONTRACT_VERSION
			&& operation.m_eSettlementState == HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_OPEN
			&& operation.m_eTerminalResult == HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_NONE;
	}

	static bool IsLocalSecurityPatrolGroupClaimant(
		HST_CampaignState state,
		HST_ActiveGroupState group)
	{
		if (!group)
			return false;
		if (!group.m_sLocalSecurityPatrolId.IsEmpty()
			|| group.m_sSpawnFallbackMode.Contains(EXACT_GROUP_MODE))
			return true;
		if (!state || group.m_sOperationId.IsEmpty())
			return false;
		HST_OperationRecordState operation = state.FindOperation(group.m_sOperationId);
		return IsLocalSecurityPatrolOperation(operation);
	}

	bool Tick(HST_CampaignState state, HST_CampaignPreset preset)
	{
		if (!state || !preset || !RuntimeServicesReady())
			return false;
		bool changed;
		foreach (HST_LocalSecurityPatrolState patrol : state.m_aLocalSecurityPatrols)
		{
			if (!patrol)
				continue;
			if (patrol.m_iContractVersion == QUARANTINED_CONTRACT_VERSION)
				continue;
			if (patrol.m_iContractVersion != EXACT_CONTRACT_VERSION)
			{
				changed = Quarantine(state, patrol, "unsupported local-security patrol contract") || changed;
				continue;
			}
			if (patrol.m_sStatus == "active")
				changed = TickActivePatrol(state, preset, patrol) || changed;
			else if (patrol.m_sStatus == "terminal")
				changed = ReconcileTerminalPatrol(state, patrol) || changed;
		}

		if (state.m_ePhase != HST_ECampaignPhase.HST_CAMPAIGN_ACTIVE)
			return changed;
		int admissions;
		foreach (HST_ZoneState zone : state.m_aZones)
		{
			if (admissions >= MAX_ADMISSIONS_PER_TICK)
				break;
			HST_CivilianZoneState civilian;
			if (!IsEligibleCanonicalEnemyTown(state, preset, zone, civilian))
				continue;
			HST_LocalSecurityPatrolState existing = state.FindLocalSecurityPatrol(zone.m_sZoneId);
			if (!CanOpenEpoch(state, zone, existing))
				continue;
			HST_LocalSecurityOperationResult admitted = AdmitEpoch(state, zone, civilian, existing);
			if (!admitted || !admitted.m_bSuccess)
				continue;
			if (!admitted.m_bAlreadyApplied)
				admissions++;
			changed = admitted.m_bStateChanged || changed;
		}
		return changed;
	}

	protected HST_LocalSecurityOperationResult AdmitEpoch(
		HST_CampaignState state,
		HST_ZoneState zone,
		HST_CivilianZoneState civilian,
		HST_LocalSecurityPatrolState existing)
	{
		HST_LocalSecurityOperationResult result = new HST_LocalSecurityOperationResult();
		if (!state || !zone || !civilian || !m_Catalog || !m_Integrity || !m_SpawnQueue)
		{
			result.m_sFailureReason = "local-security admission context is incomplete";
			return result;
		}
		if (existing && existing.m_sStatus == "active")
		{
			string activeFailure = ResolveRuntimeGraph(
				state,
				existing,
				result.m_Operation,
				result.m_Manifest,
				result.m_Batch,
				result.m_Group);
			if (!activeFailure.IsEmpty())
			{
				result.m_sFailureReason = activeFailure;
				return result;
			}
			result.m_bSuccess = true;
			result.m_bAlreadyApplied = true;
			result.m_Patrol = existing;
			return result;
		}

		int epoch = 1;
		if (existing)
			epoch = existing.m_iEpoch + 1;
		string patrolId = BuildPatrolId(
			zone.m_sZoneId,
			zone.m_sOwnerFactionKey,
			zone.m_iOwnershipRevision,
			epoch);
		if (patrolId.IsEmpty())
		{
			result.m_sFailureReason = "local-security deterministic patrol identity is invalid";
			return result;
		}

		HST_LocalSecurityCatalogResult catalog = m_Catalog.ResolveAuthoredGroup(
			zone.m_sOwnerFactionKey,
			civilian.m_iPolicePresence);
		if (!catalog || !catalog.m_bSuccess || !catalog.m_Group)
		{
			result.m_sFailureReason = "local-security authored roster is unavailable";
			if (catalog && !catalog.m_sFailureReason.IsEmpty())
				result.m_sFailureReason = catalog.m_sFailureReason;
			return result;
		}

		HST_ForceManifestState manifest = BuildManifest(state, zone, patrolId, catalog.m_Group);
		HST_OperationRecordState operation = BuildOperation(state, zone, patrolId, manifest);
		HST_ActiveGroupState group = BuildGroup(state, zone, patrolId, manifest);
		if (!manifest || !operation || !group)
		{
			result.m_sFailureReason = "local-security exact authority rows could not be built";
			return result;
		}
		string collision = FindIdentityCollision(state, patrolId, operation, manifest, group, existing);
		if (!collision.IsEmpty())
		{
			result.m_sFailureReason = collision;
			return result;
		}

		HST_ForceSpawnQueueRequest request = BuildSpawnRequest(state, patrolId, operation);
		HST_ForceSpawnQueueEnqueueResult preflight = m_SpawnQueue.CanEnqueue(
			state.m_aForceSpawnResults,
			manifest,
			request,
			Math.Max(0, state.m_iElapsedSeconds));
		if (!preflight || !preflight.m_bSuccess || preflight.m_bAlreadyApplied)
		{
			result.m_sFailureReason = "local-security spawn admission preflight failed";
			if (preflight && !preflight.m_sFailureReason.IsEmpty())
				result.m_sFailureReason = result.m_sFailureReason + ": " + preflight.m_sFailureReason;
			return result;
		}

		state.m_aForceManifests.Insert(manifest);
		state.m_aOperations.Insert(operation);
		state.m_aActiveGroups.Insert(group);
		HST_ForceSpawnQueueEnqueueResult enqueue = m_SpawnQueue.Enqueue(
			state.m_aForceSpawnResults,
			manifest,
			request,
			Math.Max(0, state.m_iElapsedSeconds));
		if (!enqueue || !enqueue.m_bSuccess || !enqueue.m_Batch || enqueue.m_bAlreadyApplied)
		{
			RemoveAdmissionRows(state, operation, manifest, group, null);
			result.m_sFailureReason = "local-security spawn admission failed";
			return result;
		}
		HST_ForceSpawnQueueCallbackResult held = m_SpawnQueue.HoldPendingProjectionForStrategicSimulation(
			state.m_aForceSpawnResults,
			manifest,
			enqueue.m_Batch.m_sResultId,
			enqueue.m_Batch.m_sProjectionId,
			Math.Max(0, state.m_iElapsedSeconds));
		if (!held || !held.m_bAccepted || !enqueue.m_Batch.m_bStrategicProjectionHeld)
		{
			RemoveAdmissionRows(state, operation, manifest, group, enqueue.m_Batch);
			result.m_sFailureReason = "local-security strategic hold failed";
			return result;
		}

		if (existing && !CompactTerminalForRearm(state, existing))
		{
			RemoveAdmissionRows(state, operation, manifest, group, enqueue.m_Batch);
			result.m_sFailureReason = "local-security prior terminal epoch could not be compacted";
			return result;
		}
		HST_LocalSecurityPatrolState patrol = existing;
		if (!patrol)
		{
			patrol = new HST_LocalSecurityPatrolState();
			state.m_aLocalSecurityPatrols.Insert(patrol);
		}
		ApplyCommittedEnvelope(state, zone, civilian, patrol, patrolId, epoch, operation, manifest, enqueue.m_Batch, group);
		result.m_bSuccess = true;
		result.m_bStateChanged = true;
		result.m_Patrol = patrol;
		result.m_Operation = operation;
		result.m_Manifest = manifest;
		result.m_Batch = enqueue.m_Batch;
		result.m_Group = group;
		return result;
	}

	protected HST_ForceManifestState BuildManifest(
		HST_CampaignState state,
		HST_ZoneState zone,
		string patrolId,
		HST_ForceGroupCatalogEntry catalogGroup)
	{
		if (!state || !zone || patrolId.IsEmpty() || !catalogGroup
			|| catalogGroup.m_aMemberSlots.Count() < 2
			|| catalogGroup.m_aMemberSlots.Count() > 5)
			return null;
		string operationId = BuildOperationId(patrolId);
		HST_ForceManifestState manifest = new HST_ForceManifestState();
		manifest.m_sManifestId = BuildManifestId(operationId);
		manifest.m_sOperationId = operationId;
		manifest.m_sForceKind = EXACT_FORCE_KIND;
		manifest.m_sFactionRole = "enemy";
		manifest.m_sFactionKey = zone.m_sOwnerFactionKey;
		manifest.m_sIntentId = EXACT_MANIFEST_INTENT;
		manifest.m_sSourceZoneId = zone.m_sZoneId;
		manifest.m_sTargetZoneId = zone.m_sZoneId;
		manifest.m_sGroupPrefab = catalogGroup.m_sExecutionPrefab;
		manifest.m_sCatalogVersion = HST_LocalSecurityCatalogService.CATALOG_VERSION;
		manifest.m_sPolicyId = EXACT_POLICY_ID;
		manifest.m_iRequestedMemberCount = catalogGroup.m_aMemberSlots.Count();
		manifest.m_iAcceptedMemberCount = catalogGroup.m_aMemberSlots.Count();
		manifest.m_iDeterministicSeed = BuildDeterministicSeed(state, patrolId);
		manifest.m_iCreatedAtSecond = Math.Max(0, state.m_iElapsedSeconds);
		manifest.m_bFrozen = true;

		HST_ForceManifestGroupState groupElement = new HST_ForceManifestGroupState();
		groupElement.m_sElementId = manifest.m_sManifestId + "_group_1";
		groupElement.m_sCatalogEntryId = catalogGroup.m_sEntryId;
		groupElement.m_sPrefab = catalogGroup.m_sExecutionPrefab;
		groupElement.m_sRole = HST_LocalSecurityCatalogService.ROLE_TOWN_POLICE;
		groupElement.m_iExpectedMemberCount = catalogGroup.m_aMemberSlots.Count();
		groupElement.m_bRequired = true;
		manifest.m_aGroups.Insert(groupElement);
		for (int memberIndex = 0; memberIndex < catalogGroup.m_aMemberSlots.Count(); memberIndex++)
		{
			HST_ForceGroupCatalogSlot authored = catalogGroup.m_aMemberSlots[memberIndex];
			if (!authored || authored.m_sPrefab.IsEmpty() || authored.m_iOrdinal != memberIndex)
				return null;
			HST_ForceManifestMemberState member = new HST_ForceManifestMemberState();
			member.m_sSlotId = string.Format("%1_member_%2", manifest.m_sManifestId, memberIndex + 1);
			member.m_sCatalogSlotId = catalogGroup.m_sEntryId + "/" + authored.m_sSlotId;
			member.m_sGroupElementId = groupElement.m_sElementId;
			member.m_sPrefab = authored.m_sPrefab;
			member.m_sRole = authored.m_sRole;
			member.m_iOrdinal = memberIndex;
			member.m_bRequired = true;
			manifest.m_aMembers.Insert(member);
		}
		manifest.m_sManifestHash = m_Integrity.BuildManifestHash(manifest);
		if (manifest.m_sManifestHash.IsEmpty())
			return null;
		return manifest;
	}

	protected HST_OperationRecordState BuildOperation(
		HST_CampaignState state,
		HST_ZoneState zone,
		string patrolId,
		HST_ForceManifestState manifest)
	{
		if (!state || !zone || patrolId.IsEmpty() || !manifest)
			return null;
		int nowSecond = Math.Max(0, state.m_iElapsedSeconds);
		HST_OperationRecordState operation = new HST_OperationRecordState();
		operation.m_sOperationId = manifest.m_sOperationId;
		operation.m_eType = HST_EOperationType.HST_OPERATION_TYPE_LOCAL_SECURITY_PATROL;
		operation.m_iContractVersion = EXACT_CONTRACT_VERSION;
		operation.m_iProjectionContractVersion = EXACT_PROJECTION_CONTRACT_VERSION;
		operation.m_sOwnerFactionKey = zone.m_sOwnerFactionKey;
		operation.m_sLocalSecurityPatrolId = patrolId;
		operation.m_sManifestId = manifest.m_sManifestId;
		operation.m_sSpawnResultId = BuildSpawnResultId(patrolId);
		operation.m_sForceId = BuildForceId(operation.m_sOperationId);
		operation.m_sProjectionId = BuildProjectionId(operation.m_sOperationId);
		operation.m_sGroupId = operation.m_sProjectionId;
		operation.m_sOriginZoneId = zone.m_sZoneId;
		operation.m_vOriginPosition = zone.m_vPosition;
		operation.m_sAssignmentKind = ASSIGNMENT_KIND;
		operation.m_sAssignmentZoneId = zone.m_sZoneId;
		operation.m_vAssignmentPosition = zone.m_vPosition;
		operation.m_sTacticalTargetZoneId = zone.m_sZoneId;
		operation.m_vTacticalTargetPosition = zone.m_vPosition;
		operation.m_vStrategicPosition = zone.m_vPosition;
		operation.m_sRecallPolicyId = RECALL_POLICY_ID;
		operation.m_sSettlementPolicyId = SETTLEMENT_POLICY_ID;
		operation.m_eDutyState = HST_EOperationDutyState.HST_OPERATION_DUTY_ON_STATION;
		operation.m_eResumeDutyState = operation.m_eDutyState;
		operation.m_eEngagementMode = HST_EOperationEngagementMode.HST_OPERATION_ENGAGEMENT_CLEAR;
		operation.m_eMaterializationState = HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_VIRTUAL;
		operation.m_ePositionAuthority = HST_EOperationPositionAuthority.HST_OPERATION_POSITION_STRATEGIC;
		operation.m_eSettlementState = HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_OPEN;
		operation.m_eTerminalResult = HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_NONE;
		operation.m_iDeterministicSeed = manifest.m_iDeterministicSeed;
		operation.m_iLastVirtualFriendlyCount = manifest.m_iAcceptedMemberCount;
		operation.m_iCreatedAtSecond = nowSecond;
		operation.m_iDutyStateEnteredAtSecond = nowSecond;
		operation.m_iEngagementStateEnteredAtSecond = nowSecond;
		operation.m_iMaterializationStateEnteredAtSecond = nowSecond;
		operation.m_iLastProgressAtSecond = nowSecond;
		operation.m_iRevision = 1;
		return operation;
	}

	protected HST_ActiveGroupState BuildGroup(
		HST_CampaignState state,
		HST_ZoneState zone,
		string patrolId,
		HST_ForceManifestState manifest)
	{
		if (!state || !zone || patrolId.IsEmpty() || !manifest
			|| manifest.m_aGroups.Count() != 1 || !manifest.m_aGroups[0])
			return null;
		string projectionId = BuildProjectionId(manifest.m_sOperationId);
		HST_ActiveGroupState group = new HST_ActiveGroupState();
		group.m_sGroupId = projectionId;
		group.m_sOperationId = manifest.m_sOperationId;
		group.m_sManifestId = manifest.m_sManifestId;
		group.m_sSpawnResultId = BuildSpawnResultId(patrolId);
		group.m_sForceId = BuildForceId(manifest.m_sOperationId);
		group.m_sProjectionId = projectionId;
		group.m_sLocalSecurityPatrolId = patrolId;
		group.m_sZoneId = zone.m_sZoneId;
		group.m_sFactionKey = zone.m_sOwnerFactionKey;
		group.m_sPrefab = manifest.m_aGroups[0].m_sPrefab;
		group.m_sCompositionRequestId = patrolId;
		group.m_sCompositionIntentId = EXACT_MANIFEST_INTENT;
		group.m_sCompositionTier = "exact";
		group.m_sCompositionSummary = string.Format("%1 exact local-security police", manifest.m_iAcceptedMemberCount);
		group.m_sSpawnFallbackMode = EXACT_GROUP_MODE;
		group.m_vPosition = zone.m_vPosition;
		group.m_vSourcePosition = zone.m_vPosition;
		group.m_vTargetPosition = zone.m_vPosition;
		group.m_sRuntimeStatus = "local_security_virtual";
		group.m_iInfantryCount = manifest.m_iAcceptedMemberCount;
		group.m_iOriginalInfantryCount = manifest.m_iAcceptedMemberCount;
		group.m_iCompositionManpower = manifest.m_iAcceptedMemberCount;
		group.m_iLastSeenAliveCount = manifest.m_iAcceptedMemberCount;
		group.m_iSurvivorInfantryCount = manifest.m_iAcceptedMemberCount;
		group.m_iDurableLivingInfantryCount = manifest.m_iAcceptedMemberCount;
		group.m_iSpawnedAtSecond = Math.Max(0, state.m_iElapsedSeconds);
		group.m_iLifecycleRevision = 1;
		return group;
	}

	protected HST_ForceSpawnQueueRequest BuildSpawnRequest(
		HST_CampaignState state,
		string patrolId,
		HST_OperationRecordState operation)
	{
		HST_ForceSpawnQueueRequest request = new HST_ForceSpawnQueueRequest();
		if (!state || patrolId.IsEmpty() || !operation)
			return request;
		request.m_sResultId = BuildSpawnResultId(patrolId);
		request.m_sRequestId = patrolId;
		request.m_sForceId = BuildForceId(operation.m_sOperationId);
		request.m_sProjectionId = BuildProjectionId(operation.m_sOperationId);
		request.m_iPriority = EXACT_PRIORITY;
		request.m_iMaxRetries = EXACT_MAX_RETRIES;
		request.m_iDeadlineSecond = Math.Max(0, state.m_iElapsedSeconds) + DEPLOYMENT_GRACE_SECONDS;
		return request;
	}

	protected void ApplyCommittedEnvelope(
		HST_CampaignState state,
		HST_ZoneState zone,
		HST_CivilianZoneState civilian,
		HST_LocalSecurityPatrolState patrol,
		string patrolId,
		int epoch,
		HST_OperationRecordState operation,
		HST_ForceManifestState manifest,
		HST_ForceSpawnResultState batch,
		HST_ActiveGroupState group)
	{
		if (!state || !zone || !civilian || !patrol || !operation || !manifest || !batch || !group)
			return;
		int nowSecond = Math.Max(0, state.m_iElapsedSeconds);
		string positiveSource = FindPositivePoliceSourceAfterTerminal(state, patrol);
		bool baseline = patrol.m_sPatrolId.IsEmpty() && positiveSource.IsEmpty();
		patrol.m_iContractVersion = EXACT_CONTRACT_VERSION;
		patrol.m_iRevision = Math.Max(1, patrol.m_iRevision + 1);
		patrol.m_sPatrolId = patrolId;
		patrol.m_sZoneId = zone.m_sZoneId;
		patrol.m_sFactionKey = zone.m_sOwnerFactionKey;
		patrol.m_iOwnershipRevision = zone.m_iOwnershipRevision;
		patrol.m_iEpoch = epoch;
		patrol.m_sSourceType = "town_police_presence";
		patrol.m_sSourceId = positiveSource;
		if (patrol.m_sSourceId.IsEmpty())
			patrol.m_sSourceId = zone.m_sLastOwnershipTransitionRequestId;
		if (patrol.m_sSourceId.IsEmpty())
			patrol.m_sSourceId = zone.m_sZoneId;
		patrol.m_bBaseline = baseline;
		patrol.m_sPolicyId = EXACT_POLICY_ID;
		patrol.m_iPoliceStrength = Math.Max(1, civilian.m_iPolicePresence);
		patrol.m_sOperationId = operation.m_sOperationId;
		patrol.m_sManifestId = manifest.m_sManifestId;
		patrol.m_sManifestHash = manifest.m_sManifestHash;
		patrol.m_sSpawnResultId = batch.m_sResultId;
		patrol.m_sForceId = batch.m_sForceId;
		patrol.m_sProjectionId = batch.m_sProjectionId;
		patrol.m_sGroupId = group.m_sGroupId;
		patrol.m_sStatus = "active";
		patrol.m_iOriginalInfantryCount = manifest.m_iAcceptedMemberCount;
		patrol.m_iLivingInfantryCount = manifest.m_iAcceptedMemberCount;
		patrol.m_iCreatedAtSecond = nowSecond;
		patrol.m_iLastChangedAtSecond = nowSecond;
		patrol.m_iTerminalAtSecond = 0;
		patrol.m_sLossEventId = "";
		patrol.m_bLossEventApplied = false;
		patrol.m_iLossEventAppliedAtSecond = 0;
		patrol.m_sTerminalReason = "";
		patrol.m_sAuthorityFailure = "";
		zone.m_sLocalSecurityPatrolId = patrolId;
	}

	protected string FindIdentityCollision(
		HST_CampaignState state,
		string patrolId,
		HST_OperationRecordState operation,
		HST_ForceManifestState manifest,
		HST_ActiveGroupState group,
		HST_LocalSecurityPatrolState reusableEnvelope)
	{
		if (!state || patrolId.IsEmpty() || !operation || !manifest || !group)
			return "local-security collision context is incomplete";
		foreach (HST_LocalSecurityPatrolState patrol : state.m_aLocalSecurityPatrols)
		{
			if (!patrol || patrol == reusableEnvelope)
				continue;
			if (patrol.m_sPatrolId == patrolId || patrol.m_sZoneId == group.m_sZoneId
				|| patrol.m_sOperationId == operation.m_sOperationId
				|| patrol.m_sManifestId == manifest.m_sManifestId
				|| patrol.m_sSpawnResultId == group.m_sSpawnResultId
				|| patrol.m_sProjectionId == group.m_sProjectionId)
				return "local-security patrol identity is already claimed";
		}
		foreach (HST_OperationRecordState candidateOperation : state.m_aOperations)
		{
			if (candidateOperation && (candidateOperation.m_sOperationId == operation.m_sOperationId
				|| candidateOperation.m_sLocalSecurityPatrolId == patrolId
				|| candidateOperation.m_sManifestId == manifest.m_sManifestId
				|| candidateOperation.m_sSpawnResultId == group.m_sSpawnResultId
				|| candidateOperation.m_sProjectionId == group.m_sProjectionId))
				return "local-security operation identity is already claimed";
		}
		foreach (HST_ForceManifestState candidateManifest : state.m_aForceManifests)
		{
			if (candidateManifest && (candidateManifest.m_sManifestId == manifest.m_sManifestId
				|| candidateManifest.m_sOperationId == operation.m_sOperationId))
				return "local-security manifest identity is already claimed";
		}
		foreach (HST_ForceSpawnResultState batch : state.m_aForceSpawnResults)
		{
			if (batch && (batch.m_sResultId == group.m_sSpawnResultId
				|| batch.m_sRequestId == patrolId
				|| batch.m_sOperationId == operation.m_sOperationId
				|| batch.m_sManifestId == manifest.m_sManifestId
				|| batch.m_sForceId == group.m_sForceId
				|| batch.m_sProjectionId == group.m_sProjectionId))
				return "local-security spawn identity is already claimed";
		}
		foreach (HST_ActiveGroupState candidateGroup : state.m_aActiveGroups)
		{
			if (candidateGroup && (candidateGroup.m_sGroupId == group.m_sGroupId
				|| candidateGroup.m_sLocalSecurityPatrolId == patrolId
				|| candidateGroup.m_sOperationId == operation.m_sOperationId
				|| candidateGroup.m_sManifestId == manifest.m_sManifestId
				|| candidateGroup.m_sSpawnResultId == group.m_sSpawnResultId
				|| candidateGroup.m_sProjectionId == group.m_sProjectionId))
				return "local-security active-group identity is already claimed";
		}
		return "";
	}

	protected void RemoveAdmissionRows(
		HST_CampaignState state,
		HST_OperationRecordState operation,
		HST_ForceManifestState manifest,
		HST_ActiveGroupState group,
		HST_ForceSpawnResultState batch)
	{
		if (!state)
			return;
		if (batch)
		{
			int batchIndex = state.m_aForceSpawnResults.Find(batch);
			if (batchIndex >= 0)
				state.m_aForceSpawnResults.Remove(batchIndex);
		}
		if (group)
		{
			int groupIndex = state.m_aActiveGroups.Find(group);
			if (groupIndex >= 0)
				state.m_aActiveGroups.Remove(groupIndex);
		}
		if (operation)
		{
			int operationIndex = state.m_aOperations.Find(operation);
			if (operationIndex >= 0)
				state.m_aOperations.Remove(operationIndex);
		}
		if (manifest)
		{
			int manifestIndex = state.m_aForceManifests.Find(manifest);
			if (manifestIndex >= 0)
				state.m_aForceManifests.Remove(manifestIndex);
		}
	}

	protected bool CompactTerminalForRearm(
		HST_CampaignState state,
		HST_LocalSecurityPatrolState patrol)
	{
		if (!state || !patrol || patrol.m_sStatus != "terminal")
			return false;
		HST_OperationRecordState operation = state.FindOperation(patrol.m_sOperationId);
		HST_ForceManifestState manifest = state.FindForceManifest(patrol.m_sManifestId);
		if (!operation || !manifest
			|| operation.m_eType != HST_EOperationType.HST_OPERATION_TYPE_LOCAL_SECURITY_PATROL
			|| operation.m_iContractVersion != EXACT_CONTRACT_VERSION
			|| operation.m_eSettlementState != HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_SETTLED
			|| operation.m_sLocalSecurityPatrolId != patrol.m_sPatrolId
			|| manifest.m_sOperationId != operation.m_sOperationId
			|| manifest.m_sManifestId != patrol.m_sManifestId
			|| manifest.m_sManifestHash != patrol.m_sManifestHash)
			return false;
		if (state.FindForceSpawnResult(patrol.m_sSpawnResultId)
			|| state.FindActiveGroup(patrol.m_sGroupId)
			|| (m_SpawnAdapter && m_SpawnAdapter.CountHandlesForProjection(patrol.m_sProjectionId) > 0))
			return false;
		state.m_aOperations.Remove(state.m_aOperations.Find(operation));
		state.m_aForceManifests.Remove(state.m_aForceManifests.Find(manifest));
		return true;
	}

	protected bool CanOpenEpoch(
		HST_CampaignState state,
		HST_ZoneState zone,
		HST_LocalSecurityPatrolState existing)
	{
		if (!state || !zone || !zone.m_sActiveOwnershipTransitionRequestId.IsEmpty())
			return false;
		if (!existing)
		{
			if (!zone.m_sLocalSecurityPatrolId.IsEmpty())
				return false;
			return CountPatrolsForZone(state, zone.m_sZoneId) == 0;
		}
		if (CountPatrolsForZone(state, zone.m_sZoneId) != 1
			|| existing.m_iContractVersion != EXACT_CONTRACT_VERSION
			|| existing.m_sStatus == "quarantined"
			|| !existing.m_sAuthorityFailure.IsEmpty())
			return false;
		if (existing.m_sStatus == "active")
			return false;
		if (existing.m_sStatus != "terminal" || existing.m_iEpoch <= 0
			|| existing.m_iEpoch >= int.MAX - 1)
			return false;
		if (zone.m_iOwnershipRevision > existing.m_iOwnershipRevision)
			return true;
		if (zone.m_iOwnershipRevision != existing.m_iOwnershipRevision
			|| zone.m_sOwnerFactionKey != existing.m_sFactionKey)
			return false;
		return !FindPositivePoliceSourceAfterTerminal(state, existing).IsEmpty();
	}

	protected bool IsEligibleCanonicalEnemyTown(
		HST_CampaignState state,
		HST_CampaignPreset preset,
		HST_ZoneState zone,
		out HST_CivilianZoneState civilian)
	{
		civilian = null;
		if (!state || !preset || !zone || zone.m_eType != HST_EZoneType.HST_ZONE_TOWN
			|| zone.m_sZoneId.IsEmpty() || zone.m_sOwnerFactionKey.IsEmpty()
			|| zone.m_iOwnershipContractVersion != HST_OwnershipTransitionService.EXACT_CONTRACT_VERSION
			|| zone.m_iOwnershipRevision <= 0 || !zone.m_sOwnershipAuthorityFailure.IsEmpty()
			|| !HST_FactionRelationService.IsEnemyFaction(preset, zone.m_sOwnerFactionKey)
			|| !m_TownInfluence || !m_TownInfluence.FindValidRecord(state, zone.m_sZoneId))
			return false;
		int zoneMatches;
		foreach (HST_ZoneState candidateZone : state.m_aZones)
		{
			if (candidateZone && candidateZone.m_sZoneId == zone.m_sZoneId)
				zoneMatches++;
		}
		if (zoneMatches != 1)
			return false;
		int civilianMatches;
		foreach (HST_CivilianZoneState candidateCivilian : state.m_aCivilianZones)
		{
			if (!candidateCivilian || candidateCivilian.m_sZoneId != zone.m_sZoneId)
				continue;
			civilianMatches++;
			civilian = candidateCivilian;
		}
		if (civilianMatches != 1 || !civilian || civilian.m_iPolicePresence <= 0)
		{
			civilian = null;
			return false;
		}
		int poolMatches;
		foreach (HST_FactionPoolState pool : state.m_aFactionPools)
		{
			if (pool && pool.m_sFactionKey == zone.m_sOwnerFactionKey)
				poolMatches++;
		}
		return poolMatches == 1;
	}

	protected string FindPositivePoliceSourceAfterTerminal(
		HST_CampaignState state,
		HST_LocalSecurityPatrolState patrol)
	{
		if (!state || !patrol || patrol.m_sStatus != "terminal")
			return "";
		bool lossSeen = patrol.m_sLossEventId.IsEmpty();
		foreach (HST_TownInfluenceEventState eventState : state.m_aTownInfluenceEvents)
		{
			if (!eventState)
				continue;
			if (!lossSeen)
			{
				if (eventState.m_sEventId == patrol.m_sLossEventId)
					lossSeen = true;
				continue;
			}
			if (eventState.m_sZoneId != patrol.m_sZoneId || !eventState.m_bApplied
				|| eventState.m_iContractVersion != HST_TownInfluenceService.EXACT_CONTRACT_VERSION
				|| eventState.m_iPoliceDelta <= 0)
				continue;
			if (patrol.m_sLossEventId.IsEmpty()
				&& eventState.m_iCreatedAtSecond <= patrol.m_iTerminalAtSecond)
				continue;
			return eventState.m_sEventId;
		}
		return "";
	}

	static string BuildPatrolId(
		string zoneId,
		string factionKey,
		int ownershipRevision,
		int epoch)
	{
		zoneId = zoneId.Trim();
		factionKey = factionKey.Trim();
		if (zoneId.IsEmpty() || factionKey.IsEmpty()
			|| ownershipRevision <= 0 || epoch <= 0)
			return "";
		return string.Format(
			"local_security_patrol_%1_%2_%3_%4",
			zoneId.Hash(),
			factionKey.Hash(),
			ownershipRevision,
			epoch);
	}

	static string BuildOperationId(string patrolId)
	{
		return HST_StableIdService.BuildOperationId("local_security", patrolId);
	}

	static string BuildManifestId(string operationId)
	{
		if (operationId.IsEmpty())
			return "";
		return "manifest_" + operationId;
	}

	static string BuildSpawnResultId(string patrolId)
	{
		if (patrolId.IsEmpty())
			return "";
		return "spawn_" + patrolId;
	}

	static string BuildForceId(string operationId)
	{
		if (operationId.IsEmpty())
			return "";
		return "force_" + operationId;
	}

	static string BuildProjectionId(string operationId)
	{
		if (operationId.IsEmpty())
			return "";
		return "projection_" + operationId;
	}

	static string BuildLossEventId(string patrolId)
	{
		if (patrolId.IsEmpty())
			return "";
		return "local_security_patrol_destroyed_" + patrolId;
	}

	protected int BuildDeterministicSeed(HST_CampaignState state, string patrolId)
	{
		if (!state || patrolId.IsEmpty())
			return 1;
		int seed = patrolId.Hash() ^ state.m_iCampaignSeed;
		if (seed == int.MIN)
			return int.MAX;
		return Math.Max(1, Math.AbsInt(seed));
	}

	protected int CountPatrolsForZone(HST_CampaignState state, string zoneId)
	{
		int count;
		if (!state || zoneId.IsEmpty())
			return count;
		foreach (HST_LocalSecurityPatrolState patrol : state.m_aLocalSecurityPatrols)
		{
			if (patrol && patrol.m_sZoneId == zoneId)
				count++;
		}
		return count;
	}

	protected bool RuntimeServicesReady()
	{
		return m_SpawnQueue && m_SpawnAdapter && m_PhysicalWar && m_TownInfluence;
	}

	protected bool TickActivePatrol(
		HST_CampaignState state,
		HST_CampaignPreset preset,
		HST_LocalSecurityPatrolState patrol)
	{
		if (!state || !preset || !patrol)
			return false;
		HST_ZoneState zone = state.FindZone(patrol.m_sZoneId);
		HST_CivilianZoneState civilian;
		bool eligible = IsEligibleCanonicalEnemyTown(state, preset, zone, civilian);
		if (!zone || zone.m_sOwnerFactionKey != patrol.m_sFactionKey
			|| zone.m_iOwnershipRevision != patrol.m_iOwnershipRevision)
			return RetireWithoutLoss(state, patrol,
				HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_INVALIDATED,
				"owner_changed",
				"local-security assignment ended because canonical ownership changed");
		if (!eligible)
			return RetireWithoutLoss(state, patrol,
				HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_CANCELLED,
				"security_cleared",
				"local-security assignment ended because town police pressure cleared");

		HST_OperationRecordState operation;
		HST_ForceManifestState manifest;
		HST_ForceSpawnResultState batch;
		HST_ActiveGroupState group;
		string failure = ResolveRuntimeGraph(state, patrol, operation, manifest, batch, group);
		if (!failure.IsEmpty())
			return Quarantine(state, patrol, failure);
		if (operation.m_eMaterializationState == HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_VIRTUAL)
			return TickVirtual(state, zone, patrol, operation, manifest, batch, group);
		if (operation.m_eMaterializationState == HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_MATERIALIZING)
			return TickMaterializing(state, patrol, operation, manifest, batch, group);
		if (operation.m_eMaterializationState == HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_PHYSICAL)
			return TickPhysical(state, zone, patrol, operation, manifest, batch, group);
		if (operation.m_eMaterializationState == HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_DEMATERIALIZING)
			return ContinueDematerialization(state, patrol, operation, manifest, batch, group);
		return Quarantine(state, patrol, "local-security materialization state is invalid");
	}

	protected bool TickVirtual(
		HST_CampaignState state,
		HST_ZoneState zone,
		HST_LocalSecurityPatrolState patrol,
		HST_OperationRecordState operation,
		HST_ForceManifestState manifest,
		HST_ForceSpawnResultState batch,
		HST_ActiveGroupState group)
	{
		if (batch.m_eStatus == HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_FAILED_FINAL
			|| batch.m_eStatus == HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_CANCELLED)
			return SettleOperation(state, patrol, operation, manifest, batch, group,
				HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_SPAWN_FAILED,
				"spawn_failed", "local-security exact projection failed");
		if (!batch.m_bStrategicProjectionHeld)
			return Quarantine(state, patrol, "virtual local-security batch is not strategically held");
		int living = m_SpawnQueue.CountStrategicLivingMemberSlots(batch);
		bool changed = SyncRoster(state, patrol, operation, group, living);
		if (living <= 0)
		{
			HST_ForceSpawnQueueCallbackResult eliminated = m_SpawnQueue.CompleteStrategicProjectionElimination(
				state.m_aForceSpawnResults,
				manifest,
				batch.m_sResultId,
				batch.m_sProjectionId,
				Math.Max(0, state.m_iElapsedSeconds),
				"local-security virtual roster eliminated");
			if (!eliminated || !eliminated.m_bAccepted)
				return Quarantine(state, patrol, "local-security virtual elimination receipt failed");
			return HandleDestroyed(state, patrol, operation, manifest, batch, group) || changed;
		}
		if (!zone.m_bActive)
		{
			group.m_sRuntimeStatus = "local_security_virtual";
			return changed;
		}
		HST_ForceSpawnQueueCallbackResult released = m_SpawnQueue.ReleaseStrategicProjectionForMaterialization(
			state.m_aForceSpawnResults,
			manifest,
			batch.m_sResultId,
			batch.m_sProjectionId,
			Math.Max(0, state.m_iElapsedSeconds),
			Math.Max(0, state.m_iElapsedSeconds) + DEPLOYMENT_GRACE_SECONDS);
		if (!released || !released.m_bAccepted)
			return Quarantine(state, patrol, "local-security materialization release failed");
		int nowSecond = Math.Max(0, state.m_iElapsedSeconds);
		operation.m_eMaterializationState = HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_MATERIALIZING;
		operation.m_ePositionAuthority = HST_EOperationPositionAuthority.HST_OPERATION_POSITION_STRATEGIC;
		operation.m_iMaterializationStateEnteredAtSecond = nowSecond;
		operation.m_iLastProjectionDecisionSecond = nowSecond;
		operation.m_sLastProjectionReason = "canonical enemy town became active";
		operation.m_iRevision++;
		group.m_sRuntimeStatus = "local_security_materializing";
		group.m_iLifecycleRevision++;
		patrol.m_iRevision++;
		patrol.m_iLastChangedAtSecond = nowSecond;
		return true;
	}

	protected bool TickMaterializing(
		HST_CampaignState state,
		HST_LocalSecurityPatrolState patrol,
		HST_OperationRecordState operation,
		HST_ForceManifestState manifest,
		HST_ForceSpawnResultState batch,
		HST_ActiveGroupState group)
	{
		if (batch.m_eStatus == HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_FAILED_FINAL
			|| batch.m_eStatus == HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_CANCELLED
			|| group.m_sRuntimeStatus == "spawn_failed")
			return RetireWithoutLoss(state, patrol,
				HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_SPAWN_FAILED,
				"spawn_failed", "local-security materialization failed");
		if (batch.m_eStatus != HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_SUCCEEDED
			|| !group.m_bSpawnedEntity)
			return false;
		HST_ForceSpawnAdapterTickResult reconciled = m_SpawnAdapter.ReconcileExactInfantryProjectionAuthority(
			state, m_SpawnQueue, m_PhysicalWar, Math.Max(0, state.m_iElapsedSeconds), patrol.m_sProjectionId);
		if (!reconciled || reconciled.m_iFailedCount > 0)
			return Quarantine(state, patrol, "local-security materialization roster reconciliation failed");
		int living = m_SpawnQueue.CountDurableLivingMemberSlots(batch);
		SyncRoster(state, patrol, operation, group, living);
		if (living <= 0)
			return HandleDestroyed(state, patrol, operation, manifest, batch, group);
		string bindingFailure;
		if (!m_SpawnAdapter.ValidateExactLivingProjectionBindingsForPersistence(
			state, batch, m_SpawnQueue, m_PhysicalWar, bindingFailure))
			return Quarantine(state, patrol, "local-security live bindings are incomplete: " + bindingFailure);
		int nowSecond = Math.Max(0, state.m_iElapsedSeconds);
		operation.m_eMaterializationState = HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_PHYSICAL;
		operation.m_ePositionAuthority = HST_EOperationPositionAuthority.HST_OPERATION_POSITION_LIVE;
		operation.m_iMaterializationStateEnteredAtSecond = nowSecond;
		operation.m_iRevision++;
		group.m_sRuntimeStatus = "local_security_physical";
		string waypointEvidence;
		if (!m_PhysicalWar.AssignExactLocalSecurityPatrolWaypoints(state, group, waypointEvidence))
			return Quarantine(state, patrol, "local-security patrol waypoints failed: " + waypointEvidence);
		patrol.m_iRevision++;
		patrol.m_iLastChangedAtSecond = nowSecond;
		return true;
	}

	protected bool TickPhysical(
		HST_CampaignState state,
		HST_ZoneState zone,
		HST_LocalSecurityPatrolState patrol,
		HST_OperationRecordState operation,
		HST_ForceManifestState manifest,
		HST_ForceSpawnResultState batch,
		HST_ActiveGroupState group)
	{
		if (batch.m_eStatus != HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_SUCCEEDED)
			return Quarantine(state, patrol, "local-security physical batch lost successful authority");
		HST_ForceSpawnAdapterTickResult reconciled = m_SpawnAdapter.ReconcileExactInfantryProjectionAuthority(
			state, m_SpawnQueue, m_PhysicalWar, Math.Max(0, state.m_iElapsedSeconds), patrol.m_sProjectionId);
		if (!reconciled || reconciled.m_iFailedCount > 0)
			return Quarantine(state, patrol, "local-security physical casualty reconciliation failed");
		int living = m_SpawnQueue.CountDurableLivingMemberSlots(batch);
		bool changed = SyncRoster(state, patrol, operation, group, living);
		if (living <= 0 || group.m_sRuntimeStatus == "eliminated")
			return HandleDestroyed(state, patrol, operation, manifest, batch, group) || changed;
		string bindingFailure;
		if (!m_SpawnAdapter.ValidateExactLivingProjectionBindingsForPersistence(
			state, batch, m_SpawnQueue, m_PhysicalWar, bindingFailure))
			return Quarantine(state, patrol, "local-security physical binding graph is unresolved: " + bindingFailure);
		vector livePosition;
		string liveEvidence;
		if (!m_PhysicalWar.TryResolveExactLocalSecurityPatrolLivePosition(state, group, livePosition, liveEvidence))
			return Quarantine(state, patrol, "local-security live position is unavailable: " + liveEvidence);
		if (zone.m_bActive)
			return changed;
		operation.m_vStrategicPosition = livePosition;
		group.m_vPosition = livePosition;
		operation.m_iLastProgressAtSecond = Math.Max(0, state.m_iElapsedSeconds);
		operation.m_eMaterializationState = HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_DEMATERIALIZING;
		operation.m_ePositionAuthority = HST_EOperationPositionAuthority.HST_OPERATION_POSITION_LIVE;
		operation.m_iMaterializationStateEnteredAtSecond = Math.Max(0, state.m_iElapsedSeconds);
		operation.m_sLastProjectionReason = "canonical enemy town became inactive";
		operation.m_iRevision++;
		group.m_sRuntimeStatus = "local_security_dematerializing";
		return ContinueDematerialization(state, patrol, operation, manifest, batch, group) || changed;
	}

	protected bool ContinueDematerialization(
		HST_CampaignState state,
		HST_LocalSecurityPatrolState patrol,
		HST_OperationRecordState operation,
		HST_ForceManifestState manifest,
		HST_ForceSpawnResultState batch,
		HST_ActiveGroupState group)
	{
		if (!batch.m_bStrategicProjectionHeld)
		{
			HST_ForceSpawnAdapterTickResult reconciled = m_SpawnAdapter.ReconcileExactInfantryProjectionAuthority(
				state, m_SpawnQueue, m_PhysicalWar, Math.Max(0, state.m_iElapsedSeconds), patrol.m_sProjectionId);
			if (!reconciled || reconciled.m_iFailedCount > 0)
				return Quarantine(state, patrol, "local-security dematerialization casualty reconciliation failed");
			int living = m_SpawnQueue.CountDurableLivingMemberSlots(batch);
			SyncRoster(state, patrol, operation, group, living);
			if (living <= 0)
				return HandleDestroyed(state, patrol, operation, manifest, batch, group);
			string bindingFailure;
			if (!m_SpawnAdapter.ValidateExactLivingProjectionBindingsForPersistence(
				state, batch, m_SpawnQueue, m_PhysicalWar, bindingFailure))
				return Quarantine(state, patrol, "local-security dematerialization bindings are incomplete: " + bindingFailure);
			HST_ForceSpawnAdapterRetireResult retired = m_SpawnAdapter.RetireProjectionRuntime(
				state, m_PhysicalWar, patrol.m_sProjectionId);
			if (!retired || !retired.m_bSuccess)
				return false;
			HST_ForceSpawnQueueCallbackResult held = m_SpawnQueue.RequeueSuccessfulProjectionForStrategicHold(
				state.m_aForceSpawnResults,
				manifest,
				batch.m_sResultId,
				batch.m_sProjectionId,
				Math.Max(0, state.m_iElapsedSeconds),
				Math.Max(0, state.m_iElapsedSeconds) + DEPLOYMENT_GRACE_SECONDS);
			if (!held || !held.m_bAccepted)
				return Quarantine(state, patrol, "local-security survivors could not enter strategic hold");
		}
		ClearGroupProcessAuthority(group);
		int livingAfter = m_SpawnQueue.CountStrategicLivingMemberSlots(batch);
		SyncRoster(state, patrol, operation, group, livingAfter);
		int nowSecond = Math.Max(0, state.m_iElapsedSeconds);
		operation.m_eMaterializationState = HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_VIRTUAL;
		operation.m_ePositionAuthority = HST_EOperationPositionAuthority.HST_OPERATION_POSITION_STRATEGIC;
		operation.m_iMaterializationStateEnteredAtSecond = nowSecond;
		operation.m_iLastProgressAtSecond = nowSecond;
		operation.m_iRevision++;
		group.m_sRuntimeStatus = "local_security_virtual";
		group.m_iLifecycleRevision++;
		patrol.m_iRevision++;
		patrol.m_iLastChangedAtSecond = nowSecond;
		return true;
	}

	protected bool HandleDestroyed(
		HST_CampaignState state,
		HST_LocalSecurityPatrolState patrol,
		HST_OperationRecordState operation,
		HST_ForceManifestState manifest,
		HST_ForceSpawnResultState batch,
		HST_ActiveGroupState group)
	{
		if (!state || !patrol || !operation || !manifest || !batch || !group)
			return false;
		if (!RetireRuntimeIfPresent(state, patrol, batch, group, true))
			return false;
		if (!ApplyDestructionLossEvent(state, patrol, operation))
			return false;
		if (patrol.m_iContractVersion != EXACT_CONTRACT_VERSION
			|| patrol.m_sStatus == "quarantined")
			return true;
		return SettleOperation(state, patrol, operation, manifest, batch, group,
			HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_DESTROYED,
			"destroyed", "local-security exact roster was eliminated");
	}

	protected bool ApplyDestructionLossEvent(
		HST_CampaignState state,
		HST_LocalSecurityPatrolState patrol,
		HST_OperationRecordState operation)
	{
		if (!state || !patrol || !operation || !m_TownInfluence)
			return false;
		string eventId = BuildLossEventId(patrol.m_sPatrolId);
		if (eventId.IsEmpty() || eventId.Length() > MAX_ID_CHARACTERS)
			return Quarantine(state, patrol, "local-security destruction event identity is invalid");
		HST_TownInfluenceCommand command = new HST_TownInfluenceCommand();
		command.m_sCommandId = eventId;
		command.m_sEventId = eventId;
		command.m_sTownId = patrol.m_sZoneId;
		command.m_sEventKind = LOSS_EVENT_KIND;
		command.m_sSourceId = operation.m_sOperationId;
		command.m_sReason = "enemy local-security patrol destroyed";
		command.m_iPoliceDelta = -1;
		command.m_bPopulationScaled = false;
		command.m_bMarkContacted = false;
		command.m_bMarkResistanceActivity = false;
		command.m_bReconcileOwnership = false;
		HST_TownInfluenceResult influence = m_TownInfluence.Execute(state, command);
		if (!influence || !influence.m_bAccepted)
		{
			string failure = "local-security destruction event was rejected";
			if (influence && !influence.m_sFailureReason.IsEmpty())
				failure = influence.m_sFailureReason;
			if (failure.Contains("reused with a different fingerprint")
				|| failure.Contains("duplicated"))
				return Quarantine(state, patrol, failure);
			return false;
		}
		bool changed = !patrol.m_bLossEventApplied
			|| patrol.m_sLossEventId != eventId;
		patrol.m_sLossEventId = eventId;
		patrol.m_bLossEventApplied = true;
		if (patrol.m_iLossEventAppliedAtSecond <= 0)
			patrol.m_iLossEventAppliedAtSecond = Math.Max(0, state.m_iElapsedSeconds);
		if (changed)
		{
			patrol.m_iRevision++;
			patrol.m_iLastChangedAtSecond = Math.Max(0, state.m_iElapsedSeconds);
		}
		return true;
	}

	protected bool RetireWithoutLoss(
		HST_CampaignState state,
		HST_LocalSecurityPatrolState patrol,
		HST_EOperationTerminalResult terminalResult,
		string detail,
		string reason)
	{
		if (!state || !patrol)
			return false;
		if (patrol.m_sStatus == "terminal")
			return true;
		HST_OperationRecordState operation;
		HST_ForceManifestState manifest;
		HST_ForceSpawnResultState batch;
		HST_ActiveGroupState group;
		string failure = ResolveRuntimeGraph(state, patrol, operation, manifest, batch, group);
		if (!failure.IsEmpty())
			return Quarantine(state, patrol, failure);
		if (!RetireRuntimeIfPresent(state, patrol, batch, group, false))
			return false;
		return SettleOperation(state, patrol, operation, manifest, batch, group,
			terminalResult, detail, reason);
	}

	protected bool RetireRuntimeIfPresent(
		HST_CampaignState state,
		HST_LocalSecurityPatrolState patrol,
		HST_ForceSpawnResultState batch,
		HST_ActiveGroupState group,
		bool allowZeroLiving)
	{
		if (!state || !patrol || !batch || !group || !m_SpawnAdapter || !m_PhysicalWar)
			return false;
		int handles = m_SpawnAdapter.CountHandlesForProjection(patrol.m_sProjectionId);
		bool runtimeExists = handles > 0 || group.m_bSpawnedEntity
			|| m_PhysicalWar.GetForceSpawnGroupRoot(group)
			|| m_PhysicalWar.CountForceSpawnRuntimeMembers(group) > 0;
		if (!runtimeExists)
			return true;
		if (batch.m_eStatus == HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_SUCCEEDED)
		{
			HST_ForceSpawnAdapterTickResult reconciled = m_SpawnAdapter.ReconcileExactInfantryProjectionAuthority(
				state, m_SpawnQueue, m_PhysicalWar, Math.Max(0, state.m_iElapsedSeconds), patrol.m_sProjectionId);
			if (!reconciled || reconciled.m_iFailedCount > 0)
				return false;
			int living = m_SpawnQueue.CountDurableLivingMemberSlots(batch);
			if (living > 0)
			{
				string bindingFailure;
				if (!m_SpawnAdapter.ValidateExactLivingProjectionBindingsForPersistence(
					state, batch, m_SpawnQueue, m_PhysicalWar, bindingFailure))
					return false;
			}
			else if (!allowZeroLiving)
				return false;
		}
		HST_ForceSpawnAdapterRetireResult retired = m_SpawnAdapter.RetireProjectionRuntime(
			state, m_PhysicalWar, patrol.m_sProjectionId);
		if (!retired || !retired.m_bSuccess)
			return false;
		ClearGroupProcessAuthority(group);
		return true;
	}

	protected bool SettleOperation(
		HST_CampaignState state,
		HST_LocalSecurityPatrolState patrol,
		HST_OperationRecordState operation,
		HST_ForceManifestState manifest,
		HST_ForceSpawnResultState batch,
		HST_ActiveGroupState group,
		HST_EOperationTerminalResult terminalResult,
		string detail,
		string reason)
	{
		if (!state || !patrol || !operation || !manifest || detail.IsEmpty()
			|| terminalResult == HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_NONE
			|| terminalResult == HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_UNKNOWN)
			return false;
		string settlementId = HST_OperationService.BuildSettlementId(
			operation.m_sOperationId, SETTLEMENT_KIND);
		if (operation.m_eSettlementState == HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_SETTLED)
		{
			if (operation.m_sSettlementId != settlementId
				|| operation.m_eTerminalResult != terminalResult)
				return Quarantine(state, patrol, "local-security terminal receipt conflicts");
			return FinalizeSettledRuntime(state, patrol, operation, batch, group);
		}
		if (!IsOpenLocalSecurityPatrolOperation(operation))
			return Quarantine(state, patrol, "local-security settlement authority is invalid");
		int living = ResolveLivingRoster(manifest, batch, group, operation);
		int nowSecond = Math.Max(0, state.m_iElapsedSeconds);
		operation.m_iLastVirtualFriendlyCount = living;
		operation.m_eDutyState = HST_EOperationDutyState.HST_OPERATION_DUTY_SETTLED;
		operation.m_eResumeDutyState = operation.m_eDutyState;
		operation.m_eEngagementMode = HST_EOperationEngagementMode.HST_OPERATION_ENGAGEMENT_CLEAR;
		operation.m_eMaterializationState = HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_RETIRED;
		operation.m_ePositionAuthority = HST_EOperationPositionAuthority.HST_OPERATION_POSITION_STRATEGIC;
		operation.m_eSettlementState = HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_SETTLED;
		operation.m_eTerminalResult = terminalResult;
		operation.m_sSettlementId = settlementId;
		operation.m_sTerminalReason = reason;
		operation.m_sLastProjectionReason = SETTLEMENT_KIND + ": " + detail;
		operation.m_iSettledAtSecond = nowSecond;
		operation.m_iLastProgressAtSecond = nowSecond;
		operation.m_iRevision++;
		patrol.m_sStatus = "terminal";
		patrol.m_iLivingInfantryCount = living;
		patrol.m_iTerminalAtSecond = nowSecond;
		patrol.m_iLastChangedAtSecond = nowSecond;
		patrol.m_sTerminalReason = reason;
		patrol.m_sAuthorityFailure = "";
		patrol.m_iRevision++;
		if (group)
		{
			ClearGroupProcessAuthority(group);
			SyncGroupRoster(group, living);
			group.m_sRuntimeStatus = "local_security_terminal_" + detail;
		}
		FinalizeSettledRuntime(state, patrol, operation, batch, group);
		return true;
	}

	protected bool FinalizeSettledRuntime(
		HST_CampaignState state,
		HST_LocalSecurityPatrolState patrol,
		HST_OperationRecordState operation,
		HST_ForceSpawnResultState batch,
		HST_ActiveGroupState group)
	{
		if (!state || !patrol || !operation
			|| operation.m_eSettlementState != HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_SETTLED)
			return false;
		if (m_SpawnAdapter.CountHandlesForProjection(patrol.m_sProjectionId) > 0)
			return false;
		if (group && (m_PhysicalWar.GetForceSpawnGroupRoot(group)
			|| m_PhysicalWar.CountForceSpawnRuntimeMembers(group) > 0))
			return false;
		bool changed;
		if (batch)
		{
			int batchIndex = state.m_aForceSpawnResults.Find(batch);
			if (batchIndex >= 0)
			{
				state.m_aForceSpawnResults.Remove(batchIndex);
				changed = true;
			}
		}
		if (group)
		{
			int groupIndex = state.m_aActiveGroups.Find(group);
			if (groupIndex >= 0)
			{
				state.m_aActiveGroups.Remove(groupIndex);
				changed = true;
			}
		}
		return changed;
	}

	protected bool Quarantine(
		HST_CampaignState state,
		HST_LocalSecurityPatrolState patrol,
		string reason)
	{
		if (!state || !patrol)
			return false;
		string failure = "local-security authority quarantined without guessed casualty or political event: " + reason;
		bool changed = patrol.m_iContractVersion != QUARANTINED_CONTRACT_VERSION
			|| patrol.m_sStatus != "quarantined" || patrol.m_sAuthorityFailure != failure;
		patrol.m_iContractVersion = QUARANTINED_CONTRACT_VERSION;
		patrol.m_sStatus = "quarantined";
		patrol.m_sAuthorityFailure = failure;
		patrol.m_iLastChangedAtSecond = Math.Max(0, state.m_iElapsedSeconds);
		patrol.m_iRevision++;
		HST_OperationRecordState operation = state.FindOperation(patrol.m_sOperationId);
		if (operation)
		{
			operation.m_iContractVersion = QUARANTINED_CONTRACT_VERSION;
			operation.m_sLastProjectionReason = failure;
			operation.m_iRevision++;
		}
		HST_ActiveGroupState group = state.FindActiveGroup(patrol.m_sGroupId);
		if (group)
		{
			group.m_sRuntimeStatus = "local_security_quarantined";
			group.m_sSpawnFailureReason = failure;
		}
		bool cleanupChanged;
		string cleanupFailure;
		TryCleanupQuarantinedAuthority(
			state,
			patrol,
			cleanupChanged,
			cleanupFailure);
		return cleanupChanged || changed;
	}

	bool PrepareQuarantinedAuthorityForPersistence(
		HST_CampaignState state,
		out string failure)
	{
		failure = "";
		if (!state)
		{
			failure = "local-security quarantine cleanup requires campaign state";
			return false;
		}
		foreach (HST_LocalSecurityPatrolState patrol : state.m_aLocalSecurityPatrols)
		{
			if (!patrol || (patrol.m_iContractVersion != QUARANTINED_CONTRACT_VERSION
				&& patrol.m_sStatus != "quarantined"))
				continue;
			bool changed;
			if (!TryCleanupQuarantinedAuthority(state, patrol, changed, failure))
				return false;
		}
		return true;
	}

	protected bool TryCleanupQuarantinedAuthority(
		HST_CampaignState state,
		HST_LocalSecurityPatrolState patrol,
		out bool changed,
		out string failure)
	{
		changed = false;
		failure = "";
		if (!state || !patrol || !m_SpawnQueue || !m_SpawnAdapter || !m_PhysicalWar)
		{
			failure = "local-security quarantine runtime services are unavailable";
			return false;
		}
		if (patrol.m_sOperationId.IsEmpty() || patrol.m_sProjectionId.IsEmpty()
			|| patrol.m_sProjectionId != BuildProjectionId(patrol.m_sOperationId))
		{
			failure = "local-security quarantine projection identity is not proven";
			return false;
		}
		int batchClaims = CountBatches(state, patrol);
		int groupClaims = CountGroups(state, patrol);
		if (batchClaims > 1 || groupClaims > 1)
		{
			failure = "local-security quarantine runtime claimant is ambiguous";
			return false;
		}
		HST_ForceSpawnResultState batch = state.FindForceSpawnResult(
			patrol.m_sSpawnResultId);
		HST_ActiveGroupState group = state.FindActiveGroup(patrol.m_sGroupId);
		if ((batchClaims == 1 && !batch) || (groupClaims == 1 && !group))
		{
			failure = "local-security quarantine expected runtime claimant is cross-keyed";
			return false;
		}
		if (batch && (batch.m_sResultId != patrol.m_sSpawnResultId
			|| batch.m_sRequestId != patrol.m_sPatrolId
			|| batch.m_sOperationId != patrol.m_sOperationId
			|| batch.m_sManifestId != patrol.m_sManifestId))
		{
			failure = "local-security quarantine batch identity is not proven";
			return false;
		}
		if (batch && (batch.m_sForceId != patrol.m_sForceId
			|| batch.m_sProjectionId != patrol.m_sProjectionId))
		{
			failure = "local-security quarantine batch identity is not proven";
			return false;
		}
		if (group && (group.m_sGroupId != patrol.m_sGroupId
			|| group.m_sLocalSecurityPatrolId != patrol.m_sPatrolId
			|| group.m_sOperationId != patrol.m_sOperationId
			|| group.m_sManifestId != patrol.m_sManifestId))
		{
			failure = "local-security quarantine group identity is not proven";
			return false;
		}
		if (group && (group.m_sSpawnResultId != patrol.m_sSpawnResultId
			|| group.m_sForceId != patrol.m_sForceId
			|| group.m_sProjectionId != patrol.m_sProjectionId))
		{
			failure = "local-security quarantine group identity is not proven";
			return false;
		}

		int handles = m_SpawnAdapter.CountHandlesForProjection(
			patrol.m_sProjectionId);
		bool physicalRuntime = group && (group.m_bSpawnedEntity
			|| m_PhysicalWar.GetForceSpawnGroupRoot(group)
			|| m_PhysicalWar.CountForceSpawnRuntimeMembers(group) > 0);
		if (handles > 0 || physicalRuntime)
		{
			HST_ForceSpawnAdapterRetireResult retired
				= m_SpawnAdapter.RetireProjectionRuntime(
					state,
					m_PhysicalWar,
					patrol.m_sProjectionId);
			if (!retired || !retired.m_bSuccess)
			{
				failure = "local-security quarantined runtime could not retire";
				return false;
			}
			changed = retired.m_bRuntimeChanged || changed;
		}
		if (group)
		{
			ClearGroupProcessAuthority(group);
			if (group.m_sRuntimeStatus != "local_security_quarantined"
				|| group.m_sSpawnFallbackMode != EXACT_GROUP_MODE + "_quarantined")
				changed = true;
			group.m_sRuntimeStatus = "local_security_quarantined";
			group.m_sSpawnFallbackMode = EXACT_GROUP_MODE + "_quarantined";
			group.m_sSpawnFailureReason = patrol.m_sAuthorityFailure;
		}
		if (batch)
		{
			bool alreadyTerminal = batch.m_eStatus
				== HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_FAILED_FINAL
				|| batch.m_eStatus
					== HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_CANCELLED;
			HST_ForceSpawnQueueCallbackResult cancelled;
			if (alreadyTerminal)
			{
				// Proven terminal rows already carry cleared physical evidence.
			}
			else if (batch.m_eStatus == HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_SUCCEEDED)
				cancelled = m_SpawnQueue.CompleteQuarantinedSuccessfulProjectionCancellation(
					state.m_aForceSpawnResults,
					batch.m_sResultId,
					batch.m_sProjectionId,
					Math.Max(0, state.m_iElapsedSeconds),
					patrol.m_sAuthorityFailure);
			else
				cancelled = m_SpawnQueue.RequestCancel(
					state.m_aForceSpawnResults,
					batch.m_sResultId,
					Math.Max(0, state.m_iElapsedSeconds),
					patrol.m_sAuthorityFailure);
			if (!alreadyTerminal
				&& (!cancelled || !cancelled.m_bAccepted || cancelled.m_bCleanupRequired))
			{
				failure = "local-security quarantined batch could not cancel";
				if (cancelled && !cancelled.m_sFailureReason.IsEmpty())
					failure = failure + ": " + cancelled.m_sFailureReason;
				return false;
			}
			if (cancelled)
				changed = cancelled.m_bStateChanged || changed;
		}
		if (m_SpawnAdapter.CountHandlesForProjection(patrol.m_sProjectionId) > 0)
		{
			failure = "local-security quarantined adapter handles remain";
			return false;
		}
		if (group && (m_PhysicalWar.GetForceSpawnGroupRoot(group)
			|| m_PhysicalWar.CountForceSpawnRuntimeMembers(group) > 0))
		{
			failure = "local-security quarantined physical runtime remains";
			return false;
		}
		return true;
	}

	protected string ResolveRuntimeGraph(
		HST_CampaignState state,
		HST_LocalSecurityPatrolState patrol,
		out HST_OperationRecordState operation,
		out HST_ForceManifestState manifest,
		out HST_ForceSpawnResultState batch,
		out HST_ActiveGroupState group)
	{
		operation = null;
		manifest = null;
		batch = null;
		group = null;
		if (!state || !IsValidPatrolEnvelopeIdentity(patrol, "active"))
			return "local-security envelope identity conflicts";
		HST_ZoneState zone = state.FindZone(patrol.m_sZoneId);
		if (!zone || zone.m_sLocalSecurityPatrolId != patrol.m_sPatrolId)
			return "local-security zone backlink conflicts";
		if (CountOperations(state, patrol) != 1 || CountManifests(state, patrol) != 1
			|| CountBatches(state, patrol) != 1 || CountGroups(state, patrol) != 1)
			return "local-security runtime graph identity is ambiguous";
		operation = state.FindOperation(patrol.m_sOperationId);
		manifest = state.FindForceManifest(patrol.m_sManifestId);
		batch = state.FindForceSpawnResult(patrol.m_sSpawnResultId);
		group = state.FindActiveGroup(patrol.m_sGroupId);
		if (!operation || !manifest || !batch || !group)
			return "local-security runtime graph is incomplete";
		if (!IsOpenLocalSecurityPatrolOperation(operation)
			|| operation.m_iProjectionContractVersion != EXACT_PROJECTION_CONTRACT_VERSION
			|| operation.m_sLocalSecurityPatrolId != patrol.m_sPatrolId
			|| operation.m_sOwnerFactionKey != patrol.m_sFactionKey)
			return "local-security operation policy conflicts";
		if (operation.m_sOriginZoneId != patrol.m_sZoneId
			|| operation.m_sAssignmentKind != ASSIGNMENT_KIND
			|| operation.m_sAssignmentZoneId != patrol.m_sZoneId
			|| operation.m_sRecallPolicyId != RECALL_POLICY_ID
			|| operation.m_sSettlementPolicyId != SETTLEMENT_POLICY_ID)
			return "local-security operation policy conflicts";
		if (!ValidateManifest(patrol, manifest))
			return "local-security frozen manifest conflicts";
		if (operation.m_sManifestId != manifest.m_sManifestId
			|| operation.m_sSpawnResultId != batch.m_sResultId
			|| operation.m_sForceId != batch.m_sForceId
			|| operation.m_sProjectionId != batch.m_sProjectionId)
			return "local-security reciprocal operation/batch links conflict";
		if (operation.m_sGroupId != group.m_sGroupId
			|| batch.m_sRequestId != patrol.m_sPatrolId
			|| batch.m_sOperationId != operation.m_sOperationId
			|| batch.m_sManifestId != manifest.m_sManifestId
			|| batch.m_sManifestHash != manifest.m_sManifestHash)
			return "local-security reciprocal operation/batch links conflict";
		if (group.m_sLocalSecurityPatrolId != patrol.m_sPatrolId
			|| group.m_sOperationId != operation.m_sOperationId
			|| group.m_sManifestId != manifest.m_sManifestId
			|| group.m_sSpawnResultId != batch.m_sResultId)
			return "local-security reciprocal active-group links conflict";
		if (group.m_sForceId != batch.m_sForceId
			|| group.m_sProjectionId != batch.m_sProjectionId
			|| group.m_sGroupId != group.m_sProjectionId
			|| group.m_sZoneId != patrol.m_sZoneId
			|| group.m_sFactionKey != patrol.m_sFactionKey
			|| !group.m_sSpawnFallbackMode.StartsWith(EXACT_GROUP_MODE))
			return "local-security reciprocal active-group links conflict";
		if (batch.m_aSlotResults.Count() != manifest.m_aMembers.Count() + 1
			|| patrol.m_iOriginalInfantryCount != manifest.m_iAcceptedMemberCount
			|| group.m_iOriginalInfantryCount != manifest.m_iAcceptedMemberCount)
			return "local-security durable roster shape conflicts";
		return "";
	}

	protected bool IsValidPatrolEnvelopeIdentity(
		HST_LocalSecurityPatrolState patrol,
		string expectedStatus)
	{
		if (!patrol || expectedStatus.IsEmpty())
			return false;
		if (patrol.m_iContractVersion != EXACT_CONTRACT_VERSION
			|| patrol.m_sStatus != expectedStatus
			|| patrol.m_sPatrolId.IsEmpty()
			|| patrol.m_sPolicyId != EXACT_POLICY_ID)
			return false;
		string expectedPatrolId = BuildPatrolId(
			patrol.m_sZoneId,
			patrol.m_sFactionKey,
			patrol.m_iOwnershipRevision,
			patrol.m_iEpoch);
		if (patrol.m_sPatrolId != expectedPatrolId)
			return false;
		if (patrol.m_sOperationId != BuildOperationId(patrol.m_sPatrolId)
			|| patrol.m_sManifestId != BuildManifestId(patrol.m_sOperationId))
			return false;
		if (patrol.m_sSpawnResultId != BuildSpawnResultId(patrol.m_sPatrolId)
			|| patrol.m_sForceId != BuildForceId(patrol.m_sOperationId))
			return false;
		return patrol.m_sProjectionId == BuildProjectionId(patrol.m_sOperationId)
			&& patrol.m_sGroupId == patrol.m_sProjectionId;
	}

	protected bool ValidateManifest(
		HST_LocalSecurityPatrolState patrol,
		HST_ForceManifestState manifest)
	{
		if (!patrol || !manifest || !manifest.m_bFrozen)
			return false;
		if (manifest.m_sManifestId != patrol.m_sManifestId
			|| manifest.m_sOperationId != patrol.m_sOperationId
			|| manifest.m_sForceKind != EXACT_FORCE_KIND
			|| manifest.m_sPolicyId != EXACT_POLICY_ID)
			return false;
		if (manifest.m_sIntentId != EXACT_MANIFEST_INTENT
			|| manifest.m_sFactionRole != "enemy"
			|| manifest.m_sFactionKey != patrol.m_sFactionKey
			|| manifest.m_sSourceZoneId != patrol.m_sZoneId
			|| manifest.m_sTargetZoneId != patrol.m_sZoneId)
			return false;
		if (manifest.m_sCatalogVersion != HST_LocalSecurityCatalogService.CATALOG_VERSION
			|| manifest.m_sManifestHash != patrol.m_sManifestHash
			|| m_Integrity.BuildManifestHash(manifest) != manifest.m_sManifestHash)
			return false;
		if (manifest.m_iMoneyCost != 0 || manifest.m_iHRCost != 0
			|| manifest.m_iEquipmentCost != 0
			|| manifest.m_iAttackResourceCost != 0
			|| manifest.m_iSupportResourceCost != 0)
			return false;
		if (manifest.m_aGroups.Count() != 1 || !manifest.m_aGroups[0]
			|| manifest.m_aVehicles.Count() != 0
			|| manifest.m_aAssets.Count() != 0
			|| manifest.m_iAcceptedVehicleCount != 0)
			return false;
		if (manifest.m_iAcceptedMemberCount < 2
			|| manifest.m_iAcceptedMemberCount > 5
			|| manifest.m_aMembers.Count() != manifest.m_iAcceptedMemberCount
			|| manifest.m_aGroups[0].m_iExpectedMemberCount
				!= manifest.m_iAcceptedMemberCount)
			return false;
		for (int memberIndex = 0; memberIndex < manifest.m_aMembers.Count(); memberIndex++)
		{
			HST_ForceManifestMemberState member = manifest.m_aMembers[memberIndex];
			if (!member || member.m_iOrdinal != memberIndex || !member.m_bRequired
				|| member.m_sPrefab.IsEmpty()
				|| member.m_sGroupElementId != manifest.m_aGroups[0].m_sElementId)
				return false;
		}
		return true;
	}

	protected bool SyncRoster(
		HST_CampaignState state,
		HST_LocalSecurityPatrolState patrol,
		HST_OperationRecordState operation,
		HST_ActiveGroupState group,
		int living)
	{
		if (!state || !patrol || !operation || !group)
			return false;
		int bounded = Math.Max(0, Math.Min(patrol.m_iOriginalInfantryCount, living));
		bool changed = patrol.m_iLivingInfantryCount != bounded
			|| operation.m_iLastVirtualFriendlyCount != bounded
			|| group.m_iInfantryCount != bounded
			|| group.m_iDurableLivingInfantryCount != bounded
			|| group.m_iLastSeenAliveCount != bounded
			|| group.m_iSurvivorInfantryCount != bounded;
		patrol.m_iLivingInfantryCount = bounded;
		operation.m_iLastVirtualFriendlyCount = bounded;
		SyncGroupRoster(group, bounded);
		if (changed)
		{
			patrol.m_iRevision++;
			patrol.m_iLastChangedAtSecond = Math.Max(0, state.m_iElapsedSeconds);
			operation.m_iRevision++;
			group.m_iLifecycleRevision++;
		}
		return changed;
	}

	protected void SyncGroupRoster(HST_ActiveGroupState group, int living)
	{
		if (!group)
			return;
		int bounded = Math.Max(0, Math.Min(group.m_iOriginalInfantryCount, living));
		group.m_iInfantryCount = bounded;
		group.m_iDurableLivingInfantryCount = bounded;
		group.m_iLastSeenAliveCount = bounded;
		group.m_iSurvivorInfantryCount = bounded;
	}

	protected int ResolveLivingRoster(
		HST_ForceManifestState manifest,
		HST_ForceSpawnResultState batch,
		HST_ActiveGroupState group,
		HST_OperationRecordState operation)
	{
		int accepted;
		if (manifest)
			accepted = Math.Max(0, manifest.m_iAcceptedMemberCount);
		int living;
		if (batch && batch.m_bStrategicProjectionHeld)
			living = m_SpawnQueue.CountStrategicLivingMemberSlots(batch);
		else if (batch && batch.m_iSuccessfulHandoffCount > 0)
			living = m_SpawnQueue.CountDurableLivingMemberSlots(batch);
		else if (group)
			living = Math.Max(group.m_iDurableLivingInfantryCount, group.m_iSurvivorInfantryCount);
		else if (operation)
			living = operation.m_iLastVirtualFriendlyCount;
		return Math.Max(0, Math.Min(accepted, living));
	}

	protected void ClearGroupProcessAuthority(HST_ActiveGroupState group)
	{
		if (!group)
			return;
		group.m_bSpawnedEntity = false;
		group.m_bSpawnAttempted = false;
		group.m_sRuntimeEntityId = "";
		group.m_iSpawnedAgentCount = 0;
		group.m_iAssignedWaypointCount = 0;
	}

	protected int CountOperations(HST_CampaignState state, HST_LocalSecurityPatrolState patrol)
	{
		int count;
		foreach (HST_OperationRecordState row : state.m_aOperations)
		{
			if (row && (row.m_sOperationId == patrol.m_sOperationId
				|| row.m_sLocalSecurityPatrolId == patrol.m_sPatrolId))
				count++;
		}
		return count;
	}

	protected int CountManifests(HST_CampaignState state, HST_LocalSecurityPatrolState patrol)
	{
		int count;
		foreach (HST_ForceManifestState row : state.m_aForceManifests)
		{
			if (row && (row.m_sManifestId == patrol.m_sManifestId
				|| row.m_sOperationId == patrol.m_sOperationId))
				count++;
		}
		return count;
	}

	protected int CountBatches(HST_CampaignState state, HST_LocalSecurityPatrolState patrol)
	{
		int count;
		foreach (HST_ForceSpawnResultState row : state.m_aForceSpawnResults)
		{
			if (row && (row.m_sResultId == patrol.m_sSpawnResultId
				|| row.m_sRequestId == patrol.m_sPatrolId
				|| row.m_sOperationId == patrol.m_sOperationId
				|| row.m_sProjectionId == patrol.m_sProjectionId))
				count++;
		}
		return count;
	}

	protected int CountGroups(HST_CampaignState state, HST_LocalSecurityPatrolState patrol)
	{
		int count;
		foreach (HST_ActiveGroupState row : state.m_aActiveGroups)
		{
			if (row && (row.m_sGroupId == patrol.m_sGroupId
				|| row.m_sLocalSecurityPatrolId == patrol.m_sPatrolId
				|| row.m_sOperationId == patrol.m_sOperationId
				|| row.m_sProjectionId == patrol.m_sProjectionId))
				count++;
		}
		return count;
	}

	bool ReconcileAfterRestore(
		HST_CampaignState state,
		HST_CampaignPreset preset)
	{
		if (!state || !preset || !RuntimeServicesReady())
			return false;
		bool changed;
		foreach (HST_LocalSecurityPatrolState patrol : state.m_aLocalSecurityPatrols)
		{
			if (!patrol)
				continue;
			if (patrol.m_iContractVersion == QUARANTINED_CONTRACT_VERSION)
				continue;
			if (patrol.m_iContractVersion != EXACT_CONTRACT_VERSION)
			{
				changed = Quarantine(state, patrol, "restore found unsupported local-security authority") || changed;
				continue;
			}
			if (patrol.m_sStatus == "terminal")
			{
				changed = ReconcileTerminalPatrol(state, patrol) || changed;
				continue;
			}
			if (patrol.m_sStatus != "active")
			{
				changed = Quarantine(state, patrol, "restore found an invalid local-security status") || changed;
				continue;
			}
			HST_OperationRecordState operation;
			HST_ForceManifestState manifest;
			HST_ForceSpawnResultState batch;
			HST_ActiveGroupState group;
			string failure = ResolveRuntimeGraph(state, patrol, operation, manifest, batch, group);
			if (!failure.IsEmpty())
			{
				changed = Quarantine(state, patrol, failure) || changed;
				continue;
			}
			if (!batch.m_bStrategicProjectionHeld
				&& batch.m_eStatus == HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_SUCCEEDED)
			{
				int handles = m_SpawnAdapter.CountHandlesForProjection(patrol.m_sProjectionId);
				bool runtime = handles > 0 || m_PhysicalWar.GetForceSpawnGroupRoot(group)
					|| m_PhysicalWar.CountForceSpawnRuntimeMembers(group) > 0;
				if (runtime)
				{
					HST_ForceSpawnAdapterRetireResult retired = m_SpawnAdapter.RetireProjectionRuntime(
						state, m_PhysicalWar, patrol.m_sProjectionId);
					if (!retired || !retired.m_bSuccess)
					{
						changed = Quarantine(state, patrol, "restored local-security runtime could not retire safely") || changed;
						continue;
					}
				}
				HST_ForceSpawnQueueCallbackResult held = m_SpawnQueue.RequeueSuccessfulProjectionForStrategicHold(
					state.m_aForceSpawnResults, manifest, batch.m_sResultId, batch.m_sProjectionId,
					Math.Max(0, state.m_iElapsedSeconds), Math.Max(0, state.m_iElapsedSeconds) + DEPLOYMENT_GRACE_SECONDS);
				if (!held || !held.m_bAccepted)
				{
					changed = Quarantine(state, patrol, "restored local-security roster could not enter strategic hold") || changed;
					continue;
				}
				changed = true;
			}
			else if (!batch.m_bStrategicProjectionHeld)
			{
				HST_ForceSpawnQueueCallbackResult heldPending = m_SpawnQueue.HoldPendingProjectionForStrategicSimulation(
					state.m_aForceSpawnResults, manifest, batch.m_sResultId, batch.m_sProjectionId,
					Math.Max(0, state.m_iElapsedSeconds));
				if (!heldPending || !heldPending.m_bAccepted)
				{
					changed = Quarantine(state, patrol, "restored pending local-security roster could not enter strategic hold") || changed;
					continue;
				}
				changed = true;
			}
			ClearGroupProcessAuthority(group);
			int nowSecond = Math.Max(0, state.m_iElapsedSeconds);
			operation.m_eMaterializationState = HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_VIRTUAL;
			operation.m_ePositionAuthority = HST_EOperationPositionAuthority.HST_OPERATION_POSITION_STRATEGIC;
			operation.m_eEngagementMode = HST_EOperationEngagementMode.HST_OPERATION_ENGAGEMENT_CLEAR;
			operation.m_iMaterializationStateEnteredAtSecond = nowSecond;
			operation.m_iLastProgressAtSecond = nowSecond;
			operation.m_sLastProjectionReason = "restored exact local-security patrol as held virtual authority";
			operation.m_iRevision++;
			int living = m_SpawnQueue.CountStrategicLivingMemberSlots(batch);
			changed = SyncRoster(state, patrol, operation, group, living) || changed;
			group.m_sRuntimeStatus = "local_security_virtual";
			group.m_iLifecycleRevision++;
			changed = true;
		}
		return changed;
	}

	bool CanReconcileZoneOwnershipChange(
		HST_CampaignState state,
		string zoneId,
		string newOwnerFactionKey,
		out string failure)
	{
		failure = "";
		if (!state || zoneId.IsEmpty() || newOwnerFactionKey.IsEmpty())
		{
			failure = "local-security ownership preflight requires state, zone, and owner";
			return false;
		}
		if (CountPatrolsForZone(state, zoneId) == 0)
			return true;
		HST_LocalSecurityPatrolState patrol = state.FindLocalSecurityPatrol(zoneId);
		if (!patrol)
		{
			failure = "local-security ownership preflight found ambiguous town authority";
			return false;
		}
		if (patrol.m_iContractVersion == QUARANTINED_CONTRACT_VERSION
			|| patrol.m_sStatus == "quarantined")
		{
			failure = "quarantined local-security authority blocks ownership transition";
			return false;
		}
		if (patrol.m_sStatus == "terminal")
		{
			HST_OperationRecordState terminalOperation;
			HST_ForceManifestState terminalManifest;
			HST_ForceSpawnResultState terminalBatch;
			HST_ActiveGroupState terminalGroup;
			failure = ResolveTerminalGraph(
				state, patrol, terminalOperation, terminalManifest, terminalBatch, terminalGroup);
			return failure.IsEmpty();
		}
		HST_OperationRecordState operation;
		HST_ForceManifestState manifest;
		HST_ForceSpawnResultState batch;
		HST_ActiveGroupState group;
		failure = ResolveRuntimeGraph(state, patrol, operation, manifest, batch, group);
		return failure.IsEmpty();
	}

	bool ReconcileZoneOwnershipChange(
		HST_CampaignState state,
		string zoneId,
		string newOwnerFactionKey,
		out bool stateChanged,
		out string failure)
	{
		stateChanged = false;
		if (!CanReconcileZoneOwnershipChange(state, zoneId, newOwnerFactionKey, failure))
			return false;
		HST_LocalSecurityPatrolState patrol = state.FindLocalSecurityPatrol(zoneId);
		if (!patrol || patrol.m_sStatus == "terminal")
			return true;
		HST_OperationRecordState operation;
		HST_ForceManifestState manifest;
		HST_ForceSpawnResultState batch;
		HST_ActiveGroupState group;
		failure = ResolveRuntimeGraph(state, patrol, operation, manifest, batch, group);
		if (!failure.IsEmpty())
			return false;
		bool physicalAuthority = operation.m_eMaterializationState
			!= HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_VIRTUAL;
		physicalAuthority = physicalAuthority
			|| m_SpawnAdapter.CountHandlesForProjection(patrol.m_sProjectionId) > 0
			|| m_PhysicalWar.GetForceSpawnGroupRoot(group)
			|| m_PhysicalWar.CountForceSpawnRuntimeMembers(group) > 0;
		if (physicalAuthority
			&& batch.m_eStatus == HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_SUCCEEDED)
		{
			HST_ForceSpawnAdapterTickResult reconciled
				= m_SpawnAdapter.ReconcileExactInfantryProjectionAuthority(
					state,
					m_SpawnQueue,
					m_PhysicalWar,
					Math.Max(0, state.m_iElapsedSeconds),
					patrol.m_sProjectionId);
			if (!reconciled || reconciled.m_iFailedCount > 0)
			{
				failure = "local-security ownership casualty reconciliation failed";
				return false;
			}
			int reconciledLiving = m_SpawnQueue.CountDurableLivingMemberSlots(batch);
			stateChanged = SyncRoster(
				state,
				patrol,
				operation,
				group,
				reconciledLiving) || stateChanged;
		}
		int living = ResolveLivingRoster(manifest, batch, group, operation);
		bool settled;
		if (living <= 0)
			settled = HandleDestroyed(
				state,
				patrol,
				operation,
				manifest,
				batch,
				group);
		else
			settled = RetireWithoutLoss(state, patrol,
				HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_INVALIDATED,
				"owner_changed", "local-security assignment ended by ownership transition");
		if (!settled || patrol.m_sStatus != "terminal")
		{
			failure = "local-security ownership retirement is pending";
			return false;
		}
		stateChanged = true;
		return true;
	}

	bool PrepareOpenPhysicalAuthorityForPersistence(
		HST_CampaignState state,
		out string failure)
	{
		return PrepareOpenPhysicalAuthority(state, failure);
	}

	bool PrepareOpenPhysicalAuthorityForSettlement(
		HST_CampaignState state,
		out string failure)
	{
		return PrepareOpenPhysicalAuthority(state, failure);
	}

	bool SettleOpenOperationsForCampaignStop(
		HST_CampaignState state,
		string reason)
	{
		if (!state)
			return false;
		if (reason.IsEmpty())
			reason = "campaign phase no longer permits local security";
		bool changed;
		foreach (HST_LocalSecurityPatrolState patrol : state.m_aLocalSecurityPatrols)
		{
			if (!patrol || patrol.m_iContractVersion != EXACT_CONTRACT_VERSION
				|| patrol.m_sStatus != "active")
				continue;
			HST_OperationRecordState operation;
			HST_ForceManifestState manifest;
			HST_ForceSpawnResultState batch;
			HST_ActiveGroupState group;
			string failure = ResolveRuntimeGraph(state, patrol, operation, manifest, batch, group);
			if (!failure.IsEmpty())
			{
				changed = Quarantine(state, patrol, failure) || changed;
				continue;
			}
			int living = ResolveLivingRoster(manifest, batch, group, operation);
			if (living <= 0)
				changed = HandleDestroyed(state, patrol, operation, manifest, batch, group) || changed;
			else
				changed = RetireWithoutLoss(state, patrol,
					HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_CANCELLED,
					"campaign_stop", reason) || changed;
		}
		return changed;
	}

	bool ReconcileSettledRuntimeCleanup(HST_CampaignState state)
	{
		if (!state || !m_SpawnAdapter || !m_PhysicalWar)
			return false;
		bool changed;
		foreach (HST_LocalSecurityPatrolState patrol : state.m_aLocalSecurityPatrols)
		{
			if (!patrol)
				continue;
			if (patrol.m_iContractVersion == EXACT_CONTRACT_VERSION
				&& patrol.m_sStatus == "terminal")
			{
				changed = ReconcileTerminalPatrol(state, patrol) || changed;
				continue;
			}
			if (patrol.m_iContractVersion != QUARANTINED_CONTRACT_VERSION
				&& patrol.m_sStatus != "quarantined")
				continue;
			bool cleanupChanged;
			string cleanupFailure;
			TryCleanupQuarantinedAuthority(
				state,
				patrol,
				cleanupChanged,
				cleanupFailure);
			changed = cleanupChanged || changed;
		}
		return changed;
	}

	static bool IsOpenLocalSecurityPatrolSpawnBatch(
		HST_CampaignState state,
		HST_ForceSpawnResultState batch)
	{
		if (!state || !batch)
			return false;
		HST_OperationRecordState operation = state.FindOperation(batch.m_sOperationId);
		if (!IsOpenLocalSecurityPatrolOperation(operation)
			|| operation.m_sSpawnResultId != batch.m_sResultId)
			return false;
		HST_LocalSecurityPatrolState patrol = state.FindLocalSecurityPatrolById(operation.m_sLocalSecurityPatrolId);
		return patrol && patrol.m_iContractVersion == EXACT_CONTRACT_VERSION
			&& patrol.m_sStatus == "active" && patrol.m_sSpawnResultId == batch.m_sResultId
			&& patrol.m_sProjectionId == batch.m_sProjectionId;
	}

	protected bool ReconcileTerminalPatrol(
		HST_CampaignState state,
		HST_LocalSecurityPatrolState patrol)
	{
		HST_OperationRecordState operation;
		HST_ForceManifestState manifest;
		HST_ForceSpawnResultState batch;
		HST_ActiveGroupState group;
		string failure = ResolveTerminalGraph(state, patrol, operation, manifest, batch, group);
		if (!failure.IsEmpty())
			return Quarantine(state, patrol, failure);

		bool changed;
		bool destroyed = operation.m_eTerminalResult
			== HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_DESTROYED;
		if (destroyed && !patrol.m_bLossEventApplied)
		{
			if (patrol.m_iLivingInfantryCount != 0)
				return Quarantine(state, patrol, "destroyed local-security receipt retained survivors");
			if (!ApplyDestructionLossEvent(state, patrol, operation))
				return false;
			if (patrol.m_iContractVersion != EXACT_CONTRACT_VERSION
				|| !patrol.m_bLossEventApplied)
				return true;
			changed = true;
		}
		else if (!destroyed && patrol.m_bLossEventApplied)
			return Quarantine(state, patrol, "non-destruction local-security receipt claims political loss");

		if (batch || group || m_SpawnAdapter.CountHandlesForProjection(patrol.m_sProjectionId) > 0)
		{
			if (!batch || !group)
				return Quarantine(state, patrol, "terminal local-security runtime cleanup graph is incomplete");
			if (!RetireRuntimeIfPresent(state, patrol, batch, group, true))
				return false;
			changed = true;
		}
		return FinalizeSettledRuntime(state, patrol, operation, batch, group) || changed;
	}

	protected string ResolveTerminalGraph(
		HST_CampaignState state,
		HST_LocalSecurityPatrolState patrol,
		out HST_OperationRecordState operation,
		out HST_ForceManifestState manifest,
		out HST_ForceSpawnResultState batch,
		out HST_ActiveGroupState group)
	{
		operation = null;
		manifest = null;
		batch = null;
		group = null;
		if (!state || !IsValidPatrolEnvelopeIdentity(patrol, "terminal"))
			return "terminal local-security envelope identity conflicts";
		HST_ZoneState zone = state.FindZone(patrol.m_sZoneId);
		if (!zone || zone.m_sLocalSecurityPatrolId != patrol.m_sPatrolId)
			return "terminal local-security zone backlink conflicts";
		if (CountOperations(state, patrol) != 1 || CountManifests(state, patrol) != 1
			|| CountBatches(state, patrol) > 1 || CountGroups(state, patrol) > 1)
			return "terminal local-security compact graph identity is ambiguous";
		operation = state.FindOperation(patrol.m_sOperationId);
		manifest = state.FindForceManifest(patrol.m_sManifestId);
		batch = state.FindForceSpawnResult(patrol.m_sSpawnResultId);
		group = state.FindActiveGroup(patrol.m_sGroupId);
		if (!operation || !manifest)
			return "terminal local-security compact graph is incomplete";
		if ((CountBatches(state, patrol) == 1 && !batch)
			|| (CountGroups(state, patrol) == 1 && !group))
			return "terminal local-security runtime claimant conflicts";
		if (!IsLocalSecurityPatrolOperation(operation)
			|| operation.m_iContractVersion != EXACT_CONTRACT_VERSION
			|| operation.m_iProjectionContractVersion != EXACT_PROJECTION_CONTRACT_VERSION
			|| operation.m_sLocalSecurityPatrolId != patrol.m_sPatrolId
			|| operation.m_sOwnerFactionKey != patrol.m_sFactionKey)
			return "terminal local-security operation policy conflicts";
		if (operation.m_sOperationId != patrol.m_sOperationId
			|| operation.m_sManifestId != patrol.m_sManifestId
			|| operation.m_sSpawnResultId != patrol.m_sSpawnResultId
			|| operation.m_sForceId != patrol.m_sForceId
			|| operation.m_sProjectionId != patrol.m_sProjectionId)
			return "terminal local-security operation policy conflicts";
		if (operation.m_sGroupId != patrol.m_sGroupId
			|| operation.m_sOriginZoneId != patrol.m_sZoneId
			|| operation.m_sAssignmentZoneId != patrol.m_sZoneId
			|| operation.m_sAssignmentKind != ASSIGNMENT_KIND
			|| operation.m_sRecallPolicyId != RECALL_POLICY_ID
			|| operation.m_sSettlementPolicyId != SETTLEMENT_POLICY_ID)
			return "terminal local-security operation policy conflicts";
		if (operation.m_eSettlementState
				!= HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_SETTLED
			|| operation.m_eDutyState != HST_EOperationDutyState.HST_OPERATION_DUTY_SETTLED
			|| operation.m_eResumeDutyState != HST_EOperationDutyState.HST_OPERATION_DUTY_SETTLED
			|| operation.m_eEngagementMode != HST_EOperationEngagementMode.HST_OPERATION_ENGAGEMENT_CLEAR)
			return "terminal local-security settlement receipt conflicts";
		if (operation.m_eMaterializationState
				!= HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_RETIRED
			|| operation.m_ePositionAuthority
				!= HST_EOperationPositionAuthority.HST_OPERATION_POSITION_STRATEGIC
			|| operation.m_eTerminalResult == HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_NONE
			|| operation.m_eTerminalResult == HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_UNKNOWN)
			return "terminal local-security settlement receipt conflicts";
		string expectedSettlementId = HST_OperationService.BuildSettlementId(
			operation.m_sOperationId,
			SETTLEMENT_KIND);
		if (operation.m_sSettlementId != expectedSettlementId
			|| operation.m_iLastVirtualFriendlyCount != patrol.m_iLivingInfantryCount
			|| operation.m_iSettledAtSecond != patrol.m_iTerminalAtSecond
			|| operation.m_sTerminalReason != patrol.m_sTerminalReason)
			return "terminal local-security settlement receipt conflicts";
		if (operation.m_eTerminalResult != HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_DESTROYED
			&& operation.m_eTerminalResult != HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_CANCELLED
			&& operation.m_eTerminalResult != HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_INVALIDATED
			&& operation.m_eTerminalResult != HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_SPAWN_FAILED)
			return "terminal local-security result is unsupported";
		if (!ValidateManifest(patrol, manifest))
			return "terminal local-security frozen manifest conflicts";
		if (batch)
		{
			if (batch.m_sResultId != patrol.m_sSpawnResultId
				|| batch.m_sRequestId != patrol.m_sPatrolId
				|| batch.m_sOperationId != patrol.m_sOperationId
				|| batch.m_sManifestId != patrol.m_sManifestId)
				return "terminal local-security batch backlink conflicts";
			if (batch.m_sManifestHash != patrol.m_sManifestHash
				|| batch.m_sForceId != patrol.m_sForceId
				|| batch.m_sProjectionId != patrol.m_sProjectionId)
				return "terminal local-security batch backlink conflicts";
		}
		if (group)
		{
			if (group.m_sGroupId != patrol.m_sGroupId
				|| group.m_sLocalSecurityPatrolId != patrol.m_sPatrolId
				|| group.m_sOperationId != patrol.m_sOperationId
				|| group.m_sManifestId != patrol.m_sManifestId)
				return "terminal local-security group backlink conflicts";
			if (group.m_sSpawnResultId != patrol.m_sSpawnResultId
				|| group.m_sForceId != patrol.m_sForceId
				|| group.m_sProjectionId != patrol.m_sProjectionId)
				return "terminal local-security group backlink conflicts";
		}
		return "";
	}

	protected bool PrepareOpenPhysicalAuthority(
		HST_CampaignState state,
		out string failure)
	{
		failure = "";
		if (!state || !m_SpawnQueue || !m_SpawnAdapter || !m_PhysicalWar)
		{
			failure = "local-security physical-authority preparation requires runtime services";
			return false;
		}
		foreach (HST_LocalSecurityPatrolState patrol : state.m_aLocalSecurityPatrols)
		{
			if (!patrol || patrol.m_iContractVersion != EXACT_CONTRACT_VERSION
				|| patrol.m_sStatus != "active")
				continue;
			HST_OperationRecordState operation;
			HST_ForceManifestState manifest;
			HST_ForceSpawnResultState batch;
			HST_ActiveGroupState group;
			failure = ResolveRuntimeGraph(state, patrol, operation, manifest, batch, group);
			if (!failure.IsEmpty())
				return false;
			if (operation.m_eMaterializationState
				== HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_VIRTUAL)
				continue;
			if (operation.m_eMaterializationState
				== HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_MATERIALIZING)
			{
				failure = "local-security materialization is not a persistable authority boundary";
				return false;
			}
			if (operation.m_eMaterializationState
				!= HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_PHYSICAL
				&& operation.m_eMaterializationState
					!= HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_DEMATERIALIZING)
			{
				failure = "local-security physical preparation found an invalid lifecycle state";
				return false;
			}
			HST_ForceSpawnAdapterTickResult reconciled = m_SpawnAdapter.ReconcileExactInfantryProjectionAuthority(
				state, m_SpawnQueue, m_PhysicalWar,
				Math.Max(0, state.m_iElapsedSeconds), patrol.m_sProjectionId);
			if (!reconciled || reconciled.m_iFailedCount > 0)
			{
				failure = "local-security physical casualties could not be reconciled";
				return false;
			}
			int living = m_SpawnQueue.CountDurableLivingMemberSlots(batch);
			SyncRoster(state, patrol, operation, group, living);
			if (living <= 0)
			{
				if (!HandleDestroyed(state, patrol, operation, manifest, batch, group))
				{
					failure = "local-security zero-survivor authority could not settle";
					return false;
				}
				continue;
			}
			string bindingFailure;
			if (!m_SpawnAdapter.ValidateExactLivingProjectionBindingsForPersistence(
				state, batch, m_SpawnQueue, m_PhysicalWar, bindingFailure))
			{
				failure = "local-security physical bindings are incomplete: " + bindingFailure;
				return false;
			}
			vector livePosition;
			string liveEvidence;
			if (!m_PhysicalWar.TryResolveExactLocalSecurityPatrolLivePosition(
				state, group, livePosition, liveEvidence))
			{
				failure = "local-security live position is unavailable: " + liveEvidence;
				return false;
			}
			bool groupPositionChanged = HasPersistencePositionChanged(
				group.m_vPosition,
				livePosition);
			bool operationPositionChanged = HasPersistencePositionChanged(
				operation.m_vStrategicPosition,
				livePosition);
			if (groupPositionChanged)
				group.m_vPosition = livePosition;
			if (operationPositionChanged)
				operation.m_vStrategicPosition = livePosition;
			if (!groupPositionChanged && !operationPositionChanged)
				continue;
			operation.m_iLastProgressAtSecond = Math.Max(0, state.m_iElapsedSeconds);
			operation.m_iRevision++;
		}
		return true;
	}

	protected bool HasPersistencePositionChanged(vector current, vector observed)
	{
		float deltaX = current[0] - observed[0];
		float deltaZ = current[2] - observed[2];
		return deltaX * deltaX + deltaZ * deltaZ
			>= PERSISTENCE_POSITION_UPDATE_THRESHOLD_SQ_METERS;
	}
}
