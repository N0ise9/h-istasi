class HST_EnemyPlanningDecisionCommand
{
	string m_sFactionKey;
	int m_iExpectedDecisionSequence;
	int m_iExpectedNextPlanningBucketSecond;
	int m_iDecisionBucketSecond;
	int m_iNextRetrySecond;
	int m_iObservedWarLevel;
	int m_iObservedAggression;
	int m_iObservedPoolRevision;
	int m_iObservedOperationalMutationCount;
	int m_iObservedAttackResources;
	int m_iObservedSupportResources;
	int m_iCommitmentCount;
	string m_sCommitmentFingerprint;
	int m_iTargetCandidateCount;
	string m_sTargetCandidateFingerprint;
	int m_iSourceCandidateCount;
	string m_sSourceCandidateFingerprint;
	string m_sSelectedTargetZoneId;
	string m_sSelectedSourceZoneId;
	HST_EEnemyOrderType m_eSelectedOrderType;
	HST_ESupportRequestType m_ePlannedSupportType;
	string m_sPlanningCapabilityHash;
	string m_sSpendMode;
	int m_iAttackCost;
	int m_iSupportCost;
	int m_iTargetPressureBefore;
	int m_iTargetPressureDelta;
	int m_iTargetPressureAfter;
	bool m_bTargetPressureApplied;
	string m_sDecisionId;
	string m_sPlannedOrderId;
	string m_sPlannedOperationId;
	string m_sPlannedManifestId;
	string m_sPlannedManifestHash;
	string m_sPlannedDebitMutationId;
}

class HST_EnemyPlanningDecisionResult
{
	bool m_bAccepted;
	bool m_bAlreadyApplied;
	bool m_bChanged;
	string m_sFailureReason;
	ref HST_EnemyPlanningState m_PlanningState;
	ref HST_EnemyOrderState m_Order;
}

// Schema-68 authority for per-enemy planning cadence and one frozen decision.
// This service never owns or mutates Schema-67 resources.
class HST_EnemyPlanningAuthorityService
{
	static const int CONTRACT_VERSION = 1;
	static const int QUARANTINE_CONTRACT_VERSION = -68;
	static const int PLANNING_INTERVAL_SECONDS = 180;
	static const int MAX_CATCHUP_STEPS_PER_TICK = 1;
	static const int RETRY_INTERVAL_SECONDS = 30;
	static const string EXACT_POLICY_ID = "schema68_enemy_planning_exact1";

	HST_EnemyPlanningState BuildBaselineState(string factionKey, int elapsedSecond)
	{
		HST_EnemyPlanningState planning = new HST_EnemyPlanningState();
		planning.m_iContractVersion = CONTRACT_VERSION;
		planning.m_iRevision = 1;
		planning.m_sFactionKey = factionKey.Trim();
		planning.m_iDecisionSequence = 0;
		planning.m_iLastPlanningBucketSecond = Math.Max(0, elapsedSecond);
		planning.m_iNextPlanningBucketSecond = planning.m_iLastPlanningBucketSecond;
		planning.m_sDisposition = "idle";
		if (planning.m_sFactionKey.IsEmpty())
		{
			Quarantine(planning, "enemy planning baseline faction is missing");
			return planning;
		}
		if (!TryAddBounded(
			planning.m_iLastPlanningBucketSecond,
			PLANNING_INTERVAL_SECONDS,
			planning.m_iNextPlanningBucketSecond))
			Quarantine(planning, "enemy planning baseline cadence would overflow");
		return planning;
	}

	bool ResolveExactState(
		HST_CampaignState campaign,
		string factionKey,
		out HST_EnemyPlanningState planning,
		out string failure)
	{
		planning = null;
		failure = "";
		factionKey = factionKey.Trim();
		if (!campaign || factionKey.IsEmpty() || !campaign.m_aEnemyPlanningStates)
		{
			failure = "enemy planning authority is unavailable";
			return false;
		}

		int count;
		foreach (HST_EnemyPlanningState candidate : campaign.m_aEnemyPlanningStates)
		{
			if (!candidate || candidate.m_sFactionKey != factionKey)
				continue;
			planning = candidate;
			count++;
		}
		if (count != 1 || !planning)
		{
			failure = "enemy planning state is missing or duplicated";
			return false;
		}
		failure = ValidateExactStateShape(planning);
		return failure.IsEmpty();
	}

	bool IsDue(HST_EnemyPlanningState planning, int nowSecond)
	{
		if (!planning || planning.m_iContractVersion != CONTRACT_VERSION
			|| !planning.m_sAuthorityFailure.IsEmpty() || IsPrepared(planning))
			return false;
		return nowSecond >= 0
			&& nowSecond >= planning.m_iNextRetrySecond
			&& nowSecond >= planning.m_iNextPlanningBucketSecond;
	}

	bool IsPrepared(HST_EnemyPlanningState planning)
	{
		return planning
			&& planning.m_iContractVersion == CONTRACT_VERSION
			&& planning.m_sAuthorityFailure.IsEmpty()
			&& planning.m_sDisposition == "prepared"
			&& !planning.m_sDecisionId.IsEmpty()
			&& !planning.m_sInputFingerprint.IsEmpty()
			&& !planning.m_sDecisionFingerprint.IsEmpty();
	}

	static string BuildDecisionId(
		string factionKey,
		int decisionSequence,
		int decisionBucketSecond)
	{
		factionKey = factionKey.Trim();
		if (factionKey.IsEmpty() || decisionSequence <= 0
			|| decisionBucketSecond < 0)
			return "";
		string canonical = string.Format(
			"%1|decision|%2|%3|%4",
			EXACT_POLICY_ID,
			factionKey,
			decisionSequence,
			decisionBucketSecond);
		return string.Format(
			"enemy_plan_%1_%2",
			canonical.Hash(),
			(canonical + "|secondary").Hash());
	}

