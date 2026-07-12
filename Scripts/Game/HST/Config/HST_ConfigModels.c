[BaseContainerProps()]
class HST_ArsenalItemRule
{
	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Prefab name or path fragment matched by this arsenal rule.", category: "Partisan Arsenal")]
	string m_sPrefabContains;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Logical arsenal category for matching items.", category: "Partisan Arsenal")]
	string m_sCategory;

	[Attribute(defvalue: "unlock", uiwidget: UIWidgets.EditBox, desc: "Unlock policy applied to matching items.", category: "Partisan Arsenal")]
	string m_sPolicy = "unlock";

	[Attribute(defvalue: "-1", uiwidget: UIWidgets.EditBox, desc: "Optional unlock threshold override; -1 uses the balance default.", category: "Partisan Arsenal")]
	int m_iUnlockThresholdOverride = -1;

	[Attribute(defvalue: "true", uiwidget: UIWidgets.CheckBox, desc: "Whether this rule applies to area loot.", params: "", category: "Partisan Arsenal")]
	bool m_bAppliesToAreaLoot = true;

	[Attribute(defvalue: "true", uiwidget: UIWidgets.CheckBox, desc: "Whether this rule applies to vehicle loot.", params: "", category: "Partisan Arsenal")]
	bool m_bAppliesToVehicleLoot = true;
}

[BaseContainerProps(configRoot: true)]
class HST_BalanceConfig
{
	[Attribute(defvalue: "900", uiwidget: UIWidgets.EditBox, desc: "Autosave interval in seconds.", category: "Partisan Balance")]
	int m_iAutosaveIntervalSeconds = 900;

	[Attribute(defvalue: "30", uiwidget: UIWidgets.EditBox, desc: "Major-change save debounce in seconds.", category: "Partisan Balance")]
	int m_iMajorChangeDebounceSeconds = 30;

	[Attribute(defvalue: "10", uiwidget: UIWidgets.EditBox, desc: "Starting HR.", category: "Partisan Balance")]
	int m_iStartingHR = 10;

