class HST_EnemyDirectorService
{
	static const int RESOURCE_TICK_SECONDS = 300;
	static const int SUPPORT_STACK_COOLDOWN_SECONDS = 600;
	static const int SUPPORT_SPEND_WINDOW_SECONDS = 1800;
	static const int RECENT_DAMAGE_WINDOW_SECONDS = 900;
	static const int MAX_DEFENCE_SPEND_BASE = 28;
	static const int MAX_DEFENCE_SPEND_PER_WAR_LEVEL = 7;
	protected ref HST_TownInfluenceService m_TownInfluence;
	protected ref HST_CampaignPreset m_Preset;
	protected ref HST_EnemyStrategicResourceService m_EnemyStrategicResources;

	void HST_EnemyDirectorService()
	{
		m_Preset = HST_DefaultCatalog.CreateVanillaEveronPreset();
		m_EnemyStrategicResources = new HST_EnemyStrategicResourceService();
	}

	void SetTownInfluenceService(HST_TownInfluenceService townInfluence)
	{
		m_TownInfluence = townInfluence;
		if (m_EnemyStrategicResources)
			m_EnemyStrategicResources.SetTownInfluenceService(townInfluence);
	}

	void SetCampaignPreset(HST_CampaignPreset preset)
	{
		m_Preset = preset;
	}

	void SetEnemyStrategicResourceAuthority(HST_EnemyStrategicResourceService authority)
	{
		m_EnemyStrategicResources = authority;
		if (m_EnemyStrategicResources && m_TownInfluence)
			m_EnemyStrategicResources.SetTownInfluenceService(m_TownInfluence);
	}

	bool TickResources(HST_CampaignState state, HST_CampaignPreset preset, HST_BalanceConfig balance, int elapsedSeconds)
	{
		if (!m_EnemyStrategicResources)
			return false;

		return m_EnemyStrategicResources.TickIncome(
			state,
			preset,
			balance,
			elapsedSeconds);
	}

	bool CanAfford(HST_CampaignState state, string factionKey, int attackCost, int supportCost)
	{
		HST_FactionPoolState pool = FindAuthoritativePool(state, factionKey);
		return pool && pool.m_iAttackResources >= attackCost && pool.m_iSupportResources >= supportCost;
	}

	bool TrySpend(
		HST_CampaignState state,
		string factionKey,
		int attackCost,
		int supportCost,
		string mutationId = "",
		string kind = "enemy_resource_spend",
		string sourceId = "",
		string orderId = "",
		string operationId = "",
		string manifestId = "",
		string zoneId = "")
	{
		if (!state || attackCost < 0 || supportCost < 0
			|| !m_EnemyStrategicResources || !m_Preset)
			return false;
		if (!mutationId.IsEmpty() && state.FindEnemyStrategicMutation(mutationId))
		{
			HST_EnemyStrategicMutationResult replay = m_EnemyStrategicResources.DebitResources(
				state,
				m_Preset,
				mutationId,
				factionKey,
				attackCost,
				supportCost,
				kind,
				sourceId,
				orderId,
				operationId,
				manifestId,
				zoneId);
			return replay && replay.m_bAccepted;
		}
		if (!CanAfford(state, factionKey, attackCost, supportCost))
			return false;
		mutationId = ResolveMutationId(state, mutationId, "enemy_resource_spend");
		if (mutationId.IsEmpty())
			return false;

		HST_EnemyStrategicMutationResult result = m_EnemyStrategicResources.DebitResources(
			state,
			m_Preset,
			mutationId,
			factionKey,
			attackCost,
			supportCost,
			kind,
			sourceId,
			orderId,
			operationId,
			manifestId,
			zoneId);
		return result && result.m_bAccepted;
	}

