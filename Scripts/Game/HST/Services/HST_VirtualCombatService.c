class HST_VirtualCombatResult
{
	bool m_bAccepted;
	bool m_bStateChanged;
	bool m_bFriendlyEliminated;
	bool m_bHostileEliminated;
	int m_iProcessedSteps;
	int m_iFriendlyCasualties;
	int m_iHostileCasualties;
	string m_sFailureReason;
	string m_sEvidence;
}

class HST_VirtualCombatService
{
	static const int COMBAT_STEP_SECONDS = 30;
	static const int MAX_COMBAT_STEPS_PER_TICK = 4;
	static const int DAMAGE_SCALE = 1000;
	static const int CASUALTY_THRESHOLD = 45000;

	protected ref HST_OperationService m_Operations = new HST_OperationService();

	HST_VirtualCombatResult TickExactPlayerQRF(
		HST_CampaignState state,
		HST_OperationRecordState operation,
		HST_SupportRequestState request,
		HST_ForceManifestState manifest,
		HST_ForceSpawnResultState batch,
		HST_ActiveGroupState group,
		HST_ForceSpawnQueueService queue)
	{
		HST_VirtualCombatResult result = new HST_VirtualCombatResult();
		string failure = ValidateContext(state, operation, request, manifest, batch, group, queue);
		if (!failure.IsEmpty())
		{
			result.m_sFailureReason = failure;
			return result;
		}
		result.m_bAccepted = true;
		if (operation.m_eDutyState != HST_EOperationDutyState.HST_OPERATION_DUTY_ON_STATION)
			return result;

		HST_ZoneState targetZone = state.FindZone(operation.m_sTacticalTargetZoneId);
		if (!targetZone || targetZone.m_sOwnerFactionKey.IsEmpty() || targetZone.m_sOwnerFactionKey == operation.m_sOwnerFactionKey)
		{
			result.m_bStateChanged = ClearEngagementIfNeeded(state, operation);
			result.m_bStateChanged = UpdateIdleCombatEvidence(
				operation,
				state.m_iElapsedSeconds,
				queue.CountStrategicLivingMemberSlots(batch),
				"no hostile target garrison") || result.m_bStateChanged;
			return result;
		}

		HST_GarrisonState hostile = state.FindGarrison(targetZone.m_sZoneId, targetZone.m_sOwnerFactionKey);
		int hostileInfantry;
		if (hostile)
			hostileInfantry = Math.Max(0, hostile.m_iInfantryCount);
		if (hostileInfantry <= 0)
		{
			result.m_bStateChanged = ClearEngagementIfNeeded(state, operation);
			result.m_bStateChanged = UpdateIdleCombatEvidence(
				operation,
				state.m_iElapsedSeconds,
				queue.CountStrategicLivingMemberSlots(batch),
				"hostile location has no abstract infantry presence") || result.m_bStateChanged;
			return result;
		}

		int livingFriendly = queue.CountStrategicLivingMemberSlots(batch);
		if (livingFriendly <= 0)
		{
			ApplyFriendlyElimination(group, operation, state.m_iElapsedSeconds);
			result.m_bFriendlyEliminated = true;
			result.m_bStateChanged = true;
			return result;
		}
		result.m_bStateChanged = EnterEngagementIfNeeded(state, operation) || result.m_bStateChanged;
		if (operation.m_iVirtualCombatLastStepSecond <= 0)
			operation.m_iVirtualCombatLastStepSecond = state.m_iElapsedSeconds;

		int availableSteps = Math.Max(0, state.m_iElapsedSeconds - operation.m_iVirtualCombatLastStepSecond) / COMBAT_STEP_SECONDS;
		int steps = Math.Min(MAX_COMBAT_STEPS_PER_TICK, availableSteps);
		for (int step = 0; step < steps; step++)
		{
			livingFriendly = queue.CountStrategicLivingMemberSlots(batch);
			hostileInfantry = Math.Max(0, hostile.m_iInfantryCount);
			if (livingFriendly <= 0 || hostileInfantry <= 0)
				break;

			int totalPower = Math.Max(1, livingFriendly + hostileInfantry);
			operation.m_iVirtualCombatHostileDamageCarry += livingFriendly * COMBAT_STEP_SECONDS * DAMAGE_SCALE / totalPower;
			operation.m_iVirtualCombatFriendlyDamageCarry += hostileInfantry * COMBAT_STEP_SECONDS * DAMAGE_SCALE / totalPower;
			operation.m_iVirtualCombatStepIndex++;
			operation.m_iVirtualCombatLastStepSecond += COMBAT_STEP_SECONDS;
			result.m_iProcessedSteps++;
			result.m_bStateChanged = true;

			if (operation.m_iVirtualCombatHostileDamageCarry >= CASUALTY_THRESHOLD && hostile.m_iInfantryCount > 0)
			{
				operation.m_iVirtualCombatHostileDamageCarry -= CASUALTY_THRESHOLD;
				hostile.m_iInfantryCount--;
				result.m_iHostileCasualties++;
				result.m_bStateChanged = true;
			}

			livingFriendly = queue.CountStrategicLivingMemberSlots(batch);
			if (operation.m_iVirtualCombatFriendlyDamageCarry >= CASUALTY_THRESHOLD && livingFriendly > 0)
			{
				string slotId = queue.SelectStrategicLivingMemberSlotId(
					batch,
					operation.m_iDeterministicSeed + operation.m_iVirtualCombatStepIndex * 97);
				HST_ForceSpawnQueueCallbackResult casualty = queue.ConfirmStrategicMemberCasualty(
					state.m_aForceSpawnResults,
					manifest,
					batch.m_sResultId,
					batch.m_sProjectionId,
					slotId,
					operation.m_iVirtualCombatLastStepSecond,
					"deterministic virtual combat at " + targetZone.m_sZoneId);
				if (!casualty || !casualty.m_bAccepted)
				{
					result.m_sFailureReason = "exact virtual casualty callback failed";
					if (casualty && !casualty.m_sFailureReason.IsEmpty())
						result.m_sFailureReason = result.m_sFailureReason + ": " + casualty.m_sFailureReason;
					return result;
				}
				operation.m_iVirtualCombatFriendlyDamageCarry -= CASUALTY_THRESHOLD;
				result.m_iFriendlyCasualties++;
				result.m_bStateChanged = true;
			}
		}

		livingFriendly = queue.CountStrategicLivingMemberSlots(batch);
		hostileInfantry = Math.Max(0, hostile.m_iInfantryCount);
		operation.m_iLastVirtualFriendlyCount = livingFriendly;
		operation.m_iLastVirtualHostileCount = hostileInfantry;
		operation.m_sLastVirtualCombatReason = string.Format(
			"deterministic %1s power steps %2 | friendly %3 hostile infantry %4 | losses %5/%6",
			COMBAT_STEP_SECONDS,
			result.m_iProcessedSteps,
			livingFriendly,
			hostileInfantry,
			result.m_iFriendlyCasualties,
			result.m_iHostileCasualties);
		result.m_sEvidence = operation.m_sLastVirtualCombatReason;
		if (result.m_iProcessedSteps > 0 || result.m_bStateChanged)
		{
			operation.m_iLastProgressAtSecond = Math.Max(operation.m_iLastProgressAtSecond, operation.m_iVirtualCombatLastStepSecond);
			operation.m_iRevision++;
			group.m_iLifecycleRevision++;
		}

		group.m_iDurableLivingInfantryCount = livingFriendly;
		group.m_iLastSeenAliveCount = livingFriendly;
		group.m_iSurvivorInfantryCount = livingFriendly;
		if (result.m_iFriendlyCasualties > 0)
			group.m_iLastCasualtySecond = operation.m_iVirtualCombatLastStepSecond;
		if (livingFriendly <= 0)
		{
			ApplyFriendlyElimination(group, operation, operation.m_iVirtualCombatLastStepSecond);
			result.m_bFriendlyEliminated = true;
			result.m_bStateChanged = true;
		}
		if (hostileInfantry <= 0)
		{
			result.m_bStateChanged = ClearEngagementIfNeeded(state, operation) || result.m_bStateChanged;
			result.m_bHostileEliminated = true;
		}
		return result;
	}