	[Attribute(defvalue: "750", uiwidget: UIWidgets.EditBox, desc: "Starting FIA money.", category: "Partisan Balance")]
	int m_iStartingFactionMoney = 750;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.EditBox, desc: "Starting FIA training level.", category: "Partisan Balance")]
	int m_iStartingTrainingLevel = 1;

	[Attribute(defvalue: "70", uiwidget: UIWidgets.EditBox, desc: "Starting occupier attack resource pool.", category: "Partisan Balance")]
	int m_iStartingOccupierAttackPool = 70;

	[Attribute(defvalue: "80", uiwidget: UIWidgets.EditBox, desc: "Starting occupier support resource pool.", category: "Partisan Balance")]
	int m_iStartingOccupierSupportPool = 80;

	[Attribute(defvalue: "35", uiwidget: UIWidgets.EditBox, desc: "Starting invader attack resource pool.", category: "Partisan Balance")]
	int m_iStartingInvaderAttackPool = 35;

	[Attribute(defvalue: "45", uiwidget: UIWidgets.EditBox, desc: "Starting invader support resource pool.", category: "Partisan Balance")]
	int m_iStartingInvaderSupportPool = 45;

	[Attribute(defvalue: "600", uiwidget: UIWidgets.EditBox, desc: "Zone income interval in seconds.", category: "Partisan Balance")]
	int m_iZoneIncomeIntervalSeconds = 600;

	[Attribute(defvalue: "3600", uiwidget: UIWidgets.EditBox, desc: "Default mission duration in seconds.", category: "Partisan Balance")]
	int m_iMissionDefaultDurationSeconds = 3600;

	[Attribute(defvalue: "1200", uiwidget: UIWidgets.EditBox, desc: "Physical activation radius in meters.", category: "Partisan Balance")]
	int m_iActivationRadiusMeters = 1200;

	[Attribute(defvalue: "1600", uiwidget: UIWidgets.EditBox, desc: "Physical deactivation radius in meters.", category: "Partisan Balance")]
	int m_iDeactivationRadiusMeters = 1600;

	[Attribute(defvalue: "1800", uiwidget: UIWidgets.EditBox, desc: "Player event/render bubble radius in meters.", category: "Partisan Balance")]
	int m_iPlayerRenderBubbleRadiusMeters = 1800;

	[Attribute(defvalue: "1800", uiwidget: UIWidgets.EditBox, desc: "Mission category target-selection radius in meters.", category: "Partisan Balance")]
	int m_iMissionSelectionRadiusMeters = 1800;

	[Attribute(defvalue: "18", uiwidget: UIWidgets.EditBox, desc: "Default arsenal unlock threshold.", category: "Partisan Loot")]
	int m_iArsenalUnlockThreshold = 18;

	[Attribute(defvalue: "3", uiwidget: UIWidgets.EditBox, desc: "Magazine unlock multiplier.", category: "Partisan Loot")]
	int m_iMagazineUnlockMultiplier = 3;

	[Attribute(defvalue: "50", uiwidget: UIWidgets.EditBox, desc: "HQ interaction radius in meters.", category: "Partisan Loot")]
	int m_iHQInteractionRadiusMeters = 50;

	[Attribute(defvalue: "15", uiwidget: UIWidgets.EditBox, desc: "Area loot radius in meters.", category: "Partisan Loot")]
	int m_iLootRadiusMeters = 15;

	[Attribute(defvalue: "true", uiwidget: UIWidgets.CheckBox, desc: "Skip area-loot items that are already unlimited in the arsenal.", params: "", category: "Partisan Loot")]
	bool m_bLootSkipUnlockedItems = true;

	[Attribute(defvalue: "true", uiwidget: UIWidgets.CheckBox, desc: "Remove collected area loot from the source.", params: "", category: "Partisan Loot")]
	bool m_bRemoveLootedItems = true;

	[Attribute(defvalue: "true", uiwidget: UIWidgets.CheckBox, desc: "Allow explosives to unlock in the arsenal.", params: "", category: "Partisan Loot")]
	bool m_bAllowExplosiveUnlocks = true;

	[Attribute(defvalue: "true", uiwidget: UIWidgets.CheckBox, desc: "Allow guided launchers to unlock in the arsenal.", params: "", category: "Partisan Loot")]
	bool m_bAllowGuidedLauncherUnlocks = true;

	[Attribute(defvalue: "true", uiwidget: UIWidgets.CheckBox, desc: "Enable vehicle loot collection.", params: "", category: "Partisan Vehicle Loot")]
	bool m_bVehicleLootEnabled = true;

	[Attribute(defvalue: "20", uiwidget: UIWidgets.EditBox, desc: "Vehicle loot radius in meters.", category: "Partisan Vehicle Loot")]
	int m_iVehicleLootRadiusMeters = 20;

	[Attribute(defvalue: "true", uiwidget: UIWidgets.CheckBox, desc: "Skip vehicle-loot items that are already unlimited in the arsenal.", params: "", category: "Partisan Vehicle Loot")]
	bool m_bVehicleLootSkipUnlockedItems = true;

	[Attribute(defvalue: "true", uiwidget: UIWidgets.CheckBox, desc: "Remove collected vehicle loot from the source.", params: "", category: "Partisan Vehicle Loot")]
	bool m_bVehicleLootRemoveSource = true;

	[Attribute(defvalue: "48", uiwidget: UIWidgets.EditBox, desc: "Maximum vehicle loot items collected per action.", category: "Partisan Vehicle Loot")]
	int m_iVehicleLootMaxItemsPerAction = 48;

	[Attribute(defvalue: "true", uiwidget: UIWidgets.CheckBox, desc: "Enable air support.", params: "", category: "Partisan Support")]
	bool m_bAirSupportEnabled = true;

	[Attribute(defvalue: "900", uiwidget: UIWidgets.EditBox, desc: "Air support cooldown in seconds.", category: "Partisan Support")]
	int m_iAirSupportCooldownSeconds = 900;

	[Attribute(defvalue: "true", uiwidget: UIWidgets.CheckBox, desc: "Enable civilian population simulation.", params: "", category: "Partisan Civilians")]
	bool m_bCivilianPopulationEnabled = true;

	[Attribute(defvalue: "12", uiwidget: UIWidgets.EditBox, desc: "Maximum active civilians per town (0-32).", category: "Partisan Civilians")]
	int m_iCivilianMaxActivePerTown = 12;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.EditBox, desc: "Minimum civilian vehicles per town.", category: "Partisan Civilians")]
	int m_iCivilianVehicleMinPerTown = 1;

	[Attribute(defvalue: "5", uiwidget: UIWidgets.EditBox, desc: "Maximum civilian vehicles per town.", category: "Partisan Civilians")]
	int m_iCivilianVehicleMaxPerTown = 5;

	[Attribute(defvalue: "5", uiwidget: UIWidgets.EditBox, desc: "Number of active civilian-driven ambient traffic vehicles per active true town.", category: "Partisan Civilians")]
	int m_iCivilianDrivingVehicleCountPerTown = 5;

	[Attribute(defvalue: "48", uiwidget: UIWidgets.EditBox, desc: "Base global budget for physically projected civilian actors (0-256).", category: "Partisan Civilians")]
	int m_iCivilianGlobalActorBudgetBase = 48;

	[Attribute(defvalue: "12", uiwidget: UIWidgets.EditBox, desc: "Additional global civilian actor budget per connected player (0-64).", category: "Partisan Civilians")]
	int m_iCivilianGlobalActorBudgetPerPlayer = 12;

	[Attribute(defvalue: "10", uiwidget: UIWidgets.EditBox, desc: "Base global budget for physically projected civilian traffic vehicles (0-64).", category: "Partisan Civilians")]
	int m_iCivilianGlobalTrafficBudgetBase = 10;

	[Attribute(defvalue: "2", uiwidget: UIWidgets.EditBox, desc: "Additional global civilian traffic budget per connected player (0-16).", category: "Partisan Civilians")]
	int m_iCivilianGlobalTrafficBudgetPerPlayer = 2;

	[Attribute(defvalue: "4", uiwidget: UIWidgets.EditBox, desc: "Percent removed from civilian runtime budgets for each war level above one (0-8).", category: "Partisan Civilians")]
	int m_iCivilianWarLevelBudgetPenaltyPercent = 4;

	[Attribute(defvalue: "5", uiwidget: UIWidgets.EditBox, desc: "Seconds between civilian runtime health and movement checks (2-30).", category: "Partisan Civilians")]
	int m_iCivilianRuntimeHealthIntervalSeconds = 5;

	[Attribute(defvalue: "15", uiwidget: UIWidgets.EditBox, desc: "Grace seconds after civilian actor admission before movement is required (health interval to 120).", category: "Partisan Civilians")]
	int m_iCivilianRuntimeStartupGraceSeconds = 15;

	[Attribute(defvalue: "30", uiwidget: UIWidgets.EditBox, desc: "Seconds without meaningful movement before a civilian runtime actor is considered stuck (health interval to 300).", category: "Partisan Civilians")]
	int m_iCivilianRuntimeStuckSeconds = 30;

	[Attribute(defvalue: "2", uiwidget: UIWidgets.EditBox, desc: "Maximum bounded recovery attempts before a stuck civilian runtime actor is recycled (0-4).", category: "Partisan Civilians")]
	int m_iCivilianRuntimeMaxRecoveryAttempts = 2;

	[Attribute(defvalue: "20", uiwidget: UIWidgets.EditBox, desc: "Seconds between civilian runtime recovery attempts (health interval to 120).", category: "Partisan Civilians")]
	int m_iCivilianRuntimeRetryBackoffSeconds = 20;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.EditBox, desc: "Minimum occupier vehicles per town.", category: "Partisan Civilians")]
	int m_iOccupierVehicleMinPerTown;

	[Attribute(defvalue: "2", uiwidget: UIWidgets.EditBox, desc: "Maximum occupier vehicles per town.", category: "Partisan Civilians")]
	int m_iOccupierVehicleMaxPerTown = 2;

	[Attribute(defvalue: "10", uiwidget: UIWidgets.EditBox, desc: "Maximum war level.", category: "Partisan Campaign")]
	int m_iWarLevelMaximum = 10;

	[Attribute(defvalue: "18", uiwidget: UIWidgets.EditBox, desc: "Owned-zone score needed for war level 2.", category: "Partisan Campaign")]
	int m_iWarLevel2Score = 18;

	[Attribute(defvalue: "38", uiwidget: UIWidgets.EditBox, desc: "Owned-zone score needed for war level 3.", category: "Partisan Campaign")]
	int m_iWarLevel3Score = 38;

	[Attribute(defvalue: "65", uiwidget: UIWidgets.EditBox, desc: "Owned-zone score needed for war level 4.", category: "Partisan Campaign")]
	int m_iWarLevel4Score = 65;

	[Attribute(defvalue: "95", uiwidget: UIWidgets.EditBox, desc: "Owned-zone score needed for war level 5.", category: "Partisan Campaign")]
	int m_iWarLevel5Score = 95;

	[Attribute(defvalue: "130", uiwidget: UIWidgets.EditBox, desc: "Owned-zone score needed for war level 6.", category: "Partisan Campaign")]
	int m_iWarLevel6Score = 130;

	[Attribute(defvalue: "170", uiwidget: UIWidgets.EditBox, desc: "Owned-zone score needed for war level 7.", category: "Partisan Campaign")]
	int m_iWarLevel7Score = 170;

	[Attribute(defvalue: "215", uiwidget: UIWidgets.EditBox, desc: "Owned-zone score needed for war level 8.", category: "Partisan Campaign")]
	int m_iWarLevel8Score = 215;

	[Attribute(defvalue: "265", uiwidget: UIWidgets.EditBox, desc: "Owned-zone score needed for war level 9.", category: "Partisan Campaign")]
	int m_iWarLevel9Score = 265;

	[Attribute(defvalue: "320", uiwidget: UIWidgets.EditBox, desc: "Owned-zone score needed for war level 10.", category: "Partisan Campaign")]
	int m_iWarLevel10Score = 320;

	[Attribute(defvalue: "70", uiwidget: UIWidgets.EditBox, desc: "Strategic control percent needed for victory.", category: "Partisan Campaign")]
	int m_iVictoryControlPercent = 70;

	[Attribute(defvalue: "true", uiwidget: UIWidgets.CheckBox, desc: "Use population support plus airfield control as the default campaign outcome.", params: "", category: "Partisan Campaign")]
	bool m_bPopulationOutcomeEnabled = true;

	[Attribute(defvalue: "50", uiwidget: UIWidgets.EditBox, desc: "Percent of remaining population support needed for population victory.", category: "Partisan Campaign")]
	int m_iVictoryPopulationSupportPercent = 50;

	[Attribute(defvalue: "false", uiwidget: UIWidgets.CheckBox, desc: "Allow legacy strategic-control victory mode when population outcome is disabled.", params: "", category: "Partisan Campaign")]
	bool m_bLegacyControlVictoryEnabled;

	[Attribute(defvalue: "true", uiwidget: UIWidgets.CheckBox, desc: "Require all airfields for victory.", params: "", category: "Partisan Campaign")]
	bool m_bVictoryRequiresAirfields = true;

	[Attribute(defvalue: "true", uiwidget: UIWidgets.CheckBox, desc: "Require all seaports for victory.", params: "", category: "Partisan Campaign")]
	bool m_bVictoryRequiresSeaports = true;

	[Attribute(defvalue: "true", uiwidget: UIWidgets.CheckBox, desc: "Enable campaign loss condition.", params: "", category: "Partisan Campaign")]
	bool m_bLossConditionEnabled = true;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.EditBox, desc: "HR threshold for loss check.", category: "Partisan Campaign")]
	int m_iLossHRThreshold;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.EditBox, desc: "Money threshold for loss check.", category: "Partisan Campaign")]
	int m_iLossMoneyThreshold;

	[Attribute(defvalue: "3", uiwidget: UIWidgets.EditBox, desc: "Petros deaths before campaign loss.", category: "Partisan Campaign")]
	int m_iLossPetrosDeathLimit = 3;

	[Attribute(defvalue: "7200", uiwidget: UIWidgets.EditBox, desc: "Grace period before economic loss can trigger.", category: "Partisan Campaign")]
	int m_iLossGraceSeconds = 7200;

	[Attribute(defvalue: "8", uiwidget: UIWidgets.EditBox, desc: "Enemy attack income multiplier percent per war level.", category: "Partisan Balance")]
	int m_iEnemyAttackIncomeWarPercent = 8;

	[Attribute(defvalue: "6", uiwidget: UIWidgets.EditBox, desc: "Enemy support income multiplier percent per war level.", category: "Partisan Balance")]
	int m_iEnemySupportIncomeWarPercent = 6;

	[Attribute(defvalue: "300", uiwidget: UIWidgets.EditBox, desc: "Aggression decay interval in seconds.", category: "Partisan Balance")]
	int m_iAggressionDecayIntervalSeconds = 300;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.EditBox, desc: "Aggression decay amount per interval.", category: "Partisan Balance")]
	int m_iAggressionDecayAmount = 1;

	[Attribute(defvalue: "100", uiwidget: UIWidgets.EditBox, desc: "Capture progress required to flip a zone.", category: "Partisan Capture")]
	int m_iCaptureProgressRequired = 100;

	[Attribute(defvalue: "2", uiwidget: UIWidgets.EditBox, desc: "Capture progress gained per second.", category: "Partisan Capture")]
	int m_iCaptureProgressPerSecond = 2;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.EditBox, desc: "Capture progress decay per second.", category: "Partisan Capture")]
	int m_iCaptureDecayPerSecond = 1;

	[Attribute(defvalue: "10", uiwidget: UIWidgets.EditBox, desc: "Aggression added by capture.", category: "Partisan Capture")]
	int m_iCaptureAggressionBase = 10;

	[Attribute(defvalue: "45", uiwidget: UIWidgets.EditBox, desc: "Counterattack chance after capture, in percent.", category: "Partisan Capture")]
	int m_iCaptureCounterattackChancePercent = 45;

	[Attribute(defvalue: "30", uiwidget: UIWidgets.EditBox, desc: "Seconds a combat area remains cooling after its last verified contributor disappears.", category: "Partisan Capture")]
	int m_iCombatPresenceCoolingSeconds = 30;

	[Attribute(desc: "Arsenal item classification and unlock rules.", category: "Partisan Loot")]
	ref array<ref HST_ArsenalItemRule> m_aArsenalItemRules = {};

	[Attribute(desc: "Civilian character prefab resources.", category: "Partisan Civilians")]
	ref array<string> m_aCivilianCharacterPrefabs = {};

	[Attribute(desc: "Civilian vehicle prefab resources.", category: "Partisan Civilians")]
	ref array<string> m_aCivilianVehiclePrefabs = {};
}

