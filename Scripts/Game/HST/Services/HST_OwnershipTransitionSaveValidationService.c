// Schema-62 restore boundary for canonical location ownership authority.
// Pre-schema saves retain every domain fact and become revision-1 baselines;
// they do not replay captures, rewards, garrisons, retaliation, or outcomes.
class HST_OwnershipTransitionSaveValidationService
{
	static const int SCHEMA_VERSION = 62;
	static const string MIGRATION_EVENT_ID = "migration_schema62_ownership_transition";
	static const string CONFLICT_EVENT_ID = "normalization_schema62_ownership_transition_conflict";

	void Normalize(HST_CampaignSaveData saveData, int restoredSchemaVersion)
	{
		if (!saveData)
			return;

		if (restoredSchemaVersion < SCHEMA_VERSION)
		{
			MigrateBaseline(saveData);
			return;
		}

		NormalizeCurrentSchemaAliases(saveData);
		int conflictCount = ValidateCurrentAuthority(saveData);
		if (conflictCount > 0)
			RecordEvent(saveData, CONFLICT_EVENT_ID, "quarantined malformed current-schema ownership authority", conflictCount);
	}

	static bool QuarantineTransitionAuthority(
		HST_CampaignSaveData saveData,
		HST_OwnershipTransitionState transition,
		string failure)
	{
		if (!saveData || !transition)
			return false;

		bool ownedRow;
		foreach (HST_OwnershipTransitionState candidate : saveData.m_aOwnershipTransitions)
		{
			if (candidate != transition)
				continue;
			ownedRow = true;
			break;
		}
		if (!ownedRow)
			return false;

		HST_OwnershipTransitionSaveValidationService validation
			= new HST_OwnershipTransitionSaveValidationService();
		HST_ZoneState zone = validation.FindZone(saveData, transition.m_sZoneId);
		string firstFailure = transition.m_sFailureReason;
		if (firstFailure.IsEmpty() && zone)
			firstFailure = zone.m_sOwnershipAuthorityFailure;
		if (firstFailure.IsEmpty())
			firstFailure = failure;
		if (firstFailure.IsEmpty())
			return false;

		validation.QuarantineTransition(saveData, transition, firstFailure);
		validation.ConvergeRelationalQuarantine(saveData);

		// Relational convergence can quarantine another claimant for the same zone.
		// Retain the first durable failure while preserving the existing zone and
		// marker quarantine rules used by the ownership validator itself.
		transition.m_sFailureReason = firstFailure;
		zone = validation.FindZone(saveData, transition.m_sZoneId);
		if (zone && zone.m_iOwnershipContractVersion
			== HST_OwnershipTransitionService.QUARANTINED_CONTRACT_VERSION)
			zone.m_sOwnershipAuthorityFailure = firstFailure;

		return transition.m_bQuarantined
			&& transition.m_iContractVersion
				== HST_OwnershipTransitionService.QUARANTINED_CONTRACT_VERSION
			&& transition.m_sStatus == "quarantined";
	}

	protected void NormalizeCurrentSchemaAliases(HST_CampaignSaveData saveData)
	{
		if (!saveData)
			return;
		foreach (HST_OwnershipTransitionState transition : saveData.m_aOwnershipTransitions)
		{
			if (!transition)
				continue;
			string previousZoneId = transition.m_sZoneId;
			string canonicalZoneId
				= HST_MaidensBayLocationSaveValidationService.ResolveCanonicalZoneId(previousZoneId);
			transition.m_sZoneId = canonicalZoneId;
			NormalizeSupportZoneAliases(saveData, transition.m_aSupportZoneIds);
			NormalizeSupportZoneAliases(saveData, transition.m_aAppliedSupportZoneIds);
			if (canonicalZoneId == previousZoneId)
				continue;

			HST_StrategicEventState strategicEvent = FindStrategicEvent(
				saveData,
				transition.m_sStrategicEventId);
			if (strategicEvent && strategicEvent.m_sTargetZoneId == previousZoneId)
				strategicEvent.m_sTargetZoneId = canonicalZoneId;
			foreach (HST_CampaignEventState campaignEvent : saveData.m_aCampaignEvents)
			{
				if (campaignEvent
					&& campaignEvent.m_sEventId == transition.m_sCampaignEventId
					&& campaignEvent.m_sAggregateType == "zone"
					&& campaignEvent.m_sAggregateId == previousZoneId)
					campaignEvent.m_sAggregateId = canonicalZoneId;
			}
			if (!transition.m_sOldGarrisonId.IsEmpty())
				transition.m_sOldGarrisonId = HST_StableIdService.BuildGarrisonId(
					canonicalZoneId,
					transition.m_sPreviousOwnerFactionKey);
			if (!transition.m_sNewGarrisonId.IsEmpty())
				transition.m_sNewGarrisonId = HST_StableIdService.BuildGarrisonId(
					canonicalZoneId,
					transition.m_sNewOwnerFactionKey);
			if (transition.m_sMarkerId == "hst_zone_" + previousZoneId)
				transition.m_sMarkerId = "hst_zone_" + canonicalZoneId;
			foreach (HST_EnemyOrderState order : saveData.m_aEnemyOrders)
			{
				if (order && order.m_sOrderId == transition.m_sEnemyOrderId
					&& order.m_sTargetZoneId == previousZoneId)
					order.m_sTargetZoneId = canonicalZoneId;
			}
		}
		foreach (HST_MapMarkerState marker : saveData.m_aMapMarkers)
		{
			if (!marker)
				continue;
			if (marker.m_sLinkedId == HST_MaidensBayLocationSaveValidationService.LEGACY_ZONE_ID)
				marker.m_sLinkedId = HST_MaidensBayLocationSaveValidationService.CANONICAL_ZONE_ID;
			if (marker.m_sMarkerId == HST_MaidensBayLocationSaveValidationService.LEGACY_ZONE_MARKER_ID)
				marker.m_sMarkerId = "hst_zone_"
					+ HST_MaidensBayLocationSaveValidationService.CANONICAL_ZONE_ID;
		}
	}

	protected void NormalizeSupportZoneAliases(
		HST_CampaignSaveData saveData,
		array<string> zoneIds)
	{
		if (!saveData || !zoneIds)
			return;
		for (int zoneIndex = zoneIds.Count() - 1; zoneIndex >= 0; zoneIndex--)
		{
			string previousZoneId = zoneIds[zoneIndex];
			string canonicalZoneId
				= HST_MaidensBayLocationSaveValidationService.ResolveCanonicalZoneId(previousZoneId);
			if (canonicalZoneId == previousZoneId)
				continue;
			HST_ZoneState canonicalZone = FindZone(saveData, canonicalZoneId);
			if (canonicalZone && canonicalZone.m_eType != HST_EZoneType.HST_ZONE_TOWN)
			{
				// The retired duplicate Maiden's Bay town has no surviving civilian
				// authority. Schema-60 already removes its influence rows, so remove
				// the corresponding frozen checklist target rather than replaying it
				// against the canonical logistics resource.
				zoneIds.Remove(zoneIndex);
				continue;
			}
			zoneIds[zoneIndex] = canonicalZoneId;
		}
	}

	protected void MigrateBaseline(HST_CampaignSaveData saveData)
	{
		saveData.m_aOwnershipTransitions.Clear();
		int migratedZones;
		foreach (HST_ZoneState zone : saveData.m_aZones)
		{
			if (!zone)
				continue;
			zone.m_iOwnershipContractVersion = HST_OwnershipTransitionService.EXACT_CONTRACT_VERSION;
			zone.m_iOwnershipRevision = 1;
			zone.m_sActiveOwnershipTransitionRequestId = "";
			zone.m_sLastOwnershipTransitionRequestId = "";
			zone.m_sOwnershipAuthorityFailure = "";
			migratedZones++;
		}

		foreach (HST_MapMarkerState marker : saveData.m_aMapMarkers)
		{
			if (!marker || marker.m_bTombstone || marker.m_sMarkerId.IndexOf("hst_zone_") != 0)
				continue;
			HST_ZoneState zone = FindZone(saveData, marker.m_sLinkedId);
			if (zone)
				marker.m_iSourceRevision = zone.m_iOwnershipRevision;
		}

		RecordEvent(
			saveData,
			MIGRATION_EVENT_ID,
			"existing location owners became revision-1 baselines without replaying ownership side effects",
			migratedZones);
	}

	protected int ValidateCurrentAuthority(HST_CampaignSaveData saveData)
	{
		int conflicts;
		foreach (HST_ZoneState zone : saveData.m_aZones)
		{
			if (!zone)
			{
				conflicts++;
				continue;
			}
			if (zone.m_sZoneId.IsEmpty() || CountZoneIdentity(saveData, zone.m_sZoneId) != 1)
			{
				QuarantineZone(saveData, zone, "missing or duplicate ownership zone identity");
				conflicts++;
				continue;
			}
			if (zone.m_iOwnershipContractVersion != HST_OwnershipTransitionService.EXACT_CONTRACT_VERSION
				|| zone.m_iOwnershipRevision <= 0 || zone.m_iOwnershipRevision == int.MAX)
			{
				QuarantineZone(saveData, zone, "invalid ownership contract version or revision");
				conflicts++;
				continue;
			}
			if (zone.m_sOwnerFactionKey.IsEmpty() || !FindFactionPool(saveData, zone.m_sOwnerFactionKey))
			{
				QuarantineZone(saveData, zone, "current-schema zone owner faction is unknown");
				conflicts++;
			}
		}
		if (CountOwnerAppliedIncompleteTopLevelTransitions(saveData) > 1)
		{
			foreach (HST_OwnershipTransitionState pendingTopLevel : saveData.m_aOwnershipTransitions)
			{
				if (!pendingTopLevel || pendingTopLevel.m_bCompleted
					|| pendingTopLevel.m_bQuarantined
					|| !pendingTopLevel.m_bOwnerApplied
					|| !pendingTopLevel.m_sProjectionParentRequestId.IsEmpty())
					continue;
				QuarantineTransition(
					saveData,
					pendingTopLevel,
					"multiple owner-applied top-level transitions cannot share one publication fence");
				conflicts++;
			}
		}
		foreach (HST_OwnershipTransitionState ownerAppliedTopLevel : saveData.m_aOwnershipTransitions)
		{
			if (!ownerAppliedTopLevel || ownerAppliedTopLevel.m_bCompleted
				|| ownerAppliedTopLevel.m_bQuarantined
				|| !ownerAppliedTopLevel.m_bOwnerApplied
				|| !ownerAppliedTopLevel.m_sProjectionParentRequestId.IsEmpty())
				continue;
			if (!HasEarlierUnresolvedTopLevelTransition(saveData, ownerAppliedTopLevel))
				continue;
			QuarantineTransition(
				saveData,
				ownerAppliedTopLevel,
				"owner-applied top-level transition is not the earliest unresolved publication authority");
			conflicts++;
		}
		conflicts += ValidateQueuedTopLevelOrder(saveData);
		conflicts += ValidateCompletedHistoryOrder(saveData);

		foreach (HST_OwnershipTransitionState transition : saveData.m_aOwnershipTransitions)
		{
			if (!transition)
			{
				conflicts++;
				continue;
			}
			if (transition.m_sRequestId.IsEmpty()
				|| CountTransitionIdentity(saveData, transition.m_sRequestId) != 1)
			{
				QuarantineTransition(saveData, transition, "missing or duplicate ownership request identity");
				conflicts++;
				continue;
			}

			string failure = ValidateTransition(saveData, transition);
			if (failure.IsEmpty())
				continue;
			QuarantineTransition(saveData, transition, failure);
			conflicts++;
		}

		conflicts += ConvergeRelationalQuarantine(saveData);
		return conflicts;
	}

