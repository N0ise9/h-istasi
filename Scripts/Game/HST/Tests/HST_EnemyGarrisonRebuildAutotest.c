#ifdef ENABLE_DIAG
// Diagnostic-only engine-process coverage for the deterministic Schema-70
// exact enemy garrison-rebuild report. Native entities, Full Campaign Debug,
// profile persistence, package/restart, networking, and soak remain separate.
class HST_EnemyGarrisonRebuildAutotestSuite : SCR_AutotestSuiteBase
{
}

[Test(suite: HST_EnemyGarrisonRebuildAutotestSuite)]
class HST_TEST_EnemyGarrisonRebuildAuthority : SCR_AutotestCaseBase
{
	[Step(EStage.Main)]
	bool Execute()
	{
		HST_EnemyGarrisonRebuildOperationProofService proof
			= new HST_EnemyGarrisonRebuildOperationProofService();
		HST_EnemyGarrisonRebuildOperationProofReport report = proof.Run();
		if (!report)
		{
			SetResultFailure(
				"Exact enemy garrison-rebuild proof did not return a report");
			return true;
		}

		Print("Partisan enemy garrison-rebuild autotest | "
			+ HST_BuildInfo.BuildRuntimeSummary());
		Print(report.BuildReport());
		Print(report.m_sAdmissionEvidence);
		Print(report.m_sDeliveryEvidence);
		Print(report.m_sCasualtyEvidence);
		Print(report.m_sRestoreEvidence);
		Print(report.m_sOwnershipEvidence);
		Print(report.m_sAdmissionRollbackEvidence);
		Print(report.m_sPrearrivalRefundEvidence);
		Print(report.m_sSettlementCrashEvidence);
		Print(report.m_sHistoricalEvidence);
		Print(report.m_sQuarantineEvidence);
		Print(report.m_sOrphanRuntimeEvidence);
		Print(report.m_sRetentionEvidence);
		Print(report.m_sSelectedOwnershipABAEvidence);

		AssertTrue(
			report.m_bAdmissionCapacityExact,
			"Garrison-rebuild admission/capacity proof failed: "
				+ report.m_sAdmissionEvidence);
		AssertTrue(
			report.m_bDeliveryHeldExact,
			"Garrison-rebuild delivery/held proof failed: "
				+ report.m_sDeliveryEvidence);
		AssertTrue(
			report.m_bCasualtyContinuityExact,
			"Garrison-rebuild casualty-continuity proof failed: "
				+ report.m_sCasualtyEvidence);
		AssertTrue(
			report.m_bRestoreExact,
			"Garrison-rebuild restore proof failed: "
				+ report.m_sRestoreEvidence);
		AssertTrue(
			report.m_bOwnershipTerminalExact,
			"Garrison-rebuild ownership-terminal proof failed: "
				+ report.m_sOwnershipEvidence);
		AssertTrue(
			report.m_bAdmissionRollbackExact,
			"Garrison-rebuild admission rollback proof failed: "
				+ report.m_sAdmissionRollbackEvidence);
		AssertTrue(
			report.m_bPrearrivalRefundExact,
			"Garrison-rebuild prearrival refund proof failed: "
				+ report.m_sPrearrivalRefundEvidence);
		AssertTrue(
			report.m_bSettlementCrashResumeExact,
			"Garrison-rebuild settlement crash-resume proof failed: "
				+ report.m_sSettlementCrashEvidence);
		AssertTrue(
			report.m_bHistoricalIsolationExact,
			"Garrison-rebuild historical isolation proof failed: "
				+ report.m_sHistoricalEvidence);
		AssertTrue(
			report.m_bSchema70QuarantineExact,
			"Garrison-rebuild Schema-70 quarantine proof failed: "
				+ report.m_sQuarantineEvidence);
		AssertTrue(
			report.m_bOrphanRuntimeQuarantineExact,
			"Garrison-rebuild orphan runtime quarantine proof failed: "
				+ report.m_sOrphanRuntimeEvidence);
		AssertTrue(
			report.m_bQuarantineRetentionExact,
			"Garrison-rebuild quarantine-retention proof failed: "
				+ report.m_sRetentionEvidence);
		AssertTrue(
			report.m_bSelectedOwnershipABAExact,
			"Garrison-rebuild selected ownership ABA proof failed: "
				+ report.m_sSelectedOwnershipABAEvidence);
		AssertTrue(
			report.AllExact(),
			"Full exact enemy garrison-rebuild proof failed: "
				+ report.BuildReport());
		SetResultSuccess();
		return true;
	}
}
#endif
