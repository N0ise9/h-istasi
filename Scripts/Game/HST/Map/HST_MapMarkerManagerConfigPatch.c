[BaseContainerProps(), SCR_MapMarkerIconEntryTitle()]
modded class SCR_MarkerIconEntry
{
	void HST_SetIconResource(ResourceName imageset, ResourceName imagesetGlow, string imageQuad, string categoryIdentifier = "general")
	{
		m_sCategoryIdentifier = categoryIdentifier;
		m_sIconImageset = imageset;
		m_sIconGlowImageset = imagesetGlow;
		m_sIconImagesetQuad = imageQuad;
	}
}

[BaseContainerProps(), SCR_MapMarkerTitle()]
modded class SCR_MapMarkerEntryPlaced
{
	static const ResourceName HST_RADIO_SIGNAL_IMAGESET = "{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset";
	static const ResourceName HST_RADIO_SIGNAL_GLOW_IMAGESET = "{00FE3DBDFD15227B}UI/Textures/Icons/icons_wrapperUI-glow.imageset";
	static const string HST_RADIO_SIGNAL_QUAD = "radio-signal";

	int HST_EnsureRadioSignalIconEntry()
	{
		return HST_EnsureIconEntry(HST_RADIO_SIGNAL_IMAGESET, HST_RADIO_SIGNAL_GLOW_IMAGESET, HST_RADIO_SIGNAL_QUAD, "general");
	}

	bool HST_IsIconEntryValid(int iconEntry)
	{
		if (!m_aPlacedMarkerIcons || !m_aPlacedMarkerIcons.IsIndexValid(iconEntry))
			return false;

		ResourceName imageset;
		ResourceName glowImageset;
		string imageQuad;
		if (!GetIconEntry(iconEntry, imageset, glowImageset, imageQuad))
			return false;

		return !imageset.IsEmpty() && !imageQuad.IsEmpty();
	}

	int HST_EnsureIconEntry(ResourceName imageset, ResourceName imagesetGlow, string imageQuad, string categoryIdentifier)
	{
		if (imageset.IsEmpty() || imageQuad.IsEmpty())
			return -1;

		if (!m_aPlacedMarkerIcons)
			m_aPlacedMarkerIcons = {};

		ResourceName existingImageset;
		ResourceName existingGlow;
		string existingQuad;
		foreach (int i, SCR_MarkerIconEntry existingEntry : m_aPlacedMarkerIcons)
		{
			if (!existingEntry)
				continue;

			existingEntry.GetIconResource(existingImageset, existingGlow, existingQuad);
			if (existingImageset == imageset && existingQuad == imageQuad)
			{
				// Repair legacy/in-memory entries as well as finding them. In
				// particular, the radio entry used to be appended without a glow
				// imageset and must not remain half-configured after a hot reload.
				existingEntry.HST_SetIconResource(imageset, imagesetGlow, imageQuad, categoryIdentifier);
				return i;
			}
		}

		SCR_MarkerIconEntry entry = new SCR_MarkerIconEntry();
		entry.HST_SetIconResource(imageset, imagesetGlow, imageQuad, categoryIdentifier);
		m_aPlacedMarkerIcons.Insert(entry);
		return m_aPlacedMarkerIcons.Count() - 1;
	}
}

