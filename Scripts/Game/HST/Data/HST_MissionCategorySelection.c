[BaseContainerProps()]
class HST_MissionCategorySelectionResult
{
	bool m_bSuccess;
	string m_sFailureReason;
	string m_sCategoryKey;
	string m_sCategoryLabel;
	string m_sMissionId;
	string m_sMissionName;
	string m_sZoneId;
	string m_sZoneLabel;
	int m_iCandidateCount;
	int m_iRadiusMeters;
	int m_iDistanceMeters;
	vector m_vOrigin;
	string m_sDebugSummary;
}
