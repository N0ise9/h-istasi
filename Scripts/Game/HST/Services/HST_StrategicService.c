class HST_CampaignOutcomeResult
{
	bool m_bEnded;
	HST_ECampaignPhase m_ePhase;
	string m_sReason;
	string m_sSummary;
	int m_iControlPercent;
	int m_iWarLevel;
	int m_iFIAZones;
	int m_iEnemyZones;
}

class HST_StrategicService
{
	bool SetZoneOwner(HST_CampaignState state, HST_EconomyService economy, HST_BalanceConfig balance, string zoneId, string factionKey, string resistanceFactionKey = "FIA")
	{
		HST_ZoneState zone = state.FindZone(zoneId);
		if (!zone || !state.FindFactionPool(factionKey) || zone.m_sOwnerFactionKey == factionKey)
			return false;

		string previousOwner = zone.m_sOwnerFactionKey;
		zone.m_sOwnerFactionKey = factionKey;
		zone.m_iResistanceCaptureProgress = 0;
		zone.m_iActiveInfantryCount = 0;
		zone.m_iActiveVehicleCount = 0;
		ClearZoneGarrison(state, zoneId, previousOwner);
		economy.RecalculateWarLevel(state, balance, resistanceFactionKey);
		EvaluateCampaignOutcome(state, economy, balance, resistanceFactionKey);
		return true;
	}

	bool AddTownSupport(HST_CampaignState state, string zoneId, int amount)
	{
		HST_ZoneState zone = state.FindZone(zoneId);
		if (!zone)
			return false;

		zone.m_iSupport = Math.Max(-100, Math.Min(100, zone.m_iSupport + amount));
		return true;
	}

	bool SetZoneActive(HST_CampaignState state, string zoneId, bool active)
	{
		HST_ZoneState zone = state.FindZone(zoneId);
		if (!zone)
			return false;

		zone.m_bActive = active;
		return true;
	}

	void OnPetrosKilled(HST_CampaignState state)
	{
		state.m_bPetrosAlive = false;
		state.m_iFactionMoney = Math.Max(0, state.m_iFactionMoney / 2);
		state.m_iHR = Math.Max(0, state.m_iHR / 2);
	}

	HST_CampaignOutcomeResult EvaluateCampaignOutcomeDetailed(HST_CampaignState state, HST_EconomyService economy, HST_BalanceConfig balance, string resistanceFactionKey)
	{
		HST_CampaignOutcomeResult result = new HST_CampaignOutcomeResult();
		if (!state || !economy || !balance || state.m_ePhase != HST_ECampaignPhase.HST_CAMPAIGN_ACTIVE)
			return result;

		int fiaZones = CountZonesOwnedBy(state, resistanceFactionKey);
		int enemyZones = CountZonesNotOwnedBy(state, resistanceFactionKey);
		int score = economy.CalculateResistanceStrategicScore(state, resistanceFactionKey);
		int totalScore = CalculateTotalStrategicScoreForOutcome(state, economy);
		int controlPercent;
		if (totalScore > 0)
			controlPercent = Math.Round(score * 100.0 / totalScore);

		result.m_iControlPercent = controlPercent;
		result.m_iWarLevel = state.m_iWarLevel;
		result.m_iFIAZones = fiaZones;
		result.m_iEnemyZones = enemyZones;

		bool airfieldsReady = !balance.m_bVictoryRequiresAirfields || AreAllTypeOwnedBy(state, HST_EZoneType.HST_ZONE_AIRFIELD, resistanceFactionKey);
		bool seaportsReady = !balance.m_bVictoryRequiresSeaports || AreAllTypeOwnedBy(state, HST_EZoneType.HST_ZONE_SEAPORT, resistanceFactionKey);
		bool controlReady = controlPercent >= balance.m_iVictoryControlPercent;
		if (controlReady && airfieldsReady && seaportsReady)
		{
			result.m_bEnded = true;
			result.m_ePhase = HST_ECampaignPhase.HST_CAMPAIGN_WON;
			result.m_sReason = "victory: strategic control achieved";
			result.m_sSummary = string.Format("FIA controls %1 percent of strategic score with required airfields %2 and seaports %3.", controlPercent, airfieldsReady, seaportsReady);
			return result;
		}

		if (ShouldLoseCampaign(state, balance, resistanceFactionKey))
		{
			result.m_bEnded = true;
			result.m_ePhase = HST_ECampaignPhase.HST_CAMPAIGN_LOST;
			result.m_sReason = "loss: FIA collapse";
			result.m_sSummary = string.Format("FIA collapsed with money %1 HR %2 Petros deaths %3.", state.m_iFactionMoney, state.m_iHR, state.m_iPetrosDeaths);
			return result;
		}

		return result;
	}

	void ApplyCampaignOutcome(HST_CampaignState state, HST_CampaignOutcomeResult outcome)
	{
		if (!state || !outcome || !outcome.m_bEnded)
			return;

		state.m_ePhase = outcome.m_ePhase;
		state.m_sCampaignEndReason = outcome.m_sReason;
		state.m_sCampaignEndSummary = outcome.m_sSummary;
		state.m_iCampaignEndedAtSecond = state.m_iElapsedSeconds;
		state.m_iCampaignEndControlPercent = outcome.m_iControlPercent;
		state.m_iCampaignEndWarLevel = outcome.m_iWarLevel;
		state.m_iCampaignEndFIAZones = outcome.m_iFIAZones;
		state.m_iCampaignEndEnemyZones = outcome.m_iEnemyZones;
		state.m_bCampaignEndReportGenerated = true;
	}