modded class SCR_MapMarkerManagerComponent
{
	static const ResourceName HST_CANONICAL_MARKER_CONFIG = "{3583D42139D9A10B}Configs/Map/CampaignMapMarkerConfig.conf";
	static const ResourceName HST_PLAYER_MARKER_CONFIG = "{6985327711306212}Configs/Map/HST_PlayerMapMarkerConfig.conf";
	protected bool m_bHSTMarkerConfigInitialized;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		EnsureHSTMarkerConfig(HST_PLAYER_MARKER_CONFIG);
	}

	override protected void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		m_bHSTMarkerConfigInitialized = true;
	}

	bool EnsureHSTMarkerConfig(ResourceName markerConfigPath)
	{
		if (markerConfigPath.IsEmpty())
			return false;

		SCR_MapMarkerEntryPlaced placedEntry = ResolveValidPlacedEntry(m_MarkerCfg);
		SCR_MapMarkerEntryConfig playerEntry = null;
		if (m_MarkerCfg)
			playerEntry = m_MarkerCfg.GetMarkerEntryConfigByType(SCR_EMapMarkerType.HST_PLAYER);
		if (placedEntry && playerEntry)
		{
			int existingRadioIconEntry = placedEntry.HST_EnsureRadioSignalIconEntry();
			return existingRadioIconEntry >= 0 && placedEntry.HST_IsIconEntryValid(existingRadioIconEntry);
		}

		SCR_MapMarkerConfig playerSource = LoadHSTMarkerConfig(markerConfigPath);
		if (!playerSource)
			return false;

		playerEntry = playerSource.GetMarkerEntryConfigByType(SCR_EMapMarkerType.HST_PLAYER);
		if (!playerEntry)
			return false;

		if (!placedEntry)
		{
			m_MarkerCfg = LoadHSTMarkerConfig(HST_CANONICAL_MARKER_CONFIG);
			placedEntry = ResolveValidPlacedEntry(m_MarkerCfg);
			if (!placedEntry)
				return false;
		}

		array<ref SCR_MapMarkerEntryConfig> entryConfigs = m_MarkerCfg.GetMarkerEntryConfigs();
		if (!entryConfigs)
			return false;

		SCR_MapMarkerEntryConfig existingPlayerEntry = m_MarkerCfg.GetMarkerEntryConfigByType(SCR_EMapMarkerType.HST_PLAYER);
		if (!existingPlayerEntry)
		{
			entryConfigs.Insert(playerEntry);
			if (m_bHSTMarkerConfigInitialized)
				InitHSTDynamicMarkerEntry(playerEntry);
		}

		int radioIconEntry = placedEntry.HST_EnsureRadioSignalIconEntry();
		return radioIconEntry >= 0
			&& placedEntry.HST_IsIconEntryValid(radioIconEntry)
			&& m_MarkerCfg.GetMarkerEntryConfigByType(SCR_EMapMarkerType.HST_PLAYER) != null;
	}

	int HST_EnsurePlacedIconEntry(ResourceName imageset, ResourceName glowImageset, string imageQuad, string categoryIdentifier = "general")
	{
		SCR_MapMarkerEntryPlaced placedEntry = ResolveValidPlacedEntry(m_MarkerCfg);
		if (!placedEntry)
			return -1;

		int iconEntry = placedEntry.HST_EnsureIconEntry(imageset, glowImageset, imageQuad, categoryIdentifier);
		if (!placedEntry.HST_IsIconEntryValid(iconEntry))
			return -1;

		return iconEntry;
	}

	int HST_ResolveValidPlacedIconEntry(int requestedIconEntry, int fallbackIconEntry)
	{
		SCR_MapMarkerEntryPlaced placedEntry = ResolveValidPlacedEntry(m_MarkerCfg);
		if (!placedEntry)
			return -1;
		if (placedEntry.HST_IsIconEntryValid(requestedIconEntry))
			return requestedIconEntry;
		if (placedEntry.HST_IsIconEntryValid(fallbackIconEntry))
			return fallbackIconEntry;

		return -1;
	}

	protected SCR_MapMarkerConfig LoadHSTMarkerConfig(ResourceName markerConfigPath)
	{
		if (markerConfigPath.IsEmpty())
			return null;

		Resource container = BaseContainerTools.LoadContainer(markerConfigPath);
		if (!container)
			return null;

		return SCR_MapMarkerConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
	}

	protected SCR_MapMarkerEntryPlaced ResolveValidPlacedEntry(SCR_MapMarkerConfig markerConfig)
	{
		if (!markerConfig)
			return null;

		SCR_MapMarkerEntryPlaced placedEntry = SCR_MapMarkerEntryPlaced.Cast(markerConfig.GetMarkerEntryConfigByType(SCR_EMapMarkerType.PLACED_CUSTOM));
		if (!placedEntry || !placedEntry.HST_IsIconEntryValid(SCR_EScenarioFrameworkMarkerCustom.OBJECTIVE_MARKER))
			return null;

		return placedEntry;
	}

	protected void InitHSTDynamicMarkerEntry(SCR_MapMarkerEntryConfig entryConfig)
	{
		SCR_MapMarkerEntryDynamic dynamicEntry = SCR_MapMarkerEntryDynamic.Cast(entryConfig);
		if (!dynamicEntry)
			return;

		bool isMaster;
		if (m_pGameMode)
			isMaster = m_pGameMode.IsMaster();

		if (isMaster)
			dynamicEntry.InitServerLogic();
		dynamicEntry.InitClientLogic();
	}
}