	static string BuildOrderId(string decisionId)
	{
		decisionId = decisionId.Trim();
		if (decisionId.IsEmpty())
			return "";
		string canonical = EXACT_POLICY_ID + "|order|" + decisionId;
		return string.Format(
			"order_plan_%1_%2",
			canonical.Hash(),
			(canonical + "|secondary").Hash());
	}

	static string BuildOperationId(string orderId)
	{
		orderId = orderId.Trim();
		if (orderId.IsEmpty())
			return "";
		return HST_StableIdService.BuildOperationId("enemy_order", orderId);
	}

	static string BuildDebitMutationId(string orderId)
	{
		orderId = orderId.Trim();
		if (orderId.IsEmpty())
			return "";
		return "enemy_resource_debit_" + orderId;
	}

	static string BuildInputFingerprint(HST_EnemyPlanningState planning)
	{
		if (!planning)
			return "";
		string canonical = string.Format(
			"%1|%2|%3|%4|%5|%6|%7|%8|%9",
			EXACT_POLICY_ID,
			planning.m_iContractVersion,
			planning.m_sFactionKey,
			planning.m_iDecisionSequence,
			planning.m_iDecisionBucketSecond,
			planning.m_iObservedWarLevel,
			planning.m_iObservedAggression,
			planning.m_iObservedPoolRevision,
			planning.m_iObservedOperationalMutationCount);
		canonical = canonical + string.Format(
			"|%1|%2|%3|%4|%5|%6|%7|%8",
			planning.m_iObservedAttackResources,
			planning.m_iObservedSupportResources,
			planning.m_iCommitmentCount,
			planning.m_sCommitmentFingerprint,
			planning.m_iTargetCandidateCount,
			planning.m_sTargetCandidateFingerprint,
			planning.m_iSourceCandidateCount,
			planning.m_sSourceCandidateFingerprint);
		return string.Format(
			"epi1_%1_%2",
			canonical.Hash(),
			(canonical + "|secondary").Hash());
	}

	static string BuildDecisionFingerprint(HST_EnemyPlanningState planning)
	{
		if (!planning || planning.m_sInputFingerprint.IsEmpty())
			return "";
		string canonical = string.Format(
			"%1|%2|%3|%4|%5|%6|%7|%8|%9",
			EXACT_POLICY_ID,
			planning.m_sInputFingerprint,
			planning.m_sSelectedTargetZoneId,
			planning.m_sSelectedSourceZoneId,
			planning.m_eSelectedOrderType,
			planning.m_ePlannedSupportType,
			planning.m_sPlanningCapabilityHash,
			planning.m_sSpendMode,
			planning.m_iAttackCost);
		canonical = canonical + string.Format(
			"|%1|%2|%3|%4|%5|%6|%7|%8|%9",
			planning.m_iSupportCost,
			planning.m_iTargetPressureBefore,
			planning.m_iTargetPressureDelta,
			planning.m_iTargetPressureAfter,
			planning.m_sDecisionId,
			planning.m_sPlannedOrderId,
			planning.m_sPlannedOperationId,
			planning.m_sPlannedManifestId,
			planning.m_sPlannedManifestHash);
		canonical = canonical + "|" + planning.m_sPlannedDebitMutationId;
		return string.Format(
			"epd1_%1_%2",
			canonical.Hash(),
			(canonical + "|secondary").Hash());
	}

	static string BuildCapabilityHash(
		HST_EEnemyOrderType orderType,
		HST_ESupportRequestType supportType,
		int operationContractVersion,
		string manifestHash,
		string routeHash)
	{
		manifestHash = manifestHash.Trim();
		routeHash = routeHash.Trim();
		string canonical = string.Format(
			"%1|capability|%2|%3|%4|%5|%6",
			EXACT_POLICY_ID,
			orderType,
			supportType,
			operationContractVersion,
			manifestHash,
			routeHash);
		return string.Format(
			"epc1_%1_%2",
			canonical.Hash(),
			(canonical + "|secondary").Hash());
	}

	static string BuildCommitmentFingerprint(
		HST_CampaignState campaign,
		string factionKey,
		out int commitmentCount)
	{
		commitmentCount = 0;
		factionKey = factionKey.Trim();
		if (!campaign || factionKey.IsEmpty())
			return "";
		array<string> rows = {};
		foreach (HST_EnemyOrderState order : campaign.m_aEnemyOrders)
		{
			if (!order || order.m_sFactionKey != factionKey)
				continue;
			if (order.m_eStatus != HST_EEnemyOrderStatus.HST_ENEMY_ORDER_QUEUED
				&& order.m_eStatus != HST_EEnemyOrderStatus.HST_ENEMY_ORDER_ACTIVE)
				continue;
			string row = string.Format(
				"order|%1|%2|%3|%4|%5|%6|%7|%8",
				order.m_sOrderId,
				order.m_eType,
				order.m_eStatus,
				order.m_sSourceZoneId,
				order.m_sTargetZoneId,
				order.m_sOperationId,
				order.m_sManifestId,
				order.m_iAttackCost);
			row = row + string.Format("|%1", order.m_iSupportCost);
			rows.Insert(row);
		}
		foreach (HST_SupportRequestState request : campaign.m_aSupportRequests)
		{
			if (!request || request.m_sFactionKey != factionKey)
				continue;
			if (request.m_eStatus != HST_ESupportRequestStatus.HST_SUPPORT_QUEUED
				&& request.m_eStatus != HST_ESupportRequestStatus.HST_SUPPORT_ACTIVE)
				continue;
			string row = string.Format(
				"support|%1|%2|%3|%4|%5|%6|%7|%8",
				request.m_sRequestId,
				request.m_eType,
				request.m_eStatus,
				request.m_sSourceZoneId,
				request.m_sTargetZoneId,
				request.m_sOperationId,
				request.m_sManifestId,
				request.m_sGroupId);
			rows.Insert(row);
		}
		foreach (HST_OperationRecordState operation : campaign.m_aOperations)
		{
			if (!operation || operation.m_sOwnerFactionKey != factionKey
				|| operation.m_eSettlementState
					== HST_EOperationSettlementState.HST_OPERATION_SETTLEMENT_SETTLED)
				continue;
			if (operation.m_eTerminalResult
					!= HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_UNKNOWN
				&& operation.m_eTerminalResult
					!= HST_EOperationTerminalResult.HST_OPERATION_TERMINAL_NONE)
				continue;
			string row = string.Format(
				"operation|%1|%2|%3|%4|%5|%6|%7|%8",
				operation.m_sOperationId,
				operation.m_eType,
				operation.m_iContractVersion,
				operation.m_sEnemyOrderId,
				operation.m_sSupportRequestId,
				operation.m_sOriginZoneId,
				operation.m_sAssignmentZoneId,
				operation.m_sTacticalTargetZoneId);
			row = row + string.Format(
				"|%1|%2",
				operation.m_eDutyState,
				operation.m_eSettlementState);
			rows.Insert(row);
		}
		rows.Sort();
		commitmentCount = rows.Count();
		string canonical = string.Format(
			"%1|commitments|%2|%3",
			EXACT_POLICY_ID,
			factionKey,
			commitmentCount);
		foreach (string commitment : rows)
			canonical = canonical + "|" + commitment;
		return string.Format(
			"epm1_%1_%2",
			canonical.Hash(),
			(canonical + "|secondary").Hash());
	}

