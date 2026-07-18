#ifdef ENABLE_DIAG
// Diagnostic-only engine-process coverage for the deterministic planning report.
// Full Campaign Debug, world integration, persistence, and networking remain
// separate validation gates.
class HST_EnemyPlanningCommitmentAutotestSuite : SCR_AutotestSuiteBase
{
	// This deterministic report is service-only. Keep the already loaded
	// packaged project active instead of entering the base-only world transition.
	override ResourceName GetWorldFile()
	{
		return "";
	}
}

[Test(suite: HST_EnemyPlanningCommitmentAutotestSuite)]
class HST_TEST_EnemyPlanningCommitmentAuthority : SCR_AutotestCaseBase
{
	[Step(EStage.Main)]
	bool Execute()
	{
		HST_EnemyPlanningProofService proof
			= new HST_EnemyPlanningProofService();
		HST_EnemyPlanningProofReport report = proof.BuildAuthorityReport();
		if (!report)
		{
			SetResultFailure("Enemy planning proof did not return a report");
			return true;
		}

		Print("Partisan planning autotest | "
			+ HST_BuildInfo.BuildRuntimeSummary());
		Print(report.BuildReport());
		Print(report.m_sBaselineCadenceEvidence);
		Print(report.m_sDecisionEvidence);
		Print(report.m_sCommitmentSelectionEvidence);
		Print(report.m_sFreezeRetryEvidence);
		Print(report.m_sRecoveryEvidence);
		Print(report.m_sPersistenceQuarantineEvidence);
		Print(report.m_sBootstrapThrottleEvidence);

		AssertTrue(
			report.m_bCommitmentAwareSelectionExact,
			"Commitment-aware selection failed: "
				+ report.m_sCommitmentSelectionEvidence);
		AssertTrue(
			report.m_bAllCommittedSkipExact,
			"All-committed skip failed: " + report.m_sDecisionEvidence);
		AssertTrue(
			report.m_bCommitmentRaceRejectionExact,
			"Commitment race rejection failed: "
				+ report.m_sDecisionEvidence);
		AssertTrue(
			report.AllExact(),
			"Full enemy planning authority proof failed: "
				+ report.BuildReport());
		SetResultSuccess();
		return true;
	}
}
#endif