	protected int ConvergeRelationalQuarantine(HST_CampaignSaveData saveData)
	{
		if (!saveData)
			return 0;
		int conflicts;
		int passLimit = Math.Max(
			1,
			saveData.m_aOwnershipTransitions.Count() + saveData.m_aZones.Count() + 1);
		for (int convergencePass; convergencePass < passLimit; convergencePass++)
		{
			bool convergenceChanged;
			foreach (HST_OwnershipTransitionState transition : saveData.m_aOwnershipTransitions)
			{
				if (!transition || transition.m_bQuarantined
					|| transition.m_iContractVersion != HST_OwnershipTransitionService.EXACT_CONTRACT_VERSION)
					continue;
				string failure = ValidateTransition(saveData, transition);
				if (failure.IsEmpty())
					failure = ValidateTransitionBacklink(saveData, transition);
				if (failure.IsEmpty())
					continue;
				QuarantineTransition(saveData, transition, failure);
				conflicts++;
				convergenceChanged = true;
			}

			foreach (HST_ZoneState zone : saveData.m_aZones)
			{
				if (!zone || zone.m_iOwnershipContractVersion
					!= HST_OwnershipTransitionService.EXACT_CONTRACT_VERSION)
					continue;
				string backlinkFailure = ValidateZoneBacklinks(saveData, zone);
				if (backlinkFailure.IsEmpty())
					continue;
				QuarantineZone(saveData, zone, backlinkFailure);
				conflicts++;
				convergenceChanged = true;
			}
			if (!convergenceChanged)
				break;
		}
		return conflicts;
	}

	protected string ValidateTransition(
		HST_CampaignSaveData saveData,
		HST_OwnershipTransitionState transition)
	{
		if (transition.m_iContractVersion == HST_OwnershipTransitionService.QUARANTINED_CONTRACT_VERSION
			|| transition.m_bQuarantined)
			return "";
		if (transition.m_iContractVersion != HST_OwnershipTransitionService.EXACT_CONTRACT_VERSION)
			return "unsupported ownership transition contract version";
		HST_ZoneState zone = FindZone(saveData, transition.m_sZoneId);
		if (transition.m_sZoneId.IsEmpty() || !zone)
			return "ownership transition references a missing zone";
		if (zone.m_iOwnershipContractVersion != HST_OwnershipTransitionService.EXACT_CONTRACT_VERSION)
			return "ownership transition references quarantined zone authority";

		string failure = ValidateTransitionFingerprint(saveData, transition);
		if (!failure.IsEmpty())
			return failure;
		failure = ValidateCausePolicy(transition);
		if (!failure.IsEmpty())
			return failure;
		failure = ValidateTransitionProgress(transition);
		if (!failure.IsEmpty())
			return failure;
		failure = ValidateTransitionSupportTargets(saveData, transition);
		if (!failure.IsEmpty())
			return failure;
		failure = ValidateTransitionDecisions(saveData, transition);
		if (!failure.IsEmpty())
			return failure;
		failure = ValidateTransitionEventCorrelations(saveData, transition);
		if (!failure.IsEmpty())
			return failure;
		failure = ValidateTransitionGarrisonCorrelations(saveData, transition);
		if (!failure.IsEmpty())
			return failure;
		failure = ValidateTransitionCounterattackCorrelation(saveData, transition);
		if (!failure.IsEmpty())
			return failure;
		failure = ValidateTransitionMarkerEvidence(saveData, transition);
		if (!failure.IsEmpty())
			return failure;
		return ValidateTransitionProjection(saveData, transition);
	}

	protected string ValidateTransitionFingerprint(
		HST_CampaignSaveData saveData,
		HST_OwnershipTransitionState transition)
	{
		if (!IsKnownCause(transition.m_sCause) || transition.m_sSourceType.IsEmpty()
			|| transition.m_sExpectedOwnerFactionKey.IsEmpty() || transition.m_sNewOwnerFactionKey.IsEmpty())
			return "ownership transition immutable request fingerprint is incomplete";
		if (transition.m_sRequestId.Length() > 180 || transition.m_sSourceType.Length() > 80
			|| transition.m_sSourceId.IsEmpty() || transition.m_sSourceId.Length() > 180
			|| transition.m_sActorIdentityId.Length() > 180
			|| transition.m_sReason.Trim().IsEmpty()
			|| transition.m_sReason != transition.m_sReason.Trim()
			|| transition.m_sReason.Length() > 320)
			return "ownership transition immutable request fingerprint is outside bounded lengths";
		if (!FindFactionPool(saveData, transition.m_sNewOwnerFactionKey)
			|| (transition.m_sCause != "migration_repair"
				&& !FindFactionPool(saveData, transition.m_sExpectedOwnerFactionKey)))
			return "ownership transition fingerprint references an unknown faction";
		if (transition.m_sExpectedOwnerFactionKey == transition.m_sNewOwnerFactionKey
			|| transition.m_sPreviousOwnerFactionKey != transition.m_sExpectedOwnerFactionKey)
			return "ownership transition owner fingerprint is contradictory";
		if (transition.m_iSupportReward < -100 || transition.m_iSupportReward > 100)
			return "ownership transition support policy is outside the bounded range";
		return "";
	}

	protected string ValidateCausePolicy(HST_OwnershipTransitionState transition)
	{
		if (!transition)
			return "ownership transition cause policy is unavailable";
		if (transition.m_sCause == "admin")
		{
			if (transition.m_bApplyEnemyConsequences)
				return "admin ownership policy cannot apply enemy consequences";
			if (transition.m_iSupportReward != 0)
				return "admin ownership policy cannot apply capture support";
		}
		if (transition.m_sCause == "debug_seed")
		{
			if (transition.m_bApplyEnemyConsequences)
				return "debug ownership policy cannot apply enemy consequences";
			if (transition.m_bNotify)
				return "debug ownership policy cannot publish gameplay notification";
			if (transition.m_iSupportReward != 0)
				return "debug ownership policy cannot apply capture support";
		}
		if (transition.m_sCause == "migration_repair")
		{
			if (transition.m_bApplyEnemyConsequences || transition.m_bNotify)
				return "migration ownership policy cannot apply retaliation or notification";
			if (transition.m_bReconcileSecurity || transition.m_bCreateSecurity)
				return "migration ownership policy cannot rewrite security authority";
			if (transition.m_iSupportReward != 0)
				return "migration ownership policy cannot apply capture support";
		}
		if (transition.m_sCause == "political_support" && transition.m_iSupportReward != 0)
			return "political ownership policy cannot recursively apply capture support";
		return "";
	}

	protected string ValidateTransitionProgress(HST_OwnershipTransitionState transition)
	{
		if (transition.m_iExpectedRevision <= 0 || transition.m_iCreatedAtSecond < 0
			|| transition.m_iLastAttemptAtSecond < 0 || transition.m_iCompletedAtSecond < 0
			|| transition.m_iAttemptCount < 0)
			return "ownership transition revision, time, or attempt metadata is invalid";
		if (transition.m_iExpectedRevision >= int.MAX - 1)
			return "ownership transition expected revision cannot advance safely";
		if (transition.m_iLastAttemptAtSecond < transition.m_iCreatedAtSecond)
			return "ownership transition attempt predates admission";
		if (transition.m_iCompletedAtSecond > 0
			&& transition.m_iCompletedAtSecond < transition.m_iCreatedAtSecond)
			return "ownership transition completion predates admission";
		if (!transition.m_bCompleted && transition.m_iCompletedAtSecond != 0)
			return "incomplete ownership transition has completion-time residue";
		if (transition.m_bCompleted && !transition.m_sFailureReason.IsEmpty())
			return "completed ownership transition retains failure residue";
		if (transition.m_iAppliedRevision != 0
			&& transition.m_iAppliedRevision != transition.m_iExpectedRevision + 1)
			return "ownership transition result revision is not exactly expected plus one";
		if (transition.m_bOwnerApplied
			&& (transition.m_iAppliedRevision != transition.m_iExpectedRevision + 1
				|| transition.m_sPreviousOwnerFactionKey != transition.m_sExpectedOwnerFactionKey))
			return "ownership transition owner step is not reciprocal with its request";
		if (!transition.m_bOwnerApplied && transition.m_iAppliedRevision != 0)
			return "ownership transition has a result revision before its owner step";
		if (!transition.m_bValidated || !CompletionOrderIsValid(transition))
			return "ownership transition checklist order is invalid";
		if (transition.m_bCompleted && !AllCompletionStepsApplied(transition))
			return "completed ownership transition is missing required checklist steps";
		if (transition.m_bCompleted && transition.m_sStatus != "completed")
			return "completed ownership transition has the wrong status";
		if (!transition.m_bCompleted && transition.m_sStatus != "validated"
			&& transition.m_sStatus != "applying" && transition.m_sStatus != "projecting")
			return "incomplete ownership transition has an unsupported status";
		if (transition.m_aAppliedSupportZoneIds.Count() > transition.m_aSupportZoneIds.Count())
			return "ownership support checklist contains more applied rows than frozen targets";
		return "";
	}

