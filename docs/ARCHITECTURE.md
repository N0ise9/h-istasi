# h-istasi Architecture

## Runtime Ownership

`HST_CampaignCoordinatorComponent` is the server-side entry point. It owns a
single `HST_CampaignState` and delegates to small services:

- `HST_EconomyService`: HR, faction money, support, aggression, and war level.
- `HST_MissionService`: mission eligibility, activation, deadlines, and
  completion rewards.
- `HST_PersistenceService`: native Reforger checkpoint requests and autosave
  debouncing.
- `HST_AuthorizationService`: persistent members, guests, admins, and the
  first commander-vacancy policy.
- `HST_StrategicService`: ownership changes, town support, Petros penalties,
  activation flags, and victory evaluation.
- `HST_ArsenalService`: item counts, unlock thresholds, and abstract garage
  records.
- `HST_EnemyDirectorService`: validated spending against separate attack and
  support pools.
- `HST_PlayerLifecycleService`: connected-player registration, deterministic
  Workbench identity fallback, personal money, and rank.
- `HST_TownService`: resistance income, HR income, and town support changes.
- `HST_GarrisonService`: abstract garrison creation and survivor fold-back.
- `HST_RecruitmentService`: troop training and abstract garrison recruitment.
- `HST_ZoneCaptureService`: capture helpers around strategic ownership
  changes.

The coordinator currently exposes server-only mutation methods that check
campaign phase, known IDs, and mission eligibility before changing state.
Client RPCs are intentionally withheld until player-owned request components
can authenticate the caller identity and enforce member, commander, and admin
permissions without trusting client-provided IDs.

## Authoring Contracts

The addon keeps authoring data separate from runtime state:

- `HST_CampaignPreset`: fixed scenario roles and capability switches.
- `HST_FactionTemplate`: faction identity and capability declarations.
- `HST_MapDefinition`: stable IDs for zones and hideout candidates.
- `HST_BalanceConfig`: Community Edition-derived initial values.
- `HST_MissionRegistryConfig`: mission definitions and deferred capabilities.

The checked-in `.conf` resources are the intended data source. The current
coordinator uses matching defaults while the resource loading and serializer
prefabs are connected in Workbench.

## Persistence

The persistence service uses `SaveGameManager.RequestSavePoint` so local hosts
and dedicated servers follow Reforger's native session-save path. The state
model is versioned from day one. A later increment must add a native scripted
serializer for `HST_CampaignState` and migration tests before save
compatibility is promised.

## World Layout

`Worlds/HST_Everon/HST_Everon.ent` is an original subscene over vanilla
Everon. Named layer files reserve ownership boundaries for physical authoring.
Strategic IDs live in `Configs/HST/Maps/HST_Everon.conf` so persistence does
not depend on fragile entity names.

The checked-in world shells include a base-backed AI world with explicit Eden
soldier and vehicle navmesh configs, a perception manager, faction, loadout,
radio, and chat managers so Workbench can initialize and play-test the plain
game mode without relying on Conflict's strategic brain.

Direct `.ent` Play mode now uses FIA as the primary playable faction:
automatic player respawn is enabled, the spawn menu forces `PLAYERS`, and
`PLAYERS` resolves to `FIA`. `StartingPoints.layer` contains FIA-affiliated
Scenario Framework spawnpoint slots at the authored hideouts, using the stock
editable FIA spawnpoint prefab and HST-authored FIA role-selection loadouts.
RHS_USAF remains the occupier in the strategic preset and may be restored as a
debug harness if stock FIA deployment regresses, but it is not the normal
player-side bootstrap. Game Master-spawned characters do not satisfy the
respawn system and are not expected to close the deployment menu. Workbench
offline play may still log blank identity ID errors from stock reconnect or
editable-entity systems. Treat those as non-blocking Workbench noise if a
character is spawned and possessed.

`HST_HQService` owns the server-side HQ lifecycle: initial hideout selection,
HQ movement between authored hideouts, Petros position, and Petros-loss
penalties. The current development bootstrap auto-selects the central hills
hideout so the campaign enters a playable active phase immediately; the setup
UI increment will replace that auto-selection with a player-facing choice.
`HST_PlayerSpawnService` owns the FIA HQ spawn contract for the next increment:
primary player faction, HQ spawn position, default FIA player prefab, and the
editable FIA spawnpoint prefab. Stock Plain deployment still performs the
actual possession for now.

## Antistasi Framework Spine

The first campaign loop is intentionally abstract. Zones carry type, income,
support, and garrison-slot data in `HST_CampaignState`; garrisons are stored as
infantry and vehicle counts until the hybrid AI activation increment turns them
into physical units near players. Mission success, failure, and timeout paths
mutate economy and aggression state. Coordinator dev actions expose deterministic
server-only hooks for Workbench tests: register a player, move HQ, capture a
zone, complete or fail a mission, tick income, train troops, recruit a garrison,
and fold survivors back into abstract state.
