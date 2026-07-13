class HST_EnemyGarrisonRebuildSaveValidationService
{
	static const int SCHEMA_VERSION = 70;
	static const int QUARANTINED_CONTRACT_VERSION = -70;
	static const string QUARANTINE_PREFIX
		= "Schema 70 exact enemy garrison rebuild authority quarantined without guessed cleanup or refund: ";
	protected HST_CampaignSaveData m_SaveData;
	protected bool m_bPrepared;

	void PrepareBeforeGenericNormalization(
		HST_CampaignSaveData saveData,
		int restoredSchemaVersion)
	{
		m_bPrepared = false;
		Normalize(saveData, restoredSchemaVersion);
		m_bPrepared = true;
	}

	void Normalize(HST_CampaignSaveData saveData, int restoredSchemaVersion)
	{
		m_SaveData = saveData;
		if (!m_SaveData)
			return;
		foreach (HST_EnemyOrderState order : m_SaveData.m_aEnemyOrders)
		{
			if (!order || order.m_eType
				!= HST_EEnemyOrderType.HST_ENEMY_ORDER_REBUILD_GARRISON
				|| order.m_iOperationContractVersion == 0)
				continue;
			if (restoredSchemaVersion < SCHEMA_VERSION)
			{
				Quarantine(order, "pre-schema-70 garrison rebuild carried a nonzero exact contract");
				continue;
			}
			if (order.m_iOperationContractVersion == QUARANTINED_CONTRACT_VERSION)
			{
				NormalizeQuarantined(order);
				continue;
			}
			string failure = ValidateAggregate(order);
			if (!failure.IsEmpty())
			{
				Quarantine(order, failure);
				continue;
			}
			// This same validator instance runs before and after the prerequisite
			// normalizers. The first pass performs the process-free restore fold;
			// the second pass revalidates cross-authority links without advancing
			// lifecycle revisions a second time.
			if (!m_bPrepared)
				NormalizeValidRuntime(order);
		}
		QuarantineOrphanExactClaimants();
	}

	static bool IsFullRefundSettlementKind(string settlementKind)
	{
		return settlementKind == "admission_failed_full"
			|| settlementKind == "restore_invalidated_full";
	}

	static bool IsSchema70QuarantinedBatchClaimant(
		HST_CampaignSaveData saveData,
		HST_ForceSpawnResultState batch)
	{
		if (!saveData || !batch)
			return false;
		foreach (HST_EnemyOrderState order : saveData.m_aEnemyOrders)
		{
			if (!order || order.m_eType
				!= HST_EEnemyOrderType.HST_ENEMY_ORDER_REBUILD_GARRISON
				|| order.m_iOperationContractVersion != QUARANTINED_CONTRACT_VERSION)
				continue;
			if (QuarantinedOrderClaimsBatch(order, batch))
				return true;
		}
		foreach (HST_OperationRecordState operation : saveData.m_aOperations)
		{
			if (!operation || operation.m_eType
				!= HST_EOperationType.HST_OPERATION_TYPE_ENEMY_GARRISON_REBUILD
				|| operation.m_iContractVersion != QUARANTINED_CONTRACT_VERSION)
				continue;
			if (QuarantinedOperationClaimsBatch(operation, batch))
				return true;
		}
		return false;
	}

	protected static bool QuarantinedOrderClaimsBatch(
		HST_EnemyOrderState order,
		HST_ForceSpawnResultState batch)
	{
		if (!order || !batch)
			return false;
		if (!order.m_sOrderId.IsEmpty() && batch.m_sRequestId == order.m_sOrderId)
			return true;
		if (!order.m_sSpawnResultId.IsEmpty()
			&& batch.m_sResultId == order.m_sSpawnResultId)
			return true;
		if (!order.m_sOperationId.IsEmpty()
			&& batch.m_sOperationId == order.m_sOperationId)
			return true;
		if (!order.m_sManifestId.IsEmpty()
			&& batch.m_sManifestId == order.m_sManifestId)
			return true;
		if (!order.m_sGroupId.IsEmpty()
			&& batch.m_sProjectionId == order.m_sGroupId)
			return true;
		if (order.m_sOperationId.IsEmpty())
			return false;
		if (batch.m_sForceId == "force_" + order.m_sOperationId)
			return true;
		return batch.m_sProjectionId == "projection_" + order.m_sOperationId;
	}

	protected static bool QuarantinedOperationClaimsBatch(
		HST_OperationRecordState operation,
		HST_ForceSpawnResultState batch)
	{
		if (!operation || !batch)
			return false;
		if (!operation.m_sEnemyOrderId.IsEmpty()
			&& batch.m_sRequestId == operation.m_sEnemyOrderId)
			return true;
		if (!operation.m_sSpawnResultId.IsEmpty()
			&& batch.m_sResultId == operation.m_sSpawnResultId)
			return true;
		if (!operation.m_sOperationId.IsEmpty()
			&& batch.m_sOperationId == operation.m_sOperationId)
			return true;
		if (!operation.m_sManifestId.IsEmpty()
			&& batch.m_sManifestId == operation.m_sManifestId)
			return true;
		if (!operation.m_sForceId.IsEmpty()
			&& batch.m_sForceId == operation.m_sForceId)
			return true;
		if (!operation.m_sProjectionId.IsEmpty()
			&& batch.m_sProjectionId == operation.m_sProjectionId)
			return true;
		return !operation.m_sGroupId.IsEmpty()
			&& batch.m_sProjectionId == operation.m_sGroupId;
	}

	static bool ResolveSettlementPolicy(
		string settlementKind,
		out HST_EOperationTerminalResult terminalResult,
		out bool fullRefund,
		out bool delivery)
	{
		terminalResult = HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_UNKNOWN;
		fullRefund = false;
		delivery = false;
		if (settlementKind
			== HST_OperationService.EXACT_ENEMY_GARRISON_REBUILD_DELIVERY_SETTLEMENT_KIND)
		{
			terminalResult = HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_NONE;
			delivery = true;
		}
		else if (settlementKind == "admission_failed_full")
		{
			terminalResult = HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_SPAWN_FAILED;
			fullRefund = true;
		}
		else if (settlementKind == "restore_invalidated_full")
		{
			terminalResult = HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_INVALIDATED;
			fullRefund = true;
		}
		else if (settlementKind == "virtual_projection_failed_survivors"
			|| settlementKind == "materialization_failed_survivors")
			terminalResult = HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_SPAWN_FAILED;
		else if (settlementKind == "route_failed_survivors")
			terminalResult = HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_ROUTE_FAILED;
		else if (settlementKind == "destroyed")
			terminalResult = HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_DESTROYED;
		else if (settlementKind == "campaign_stopped_survivors")
			terminalResult = HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_CANCELLED;
		else if (settlementKind == "returned_invalidated_survivors"
			|| settlementKind == "ownership_invalidated_survivors"
			|| settlementKind == "restore_invalidated_survivors"
			|| settlementKind == "invalidated_survivors")
			terminalResult = HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_INVALIDATED;
		else
			return false;
		return true;
	}

	static string ValidateTerminalResultPolicy(
		HST_EnemyOrderState order,
		HST_OperationRecordState operation)
	{
		if (!order || !operation)
			return "exact enemy garrison rebuild terminal policy context is incomplete";
		HST_EOperationTerminalResult expectedResult;
		bool fullRefund;
		bool delivery;
		if (!ResolveSettlementPolicy(
			order.m_sResourceSettlementKind,
			expectedResult,
			fullRefund,
			delivery))
			return "exact enemy garrison rebuild terminal settlement kind is unsupported";
		if (delivery)
		{
			bool deliveredTerminal = operation.m_eTerminalResult
				== HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_INVALIDATED
				|| operation.m_eTerminalResult
					== HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_DESTROYED
				|| operation.m_eTerminalResult
					== HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_CANCELLED;
			if (!deliveredTerminal)
				return "delivered exact enemy garrison rebuild terminal result conflicts";
		}
		else if (operation.m_eTerminalResult != expectedResult)
			return "exact enemy garrison rebuild terminal result conflicts with settlement kind";
		return "";
	}

	static string ValidateOriginalResourceDebitAuthority(
		array<ref HST_EnemyStrategicMutationState> mutations,
		HST_EnemyOrderState order)
	{
		if (!mutations || !order || order.m_iAttackCost != 0
			|| order.m_iSupportCost
				!= HST_OperationService.EXACT_ENEMY_GARRISON_REBUILD_SUPPORT_COST)
			return "exact enemy garrison rebuild debit context is invalid";
		string mutationId = "enemy_resource_debit_" + order.m_sOrderId;
		if (order.m_sOrderId.IsEmpty() || order.m_sResourceDebitMutationId != mutationId)
			return "exact enemy garrison rebuild debit identity conflicts";
		HST_EnemyStrategicMutationState debit;
		int identityCount;
		foreach (HST_EnemyStrategicMutationState mutation : mutations)
		{
			if (mutation && mutation.m_sMutationId == mutationId)
			{
				debit = mutation;
				identityCount++;
			}
		}
		if (identityCount != 1 || !debit || CountMutationClaimants(mutations, order, true) != 1)
			return "exact enemy garrison rebuild debit is missing or ambiguous";
		bool debitHeaderExact = debit.m_iContractVersion
			== HST_EnemyStrategicResourceService.CONTRACT_VERSION
			&& debit.m_bApplied && debit.m_sKind == "defense_support_debit"
			&& debit.m_sFactionKey == order.m_sFactionKey
			&& debit.m_sSourceId == order.m_sOrderId;
		bool debitBacklinksExact = debit.m_sOrderId == order.m_sOrderId
			&& debit.m_sOperationId == order.m_sOperationId
			&& debit.m_sManifestId == order.m_sManifestId
			&& debit.m_sZoneId == order.m_sTargetZoneId;
		bool debitDeltaExact = debit.m_iAttackDelta == 0
			&& debit.m_iSupportDelta == -order.m_iSupportCost
			&& debit.m_iAggressionDelta == 0
			&& debit.m_iCreatedAtSecond >= order.m_iCreatedAtSecond
			&& debit.m_sContributionHash.IsEmpty();
		if (!debitHeaderExact || !debitBacklinksExact || !debitDeltaExact)
			return "exact enemy garrison rebuild debit backlink, pool, or amount conflicts";
		string shapeFailure;
		if (!HST_EnemyStrategicResourceSaveValidationService.ValidateMutationShape(
			debit,
			shapeFailure))
			return "exact enemy garrison rebuild debit shape conflicts: " + shapeFailure;
		return "";
	}

	static string ValidatePreparedResourceSettlementTuple(HST_EnemyOrderState order)
	{
		if (!order || order.m_sOperationId.IsEmpty()
			|| order.m_sResourceSettlementKind.IsEmpty()
			|| order.m_sResourceSettlementId != HST_OperationService.BuildSettlementId(
				order.m_sOperationId,
				order.m_sResourceSettlementKind)
			|| order.m_iSettlementAcceptedMemberCount <= 0
			|| order.m_iSettlementSurvivorMemberCount < 0
			|| order.m_iSettlementSurvivorMemberCount
				> order.m_iSettlementAcceptedMemberCount)
			return "prepared exact enemy garrison rebuild settlement tuple is incomplete";
		HST_EOperationTerminalResult terminalResult;
		bool fullRefund;
		bool delivery;
		if (!ResolveSettlementPolicy(
			order.m_sResourceSettlementKind,
			terminalResult,
			fullRefund,
			delivery))
			return "prepared exact enemy garrison rebuild settlement kind is unsupported";
		int expectedSupportRefund;
		if (fullRefund)
			expectedSupportRefund = Math.Max(0, order.m_iSupportCost);
		else if (!delivery)
			expectedSupportRefund = Math.Max(0, order.m_iSupportCost)
				* order.m_iSettlementSurvivorMemberCount
				/ order.m_iSettlementAcceptedMemberCount;
		if (order.m_iRefundedAttackResources != 0
			|| order.m_iRefundedSupportResources != expectedSupportRefund
			|| order.m_sResourceRefundMutationId
				!= "enemy_resource_refund_" + order.m_sResourceSettlementId)
			return "prepared exact enemy garrison rebuild refund amount or identity conflicts";
		if (delivery && order.m_iSettlementSurvivorMemberCount <= 0)
			return "prepared exact enemy garrison rebuild delivery has no survivors";
		return "";
	}

	static string ValidateSettledResourceRefundAuthority(
		array<ref HST_EnemyStrategicMutationState> mutations,
		HST_EnemyOrderState order)
	{
		if (!mutations || !order || !order.m_bResourceSettlementApplied)
			return "settled exact enemy garrison rebuild refund context is incomplete";
		string tupleFailure = ValidatePreparedResourceSettlementTuple(order);
		if (!tupleFailure.IsEmpty())
			return tupleFailure;
		string debitFailure = ValidateOriginalResourceDebitAuthority(mutations, order);
		if (!debitFailure.IsEmpty())
			return debitFailure;
		HST_EnemyStrategicMutationState refund;
		HST_EnemyStrategicMutationState debit;
		int refundCount;
		foreach (HST_EnemyStrategicMutationState mutation : mutations)
		{
			if (!mutation)
				continue;
			if (mutation.m_sMutationId == order.m_sResourceRefundMutationId)
			{
				refund = mutation;
				refundCount++;
			}
			if (mutation.m_sMutationId == order.m_sResourceDebitMutationId)
				debit = mutation;
		}
		if (refundCount != 1 || !refund || !debit
			|| CountMutationClaimants(mutations, order, false) != 1)
			return "settled exact enemy garrison rebuild refund is missing or ambiguous";
		bool refundHeaderExact = refund.m_iContractVersion
			== HST_EnemyStrategicResourceService.CONTRACT_VERSION
			&& refund.m_bApplied && refund.m_sKind == "defense_support_refund"
			&& refund.m_sSourceId == order.m_sResourceSettlementId
			&& refund.m_sFactionKey == order.m_sFactionKey;
		bool refundBacklinksExact = refund.m_sOrderId == order.m_sOrderId
			&& refund.m_sOperationId == order.m_sOperationId
			&& refund.m_sManifestId == order.m_sManifestId
			&& refund.m_sZoneId == order.m_sTargetZoneId;
		bool refundDeltaExact = refund.m_iAttackDelta == 0
			&& refund.m_iSupportDelta == order.m_iRefundedSupportResources
			&& refund.m_iAggressionDelta == 0
			&& refund.m_iCreatedAtSecond >= debit.m_iCreatedAtSecond
			&& refund.m_sContributionHash.IsEmpty();
		if (!refundHeaderExact || !refundBacklinksExact || !refundDeltaExact)
			return "settled exact enemy garrison rebuild refund backlink, pool, or amount conflicts";
		string shapeFailure;
		if (!HST_EnemyStrategicResourceSaveValidationService.ValidateMutationShape(
			refund,
			shapeFailure))
			return "settled exact enemy garrison rebuild refund shape conflicts: " + shapeFailure;
		return "";
	}

	static string ValidatePendingResourceRefundAuthority(
		array<ref HST_EnemyStrategicMutationState> mutations,
		HST_EnemyOrderState order)
	{
		if (!mutations || !order || order.m_bResourceSettlementApplied)
			return "prepared exact enemy garrison rebuild pending refund context is incomplete";
		string tupleFailure = ValidatePreparedResourceSettlementTuple(order);
		if (!tupleFailure.IsEmpty())
			return tupleFailure;
		string debitFailure = ValidateOriginalResourceDebitAuthority(mutations, order);
		if (!debitFailure.IsEmpty())
			return debitFailure;
		HST_EnemyStrategicMutationState refund;
		HST_EnemyStrategicMutationState debit;
		int refundCount;
		foreach (HST_EnemyStrategicMutationState mutation : mutations)
		{
			if (!mutation)
				continue;
			if (mutation.m_sMutationId == order.m_sResourceRefundMutationId)
			{
				refund = mutation;
				refundCount++;
			}
			if (mutation.m_sMutationId == order.m_sResourceDebitMutationId)
				debit = mutation;
		}
		if (refundCount != 1 || !refund || !debit
			|| CountMutationClaimants(mutations, order, false) != 1)
			return "prepared exact enemy garrison rebuild pending refund is missing or ambiguous";
		bool refundHeaderExact = refund.m_iContractVersion
			== HST_EnemyStrategicResourceService.CONTRACT_VERSION
			&& refund.m_bApplied && refund.m_sKind == "defense_support_refund"
			&& refund.m_sSourceId == order.m_sResourceSettlementId
			&& refund.m_sFactionKey == order.m_sFactionKey;
		bool refundBacklinksExact = refund.m_sOrderId == order.m_sOrderId
			&& refund.m_sOperationId == order.m_sOperationId
			&& refund.m_sManifestId == order.m_sManifestId
			&& refund.m_sZoneId == order.m_sTargetZoneId;
		bool refundDeltaExact = refund.m_iAttackDelta == 0
			&& refund.m_iSupportDelta == order.m_iRefundedSupportResources
			&& refund.m_iAggressionDelta == 0
			&& refund.m_iCreatedAtSecond >= debit.m_iCreatedAtSecond
			&& refund.m_sContributionHash.IsEmpty();
		if (!refundHeaderExact || !refundBacklinksExact || !refundDeltaExact)
			return "prepared exact enemy garrison rebuild pending refund conflicts";
		string shapeFailure;
		if (!HST_EnemyStrategicResourceSaveValidationService.ValidateMutationShape(
			refund,
			shapeFailure))
			return "prepared exact enemy garrison rebuild pending refund shape conflicts: "
				+ shapeFailure;
		return "";
	}

	static string ValidatePendingResourceRefundAggregateAuthority(
		array<ref HST_EnemyStrategicMutationState> mutations,
		HST_EnemyOrderState order,
		HST_OperationRecordState operation,
		HST_ForceManifestState manifest,
		HST_ForceSpawnResultState batch,
		HST_ActiveGroupState group)
	{
		string pendingFailure = ValidatePendingResourceRefundAuthority(mutations, order);
		if (!pendingFailure.IsEmpty())
			return pendingFailure;
		if (order.m_iAttackCost != 0
			|| order.m_iSupportCost
				!= HST_OperationService.EXACT_ENEMY_GARRISON_REBUILD_SUPPORT_COST)
			return "prepared exact enemy garrison rebuild aggregate ledger conflicts";

		bool fullRollback = IsFullRefundSettlementKind(order.m_sResourceSettlementKind)
			&& !order.m_bStrategicServiceCommitted;
		if (fullRollback)
		{
			string fullGraphFailure = ValidateOptionalFullRefundGraphAuthority(
				order,
				operation,
				manifest,
				batch,
				group);
			if (!fullGraphFailure.IsEmpty() || !operation)
				return fullGraphFailure;
			return ValidateTerminalResultPolicy(order, operation);
		}

		if (!operation || !manifest || !batch || !group
			|| manifest.m_iAttackResourceCost != 0
			|| manifest.m_iSupportResourceCost != order.m_iSupportCost
			|| operation.m_sOperationId != order.m_sOperationId
			|| operation.m_sManifestId != manifest.m_sManifestId
			|| (operation.m_eSettlementState
				!= HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_PREPARED
				&& !(order.m_sResourceSettlementKind
					== HST_OperationService.EXACT_ENEMY_GARRISON_REBUILD_DELIVERY_SETTLEMENT_KIND
					&& operation.m_eSettlementState
						== HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_OPEN
					&& operation.m_eDutyState
						== HST_EOperationDutyState.HST_OPERATION_DUTY_ON_STATION))
			|| (operation.m_eSettlementState
				== HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_PREPARED
				&& operation.m_sSettlementId != order.m_sResourceSettlementId))
			return "prepared exact enemy garrison rebuild aggregate authority conflicts";
		if (operation.m_eSettlementState
			== HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_PREPARED)
		{
			string policyFailure = ValidateTerminalResultPolicy(order, operation);
			if (!policyFailure.IsEmpty())
				return policyFailure;
		}
		if (batch.m_sOperationId != order.m_sOperationId
			|| batch.m_sManifestId != manifest.m_sManifestId)
			return "prepared exact enemy garrison rebuild batch authority conflicts";
		if (group.m_sOperationId != order.m_sOperationId
			|| group.m_sManifestId != manifest.m_sManifestId)
			return "prepared exact enemy garrison rebuild group authority conflicts";
		return "";
	}

	static string ValidateOptionalFullRefundGraphAuthority(
		HST_EnemyOrderState order,
		HST_OperationRecordState operation,
		HST_ForceManifestState manifest,
		HST_ForceSpawnResultState batch,
		HST_ActiveGroupState group)
	{
		if (!order || order.m_iAttackCost != 0
			|| order.m_iSupportCost
				!= HST_OperationService.EXACT_ENEMY_GARRISON_REBUILD_SUPPORT_COST)
			return "prepared full-refund exact enemy garrison rebuild ledger conflicts";
		if (operation)
		{
			bool operationIdentityExact = operation.m_sOperationId == order.m_sOperationId
				&& operation.m_sEnemyOrderId == order.m_sOrderId
				&& operation.m_sManifestId == order.m_sManifestId
				&& operation.m_eType
					== HST_EOperationType.HST_OPERATION_TYPE_ENEMY_GARRISON_REBUILD;
			bool prepared = operation.m_eSettlementState
				== HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_PREPARED;
			bool settled = operation.m_eSettlementState
				== HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_SETTLED;
			bool settlementIdentityExact = (prepared || settled)
				&& operation.m_sSettlementId == order.m_sResourceSettlementId
				&& !operation.m_sTerminalReason.IsEmpty()
				&& operation.m_iSettledAtSecond > 0;
			if (!operationIdentityExact || !settlementIdentityExact)
				return "prepared full-refund exact enemy garrison rebuild operation authority conflicts";
			if (settled)
			{
				bool terminalShapeExact = operation.m_eDutyState
					== HST_EOperationDutyState.HST_OPERATION_DUTY_SETTLED
					&& operation.m_eResumeDutyState
						== HST_EOperationDutyState.HST_OPERATION_DUTY_SETTLED
					&& operation.m_eMaterializationState
						== HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_RETIRED
					&& operation.m_ePositionAuthority
						== HST_EOperationPositionAuthority.HST_OPERATION_POSITION_STRATEGIC;
				if (!terminalShapeExact)
					return "settled full-refund exact enemy garrison rebuild terminal shape conflicts";
			}
		}
		if (manifest && (manifest.m_sManifestId != order.m_sManifestId
			|| manifest.m_sOperationId != order.m_sOperationId
			|| manifest.m_sFactionKey != order.m_sFactionKey
			|| manifest.m_iAttackResourceCost != 0
			|| manifest.m_iSupportResourceCost != order.m_iSupportCost))
			return "prepared full-refund exact enemy garrison rebuild manifest authority conflicts";
		if (batch && (batch.m_sResultId != "spawn_" + order.m_sOrderId
			|| batch.m_sRequestId != order.m_sOrderId
			|| batch.m_sOperationId != order.m_sOperationId
			|| batch.m_sManifestId != order.m_sManifestId))
			return "prepared full-refund exact enemy garrison rebuild batch authority conflicts";
		if (group && (group.m_sGroupId != "projection_" + order.m_sOperationId
			|| group.m_sOperationId != order.m_sOperationId
			|| group.m_sEnemyOrderId != order.m_sOrderId
			|| group.m_sManifestId != order.m_sManifestId))
			return "prepared full-refund exact enemy garrison rebuild group authority conflicts";
		return "";
	}

	protected static int CountMutationClaimants(
		array<ref HST_EnemyStrategicMutationState> mutations,
		HST_EnemyOrderState order,
		bool debit)
	{
		int count;
		foreach (HST_EnemyStrategicMutationState mutation : mutations)
		{
			if (!mutation)
				continue;
			bool claims = mutation.m_sOrderId == order.m_sOrderId
				|| mutation.m_sOperationId == order.m_sOperationId
				|| mutation.m_sManifestId == order.m_sManifestId;
			if (!claims)
				continue;
			if (debit && mutation.m_sKind == "defense_support_debit")
				count++;
			if (!debit && mutation.m_sKind == "defense_support_refund")
				count++;
		}
		return count;
	}

	protected string ValidateAggregate(HST_EnemyOrderState order)
	{
		if (!order || order.m_iOperationContractVersion
			!= HST_OperationService.EXACT_ENEMY_GARRISON_REBUILD_CONTRACT_VERSION
			|| order.m_sOrderId.IsEmpty()
			|| order.m_sOperationId
				!= HST_StableIdService.BuildOperationId("enemy_order", order.m_sOrderId)
			|| order.m_sManifestId != "manifest_" + order.m_sOperationId
			|| order.m_sFactionKey.IsEmpty() || order.m_sSourceZoneId.IsEmpty()
			|| order.m_sTargetZoneId.IsEmpty() || order.m_iTargetOwnershipRevision <= 0
			|| order.m_iAttackCost != 0
			|| order.m_iSupportCost
				!= HST_OperationService.EXACT_ENEMY_GARRISON_REBUILD_SUPPORT_COST)
			return "exact enemy garrison rebuild order identity or ledger conflicts";
		if (CountOrders(order) != 1)
			return "exact enemy garrison rebuild order identity is ambiguous";
		string debitFailure = ValidateOriginalResourceDebitAuthority(
			m_SaveData.m_aEnemyStrategicMutations,
			order);
		if (!debitFailure.IsEmpty())
			return debitFailure;

		if (!order.m_bStrategicServiceCommitted
			&& IsFullRefundSettlementKind(order.m_sResourceSettlementKind))
		{
			int fullOperationCount = CountOperations(order);
			int fullManifestCount = CountManifests(order);
			int fullBatchCount = CountBatches(order);
			int fullGroupCount = CountGroups(order);
			if (fullOperationCount > 1 || fullManifestCount > 1
				|| fullBatchCount > 1 || fullGroupCount > 1
				|| (order.m_eStatus != HST_EEnemyOrderStatus.HST_ENEMY_ORDER_QUEUED
					&& order.m_eStatus != HST_EEnemyOrderStatus.HST_ENEMY_ORDER_ABORTED))
				return "full-refund exact enemy garrison rebuild authority is ambiguous";
			HST_OperationRecordState fullOperation = FindOperation(order);
			HST_ForceManifestState fullManifest = FindManifest(order);
			HST_ForceSpawnResultState fullBatch = FindBatch(order);
			HST_ActiveGroupState fullGroup = FindGroup(order);
			if ((fullOperationCount == 1 && !fullOperation)
				|| (fullManifestCount == 1 && !fullManifest)
				|| (fullBatchCount == 1 && !fullBatch)
				|| (fullGroupCount == 1 && !fullGroup))
				return "full-refund exact enemy garrison rebuild claimant backlinks conflict";
			string fullGraphFailure = ValidateOptionalFullRefundGraphAuthority(
				order,
				fullOperation,
				fullManifest,
				fullBatch,
				fullGroup);
			if (!fullGraphFailure.IsEmpty())
				return fullGraphFailure;
			if (fullOperation)
			{
				string fullPolicyFailure = ValidateTerminalResultPolicy(order, fullOperation);
				if (!fullPolicyFailure.IsEmpty())
					return fullPolicyFailure;
			}
			if (order.m_bResourceSettlementApplied)
				return ValidateSettledResourceRefundAuthority(
					m_SaveData.m_aEnemyStrategicMutations,
					order);
			int pendingFullClaims = CountMutationClaimants(
				m_SaveData.m_aEnemyStrategicMutations,
				order,
				false);
			if (pendingFullClaims == 0)
				return ValidatePreparedResourceSettlementTuple(order);
			if (pendingFullClaims == 1)
				return ValidatePendingResourceRefundAggregateAuthority(
					m_SaveData.m_aEnemyStrategicMutations,
					order,
					fullOperation,
					fullManifest,
					fullBatch,
					fullGroup);
			return "prepared full-refund exact enemy garrison rebuild refund is ambiguous";
		}

		HST_OperationRecordState operation = FindOperation(order);
		HST_ForceManifestState manifest = FindManifest(order);
		HST_ForceSpawnResultState batch = FindBatch(order);
		HST_ActiveGroupState group = FindGroup(order);
		int batchCount = CountBatches(order);
		int groupCount = CountGroups(order);
		if (CountOperations(order) != 1 || CountManifests(order) != 1
			|| !operation || !manifest)
			return "exact enemy garrison rebuild durable graph is incomplete or ambiguous";
		if (operation.m_eType
				!= HST_EOperationType.HST_OPERATION_TYPE_ENEMY_GARRISON_REBUILD
			|| operation.m_iContractVersion
				!= HST_OperationService.EXACT_ENEMY_GARRISON_REBUILD_CONTRACT_VERSION
			|| operation.m_sOperationId != order.m_sOperationId
			|| operation.m_sEnemyOrderId != order.m_sOrderId
			|| operation.m_sManifestId != manifest.m_sManifestId
			|| order.m_sSpawnResultId != "spawn_" + order.m_sOrderId
			|| order.m_sGroupId != "projection_" + order.m_sOperationId
			|| operation.m_sSpawnResultId != order.m_sSpawnResultId
			|| operation.m_sGroupId != order.m_sGroupId
			|| operation.m_sOwnerFactionKey != order.m_sFactionKey
			|| operation.m_sOriginZoneId != order.m_sSourceZoneId
			|| operation.m_sAssignmentZoneId != order.m_sTargetZoneId
			|| operation.m_sAssignmentKind
				!= HST_OperationService.EXACT_ENEMY_GARRISON_REBUILD_ASSIGNMENT_KIND
			|| operation.m_sRecallPolicyId
				!= HST_OperationService.EXACT_ENEMY_GARRISON_REBUILD_RECALL_POLICY
			|| operation.m_sSettlementPolicyId
				!= HST_OperationService.EXACT_ENEMY_GARRISON_REBUILD_SETTLEMENT_POLICY)
			return "exact enemy garrison rebuild operation authority conflicts";
		string manifestFailure = ValidateManifest(order, manifest);
		if (!manifestFailure.IsEmpty())
			return manifestFailure;

		if (operation.m_eSettlementState
			== HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_SETTLED)
		{
			string settledPolicyFailure = ValidateTerminalResultPolicy(order, operation);
			if (!settledPolicyFailure.IsEmpty())
				return settledPolicyFailure;
			if (batchCount != groupCount || (batchCount != 0 && batchCount != 1))
				return "settled exact enemy garrison rebuild retained partial runtime authority";
			if (batchCount == 1)
			{
				string settledRuntimeFailure = ValidateRuntimeBacklinks(
					order,
					manifest,
					batch,
					group);
				if (!settledRuntimeFailure.IsEmpty())
					return settledRuntimeFailure;
			}
			if (operation.m_eDutyState
					!= HST_EOperationDutyState.HST_OPERATION_DUTY_SETTLED
				|| operation.m_eMaterializationState
					!= HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_RETIRED
				|| operation.m_sSettlementId != order.m_sResourceSettlementId
				|| !order.m_bResourceSettlementApplied
				|| CountAcceptedManifestLinks(order.m_sTargetZoneId,
					order.m_sFactionKey, manifest.m_sManifestId) != 0)
				return "settled exact enemy garrison rebuild terminal authority conflicts";
			return ValidateSettledResourceRefundAuthority(
				m_SaveData.m_aEnemyStrategicMutations,
				order);
		}

		if (batchCount != 1 || groupCount != 1 || !batch || !group)
			return "exact enemy garrison rebuild reciprocal runtime graph is incomplete or ambiguous";
		string runtimeFailure = ValidateRuntimeBacklinks(order, manifest, batch, group);
		if (!runtimeFailure.IsEmpty())
			return runtimeFailure;

		if (operation.m_eSettlementState
			== HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_PREPARED)
		{
			string preparedPolicyFailure = ValidateTerminalResultPolicy(order, operation);
			if (!preparedPolicyFailure.IsEmpty())
				return preparedPolicyFailure;
			if (operation.m_sSettlementId != order.m_sResourceSettlementId
				|| operation.m_sTerminalReason.IsEmpty())
				return "prepared exact enemy garrison rebuild terminal intent conflicts";
			if (order.m_bResourceSettlementApplied)
				return ValidateSettledResourceRefundAuthority(
					m_SaveData.m_aEnemyStrategicMutations,
					order);
			int refundClaims = CountMutationClaimants(
				m_SaveData.m_aEnemyStrategicMutations,
				order,
				false);
			if (refundClaims == 0)
				return ValidatePreparedResourceSettlementTuple(order);
			return ValidatePendingResourceRefundAggregateAuthority(
				m_SaveData.m_aEnemyStrategicMutations,
				order,
				operation,
				manifest,
				batch,
				group);
		}
		if (operation.m_eSettlementState
			!= HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_OPEN)
			return "exact enemy garrison rebuild settlement state is invalid";

		bool delivered = order.m_sResourceSettlementKind
			== HST_OperationService.EXACT_ENEMY_GARRISON_REBUILD_DELIVERY_SETTLEMENT_KIND;
		if (delivered)
		{
			if (CountAcceptedManifestLinks(order.m_sTargetZoneId,
				order.m_sFactionKey, manifest.m_sManifestId) != 1)
				return "delivered exact enemy garrison rebuild backlink is missing or ambiguous";
			if (operation.m_eDutyState != HST_EOperationDutyState.HST_OPERATION_DUTY_ON_STATION)
				return "delivered exact enemy garrison rebuild is not on station";
			if (!order.m_bResourceSettlementApplied)
			{
				if (order.m_eStatus != HST_EEnemyOrderStatus.HST_ENEMY_ORDER_ACTIVE
					|| order.m_bOutcomeApplied)
					return "staged exact enemy garrison rebuild delivery lifecycle conflicts";
				string stagedFailure = ValidatePreparedResourceSettlementTuple(order);
				if (!stagedFailure.IsEmpty())
					return stagedFailure;
				int pendingClaims = CountMutationClaimants(
					m_SaveData.m_aEnemyStrategicMutations,
					order,
					false);
				if (pendingClaims > 1)
					return "staged exact enemy garrison rebuild delivery refund is ambiguous";
				if (pendingClaims == 1)
					return ValidatePendingResourceRefundAuthority(
						m_SaveData.m_aEnemyStrategicMutations,
						order);
			}
			else
			{
				string refundFailure = ValidateSettledResourceRefundAuthority(
					m_SaveData.m_aEnemyStrategicMutations,
					order);
				if (!refundFailure.IsEmpty())
					return refundFailure;
				bool fullyProjected = order.m_eStatus
					== HST_EEnemyOrderStatus.HST_ENEMY_ORDER_RESOLVED
					&& order.m_bOutcomeApplied
					&& group.m_sGarrisonZoneId == order.m_sTargetZoneId;
				bool receiptBeforeProjection = order.m_eStatus
					== HST_EEnemyOrderStatus.HST_ENEMY_ORDER_ACTIVE
					&& !order.m_bOutcomeApplied
					&& group.m_sGarrisonZoneId.IsEmpty();
				if (!fullyProjected && !receiptBeforeProjection)
					return "applied exact enemy garrison rebuild delivery lifecycle conflicts";
			}
		}
		else if (order.m_bResourceSettlementApplied)
			return "open pre-arrival exact enemy garrison rebuild retained terminal resource receipt";
		else if (operation.m_eDutyState == HST_EOperationDutyState.HST_OPERATION_DUTY_ON_STATION)
		{
			// Link-before-receipt crash prefix is retained only for one exact reciprocal graph.
			if (order.m_eStatus != HST_EEnemyOrderStatus.HST_ENEMY_ORDER_ACTIVE
				|| CountAcceptedManifestLinks(order.m_sTargetZoneId,
					order.m_sFactionKey, manifest.m_sManifestId) != 1)
				return "unreceipted exact enemy garrison rebuild on-station backlink conflicts";
		}
		else if (CountAcceptedManifestLinks(order.m_sTargetZoneId,
			order.m_sFactionKey, manifest.m_sManifestId) != 0)
			return "pre-arrival exact enemy garrison rebuild retained a garrison backlink";
		return "";
	}

	protected string ValidateRuntimeBacklinks(
		HST_EnemyOrderState order,
		HST_ForceManifestState manifest,
		HST_ForceSpawnResultState batch,
		HST_ActiveGroupState group)
	{
		if (!order || !manifest || !batch || !group)
			return "exact enemy garrison rebuild runtime backlinks or roster conflict";
		bool batchIdentityExact = batch.m_sResultId == "spawn_" + order.m_sOrderId
			&& batch.m_sRequestId == order.m_sOrderId
			&& batch.m_sOperationId == order.m_sOperationId
			&& batch.m_sManifestId == manifest.m_sManifestId;
		bool batchProjectionExact = batch.m_sForceId == "force_" + order.m_sOperationId
			&& batch.m_sProjectionId == "projection_" + order.m_sOperationId;
		bool groupProjectionExact = group.m_sGroupId == batch.m_sProjectionId
			&& group.m_sProjectionId == batch.m_sProjectionId
			&& group.m_sForceId == batch.m_sForceId
			&& group.m_sSpawnResultId == batch.m_sResultId;
		bool groupAuthorityExact = group.m_sOperationId == order.m_sOperationId
			&& group.m_sEnemyOrderId == order.m_sOrderId
			&& group.m_sManifestId == manifest.m_sManifestId
			&& group.m_sFactionKey == order.m_sFactionKey;
		bool groupRosterExact = group.m_iOriginalInfantryCount
			== manifest.m_iAcceptedMemberCount
			&& group.m_iVehicleCount == 0;
		if (!batchIdentityExact || !batchProjectionExact || !groupProjectionExact
			|| !groupAuthorityExact || !groupRosterExact)
			return "exact enemy garrison rebuild runtime backlinks or roster conflict";
		return "";
	}

	protected string ValidateManifest(
		HST_EnemyOrderState order,
		HST_ForceManifestState manifest)
	{
		HST_ForcePlanningIntegrityService integrity = new HST_ForcePlanningIntegrityService();
		HST_StrategicMovementService movement = new HST_StrategicMovementService();
		if (!order || !manifest)
			return "exact enemy garrison rebuild frozen manifest conflicts";
		bool hashExact = manifest.m_bFrozen && !manifest.m_sManifestHash.IsEmpty()
			&& integrity.BuildManifestHash(manifest) == manifest.m_sManifestHash
			&& order.m_sManifestHash == manifest.m_sManifestHash;
		bool policyExact = manifest.m_sForceKind
			== HST_OperationService.EXACT_ENEMY_GARRISON_REBUILD_FORCE_KIND
			&& manifest.m_sPolicyId
				== HST_OperationService.EXACT_ENEMY_GARRISON_REBUILD_POLICY_ID
			&& manifest.m_sIntentId
				== HST_OperationService.EXACT_ENEMY_GARRISON_REBUILD_MANIFEST_INTENT;
		bool authorityExact = manifest.m_sOperationId == order.m_sOperationId
			&& manifest.m_sFactionKey == order.m_sFactionKey
			&& manifest.m_sSourceZoneId == order.m_sSourceZoneId
			&& manifest.m_sTargetZoneId == order.m_sTargetZoneId;
		bool ledgerExact = manifest.m_iAttackResourceCost == 0
			&& manifest.m_iSupportResourceCost
				== HST_OperationService.EXACT_ENEMY_GARRISON_REBUILD_SUPPORT_COST
			&& manifest.m_iSupportResourceCost == order.m_iSupportCost;
		bool rosterExact = movement.IsSupportedExactInfantryManifest(manifest)
			&& manifest.m_aGroups.Count() == 1 && manifest.m_aGroups[0]
			&& manifest.m_aGroups[0].m_iExpectedMemberCount
				== manifest.m_iAcceptedMemberCount;
		if (!hashExact || !policyExact || !authorityExact || !ledgerExact || !rosterExact)
			return "exact enemy garrison rebuild frozen manifest conflicts";
		return "";
	}

	protected HST_OperationRecordState FindOperation(HST_EnemyOrderState order)
	{
		HST_OperationRecordState match;
		foreach (HST_OperationRecordState operation : m_SaveData.m_aOperations)
		{
			if (operation && operation.m_sOperationId == order.m_sOperationId)
				match = operation;
		}
		return match;
	}

	protected HST_ForceManifestState FindManifest(HST_EnemyOrderState order)
	{
		HST_ForceManifestState match;
		foreach (HST_ForceManifestState manifest : m_SaveData.m_aForceManifests)
		{
			if (manifest && manifest.m_sManifestId == order.m_sManifestId)
				match = manifest;
		}
		return match;
	}

	protected HST_ForceSpawnResultState FindBatch(HST_EnemyOrderState order)
	{
		HST_ForceSpawnResultState match;
		foreach (HST_ForceSpawnResultState batch : m_SaveData.m_aForceSpawnResults)
		{
			if (batch && batch.m_sResultId == order.m_sSpawnResultId)
				match = batch;
		}
		return match;
	}

	protected HST_ActiveGroupState FindGroup(HST_EnemyOrderState order)
	{
		HST_ActiveGroupState match;
		foreach (HST_ActiveGroupState group : m_SaveData.m_aActiveGroups)
		{
			if (group && group.m_sGroupId == order.m_sGroupId)
				match = group;
		}
		return match;
	}

	protected int CountOrders(HST_EnemyOrderState order)
	{
		int count;
		foreach (HST_EnemyOrderState candidate : m_SaveData.m_aEnemyOrders)
		{
			if (candidate && (candidate.m_sOrderId == order.m_sOrderId
				|| candidate.m_sOperationId == order.m_sOperationId
				|| candidate.m_sManifestId == order.m_sManifestId))
				count++;
		}
		return count;
	}

	protected int CountOperations(HST_EnemyOrderState order)
	{
		int count;
		foreach (HST_OperationRecordState operation : m_SaveData.m_aOperations)
		{
			if (operation && (operation.m_sOperationId == order.m_sOperationId
				|| operation.m_sEnemyOrderId == order.m_sOrderId
				|| operation.m_sManifestId == order.m_sManifestId))
				count++;
		}
		return count;
	}

	protected int CountManifests(HST_EnemyOrderState order)
	{
		int count;
		foreach (HST_ForceManifestState manifest : m_SaveData.m_aForceManifests)
		{
			if (manifest && (manifest.m_sManifestId == order.m_sManifestId
				|| manifest.m_sOperationId == order.m_sOperationId))
				count++;
		}
		return count;
	}

	protected int CountBatches(HST_EnemyOrderState order)
	{
		int count;
		foreach (HST_ForceSpawnResultState batch : m_SaveData.m_aForceSpawnResults)
		{
			if (batch && (batch.m_sResultId == order.m_sSpawnResultId
				|| batch.m_sRequestId == order.m_sOrderId
				|| batch.m_sOperationId == order.m_sOperationId
				|| batch.m_sManifestId == order.m_sManifestId))
				count++;
		}
		return count;
	}

	protected int CountGroups(HST_EnemyOrderState order)
	{
		int count;
		foreach (HST_ActiveGroupState group : m_SaveData.m_aActiveGroups)
		{
			if (group && (group.m_sGroupId == order.m_sGroupId
				|| group.m_sEnemyOrderId == order.m_sOrderId
				|| group.m_sOperationId == order.m_sOperationId
				|| group.m_sManifestId == order.m_sManifestId))
				count++;
		}
		return count;
	}

	protected int CountAcceptedManifestLinks(
		string zoneId,
		string factionKey,
		string manifestId)
	{
		int count;
		foreach (HST_GarrisonState garrison : m_SaveData.m_aGarrisons)
		{
			if (!garrison || garrison.m_sZoneId != zoneId
				|| garrison.m_sFactionKey != factionKey)
				continue;
			foreach (string acceptedId : garrison.m_aAcceptedManifestIds)
			{
				if (acceptedId == manifestId)
					count++;
			}
		}
		return count;
	}

	protected void NormalizeValidRuntime(HST_EnemyOrderState order)
	{
		HST_OperationRecordState operation = FindOperation(order);
		HST_ForceSpawnResultState batch = FindBatch(order);
		HST_ActiveGroupState group = FindGroup(order);
		if (!operation || operation.m_eSettlementState
			== HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_PREPARED)
			return;
		if (operation.m_eSettlementState
			== HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_SETTLED)
		{
			NormalizeTerminalRuntime(batch, group);
			return;
		}
		int restoreSecond = Math.Max(0, m_SaveData.m_iElapsedSeconds);
		bool savedLive = operation.m_ePositionAuthority
			== HST_EOperationPositionAuthority.HST_OPERATION_POSITION_LIVE
			&& (operation.m_eMaterializationState
				== HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_PHYSICAL
				|| operation.m_eMaterializationState
					== HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_DEMATERIALIZING
				|| operation.m_eMaterializationState
					== HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_MATERIALIZING);
		if (savedLive && group && !IsZeroVector(group.m_vPosition))
		{
			operation.m_vStrategicPosition = group.m_vPosition;
			HST_StrategicMovementService movement = new HST_StrategicMovementService();
			movement.SyncRouteProgressFromPosition(operation, group.m_vPosition);
		}
		else if (group && !IsZeroVector(operation.m_vStrategicPosition))
			group.m_vPosition = operation.m_vStrategicPosition;
		operation.m_eMaterializationState
			= HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_VIRTUAL;
		operation.m_ePositionAuthority
			= HST_EOperationPositionAuthority.HST_OPERATION_POSITION_STRATEGIC;
		operation.m_iMaterializationStateEnteredAtSecond = restoreSecond;
		operation.m_iStrategicLastUpdateSecond = restoreSecond;
		operation.m_iVirtualCombatLastStepSecond = restoreSecond;
		operation.m_iLastProgressAtSecond = restoreSecond;
		operation.m_iArrivalConfirmationCount = 0;
		operation.m_iLastArrivalConfirmationSecond = 0;
		operation.m_sLastProjectionReason
			= "restored exact enemy garrison rebuild as strategic authority";
		operation.m_iRevision++;
		NormalizeBatchForStrategicHold(batch, restoreSecond);
		NormalizeGroupForStrategicHold(operation, batch, group);
		order.m_bPhysicalized = false;
		if (order.m_sResourceSettlementKind
			== HST_OperationService.EXACT_ENEMY_GARRISON_REBUILD_DELIVERY_SETTLEMENT_KIND)
			order.m_sRuntimeStatus = "resolved_exact_rebuild_restore_virtual";
		else
			order.m_sRuntimeStatus = "exact_rebuild_restore_virtual";
	}

	protected void NormalizeBatchForStrategicHold(
		HST_ForceSpawnResultState batch,
		int restoreSecond)
	{
		if (!batch || batch.m_eStatus == HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_FAILED_FINAL
			|| batch.m_eStatus == HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_CANCELLED)
			return;
		bool wasPhysical = batch.m_eStatus
			== HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_SUCCEEDED
			&& !batch.m_bStrategicProjectionHeld;
		batch.m_sNativeGroupId = "";
		batch.m_eStatus = HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_PENDING;
		batch.m_bStrategicProjectionHeld = true;
		if (wasPhysical)
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
		HST_ForceSpawnResultState batch,
		HST_ActiveGroupState group)
	{
		if (!group)
			return;
		group.m_bSpawnedEntity = false;
		group.m_bSpawnAttempted = false;
		group.m_sRuntimeEntityId = "";
		group.m_iSpawnedAgentCount = 0;
		group.m_iAssignedWaypointCount = 0;
		if (operation.m_eDutyState == HST_EOperationDutyState.HST_OPERATION_DUTY_ON_STATION)
			group.m_sRuntimeStatus = "enemy_garrison_rebuild_virtual_on_station";
		else
			group.m_sRuntimeStatus = "enemy_garrison_rebuild_virtual";
		if (!batch)
			return;
		HST_ForceSpawnQueueService queue = new HST_ForceSpawnQueueService();
		int living = queue.CountStrategicLivingMemberSlots(batch);
		operation.m_iLastVirtualFriendlyCount = living;
		group.m_iInfantryCount = living;
		group.m_iLastSeenAliveCount = living;
		group.m_iSurvivorInfantryCount = living;
		group.m_iDurableLivingInfantryCount = living;
	}

	protected void NormalizeTerminalRuntime(
		HST_ForceSpawnResultState batch,
		HST_ActiveGroupState group)
	{
		if (batch)
			batch.m_bStrategicProjectionHeld = false;
		if (!group)
			return;
		group.m_bSpawnedEntity = false;
		group.m_bSpawnAttempted = false;
		group.m_sRuntimeEntityId = "";
		group.m_iSpawnedAgentCount = 0;
		group.m_iAssignedWaypointCount = 0;
		group.m_sRuntimeStatus = "exact_garrison_rebuild_terminal";
	}

	protected void Quarantine(HST_EnemyOrderState order, string reason)
	{
		if (!order)
			return;
		string durableReason = reason;
		if (!durableReason.StartsWith(QUARANTINE_PREFIX))
			durableReason = QUARANTINE_PREFIX + durableReason;
		order.m_iOperationContractVersion = QUARANTINED_CONTRACT_VERSION;
		order.m_eStatus = HST_EEnemyOrderStatus.HST_ENEMY_ORDER_ABORTED;
		order.m_bPhysicalized = false;
		order.m_sRuntimeStatus = "exact_garrison_rebuild_quarantined";
		order.m_sFailureReason = durableReason;
		foreach (HST_OperationRecordState operation : m_SaveData.m_aOperations)
		{
			if (OperationClaimsOrder(operation, order))
				NormalizeQuarantinedOperation(operation, durableReason);
		}
		int restoreSecond = Math.Max(0, m_SaveData.m_iElapsedSeconds);
		foreach (HST_ForceSpawnResultState batch : m_SaveData.m_aForceSpawnResults)
		{
			if (BatchClaimsOrderAggregate(batch, order)
				&& BatchRequiresStrategicHoldNormalization(batch, restoreSecond))
				NormalizeBatchForStrategicHold(batch, restoreSecond);
		}
		foreach (HST_ActiveGroupState group : m_SaveData.m_aActiveGroups)
		{
			if (GroupClaimsOrderAggregate(group, order))
				NormalizeQuarantinedGroup(group, durableReason);
		}
	}

	protected void NormalizeQuarantined(HST_EnemyOrderState order)
	{
		if (!order)
			return;
		if (m_bPrepared)
			return;
		string reason = order.m_sFailureReason;
		if (reason.IsEmpty())
			reason = "quarantined exact enemy garrison rebuild retained its evidence";
		Quarantine(order, reason);
	}

	protected void QuarantineOrphanExactClaimants()
	{
		foreach (HST_OperationRecordState operation : m_SaveData.m_aOperations)
		{
			if (!operation || operation.m_eType
				!= HST_EOperationType.HST_OPERATION_TYPE_ENEMY_GARRISON_REBUILD)
				continue;
			HST_EnemyOrderState reciprocal;
			int claims;
			foreach (HST_EnemyOrderState order : m_SaveData.m_aEnemyOrders)
			{
				if (OperationClaimsOrder(operation, order))
				{
					reciprocal = order;
					claims++;
				}
			}
			if (claims == 1 && reciprocal)
				continue;
			string durableReason
				= "Schema 70 orphan exact enemy garrison rebuild authority quarantined";
			NormalizeQuarantinedOperation(operation, durableReason);
			int restoreSecond = Math.Max(0, m_SaveData.m_iElapsedSeconds);
			foreach (HST_ForceSpawnResultState batch : m_SaveData.m_aForceSpawnResults)
			{
				if (BatchClaimsOperation(batch, operation)
					&& BatchRequiresStrategicHoldNormalization(batch, restoreSecond))
					NormalizeBatchForStrategicHold(batch, restoreSecond);
			}
			foreach (HST_ActiveGroupState group : m_SaveData.m_aActiveGroups)
			{
				if (GroupClaimsOperation(group, operation))
					NormalizeQuarantinedGroup(group, durableReason);
			}
		}
	}

	protected bool IsNonterminalBatch(HST_ForceSpawnResultState batch)
	{
		return batch
			&& batch.m_eStatus != HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_SUCCEEDED
			&& batch.m_eStatus != HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_FAILED_FINAL
			&& batch.m_eStatus != HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_CANCELLED;
	}

	protected bool BatchRequiresStrategicHoldNormalization(
		HST_ForceSpawnResultState batch,
		int restoreSecond)
	{
		if (!IsNonterminalBatch(batch))
			return false;
		if (batch.m_eStatus != HST_EForceSpawnBatchStatus.HST_FORCE_SPAWN_PENDING
			|| !batch.m_bStrategicProjectionHeld || !batch.m_sNativeGroupId.IsEmpty()
			|| batch.m_iStrategicHoldSinceSecond != restoreSecond
			|| batch.m_iNextAttemptSecond != 0 || batch.m_iCompletedAtSecond != 0)
			return true;
		foreach (HST_ForceSpawnSlotResultState slot : batch.m_aSlotResults)
		{
			if (!slot)
				continue;
			bool confirmedCasualty = slot.m_eStatus
				== HST_EForceSpawnSlotStatus.HST_FORCE_SLOT_RETIRED
				&& slot.m_bCasualtyConfirmed;
			if ((!confirmedCasualty
					&& slot.m_eStatus != HST_EForceSpawnSlotStatus.HST_FORCE_SLOT_QUEUED)
				|| !slot.m_sSpawnedPrefab.IsEmpty() || !slot.m_sEntityId.IsEmpty()
				|| !slot.m_sAssignedVehicleEntityId.IsEmpty()
				|| !slot.m_sNativeGroupId.IsEmpty() || slot.m_bAliveVerified
				|| slot.m_bFactionVerified || slot.m_bGroupVerified
				|| slot.m_bGameMasterVerified || slot.m_bProjectionVerified
				|| slot.m_bSeatVerified || slot.m_iUpdatedAtSecond != restoreSecond)
				return true;
		}
		return false;
	}

	protected bool OperationClaimsOrder(
		HST_OperationRecordState operation,
		HST_EnemyOrderState order)
	{
		if (!operation || !order || operation.m_eType
			!= HST_EOperationType.HST_OPERATION_TYPE_ENEMY_GARRISON_REBUILD)
			return false;
		if (!order.m_sOrderId.IsEmpty()
			&& operation.m_sEnemyOrderId == order.m_sOrderId)
			return true;
		if (!order.m_sOperationId.IsEmpty()
			&& operation.m_sOperationId == order.m_sOperationId)
			return true;
		if (!order.m_sManifestId.IsEmpty()
			&& operation.m_sManifestId == order.m_sManifestId)
			return true;
		if (!order.m_sSpawnResultId.IsEmpty()
			&& operation.m_sSpawnResultId == order.m_sSpawnResultId)
			return true;
		if (!order.m_sGroupId.IsEmpty()
			&& operation.m_sGroupId == order.m_sGroupId)
			return true;
		if (!order.m_sGroupId.IsEmpty()
			&& operation.m_sProjectionId == order.m_sGroupId)
			return true;
		if (order.m_sOperationId.IsEmpty())
			return false;
		if (operation.m_sForceId == "force_" + order.m_sOperationId)
			return true;
		return operation.m_sProjectionId == "projection_" + order.m_sOperationId;
	}

	protected bool BatchClaimsOrderAggregate(
		HST_ForceSpawnResultState batch,
		HST_EnemyOrderState order)
	{
		if (!batch || !order)
			return false;
		if (!order.m_sOrderId.IsEmpty() && batch.m_sRequestId == order.m_sOrderId)
			return true;
		if (!order.m_sSpawnResultId.IsEmpty()
			&& batch.m_sResultId == order.m_sSpawnResultId)
			return true;
		if (!order.m_sOperationId.IsEmpty()
			&& batch.m_sOperationId == order.m_sOperationId)
			return true;
		if (!order.m_sManifestId.IsEmpty()
			&& batch.m_sManifestId == order.m_sManifestId)
			return true;
		if (!order.m_sGroupId.IsEmpty()
			&& batch.m_sProjectionId == order.m_sGroupId)
			return true;
		if (!order.m_sOperationId.IsEmpty()
			&& batch.m_sForceId == "force_" + order.m_sOperationId)
			return true;
		if (!order.m_sOperationId.IsEmpty()
			&& batch.m_sProjectionId == "projection_" + order.m_sOperationId)
			return true;
		foreach (HST_OperationRecordState operation : m_SaveData.m_aOperations)
		{
			if (OperationClaimsOrder(operation, order)
				&& BatchClaimsOperation(batch, operation))
				return true;
		}
		return false;
	}

	protected bool GroupClaimsOrderAggregate(
		HST_ActiveGroupState group,
		HST_EnemyOrderState order)
	{
		if (!group || !order)
			return false;
		if (!order.m_sOrderId.IsEmpty() && group.m_sEnemyOrderId == order.m_sOrderId)
			return true;
		if (!order.m_sSpawnResultId.IsEmpty()
			&& group.m_sSpawnResultId == order.m_sSpawnResultId)
			return true;
		if (!order.m_sOperationId.IsEmpty()
			&& group.m_sOperationId == order.m_sOperationId)
			return true;
		if (!order.m_sManifestId.IsEmpty()
			&& group.m_sManifestId == order.m_sManifestId)
			return true;
		if (!order.m_sGroupId.IsEmpty() && group.m_sGroupId == order.m_sGroupId)
			return true;
		if (!order.m_sGroupId.IsEmpty()
			&& group.m_sProjectionId == order.m_sGroupId)
			return true;
		if (!order.m_sOperationId.IsEmpty()
			&& group.m_sForceId == "force_" + order.m_sOperationId)
			return true;
		if (!order.m_sOperationId.IsEmpty()
			&& (group.m_sGroupId == "projection_" + order.m_sOperationId
				|| group.m_sProjectionId == "projection_" + order.m_sOperationId))
			return true;
		foreach (HST_OperationRecordState operation : m_SaveData.m_aOperations)
		{
			if (OperationClaimsOrder(operation, order)
				&& GroupClaimsOperation(group, operation))
				return true;
		}
		return false;
	}

	protected bool BatchClaimsOperation(
		HST_ForceSpawnResultState batch,
		HST_OperationRecordState operation)
	{
		if (!batch || !operation)
			return false;
		if (!operation.m_sEnemyOrderId.IsEmpty()
			&& batch.m_sRequestId == operation.m_sEnemyOrderId)
			return true;
		if (!operation.m_sSpawnResultId.IsEmpty()
			&& batch.m_sResultId == operation.m_sSpawnResultId)
			return true;
		if (!operation.m_sOperationId.IsEmpty()
			&& batch.m_sOperationId == operation.m_sOperationId)
			return true;
		if (!operation.m_sManifestId.IsEmpty()
			&& batch.m_sManifestId == operation.m_sManifestId)
			return true;
		if (!operation.m_sForceId.IsEmpty()
			&& batch.m_sForceId == operation.m_sForceId)
			return true;
		if (!operation.m_sProjectionId.IsEmpty()
			&& batch.m_sProjectionId == operation.m_sProjectionId)
			return true;
		return !operation.m_sGroupId.IsEmpty()
			&& batch.m_sProjectionId == operation.m_sGroupId;
	}

	protected bool GroupClaimsOperation(
		HST_ActiveGroupState group,
		HST_OperationRecordState operation)
	{
		if (!group || !operation)
			return false;
		if (!operation.m_sEnemyOrderId.IsEmpty()
			&& group.m_sEnemyOrderId == operation.m_sEnemyOrderId)
			return true;
		if (!operation.m_sSpawnResultId.IsEmpty()
			&& group.m_sSpawnResultId == operation.m_sSpawnResultId)
			return true;
		if (!operation.m_sOperationId.IsEmpty()
			&& group.m_sOperationId == operation.m_sOperationId)
			return true;
		if (!operation.m_sManifestId.IsEmpty()
			&& group.m_sManifestId == operation.m_sManifestId)
			return true;
		if (!operation.m_sForceId.IsEmpty()
			&& group.m_sForceId == operation.m_sForceId)
			return true;
		if (!operation.m_sProjectionId.IsEmpty()
			&& group.m_sProjectionId == operation.m_sProjectionId)
			return true;
		return !operation.m_sGroupId.IsEmpty()
			&& group.m_sGroupId == operation.m_sGroupId;
	}

	protected void NormalizeQuarantinedGroup(
		HST_ActiveGroupState group,
		string durableReason)
	{
		if (!group)
			return;
		string normalizedReason = durableReason;
		if (group.m_sRuntimeStatus == "exact_garrison_rebuild_quarantined"
			&& !group.m_sSpawnFailureReason.IsEmpty())
			normalizedReason = group.m_sSpawnFailureReason;
		bool changed = group.m_bSpawnedEntity || group.m_bSpawnAttempted
			|| !group.m_sRuntimeEntityId.IsEmpty() || group.m_iSpawnedAgentCount != 0
			|| group.m_iAssignedWaypointCount != 0
			|| group.m_sRuntimeStatus != "exact_garrison_rebuild_quarantined"
			|| group.m_sSpawnFailureReason != normalizedReason;
		group.m_bSpawnedEntity = false;
		group.m_bSpawnAttempted = false;
		group.m_sRuntimeEntityId = "";
		group.m_iSpawnedAgentCount = 0;
		group.m_iAssignedWaypointCount = 0;
		group.m_sRuntimeStatus = "exact_garrison_rebuild_quarantined";
		group.m_sSpawnFailureReason = normalizedReason;
		if (changed)
			group.m_iLifecycleRevision++;
	}

	protected void NormalizeQuarantinedOperation(
		HST_OperationRecordState operation,
		string durableReason)
	{
		if (!operation)
			return;
		bool changed = operation.m_iContractVersion != QUARANTINED_CONTRACT_VERSION
			|| operation.m_sLastProjectionReason.IsEmpty();
		if (!changed)
			return;
		operation.m_iContractVersion = QUARANTINED_CONTRACT_VERSION;
		operation.m_sLastProjectionReason = durableReason;
		operation.m_iRevision++;
	}

	protected bool IsZeroVector(vector value)
	{
		return Math.AbsFloat(value[0]) < 0.01
			&& Math.AbsFloat(value[1]) < 0.01
			&& Math.AbsFloat(value[2]) < 0.01;
	}
}
