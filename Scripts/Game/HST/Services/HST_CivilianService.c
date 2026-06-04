class HST_CivilianService
{
	static const int HEAT_DECAY_SECONDS = 300;
	static const int UNDERCOVER_RECHECK_SECONDS = 20;

	bool EnsureCivilianZones(HST_CampaignState state)
	{
		if (!state || state.m_aCivilianZones.Count() > 0)
			return false;

		foreach (HST_ZoneState zone : state.m_aZones)
		{
			if (!zone || zone.m_eType != HST_EZoneType.HST_ZONE_TOWN)
				continue;

			HST_CivilianZoneState civilianZone = new HST_CivilianZoneState();
			civilianZone.m_sZoneId = zone.m_sZoneId;
			civilianZone.m_iReputation = 50;
			civilianZone.m_iCivilianPresence = Math.Max(4, zone.m_iIncomeValue / 8);
			civilianZone.m_iPolicePresence = Math.Max(1, zone.m_iGarrisonSlots / 6);
			civilianZone.m_iRoadblockPresence = 1;
			civilianZone.m_bUndercoverRestricted = zone.m_iSupport < 25;
			state.m_aCivilianZones.Insert(civilianZone);
		}

		Print(string.Format("h-istasi | initialized %1 civilian town record(s)", state.m_aCivilianZones.Count()));
		return true;
	}

	bool Tick(HST_CampaignState state, int elapsedSeconds)
	{
		if (!state || elapsedSeconds <= 0)
			return false;

		bool changed;
		foreach (HST_CivilianZoneState civilianZone : state.m_aCivilianZones)
		{
			if (!civilianZone || civilianZone.m_iWantedHeat <= 0)
				continue;

			if (state.m_iElapsedSeconds < civilianZone.m_iLastIncidentSecond + HEAT_DECAY_SECONDS)
				continue;

			civilianZone.m_iWantedHeat = Math.Max(0, civilianZone.m_iWantedHeat - 1);
			civilianZone.m_iLastIncidentSecond = state.m_iElapsedSeconds;
			changed = true;
		}

		foreach (HST_PlayerUndercoverState undercover : state.m_aUndercoverPlayers)
		{
			if (!undercover || undercover.m_iWantedHeat <= 0)
				continue;

			if (undercover.m_eStatus == HST_EUndercoverStatus.HST_UNDERCOVER_COMPROMISED && state.m_iElapsedSeconds < undercover.m_iCompromisedUntilSecond)
				continue;

			undercover.m_iWantedHeat = Math.Max(0, undercover.m_iWantedHeat - 1);
			if (undercover.m_iWantedHeat == 0)
			{
				undercover.m_eStatus = HST_EUndercoverStatus.HST_UNDERCOVER_CLEAR;
				undercover.m_sLastReason = "heat cooled";
			}

			changed = true;
		}

		return changed;
	}

	HST_PlayerUndercoverState EnsurePlayer(HST_CampaignState state, string identityId)
	{
		if (!state || identityId.IsEmpty())
			return null;

		HST_PlayerUndercoverState undercover = state.FindUndercoverPlayer(identityId);
		if (undercover)
			return undercover;

		undercover = new HST_PlayerUndercoverState();
		undercover.m_sIdentityId = identityId;
		state.m_aUndercoverPlayers.Insert(undercover);
		return undercover;
	}

	bool RegisterIncident(HST_CampaignState state, string zoneId, int reputationDelta, int heatDelta, string reason)
	{
		if (!state || zoneId.IsEmpty())
			return false;

		HST_CivilianZoneState civilianZone = state.FindCivilianZone(zoneId);
		if (!civilianZone)
			return false;

		civilianZone.m_iReputation = Math.Max(0, Math.Min(100, civilianZone.m_iReputation + reputationDelta));
		civilianZone.m_iWantedHeat = Math.Max(0, civilianZone.m_iWantedHeat + heatDelta);
		civilianZone.m_iLastIncidentSecond = state.m_iElapsedSeconds;
		HST_ZoneState zone = state.FindZone(zoneId);
		if (zone)
			zone.m_iSupport = Math.Max(-100, Math.Min(100, zone.m_iSupport + reputationDelta / 2));

		return true;
	}

	bool CheckUndercover(HST_CampaignState state, string identityId, string zoneId, bool visiblyArmed, bool suspiciousVehicle, bool recentCombat)
	{
		HST_PlayerUndercoverState undercover = EnsurePlayer(state, identityId);
		if (!undercover)
			return false;

		if (state.m_iElapsedSeconds < undercover.m_iLastCheckedSecond + UNDERCOVER_RECHECK_SECONDS)
			return false;

		undercover.m_iLastCheckedSecond = state.m_iElapsedSeconds;
		HST_CivilianZoneState civilianZone = state.FindCivilianZone(zoneId);
		if (!civilianZone)
		{
			undercover.m_eStatus = HST_EUndercoverStatus.HST_UNDERCOVER_CLEAR;
			undercover.m_sLastReason = "outside civilian zone";
			return true;
		}

		if (recentCombat || visiblyArmed)
		{
			undercover.m_eStatus = HST_EUndercoverStatus.HST_UNDERCOVER_COMPROMISED;
			undercover.m_iWantedHeat = Math.Max(undercover.m_iWantedHeat, 4);
			undercover.m_iCompromisedUntilSecond = state.m_iElapsedSeconds + 180;
			undercover.m_sLastReason = "armed or combat exposure";
			civilianZone.m_iWantedHeat = Math.Max(civilianZone.m_iWantedHeat, 3);
			return true;
		}

		if (suspiciousVehicle || civilianZone.m_bUndercoverRestricted || civilianZone.m_iWantedHeat >= 4)
		{
			undercover.m_eStatus = HST_EUndercoverStatus.HST_UNDERCOVER_SUSPICIOUS;
			undercover.m_iWantedHeat = Math.Max(undercover.m_iWantedHeat, 1);
			undercover.m_sLastReason = "suspicious presence";
			return true;
		}

		undercover.m_eStatus = HST_EUndercoverStatus.HST_UNDERCOVER_CLEAR;
		undercover.m_sLastReason = "clear";
		return true;
	}

	string BuildCivilianReport(HST_CampaignState state)
	{
		if (!state)
			return "h-istasi civilians | state not ready";

		int reputationTotal;
		int heatTotal;
		int policeTotal;
		int roadblocks;
		foreach (HST_CivilianZoneState civilianZone : state.m_aCivilianZones)
		{
			if (!civilianZone)
				continue;

			reputationTotal += civilianZone.m_iReputation;
			heatTotal += civilianZone.m_iWantedHeat;
			policeTotal += civilianZone.m_iPolicePresence;
			roadblocks += civilianZone.m_iRoadblockPresence;
		}

		int averageReputation;
		if (state.m_aCivilianZones.Count() > 0)
			averageReputation = reputationTotal / state.m_aCivilianZones.Count();

		return string.Format("h-istasi civilians | towns %1 | avg reputation %2 | heat %3 | police %4 | roadblocks %5", state.m_aCivilianZones.Count(), averageReputation, heatTotal, policeTotal, roadblocks);
	}

	string BuildUndercoverReport(HST_CampaignState state, string identityId = "")
	{
		if (!state)
			return "h-istasi undercover | state not ready";

		if (!identityId.IsEmpty())
		{
			HST_PlayerUndercoverState undercover = state.FindUndercoverPlayer(identityId);
			if (!undercover)
				return "h-istasi undercover | no record";

			return string.Format("h-istasi undercover | %1 | status %2 | heat %3 | reason %4", identityId, undercover.m_eStatus, undercover.m_iWantedHeat, undercover.m_sLastReason);
		}

		int suspicious;
		int compromised;
		foreach (HST_PlayerUndercoverState undercoverPlayer : state.m_aUndercoverPlayers)
		{
			if (!undercoverPlayer)
				continue;

			if (undercoverPlayer.m_eStatus == HST_EUndercoverStatus.HST_UNDERCOVER_SUSPICIOUS)
				suspicious++;
			else if (undercoverPlayer.m_eStatus == HST_EUndercoverStatus.HST_UNDERCOVER_COMPROMISED || undercoverPlayer.m_eStatus == HST_EUndercoverStatus.HST_UNDERCOVER_WANTED)
				compromised++;
		}

		return string.Format("h-istasi undercover | tracked %1 | suspicious %2 | compromised/wanted %3", state.m_aUndercoverPlayers.Count(), suspicious, compromised);
	}
}
