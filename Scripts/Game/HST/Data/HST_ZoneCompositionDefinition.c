[BaseContainerProps()]
class HST_ZoneCompositionPropEntry
{
	string m_sKind;
	string m_sPrefab;
	int m_iCount = 1;
	float m_fRadiusMeters;
	bool m_bOptional = true;
}

[BaseContainerProps()]
class HST_ZoneCompositionDefinition
{
	string m_sCompositionId;
	string m_sSpawnProfileId;
	int m_iInfantrySlotCount;
	int m_iVehicleSlotCount;
	int m_iPropSlotCount;
	int m_iPatrolPointCount;
	ref array<ref HST_ZoneCompositionPropEntry> m_aProps = {};
}

[BaseContainerProps()]
class HST_ZoneSpawnSlotState
{
	string m_sZoneId;
	string m_sSlotId;
	string m_sKind;
	vector m_vPosition;
	vector m_vAngles;
	bool m_bOccupied;
}
