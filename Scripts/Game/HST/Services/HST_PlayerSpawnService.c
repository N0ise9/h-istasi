class HST_PlayerSpawnService
{
	static const string PRIMARY_PLAYER_FACTION = "FIA";
	static const string DEFAULT_PLAYER_PREFAB = "{84B40583F4D1B7A3}Prefabs/Characters/Factions/INDFOR/FIA/Character_FIA_Rifleman.et";
	static const string DEFAULT_SPAWNPOINT_PREFAB = "{72713ED566A531F3}PrefabsEditable/SpawnPoints/E_SpawnPoint_FIA.et";

	string GetPrimaryPlayerFaction(HST_CampaignPreset preset)
	{
		if (preset && !preset.m_sResistanceFactionKey.IsEmpty())
			return preset.m_sResistanceFactionKey;

		return PRIMARY_PLAYER_FACTION;
	}

	vector GetHQSpawnPosition(HST_CampaignState state)
	{
		if (!state)
			return "0 0 0";

		return state.m_vHQPosition;
	}

	string GetDefaultPlayerPrefab()
	{
		return DEFAULT_PLAYER_PREFAB;
	}

	string GetDefaultSpawnPointPrefab()
	{
		return DEFAULT_SPAWNPOINT_PREFAB;
	}
}