[BaseContainerProps()]
class HST_PrefabPoolEntry
{
	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Prefab resource.", category: "Partisan Prefab Pool")]
	string m_sPrefab;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.EditBox, desc: "Selection weight.", category: "Partisan Prefab Pool")]
	int m_iWeight = 1;

	[Attribute(desc: "Tags for filtering this prefab pool entry.", category: "Partisan Prefab Pool")]
	ref array<string> m_aTags = {};

	bool HasTag(string tag)
	{
		return !tag.IsEmpty() && m_aTags.Contains(tag);
	}
}

[BaseContainerProps()]
class HST_FactionTemplate
{
	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Template identifier.", category: "Partisan Faction")]
	string m_sTemplateId;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Faction key.", category: "Partisan Faction")]
	string m_sFactionKey;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Display name.", category: "Partisan Faction")]
	string m_sDisplayName;

	[Attribute(defvalue: "false", uiwidget: UIWidgets.CheckBox, desc: "Whether this faction can be selected by players.", params: "", category: "Partisan Faction")]
	bool m_bPlayable;

	[Attribute(desc: "Faction capability ids.", category: "Partisan Faction")]
	ref array<string> m_aCapabilities = {};

	[Attribute(desc: "Infantry character prefab resources.", category: "Partisan Faction")]
	ref array<string> m_aInfantryPrefabs = {};