	HST_EnemyPlanningDecisionResult BeginDecision(
		HST_EnemyPlanningState planning,
		HST_EnemyPlanningDecisionCommand command)
	{
		HST_EnemyPlanningDecisionResult result
			= NewResult(planning);
		if (!planning || !command)
			return Reject(result, "enemy planning decision authority is unavailable");

		string planningFailure = ValidateExactStateShape(planning);
		if (!planningFailure.IsEmpty())
			return Reject(result, planningFailure);
		if (command.m_iExpectedDecisionSequence
			== planning.m_iDecisionSequence + 1
			&& planning.m_iRevision > int.MAX - 4)
			return Reject(
				result,
				"enemy planning decision authority is exhausted");
		string commandFailure = ValidateCommand(planning, command);
		if (!commandFailure.IsEmpty())
		{
			bool quarantined = Quarantine(planning, commandFailure);
			result.m_bChanged = quarantined;
			return Reject(result, commandFailure);
		}

		if (planning.m_iDecisionSequence == command.m_iExpectedDecisionSequence)
		{
			if (CommandMatchesPlanning(planning, command))
			{
				result.m_bAccepted = true;
				result.m_bAlreadyApplied = true;
				return result;
			}
			string conflict = "enemy planning decision replay changed its frozen fingerprint";
			result.m_bChanged = Quarantine(planning, conflict);
			return Reject(result, conflict);
		}

		HST_EnemyPlanningState candidate = BuildPreparedCandidate(planning, command);
		if (!candidate)
			return Reject(result, "enemy planning prepared candidate could not be built");
		candidate.m_sInputFingerprint = BuildInputFingerprint(candidate);
		candidate.m_sDecisionFingerprint = BuildDecisionFingerprint(candidate);
		if (candidate.m_sInputFingerprint.IsEmpty()
			|| candidate.m_sDecisionFingerprint.IsEmpty())
			return Reject(result, "enemy planning decision fingerprint could not be built");

		CopyPreparedCandidate(planning, candidate);
		result.m_bAccepted = true;
		result.m_bChanged = true;
		return result;
	}

	HST_EnemyPlanningDecisionResult CompleteDecision(
		HST_EnemyPlanningState planning,
		string disposition,
		HST_EnemyOrderState order,
		string failure)
	{
		HST_EnemyPlanningDecisionResult result = NewResult(planning);
		result.m_Order = order;
		if (!planning)
			return Reject(result, "enemy planning completion authority is unavailable");
		disposition = disposition.Trim();
		failure = failure.Trim();
		if (disposition != "committed" && disposition != "skipped"
			&& disposition != "rejected")
			return Reject(result, "enemy planning completion disposition is invalid");

		if (planning.m_sDisposition == disposition)
		{
			string replayFailure = ValidateCompletion(planning, disposition, order, failure);
			if (replayFailure.IsEmpty() && planning.m_sFailureReason == failure)
			{
				result.m_bAccepted = true;
				result.m_bAlreadyApplied = true;
				return result;
			}
			string conflict = "enemy planning completion replay changed its frozen result";
			result.m_bChanged = Quarantine(planning, conflict);
			return Reject(result, conflict);
		}
		if (!IsPrepared(planning))
			return Reject(result, "enemy planning decision is not prepared");
		if (planning.m_iRevision > int.MAX - 2)
			return Reject(
				result,
				"enemy planning completion authority is exhausted");

		string completionFailure = ValidateCompletion(
			planning,
			disposition,
			order,
			failure);
		if (!completionFailure.IsEmpty())
		{
			result.m_bChanged = Quarantine(planning, completionFailure);
			return Reject(result, completionFailure);
		}
		planning.m_sDisposition = disposition;
		planning.m_sFailureReason = failure;
		planning.m_iNextRetrySecond = 0;
		planning.m_iRevision++;
		result.m_bAccepted = true;
		result.m_bChanged = true;
		return result;
	}

