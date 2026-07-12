[BaseContainerProps()]
class HST_ZonePressureProjectionRow
{
	string m_sTownId;
	string m_sDisplayName;
	string m_sOwnerFactionKey;
	string m_sHysteresisBand;
	int m_iFIASupportBasisPoints;
	int m_iFIASupportPercent;
	int m_iEnemySupportPercent;
	int m_iSignedSupportPercent;
	int m_iInfluenceRevision;
	bool m_bCurrentTown;
}

[BaseContainerProps()]
class HST_ResistanceTerritoryProjectionRow
{
	string m_sZoneId;
	string m_sDisplayName;
	string m_sOwnerFactionKey;
	HST_EZoneType m_eZoneType;
	int m_iOwnershipRevision;
	int m_iTownInfluenceRevision;
}

// Pure state projection for the Map/War tab. It never scans the world and it
// never mutates discovery, ownership, or political support while rendering.
class HST_MapWarProjectionService
{
	static const float CURRENT_TOWN_MIN_RADIUS_METERS = 250.0;

	protected ref HST_TownInfluenceService m_TownInfluence = new HST_TownInfluenceService();
	protected ref HST_MapMarkerService m_MapMarkers = new HST_MapMarkerService();

	void SetTownInfluenceService(HST_TownInfluenceService townInfluence)
	{
		if (townInfluence)
			m_TownInfluence = townInfluence;
	}

	void SetMapMarkerService(HST_MapMarkerService mapMarkers)
	{
		if (mapMarkers)
			m_MapMarkers = mapMarkers;
	}

	array<ref HST_ZonePressureProjectionRow> BuildZonePressure(
		HST_CampaignState state,
		vector playerPosition)
	{
		array<ref HST_ZonePressureProjectionRow> rows = {};
		if (!state || !m_TownInfluence)
			return rows;

		string currentTownId = ResolveCurrentTownId(state, playerPosition);
		foreach (HST_ZoneState zone : state.m_aZones)
		{
			if (!zone || zone.m_eType != HST_EZoneType.HST_ZONE_TOWN)
				continue;
			if (!IsUniqueZoneAuthority(state, zone))
				continue;

			HST_TownInfluenceRecord record = m_TownInfluence.FindValidRecord(state, zone.m_sZoneId);
			if (!record || !record.m_bContacted)
				continue;
			string publishedOwnerFactionKey;
			int publishedOwnershipRevision;
			if (!m_MapMarkers || !m_MapMarkers.ResolvePublishedZoneOwnership(
					state,
					zone,
					publishedOwnerFactionKey,
					publishedOwnershipRevision))
				continue;

			HST_ZonePressureProjectionRow row = new HST_ZonePressureProjectionRow();
			row.m_sTownId = zone.m_sZoneId;
			row.m_sDisplayName = ResolveDisplayName(zone);
			row.m_sOwnerFactionKey = publishedOwnerFactionKey;
			row.m_sHysteresisBand = record.m_sHysteresisBand;
			row.m_iFIASupportBasisPoints = Math.Max(0, Math.Min(10000, record.m_iFIASupportBasisPoints));
			row.m_iFIASupportPercent = BasisPointsToPercent(record.m_iFIASupportBasisPoints);
			row.m_iEnemySupportPercent = BasisPointsToPercent(Math.Max(
				record.m_iOccupierSupportBasisPoints,
				record.m_iInvaderSupportBasisPoints));
			row.m_iSignedSupportPercent = m_TownInfluence.ResolveSignedSupportPercent(
				state,
				zone.m_sZoneId);
			row.m_iInfluenceRevision = record.m_iRevision;
			row.m_bCurrentTown = zone.m_sZoneId == currentTownId;
			rows.Insert(row);
		}

		SortZonePressure(rows);
		return rows;
	}