	bool CanSpendProactiveAttack(HST_CampaignState state, string factionKey, int attackCost, out string reason)
	{
		reason = "";
		if (!state || factionKey.IsEmpty())
		{
			reason = "state or faction missing";
			return false;
		}

		if (attackCost < 0)
		{
			reason = "negative proactive attack spend requested";
			return false;
		}

		HST_FactionPoolState pool = FindAuthoritativePool(state, factionKey);
		if (!pool)
		{
			reason = "faction resource pool missing";
			return false;
		}

		if (pool.m_iAttackResources < attackCost)
		{
			reason = string.Format("cannot afford proactive attack %1/%2", pool.m_iAttackResources, attackCost);
			return false;
		}

		reason = string.Format("proactive attack spend allowed | attack %1/%2", pool.m_iAttackResources, attackCost);
		return true;
	}

	bool TrySpendProactiveAttack(
		HST_CampaignState state,
		string factionKey,
		int attackCost,
		out string reason,
		string mutationId = "",
		string sourceId = "",
		string orderId = "",
		string operationId = "",
		string manifestId = "",
		string zoneId = "")
	{
		if (!state || !m_EnemyStrategicResources || !m_Preset)
		{
			reason = "enemy proactive resource authority unavailable";
			return false;
		}
		if (!mutationId.IsEmpty() && state.FindEnemyStrategicMutation(mutationId))
		{
			HST_EnemyStrategicMutationResult replay = m_EnemyStrategicResources.DebitResources(
				state,
				m_Preset,
				mutationId,
				factionKey,
				attackCost,
				0,
				"proactive_attack_debit",
				sourceId,
				orderId,
				operationId,
				manifestId,
				zoneId);
			if (replay && replay.m_bAccepted)
			{
				reason = "proactive attack debit replay accepted";
				return true;
			}
			reason = "proactive attack debit replay rejected";
			if (replay && !replay.m_sFailureReason.IsEmpty())
				reason = replay.m_sFailureReason;
			return false;
		}
		if (!CanSpendProactiveAttack(state, factionKey, attackCost, reason))
			return false;
		mutationId = ResolveMutationId(state, mutationId, "enemy_proactive_debit");
		if (mutationId.IsEmpty())
		{
			reason = "enemy proactive debit identity unavailable";
			return false;
		}
		HST_EnemyStrategicMutationResult result = m_EnemyStrategicResources.DebitResources(
			state,
			m_Preset,
			mutationId,
			factionKey,
			attackCost,
			0,
			"proactive_attack_debit",
			sourceId,
			orderId,
			operationId,
			manifestId,
			zoneId);
		if (!result || !result.m_bAccepted)
		{
			reason = "enemy proactive debit rejected";
			if (result && !result.m_sFailureReason.IsEmpty())
				reason = result.m_sFailureReason;
			return false;
		}
		reason = string.Format("proactive attack debit accepted | attack %1", attackCost);
		return true;
	}

	bool RefundProactiveAttackResources(
		HST_CampaignState state,
		string factionKey,
		int attackRefund,
		string reason,
		string mutationId = "",
		string sourceId = "",
		string orderId = "",
		string operationId = "",
		string manifestId = "",
		string zoneId = "")
	{
		if (!state || factionKey.IsEmpty() || attackRefund < 0
			|| !m_EnemyStrategicResources || !m_Preset)
			return false;
		mutationId = ResolveMutationId(state, mutationId, "enemy_proactive_refund");
		if (mutationId.IsEmpty())
			return false;

		HST_EnemyStrategicMutationResult result = m_EnemyStrategicResources.RefundResources(
			state,
			m_Preset,
			mutationId,
			factionKey,
			Math.Max(0, attackRefund),
			0,
			"proactive_attack_refund",
			sourceId,
			orderId,
			operationId,
			manifestId,
			zoneId);
		return result && result.m_bAccepted;
	}

