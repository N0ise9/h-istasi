class HST_EnemyDirectorService
{
	static const int RESOURCE_TICK_SECONDS = 300;

	bool TickResources(HST_CampaignState state, HST_CampaignPreset preset, HST_BalanceConfig balance, int elapsedSeconds)
	{
		if (!state || !preset || !balance || elapsedSeconds <= 0)
			return false;

		state.m_iEnemyResourceAccumulatorSeconds += elapsedSeconds;
		if (state.m_iEnemyResourceAccumulatorSeconds < RESOURCE_TICK_SECONDS)
			return false;

		int resourceSteps = state.m_iEnemyResourceAccumulatorSeconds / RESOURCE_TICK_SECONDS;
		state.m_iEnemyResourceAccumulatorSeconds = state.m_iEnemyResourceAccumulatorSeconds % RESOURCE_TICK_SECONDS;
		foreach (HST_ZoneState zone : state.m_aZones)
		{
			if (!zone || zone.m_sOwnerFactionKey == preset.m_sResistanceFactionKey)
				continue;

			int attackIncome = ResolveEnemyAttackIncome(state, balance, zone) * resourceSteps;
			int supportIncome = ResolveEnemySupportIncome(state, balance, zone) * resourceSteps;
			AddResources(state, zone.m_sOwnerFactionKey, attackIncome, supportIncome);
		}

		return true;
	}

	bool CanAfford(HST_CampaignState state, string factionKey, int attackCost, int supportCost)
	{
		HST_FactionPoolState pool = state.FindFactionPool(factionKey);
		return pool && pool.m_iAttackResources >= attackCost && pool.m_iSupportResources >= supportCost;
	}

	bool TrySpend(HST_CampaignState state, string factionKey, int attackCost, int supportCost)
	{
		if (attackCost < 0 || supportCost < 0 || !CanAfford(state, factionKey, attackCost, supportCost))
			return false;

		HST_FactionPoolState pool = state.FindFactionPool(factionKey);
		pool.m_iAttackResources -= attackCost;
		pool.m_iSupportResources -= supportCost;
		return true;
	}

	void AddResources(HST_CampaignState state, string factionKey, int attackResources, int supportResources)
	{
		HST_FactionPoolState pool = state.FindFactionPool(factionKey);
		if (!pool)
			return;

		pool.m_iAttackResources = Math.Max(0, pool.m_iAttackResources + attackResources);
		pool.m_iSupportResources = Math.Max(0, pool.m_iSupportResources + supportResources);
	}

	string BuildEnemyResourceReport(HST_CampaignState state, HST_CampaignPreset preset, HST_BalanceConfig balance)
	{
		if (!state || !preset || !balance)
			return "h-istasi enemy resources | state/preset/balance not ready";

		string report = string.Format(
			"h-istasi enemy resources | tick %1s | timer %2s | war %3 | attack war pct %4 | support war pct %5",
			RESOURCE_TICK_SECONDS,
			state.m_iEnemyResourceAccumulatorSeconds,
			state.m_iWarLevel,
			balance.m_iEnemyAttackIncomeWarPercent,
			balance.m_iEnemySupportIncomeWarPercent
		);

		foreach (HST_FactionPoolState pool : state.m_aFactionPools)
		{
			if (!pool || pool.m_sFactionKey == preset.m_sResistanceFactionKey)
				continue;

			int attackIncome;
			int supportIncome;
			CalculateProjectedIncomeForFaction(state, balance, pool.m_sFactionKey, attackIncome, supportIncome);
			report = report + string.Format(
				"\n%1 | current attack %2 support %3 aggression %4 | next attack +%5 support +%6",
				pool.m_sFactionKey,
				pool.m_iAttackResources,
				pool.m_iSupportResources,
				pool.m_iAggression,
				attackIncome,
				supportIncome
			);
		}

		return report;
	}

	protected void CalculateProjectedIncomeForFaction(HST_CampaignState state, HST_BalanceConfig balance, string factionKey, out int attackIncome, out int supportIncome)
	{
		attackIncome = 0;
		supportIncome = 0;
		foreach (HST_ZoneState zone : state.m_aZones)
		{
			if (!zone || zone.m_sOwnerFactionKey != factionKey)
				continue;

			attackIncome += ResolveEnemyAttackIncome(state, balance, zone);
			supportIncome += ResolveEnemySupportIncome(state, balance, zone);
		}
	}

	protected int ResolveEnemyAttackIncome(HST_CampaignState state, HST_BalanceConfig balance, HST_ZoneState zone)
	{
		int income = Math.Max(1, zone.m_iIncomeValue / 60) + Math.Max(0, zone.m_iPriority / 10);
		if (zone.m_eType == HST_EZoneType.HST_ZONE_AIRFIELD || zone.m_eType == HST_EZoneType.HST_ZONE_SEAPORT)
			income += 3;
		else if (zone.m_eType == HST_EZoneType.HST_ZONE_FACTORY)
			income += 2;

		if (zone.m_eType == HST_EZoneType.HST_ZONE_RESOURCE && zone.m_sResourceKind == "fuel")
			income += 2;

		int multiplier = 100 + state.m_iWarLevel * Math.Max(0, balance.m_iEnemyAttackIncomeWarPercent);
		income = Math.Max(1, Math.Round(income * multiplier / 100.0));
		return income;
	}

	protected int ResolveEnemySupportIncome(HST_CampaignState state, HST_BalanceConfig balance, HST_ZoneState zone)
	{
		int income = 1;
		if (zone.m_eType == HST_EZoneType.HST_ZONE_RESOURCE && zone.m_sResourceKind == "supplies")
			income += 2;
		if (zone.m_eType == HST_EZoneType.HST_ZONE_RADIO_TOWER || zone.m_eType == HST_EZoneType.HST_ZONE_POLICE_STATION)
			income += 1;
		if (zone.m_eType == HST_EZoneType.HST_ZONE_TOWN)
			income += Math.Max(0, 1 - zone.m_iSupport / 50);

		int multiplier = 100 + state.m_iWarLevel * Math.Max(0, balance.m_iEnemySupportIncomeWarPercent);
		income = Math.Max(1, Math.Round(income * multiplier / 100.0));
		return income;
	}
}