	array<ref HST_ResistanceTerritoryProjectionRow> BuildResistanceTerritory(
		HST_CampaignState state,
		string resistanceFactionKey)
	{
		array<ref HST_ResistanceTerritoryProjectionRow> rows = {};
		if (!state || resistanceFactionKey.IsEmpty())
			return rows;

		foreach (HST_ZoneState zone : state.m_aZones)
		{
			if (!zone || zone.m_sZoneId.IsEmpty()
				|| zone.m_eType == HST_EZoneType.HST_ZONE_MISSION_SITE)
				continue;
			if (!IsUniqueZoneAuthority(state, zone))
				continue;
			string publishedOwnerFactionKey;
			int publishedOwnershipRevision;
			if (!m_MapMarkers || !m_MapMarkers.ResolvePublishedZoneOwnership(
					state,
					zone,
					publishedOwnerFactionKey,
					publishedOwnershipRevision))
				continue;
			if (publishedOwnerFactionKey != resistanceFactionKey)
				continue;
			if (publishedOwnershipRevision <= 0)
				continue;

			HST_ResistanceTerritoryProjectionRow row = new HST_ResistanceTerritoryProjectionRow();
			row.m_sZoneId = zone.m_sZoneId;
			row.m_sDisplayName = ResolveDisplayName(zone);
			row.m_sOwnerFactionKey = publishedOwnerFactionKey;
			row.m_eZoneType = zone.m_eType;
			row.m_iOwnershipRevision = publishedOwnershipRevision;
			if (zone.m_eType == HST_EZoneType.HST_ZONE_TOWN && m_TownInfluence)
			{
				HST_TownInfluenceRecord record = m_TownInfluence.FindValidRecord(state, zone.m_sZoneId);
				if (record)
					row.m_iTownInfluenceRevision = record.m_iRevision;
			}
			rows.Insert(row);
		}

		SortResistanceTerritory(rows);
		return rows;
	}

	protected string ResolveCurrentTownId(HST_CampaignState state, vector playerPosition)
	{
		if (!state || IsZeroVector(playerPosition) || !m_TownInfluence)
			return "";

		string currentTownId;
		string currentDisplayName;
		float currentNormalizedDistance = 999999999.0;
		float currentDistanceSq = 999999999.0;
		foreach (HST_ZoneState zone : state.m_aZones)
		{
			if (!zone || zone.m_eType != HST_EZoneType.HST_ZONE_TOWN)
				continue;
			if (!IsUniqueZoneAuthority(state, zone))
				continue;
			HST_TownInfluenceRecord record = m_TownInfluence.FindValidRecord(state, zone.m_sZoneId);
			if (!record || !record.m_bContacted)
				continue;

			float radius = Math.Max(CURRENT_TOWN_MIN_RADIUS_METERS, zone.m_iCaptureRadiusMeters);
			float distanceSq = DistanceSq2D(playerPosition, zone.m_vPosition);
			if (distanceSq > radius * radius)
				continue;
			float normalizedDistance = distanceSq / (radius * radius);
			if (normalizedDistance > currentNormalizedDistance)
				continue;
			if (Math.AbsFloat(normalizedDistance - currentNormalizedDistance) < 0.0001)
			{
				if (distanceSq > currentDistanceSq)
					continue;
				if (Math.AbsFloat(distanceSq - currentDistanceSq) < 0.01 && !currentTownId.IsEmpty())
				{
					string candidateDisplayName = ResolveDisplayName(zone);
					string normalizedCandidateName = candidateDisplayName;
					string normalizedCurrentName = currentDisplayName;
					normalizedCandidateName.ToLower();
					normalizedCurrentName.ToLower();
					int displayCompare = normalizedCandidateName.Compare(normalizedCurrentName);
					if (displayCompare > 0
						|| (displayCompare == 0 && zone.m_sZoneId.Compare(currentTownId) >= 0))
						continue;
				}
			}

			currentTownId = zone.m_sZoneId;
			currentDisplayName = ResolveDisplayName(zone);
			currentNormalizedDistance = normalizedDistance;
			currentDistanceSq = distanceSq;
		}
		return currentTownId;
	}

	protected void SortZonePressure(notnull array<ref HST_ZonePressureProjectionRow> rows)
	{
		for (int rowIndex = 1; rowIndex < rows.Count(); rowIndex++)
		{
			HST_ZonePressureProjectionRow selected = rows[rowIndex];
			int insertionIndex = rowIndex - 1;
			while (insertionIndex >= 0 && CompareZonePressure(selected, rows[insertionIndex]) < 0)
			{
				rows[insertionIndex + 1] = rows[insertionIndex];
				insertionIndex--;
			}
			rows[insertionIndex + 1] = selected;
		}
	}