	bool CanSpendDefense(HST_CampaignState state, HST_ZoneState targetZone, string factionKey, int attackCost, int supportCost, out string reason)
	{
		reason = "";
		if (!state || !targetZone || factionKey.IsEmpty())
		{
			reason = "state, target zone, or faction missing";
			return false;
		}

		if (attackCost < 0 || supportCost < 0)
		{
			reason = "negative defense spend requested";
			return false;
		}

		string canonicalZoneId = HST_MaidensBayLocationSaveValidationService.ResolveCanonicalZoneId(
			targetZone.m_sZoneId);
		HST_EnemySupportLedgerState ledger;
		int ledgerCount = ResolveSupportLedgerCount(
			state,
			factionKey,
			canonicalZoneId,
			ledger);
		if (ledgerCount > 1)
		{
			reason = "enemy support ledger authority is duplicated";
			return false;
		}
		if (ledger && ledger.m_iCooldownUntilSecond > state.m_iElapsedSeconds)
		{
			reason = string.Format("support stack cooldown active for %1s", ledger.m_iCooldownUntilSecond - state.m_iElapsedSeconds);
			return false;
		}

		HST_FactionPoolState pool = FindAuthoritativePool(state, factionKey);
		if (!pool)
		{
			reason = "authoritative faction resource pool unavailable";
			return false;
		}

		if (pool.m_iAttackResources < attackCost || pool.m_iSupportResources < supportCost)
		{
			reason = string.Format("cannot afford attack %1/%2 support %3/%4", pool.m_iAttackResources, attackCost, pool.m_iSupportResources, supportCost);
			return false;
		}

		int maxSpend = ResolveMaxDefenseSpend(state, targetZone, ledger);
		int projectedSpend = ResolveEffectiveAttackSpent(state, ledger)
			+ ResolveEffectiveSupportSpent(state, ledger)
			+ attackCost
			+ supportCost;
		if (projectedSpend > maxSpend)
		{
			reason = string.Format("max defense spend reached for zone %1 | projected %2 cap %3", targetZone.m_sZoneId, projectedSpend, maxSpend);
			return false;
		}

		reason = string.Format("defense spend allowed | projected %1 cap %2 damage %3", projectedSpend, maxSpend, ResolveRecentDamageScore(state, ledger));
		return true;
	}

	bool TrySpendDefense(
		HST_CampaignState state,
		HST_ZoneState targetZone,
		string factionKey,
		int attackCost,
		int supportCost,
		out string reason,
		string mutationId = "",
		string sourceId = "",
		string orderId = "",
		string operationId = "",
		string manifestId = "")
	{
		if (!state || !targetZone || !m_EnemyStrategicResources || !m_Preset)
		{
			reason = "enemy defense resource authority unavailable";
			return false;
		}
		if (!mutationId.IsEmpty() && state.FindEnemyStrategicMutation(mutationId))
		{
			HST_EnemySupportLedgerState replayLedger;
			int replayLedgerCount = ResolveSupportLedgerCount(
				state,
				factionKey,
				targetZone.m_sZoneId,
				replayLedger);
			if (replayLedgerCount != 1)
			{
				reason = "defense resource debit replay has no unique support ledger";
				return false;
			}
			HST_EnemyStrategicMutationResult replay = m_EnemyStrategicResources.DebitResources(
				state,
				m_Preset,
				mutationId,
				factionKey,
				attackCost,
				supportCost,
				"defense_support_debit",
				sourceId,
				orderId,
				operationId,
				manifestId,
				targetZone.m_sZoneId);
			if (replay && replay.m_bAccepted)
			{
				reason = "defense resource debit replay accepted";
				return true;
			}
			reason = "defense resource debit replay rejected";
			if (replay && !replay.m_sFailureReason.IsEmpty())
				reason = replay.m_sFailureReason;
			return false;
		}
		if (!CanSpendDefense(state, targetZone, factionKey, attackCost, supportCost, reason))
			return false;
		mutationId = ResolveMutationId(state, mutationId, "enemy_defense_debit");
		if (mutationId.IsEmpty())
		{
			reason = "enemy defense debit identity unavailable";
			return false;
		}
		HST_EnemyStrategicMutationResult result = m_EnemyStrategicResources.DebitResources(
			state,
			m_Preset,
			mutationId,
			factionKey,
			attackCost,
			supportCost,
			"defense_support_debit",
			sourceId,
			orderId,
			operationId,
			manifestId,
			targetZone.m_sZoneId);
		if (!result || !result.m_bAccepted)
		{
			reason = "enemy resource debit failed after defense gate";
			if (result && !result.m_sFailureReason.IsEmpty())
				reason = result.m_sFailureReason;
			return false;
		}
		if (result.m_bAlreadyApplied)
			return true;

		HST_EnemySupportLedgerState ledger = FindOrCreateSupportLedger(state, factionKey, targetZone.m_sZoneId);
		if (ledger)
		{
			UpdateLedgerWindows(state, ledger);
			ledger.m_iAttackSpent += attackCost;
			ledger.m_iSupportSpent += supportCost;
			ledger.m_iLastSpendSecond = state.m_iElapsedSeconds;
			ledger.m_iCooldownUntilSecond = state.m_iElapsedSeconds + SUPPORT_STACK_COOLDOWN_SECONDS;
			ledger.m_sLastDecisionReason = string.Format("spent attack %1 support %2 | cooldown until %3", attackCost, supportCost, ledger.m_iCooldownUntilSecond);
		}

		return true;
	}