	protected string ValidateTransitionSupportTargets(
		HST_CampaignSaveData saveData,
		HST_OwnershipTransitionState transition)
	{
		if (transition.m_iSupportReward == 0
			&& (!transition.m_aSupportZoneIds.IsEmpty()
				|| !transition.m_aAppliedSupportZoneIds.IsEmpty()))
			return "zero-reward ownership transition has a support target checklist";
		string exactTargetFailure = ValidateExactSupportTargetSet(saveData, transition);
		if (!exactTargetFailure.IsEmpty())
			return exactTargetFailure;
		array<string> supportZoneIds = {};
		foreach (string supportZoneId : transition.m_aSupportZoneIds)
		{
			HST_ZoneState supportZone = FindZone(saveData, supportZoneId);
			if (supportZoneId.IsEmpty() || supportZoneIds.Contains(supportZoneId)
				|| supportZoneId != HST_MaidensBayLocationSaveValidationService.ResolveCanonicalZoneId(supportZoneId)
				|| !supportZone || supportZone.m_eType != HST_EZoneType.HST_ZONE_TOWN)
				return "ownership support target checklist is duplicated or invalid";
			supportZoneIds.Insert(supportZoneId);
		}
		array<string> appliedSupportZoneIds = {};
		int appliedSupportIndex;
		foreach (string appliedZoneId : transition.m_aAppliedSupportZoneIds)
		{
			if (appliedSupportIndex >= transition.m_aSupportZoneIds.Count()
				|| transition.m_aSupportZoneIds[appliedSupportIndex] != appliedZoneId)
				return "ownership applied-support checklist is not the ordered retry prefix";
			if (!transition.m_aSupportZoneIds.Contains(appliedZoneId)
				|| appliedSupportZoneIds.Contains(appliedZoneId))
				return "ownership support checklist references an unfrozen town";
			string eventFailure = ValidateAppliedSupportEvent(
				saveData,
				transition,
				appliedZoneId);
			if (!eventFailure.IsEmpty())
				return eventFailure;
			appliedSupportZoneIds.Insert(appliedZoneId);
			appliedSupportIndex++;
		}
		if (transition.m_bSupportApplied
			&& transition.m_aAppliedSupportZoneIds.Count() != transition.m_aSupportZoneIds.Count())
			return "completed ownership support step has unapplied frozen towns";
		return "";
	}

	protected string ValidateExactSupportTargetSet(
		HST_CampaignSaveData saveData,
		HST_OwnershipTransitionState transition)
	{
		if (!saveData || !transition)
			return "ownership deterministic support target authority is unavailable";
		array<string> expectedZoneIds = {};
		if (transition.m_iSupportReward != 0)
		{
			HST_ZoneState sourceZone = FindZone(saveData, transition.m_sZoneId);
			if (!sourceZone)
				return "ownership deterministic support source zone is unavailable";
			foreach (string linkedZoneId : sourceZone.m_aLinkedZoneIds)
			{
				HST_ZoneState linkedZone = FindZone(saveData, linkedZoneId);
				if (linkedZone && linkedZone.m_eType == HST_EZoneType.HST_ZONE_TOWN
					&& !expectedZoneIds.Contains(linkedZone.m_sZoneId))
					expectedZoneIds.Insert(linkedZone.m_sZoneId);
			}

			float supportRadiusSq = 1500.0 * 1500.0;
			foreach (HST_ZoneState candidateZone : saveData.m_aZones)
			{
				if (!candidateZone || candidateZone.m_eType != HST_EZoneType.HST_ZONE_TOWN
					|| expectedZoneIds.Contains(candidateZone.m_sZoneId))
					continue;
				if (DistanceSq2D(candidateZone.m_vPosition, sourceZone.m_vPosition) <= supportRadiusSq)
					expectedZoneIds.Insert(candidateZone.m_sZoneId);
			}
			expectedZoneIds.Sort();
		}

		if (transition.m_aSupportZoneIds.Count() != expectedZoneIds.Count())
			return "ownership support targets do not match the deterministic admission set";
		for (int targetIndex; targetIndex < expectedZoneIds.Count(); targetIndex++)
		{
			if (transition.m_aSupportZoneIds[targetIndex] != expectedZoneIds[targetIndex])
				return "ownership support targets do not match the deterministic admission set";
		}
		return "";
	}

	protected string ValidateAppliedSupportEvent(
		HST_CampaignSaveData saveData,
		HST_OwnershipTransitionState transition,
		string zoneId)
	{
		if (!saveData || !transition || zoneId.IsEmpty())
			return "ownership support influence-event correlation is unavailable";
		string expectedEventId = HST_TownInfluenceService.BuildOwnershipRewardEventId(
			zoneId,
			transition.m_sRequestId);
		string legacyExpectedEventId = string.Format(
			"town_influence_ownership_%1_%2",
			zoneId,
			transition.m_sRequestId.Hash());
		int exactIdCount;
		int correlatedCount;
		HST_TownInfluenceEventState correlatedEvent;
		foreach (HST_TownInfluenceEventState influenceEvent : saveData.m_aTownInfluenceEvents)
		{
			if (influenceEvent && (influenceEvent.m_sEventId == expectedEventId
				|| influenceEvent.m_sEventId == legacyExpectedEventId))
			{
				exactIdCount++;
				correlatedEvent = influenceEvent;
			}
			if (!influenceEvent || influenceEvent.m_sKind != "ownership_support"
				|| influenceEvent.m_sSourceId != transition.m_sRequestId
				|| influenceEvent.m_sZoneId != zoneId)
				continue;
			correlatedCount++;
		}
		if (exactIdCount != 1 || correlatedCount != 1 || !correlatedEvent
			|| (correlatedEvent.m_sEventId != expectedEventId
				&& correlatedEvent.m_sEventId != legacyExpectedEventId))
			return "ownership applied-support claim lacks one deterministic influence event";
		if (correlatedEvent.m_sKind != "ownership_support"
			|| correlatedEvent.m_sSourceId != transition.m_sRequestId
			|| correlatedEvent.m_sZoneId != zoneId)
			return "ownership applied-support exact event row is not causally correlated";
		if (correlatedEvent.m_iContractVersion != 0
			&& correlatedEvent.m_iContractVersion
				!= HST_TownInfluenceService.EXACT_CONTRACT_VERSION)
			return "ownership applied-support event contract is invalid";
		if (correlatedEvent.m_iContractVersion
			== HST_TownInfluenceService.EXACT_CONTRACT_VERSION)
		{
			HST_TownInfluenceRecord influenceRecord
				= FindUniqueTownInfluenceRecord(saveData, zoneId);
			if (!influenceRecord
				|| influenceRecord.m_iContractVersion
					!= HST_TownInfluenceService.EXACT_CONTRACT_VERSION
				|| !influenceRecord.m_sAuthorityFailure.IsEmpty())
				return "ownership applied-support event lacks canonical town authority";
		}

		int expectedFIADelta;
		int expectedOccupierDelta;
		if (transition.m_iSupportReward > 0)
			expectedFIADelta = transition.m_iSupportReward;
		else
			expectedOccupierDelta = -transition.m_iSupportReward;
		if (!correlatedEvent.m_bApplied || correlatedEvent.m_iExpiresAtSecond != 0
			|| correlatedEvent.m_sReason != "linked support from ownership transition"
			|| correlatedEvent.m_iFIASupportDelta != expectedFIADelta
			|| correlatedEvent.m_iOccupierSupportDelta != expectedOccupierDelta
			|| correlatedEvent.m_iReputationDelta != transition.m_iSupportReward / 2
			|| correlatedEvent.m_iHeatDelta != 0 || correlatedEvent.m_iPopulationDelta != 0
			|| correlatedEvent.m_iPoliceDelta != 0 || correlatedEvent.m_iRoadblockDelta != 0)
			return "ownership applied-support influence event has mismatched policy facts";
		return "";
	}

	protected string ValidateTransitionDecisions(
		HST_CampaignSaveData saveData,
		HST_OwnershipTransitionState transition)
	{
		if (transition.m_sStrategicEventId.IsEmpty())
			return "ownership transition strategic event authority is missing";
		if (!transition.m_bStrategicEventCompleted
			&& !FindStrategicEvent(saveData, transition.m_sStrategicEventId))
			return "incomplete ownership strategic event authority is missing";
		if (transition.m_bEventAppended
			&& transition.m_sCampaignEventId.IsEmpty())
			return "ownership transition campaign event identity is missing";
		if (transition.m_bFacilitiesApplied && transition.m_sFacilityLogisticsDecision.IsEmpty())
			return "ownership transition facility/logistics decision is missing";
		if (transition.m_bEnemyConsequencesApplied && transition.m_sEnemyConsequenceDecision.IsEmpty())
			return "ownership transition enemy-consequence decision is missing";
		if (transition.m_bProjectionRequested && transition.m_sProjectionDecision.IsEmpty())
			return "ownership transition projection decision is missing";
		if (transition.m_iCounterattackChance < 0 || transition.m_iCounterattackChance > 95
			|| transition.m_iCounterattackRoll < 0 || transition.m_iCounterattackRoll > 99)
			return "ownership transition frozen counterattack roll is outside policy bounds";
		if (transition.m_bCounterattackSelected
			!= (transition.m_iCounterattackRoll < transition.m_iCounterattackChance))
			return "ownership transition frozen counterattack selection contradicts its roll";
		if (!transition.m_bApplyEnemyConsequences
			&& (transition.m_iCounterattackChance != 0 || transition.m_iCounterattackRoll != 0
				|| transition.m_bCounterattackSelected))
			return "inapplicable ownership counterattack policy has frozen decision residue";
		if (transition.m_iAggressionApplied < 0)
			return "ownership transition aggression evidence cannot be negative";
		return "";
	}