	[Attribute(desc: "Vehicle prefab resources.", category: "Partisan Faction")]
	ref array<string> m_aVehiclePrefabs = {};

	[Attribute(desc: "Group prefab resources.", category: "Partisan Faction")]
	ref array<string> m_aGroupPrefabs = {};

	[Attribute(desc: "Patrol group prefab resources.", category: "Partisan Faction")]
	ref array<string> m_aPatrolGroupPrefabs = {};

	[Attribute(desc: "QRF group prefab resources.", category: "Partisan Faction")]
	ref array<string> m_aQRFGroupPrefabs = {};

	[Attribute(desc: "Support ids available to this faction.", category: "Partisan Faction")]
	ref array<string> m_aSupportIds = {};

	[Attribute(desc: "Weighted group prefab pool.", category: "Partisan Faction")]
	ref array<ref HST_PrefabPoolEntry> m_aGroupPool = {};

	[Attribute(desc: "Weighted patrol group prefab pool.", category: "Partisan Faction")]
	ref array<ref HST_PrefabPoolEntry> m_aPatrolGroupPool = {};

	[Attribute(desc: "Weighted QRF group prefab pool.", category: "Partisan Faction")]
	ref array<ref HST_PrefabPoolEntry> m_aQRFGroupPool = {};

	[Attribute(desc: "Weighted rare group prefab pool.", category: "Partisan Faction")]
	ref array<ref HST_PrefabPoolEntry> m_aRareGroupPool = {};

