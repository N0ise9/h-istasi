class HST_HQService
{
	static const string PETROS_PREFAB = "{84B40583F4D1B7A3}Prefabs/Characters/Factions/INDFOR/FIA/Character_FIA_Rifleman.et";
	static const string HQ_CACHE_PREFAB = "{AB1A97B1BAE8C395}Prefabs/Compositions/Slotted/SlotFlatSmall/SupplyCache_S_FIA_01.et";
	static const string HQ_TENT_PREFAB = "{01AE5FD77A9A4C21}Prefabs/Structures/Military/Camps/TentSmallUS_01/TentSmallUS_01.et";

	bool SelectInitialHideout(HST_CampaignState state, string hideoutId)
	{
		if (!state || state.m_ePhase != HST_ECampaignPhase.HST_CAMPAIGN_SETUP || !HST_DefaultCatalog.IsKnownHideout(hideoutId))
			return false;

		SetHQPosition(state, hideoutId, HST_DefaultCatalog.GetHideoutPosition(hideoutId));
		state.m_ePhase = HST_ECampaignPhase.HST_CAMPAIGN_ACTIVE;
		return true;
	}

	bool MoveHQ(HST_CampaignState state, string hideoutId)
	{
		if (!state || state.m_ePhase != HST_ECampaignPhase.HST_CAMPAIGN_ACTIVE || !state.m_bPetrosAlive || !HST_DefaultCatalog.IsKnownHideout(hideoutId))
			return false;

		SetHQPosition(state, hideoutId, HST_DefaultCatalog.GetHideoutPosition(hideoutId));
		return true;
	}

	void OnPetrosKilled(HST_CampaignState state, HST_EconomyService economy, int moneyPenalty, int hrPenalty)
	{
		if (!state || !state.m_bPetrosAlive)
			return;

		state.m_bPetrosAlive = false;
		state.m_iPetrosDeaths++;
		state.m_bHQRuntimeObjectsSpawned = false;
		economy.AddFactionMoney(state, -moneyPenalty);
		economy.AddHR(state, -hrPenalty);
	}

	bool EnsureRuntimeObjects(HST_CampaignState state)
	{
		if (!state || !state.m_bHQDeployed || state.m_bHQRuntimeObjectsSpawned || !state.m_bPetrosAlive)
			return false;

		SCR_RespawnSystemComponent respawnSystem = SCR_RespawnSystemComponent.GetInstance();
		if (!respawnSystem)
			return false;

		GenericEntity petros = respawnSystem.DoSpawn(PETROS_PREFAB, state.m_vPetrosPosition, "0 0 0");
		GenericEntity cache = respawnSystem.DoSpawn(HQ_CACHE_PREFAB, state.m_vHQCachePosition, "0 0 0");
		GenericEntity tent = respawnSystem.DoSpawn(HQ_TENT_PREFAB, state.m_vHQTentPosition, "0 0 0");
		state.m_bHQRuntimeObjectsSpawned = true;
		if (!petros || !cache || !tent)
		{
			Print("h-istasi | HQ runtime object spawn incomplete; inspect placeholder prefab resources", LogLevel.WARNING);
			return true;
		}

		ApplyFaction(petros);
		return true;
	}

	protected void SetHQPosition(HST_CampaignState state, string hideoutId, vector hqPosition)
	{
		vector petrosOffset = "2 0 2";
		vector cacheOffset = "4 0 -2";
		vector tentOffset = "-4 0 2";
		state.m_sHQHideoutId = hideoutId;
		state.m_vHQPosition = hqPosition;
		state.m_vPetrosPosition = hqPosition + petrosOffset;
		state.m_vHQCachePosition = hqPosition + cacheOffset;
		state.m_vHQTentPosition = hqPosition + tentOffset;
		state.m_bHQDeployed = true;
		state.m_bHQRuntimeObjectsSpawned = false;
		state.m_bPetrosAlive = true;
		state.m_sPetrosPrefab = PETROS_PREFAB;
		state.m_sHQCachePrefab = HQ_CACHE_PREFAB;
		state.m_sHQTentPrefab = HQ_TENT_PREFAB;
	}

	protected void ApplyFaction(IEntity entity)
	{
		if (!entity)
			return;

		FactionAffiliationComponent factionComponent = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (factionComponent)
			factionComponent.SetAffiliatedFactionByKey("FIA");
	}
}