	protected string ValidateTransitionEventCorrelations(
		HST_CampaignSaveData saveData,
		HST_OwnershipTransitionState transition)
	{
		if (!saveData || !transition)
			return "ownership transition event correlation is unavailable";
		int strategicEventCount;
		int strategicClaimantCount;
		HST_StrategicEventState strategicEvent;
		foreach (HST_StrategicEventState candidateEvent : saveData.m_aStrategicEvents)
		{
			if (candidateEvent && candidateEvent.m_sEventId == transition.m_sStrategicEventId)
			{
				strategicEventCount++;
				strategicEvent = candidateEvent;
			}
		}
		foreach (HST_OwnershipTransitionState claimant : saveData.m_aOwnershipTransitions)
		{
			if (claimant && claimant.m_sStrategicEventId == transition.m_sStrategicEventId)
				strategicClaimantCount++;
		}
		if (strategicEventCount != 1 || strategicClaimantCount != 1 || !strategicEvent)
			return "ownership transition strategic event identity is missing, duplicated, or shared";
		string expectedKind = "ownership_transition";
		if (transition.m_sCause == "military_capture" || transition.m_sCause == "mission_capture")
			expectedKind = "zone_captured";
		if (strategicEvent.m_sKind != expectedKind
			|| strategicEvent.m_sSourceType != transition.m_sSourceType
			|| strategicEvent.m_sSourceId != transition.m_sSourceId
			|| strategicEvent.m_sTargetZoneId != transition.m_sZoneId
			|| strategicEvent.m_sTargetFactionKey != transition.m_sPreviousOwnerFactionKey
			|| strategicEvent.m_sReason != transition.m_sReason
			|| strategicEvent.m_iCreatedAtSecond != transition.m_iCreatedAtSecond
			|| strategicEvent.m_sOwnerBefore != transition.m_sPreviousOwnerFactionKey)
			return "ownership transition strategic event immutable facts do not correlate";
		if (strategicEvent.m_iFactionMoneyDelta != 0 || strategicEvent.m_iHRDelta != 0
			|| strategicEvent.m_iHQKnowledgeDelta != 0
			|| strategicEvent.m_iHQKnowledgeBefore != 0
			|| strategicEvent.m_iHQKnowledgeAfter != 0
			|| strategicEvent.m_iTownSupportDelta != 0
			|| strategicEvent.m_iSupportBefore != 0
			|| strategicEvent.m_iSupportAfter != 0
			|| strategicEvent.m_iAttackResourceDelta != 0
			|| strategicEvent.m_iSupportResourceDelta != 0)
			return "ownership strategic event absorbed unrelated campaign deltas";
		if (!transition.m_bOwnerApplied)
		{
			if (!strategicEvent.m_sOwnerAfter.IsEmpty()
				|| strategicEvent.m_iCaptureProgressAfter
					!= strategicEvent.m_iCaptureProgressBefore
				|| strategicEvent.m_iCaptureProgressDelta != 0
				|| strategicEvent.m_iAggressionDelta != 0)
				return "pre-owner ownership strategic event has owner-effect residue";
		}
		else if (strategicEvent.m_iCaptureProgressBefore < 0
			|| strategicEvent.m_iCaptureProgressAfter != 0
			|| strategicEvent.m_iCaptureProgressDelta
				!= -strategicEvent.m_iCaptureProgressBefore)
			return "owner-applied strategic event lacks exact capture-progress reset evidence";
		if (!transition.m_bStrategicEventCompleted)
		{
			if (strategicEvent.m_bApplied || !strategicEvent.m_sSummary.IsEmpty()
				|| !strategicEvent.m_sOwnerAfter.IsEmpty()
				|| strategicEvent.m_iAggressionDelta != 0)
				return "incomplete ownership strategic event has completion residue";
		}
		else if (!strategicEvent.m_bApplied || strategicEvent.m_sSummary.IsEmpty()
			|| strategicEvent.m_sOwnerAfter != transition.m_sNewOwnerFactionKey
			|| strategicEvent.m_iAggressionDelta != transition.m_iAggressionApplied)
			return "completed ownership strategic event lacks applied owner evidence";

		int campaignIdCount;
		int campaignClaimantCount;
		int commandRowCount;
		HST_CampaignEventState campaignEvent;
		foreach (HST_CampaignEventState eventRow : saveData.m_aCampaignEvents)
		{
			if (!eventRow)
				continue;
			if (!transition.m_sCampaignEventId.IsEmpty()
				&& eventRow.m_sEventId == transition.m_sCampaignEventId)
			{
				campaignIdCount++;
				campaignEvent = eventRow;
			}
			if (eventRow.m_sCommandRequestId == transition.m_sRequestId)
				commandRowCount++;
		}
		if (!transition.m_sCampaignEventId.IsEmpty())
		{
			foreach (HST_OwnershipTransitionState campaignClaimant : saveData.m_aOwnershipTransitions)
			{
				if (campaignClaimant
					&& campaignClaimant.m_sCampaignEventId == transition.m_sCampaignEventId)
					campaignClaimantCount++;
			}
		}
		if (!transition.m_bEventAppended)
		{
			if (!transition.m_sCampaignEventId.IsEmpty() || commandRowCount != 0)
				return "ownership transition has campaign-event residue before append";
			return "";
		}
		if (transition.m_sCampaignEventId.IsEmpty() || campaignClaimantCount != 1
			|| campaignIdCount > 1 || commandRowCount > 1
			|| campaignIdCount != commandRowCount)
			return "ownership campaign event identity is empty, duplicated, shared, or contradictory";
		if (!campaignEvent)
			return "";
		string expectedTransition = string.Format(
			"owner_%1_to_%2_revision_%3",
			transition.m_sPreviousOwnerFactionKey,
			transition.m_sNewOwnerFactionKey,
			transition.m_iAppliedRevision);
		if (campaignEvent.m_sCategory != "ownership" || campaignEvent.m_sAggregateType != "zone"
			|| campaignEvent.m_sAggregateId != transition.m_sZoneId
			|| campaignEvent.m_sCommandRequestId != transition.m_sRequestId
			|| campaignEvent.m_sTransition != expectedTransition
			|| campaignEvent.m_sReason != transition.m_sReason)
			return "ownership campaign event retained row does not correlate";
		return "";
	}

	protected string ValidateTransitionGarrisonCorrelations(
		HST_CampaignSaveData saveData,
		HST_OwnershipTransitionState transition)
	{
		if (!saveData || !transition)
			return "ownership transition garrison correlation is unavailable";
		if (!transition.m_bOldSecurityRetired && !transition.m_sOldGarrisonId.IsEmpty())
			return "ownership transition has old-garrison residue before retirement";
		if (!transition.m_bNewSecurityApplied && !transition.m_sNewGarrisonId.IsEmpty())
			return "ownership transition has new-garrison residue before security apply";
		string oldFailure = ValidateGarrisonIdentity(
			saveData,
			transition.m_sOldGarrisonId,
			transition.m_sZoneId,
			transition.m_sPreviousOwnerFactionKey);
		if (!oldFailure.IsEmpty())
			return oldFailure;
		string newFailure = ValidateGarrisonIdentity(
			saveData,
			transition.m_sNewGarrisonId,
			transition.m_sZoneId,
			transition.m_sNewOwnerFactionKey);
		if (!newFailure.IsEmpty())
			return newFailure;

		HST_ZoneState zone = FindZone(saveData, transition.m_sZoneId);
		if (!transition.m_bCompleted && !transition.m_bOwnerApplied
			&& transition.m_bReconcileSecurity && transition.m_bCreateSecurity
			&& transition.m_bNewSecurityApplied && zone && zone.m_iGarrisonSlots > 0
			&& (transition.m_sNewGarrisonId.IsEmpty()
				|| CountGarrisonIdentityRows(saveData, transition.m_sNewGarrisonId) != 1))
			return "incomplete pre-owner transition lacks required new-garrison authority";
		return "";
	}

	protected string ValidateGarrisonIdentity(
		HST_CampaignSaveData saveData,
		string garrisonId,
		string zoneId,
		string factionKey)
	{
		if (garrisonId.IsEmpty())
			return "";
		string expectedId = HST_StableIdService.BuildGarrisonId(zoneId, factionKey);
		if (garrisonId != expectedId)
			return "ownership transition garrison identity is not stable for zone and owner";
		int rowCount;
		HST_GarrisonState retained;
		foreach (HST_GarrisonState garrison : saveData.m_aGarrisons)
		{
			if (garrison && garrison.m_sGarrisonId == garrisonId)
			{
				rowCount++;
				retained = garrison;
			}
		}
		if (rowCount > 1)
			return "ownership transition garrison identity is duplicated";
		if (retained && (retained.m_sZoneId != zoneId || retained.m_sFactionKey != factionKey))
			return "ownership transition retained garrison row does not correlate";
		return "";
	}

	protected int CountGarrisonIdentityRows(HST_CampaignSaveData saveData, string garrisonId)
	{
		if (!saveData || garrisonId.IsEmpty())
			return 0;
		int count;
		foreach (HST_GarrisonState garrison : saveData.m_aGarrisons)
		{
			if (garrison && garrison.m_sGarrisonId == garrisonId)
				count++;
		}
		return count;
	}