	bool HasCapability(string capabilityId)
	{
		return m_aCapabilities.Contains(capabilityId);
	}
}

[BaseContainerProps()]
class HST_FactionRuntimeSpawnSpec
{
	string m_sFactionKey;
	string m_sGroupPrefab;
	string m_sExpectedNativeFactionKey;
	string m_sSourceCatalog;
	string m_sRole;
	string m_sSourceContext;
	ref array<string> m_aDirectInfantryPrefabs = {};
	ref array<string> m_aVehiclePrefabs = {};
	ref array<string> m_aGroupPrefabs = {};
}

[BaseContainerProps()]
class HST_ZoneDefinition
{
	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Zone identifier.", category: "Partisan Zone")]
	string m_sZoneId;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Display name.", category: "Partisan Zone")]
	string m_sDisplayName;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Source layout identifier.", category: "Partisan Zone")]
	string m_sSourceLayoutId;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Source layer name.", category: "Partisan Zone")]
	string m_sSourceLayerName;

	[Attribute(defvalue: "HST_ZONE_TOWN", uiwidget: UIWidgets.ComboBox, desc: "Strategic zone type.", params: "", enums: ParamEnumArray.FromEnum(HST_EZoneType), category: "Partisan Zone")]
	HST_EZoneType m_eType;