	HST_EnemyPlanningDecisionResult RecordRetry(
		HST_EnemyPlanningState planning,
		string failure,
		int nowSecond)
	{
		HST_EnemyPlanningDecisionResult result = NewResult(planning);
		failure = failure.Trim();
		if (!IsPrepared(planning) || failure.IsEmpty() || nowSecond < 0)
			return Reject(result, "enemy planning retry request is invalid");
		if (planning.m_iNextRetrySecond > nowSecond)
		{
			result.m_bAccepted = true;
			result.m_bAlreadyApplied = true;
			return result;
		}
		if (planning.m_iRevision > int.MAX - 3)
			return Reject(
				result,
				"enemy planning retry authority is exhausted");
		int nextRetrySecond;
		if (!TryAddBounded(nowSecond, RETRY_INTERVAL_SECONDS, nextRetrySecond))
			return Reject(result, "enemy planning retry cadence would overflow");
		planning.m_sFailureReason = failure;
		planning.m_iNextRetrySecond = nextRetrySecond;
		planning.m_iRevision++;
		result.m_bAccepted = true;
		result.m_bChanged = true;
		return result;
	}

	// Persists a bounded gate when a due decision cannot be frozen yet. This is
	// intentionally distinct from RecordRetry: no decision sequence or frozen
	// decision fields are changed, so the prior terminal decision remains exact.
	HST_EnemyPlanningDecisionResult RecordPreparationRetry(
		HST_EnemyPlanningState planning,
		string failure,
		int nowSecond)
	{
		HST_EnemyPlanningDecisionResult result = NewResult(planning);
		failure = failure.Trim();
		if (!planning || failure.IsEmpty() || nowSecond < 0)
			return Reject(result, "enemy planning preparation retry request is invalid");
		string planningFailure = ValidateExactStateShape(planning);
		if (!planningFailure.IsEmpty())
			return Reject(result, planningFailure);
		if (IsPrepared(planning)
			|| nowSecond < planning.m_iNextPlanningBucketSecond)
			return Reject(result, "enemy planning preparation retry is not due");
		if (planning.m_iNextRetrySecond > nowSecond)
		{
			result.m_bAccepted = true;
			result.m_bAlreadyApplied = true;
			return result;
		}
		if (planning.m_iRevision > int.MAX - 5)
			return Reject(
				result,
				"enemy planning preparation retry authority is exhausted");
		int nextRetrySecond;
		if (!TryAddBounded(nowSecond, RETRY_INTERVAL_SECONDS, nextRetrySecond))
			return Reject(
				result,
				"enemy planning preparation retry cadence would overflow");

		planning.m_sFailureReason = failure;
		planning.m_iNextRetrySecond = nextRetrySecond;
		planning.m_iRevision++;
		result.m_bAccepted = true;
		result.m_bChanged = true;
		return result;
	}

	bool CanMarkTargetPressureApplied(
		HST_EnemyPlanningState planning,
		out string failure)
	{
		failure = "";
		string planningFailure = ValidateExactStateShape(planning);
		if (!planningFailure.IsEmpty())
		{
			failure = planningFailure;
			return false;
		}
		if (!IsPrepared(planning))
		{
			failure = "enemy planning target pressure requires an exact prepared decision";
			return false;
		}
		if (planning.m_bTargetPressureApplied)
			return true;
		if (planning.m_iRevision > int.MAX - 3)
		{
			failure = "enemy planning target-pressure authority is exhausted";
			return false;
		}
		return true;
	}

	HST_EnemyPlanningDecisionResult MarkTargetPressureApplied(
		HST_EnemyPlanningState planning)
	{
		HST_EnemyPlanningDecisionResult result = NewResult(planning);
		string failure;
		if (!CanMarkTargetPressureApplied(planning, failure))
			return Reject(result, failure);
		if (planning.m_bTargetPressureApplied)
		{
			result.m_bAccepted = true;
			result.m_bAlreadyApplied = true;
			return result;
		}
		planning.m_bTargetPressureApplied = true;
		planning.m_iRevision++;
		result.m_bAccepted = true;
		result.m_bChanged = true;
		return result;
	}

	bool Quarantine(HST_EnemyPlanningState planning, string failure)
	{
		if (!planning)
			return false;
		failure = failure.Trim();
		if (failure.IsEmpty())
			failure = "enemy planning authority quarantined";
		if (planning.m_iContractVersion == QUARANTINE_CONTRACT_VERSION
			&& planning.m_sAuthorityFailure == failure
			&& planning.m_sDisposition == "quarantined")
			return false;
		planning.m_iContractVersion = QUARANTINE_CONTRACT_VERSION;
		planning.m_sAuthorityFailure = failure;
		planning.m_sFailureReason = failure;
		planning.m_sDisposition = "quarantined";
		if (planning.m_iRevision < int.MAX - 1)
			planning.m_iRevision++;
		return true;
	}