	protected string ValidateTransitionCounterattackCorrelation(
		HST_CampaignSaveData saveData,
		HST_OwnershipTransitionState transition)
	{
		if (!saveData || !transition)
			return "ownership transition counterattack correlation is unavailable";
		if (!transition.m_bEnemyConsequencesApplied)
		{
			if (transition.m_bCounterattackQueued || !transition.m_sEnemyOrderId.IsEmpty()
				|| !transition.m_sEnemyConsequenceDecision.IsEmpty())
				return "ownership transition has enemy-consequence residue before apply";
			if (transition.m_iAggressionApplied > 0
				&& (!transition.m_bApplyEnemyConsequences
					|| transition.m_bOwnerApplied || transition.m_bCompleted
					|| transition.m_sStatus != "applying"))
				return "ownership transition has invalid pre-owner aggression admission";
			return "";
		}
		string nonApplicableDecision = "enemy retaliation not applicable for this ownership policy";
		if (transition.m_sEnemyConsequenceDecision == nonApplicableDecision)
		{
			if (transition.m_iAggressionApplied != 0
				|| transition.m_iCounterattackChance != 0
				|| transition.m_iCounterattackRoll != 0
				|| transition.m_bCounterattackSelected
				|| transition.m_bCounterattackQueued
				|| !transition.m_sEnemyOrderId.IsEmpty())
				return "non-applicable ownership enemy policy has consequence residue";
			return "";
		}
		string expectedDecision = string.Format(
			"aggression %1 | counterattack chance %2 roll %3 selected %4 queued %5 order %6",
			transition.m_iAggressionApplied,
			transition.m_iCounterattackChance,
			transition.m_iCounterattackRoll,
			transition.m_bCounterattackSelected,
			transition.m_bCounterattackQueued,
			transition.m_sEnemyOrderId);
		if (transition.m_iAggressionApplied <= 0
			|| transition.m_sEnemyConsequenceDecision != expectedDecision)
			return "ownership enemy-consequence decision does not match frozen receipt facts";
		if (!transition.m_bCounterattackQueued)
		{
			if (!transition.m_sEnemyOrderId.IsEmpty())
				return "nonqueued ownership counterattack retains an enemy-order identity";
			return "";
		}
		if (!transition.m_bCounterattackSelected || transition.m_sEnemyOrderId.IsEmpty())
			return "queued ownership counterattack lacks frozen selection or identity";
		int orderCount;
		int claimantCount;
		HST_EnemyOrderState retainedOrder;
		foreach (HST_EnemyOrderState order : saveData.m_aEnemyOrders)
		{
			if (order && order.m_sOrderId == transition.m_sEnemyOrderId)
			{
				orderCount++;
				retainedOrder = order;
			}
		}
		foreach (HST_OwnershipTransitionState claimant : saveData.m_aOwnershipTransitions)
		{
			if (claimant && claimant.m_sEnemyOrderId == transition.m_sEnemyOrderId)
				claimantCount++;
		}
		if (orderCount != 1 || claimantCount != 1 || !retainedOrder)
			return "ownership counterattack order is missing, duplicated, or shared";
		if (retainedOrder.m_eType != HST_EEnemyOrderType.HST_ENEMY_ORDER_COUNTERATTACK
			|| retainedOrder.m_sFactionKey != transition.m_sPreviousOwnerFactionKey
			|| retainedOrder.m_sTargetZoneId != transition.m_sZoneId)
			return "ownership counterattack retained order does not correlate";
		return "";
	}

	protected string ValidateTransitionMarkerEvidence(
		HST_CampaignSaveData saveData,
		HST_OwnershipTransitionState transition)
	{
		if (!saveData || !transition)
			return "ownership transition marker evidence is unavailable";
		bool unpublishedOwnerStep = (transition.m_sProjectionParentRequestId.IsEmpty()
				&& !transition.m_bCompleted && !transition.m_bProjectionRequested)
			|| (!transition.m_sProjectionParentRequestId.IsEmpty()
				&& !transition.m_bDeferredPublicationReleased);
		if (unpublishedOwnerStep)
		{
			string unpublishedMarkerId = "hst_zone_"
				+ HST_MaidensBayLocationSaveValidationService.ResolveCanonicalZoneId(
					transition.m_sZoneId);
			int retainedUnpublishedCount;
			foreach (HST_MapMarkerState unpublishedMarker : saveData.m_aMapMarkers)
			{
				if (!unpublishedMarker || unpublishedMarker.m_sMarkerId != unpublishedMarkerId)
					continue;
				retainedUnpublishedCount++;
				if (unpublishedMarker.m_sOwnerFactionKey
						!= transition.m_sPreviousOwnerFactionKey
					|| unpublishedMarker.m_iSourceRevision != transition.m_iExpectedRevision)
					return "unpublished ownership transition leaked a non-prior marker snapshot";
			}
			if (retainedUnpublishedCount > 1)
				return "unpublished ownership transition has duplicate prior marker rows";
		}
		bool anyMarkerEvidence = !transition.m_sMarkerId.IsEmpty()
			|| transition.m_iMarkerProjectionEpoch != 0 || transition.m_iMarkerRevision != 0
			|| transition.m_iMarkerStreamSequence != 0;
		if (transition.m_bSetupProjectionWithoutMarkers)
		{
			if (anyMarkerEvidence)
				return "setup ownership publication flag conflicts with marker evidence";
			if (!transition.m_bProjectionRequested)
				return "setup ownership publication flag predates projection completion";
			if (transition.m_sProjectionParentRequestId.IsEmpty())
			{
				if (transition.m_sProjectionDecision
					!= "setup phase has no live zone-marker projection; owner-derived views will publish on activation")
					return "setup ownership publication flag lacks its immutable decision";
			}
			else
			{
				if (!transition.m_bCompleted || !transition.m_bDeferredPublicationReleased
					|| !transition.m_sProjectionDecision.Contains("publication released by parent "))
					return "nested setup ownership publication flag lacks released child authority";
				HST_OwnershipTransitionState setupParent = FindTransition(
					saveData,
					transition.m_sProjectionParentRequestId);
				if (setupParent && !setupParent.m_bSetupProjectionWithoutMarkers)
					return "nested setup ownership publication flag disagrees with its retained parent";
			}
			HST_ZoneState setupZone = FindZone(saveData, transition.m_sZoneId);
			bool setupCurrentChain = setupZone
				&& (setupZone.m_sActiveOwnershipTransitionRequestId == transition.m_sRequestId
					|| setupZone.m_sLastOwnershipTransitionRequestId == transition.m_sRequestId);
			if (setupCurrentChain)
			{
				string setupMarkerId = "hst_zone_"
					+ HST_MaidensBayLocationSaveValidationService.ResolveCanonicalZoneId(
						transition.m_sZoneId);
				int setupMarkerCount;
				HST_MapMarkerState setupMarker;
				foreach (HST_MapMarkerState candidateSetupMarker : saveData.m_aMapMarkers)
				{
					if (!candidateSetupMarker
						|| candidateSetupMarker.m_sMarkerId != setupMarkerId)
						continue;
					setupMarkerCount++;
					setupMarker = candidateSetupMarker;
				}
				if (setupMarkerCount > 1)
					return "setup ownership publication has duplicate current marker rows";
				if (setupMarker && !setupMarker.m_bTombstone
					&& (!setupMarker.m_bVisible
						|| setupMarker.m_sOwnerFactionKey != transition.m_sNewOwnerFactionKey
						|| setupMarker.m_iSourceRevision != transition.m_iAppliedRevision))
					return "setup ownership current marker row diverges from published authority";
			}
			return "";
		}
		if (!transition.m_sProjectionParentRequestId.IsEmpty()
			&& !transition.m_bDeferredPublicationReleased)
		{
			if (anyMarkerEvidence)
				return "unreleased nested ownership transition has marker evidence residue";
			return "";
		}

		bool markerEvidenceRequired = transition.m_bProjectionRequested;
		if (!transition.m_sProjectionParentRequestId.IsEmpty()
			&& transition.m_bDeferredPublicationReleased)
			markerEvidenceRequired = true;
		if (!anyMarkerEvidence)
		{
			if (markerEvidenceRequired)
				return "published ownership transition lacks marker evidence";
			return "";
		}

		string canonicalZoneId = HST_MaidensBayLocationSaveValidationService.ResolveCanonicalZoneId(
			transition.m_sZoneId);
		string expectedMarkerId = "hst_zone_" + canonicalZoneId;
		if (transition.m_sMarkerId != expectedMarkerId
			|| transition.m_iMarkerProjectionEpoch <= 0
			|| transition.m_iMarkerProjectionEpoch > saveData.m_iMarkerProjectionEpoch
			|| transition.m_iMarkerRevision <= 0 || transition.m_iMarkerRevision == int.MAX
			|| transition.m_iMarkerStreamSequence <= 0
			|| transition.m_iMarkerStreamSequence > saveData.m_iMarkerProjectionSequence)
			return "ownership transition marker evidence is partial, noncanonical, or outside campaign bounds";

		HST_ZoneState zone = FindZone(saveData, transition.m_sZoneId);
		bool currentChain = zone
			&& (zone.m_sActiveOwnershipTransitionRequestId == transition.m_sRequestId
				|| zone.m_sLastOwnershipTransitionRequestId == transition.m_sRequestId);
		if (!currentChain)
			return "";
		int markerCount;
		HST_MapMarkerState retainedMarker;
		foreach (HST_MapMarkerState marker : saveData.m_aMapMarkers)
		{
			if (marker && marker.m_sMarkerId == expectedMarkerId)
			{
				markerCount++;
				retainedMarker = marker;
			}
		}
		if (markerCount != 1 || !retainedMarker || retainedMarker.m_bTombstone
			|| !retainedMarker.m_bVisible)
			return "current ownership marker evidence is missing, duplicated, or hidden";
		if (retainedMarker.m_sOwnerFactionKey != transition.m_sNewOwnerFactionKey
			|| retainedMarker.m_iSourceRevision != transition.m_iAppliedRevision
			|| retainedMarker.m_iRevision < transition.m_iMarkerRevision
			|| retainedMarker.m_iStreamSequence < transition.m_iMarkerStreamSequence)
			return "current ownership marker row is older than or divergent from receipt evidence";
		return "";
	}