	[Attribute(defvalue: "0 0 0", desc: "World position.", category: "Partisan Zone")]
	vector m_vPosition;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Initial owner faction key.", category: "Partisan Zone")]
	string m_sInitialOwnerFactionKey;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.EditBox, desc: "Income value.", category: "Partisan Zone")]
	int m_iIncomeValue;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Resource kind.", category: "Partisan Zone")]
	string m_sResourceKind;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.EditBox, desc: "Capture radius in meters.", category: "Partisan Zone")]
	int m_iCaptureRadiusMeters;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.EditBox, desc: "Strategic priority.", category: "Partisan Zone")]
	int m_iPriority;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Composition identifier.", category: "Partisan Zone")]
	string m_sCompositionId;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Spawn profile identifier.", category: "Partisan Zone")]
	string m_sSpawnProfileId;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.EditBox, desc: "Town support value.", category: "Partisan Zone")]
	int m_iSupport;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.EditBox, desc: "Garrison slot count.", category: "Partisan Zone")]
	int m_iGarrisonSlots;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.EditBox, desc: "Zone activation radius in meters.", category: "Partisan Zone")]
	int m_iActivationRadiusMeters;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Patrol route identifier.", category: "Partisan Zone")]
	string m_sPatrolRouteId;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "QRF route identifier.", category: "Partisan Zone")]
	string m_sQRFRouteId;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Mission site identifier.", category: "Partisan Zone")]
	string m_sMissionSiteId;

	[Attribute(defvalue: "true", uiwidget: UIWidgets.CheckBox, desc: "Show this zone on the map.", params: "", category: "Partisan Zone Marker")]
	bool m_bShowMapMarker = true;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Map marker label.", category: "Partisan Zone Marker")]
	string m_sMarkerLabel;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Map marker callsign.", category: "Partisan Zone Marker")]
	string m_sMarkerCallsign;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Map marker icon id.", category: "Partisan Zone Marker")]
	string m_sMarkerIcon;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Map marker color id.", category: "Partisan Zone Marker")]
	string m_sMarkerColor;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Map marker text color id.", category: "Partisan Zone Marker")]
	string m_sMarkerTextColor;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Map marker style id.", category: "Partisan Zone Marker")]
	string m_sMarkerStyle;

	[Attribute(desc: "Nearby town zone ids.", category: "Partisan Zone")]
	ref array<string> m_aNearbyTownIds = {};

	[Attribute(desc: "Linked strategic zone ids.", category: "Partisan Zone")]
	ref array<string> m_aLinkedZoneIds = {};

	[Attribute(desc: "Zone capability ids.", category: "Partisan Zone")]
	ref array<string> m_aCapabilities = {};
}

