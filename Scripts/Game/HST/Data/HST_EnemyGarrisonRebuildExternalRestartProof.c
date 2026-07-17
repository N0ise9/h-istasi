// Disposable, profile-local exact enemy-garrison-rebuild restart proof
// artifacts. These DTOs contain no machine paths or live service references.
[BaseContainerProps()]
class HST_EnemyGarrisonRebuildExternalRestartExpectation
{
	string m_sOrderId;
	string m_sOperationId;
	string m_sManifestId;
	string m_sManifestHash;
	string m_sBatchId;
	string m_sGroupId;
	string m_sProjectionId;
	string m_sForceId;
	string m_sFactionKey;
	string m_sSourceZoneId;
	string m_sTargetZoneId;
	string m_sExpectedSourceOwnerFactionKey;
	int m_iExpectedSourceOwnershipRevision;
	string m_sExpectedTargetOwnerFactionKey;
	int m_iExpectedTargetOwnershipRevision;
	string m_sDebitMutationId;
	string m_sDeliverySettlementKind;
	string m_sDeliverySettlementId;
	string m_sRefundMutationId;
	int m_iAttackCost;
	int m_iSupportCost;
	int m_iExpectedAttackPool;
	int m_iExpectedSupportPool;
	int m_iExpectedPendingPoolRevision;
	int m_iExpectedPendingPoolOperationalMutationCount;
	int m_iAcceptedMemberCount;
	int m_iLivingMemberCount;
	string m_sLivingSlotFingerprint;
	string m_sConfirmedCasualtySlotId;
	string m_sCasualtyTombstoneFingerprint;
	int m_iExpectedAggregateInfantry;
	int m_iExpectedAuthoritativePendingInfantry;
	int m_iExpectedAuthoritativeDeliveredInfantry;
}

[BaseContainerProps()]
class HST_EnemyGarrisonRebuildExternalRestartOwner
{
	string m_sMagic;
	int m_iVersion;
	string m_sPurpose;
	string m_sSessionNonce;
	string m_sRunId;
	string m_sRequestedCut;
	string m_sBuildSha;
	string m_sBuildUtc;
	string m_sBuildLabel;
	int m_iCampaignSchemaVersion;
	int m_iSettingsSchemaVersion;
	string m_sWorld;
	bool m_bDisposableProfile;
}

[BaseContainerProps()]
class HST_EnemyGarrisonRebuildExternalRestartGuard
{
	string m_sMagic;
	int m_iVersion;
	string m_sSessionNonce;
	string m_sStageNonce;
	string m_sRunId;
	string m_sRequestedCut;
	string m_sRequestedStage;
	int m_iStageOrdinal;
	string m_sBuildSha;
	string m_sBuildUtc;
	string m_sBuildLabel;
	int m_iCampaignSchemaVersion;
	int m_iSettingsSchemaVersion;
	string m_sWorld;
	bool m_bAllowCanonicalCampaignOverwrite;
}

[BaseContainerProps()]
class HST_EnemyGarrisonRebuildExternalRestartCarrier
{
	string m_sMagic;
	string m_sSessionNonce;
	string m_sRunId;
	string m_sBuildSha;
	string m_sBuildUtc;
	string m_sBuildLabel;
	int m_iCampaignSchemaVersion;
	int m_iSettingsSchemaVersion;
	string m_sWorld;
	string m_sCutName;
	int m_iCut;

	ref HST_EnemyGarrisonRebuildExternalRestartExpectation m_Expectation;
	int m_iPreparedElapsedSecond;
	float m_fPreparedRouteProgressMeters;
	float m_fPreparedRouteTotalDistanceMeters;
	vector m_vPreparedStrategicPosition;
	int m_iExpectedPhysicalAdapterHandleCount;
	int m_iExpectedPhysicalRuntimeMemberCount;
	string m_sPreparedSemanticFingerprint;
}

[BaseContainerProps()]
class HST_EnemyGarrisonRebuildExternalRestartResult
{
	string m_sMagic;
	string m_sSessionNonce;
	string m_sStageNonce;
	string m_sRunId;
	string m_sStage;
	bool m_bSuccess;
	string m_sBuildSha;
	string m_sBuildUtc;
	string m_sBuildLabel;
	int m_iCampaignSchemaVersion;
	int m_iSettingsSchemaVersion;
	string m_sWorld;
	string m_sCutName;
	int m_iCut;
	bool m_bRestored;
	bool m_bStartupReconcileChanged;
	bool m_bSourceExact;
	bool m_bContinuationExact;
	bool m_bSameStateSemanticNoOp;
	bool m_bRuntimeClaimantsZero;
	bool m_bPersistedReadBackExact;
	bool m_bPreparedCutExact;
	bool m_bCasualtyContinuityExact;
	bool m_bDeliveryReceiptExact;
	bool m_bHeldGarrisonExact;
	bool m_bAggregateNotDoubleCounted;
	bool m_bResourceExactlyOnce;
	int m_iPhysicalAdapterHandleCount;
	int m_iPhysicalRuntimeMemberCount;
	float m_fProgressBeforeMeters;
	float m_fProgressAfterMeters;
	string m_sSourceSemanticFingerprint;
	string m_sFinalSemanticFingerprint;
	string m_sEvidence;
}