	protected string ValidateTransitionProjection(
		HST_CampaignSaveData saveData,
		HST_OwnershipTransitionState transition)
	{
		if (HasProjectionParentCycle(saveData, transition))
			return "ownership transition projection-parent authority is cyclic";
		if (transition.m_sProjectionParentRequestId.IsEmpty())
		{
			string childSetFailure = ValidateUnreleasedChildSet(saveData, transition);
			if (!childSetFailure.IsEmpty())
				return childSetFailure;
			if (transition.m_bDeferredPublicationReleased)
				return "top-level ownership transition claims a deferred publication release";
			if (transition.m_bProjectionRequested
				&& !transition.m_bSetupProjectionWithoutMarkers
				&& (transition.m_sMarkerId.IsEmpty() || transition.m_iMarkerRevision <= 0
					|| transition.m_iMarkerStreamSequence <= 0))
				return "top-level ownership publication lacks marker correlation evidence";
		}
		else
		{
			if (transition.m_sProjectionParentRequestId == transition.m_sRequestId)
				return "ownership transition delegates publication to itself";
			HST_OwnershipTransitionState parent = FindTransition(saveData, transition.m_sProjectionParentRequestId);
			if (parent && parent.m_sZoneId == transition.m_sZoneId)
				return "nested ownership transition cannot share its parent's zone identity";
			string childShapeFailure = ValidateNestedChildShape(saveData, transition, parent);
			if (!childShapeFailure.IsEmpty())
				return childShapeFailure;
			if (!transition.m_bDeferredPublicationReleased)
			{
				string causalityFailure = ValidateUnreleasedChildCausality(
					saveData,
					transition,
					parent);
				if (!causalityFailure.IsEmpty())
					return causalityFailure;
				if (!parent || parent.m_bQuarantined || parent.m_bCompleted)
					return "unreleased nested ownership publication has no live parent";
			}
			else if (!transition.m_bCompleted)
				return "incomplete nested ownership transition claims released publication";
			else if (!transition.m_bSetupProjectionWithoutMarkers
				&& (transition.m_sMarkerId.IsEmpty() || transition.m_iMarkerRevision <= 0
					|| transition.m_iMarkerStreamSequence <= 0))
				return "released nested ownership publication lacks marker correlation evidence";
		}
		return "";
	}

	protected string ValidateUnreleasedChildSet(
		HST_CampaignSaveData saveData,
		HST_OwnershipTransitionState parent)
	{
		if (!saveData || !parent)
			return "ownership transition parent publication graph is unavailable";
		foreach (HST_OwnershipTransitionState child : saveData.m_aOwnershipTransitions)
		{
			if (!child || child.m_sProjectionParentRequestId != parent.m_sRequestId)
				continue;
			if (child.m_bQuarantined
				|| child.m_iContractVersion != HST_OwnershipTransitionService.EXACT_CONTRACT_VERSION)
				return "ownership transition parent retains quarantined or unsupported child authority";
			if (child.m_bDeferredPublicationReleased)
			{
				string releasedFailure = ValidateNestedChildShape(saveData, child, parent);
				if (!releasedFailure.IsEmpty())
					return "ownership transition parent has invalid released child authority: "
						+ releasedFailure;
				continue;
			}
			if (parent.m_bProjectionRequested)
				return "published ownership parent retains an unreleased direct child";
			string failure = ValidateUnreleasedChildCausality(saveData, child, parent);
			if (!failure.IsEmpty())
				return "ownership transition parent has noncausal unreleased child authority: " + failure;
		}
		return "";
	}

	protected string ValidateUnreleasedChildCausality(
		HST_CampaignSaveData saveData,
		HST_OwnershipTransitionState child,
		HST_OwnershipTransitionState parent)
	{
		if (!saveData || !child || !parent)
			return "unreleased nested ownership transition has no causal parent";
		if (child.m_iContractVersion != HST_OwnershipTransitionService.EXACT_CONTRACT_VERSION
			|| child.m_bQuarantined)
			return "unreleased nested ownership transition child is quarantined or unsupported";
		if (parent.m_iContractVersion != HST_OwnershipTransitionService.EXACT_CONTRACT_VERSION
			|| parent.m_bQuarantined || parent.m_bCompleted
			|| !parent.m_sProjectionParentRequestId.IsEmpty())
			return "unreleased nested ownership transition parent is not live exact top-level authority";
		if (child.m_sCause != "political_support" || child.m_sSourceType != "town_influence")
			return "unreleased nested ownership transition is not a causal political-support child";
		return ValidateCausalChildBinding(saveData, child, parent);
	}

	protected string ValidateNestedChildShape(
		HST_CampaignSaveData saveData,
		HST_OwnershipTransitionState child,
		HST_OwnershipTransitionState parent)
	{
		if (!child || child.m_sCause != "political_support"
			|| child.m_sSourceType != "town_influence")
			return "nested ownership transition is not a political town-influence child";
		if (!parent)
		{
			if (!child.m_bDeferredPublicationReleased)
				return "unreleased nested ownership transition has no retained parent";
			return "";
		}
		if (parent.m_iContractVersion != HST_OwnershipTransitionService.EXACT_CONTRACT_VERSION
			|| parent.m_bQuarantined || !parent.m_sProjectionParentRequestId.IsEmpty())
			return "nested ownership transition retained parent is not exact top-level authority";
		if (child.m_bDeferredPublicationReleased
			&& (!parent.m_bProjectionRequested || !parent.m_bCompleted))
			return "released nested ownership transition has no completed published parent";
		if (child.m_bDeferredPublicationReleased
			&& child.m_bSetupProjectionWithoutMarkers
				!= parent.m_bSetupProjectionWithoutMarkers)
			return "released nested ownership transition disagrees with parent setup publication mode";
		return ValidateCausalChildBinding(saveData, child, parent);
	}

	protected string ValidateCausalChildBinding(
		HST_CampaignSaveData saveData,
		HST_OwnershipTransitionState child,
		HST_OwnershipTransitionState parent)
	{
		if (!TransitionAppearsBefore(saveData, parent, child))
			return "nested ownership transition does not follow its parent in queue order";
		if (parent.m_iSupportReward == 0 || !SupportTargetsZone(parent, child.m_sZoneId)
			|| (child.m_bCompleted
				&& !parent.m_aAppliedSupportZoneIds.Contains(child.m_sZoneId)))
			return "nested ownership transition is outside its parent's applied frozen support policy";
		string expectedEventId = HST_TownInfluenceService.BuildOwnershipRewardEventId(
			child.m_sZoneId,
			parent.m_sRequestId);
		string legacyExpectedEventId = string.Format(
			"town_influence_ownership_%1_%2",
			child.m_sZoneId,
			parent.m_sRequestId.Hash());
		string eventFailure = ValidateAppliedSupportEvent(saveData, parent, child.m_sZoneId);
		if (!eventFailure.IsEmpty())
			return "nested ownership transition parent support event is invalid: " + eventFailure;
		if (child.m_sSourceId != expectedEventId
			&& child.m_sSourceId != legacyExpectedEventId)
			return "nested ownership transition source is not the parent support event";
		string expectedRequestId = HST_TownInfluenceService.BuildPoliticalOwnershipRequestId(
			child.m_sZoneId,
			child.m_iExpectedRevision,
			child.m_sSourceId);
		if (child.m_sSourceId == legacyExpectedEventId)
		{
			expectedRequestId = string.Format(
				"ownership_political_%1_%2_%3",
				child.m_sZoneId,
				child.m_iExpectedRevision,
				child.m_sSourceId.Hash());
		}
		if (child.m_sRequestId != expectedRequestId)
			return "nested ownership transition request identity is not deterministic";
		return ValidatePoliticalThresholdEvidence(saveData, child, parent);
	}

	protected string ValidatePoliticalThresholdEvidence(
		HST_CampaignSaveData saveData,
		HST_OwnershipTransitionState child,
		HST_OwnershipTransitionState parent)
	{
		// Civilian support and heat keep evolving while a parent waits at the
		// publication fence. The immutable influence event, deterministic child
		// identity, target membership, and policy reason are the durable admission
		// evidence; re-reading today's aggregates would quarantine valid retries.
		if (parent.m_iSupportReward > 0
			&& child.m_sReason != "support flipped town to resistance")
			return "nested ownership transition has the wrong resistance policy reason";
		if (parent.m_iSupportReward < 0
			&& child.m_sReason != "support flipped town to enemy")
			return "nested ownership transition has the wrong enemy policy reason";
		return "";
	}

	protected bool TransitionAppearsBefore(
		HST_CampaignSaveData saveData,
		HST_OwnershipTransitionState earlier,
		HST_OwnershipTransitionState later)
	{
		if (!saveData || !earlier || !later)
			return false;
		bool earlierSeen;
		foreach (HST_OwnershipTransitionState candidate : saveData.m_aOwnershipTransitions)
		{
			if (!candidate)
				continue;
			if (candidate == earlier || candidate.m_sRequestId == earlier.m_sRequestId)
				earlierSeen = true;
			if (candidate == later || candidate.m_sRequestId == later.m_sRequestId)
				return earlierSeen;
		}
		return false;
	}

	protected bool SupportTargetsZone(HST_OwnershipTransitionState transition, string zoneId)
	{
		if (!transition || zoneId.IsEmpty())
			return false;
		string canonicalZoneId = HST_MaidensBayLocationSaveValidationService.ResolveCanonicalZoneId(zoneId);
		foreach (string supportZoneId : transition.m_aSupportZoneIds)
		{
			if (HST_MaidensBayLocationSaveValidationService.ResolveCanonicalZoneId(supportZoneId)
				== canonicalZoneId)
				return true;
		}
		return false;
	}

	protected bool HasProjectionParentCycle(
		HST_CampaignSaveData saveData,
		HST_OwnershipTransitionState transition)
	{
		if (!saveData || !transition)
			return false;
		array<string> visitedRequestIds = {};
		HST_OwnershipTransitionState current = transition;
		while (current && !current.m_sProjectionParentRequestId.IsEmpty())
		{
			if (visitedRequestIds.Contains(current.m_sRequestId))
				return true;
			visitedRequestIds.Insert(current.m_sRequestId);
			current = FindTransition(saveData, current.m_sProjectionParentRequestId);
		}
		return false;
	}