[BaseContainerProps()]
class HST_HideoutDefinition
{
	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Hideout identifier.", category: "Partisan Hideout")]
	string m_sHideoutId;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Display name.", category: "Partisan Hideout")]
	string m_sDisplayName;

	[Attribute(defvalue: "0 0 0", desc: "World position.", category: "Partisan Hideout")]
	vector m_vPosition;
}

[BaseContainerProps(configRoot: true)]
class HST_MapDefinition
{
	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Map identifier.", category: "Partisan Map")]
	string m_sMapId;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Display name.", category: "Partisan Map")]
	string m_sDisplayName;

	[Attribute(defvalue: "0 0 0", desc: "World-space map minimum bounds.", category: "Partisan Map")]
	vector m_vWorldMin;

	[Attribute(defvalue: "12800 0 12800", desc: "World-space map maximum bounds.", category: "Partisan Map")]
	vector m_vWorldMax;

	[Attribute(desc: "Strategic zone definitions.", category: "Partisan Map")]
	ref array<ref HST_ZoneDefinition> m_aZones = {};

	[Attribute(desc: "Resistance hideout definitions.", category: "Partisan Map")]
	ref array<ref HST_HideoutDefinition> m_aHideouts = {};
}

[BaseContainerProps()]
class HST_MissionDefinition
{
	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Mission identifier.", category: "Partisan Mission")]
	string m_sMissionId;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Display name.", category: "Partisan Mission")]
	string m_sDisplayName;

	[Attribute(defvalue: "HST_MISSION_ASSASSINATION", uiwidget: UIWidgets.ComboBox, desc: "Mission category.", params: "", enums: ParamEnumArray.FromEnum(HST_EMissionCategory), category: "Partisan Mission")]
	HST_EMissionCategory m_eCategory;