	protected string ValidateContext(
		HST_CampaignState state,
		HST_OperationRecordState operation,
		HST_SupportRequestState request,
		HST_ForceManifestState manifest,
		HST_ForceSpawnResultState batch,
		HST_ActiveGroupState group,
		HST_ForceSpawnQueueService queue)
	{
		if (!state || !operation || !request || !manifest || !batch || !group || !queue)
			return "virtual combat authority is incomplete";
		HST_StrategicMovementService movement = new HST_StrategicMovementService();
		if (!movement.IsSupportedExactInfantryManifest(manifest))
			return "virtual combat rejects vehicle, asset, empty, or multi-root manifests";
		if (!batch.m_bStrategicProjectionHeld
			|| operation.m_eMaterializationState != HST_EOperationMaterializationState.HST_OPERATION_MATERIALIZATION_VIRTUAL
			|| operation.m_ePositionAuthority != HST_EOperationPositionAuthority.HST_OPERATION_POSITION_STRATEGIC)
			return "virtual combat does not own strategic projection authority";
		if (operation.m_sOperationId != request.m_sOperationId || operation.m_sManifestId != manifest.m_sManifestId
			|| operation.m_sSpawnResultId != batch.m_sResultId || operation.m_sGroupId != group.m_sGroupId)
			return "virtual combat aggregate identity conflicts";
		return "";
	}

