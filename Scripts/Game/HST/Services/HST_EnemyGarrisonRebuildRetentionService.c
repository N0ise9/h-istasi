class HST_EnemyGarrisonRebuildRetentionService
{
	static void AddQuarantinedSpawnPins(
		HST_CampaignState state,
		HST_ForceSpawnQueueRetentionPins pins)
	{
		if (!state || !pins)
			return;
		foreach (HST_EnemyOrderState order : state.m_aEnemyOrders)
		{
			if (!order
				|| order.m_eType != HST_EEnemyOrderType.HST_ENEMY_ORDER_REBUILD_GARRISON
				|| order.m_iOperationContractVersion
					!= HST_EnemyGarrisonRebuildSaveValidationService.QUARANTINED_CONTRACT_VERSION)
				continue;
			AddPin(pins.m_aRequestIds, order.m_sOrderId);
			AddPin(pins.m_aResultIds, order.m_sSpawnResultId);
			AddPin(pins.m_aManifestIds, order.m_sManifestId);
			AddPin(pins.m_aOperationIds, order.m_sOperationId);
			foreach (HST_OperationRecordState operation : state.m_aOperations)
			{
				if (!OperationClaimsOrder(operation, order))
					continue;
				AddPin(pins.m_aResultIds, operation.m_sSpawnResultId);
				AddPin(pins.m_aManifestIds, operation.m_sManifestId);
				AddPin(pins.m_aOperationIds, operation.m_sOperationId);
				AddPin(pins.m_aForceIds, operation.m_sForceId);
				AddPin(pins.m_aProjectionIds, operation.m_sProjectionId);
			}
			foreach (HST_ForceManifestState manifest : state.m_aForceManifests)
			{
				if (!manifest)
					continue;
				bool claimsManifest = !manifest.m_sManifestId.IsEmpty()
					&& pins.m_aManifestIds.Contains(manifest.m_sManifestId);
				if (!claimsManifest)
					claimsManifest = !manifest.m_sOperationId.IsEmpty()
						&& pins.m_aOperationIds.Contains(manifest.m_sOperationId);
				if (!claimsManifest)
					continue;
				AddPin(pins.m_aManifestIds, manifest.m_sManifestId);
				AddPin(pins.m_aOperationIds, manifest.m_sOperationId);
			}
			foreach (HST_ForceSpawnResultState batch : state.m_aForceSpawnResults)
			{
				if (!batch)
					continue;
				bool claims = !order.m_sOrderId.IsEmpty()
					&& batch.m_sRequestId == order.m_sOrderId;
				if (!claims)
					claims = pins.Contains(batch);
				if (!claims)
					continue;
				AddPin(pins.m_aResultIds, batch.m_sResultId);
				AddPin(pins.m_aRequestIds, batch.m_sRequestId);
				AddPin(pins.m_aManifestIds, batch.m_sManifestId);
				AddPin(pins.m_aOperationIds, batch.m_sOperationId);
				AddPin(pins.m_aForceIds, batch.m_sForceId);
				AddPin(pins.m_aProjectionIds, batch.m_sProjectionId);
			}
		}

		// An orphan exact operation is itself quarantined evidence. It has no
		// canonical order from which to seed pins, so discover it directly and
		// retain every reciprocal queue identity without fabricating an order.
		foreach (HST_OperationRecordState orphanOperation : state.m_aOperations)
		{
			if (!orphanOperation
				|| orphanOperation.m_eType
					!= HST_EOperationType.HST_OPERATION_TYPE_ENEMY_GARRISON_REBUILD
				|| orphanOperation.m_iContractVersion
					!= HST_EnemyGarrisonRebuildSaveValidationService.QUARANTINED_CONTRACT_VERSION)
				continue;
			AddOperationPins(pins, orphanOperation);
		}
		foreach (HST_ForceManifestState orphanManifest : state.m_aForceManifests)
		{
			if (!orphanManifest)
				continue;
			bool claimsPinned = !orphanManifest.m_sManifestId.IsEmpty()
				&& pins.m_aManifestIds.Contains(orphanManifest.m_sManifestId);
			if (!claimsPinned)
				claimsPinned = !orphanManifest.m_sOperationId.IsEmpty()
					&& pins.m_aOperationIds.Contains(orphanManifest.m_sOperationId);
			if (!claimsPinned)
				continue;
			AddPin(pins.m_aManifestIds, orphanManifest.m_sManifestId);
			AddPin(pins.m_aOperationIds, orphanManifest.m_sOperationId);
		}
		foreach (HST_ForceSpawnResultState orphanBatch : state.m_aForceSpawnResults)
		{
			if (orphanBatch && pins.Contains(orphanBatch))
				AddBatchPins(pins, orphanBatch);
		}
	}

	protected static void AddOperationPins(
		HST_ForceSpawnQueueRetentionPins pins,
		HST_OperationRecordState operation)
	{
		AddPin(pins.m_aRequestIds, operation.m_sEnemyOrderId);
		AddPin(pins.m_aResultIds, operation.m_sSpawnResultId);
		AddPin(pins.m_aManifestIds, operation.m_sManifestId);
		AddPin(pins.m_aOperationIds, operation.m_sOperationId);
		AddPin(pins.m_aForceIds, operation.m_sForceId);
		AddPin(pins.m_aProjectionIds, operation.m_sProjectionId);
	}

	protected static void AddBatchPins(
		HST_ForceSpawnQueueRetentionPins pins,
		HST_ForceSpawnResultState batch)
	{
		AddPin(pins.m_aResultIds, batch.m_sResultId);
		AddPin(pins.m_aRequestIds, batch.m_sRequestId);
		AddPin(pins.m_aManifestIds, batch.m_sManifestId);
		AddPin(pins.m_aOperationIds, batch.m_sOperationId);
		AddPin(pins.m_aForceIds, batch.m_sForceId);
		AddPin(pins.m_aProjectionIds, batch.m_sProjectionId);
	}

	protected static bool OperationClaimsOrder(
		HST_OperationRecordState operation,
		HST_EnemyOrderState order)
	{
		if (!operation || !order
			|| operation.m_eType != HST_EOperationType.HST_OPERATION_TYPE_ENEMY_GARRISON_REBUILD)
			return false;
		return (!order.m_sOrderId.IsEmpty() && operation.m_sEnemyOrderId == order.m_sOrderId)
			|| (!order.m_sOperationId.IsEmpty() && operation.m_sOperationId == order.m_sOperationId)
			|| (!order.m_sManifestId.IsEmpty() && operation.m_sManifestId == order.m_sManifestId)
			|| (!order.m_sSpawnResultId.IsEmpty() && operation.m_sSpawnResultId == order.m_sSpawnResultId)
			|| (!order.m_sGroupId.IsEmpty() && operation.m_sGroupId == order.m_sGroupId);
	}

	protected static void AddPin(array<string> values, string value)
	{
		if (values && !value.IsEmpty() && !values.Contains(value))
			values.Insert(value);
	}
}