	[Attribute(defvalue: "3600", uiwidget: UIWidgets.EditBox, desc: "Duration in seconds.", category: "Partisan Mission")]
	int m_iDurationSeconds = 3600;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.EditBox, desc: "Money reward.", category: "Partisan Mission Rewards")]
	int m_iRewardMoney;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.EditBox, desc: "HR reward.", category: "Partisan Mission Rewards")]
	int m_iRewardHR;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.EditBox, desc: "Aggression added on failure.", category: "Partisan Mission")]
	int m_iFailureAggression;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Briefing text.", category: "Partisan Mission Text")]
	string m_sBriefingText;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Requirement text.", category: "Partisan Mission Text")]
	string m_sRequirementText;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Failure text.", category: "Partisan Mission Text")]
	string m_sFailureText;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Runtime type id.", category: "Partisan Mission Runtime")]
	string m_sRuntimeType;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Preferred target site type.", category: "Partisan Mission Runtime")]
	string m_sTargetSitePreference;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Reward text.", category: "Partisan Mission Text")]
	string m_sRewardText;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Failure penalty text.", category: "Partisan Mission Text")]
	string m_sFailurePenaltyText;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.EditBox, desc: "Town support reward.", category: "Partisan Mission Rewards")]
	int m_iRewardSupport;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.EditBox, desc: "Capture progress reward.", category: "Partisan Mission Rewards")]
	int m_iRewardCaptureProgress;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.EditBox, desc: "Required cargo count.", category: "Partisan Mission Runtime")]
	int m_iCargoCount;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.EditBox, desc: "Required captive count.", category: "Partisan Mission Runtime")]
	int m_iCaptiveCount;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.EditBox, desc: "Vehicle count.", category: "Partisan Mission Runtime")]
	int m_iVehicleCount;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.EditBox, desc: "Required war level.", category: "Partisan Mission")]
	int m_iRequiredWarLevel = 1;

	[Attribute(defvalue: "true", uiwidget: UIWidgets.CheckBox, desc: "Allow hostile target zones.", params: "", category: "Partisan Mission Targeting")]
	bool m_bAllowEnemyTarget = true;

	[Attribute(defvalue: "false", uiwidget: UIWidgets.CheckBox, desc: "Allow friendly target zones.", params: "", category: "Partisan Mission Targeting")]
	bool m_bAllowFriendlyTarget;

	[Attribute(desc: "Capabilities required by this mission.", category: "Partisan Mission Targeting")]
	ref array<string> m_aRequiredCapabilities = {};

	[Attribute(desc: "Allowed target zone type ids.", category: "Partisan Mission Targeting")]
	ref array<string> m_aTargetZoneTypes = {};
}

[BaseContainerProps(configRoot: true)]
class HST_MissionRegistryConfig
{
	[Attribute(desc: "Mission definitions available to the campaign.", category: "Partisan Missions")]
	ref array<ref HST_MissionDefinition> m_aMissions = {};
}

[BaseContainerProps(configRoot: true)]
class HST_CampaignPreset
{
	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Campaign preset identifier.", category: "Partisan Preset")]
	string m_sPresetId;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Display name.", category: "Partisan Preset")]
	string m_sDisplayName;

	[Attribute(defvalue: "FIA", uiwidget: UIWidgets.EditBox, desc: "Resistance faction key.", category: "Partisan Preset")]
	string m_sResistanceFactionKey;

	[Attribute(defvalue: "US", uiwidget: UIWidgets.EditBox, desc: "Occupier faction key.", category: "Partisan Preset")]
	string m_sOccupierFactionKey;

	[Attribute(defvalue: "USSR", uiwidget: UIWidgets.EditBox, desc: "Invader faction key.", category: "Partisan Preset")]
	string m_sInvaderFactionKey;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Balance config resource.", category: "Partisan Preset")]
	string m_sBalanceConfig;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Map definition config resource.", category: "Partisan Preset")]
	string m_sMapDefinition;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Mission registry config resource.", category: "Partisan Preset")]
	string m_sMissionRegistry;

	[Attribute(desc: "Capabilities unavailable in this preset.", category: "Partisan Preset")]
	ref array<string> m_aUnavailableCapabilities = {};

	bool HasCapability(string capabilityId)
	{
		return !m_aUnavailableCapabilities.Contains(capabilityId);
	}
}
