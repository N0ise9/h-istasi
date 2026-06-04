class HST_SupportRequestService
{
	static const int PLAYER_SUPPLY_COST = 150;
	static const int DEFAULT_ETA_SECONDS = 120;
	static const int HELICOPTER_STYLE_ETA_SECONDS = 180;

	HST_SupportRequestState RequestSupport(HST_CampaignState state, HST_CampaignPreset preset, HST_EconomyService economy, HST_EnemyDirectorService enemyDirector, string factionKey, HST_ESupportRequestType supportType, string targetZoneId, bool playerRequested = false)
	{
		if (!state || !preset || factionKey.IsEmpty())
			return null;

		HST_ZoneState targetZone = state.FindZone(targetZoneId);
		if (!targetZone && targetZoneId.IsEmpty())
			targetZone = FindFallbackTargetZone(state, preset);

		if (!targetZone)
			return null;

		int attackCost;
		int supportCost;
		ResolveCosts(supportType, attackCost, supportCost);
		if (factionKey == preset.m_sResistanceFactionKey)
		{
			if (!economy || !economy.SpendFactionMoney(state, PLAYER_SUPPLY_COST))
				return null;
		}
		else if (!enemyDirector || !enemyDirector.TrySpend(state, factionKey, attackCost, supportCost))
		{
			return null;
		}

		HST_SupportRequestState request = new HST_SupportRequestState();
		request.m_sRequestId = BuildRequestId(state, factionKey, supportType);
		request.m_sFactionKey = factionKey;
		request.m_eType = supportType;
		request.m_eStatus = HST_ESupportRequestStatus.HST_SUPPORT_QUEUED;
		request.m_sTargetZoneId = targetZone.m_sZoneId;
		request.m_sSourceZoneId = FindSourceZoneId(state, preset, factionKey, targetZone.m_sZoneId);
		request.m_vTargetPosition = targetZone.m_vPosition;
		request.m_vSourcePosition = ResolveSourcePosition(state, request.m_sSourceZoneId, targetZone.m_vPosition);
		request.m_iRequestedAtSecond = state.m_iElapsedSeconds;
		request.m_iETASeconds = ResolveETA(supportType);
		request.m_iAttackCost = attackCost;
		request.m_iSupportCost = supportCost;
		request.m_bHelicopterStyle = IsHelicopterStyle(supportType);
		request.m_bPlayerRequested = playerRequested;
		state.m_aSupportRequests.Insert(request);
		Print(string.Format("h-istasi | support request %1 queued for %2 at %3", request.m_sRequestId, factionKey, targetZone.m_sZoneId));
		return request;
	}

	bool Tick(HST_CampaignState state, HST_CampaignPreset preset, HST_GarrisonService garrisons)
	{
		if (!state || !preset)
			return false;

		bool changed;
		foreach (HST_SupportRequestState request : state.m_aSupportRequests)
		{
			if (!request || request.m_eStatus == HST_ESupportRequestStatus.HST_SUPPORT_RESOLVED || request.m_eStatus == HST_ESupportRequestStatus.HST_SUPPORT_CANCELLED)
				continue;

			if (request.m_eStatus == HST_ESupportRequestStatus.HST_SUPPORT_QUEUED)
			{
				request.m_eStatus = HST_ESupportRequestStatus.HST_SUPPORT_ACTIVE;
				ApplyActiveSupport(state, preset, request);
				changed = true;
			}

			if (state.m_iElapsedSeconds < request.m_iRequestedAtSecond + request.m_iETASeconds)
				continue;

			ResolveSupport(state, preset, garrisons, request);
			changed = true;
		}

		return changed;
	}

	string BuildSupportReport(HST_CampaignState state)
	{
		if (!state)
			return "h-istasi support | state not ready";

		int queued;
		int active;
		int resolved;
		int helicopterStyle;
		foreach (HST_SupportRequestState request : state.m_aSupportRequests)
		{
			if (!request)
				continue;

			if (request.m_eStatus == HST_ESupportRequestStatus.HST_SUPPORT_QUEUED)
				queued++;
			else if (request.m_eStatus == HST_ESupportRequestStatus.HST_SUPPORT_ACTIVE)
				active++;
			else if (request.m_eStatus == HST_ESupportRequestStatus.HST_SUPPORT_RESOLVED)
				resolved++;

			if (request.m_bHelicopterStyle)
				helicopterStyle++;
		}

		return string.Format("h-istasi support | queued %1 | active %2 | resolved %3 | helo-style abstract %4", queued, active, resolved, helicopterStyle);
	}

	protected void ApplyActiveSupport(HST_CampaignState state, HST_CampaignPreset preset, HST_SupportRequestState request)
	{
		if (!request || request.m_bHelicopterStyle)
			return;

		if (request.m_eType == HST_ESupportRequestType.HST_SUPPORT_SUPPLY_DROP || request.m_eType == HST_ESupportRequestType.HST_SUPPORT_EVACUATION)
			return;

		HST_ActiveGroupState group = new HST_ActiveGroupState();
		group.m_sGroupId = string.Format("support_%1", request.m_sRequestId);
		group.m_sZoneId = request.m_sTargetZoneId;
		group.m_sFactionKey = request.m_sFactionKey;
		group.m_sPrefab = SelectGroupPrefab(request.m_sFactionKey, request.m_eType);
		group.m_vPosition = HST_WorldPositionService.ResolveGroundPosition(request.m_vTargetPosition, HST_WorldPositionService.CHARACTER_GROUND_OFFSET, false);
		group.m_iInfantryCount = 4 + state.m_iWarLevel;
		group.m_iVehicleCount = 0;
		group.m_bQRF = request.m_eType == HST_ESupportRequestType.HST_SUPPORT_QRF;
		state.m_aActiveGroups.Insert(group);
		request.m_sGroupId = group.m_sGroupId;
	}

	protected void ResolveSupport(HST_CampaignState state, HST_CampaignPreset preset, HST_GarrisonService garrisons, HST_SupportRequestState request)
	{
		if (!request || request.m_eStatus == HST_ESupportRequestStatus.HST_SUPPORT_RESOLVED)
			return;

		request.m_eStatus = HST_ESupportRequestStatus.HST_SUPPORT_RESOLVED;
		if (request.m_eType == HST_ESupportRequestType.HST_SUPPORT_SUPPLY_DROP && request.m_sFactionKey == preset.m_sResistanceFactionKey)
		{
			state.m_iFactionMoney += 75;
			state.m_iHR += 1;
		}

		if (request.m_eType == HST_ESupportRequestType.HST_SUPPORT_TROOP_LANDING && garrisons)
			garrisons.AddAbstractForces(state, request.m_sTargetZoneId, request.m_sFactionKey, 3 + state.m_iWarLevel, 0);
	}

	protected void ResolveCosts(HST_ESupportRequestType supportType, out int attackCost, out int supportCost)
	{
		attackCost = 8;
		supportCost = 4;

		if (supportType == HST_ESupportRequestType.HST_SUPPORT_QRF || supportType == HST_ESupportRequestType.HST_SUPPORT_SEARCH_AND_DESTROY)
		{
			attackCost = 15;
			supportCost = 5;
			return;
		}

		if (supportType == HST_ESupportRequestType.HST_SUPPORT_TROOP_LANDING || supportType == HST_ESupportRequestType.HST_SUPPORT_SUPPRESSIVE_FIRE)
		{
			attackCost = 20;
			supportCost = 8;
			return;
		}

		if (supportType == HST_ESupportRequestType.HST_SUPPORT_SUPPLY_DROP)
		{
			attackCost = 0;
			supportCost = 10;
		}
	}

	protected int ResolveETA(HST_ESupportRequestType supportType)
	{
		if (IsHelicopterStyle(supportType))
			return HELICOPTER_STYLE_ETA_SECONDS;

		return DEFAULT_ETA_SECONDS;
	}

	protected bool IsHelicopterStyle(HST_ESupportRequestType supportType)
	{
		return supportType == HST_ESupportRequestType.HST_SUPPORT_TRANSPORT || supportType == HST_ESupportRequestType.HST_SUPPORT_TROOP_LANDING || supportType == HST_ESupportRequestType.HST_SUPPORT_EVACUATION;
	}

	protected string BuildRequestId(HST_CampaignState state, string factionKey, HST_ESupportRequestType supportType)
	{
		return string.Format("support_%1_%2_%3", factionKey, state.m_iElapsedSeconds, state.m_aSupportRequests.Count());
	}

	protected string FindSourceZoneId(HST_CampaignState state, HST_CampaignPreset preset, string factionKey, string targetZoneId)
	{
		foreach (HST_ZoneState zone : state.m_aZones)
		{
			if (!zone || zone.m_sZoneId == targetZoneId)
				continue;

			if (zone.m_sOwnerFactionKey == factionKey)
				return zone.m_sZoneId;
		}

		return targetZoneId;
	}

	protected vector ResolveSourcePosition(HST_CampaignState state, string sourceZoneId, vector fallback)
	{
		HST_ZoneState sourceZone = state.FindZone(sourceZoneId);
		if (sourceZone)
			return sourceZone.m_vPosition;

		return fallback;
	}

	protected HST_ZoneState FindFallbackTargetZone(HST_CampaignState state, HST_CampaignPreset preset)
	{
		foreach (HST_ZoneState zone : state.m_aZones)
		{
			if (zone && zone.m_sOwnerFactionKey != preset.m_sResistanceFactionKey)
				return zone;
		}

		if (state.m_aZones.Count() > 0)
			return state.m_aZones[0];

		return null;
	}

	protected string SelectGroupPrefab(string factionKey, HST_ESupportRequestType supportType)
	{
		HST_FactionTemplate faction = HST_DefaultCatalog.CreateFactionTemplate(factionKey);
		if (!faction)
			return "";

		if ((supportType == HST_ESupportRequestType.HST_SUPPORT_QRF || supportType == HST_ESupportRequestType.HST_SUPPORT_SEARCH_AND_DESTROY) && faction.m_aQRFGroupPrefabs.Count() > 0)
			return faction.m_aQRFGroupPrefabs[0];

		if (faction.m_aPatrolGroupPrefabs.Count() > 0)
			return faction.m_aPatrolGroupPrefabs[0];

		if (faction.m_aGroupPrefabs.Count() > 0)
			return faction.m_aGroupPrefabs[0];

		return "";
	}
}