	protected HST_EnemyPlanningState BuildPreparedCandidate(
		HST_EnemyPlanningState planning,
		HST_EnemyPlanningDecisionCommand command)
	{
		if (!planning || !command)
			return null;
		HST_EnemyPlanningState candidate = new HST_EnemyPlanningState();
		candidate.m_iContractVersion = CONTRACT_VERSION;
		candidate.m_iRevision = planning.m_iRevision + 1;
		candidate.m_sFactionKey = command.m_sFactionKey;
		candidate.m_iDecisionSequence = command.m_iExpectedDecisionSequence;
		candidate.m_iLastPlanningBucketSecond
			= planning.m_iNextPlanningBucketSecond;
		if (!TryAddBounded(
			candidate.m_iLastPlanningBucketSecond,
			PLANNING_INTERVAL_SECONDS,
			candidate.m_iNextPlanningBucketSecond))
			return null;
		candidate.m_iDecisionBucketSecond = command.m_iDecisionBucketSecond;
		candidate.m_iNextRetrySecond = command.m_iNextRetrySecond;
		candidate.m_iObservedWarLevel = command.m_iObservedWarLevel;
		candidate.m_iObservedAggression = command.m_iObservedAggression;
		candidate.m_iObservedPoolRevision = command.m_iObservedPoolRevision;
		candidate.m_iObservedOperationalMutationCount
			= command.m_iObservedOperationalMutationCount;
		candidate.m_iObservedAttackResources
			= command.m_iObservedAttackResources;
		candidate.m_iObservedSupportResources
			= command.m_iObservedSupportResources;
		candidate.m_iCommitmentCount = command.m_iCommitmentCount;
		candidate.m_sCommitmentFingerprint
			= command.m_sCommitmentFingerprint;
		candidate.m_iTargetCandidateCount = command.m_iTargetCandidateCount;
		candidate.m_sTargetCandidateFingerprint
			= command.m_sTargetCandidateFingerprint;
		candidate.m_iSourceCandidateCount = command.m_iSourceCandidateCount;
		candidate.m_sSourceCandidateFingerprint
			= command.m_sSourceCandidateFingerprint;
		candidate.m_sSelectedTargetZoneId = command.m_sSelectedTargetZoneId;
		candidate.m_sSelectedSourceZoneId = command.m_sSelectedSourceZoneId;
		candidate.m_eSelectedOrderType = command.m_eSelectedOrderType;
		candidate.m_ePlannedSupportType = command.m_ePlannedSupportType;
		candidate.m_sPlanningCapabilityHash
			= command.m_sPlanningCapabilityHash;
		candidate.m_sSpendMode = command.m_sSpendMode;
		candidate.m_iAttackCost = command.m_iAttackCost;
		candidate.m_iSupportCost = command.m_iSupportCost;
		candidate.m_iTargetPressureBefore = command.m_iTargetPressureBefore;
		candidate.m_iTargetPressureDelta = command.m_iTargetPressureDelta;
		candidate.m_iTargetPressureAfter = command.m_iTargetPressureAfter;
		candidate.m_bTargetPressureApplied = command.m_bTargetPressureApplied;
		candidate.m_sDecisionId = command.m_sDecisionId;
		candidate.m_sPlannedOrderId = command.m_sPlannedOrderId;
		candidate.m_sPlannedOperationId = command.m_sPlannedOperationId;
		candidate.m_sPlannedManifestId = command.m_sPlannedManifestId;
		candidate.m_sPlannedManifestHash = command.m_sPlannedManifestHash;
		candidate.m_sPlannedDebitMutationId
			= command.m_sPlannedDebitMutationId;
		candidate.m_sDisposition = "prepared";
		return candidate;
	}

	protected void CopyPreparedCandidate(
		HST_EnemyPlanningState target,
		HST_EnemyPlanningState source)
	{
		if (!target || !source)
			return;
		target.m_iContractVersion = source.m_iContractVersion;
		target.m_iRevision = source.m_iRevision;
		target.m_sFactionKey = source.m_sFactionKey;
		target.m_iDecisionSequence = source.m_iDecisionSequence;
		target.m_iLastPlanningBucketSecond = source.m_iLastPlanningBucketSecond;
		target.m_iNextPlanningBucketSecond = source.m_iNextPlanningBucketSecond;
		target.m_iDecisionBucketSecond = source.m_iDecisionBucketSecond;
		target.m_iNextRetrySecond = source.m_iNextRetrySecond;
		target.m_iObservedWarLevel = source.m_iObservedWarLevel;
		target.m_iObservedAggression = source.m_iObservedAggression;
		target.m_iObservedPoolRevision = source.m_iObservedPoolRevision;
		target.m_iObservedOperationalMutationCount
			= source.m_iObservedOperationalMutationCount;
		target.m_iObservedAttackResources = source.m_iObservedAttackResources;
		target.m_iObservedSupportResources = source.m_iObservedSupportResources;
		target.m_iCommitmentCount = source.m_iCommitmentCount;
		target.m_sCommitmentFingerprint = source.m_sCommitmentFingerprint;
		target.m_iTargetCandidateCount = source.m_iTargetCandidateCount;
		target.m_sTargetCandidateFingerprint
			= source.m_sTargetCandidateFingerprint;
		target.m_iSourceCandidateCount = source.m_iSourceCandidateCount;
		target.m_sSourceCandidateFingerprint
			= source.m_sSourceCandidateFingerprint;
		target.m_sSelectedTargetZoneId = source.m_sSelectedTargetZoneId;
		target.m_sSelectedSourceZoneId = source.m_sSelectedSourceZoneId;
		target.m_eSelectedOrderType = source.m_eSelectedOrderType;
		target.m_ePlannedSupportType = source.m_ePlannedSupportType;
		target.m_sPlanningCapabilityHash = source.m_sPlanningCapabilityHash;
		target.m_sSpendMode = source.m_sSpendMode;
		target.m_iAttackCost = source.m_iAttackCost;
		target.m_iSupportCost = source.m_iSupportCost;
		target.m_iTargetPressureBefore = source.m_iTargetPressureBefore;
		target.m_iTargetPressureDelta = source.m_iTargetPressureDelta;
		target.m_iTargetPressureAfter = source.m_iTargetPressureAfter;
		target.m_bTargetPressureApplied = source.m_bTargetPressureApplied;
		target.m_sDecisionId = source.m_sDecisionId;
		target.m_sPlannedOrderId = source.m_sPlannedOrderId;
		target.m_sPlannedOperationId = source.m_sPlannedOperationId;
		target.m_sPlannedManifestId = source.m_sPlannedManifestId;
		target.m_sPlannedManifestHash = source.m_sPlannedManifestHash;
		target.m_sPlannedDebitMutationId = source.m_sPlannedDebitMutationId;
		target.m_sInputFingerprint = source.m_sInputFingerprint;
		target.m_sDecisionFingerprint = source.m_sDecisionFingerprint;
		target.m_sDisposition = source.m_sDisposition;
		target.m_sFailureReason = "";
		target.m_sAuthorityFailure = "";
	}