	protected string ValidateZoneBacklinks(HST_CampaignSaveData saveData, HST_ZoneState zone)
	{
		if (zone.m_iOwnershipRevision == 1
			&& zone.m_sLastOwnershipTransitionRequestId.IsEmpty()
			&& zone.m_sActiveOwnershipTransitionRequestId.IsEmpty())
			return "";

		if (!zone.m_sActiveOwnershipTransitionRequestId.IsEmpty())
		{
			HST_OwnershipTransitionState active = FindTransition(saveData, zone.m_sActiveOwnershipTransitionRequestId);
			if (!active || active.m_bCompleted || active.m_bQuarantined || active.m_sZoneId != zone.m_sZoneId)
				return "zone active ownership backlink is missing or non-reciprocal";
			if (CountIncompleteTransitionsForZone(saveData, zone.m_sZoneId) != 1)
				return "zone has ambiguous active ownership authority";
			if (active.m_bOwnerApplied)
			{
				if (zone.m_sOwnerFactionKey != active.m_sNewOwnerFactionKey
					|| zone.m_iOwnershipRevision != active.m_iAppliedRevision)
					return "zone state diverges from active ownership result";
			}
			else if (zone.m_sOwnerFactionKey != active.m_sPreviousOwnerFactionKey
				|| zone.m_iOwnershipRevision != active.m_iExpectedRevision)
				return "zone state diverges from active ownership precondition";

			if (active.m_iExpectedRevision == 1)
			{
				if (!zone.m_sLastOwnershipTransitionRequestId.IsEmpty())
					return "baseline ownership transition has an unexpected prior backlink";
			}
			else
			{
				HST_OwnershipTransitionState prior = FindTransition(saveData, zone.m_sLastOwnershipTransitionRequestId);
				if (!prior || !prior.m_bCompleted || prior.m_bQuarantined
					|| prior.m_sZoneId != zone.m_sZoneId
					|| prior.m_iAppliedRevision != active.m_iExpectedRevision
					|| prior.m_sNewOwnerFactionKey != active.m_sPreviousOwnerFactionKey)
					return "active ownership transition has no reciprocal prior revision";
			}
			return "";
		}

		if (CountIncompleteTransitionsForZone(saveData, zone.m_sZoneId) > 0)
			return "zone has an orphan incomplete ownership transition";

		if (zone.m_sLastOwnershipTransitionRequestId.IsEmpty())
			return "post-baseline ownership revision has no receipt backlink";
		HST_OwnershipTransitionState latest = FindTransition(saveData, zone.m_sLastOwnershipTransitionRequestId);
		if (!latest || !latest.m_bCompleted || latest.m_bQuarantined
			|| latest.m_sZoneId != zone.m_sZoneId
			|| latest.m_sNewOwnerFactionKey != zone.m_sOwnerFactionKey
			|| latest.m_iAppliedRevision != zone.m_iOwnershipRevision)
			return "zone latest ownership backlink is missing or non-reciprocal";
		return "";
	}

	protected string ValidateTransitionBacklink(
		HST_CampaignSaveData saveData,
		HST_OwnershipTransitionState transition)
	{
		HST_ZoneState zone = FindZone(saveData, transition.m_sZoneId);
		if (!zone)
			return "ownership transition backlink zone is missing";
		if (!transition.m_bCompleted)
		{
			if (CountIncompleteTransitionsForZone(saveData, transition.m_sZoneId) != 1
				|| zone.m_sActiveOwnershipTransitionRequestId != transition.m_sRequestId)
				return "incomplete ownership transition is not the zone's unique active authority";
			if (transition.m_bOwnerApplied)
			{
				if (zone.m_sOwnerFactionKey != transition.m_sNewOwnerFactionKey
					|| zone.m_iOwnershipRevision != transition.m_iAppliedRevision)
					return "incomplete ownership transition result diverges from its zone";
			}
			else if (zone.m_sOwnerFactionKey != transition.m_sPreviousOwnerFactionKey
				|| zone.m_iOwnershipRevision != transition.m_iExpectedRevision)
				return "incomplete ownership transition precondition diverges from its zone";
			return "";
		}

		if (transition.m_iAppliedRevision <= 0 || transition.m_iAppliedRevision > zone.m_iOwnershipRevision)
			return "completed ownership transition revision exceeds its zone authority";
		if (zone.m_sLastOwnershipTransitionRequestId == transition.m_sRequestId
			&& (zone.m_sOwnerFactionKey != transition.m_sNewOwnerFactionKey
				|| zone.m_iOwnershipRevision != transition.m_iAppliedRevision))
			return "latest completed ownership transition diverges from its zone";
		return "";
	}

	protected bool CompletionOrderIsValid(HST_OwnershipTransitionState transition)
	{
		if (!transition)
			return false;
		if (transition.m_bHostileRuntimeRetired && !transition.m_bOldSecurityRetired)
			return false;
		if (transition.m_bNewSecurityApplied && !transition.m_bHostileRuntimeRetired)
			return false;
		if (transition.m_bSupportApplied && !transition.m_bNewSecurityApplied)
			return false;
		if (transition.m_bOwnerApplied && !transition.m_bSupportApplied)
			return false;
		if (transition.m_bTownPolicyApplied && !transition.m_bOwnerApplied)
			return false;
		if ((transition.m_bFacilitiesApplied || transition.m_bLogisticsApplied)
			&& !transition.m_bTownPolicyApplied)
			return false;
		if (transition.m_bFacilitiesApplied != transition.m_bLogisticsApplied)
			return false;
		if (transition.m_bEnemyConsequencesApplied
			&& (!transition.m_bFacilitiesApplied || !transition.m_bLogisticsApplied))
			return false;
		if (transition.m_bEconomyApplied && !transition.m_bEnemyConsequencesApplied)
			return false;
		if (transition.m_bStrategicEventCompleted && !transition.m_bEconomyApplied)
			return false;
		if (transition.m_bEventAppended && !transition.m_bStrategicEventCompleted)
			return false;
		if (transition.m_bProjectionRequested && !transition.m_bEventAppended)
			return false;
		if (transition.m_bNotificationApplied && !transition.m_bProjectionRequested)
			return false;
		if (transition.m_bPersistenceRequested && !transition.m_bNotificationApplied)
			return false;
		if (transition.m_bCompleted && !transition.m_bPersistenceRequested)
			return false;
		return true;
	}

	protected bool AllCompletionStepsApplied(HST_OwnershipTransitionState transition)
	{
		if (!transition.m_bValidated || !transition.m_bOwnerApplied)
			return false;
		if (!transition.m_bTownPolicyApplied || !transition.m_bOldSecurityRetired)
			return false;
		if (!transition.m_bHostileRuntimeRetired || !transition.m_bNewSecurityApplied)
			return false;
		if (!transition.m_bSupportApplied || !transition.m_bFacilitiesApplied)
			return false;
		if (!transition.m_bLogisticsApplied || !transition.m_bEconomyApplied)
			return false;
		if (!transition.m_bEnemyConsequencesApplied || !transition.m_bStrategicEventCompleted)
			return false;
		if (!transition.m_bEventAppended || !transition.m_bNotificationApplied)
			return false;
		if (!transition.m_bProjectionRequested || !transition.m_bPersistenceRequested)
			return false;
		return true;
	}

	protected void QuarantineTransition(
		HST_CampaignSaveData saveData,
		HST_OwnershipTransitionState transition,
		string failure)
	{
		transition.m_iContractVersion = HST_OwnershipTransitionService.QUARANTINED_CONTRACT_VERSION;
		transition.m_sStatus = "quarantined";
		transition.m_bQuarantined = true;
		transition.m_sFailureReason = failure;
		if (!transition.m_bCompleted)
			PurgeZoneMarkerRows(saveData, transition.m_sZoneId);
		HST_ZoneState zone = FindZone(saveData, transition.m_sZoneId);
		if (zone && (!transition.m_bCompleted
			|| zone.m_sActiveOwnershipTransitionRequestId == transition.m_sRequestId
			|| zone.m_sLastOwnershipTransitionRequestId == transition.m_sRequestId))
			QuarantineZone(saveData, zone, failure);
	}

	protected void QuarantineZone(
		HST_CampaignSaveData saveData,
		HST_ZoneState zone,
		string failure)
	{
		if (!zone)
			return;
		zone.m_iOwnershipContractVersion = HST_OwnershipTransitionService.QUARANTINED_CONTRACT_VERSION;
		zone.m_sOwnershipAuthorityFailure = failure;
		PurgeZoneMarkerRows(saveData, zone.m_sZoneId);
	}

	protected void PurgeZoneMarkerRows(HST_CampaignSaveData saveData, string zoneId)
	{
		if (!saveData || zoneId.IsEmpty())
			return;
		string markerId = "hst_zone_"
			+ HST_MaidensBayLocationSaveValidationService.ResolveCanonicalZoneId(zoneId);
		for (int markerIndex = saveData.m_aMapMarkers.Count() - 1; markerIndex >= 0; markerIndex--)
		{
			HST_MapMarkerState marker = saveData.m_aMapMarkers[markerIndex];
			if (marker && marker.m_sMarkerId == markerId)
				saveData.m_aMapMarkers.Remove(markerIndex);
		}
	}

	protected HST_TownInfluenceRecord FindUniqueTownInfluenceRecord(
		HST_CampaignSaveData saveData,
		string townId)
	{
		if (!saveData || townId.IsEmpty())
			return null;
		HST_TownInfluenceRecord match;
		foreach (HST_TownInfluenceRecord record : saveData.m_aTownInfluenceRecords)
		{
			if (!record || record.m_sTownId != townId)
				continue;
			if (match)
				return null;
			match = record;
		}
		return match;
	}

	protected HST_ZoneState FindZone(HST_CampaignSaveData saveData, string zoneId)
	{
		if (!saveData || zoneId.IsEmpty())
			return null;
		string canonicalZoneId = HST_MaidensBayLocationSaveValidationService.ResolveCanonicalZoneId(zoneId);
		foreach (HST_ZoneState zone : saveData.m_aZones)
		{
			if (zone && zone.m_sZoneId == canonicalZoneId)
				return zone;
		}
		return null;
	}

	protected HST_CivilianZoneState FindCivilianZone(HST_CampaignSaveData saveData, string zoneId)
	{
		if (!saveData || zoneId.IsEmpty())
			return null;
		string canonicalZoneId = HST_MaidensBayLocationSaveValidationService.ResolveCanonicalZoneId(zoneId);
		foreach (HST_CivilianZoneState civilianZone : saveData.m_aCivilianZones)
		{
			if (civilianZone && civilianZone.m_sZoneId == canonicalZoneId)
				return civilianZone;
		}
		return null;
	}