	string BuildCampaignEndReport(HST_CampaignState state, HST_EconomyService economy, HST_BalanceConfig balance, HST_CampaignPreset preset)
	{
		if (!state)
			return "h-istasi campaign end | state not ready";

		string resistanceFactionKey = "FIA";
		if (preset && !preset.m_sResistanceFactionKey.IsEmpty())
			resistanceFactionKey = preset.m_sResistanceFactionKey;

		string phase = string.Format("%1", state.m_ePhase);
		int score;
		int totalScore;
		int controlPercent;
		if (economy)
		{
			score = economy.CalculateResistanceStrategicScore(state, resistanceFactionKey);
			totalScore = CalculateTotalStrategicScoreForOutcome(state, economy);
			if (totalScore > 0)
				controlPercent = Math.Round(score * 100.0 / totalScore);
		}

		string report = string.Format(
			"h-istasi campaign end | phase %1 | ended %2 | reason %3 | summary %4 | control %5 pct | score %6/%7 | war %8 | FIA zones %9",
			phase,
			state.m_iCampaignEndedAtSecond,
			state.m_sCampaignEndReason,
			state.m_sCampaignEndSummary,
			controlPercent,
			score,
			totalScore,
			state.m_iWarLevel,
			CountZonesOwnedBy(state, resistanceFactionKey)
		);
		report = report + string.Format(" | enemy zones %1", CountZonesNotOwnedBy(state, resistanceFactionKey));

		if (state.m_bCampaignEndReportGenerated)
		{
			report = report + string.Format(
				"\nfinal persisted | control %1 pct | war %2 | FIA zones %3 | enemy zones %4",
				state.m_iCampaignEndControlPercent,
				state.m_iCampaignEndWarLevel,
				state.m_iCampaignEndFIAZones,
				state.m_iCampaignEndEnemyZones
			);
		}

		if (balance)
		{
			report = report + string.Format(
				"\nrequirements | victory %1 pct | airfields %2 | seaports %3 | loss enabled %4 | Petros death limit %5 | grace %6s",
				balance.m_iVictoryControlPercent,
				balance.m_bVictoryRequiresAirfields,
				balance.m_bVictoryRequiresSeaports,
				balance.m_bLossConditionEnabled,
				balance.m_iLossPetrosDeathLimit,
				balance.m_iLossGraceSeconds
			);
		}

		return report;
	}

	int CountZonesOwnedBy(HST_CampaignState state, string factionKey)
	{
		int count;
		if (!state)
			return count;

		foreach (HST_ZoneState zone : state.m_aZones)
		{
			if (IsStrategicZoneCounted(zone) && zone.m_sOwnerFactionKey == factionKey)
				count++;
		}

		return count;
	}

	int CountZonesNotOwnedBy(HST_CampaignState state, string factionKey)
	{
		int count;
		if (!state)
			return count;

		foreach (HST_ZoneState zone : state.m_aZones)
		{
			if (IsStrategicZoneCounted(zone) && zone.m_sOwnerFactionKey != factionKey)
				count++;
		}

		return count;
	}

	protected bool IsStrategicZoneCounted(HST_ZoneState zone)
	{
		return zone && zone.m_eType != HST_EZoneType.HST_ZONE_HIDEOUT && zone.m_eType != HST_EZoneType.HST_ZONE_MISSION_SITE;
	}

	protected void ClearZoneGarrison(HST_CampaignState state, string zoneId, string factionKey)
	{
		if (!state || zoneId.IsEmpty() || factionKey.IsEmpty())
			return;

		HST_GarrisonState garrison = state.FindGarrison(zoneId, factionKey);
		if (!garrison)
			return;

		garrison.m_iInfantryCount = 0;
		garrison.m_iVehicleCount = 0;
	}

	protected void EvaluateCampaignOutcome(HST_CampaignState state, HST_EconomyService economy, HST_BalanceConfig balance, string resistanceFactionKey)
	{
		HST_CampaignOutcomeResult outcome = EvaluateCampaignOutcomeDetailed(state, economy, balance, resistanceFactionKey);
		if (!outcome || !outcome.m_bEnded)
			return;

		ApplyCampaignOutcome(state, outcome);
	}

	protected int CalculateTotalStrategicScoreForOutcome(HST_CampaignState state, HST_EconomyService economy)
	{
		if (!economy)
			return 0;

		return economy.CalculateTotalStrategicScore(state);
	}

	protected bool AreAllTypeOwnedBy(HST_CampaignState state, HST_EZoneType zoneType, string factionKey)
	{
		bool found;
		foreach (HST_ZoneState zone : state.m_aZones)
		{
			if (!zone || zone.m_eType != zoneType)
				continue;

			found = true;
			if (zone.m_sOwnerFactionKey != factionKey)
				return false;
		}

		return found;
	}

	protected bool ShouldLoseCampaign(HST_CampaignState state, HST_BalanceConfig balance, string resistanceFactionKey)
	{
		if (!balance.m_bLossConditionEnabled)
			return false;
		if (state.m_iElapsedSeconds < balance.m_iLossGraceSeconds)
			return false;
		if (state.m_iPetrosDeaths >= balance.m_iLossPetrosDeathLimit)
			return true;
		if (state.m_iHR <= balance.m_iLossHRThreshold && state.m_iFactionMoney <= balance.m_iLossMoneyThreshold && !HasRecoverableFriendlyZone(state, resistanceFactionKey))
			return true;

		return false;
	}

	protected bool HasRecoverableFriendlyZone(HST_CampaignState state, string resistanceFactionKey)
	{
		foreach (HST_ZoneState zone : state.m_aZones)
		{
			if (zone && zone.m_sOwnerFactionKey == resistanceFactionKey)
				return true;
		}

		return false;
	}
}
