class HST_CommandUIService
{
	string BuildMemberMenu(HST_CampaignState state, HST_CampaignPreset preset, HST_MapMarkerService markers)
	{
		if (!state)
			return "h-istasi command | campaign state not ready";

		string header = string.Format("h-istasi command | money %1 | HR %2 | war level %3 | training %4", state.m_iFactionMoney, state.m_iHR, state.m_iWarLevel, state.m_iTrainingLevel);
		string hq = string.Format("\nHQ: %1 at %2 | Petros alive %3", state.m_sHQHideoutId, state.m_vHQPosition, state.m_bPetrosAlive);
		string markerSummary;
		if (markers)
			markerSummary = "\n" + markers.BuildMarkerReport(state);

		return header + hq + markerSummary + "\nMember actions: inspect_campaign, inspect_markers, inspect_economy, inspect_zones, inspect_missions, checkpoint";
	}

	string BuildCommanderMenu(HST_CampaignState state, HST_CampaignPreset preset, HST_MapMarkerService markers)
	{
		string menu = BuildMemberMenu(state, preset, markers);
		return menu + "\nCommander actions: move_hq <hideout>, income_now, train_troops, recruit_zone <zone>, mission_zone <zone>, complete_mission <id>";
	}

	string BuildAdminMenu(HST_CampaignState state, HST_CampaignPreset preset, HST_MapMarkerService markers)
	{
		string menu = BuildCommanderMenu(state, preset, markers);
		return menu + "\nAdmin actions: activate_zone <zone>, deactivate_zone <zone>, capture_zone <zone>, progress_zone <zone>, debug_mission <zone>, award_small";
	}

	string BuildEconomyReport(HST_CampaignState state)
	{
		if (!state)
			return "h-istasi economy | campaign state not ready";

		string summary = string.Format("h-istasi economy | money %1 | HR %2 | training %3 | income timer %4s", state.m_iFactionMoney, state.m_iHR, state.m_iTrainingLevel, state.m_iIncomeAccumulatorSeconds);
		string enemy = "";
		foreach (HST_FactionPoolState pool : state.m_aFactionPools)
		{
			if (!pool)
				continue;

			enemy = enemy + string.Format("\n%1 | attack %2 | support %3 | aggression %4", pool.m_sFactionKey, pool.m_iAttackResources, pool.m_iSupportResources, pool.m_iAggression);
		}

		return summary + enemy;
	}

	string BuildZoneListReport(HST_CampaignState state, HST_CampaignPreset preset)
	{
		if (!state)
			return "h-istasi zones | campaign state not ready";

		int resistanceZones;
		int enemyZones;
		string details = "";
		foreach (HST_ZoneState zone : state.m_aZones)
		{
			if (!zone)
				continue;

			if (preset && zone.m_sOwnerFactionKey == preset.m_sResistanceFactionKey)
				resistanceZones++;
			else
				enemyZones++;

			details = details + string.Format("\n%1 | owner %2 | support %3 | income %4 | active %5 | capture %6", zone.m_sZoneId, zone.m_sOwnerFactionKey, zone.m_iSupport, zone.m_iIncomeValue, zone.m_bActive, zone.m_iResistanceCaptureProgress);
		}

		string header = string.Format("h-istasi zones | resistance %1 | enemy %2", resistanceZones, enemyZones);
		return header + details;
	}

	string BuildMissionReport(HST_CampaignState state)
	{
		if (!state)
			return "h-istasi missions | campaign state not ready";

		string report = string.Format("h-istasi missions | active records %1", state.m_aActiveMissions.Count());
		foreach (HST_ActiveMissionState mission : state.m_aActiveMissions)
		{
			if (!mission)
				continue;

			report = report + string.Format("\n%1 | %2 | target %3 | status %4 | remaining %5s", mission.m_sInstanceId, mission.m_sMissionId, mission.m_sTargetZoneId, mission.m_eStatus, mission.m_iRemainingSeconds);
		}

		return report;
	}

	bool ExecuteQuickCommand(HST_CampaignCoordinatorComponent coordinator, int playerId, string commandId, string argument = "")
	{
		if (!coordinator || commandId.IsEmpty())
			return false;

		if (commandId == "checkpoint")
			return coordinator.RequestMemberManualCheckpoint(playerId);

		if (commandId == "inspect_campaign")
			return !coordinator.RequestMemberInspectCampaign(playerId).IsEmpty();

		if (commandId == "inspect_markers")
			return !coordinator.RequestMemberInspectMarkers(playerId).IsEmpty();

		if (commandId == "inspect_economy")
			return !coordinator.RequestMemberInspectEconomy(playerId).IsEmpty();

		if (commandId == "inspect_zones")
			return !coordinator.RequestMemberInspectZones(playerId).IsEmpty();

		if (commandId == "inspect_missions")
			return !coordinator.RequestMemberInspectMissions(playerId).IsEmpty();

		if (commandId == "move_hq")
			return coordinator.RequestCommanderMoveHQ(playerId, argument);

		if (commandId == "income_now")
			return coordinator.RequestCommanderApplyIncomeNow(playerId);

		if (commandId == "train_troops")
			return coordinator.RequestCommanderTrainTroops(playerId);

		if (commandId == "recruit_zone")
			return coordinator.RequestCommanderRecruitGarrison(playerId, argument, 2, 0, 100, 1);

		if (commandId == "mission_zone")
			return coordinator.RequestCommanderStartZoneMission(playerId, argument);

		if (commandId == "complete_mission")
			return coordinator.RequestCommanderCompleteMission(playerId, argument);

		if (commandId == "activate_zone")
			return coordinator.RequestAdminSetZoneActive(playerId, argument, true);

		if (commandId == "deactivate_zone")
			return coordinator.RequestAdminSetZoneActive(playerId, argument, false);

		if (commandId == "capture_zone")
			return coordinator.RequestAdminCaptureZoneForResistance(playerId, argument, 10);

		if (commandId == "progress_zone")
			return coordinator.RequestAdminAddCaptureProgress(playerId, argument, 50);

		if (commandId == "debug_mission")
			return coordinator.RequestAdminStartDebugMission(playerId, argument);

		if (commandId == "award_small")
			return coordinator.RequestAdminAwardResources(playerId, 500, 5);

		return false;
	}
}