	protected string ValidateCommand(
		HST_EnemyPlanningState planning,
		HST_EnemyPlanningDecisionCommand command)
	{
		if (!planning || !command)
			return "enemy planning decision command is unavailable";
		if (command.m_sFactionKey != command.m_sFactionKey.Trim()
			|| command.m_sFactionKey.IsEmpty()
			|| command.m_sFactionKey != planning.m_sFactionKey)
			return "enemy planning decision faction conflicts with authority";
		if (command.m_iExpectedDecisionSequence
			!= planning.m_iDecisionSequence + 1
			&& command.m_iExpectedDecisionSequence
				!= planning.m_iDecisionSequence)
			return "enemy planning decision sequence conflicts with authority";
		if (command.m_iExpectedDecisionSequence == planning.m_iDecisionSequence)
			return "";
		if (planning.m_sDisposition == "prepared")
			return "enemy planning authority already has a prepared decision";
		if (command.m_iExpectedNextPlanningBucketSecond
			!= planning.m_iNextPlanningBucketSecond
			|| command.m_iDecisionBucketSecond
				!= planning.m_iNextPlanningBucketSecond)
			return "enemy planning decision cadence conflicts with authority";
		if (planning.m_iRevision > int.MAX - 4
			|| planning.m_iNextPlanningBucketSecond
				> int.MAX - PLANNING_INTERVAL_SECONDS)
			return "enemy planning decision authority is exhausted";
		if (command.m_iNextRetrySecond != 0
			|| command.m_iObservedWarLevel < 0
			|| command.m_iObservedAggression < 0
			|| command.m_iObservedPoolRevision <= 0
			|| command.m_iObservedOperationalMutationCount < 0
			|| command.m_iObservedAttackResources < 0
			|| command.m_iObservedSupportResources < 0
			|| command.m_iCommitmentCount < 0
			|| command.m_iTargetCandidateCount < 0
			|| command.m_iSourceCandidateCount < 0)
			return "enemy planning decision observed input is invalid";
		if (!IsRequiredTrimmed(command.m_sCommitmentFingerprint)
			|| !IsRequiredTrimmed(command.m_sTargetCandidateFingerprint)
			|| !IsRequiredTrimmed(command.m_sSourceCandidateFingerprint)
			|| !IsRequiredTrimmed(command.m_sPlanningCapabilityHash)
			|| !IsRequiredTrimmed(command.m_sSpendMode))
			return "enemy planning decision input or selection identity is invalid";
		if ((command.m_iTargetCandidateCount == 0
				&& !command.m_sSelectedTargetZoneId.IsEmpty())
			|| (command.m_iTargetCandidateCount > 0
				&& !IsRequiredTrimmed(command.m_sSelectedTargetZoneId)))
			return "enemy planning decision target selection conflicts with its candidate set";
		if (!IsOptionalTrimmed(command.m_sSelectedSourceZoneId)
			|| !IsRequiredTrimmed(command.m_sDecisionId)
			|| !IsRequiredTrimmed(command.m_sPlannedOrderId)
			|| !IsRequiredTrimmed(command.m_sPlannedOperationId)
			|| !IsOptionalTrimmed(command.m_sPlannedManifestId)
			|| !IsOptionalTrimmed(command.m_sPlannedManifestHash)
			|| !IsRequiredTrimmed(command.m_sPlannedDebitMutationId))
			return "enemy planning decision durable identity is invalid";
		if (command.m_sPlannedManifestId.IsEmpty()
			!= command.m_sPlannedManifestHash.IsEmpty())
			return "enemy planning decision manifest identity is incomplete";
		if (command.m_iAttackCost < 0 || command.m_iSupportCost < 0
			|| command.m_iTargetPressureBefore < 0
			|| command.m_iTargetPressureDelta < 0
			|| command.m_iTargetPressureAfter < 0
			|| command.m_iTargetPressureBefore
				> int.MAX - command.m_iTargetPressureDelta
			|| command.m_iTargetPressureAfter
				!= command.m_iTargetPressureBefore
					+ command.m_iTargetPressureDelta)
			return "enemy planning decision cost or pressure projection is invalid";
		string expectedDecisionId = BuildDecisionId(
			command.m_sFactionKey,
			command.m_iExpectedDecisionSequence,
			command.m_iDecisionBucketSecond);
		string expectedOrderId = BuildOrderId(expectedDecisionId);
		if (command.m_sDecisionId != expectedDecisionId
			|| command.m_sPlannedOrderId != expectedOrderId
			|| command.m_sPlannedOperationId != BuildOperationId(expectedOrderId)
			|| command.m_sPlannedDebitMutationId
				!= BuildDebitMutationId(expectedOrderId))
			return "enemy planning decision derived identity conflicts with authority";
		return "";
	}

