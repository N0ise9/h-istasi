// Respawn-system backend for h-istasi's custom FIA HQ spawn path. This avoids
// the stock role-selection menu while still using Reforger's native possession.
[BaseContainerProps()]
class HST_PlayerSpawnLogic : SCR_AutoSpawnLogic
{
	override protected void DoSpawn_S(int playerId)
	{
		HST_CampaignCoordinatorComponent coordinator = HST_CampaignCoordinatorComponent.GetInstance();
		if (!coordinator)
		{
			Print(string.Format("h-istasi | cannot spawn player %1: campaign coordinator is not ready", playerId), LogLevel.ERROR);
			return;
		}

		if (!coordinator.SpawnOrRespawnPlayer(playerId))
			Print(string.Format("h-istasi | custom FIA spawn failed for player %1", playerId), LogLevel.ERROR);
	}
}

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

	int SpawnMissingConnectedPlayers(HST_CampaignState state, HST_AuthorizationService authorization, HST_PlayerLifecycleService lifecycle, bool diagnostics = false)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
		{
			if (diagnostics)
				Print("h-istasi | FIA spawn sweep: no PlayerManager yet");

			return 0;
		}

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		if (diagnostics)
			Print(string.Format("h-istasi | FIA spawn sweep: %1 connected player(s)", playerIds.Count()));

		int spawned;
		foreach (int playerId : playerIds)
		{
			RegisterPlayerOnly(state, authorization, lifecycle, playerId);
			if (playerManager.GetPlayerControlledEntity(playerId))
			{
				if (diagnostics)
					Print(string.Format("h-istasi | FIA spawn sweep: player %1 already has a controlled entity", playerId));

				continue;
			}

			if (SpawnOrRespawnPlayer(state, authorization, lifecycle, playerId, diagnostics))
				spawned++;
		}

		if (spawned > 0)
			SCR_RespawnSystemComponent.CloseRespawnMenu();

		return spawned;
	}

	bool SpawnOrRespawnPlayer(HST_CampaignState state, HST_AuthorizationService authorization, HST_PlayerLifecycleService lifecycle, int playerId, bool diagnostics = false)
	{
		if (!Replication.IsServer() || !state || !authorization || !lifecycle || playerId <= 0)
		{
			if (diagnostics)
				Print(string.Format("h-istasi | cannot spawn player %1: invalid spawn context", playerId), LogLevel.WARNING);

			return false;
		}

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager || !playerManager.IsPlayerConnected(playerId))
		{
			if (diagnostics)
				Print(string.Format("h-istasi | cannot spawn player %1: player manager missing or player not connected", playerId), LogLevel.WARNING);

			return false;
		}

		HST_PlayerState player = RegisterPlayerOnly(state, authorization, lifecycle, playerId);
		if (!player)
		{
			if (diagnostics)
				Print(string.Format("h-istasi | cannot spawn player %1: registration failed", playerId), LogLevel.WARNING);

			return false;
		}

		IEntity existingEntity = playerManager.GetPlayerControlledEntity(playerId);
		if (existingEntity)
		{
			SCR_RespawnSystemComponent.CloseRespawnMenu();
			return true;
		}

		SCR_RespawnSystemComponent respawnSystem = SCR_RespawnSystemComponent.GetInstance();
		if (!respawnSystem)
		{
			Print("h-istasi | cannot spawn player: no SCR_RespawnSystemComponent instance", LogLevel.ERROR);
			return false;
		}

		RespawnComponent respawnComponent = playerManager.GetPlayerRespawnComponent(playerId);
		if (!respawnComponent)
		{
			Print(string.Format("h-istasi | cannot spawn player %1: no player respawn component", playerId), LogLevel.ERROR);
			return false;
		}

		vector spawnPosition = GetPlayerSpawnPosition(state, playerId);
		if (diagnostics)
			Print(string.Format("h-istasi | spawning FIA player %1 at HQ hideout", playerId));

		GenericEntity spawnedEntity = respawnSystem.DoSpawn(DEFAULT_PLAYER_PREFAB, spawnPosition, "0 0 0");
		if (!spawnedEntity)
		{
			Print(string.Format("h-istasi | failed to create FIA player prefab for player %1", playerId), LogLevel.ERROR);
			return false;
		}

		ApplyFaction(spawnedEntity, PRIMARY_PLAYER_FACTION);
		respawnComponent.NotifySpawn(spawnedEntity);
		SCR_RespawnSystemComponent.CloseRespawnMenu();

		player.m_sFactionKey = PRIMARY_PLAYER_FACTION;
		player.m_bHasSpawnRecord = true;
		player.m_iSpawnCount++;
		player.m_sLastSpawnPrefab = DEFAULT_PLAYER_PREFAB;
		player.m_vLastSpawnPosition = spawnPosition;
		Print(string.Format("h-istasi | spawned and possessed FIA player %1", playerId));
		return true;
	}

	protected HST_PlayerState RegisterPlayerOnly(HST_CampaignState state, HST_AuthorizationService authorization, HST_PlayerLifecycleService lifecycle, int playerId)
	{
		if (!state || !authorization || !lifecycle || playerId <= 0)
			return null;

		return lifecycle.RegisterConnectedPlayer(state, authorization, playerId, "", false);
	}

	protected vector GetPlayerSpawnPosition(HST_CampaignState state, int playerId)
	{
		vector spawnPosition = GetHQSpawnPosition(state);
		if (!state || !state.m_bHQDeployed)
			spawnPosition = HST_DefaultCatalog.GetHideoutPosition(HST_DefaultCatalog.GetDefaultHideoutId());

		int slot = PositiveModulo(playerId, 9);
		vector offset = "0 0 0";
		offset[0] = (PositiveModulo(slot, 3) - 1) * 2;
		offset[2] = ((slot / 3) - 1) * 2;
		return spawnPosition + offset;
	}

	protected int PositiveModulo(int value, int divisor)
	{
		if (divisor <= 0)
			return 0;

		int quotient = value / divisor;
		int remainder = value - quotient * divisor;
		if (remainder < 0)
			remainder += divisor;

		return remainder;
	}

	protected void ApplyFaction(IEntity entity, string factionKey)
	{
		if (!entity || factionKey.IsEmpty())
			return;

		FactionAffiliationComponent factionComponent = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (factionComponent)
			factionComponent.SetAffiliatedFactionByKey(factionKey);
	}
}