	void RecordZoneDamageSignal(HST_CampaignState state, string factionKey, HST_ZoneState targetZone, int damageScore, string reason)
	{
		if (!state || !targetZone || factionKey.IsEmpty() || damageScore <= 0)
			return;

		HST_EnemySupportLedgerState ledger = FindOrCreateSupportLedger(state, factionKey, targetZone.m_sZoneId);
		if (!ledger)
			return;

		UpdateLedgerWindows(state, ledger);
		ledger.m_iRecentDamageScore = Math.Min(100, ledger.m_iRecentDamageScore + damageScore);
		ledger.m_iLastDamageSecond = state.m_iElapsedSeconds;
		if (reason.IsEmpty())
			reason = "recent resistance pressure";
		ledger.m_sLastDecisionReason = string.Format("%1 | damage score %2", reason, ledger.m_iRecentDamageScore);
	}

	int GetRecentDamageScore(HST_CampaignState state, string factionKey, string zoneId)
	{
		if (!state)
			return 0;

		HST_EnemySupportLedgerState ledger = state.FindEnemySupportLedger(factionKey, zoneId);
		if (!ledger)
			return 0;

		return ResolveRecentDamageScore(state, ledger);
	}

	bool RefundDefenseResources(
		HST_CampaignState state,
		string factionKey,
		string zoneId,
		int attackRefund,
		int supportRefund,
		string reason,
		string mutationId = "",
		string sourceId = "",
		string orderId = "",
		string operationId = "",
		string manifestId = "")
	{
		if (!state || factionKey.IsEmpty() || !m_EnemyStrategicResources || !m_Preset)
			return false;

		int clampedAttack = Math.Max(0, attackRefund);
		int clampedSupport = Math.Max(0, supportRefund);
		HST_EnemySupportLedgerState ledger;
		int ledgerCount = ResolveSupportLedgerCount(
			state,
			factionKey,
			zoneId,
			ledger);
		if (ledgerCount != 1)
			return false;
		mutationId = ResolveMutationId(state, mutationId, "enemy_defense_refund");
		if (mutationId.IsEmpty())
			return false;

		HST_EnemyStrategicMutationResult result = m_EnemyStrategicResources.RefundResources(
			state,
			m_Preset,
			mutationId,
			factionKey,
			clampedAttack,
			clampedSupport,
			"defense_support_refund",
			sourceId,
			orderId,
			operationId,
			manifestId,
			zoneId);
		if (!result || !result.m_bAccepted)
			return false;
		if (result.m_bAlreadyApplied)
			return true;
		if (ledger)
		{
			ledger.m_iRefundedAttackResources += clampedAttack;
			ledger.m_iRefundedSupportResources += clampedSupport;
			ledger.m_iAttackSpent = Math.Max(0, ledger.m_iAttackSpent - clampedAttack);
			ledger.m_iSupportSpent = Math.Max(0, ledger.m_iSupportSpent - clampedSupport);
			if (reason.IsEmpty())
				reason = "defense refund";
			ledger.m_sLastDecisionReason = string.Format("%1 | refund attack %2 support %3", reason, clampedAttack, clampedSupport);
		}
		return true;
	}