	protected int CompareZonePressure(HST_ZonePressureProjectionRow left, HST_ZonePressureProjectionRow right)
	{
		if (!left && !right)
			return 0;
		if (!left)
			return 1;
		if (!right)
			return -1;
		if (left.m_bCurrentTown != right.m_bCurrentTown)
		{
			if (left.m_bCurrentTown)
				return -1;
			return 1;
		}
		if (left.m_iFIASupportBasisPoints != right.m_iFIASupportBasisPoints)
			return left.m_iFIASupportBasisPoints - right.m_iFIASupportBasisPoints;

		string leftName = left.m_sDisplayName;
		string rightName = right.m_sDisplayName;
		leftName.ToLower();
		rightName.ToLower();
		int displayCompare = leftName.Compare(rightName);
		if (displayCompare != 0)
			return displayCompare;
		return left.m_sTownId.Compare(right.m_sTownId);
	}

	protected void SortResistanceTerritory(notnull array<ref HST_ResistanceTerritoryProjectionRow> rows)
	{
		for (int rowIndex = 1; rowIndex < rows.Count(); rowIndex++)
		{
			HST_ResistanceTerritoryProjectionRow selected = rows[rowIndex];
			int insertionIndex = rowIndex - 1;
			while (insertionIndex >= 0 && CompareResistanceTerritory(selected, rows[insertionIndex]) < 0)
			{
				rows[insertionIndex + 1] = rows[insertionIndex];
				insertionIndex--;
			}
			rows[insertionIndex + 1] = selected;
		}
	}

	protected int CompareResistanceTerritory(
		HST_ResistanceTerritoryProjectionRow left,
		HST_ResistanceTerritoryProjectionRow right)
	{
		if (!left && !right)
			return 0;
		if (!left)
			return 1;
		if (!right)
			return -1;

		int typeCompare = ZoneTypeRank(left.m_eZoneType) - ZoneTypeRank(right.m_eZoneType);
		if (typeCompare != 0)
			return typeCompare;
		string leftName = left.m_sDisplayName;
		string rightName = right.m_sDisplayName;
		leftName.ToLower();
		rightName.ToLower();
		int displayCompare = leftName.Compare(rightName);
		if (displayCompare != 0)
			return displayCompare;
		return left.m_sZoneId.Compare(right.m_sZoneId);
	}

	protected int ZoneTypeRank(HST_EZoneType zoneType)
	{
		if (zoneType == HST_EZoneType.HST_ZONE_TOWN)
			return 0;
		if (zoneType == HST_EZoneType.HST_ZONE_OUTPOST)
			return 1;
		if (zoneType == HST_EZoneType.HST_ZONE_RESOURCE)
			return 2;
		if (zoneType == HST_EZoneType.HST_ZONE_FACTORY)
			return 3;
		if (zoneType == HST_EZoneType.HST_ZONE_RADIO_TOWER)
			return 4;
		if (zoneType == HST_EZoneType.HST_ZONE_AIRFIELD)
			return 5;
		if (zoneType == HST_EZoneType.HST_ZONE_SEAPORT)
			return 6;
		if (zoneType == HST_EZoneType.HST_ZONE_POLICE_STATION)
			return 7;
		if (zoneType == HST_EZoneType.HST_ZONE_BANK)
			return 8;
		if (zoneType == HST_EZoneType.HST_ZONE_HIDEOUT)
			return 9;
		return 10;
	}

	protected int BasisPointsToPercent(int basisPoints)
	{
		return Math.Round(Math.Max(0, Math.Min(10000, basisPoints)) / 100.0);
	}

	protected bool IsUniqueZoneAuthority(
		HST_CampaignState state,
		HST_ZoneState zone)
	{
		if (!state || !zone || zone.m_sZoneId.IsEmpty())
			return false;
		int matches;
		foreach (HST_ZoneState candidate : state.m_aZones)
		{
			if (!candidate || candidate.m_sZoneId != zone.m_sZoneId)
				continue;
			matches++;
			if (matches > 1)
				return false;
		}
		return matches == 1;
	}

	protected string ResolveDisplayName(HST_ZoneState zone)
	{
		if (!zone)
			return "Unknown location";
		if (!zone.m_sDisplayName.IsEmpty())
			return zone.m_sDisplayName;
		return zone.m_sZoneId;
	}

	protected float DistanceSq2D(vector a, vector b)
	{
		float dx = a[0] - b[0];
		float dz = a[2] - b[2];
		return dx * dx + dz * dz;
	}

	protected bool IsZeroVector(vector value)
	{
		return value[0] == 0 && value[1] == 0 && value[2] == 0;
	}
}