	protected HST_FactionPoolState FindFactionPool(HST_CampaignSaveData saveData, string factionKey)
	{
		if (!saveData || factionKey.IsEmpty())
			return null;
		foreach (HST_FactionPoolState pool : saveData.m_aFactionPools)
		{
			if (pool && pool.m_sFactionKey == factionKey)
				return pool;
		}
		return null;
	}

	protected HST_StrategicEventState FindStrategicEvent(HST_CampaignSaveData saveData, string eventId)
	{
		if (!saveData || eventId.IsEmpty())
			return null;
		foreach (HST_StrategicEventState strategicEvent : saveData.m_aStrategicEvents)
		{
			if (strategicEvent && strategicEvent.m_sEventId == eventId)
				return strategicEvent;
		}
		return null;
	}

	protected int CountZoneIdentity(HST_CampaignSaveData saveData, string zoneId)
	{
		if (!saveData || zoneId.IsEmpty())
			return 0;
		string canonicalZoneId = HST_MaidensBayLocationSaveValidationService.ResolveCanonicalZoneId(zoneId);
		int count;
		foreach (HST_ZoneState zone : saveData.m_aZones)
		{
			if (zone && HST_MaidensBayLocationSaveValidationService.ResolveCanonicalZoneId(zone.m_sZoneId) == canonicalZoneId)
				count++;
		}
		return count;
	}

	protected int CountTransitionIdentity(HST_CampaignSaveData saveData, string requestId)
	{
		if (!saveData || requestId.IsEmpty())
			return 0;
		int count;
		foreach (HST_OwnershipTransitionState transition : saveData.m_aOwnershipTransitions)
		{
			if (transition && transition.m_sRequestId == requestId)
				count++;
		}
		return count;
	}

	protected int CountIncompleteTransitionsForZone(HST_CampaignSaveData saveData, string zoneId)
	{
		if (!saveData || zoneId.IsEmpty())
			return 0;
		int count;
		foreach (HST_OwnershipTransitionState transition : saveData.m_aOwnershipTransitions)
		{
			if (!transition || transition.m_bCompleted || transition.m_bQuarantined
				|| transition.m_iContractVersion != HST_OwnershipTransitionService.EXACT_CONTRACT_VERSION)
				continue;
			if (transition.m_sZoneId == zoneId)
				count++;
		}
		return count;
	}

	protected int ValidateQueuedTopLevelOrder(HST_CampaignSaveData saveData)
	{
		if (!saveData)
			return 0;
		int conflicts;
		bool earlierUnresolvedTopLevel;
		foreach (HST_OwnershipTransitionState transition : saveData.m_aOwnershipTransitions)
		{
			if (!transition || transition.m_bCompleted
				|| !transition.m_sProjectionParentRequestId.IsEmpty())
				continue;
			if (!earlierUnresolvedTopLevel)
			{
				earlierUnresolvedTopLevel = true;
				continue;
			}
			if (transition.m_bQuarantined || IsPristineQueuedTopLevelTransition(transition))
				continue;
			QuarantineTransition(
				saveData,
				transition,
				"later unresolved top-level transition is not pristine queued authority");
			conflicts++;
		}
		return conflicts;
	}

	protected bool IsPristineQueuedTopLevelTransition(HST_OwnershipTransitionState transition)
	{
		if (!transition || transition.m_bCompleted || transition.m_bQuarantined
			|| transition.m_iContractVersion != HST_OwnershipTransitionService.EXACT_CONTRACT_VERSION)
			return false;
		if (!transition.m_bValidated || !transition.m_sProjectionParentRequestId.IsEmpty()
			|| transition.m_bOwnerApplied || transition.m_iAppliedRevision != 0)
			return false;
		if (transition.m_bOldSecurityRetired || transition.m_bHostileRuntimeRetired
			|| transition.m_bNewSecurityApplied || transition.m_bSupportApplied
			|| transition.m_bTownPolicyApplied || transition.m_bFacilitiesApplied
			|| transition.m_bLogisticsApplied || transition.m_bEconomyApplied
			|| transition.m_bEnemyConsequencesApplied || transition.m_bStrategicEventCompleted
			|| transition.m_bEventAppended || transition.m_bProjectionRequested
			|| transition.m_bNotificationApplied || transition.m_bDeferredPublicationReleased
			|| transition.m_bSetupProjectionWithoutMarkers
			|| transition.m_bPersistenceRequested)
			return false;
		if (!transition.m_sCampaignEventId.IsEmpty() || !transition.m_sOldGarrisonId.IsEmpty()
			|| !transition.m_sNewGarrisonId.IsEmpty() || !transition.m_sSecurityDecision.IsEmpty()
			|| !transition.m_sFacilityLogisticsDecision.IsEmpty()
			|| !transition.m_sEnemyConsequenceDecision.IsEmpty()
			|| !transition.m_sEnemyOrderId.IsEmpty() || !transition.m_sProjectionDecision.IsEmpty()
			|| !transition.m_sMarkerId.IsEmpty())
			return false;
		if (transition.m_iMarkerProjectionEpoch != 0 || transition.m_iMarkerRevision != 0
			|| transition.m_iMarkerStreamSequence != 0 || transition.m_iAggressionApplied != 0
			|| transition.m_bCounterattackQueued || !transition.m_aAppliedSupportZoneIds.IsEmpty()
			|| transition.m_iCompletedAtSecond != 0)
			return false;
		return true;
	}

	protected int ValidateCompletedHistoryOrder(HST_CampaignSaveData saveData)
	{
		if (!saveData)
			return 0;
		int conflicts;
		map<string, ref HST_OwnershipTransitionState> previousByZone = new map<string, ref HST_OwnershipTransitionState>();
		foreach (HST_OwnershipTransitionState transition : saveData.m_aOwnershipTransitions)
		{
			if (!transition || !transition.m_bCompleted || transition.m_bQuarantined
				|| transition.m_iContractVersion != HST_OwnershipTransitionService.EXACT_CONTRACT_VERSION
				|| transition.m_iAppliedRevision <= 0)
				continue;
			string canonicalZoneId = HST_MaidensBayLocationSaveValidationService.ResolveCanonicalZoneId(
				transition.m_sZoneId);
			if (canonicalZoneId.IsEmpty())
				continue;
			HST_OwnershipTransitionState previous = previousByZone.Get(canonicalZoneId);
			string failure;
			if (previous && transition.m_iAppliedRevision <= previous.m_iAppliedRevision)
				failure = "completed ownership history revision is duplicate or non-increasing";
			else if (previous
				&& transition.m_iAppliedRevision == previous.m_iAppliedRevision + 1
				&& (transition.m_iExpectedRevision != previous.m_iAppliedRevision
					|| transition.m_sPreviousOwnerFactionKey != previous.m_sNewOwnerFactionKey
					|| transition.m_sExpectedOwnerFactionKey != previous.m_sNewOwnerFactionKey))
				failure = "consecutive completed ownership history does not chain prior owner authority";
			if (!failure.IsEmpty())
			{
				QuarantineTransition(saveData, transition, failure);
				HST_ZoneState zone = FindZone(saveData, transition.m_sZoneId);
				if (zone)
					QuarantineZone(saveData, zone, failure);
				conflicts++;
				continue;
			}
			previousByZone.Set(canonicalZoneId, transition);
		}
		return conflicts;
	}

	protected int CountOwnerAppliedIncompleteTopLevelTransitions(HST_CampaignSaveData saveData)
	{
		if (!saveData)
			return 0;
		int count;
		foreach (HST_OwnershipTransitionState transition : saveData.m_aOwnershipTransitions)
		{
			if (!transition || transition.m_bCompleted || transition.m_bQuarantined
				|| !transition.m_bOwnerApplied
				|| !transition.m_sProjectionParentRequestId.IsEmpty())
				continue;
			count++;
		}
		return count;
	}

	protected bool HasEarlierUnresolvedTopLevelTransition(
		HST_CampaignSaveData saveData,
		HST_OwnershipTransitionState transition)
	{
		if (!saveData || !transition || !transition.m_sProjectionParentRequestId.IsEmpty())
			return false;
		foreach (HST_OwnershipTransitionState candidate : saveData.m_aOwnershipTransitions)
		{
			if (candidate == transition || (candidate
				&& candidate.m_sRequestId == transition.m_sRequestId))
				return false;
			if (!candidate || candidate.m_bCompleted
				|| !candidate.m_sProjectionParentRequestId.IsEmpty())
				continue;
			return true;
		}
		return false;
	}

	protected HST_OwnershipTransitionState FindTransition(HST_CampaignSaveData saveData, string requestId)
	{
		if (!saveData || requestId.IsEmpty())
			return null;
		foreach (HST_OwnershipTransitionState transition : saveData.m_aOwnershipTransitions)
		{
			if (transition && transition.m_sRequestId == requestId)
				return transition;
		}
		return null;
	}

	protected bool IsKnownCause(string cause)
	{
		return cause == "military_capture"
			|| cause == "mission_capture"
			|| cause == "political_support"
			|| cause == "admin"
			|| cause == "debug_seed"
			|| cause == "migration_repair";
	}

	protected float DistanceSq2D(vector a, vector b)
	{
		float dx = a[0] - b[0];
		float dz = a[2] - b[2];
		return dx * dx + dz * dz;
	}

	protected void RecordEvent(
		HST_CampaignSaveData saveData,
		string eventId,
		string reason,
		int affectedRows)
	{
		if (!saveData || HasEvent(saveData, eventId))
			return;
		HST_CampaignEventState eventState = new HST_CampaignEventState();
		eventState.m_sEventId = eventId;
		eventState.m_sCategory = "migration";
		eventState.m_sAggregateType = "ownership_transition";
		eventState.m_sAggregateId = "schema62";
		eventState.m_sTransition = "normalized";
		eventState.m_sReason = string.Format("%1 | affected rows %2", reason, affectedRows);
		eventState.m_iCreatedAtSecond = saveData.m_iElapsedSeconds;
		saveData.m_aCampaignEvents.Insert(eventState);
	}

	protected bool HasEvent(HST_CampaignSaveData saveData, string eventId)
	{
		foreach (HST_CampaignEventState eventState : saveData.m_aCampaignEvents)
		{
			if (eventState && eventState.m_sEventId == eventId)
				return true;
		}
		return false;
	}
}