	bool AddResources(
		HST_CampaignState state,
		string factionKey,
		int attackResources,
		int supportResources,
		string mutationId = "",
		string kind = "resource_adjustment",
		string sourceId = "",
		string orderId = "",
		string operationId = "",
		string manifestId = "",
		string zoneId = "")
	{
		if (!state || !m_EnemyStrategicResources || !m_Preset)
			return false;
		mutationId = ResolveMutationId(state, mutationId, "enemy_resource_adjustment");
		if (mutationId.IsEmpty())
			return false;

		HST_EnemyStrategicMutationResult result = m_EnemyStrategicResources.ApplyResourceDelta(
			state,
			m_Preset,
			mutationId,
			factionKey,
			attackResources,
			supportResources,
			kind,
			sourceId,
			orderId,
			operationId,
			manifestId,
			zoneId);
		return result && result.m_bAccepted;
	}

	string BuildEnemyResourceReport(HST_CampaignState state, HST_CampaignPreset preset, HST_BalanceConfig balance)
	{
		if (!state || !preset || !balance)
			return "Partisan enemy resources | state/preset/balance not ready";

		string report = string.Format(
			"Partisan enemy resources | per-faction tick %1s | war %2 | attack war pct %3 | support war pct %4",
			RESOURCE_TICK_SECONDS,
			state.m_iWarLevel,
			balance.m_iEnemyAttackIncomeWarPercent,
			balance.m_iEnemySupportIncomeWarPercent
		);
		report = report + "\nspend policy | proactive attacks use attack pool | reactive defense/support uses support ledger";

		foreach (HST_FactionPoolState pool : state.m_aFactionPools)
		{
			if (!pool || !HST_FactionRelationService.IsEnemyFaction(preset, pool.m_sFactionKey))
				continue;

			int attackIncome;
			int supportIncome;
			CalculateProjectedIncomeForFaction(state, balance, pool.m_sFactionKey, attackIncome, supportIncome);
			report = report + string.Format(
				"\n%1 | current attack %2 support %3 aggression %4 | next attack +%5 support +%6 | cadence %7/%8s | revision %9",
				pool.m_sFactionKey,
				pool.m_iAttackResources,
				pool.m_iSupportResources,
				pool.m_iAggression,
				attackIncome,
				supportIncome,
				pool.m_iResourceAccumulatorSeconds,
				pool.m_iAggressionAccumulatorSeconds,
				pool.m_iStrategicRevision
			);
			report = report + string.Format(
				" | authority %1",
				pool.m_sStrategicAuthorityFailure.IsEmpty());
		}

		string ledgerReport = BuildEnemySupportLedgerReport(state, preset);
		if (!ledgerReport.IsEmpty())
			report = report + "\n" + ledgerReport;
		if (m_EnemyStrategicResources)
			report = report + "\n" + m_EnemyStrategicResources.BuildReport(
				state,
				preset);

		return report;
	}