	protected string ValidateExactStateShape(HST_EnemyPlanningState planning)
	{
		if (!planning)
			return "enemy planning state is missing";
		if (planning.m_iContractVersion != CONTRACT_VERSION
			|| planning.m_iRevision <= 0 || planning.m_iRevision >= int.MAX
			|| planning.m_sFactionKey.IsEmpty()
			|| planning.m_sFactionKey != planning.m_sFactionKey.Trim()
			|| !planning.m_sAuthorityFailure.IsEmpty())
			return "enemy planning state contract or identity is invalid";
		if (planning.m_iDecisionSequence < 0
			|| planning.m_iDecisionSequence >= int.MAX - 1
			|| planning.m_iLastPlanningBucketSecond < 0
			|| planning.m_iLastPlanningBucketSecond
				> int.MAX - PLANNING_INTERVAL_SECONDS
			|| planning.m_iNextPlanningBucketSecond
				!= planning.m_iLastPlanningBucketSecond
					+ PLANNING_INTERVAL_SECONDS)
			return "enemy planning cadence is invalid";
		if (planning.m_iDecisionSequence == 0)
		{
			if (planning.m_sDisposition != "idle"
				|| !planning.m_sDecisionId.IsEmpty()
				|| !planning.m_sInputFingerprint.IsEmpty()
				|| !planning.m_sDecisionFingerprint.IsEmpty())
				return "enemy planning baseline carries decision residue";
			if (planning.m_iNextRetrySecond == 0)
			{
				if (planning.m_iRevision != 1
					|| !planning.m_sFailureReason.IsEmpty())
					return "enemy planning baseline retry state is invalid";
			}
			else if (planning.m_iNextRetrySecond < 0
				|| planning.m_iRevision <= 1
				|| planning.m_sFailureReason.IsEmpty())
				return "enemy planning baseline retry state is invalid";
			return "";
		}
		if (planning.m_iDecisionBucketSecond
			!= planning.m_iLastPlanningBucketSecond
			|| planning.m_iNextRetrySecond < 0
			|| planning.m_iObservedWarLevel < 0
			|| planning.m_iObservedAggression < 0
			|| planning.m_iObservedPoolRevision <= 0
			|| planning.m_iObservedOperationalMutationCount < 0
			|| planning.m_iObservedAttackResources < 0
			|| planning.m_iObservedSupportResources < 0
			|| planning.m_iCommitmentCount < 0
			|| planning.m_iTargetCandidateCount < 0
			|| planning.m_iSourceCandidateCount < 0)
			return "enemy planning frozen input is invalid";
		if (!IsRequiredTrimmed(planning.m_sCommitmentFingerprint)
			|| !IsRequiredTrimmed(planning.m_sTargetCandidateFingerprint)
			|| !IsRequiredTrimmed(planning.m_sSourceCandidateFingerprint)
			|| !IsRequiredTrimmed(planning.m_sPlanningCapabilityHash)
			|| !IsRequiredTrimmed(planning.m_sSpendMode)
			|| !IsRequiredTrimmed(planning.m_sDecisionId)
			|| !IsRequiredTrimmed(planning.m_sPlannedOrderId)
			|| !IsRequiredTrimmed(planning.m_sPlannedOperationId)
			|| !IsRequiredTrimmed(planning.m_sPlannedDebitMutationId)
			|| !IsRequiredTrimmed(planning.m_sInputFingerprint)
			|| !IsRequiredTrimmed(planning.m_sDecisionFingerprint))
			return "enemy planning frozen identity is invalid";
		if ((planning.m_iTargetCandidateCount == 0
				&& !planning.m_sSelectedTargetZoneId.IsEmpty())
			|| (planning.m_iTargetCandidateCount > 0
				&& !IsRequiredTrimmed(planning.m_sSelectedTargetZoneId)))
			return "enemy planning frozen target conflicts with its candidate set";
		if (planning.m_sPlannedManifestId.IsEmpty()
			!= planning.m_sPlannedManifestHash.IsEmpty())
			return "enemy planning frozen manifest identity is incomplete";
		if (planning.m_iAttackCost < 0 || planning.m_iSupportCost < 0
			|| planning.m_iTargetPressureBefore < 0
			|| planning.m_iTargetPressureDelta < 0
			|| planning.m_iTargetPressureAfter < 0
			|| planning.m_iTargetPressureBefore
				> int.MAX - planning.m_iTargetPressureDelta
			|| planning.m_iTargetPressureAfter
				!= planning.m_iTargetPressureBefore
					+ planning.m_iTargetPressureDelta)
			return "enemy planning frozen cost or pressure projection is invalid";
		if (planning.m_sInputFingerprint != BuildInputFingerprint(planning)
			|| planning.m_sDecisionFingerprint
				!= BuildDecisionFingerprint(planning))
			return "enemy planning frozen fingerprint is invalid";
		if (planning.m_sDisposition != "prepared"
			&& planning.m_sDisposition != "committed"
			&& planning.m_sDisposition != "skipped"
			&& planning.m_sDisposition != "rejected")
			return "enemy planning disposition is invalid";
		if (planning.m_sDisposition == "prepared")
		{
			if (planning.m_iNextRetrySecond > 0
				&& planning.m_sFailureReason.IsEmpty())
				return "enemy planning prepared retry lacks failure evidence";
		}
		else if (planning.m_iNextRetrySecond > 0
			&& planning.m_sFailureReason.IsEmpty())
			return "enemy planning preparation retry lacks failure evidence";
		else if (planning.m_sDisposition == "committed"
			&& planning.m_iNextRetrySecond == 0
			&& !planning.m_sFailureReason.IsEmpty())
			return "enemy planning committed decision carries failure residue";
		return "";
	}

	protected bool CommandMatchesPlanning(
		HST_EnemyPlanningState planning,
		HST_EnemyPlanningDecisionCommand command)
	{
		if (!planning || !command)
			return false;
		if (planning.m_sFactionKey != command.m_sFactionKey
			|| planning.m_iDecisionSequence
				!= command.m_iExpectedDecisionSequence
			|| planning.m_iDecisionBucketSecond
				!= command.m_iDecisionBucketSecond
			|| planning.m_iLastPlanningBucketSecond
				!= command.m_iExpectedNextPlanningBucketSecond)
			return false;
		HST_EnemyPlanningState candidate = BuildPreparedCandidateForReplay(
			planning,
			command);
		if (!candidate)
			return false;
		candidate.m_sInputFingerprint = BuildInputFingerprint(candidate);
		candidate.m_sDecisionFingerprint = BuildDecisionFingerprint(candidate);
		return planning.m_sInputFingerprint == candidate.m_sInputFingerprint
			&& planning.m_sDecisionFingerprint
				== candidate.m_sDecisionFingerprint;
	}