	protected bool EnterEngagementIfNeeded(HST_CampaignState state, HST_OperationRecordState operation)
	{
		if (!state || !operation)
			return false;
		bool changed;
		if (operation.m_eEngagementMode == HST_EOperationEngagementMode.HST_OPERATION_ENGAGEMENT_CLEAR)
		{
			HST_OperationTransitionResult contact = m_Operations.RecordEngagement(state, operation.m_sOperationId, HST_EOperationEngagementMode.HST_OPERATION_ENGAGEMENT_CONTACT);
			changed = contact && contact.m_bStateChanged;
		}
		if (operation.m_eEngagementMode == HST_EOperationEngagementMode.HST_OPERATION_ENGAGEMENT_CONTACT)
		{
			HST_OperationTransitionResult engaged = m_Operations.RecordEngagement(state, operation.m_sOperationId, HST_EOperationEngagementMode.HST_OPERATION_ENGAGEMENT_ENGAGED);
			changed = (engaged && engaged.m_bStateChanged) || changed;
		}
		return changed;
	}

	protected bool ClearEngagementIfNeeded(HST_CampaignState state, HST_OperationRecordState operation)
	{
		if (!state || !operation)
			return false;
		bool changed;
		if (operation.m_eEngagementMode == HST_EOperationEngagementMode.HST_OPERATION_ENGAGEMENT_CONTACT)
		{
			HST_OperationTransitionResult engaged = m_Operations.RecordEngagement(state, operation.m_sOperationId, HST_EOperationEngagementMode.HST_OPERATION_ENGAGEMENT_ENGAGED);
			changed = engaged && engaged.m_bStateChanged;
		}
		if (operation.m_eEngagementMode == HST_EOperationEngagementMode.HST_OPERATION_ENGAGEMENT_ENGAGED)
		{
			HST_OperationTransitionResult disengaging = m_Operations.RecordEngagement(state, operation.m_sOperationId, HST_EOperationEngagementMode.HST_OPERATION_ENGAGEMENT_DISENGAGING);
			changed = (disengaging && disengaging.m_bStateChanged) || changed;
		}
		if (operation.m_eEngagementMode == HST_EOperationEngagementMode.HST_OPERATION_ENGAGEMENT_DISENGAGING)
		{
			HST_OperationTransitionResult cleared = m_Operations.RecordEngagement(state, operation.m_sOperationId, HST_EOperationEngagementMode.HST_OPERATION_ENGAGEMENT_CLEAR);
			changed = (cleared && cleared.m_bStateChanged) || changed;
		}
		return changed;
	}

	protected bool UpdateIdleCombatEvidence(HST_OperationRecordState operation, int nowSecond, int livingFriendly, string reason)
	{
		if (!operation)
			return false;
		int lastStepSecond = Math.Max(operation.m_iVirtualCombatLastStepSecond, nowSecond);
		bool changed = operation.m_iVirtualCombatLastStepSecond != lastStepSecond
			|| operation.m_iLastVirtualFriendlyCount != livingFriendly
			|| operation.m_iLastVirtualHostileCount != 0
			|| operation.m_sLastVirtualCombatReason != reason;
		if (!changed)
			return false;

		operation.m_iVirtualCombatLastStepSecond = lastStepSecond;
		operation.m_iLastVirtualFriendlyCount = livingFriendly;
		operation.m_iLastVirtualHostileCount = 0;
		operation.m_sLastVirtualCombatReason = reason;
		operation.m_iLastProgressAtSecond = Math.Max(operation.m_iLastProgressAtSecond, lastStepSecond);
		operation.m_iRevision++;
		return true;
	}

	protected void ApplyFriendlyElimination(HST_ActiveGroupState group, HST_OperationRecordState operation, int nowSecond)
	{
		if (group)
		{
			group.m_iDurableLivingInfantryCount = 0;
			group.m_iLastSeenAliveCount = 0;
			group.m_iSurvivorInfantryCount = 0;
			group.m_iSpawnedAgentCount = 0;
			group.m_sRuntimeStatus = "eliminated";
			group.m_iEliminatedAtSecond = Math.Max(group.m_iEliminatedAtSecond, nowSecond);
			group.m_iLifecycleRevision++;
		}
		if (operation)
			operation.m_sLastVirtualCombatReason = "exact virtual roster eliminated";
	}
}