	string BuildEnemySupportLedgerReport(HST_CampaignState state, HST_CampaignPreset preset)
	{
		if (!state || !preset)
			return "";

		int count;
		string report = "Partisan enemy support ledgers";
		foreach (HST_EnemySupportLedgerState ledger : state.m_aEnemySupportLedgers)
		{
			if (!ledger || !HST_FactionRelationService.IsEnemyFaction(preset, ledger.m_sFactionKey))
				continue;

			int effectiveAttackSpent = ResolveEffectiveAttackSpent(state, ledger);
			int effectiveSupportSpent = ResolveEffectiveSupportSpent(state, ledger);
			int effectiveDamage = ResolveRecentDamageScore(state, ledger);
			if (effectiveAttackSpent <= 0 && effectiveSupportSpent <= 0 && effectiveDamage <= 0 && ledger.m_iCooldownUntilSecond <= state.m_iElapsedSeconds && ledger.m_sLastDecisionReason.IsEmpty())
				continue;

			HST_ZoneState zone = state.FindZone(ledger.m_sZoneId);
			int maxSpend = ResolveMaxDefenseSpend(state, zone, ledger);
			int recentDamage = effectiveDamage;
			int cooldownRemaining = Math.Max(0, ledger.m_iCooldownUntilSecond - state.m_iElapsedSeconds);
			string line = string.Format(
				"\n%1/%2 | spent %3/%4 cap %5 | damage %6 | cooldown %7s | refunds %8/%9",
				ledger.m_sFactionKey,
				ledger.m_sZoneId,
				effectiveAttackSpent,
				effectiveSupportSpent,
				maxSpend,
				recentDamage,
				cooldownRemaining,
				ledger.m_iRefundedAttackResources,
				ledger.m_iRefundedSupportResources
			);
			report = report + line + string.Format(" | reason %1", ledger.m_sLastDecisionReason);
			count++;
			if (count >= 12)
				break;
		}

		if (count <= 0)
			return "Partisan enemy support ledgers | none";

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

	protected string ResolveMutationId(
		HST_CampaignState state,
		string requestedMutationId,
		string prefix)
	{
		if (!requestedMutationId.IsEmpty())
			return requestedMutationId;
		return HST_StableIdService.NextId(state, prefix);
	}

	protected HST_FactionPoolState FindAuthoritativePool(
		HST_CampaignState state,
		string factionKey)
	{
		if (!state || factionKey.IsEmpty())
			return null;
		HST_FactionPoolState found;
		int matches;
		foreach (HST_FactionPoolState pool : state.m_aFactionPools)
		{
			if (!pool || pool.m_sFactionKey != factionKey)
				continue;
			found = pool;
			matches++;
		}
		if (matches != 1 || !found || found.m_iStrategicContractVersion != 1
			|| !found.m_sStrategicAuthorityFailure.IsEmpty())
			return null;
		return found;
	}

	protected int ResolveEffectiveAttackSpent(
		HST_CampaignState state,
		HST_EnemySupportLedgerState ledger)
	{
		if (!state || !ledger)
			return 0;
		if (state.m_iElapsedSeconds - ledger.m_iLastSpendSecond > SUPPORT_SPEND_WINDOW_SECONDS)
			return 0;
		return Math.Max(0, ledger.m_iAttackSpent);
	}

	protected int ResolveEffectiveSupportSpent(
		HST_CampaignState state,
		HST_EnemySupportLedgerState ledger)
	{
		if (!state || !ledger)
			return 0;
		if (state.m_iElapsedSeconds - ledger.m_iLastSpendSecond > SUPPORT_SPEND_WINDOW_SECONDS)
			return 0;
		return Math.Max(0, ledger.m_iSupportSpent);
	}

	protected HST_EnemySupportLedgerState FindOrCreateSupportLedger(HST_CampaignState state, string factionKey, string zoneId)
	{
		if (!state || factionKey.IsEmpty() || zoneId.IsEmpty())
			return null;
		zoneId = HST_MaidensBayLocationSaveValidationService.ResolveCanonicalZoneId(zoneId);

		HST_EnemySupportLedgerState ledger;
		int ledgerCount = ResolveSupportLedgerCount(
			state,
			factionKey,
			zoneId,
			ledger);
		if (ledgerCount > 1)
			return null;
		if (ledgerCount == 1)
			return ledger;

		ledger = new HST_EnemySupportLedgerState();
		ledger.m_sFactionKey = factionKey;
		ledger.m_sZoneId = zoneId;
		state.m_aEnemySupportLedgers.Insert(ledger);
		return ledger;
	}

	protected int ResolveSupportLedgerCount(
		HST_CampaignState state,
		string factionKey,
		string zoneId,
		out HST_EnemySupportLedgerState match)
	{
		match = null;
		if (!state || factionKey.IsEmpty() || zoneId.IsEmpty())
			return 0;
		zoneId = HST_MaidensBayLocationSaveValidationService.ResolveCanonicalZoneId(zoneId);
		int count;
		foreach (HST_EnemySupportLedgerState ledger : state.m_aEnemySupportLedgers)
		{
			if (!ledger || ledger.m_sFactionKey != factionKey
				|| !HST_MaidensBayLocationSaveValidationService.AreEquivalentZoneIds(
					ledger.m_sZoneId,
					zoneId))
				continue;
			match = ledger;
			count++;
		}
		return count;
	}

	protected void UpdateLedgerWindows(HST_CampaignState state, HST_EnemySupportLedgerState ledger)
	{
		if (!state || !ledger)
			return;

		if ((ledger.m_iAttackSpent > 0 || ledger.m_iSupportSpent > 0) && state.m_iElapsedSeconds - ledger.m_iLastSpendSecond > SUPPORT_SPEND_WINDOW_SECONDS)
		{
			ledger.m_iAttackSpent = 0;
			ledger.m_iSupportSpent = 0;
		}

		if (ledger.m_iRecentDamageScore > 0 && state.m_iElapsedSeconds - ledger.m_iLastDamageSecond > RECENT_DAMAGE_WINDOW_SECONDS)
			ledger.m_iRecentDamageScore = 0;
	}

	protected int ResolveRecentDamageScore(HST_CampaignState state, HST_EnemySupportLedgerState ledger)
	{
		if (!state || !ledger)
			return 0;

		if (ledger.m_iRecentDamageScore <= 0)
			return Math.Max(0, ledger.m_iRecentDamageScore);

		int age = state.m_iElapsedSeconds - ledger.m_iLastDamageSecond;
		if (age <= 0)
			return Math.Max(0, ledger.m_iRecentDamageScore);

		if (age >= RECENT_DAMAGE_WINDOW_SECONDS)
			return 0;

		int remainingPercent = Math.Max(0, 100 - Math.Round(age * 100.0 / RECENT_DAMAGE_WINDOW_SECONDS));
		return Math.Max(0, Math.Round(ledger.m_iRecentDamageScore * remainingPercent / 100.0));
	}

	protected int ResolveMaxDefenseSpend(HST_CampaignState state, HST_ZoneState zone, HST_EnemySupportLedgerState ledger)
	{
		if (!state)
			return MAX_DEFENCE_SPEND_BASE;

		int maxSpend = MAX_DEFENCE_SPEND_BASE + Math.Max(0, state.m_iWarLevel) * MAX_DEFENCE_SPEND_PER_WAR_LEVEL;
		if (zone)
		{
			maxSpend += Math.Max(0, zone.m_iPriority / 2);
			maxSpend += Math.Max(0, zone.m_iResistanceCaptureProgress / 4);
			if (zone.m_eType == HST_EZoneType.HST_ZONE_AIRFIELD || zone.m_eType == HST_EZoneType.HST_ZONE_SEAPORT)
				maxSpend += 10;
			else if (zone.m_eType == HST_EZoneType.HST_ZONE_OUTPOST || zone.m_eType == HST_EZoneType.HST_ZONE_RADIO_TOWER)
				maxSpend += 6;
		}

		maxSpend += ResolveRecentDamageScore(state, ledger) / 3;
		return Math.Max(20, maxSpend);
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
		if (zone.m_eType == HST_EZoneType.HST_ZONE_TOWN && m_TownInfluence)
		{
			int signedSupport = m_TownInfluence.ResolveSignedSupportPercent(state, zone.m_sZoneId);
			income += Math.Max(0, 1 - signedSupport / 50);
		}

		int multiplier = 100 + state.m_iWarLevel * Math.Max(0, balance.m_iEnemySupportIncomeWarPercent);
		income = Math.Max(1, Math.Round(income * multiplier / 100.0));
		return income;
	}
}