	protected HST_EnemyPlanningState BuildPreparedCandidateForReplay(
		HST_EnemyPlanningState planning,
		HST_EnemyPlanningDecisionCommand command)
	{
		if (!planning || !command)
			return null;
		HST_EnemyPlanningState candidate = new HST_EnemyPlanningState();
		candidate.m_iContractVersion = CONTRACT_VERSION;
		candidate.m_sFactionKey = command.m_sFactionKey;
		candidate.m_iDecisionSequence = command.m_iExpectedDecisionSequence;
		candidate.m_iDecisionBucketSecond = command.m_iDecisionBucketSecond;
		candidate.m_iObservedWarLevel = command.m_iObservedWarLevel;
		candidate.m_iObservedAggression = command.m_iObservedAggression;
		candidate.m_iObservedPoolRevision = command.m_iObservedPoolRevision;
		candidate.m_iObservedOperationalMutationCount
			= command.m_iObservedOperationalMutationCount;
		candidate.m_iObservedAttackResources
			= command.m_iObservedAttackResources;
		candidate.m_iObservedSupportResources
			= command.m_iObservedSupportResources;
		candidate.m_iCommitmentCount = command.m_iCommitmentCount;
		candidate.m_sCommitmentFingerprint = command.m_sCommitmentFingerprint;
		candidate.m_iTargetCandidateCount = command.m_iTargetCandidateCount;
		candidate.m_sTargetCandidateFingerprint
			= command.m_sTargetCandidateFingerprint;
		candidate.m_iSourceCandidateCount = command.m_iSourceCandidateCount;
		candidate.m_sSourceCandidateFingerprint
			= command.m_sSourceCandidateFingerprint;
		candidate.m_sSelectedTargetZoneId = command.m_sSelectedTargetZoneId;
		candidate.m_sSelectedSourceZoneId = command.m_sSelectedSourceZoneId;
		candidate.m_eSelectedOrderType = command.m_eSelectedOrderType;
		candidate.m_ePlannedSupportType = command.m_ePlannedSupportType;
		candidate.m_sPlanningCapabilityHash
			= command.m_sPlanningCapabilityHash;
		candidate.m_sSpendMode = command.m_sSpendMode;
		candidate.m_iAttackCost = command.m_iAttackCost;
		candidate.m_iSupportCost = command.m_iSupportCost;
		candidate.m_iTargetPressureBefore = command.m_iTargetPressureBefore;
		candidate.m_iTargetPressureDelta = command.m_iTargetPressureDelta;
		candidate.m_iTargetPressureAfter = command.m_iTargetPressureAfter;
		candidate.m_sDecisionId = command.m_sDecisionId;
		candidate.m_sPlannedOrderId = command.m_sPlannedOrderId;
		candidate.m_sPlannedOperationId = command.m_sPlannedOperationId;
		candidate.m_sPlannedManifestId = command.m_sPlannedManifestId;
		candidate.m_sPlannedManifestHash = command.m_sPlannedManifestHash;
		candidate.m_sPlannedDebitMutationId
			= command.m_sPlannedDebitMutationId;
		return candidate;
	}

	protected string ValidateCompletion(
		HST_EnemyPlanningState planning,
		string disposition,
		HST_EnemyOrderState order,
		string failure)
	{
		if (disposition == "committed")
		{
			if (!order)
				return "committed enemy planning decision has no order";
			if (!failure.IsEmpty())
				return "committed enemy planning decision carries failure residue";
			if (order.m_iPlanningContractVersion != CONTRACT_VERSION
				|| order.m_iPlanningDecisionSequence
					!= planning.m_iDecisionSequence
				|| order.m_iPlanningBucketSecond
					!= planning.m_iDecisionBucketSecond
				|| order.m_sPlanningDecisionId != planning.m_sDecisionId
				|| order.m_sPlanningInputFingerprint
					!= planning.m_sInputFingerprint
				|| order.m_sPlanningDecisionFingerprint
					!= planning.m_sDecisionFingerprint)
				return "committed enemy order planning backlink diverged";
			if (order.m_sFactionKey != planning.m_sFactionKey
				|| order.m_sTargetZoneId != planning.m_sSelectedTargetZoneId
				|| order.m_sSourceZoneId != planning.m_sSelectedSourceZoneId
				|| order.m_eType != planning.m_eSelectedOrderType
				|| order.m_iAttackCost != planning.m_iAttackCost
				|| order.m_iSupportCost != planning.m_iSupportCost)
				return "committed enemy order frozen selection diverged";
			if (order.m_sOrderId != planning.m_sPlannedOrderId
				|| order.m_sOperationId != planning.m_sPlannedOperationId
				|| order.m_sManifestId != planning.m_sPlannedManifestId
				|| order.m_sManifestHash != planning.m_sPlannedManifestHash
				|| order.m_sResourceDebitMutationId
					!= planning.m_sPlannedDebitMutationId)
				return "committed enemy order frozen durable identity diverged";
			if (order.m_ePlannedSupportType != planning.m_ePlannedSupportType
				|| order.m_sPlanningCapabilityHash
					!= planning.m_sPlanningCapabilityHash)
				return "committed enemy order frozen capability diverged";
			return "";
		}
		if (order)
			return "skipped or rejected enemy planning decision carries an order";
		if (disposition == "rejected" && failure.IsEmpty())
			return "rejected enemy planning decision has no failure reason";
		return "";
	}

	protected HST_EnemyPlanningDecisionResult NewResult(
		HST_EnemyPlanningState planning)
	{
		HST_EnemyPlanningDecisionResult result
			= new HST_EnemyPlanningDecisionResult();
		result.m_PlanningState = planning;
		return result;
	}

	protected HST_EnemyPlanningDecisionResult Reject(
		HST_EnemyPlanningDecisionResult result,
		string failure)
	{
		if (!result)
		{
			ref HST_EnemyPlanningDecisionResult missingResult
				= new HST_EnemyPlanningDecisionResult();
			missingResult.m_sFailureReason = failure;
			return missingResult;
		}
		result.m_sFailureReason = failure;
		return result;
	}

	protected static bool TryAddBounded(int value, int addition, out int result)
	{
		result = value;
		if (value < 0 || addition < 0 || value > int.MAX - addition)
			return false;
		result = value + addition;
		return true;
	}

	protected static bool IsRequiredTrimmed(string value)
	{
		return !value.IsEmpty() && value == value.Trim();
	}

	protected static bool IsOptionalTrimmed(string value)
	{
		return value == value.Trim();
	}
}
