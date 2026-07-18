# Partisan - Antistasi CE 3.11.1 Parity Matrix

> Generated from `docs/data/antistasi_ce311_parity.json` and current source inventories. Do not hand-edit this file; run `tools/update-release-docs.ps1`.

## Pinned baseline

- Version: **3.11.1**
- Tag commit: [`6e4226d3863ca8673535386c2fff8b6e08a806c4`](https://github.com/official-antistasi-community/A3-Antistasi/tree/6e4226d3863ca8673535386c2fff8b6e08a806c4)
- [Official release](https://github.com/official-antistasi-community/A3-Antistasi/releases/tag/3.11.1)
- [Official detailed guide](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/reference_guide_index.html)

This matrix is a behavioral specification, not a claim that matching names or source paths are complete. Engine-driven differences must be explicit adaptations.

## Proof vocabulary

| Term | Meaning |
| --- | --- |
| `implemented` | A production source path exists. |
| `state-proven` | A deterministic service or state fixture passes. |
| `native-proven` | Ordinary engine entities and callbacks pass. |
| `restart-proven` | A new process restores the exact scenario. |
| `mp-jip-proven` | Packaged server, clients, reconnect, and late join converge. |
| `soak-proven` | Behavior and performance remain bounded over the declared duration and load. |
| `certified` | Every required lower proof rung passes on one unchanged package. |

## Contract summary

| ID | Domain | Priority | Disposition | Blocking milestone |
| --- | --- | --- | --- | --- |
| `arsenal-garage-logistics` | Arsenal, unlocks, loot, loadouts, garage, vehicle ammo, fuel, repair, salvage, and cargo | `P0` | `partial` | Gate 8 resistance progression |
| `civilians-undercover` | Civilians, traffic, theft, casualties, undercover, wanted state, police, and roadblocks | `P0` | `partial` | Gate 5 strategic truth and living Everon |
| `development-diagnostics` | Development proofs, Campaign Debug, destructive admin diagnostics, and release-surface separation | `P0` | `development-only` | Gate 1 immutable package and PR 3 development/release separation |
| `enemy-strategy` | War level, aggression, attack/support resources, planning, QRFs, counterattacks, and reinforcements | `P0` | `partial` | Gate 6 autonomous enemy campaign |
| `facilities-income` | Resources, factories, seaports, airfields, radio sites, banks, and roadblocks | `P0` | `partial` | Gate 7 mission parity and Gate 8 progression |
| `hq-command-economy` | HQ, Petros, commander, membership, rank, points, personal and faction money | `P0` | `partial` | Gate 8 resistance progression |
| `intel-network` | Intel, reveal, interrogation, informants, prisoners, radio, and intelligence network | `P0` | `missing` | Gate 8 resistance progression |
| `locations-ownership` | Location taxonomy, map graph, ownership, flags, garrisons, and facilities | `P0` | `partial` | Gate 5 strategic truth and living Everon |
| `mission-assassination` | Assassination missions | `P0` | `partial` | Gate 7 Tier A missions |
| `mission-conquest` | Conquest and facility-acquisition missions | `P0` | `legacy` | Gate 7 Tier A missions |
| `mission-convoy` | Money, reinforcement, supply, ammo, prisoner, and armored convoys | `P0` | `partial` | Gate 7 Tier A missions |
| `mission-destroy` | Destroy, sabotage, radio, cache, and armor missions | `P0` | `partial` | Gate 7 Tier A missions |
| `mission-dynamic` | Defend Petros, tower rebuild, city battle, minor task, and gun-shop missions | `P0` | `partial` | Gate 7 Tier A and Tier B missions |
| `mission-logistics` | Bank, cache, factory, seaport, airfield, ammo-truck, and weapons-truck logistics | `P0` | `legacy` | Gate 7 Tier A missions |
| `mission-portfolio` | Mission selection, shared lifecycle, tasking, expiry, cleanup, and consequences | `P0` | `partial` | Gate 7 mission-by-mission behavior parity |
| `mission-rescue` | POW and refugee rescue missions | `P0` | `partial` | Gate 7 Tier A missions |
| `mission-support` | City supplies and support missions | `P0` | `legacy` | Gate 7 Tier A missions |
| `persistence-network-admin` | Save, restart, migration, reconnect, JIP, admin controls, security, backup, and rollback | `P0` | `partial` | Gate 9 full graph persistence and fault tolerance |
| `resistance-forces-supports` | Resistance squads, garrisons, training, supports, recall, statics, and construction | `P0` | `partial` | Gate 3 one reliable force runtime |
| `respawn-travel` | Respawn, incapacitation, revive, death penalties, and fast travel | `P0` | `missing` | Gate 8 resistance progression |
| `setup-campaign` | Setup, faction choice, campaign slots, parameters, solo and co-op | `P0` | `partial` | Gate 8 resistance progression and Gate 10 complete-campaign validation |
| `town-politics` | Town population, three-way support, political flips, HR, tax, and police | `P0` | `partial` | Gate 5 strategic truth and living Everon |
| `ui-projection` | Menus, map targeting, markers, tasks, reports, notifications, and rendered accessibility | `P0` | `partial` | Gate 2 immediate regressions and Gate 4 one campaign view |
| `victory-loss-balance` | Victory, loss, pacing, difficulty, and player-count scaling | `P0` | `partial` | Gate 10 balance and complete-campaign validation |

## Detailed behavioral contracts

### arsenal-garage-logistics - Arsenal, unlocks, loot, loadouts, garage, vehicle ammo, fuel, repair, salvage, and cargo

- **Priority / disposition:** `P0` / `partial`
- **Upstream change history:** CE 3.11.0 expanded vehicle-ammo economy, distinct logistics trucks, persistent strategic assets, and garage combat lock.
- **Upstream evidence:** [evidence 1](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/reference_guide_index.html), [evidence 2](https://github.com/official-antistasi-community/A3-Antistasi/releases/tag/3.11.1)
- **Observable contract:** Loot and finite quantities drive unlocks and AI equipment; vehicles and cargo retain exact capacity, damage, fuel, ammo, attachments, ownership, storage, salvage, and combat restrictions.
- **Formula/pacing contract:** Unlock thresholds, capacity, garage slots, purchase/sale, combat lock, ammo/fuel/repair, salvage, and training-loadout rules require golden fixtures.
- **Reforger adaptation:** Reforger inventory, loadout, replication, damage, fuel, turret, and compartment APIs replace Arma 3 storage mechanics.
- **Partisan state owner:** Arsenal items, vehicle cargo, garage/runtime/field vehicles, loadouts, transactions, and mission cargo records
- **Partisan service owner:** HST_ArsenalService, HST_LoadoutEditorService, and vehicle persistence/logistics services
- **Physical behavior:** Native items, cargo, vehicles, attachments, fuel, damage, ammo, crew, and interaction map to exact durable identities.
- **Virtual behavior:** Stored and off-screen vehicles/cargo retain strategic state without duplicate native roots.
- **Transaction policy:** Loot, withdraw, purchase, sale, capture, storage, redeploy, repair, rearm, fuel, and salvage use typed once-only mutations.
- **Persistence/migration:** Items, quantities, unlocks, loadouts, vehicles, cargo, damage/fuel/ammo, and bindings restore exactly.
- **Client projection:** Arsenal, loadout, garage, cargo, costs, capacity, and disabled reasons converge for clients.
- **Proof IDs:** `campaign_debug.phase14_15`, `focused.field_vehicle_restart`, `external.logistics_soak_mp_jip`
- **Blocking milestone:** Gate 8 resistance progression

### civilians-undercover - Civilians, traffic, theft, casualties, undercover, wanted state, police, and roadblocks

- **Priority / disposition:** `P0` / `partial`
- **Upstream change history:** CE 3.11.1 includes civilian-car locality corrections and retains police/undercover rules.
- **Upstream evidence:** [evidence 1](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/reference_guide_index.html), [evidence 2](https://github.com/official-antistasi-community/A3-Antistasi/releases/tag/3.11.1)
- **Observable contract:** Towns feel alive while bounded civilians and traffic move, react, and affect politics; undercover status changes from gear, weapons, vehicles, roads, police, heat, and actions.
- **Formula/pacing contract:** Actor budgets, support consequences, wanted/heat timing, scans, detection, theft, casualties, and roadblock rules require pinned fixtures.
- **Reforger adaptation:** Reforger AI, vehicle seats, road navigation, and perception replace Arma 3 locality and scan mechanics.
- **Partisan state owner:** HST_CampaignState civilian zones, actor slots, consequence receipts, undercover, heat, security, and town influence
- **Partisan service owner:** HST_CivilianService, HST_CivilianConsequenceService, and undercover/security services
- **Physical behavior:** Pedestrians and traffic walk/drive, panic/recover, respect ownership, and publish exact theft/casualty/security evidence.
- **Virtual behavior:** Budgets, rotation, heat, political effects, and security epochs persist without native actor leakage.
- **Transaction policy:** Aid, theft, casualty, scan, exposure, and political consequences settle once.
- **Persistence/migration:** Actor slots, consequence receipts, heat, wanted state, security casualties, and town effects restore exactly.
- **Client projection:** Undercover state, reasons, town support, security, and notifications agree for owner and observers as authorized.
- **Proof IDs:** `focused.ambient`, `focused.civilian_consequence`, `campaign_debug.phase20_21`, `external.living_towns_soak`
- **Blocking milestone:** Gate 5 strategic truth and living Everon

### development-diagnostics - Development proofs, Campaign Debug, destructive admin diagnostics, and release-surface separation

- **Priority / disposition:** `P0` / `development-only`
- **Upstream change history:** Partisan-specific release engineering control; no upstream gameplay parity claim.
- **Upstream evidence:** [evidence 1](https://github.com/official-antistasi-community/A3-Antistasi/tree/6e4226d3863ca8673535386c2fff8b6e08a806c4)
- **Observable contract:** Development fixtures remain selectable and retained for engineering, while the release package exposes only safe production observability and authorized administration.
- **Formula/pacing contract:** Release allow/deny symbol lists, command permissions, profile isolation, timeouts, cleanup, and evidence retention are machine-validated.
- **Reforger adaptation:** Use separate project/package boundaries or compile guards supported by the Reforger toolchain.
- **Partisan state owner:** No production gameplay state owner; diagnostic state is disposable and explicitly scoped.
- **Partisan service owner:** Development addon/test runners and release-package audit
- **Physical behavior:** Diagnostics may create native fixtures only inside owned disposable scopes with exact cleanup.
- **Virtual behavior:** Synthetic state never becomes ordinary campaign authority.
- **Transaction policy:** Destructive or mutating diagnostics require trusted admin scope and cannot ship as ordinary player actions.
- **Persistence/migration:** Development artifacts are not accepted as public campaign state unless a production migration explicitly owns them.
- **Client projection:** No development-only action or fixture is visible in the release client.
- **Proof IDs:** `foundation.release_package_audit`, `external.release_pak_forbidden_symbols`
- **Blocking milestone:** Gate 1 immutable package and PR 3 development/release separation

### enemy-strategy - War level, aggression, attack/support resources, planning, QRFs, counterattacks, and reinforcements

- **Priority / disposition:** `P0` / `partial`
- **Upstream change history:** CE 3.11.0 made counterattacks budget-backed and reinforcements physically interceptable; CE 3.11.1 is the pinned correction level.
- **Upstream evidence:** [evidence 1](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/concepts_info/concepts/war_level.html), [evidence 2](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/concepts_info/concepts/aggto_system_change.html), [evidence 3](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/reference_guide_index.html)
- **Observable contract:** Occupier and invader plan and spend separately, fight each other and the resistance, and continue campaigning off-screen.
- **Formula/pacing contract:** War-level thresholds, aggression, budget cadence, target choice, composition, attack timing, debit, refund, and settlement require golden fixtures.
- **Reforger adaptation:** Reforger strategic simulation owns off-screen movement; native groups are projections of budget-backed orders.
- **Partisan state owner:** HST_CampaignState enemy resource pools, planning receipts, orders, operations, routes, and settlements
- **Partisan service owner:** HST_EnemyStrategicResourceService, HST_EnemyPlanningAuthorityService, and HST_EnemyCommanderService
- **Physical behavior:** Budget-backed forces materialize exact rosters and reconcile native casualties and outcomes.
- **Virtual behavior:** Planning, travel, combat, rebuilding, and return advance with no nearby player.
- **Transaction policy:** Commit, debit, cancellation, survivor settlement, and refund share stable order and transaction identities.
- **Persistence/migration:** Pools, aggression, planning cadence, commitments, routes, casualties, and receipts restore without replay.
- **Client projection:** Players see only authorized intel, but operation markers/tasks remain revision-consistent across clients.
- **Proof IDs:** `focused.enemy_resources`, `focused.enemy_planning`, `focused.counterattack`, `external.autonomous_enemy_soak`
- **Blocking milestone:** Gate 6 autonomous enemy campaign

### facilities-income - Resources, factories, seaports, airfields, radio sites, banks, and roadblocks

- **Priority / disposition:** `P0` / `partial`
- **Upstream change history:** CE 3.11.0 added or expanded persistent strategic assets, logistics distinctions, and garage/economy behavior.
- **Upstream evidence:** [evidence 1](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/concepts_info/concepts/radio_towers.html), [evidence 2](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/reference_guide_index.html)
- **Observable contract:** Each facility has a distinct strategic value, lifecycle, income/logistics consequence, and enemy response rather than acting as a generic capture-progress source.
- **Formula/pacing contract:** Income multipliers, airfield restrictions, radio effects, roadblock effects, and facility-specific rewards require pinned fixtures.
- **Reforger adaptation:** Only stock Reforger assets with a safe native lifecycle are enabled; unavailable strategic asset families are explicit deferrals.
- **Partisan state owner:** HST_CampaignState zones, facilities, radio sites, strategic events, and economy records
- **Partisan service owner:** HST_StrategicService, HST_RadioSiteLifecycleService, and HST_BuildModeService
- **Physical behavior:** Facility assets bind to stable authored/generated entities and accept native destruction, repair, capture, and construction evidence.
- **Virtual behavior:** Income, rebuild, radio, and roadblock effects continue from durable strategic state.
- **Transaction policy:** Every income, rebuild, purchase, loss, and mission effect has a typed once-only transaction.
- **Persistence/migration:** Facility lifecycle, asset identity, destruction epoch, and pending settlement restore exactly.
- **Client projection:** Facility state, task, marker, report, and disabled reason converge for all clients.
- **Proof IDs:** `focused.radio_lifecycle`, `campaign_debug.facilities`, `external.facility_restart_mp_jip`
- **Blocking milestone:** Gate 7 mission parity and Gate 8 progression

### hq-command-economy - HQ, Petros, commander, membership, rank, points, personal and faction money

- **Priority / disposition:** `P0` / `partial`
- **Upstream change history:** CE 3.11.1 retains the CE 3.11.x commander, HQ, Petros, and split player/faction economy model.
- **Upstream evidence:** [evidence 1](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/concepts_info/concepts/hq_attack.html), [evidence 2](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/reference_guide_index.html)
- **Observable contract:** Players can distinguish personal authority and money from faction authority and money, while HQ and Petros create meaningful campaign risk.
- **Formula/pacing contract:** Rank, points, election, income, HQ discovery, Petros loss, and rebuild values require golden CE 3.11.1 fixtures.
- **Reforger adaptation:** Backend identities and Reforger role/locality rules replace Arma 3 player-object assumptions.
- **Partisan state owner:** HST_CampaignState player, commander, economy, HQ, and Petros records
- **Partisan service owner:** HST_PlayerLifecycleService, HST_EconomyService, and HST_HQService
- **Physical behavior:** HQ assets and Petros are native projections with exact ownership and lifecycle callbacks.
- **Virtual behavior:** HQ knowledge, threat, attack preparation, and recovery continue without nearby players.
- **Transaction policy:** Personal and faction mutations require separate stable ledger identities and once-only settlement.
- **Persistence/migration:** Identity, membership, balances, HQ threat, Petros state, and pending consequences restore exactly.
- **Client projection:** Authorized clients receive clear commander/member state, balances, HQ threat, and disabled reasons.
- **Proof IDs:** `campaign_debug.members`, `campaign_debug.economy`, `campaign_debug.phase22`, `external.hq_restart`
- **Blocking milestone:** Gate 8 resistance progression

### intel-network - Intel, reveal, interrogation, informants, prisoners, radio, and intelligence network

- **Priority / disposition:** `P0` / `missing`
- **Upstream change history:** CE 3.11.1 retains intelligence-driven reveal, radio, prisoner, and HQ knowledge systems.
- **Upstream evidence:** [evidence 1](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/reference_guide_index.html)
- **Observable contract:** Players gain imperfect, durable intelligence through intended actions; it reveals appropriate locations/operations and affects HQ/radio pressure.
- **Formula/pacing contract:** Intel chance, amount, reveal scope/duration, interrogation/informant effects, radio modifiers, and HQ knowledge require pinned fixtures.
- **Reforger adaptation:** Reforger task and marker projection present intel while server authority retains hidden truth.
- **Partisan state owner:** Intel, reveal, radio, prisoner, HQ knowledge, marker, and event records
- **Partisan service owner:** HST_MissionService, HST_RadioSiteLifecycleService, and future dedicated intel authority
- **Physical behavior:** Intel objects, prisoners, radios, and interactions publish server-authoritative evidence.
- **Virtual behavior:** Reveal expiry, radio effects, and enemy knowledge continue from strategic time.
- **Transaction policy:** Intel grant, spend, reveal, interrogation, and consequence require stable once-only identities.
- **Persistence/migration:** Intel balances, reveals, radio state, prisoners, and knowledge restore exactly.
- **Client projection:** Only authorized intel is projected, with consistent markers/tasks after reconnect/JIP.
- **Proof IDs:** `external.intel_restart_mp_jip`
- **Blocking milestone:** Gate 8 resistance progression

### locations-ownership - Location taxonomy, map graph, ownership, flags, garrisons, and facilities

- **Priority / disposition:** `P0` / `partial`
- **Upstream change history:** CE 3.11.0 expanded invader town capture and persistent strategic assets.
- **Upstream evidence:** [evidence 1](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/concepts_info/concepts/map_markers.html), [evidence 2](https://github.com/official-antistasi-community/A3-Antistasi/releases/tag/3.11.1)
- **Observable contract:** Every strategic location has one stable identity and one owner; military sites change through physical control and publish every consequence once.
- **Formula/pacing contract:** Location values, capture conditions, garrison policy, and war-level contribution must match the pinned rules or an approved adaptation.
- **Reforger adaptation:** Everon authored locations and Game Master markers replace Arma 3 marker topology while retaining the same strategic cause and effect.
- **Partisan state owner:** HST_CampaignState zone, route, garrison, ownership receipt, and strategic-event records
- **Partisan service owner:** HST_OwnershipTransitionService and HST_ZoneCaptureService
- **Physical behavior:** Native defenders, flags, authored markers, vehicles, and assets follow one accepted transition revision.
- **Virtual behavior:** Off-screen ownership, garrison policy, and strategic consequences use the same revisioned receipt.
- **Transaction policy:** Capture rewards, losses, and enemy commitments settle once from the transition receipt.
- **Persistence/migration:** Owner, revision, pending transition, garrison policy, and legacy location aliases restore without duplicate side effects.
- **Client projection:** Host, clients, reconnect, and JIP converge on authored and projected owner state.
- **Proof IDs:** `focused.ownership_transition`, `campaign_debug.phase17`, `external.figari_ownership_mp_jip_restart`
- **Blocking milestone:** Gate 5 strategic truth and living Everon

### mission-assassination - Assassination missions

- **Priority / disposition:** `P0` / `partial`
- **Upstream change history:** Pinned to CE 3.11.1 mission definitions and consequences.
- **Upstream evidence:** [evidence 1](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/reference_guide_index.html)
- **Observable contract:** Officer, traitor, and special-operations targets have distinct admission, guards, success/failure, escape/expiry, and strategic effects.
- **Formula/pacing contract:** Rewards, aggression, HQ knowledge, points, and target/guard difficulty are mission-specific.
- **Reforger adaptation:** Stock characters and Everon sites replace unavailable content while retaining distinct targets and consequences.
- **Partisan state owner:** Mission, HVT asset, exact guard operation, consequence, and settlement records
- **Partisan service owner:** HST_MissionRuntimeService and assassination guard operation services
- **Physical behavior:** Separate HVT and exact guard projections use native death/deletion authority.
- **Virtual behavior:** Expiry and strategic state continue while off-screen; the HVT is never duplicated by projection.
- **Transaction policy:** Each target outcome settles one mission-specific reward or penalty.
- **Persistence/migration:** HVT, guard roster, expiry, and consequence restore exactly.
- **Client projection:** Target task and marker state converge through JIP and restart.
- **Proof IDs:** `focused.assassination_guards`, `campaign_debug.assassination`, `external.assassination_restart_mp_jip`
- **Blocking milestone:** Gate 7 Tier A missions

### mission-conquest - Conquest and facility-acquisition missions

- **Priority / disposition:** `P0` / `legacy`
- **Upstream change history:** Pinned to CE 3.11.1 location and mission behavior.
- **Upstream evidence:** [evidence 1](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/reference_guide_index.html)
- **Observable contract:** Town politics and military-site control remain distinct; a generic clear-and-hold reward cannot substitute for facility-specific strategic behavior.
- **Formula/pacing contract:** Admission, war-level restriction, defense, hold, ownership, reward, aggression, and facility consequences are location-specific.
- **Reforger adaptation:** Everon site geometry changes physical objectives without merging political and military capture rules.
- **Partisan state owner:** Mission, objective, combat-presence, ownership-transition, facility, and settlement records
- **Partisan service owner:** HST_MissionRuntimeService, HST_CombatPresenceService, and HST_OwnershipTransitionService
- **Physical behavior:** Conscious hostile presence and valid control conditions drive the objective.
- **Virtual behavior:** Ownership is never granted by player-distance suspension or an empty asset.
- **Transaction policy:** The canonical ownership receipt owns rewards and strategic side effects once.
- **Persistence/migration:** Hold progress, presence state, pending owner transition, and consequence restore exactly.
- **Client projection:** Objective, owner, authored marker, projected marker, and Map/War row share one revision.
- **Proof IDs:** `focused.ownership_transition`, `external.conquest_specific_mp_jip_restart`
- **Blocking milestone:** Gate 7 Tier A missions

### mission-convoy - Money, reinforcement, supply, ammo, prisoner, and armored convoys

- **Priority / disposition:** `P0` / `partial`
- **Upstream change history:** CE 3.11.1 includes convoy server-execution corrections; CE 3.11.0 requires real interceptable reinforcements and distinct logistics cargo.
- **Upstream evidence:** [evidence 1](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/reference_guide_index.html), [evidence 2](https://github.com/official-antistasi-community/A3-Antistasi/releases/tag/3.11.1)
- **Observable contract:** A convoy moves without nearby players, materializes exact vehicles/crew/cargo, can be intercepted, and settles its mission-specific payload and strategic effects.
- **Formula/pacing contract:** Composition, route, difficulty, money/cargo/captive outcome, attack timing, support, aggression, points, and failure are convoy-specific.
- **Reforger adaptation:** Server-local Reforger drivers, seats, road routes, and obstruction recovery replace Arma 3 vehicle-locality technique.
- **Partisan state owner:** Mission convoy operation, route, manifest, vehicles, groups, cargo/captive, objective, and settlement records
- **Partisan service owner:** HST_MissionConvoyOperationService, HST_ConvoyVehicleControlAdapter, and HST_ConvoyOutcomeService
- **Physical behavior:** Exact vehicles, drivers, seats, spacing, movement, contact, salvage, and cleanup use native evidence.
- **Virtual behavior:** Staging and route progress continue with no client nearby and reconcile with physical progress.
- **Transaction policy:** Mission-specific payload, reward, failure, aggression, resource, and salvage outcomes settle once.
- **Persistence/migration:** Every staging, virtual, materializing, physical, contact, folding, arrival, and settlement phase restores exactly.
- **Client projection:** Task, convoy marker, route status, asset outcome, and JIP/restart state converge.
- **Proof IDs:** `focused.mission_convoy`, `campaign_debug.convoy`, `external.convoy_natural_route_restart_mp_jip`
- **Blocking milestone:** Gate 7 Tier A missions

### mission-destroy - Destroy, sabotage, radio, cache, and armor missions

- **Priority / disposition:** `P0` / `partial`
- **Upstream change history:** CE 3.11.x retains mission-specific radio, armor, and sabotage consequences rather than one generic destroy reward.
- **Upstream evidence:** [evidence 1](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/concepts_info/concepts/radio_towers.html), [evidence 2](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/reference_guide_index.html)
- **Observable contract:** The intended physical asset must be destroyed or captured through its declared branch, with mission-specific strategic effects and no ambient-equipment false positive.
- **Formula/pacing contract:** Radio timing/aggression, armor branch rewards/penalties, facility sabotage, and failure consequences are distinct.
- **Reforger adaptation:** Stock destructible assets and explicit damage-authority adapters replace unavailable target classes.
- **Partisan state owner:** Mission target assets, radio lifecycle, damage witnesses, vehicle capture, objectives, and settlements
- **Partisan service owner:** HST_MissionRuntimeService, HST_RadioSiteLifecycleService, and HST_MissionObjectiveService
- **Physical behavior:** Native damage/destruction/capture evidence binds to the intended stable asset.
- **Virtual behavior:** Expiry and strategic rebuild state continue off-screen without inventing destruction.
- **Transaction policy:** Success, capture alternative, failure, expiry, and strategic effects settle once per target epoch.
- **Persistence/migration:** Target binding, damage evidence, radio epoch, branch, and consequence restore exactly.
- **Client projection:** Task, marker, target status, branch, and result converge after JIP/restart.
- **Proof IDs:** `focused.radio_lifecycle`, `campaign_debug.destroy`, `external.destroy_specific_restart_mp_jip`
- **Blocking milestone:** Gate 7 Tier A missions

### mission-dynamic - Defend Petros, tower rebuild, city battle, minor task, and gun-shop missions

- **Priority / disposition:** `P0` / `partial`
- **Upstream change history:** Pinned to CE 3.11.1 dynamic-event behavior and inherited 3.11.x campaign pressure rules.
- **Upstream evidence:** [evidence 1](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/concepts_info/concepts/hq_attack.html), [evidence 2](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/reference_guide_index.html)
- **Observable contract:** Each dynamic event is triggered by real campaign state, persists its nonterminal phases, and applies its distinct success/failure consequence.
- **Formula/pacing contract:** Trigger chance, HQ knowledge, timing, composition, shop inventory/prices, support, aggression, and loss consequences are event-specific.
- **Reforger adaptation:** Everon sites and Reforger interaction UI adapt presentation without replacing campaign causality with admin or synthetic timers.
- **Partisan state owner:** Dynamic mission, HQ/Petros, radio rebuild, town battle, gun-shop inventory, objective, and transaction records
- **Partisan service owner:** HST_MissionService, HST_MissionRuntimeService, HST_HQService, and HST_RadioSiteLifecycleService
- **Physical behavior:** Required attackers, defenders, targets, sellers, inventory, and interactions are native and authoritative.
- **Virtual behavior:** Threat, admission, expiry, travel, and strategic consequences continue off-screen where applicable.
- **Transaction policy:** Event admission, purchase, reward, penalty, and cleanup settle exactly once.
- **Persistence/migration:** Trigger, inventory, objective, runtime binding, and consequence windows restore exactly.
- **Client projection:** Tasks, shop actions, markers, HQ threat, and outcomes converge for clients/JIP.
- **Proof IDs:** `campaign_debug.dynamic`, `focused.radio_lifecycle`, `external.dynamic_restart_mp_jip`
- **Blocking milestone:** Gate 7 Tier A and Tier B missions

### mission-logistics - Bank, cache, factory, seaport, airfield, ammo-truck, and weapons-truck logistics

- **Priority / disposition:** `P0` / `legacy`
- **Upstream change history:** CE 3.11.0 distinguishes weapons and ammo logistics and expands vehicle-ammo economy.
- **Upstream evidence:** [evidence 1](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/reference_guide_index.html), [evidence 2](https://github.com/official-antistasi-community/A3-Antistasi/releases/tag/3.11.1)
- **Observable contract:** Cargo, vehicle, source, delivery, interception, success, failure, and strategic effect differ by logistics mission.
- **Formula/pacing contract:** Cargo counts, rewards, support shifts, aggression, unlocks, ammo/fuel/repair effects, and failure penalties are mission-specific.
- **Reforger adaptation:** Reforger inventory and vehicle storage APIs replace Arma 3 cargo handling while retaining finite, typed quantities.
- **Partisan state owner:** Mission assets, cargo ownership, carrier binding, vehicle state, arsenal/logistics, objectives, and settlement records
- **Partisan service owner:** HST_MissionRuntimeService, HST_ArsenalService, and vehicle/logistics services
- **Physical behavior:** Exact cargo or vehicle is picked up, carried, delivered, lost, or captured through native interaction.
- **Virtual behavior:** Mission expiry and enemy strategic consequences continue off-screen; player-bound cargo does not teleport.
- **Transaction policy:** Cargo transfer, rewards, penalties, unlocks, and strategic effects settle once.
- **Persistence/migration:** Cargo, carrier, vehicle, delivery, expiry, and transaction state restore exactly.
- **Client projection:** Task, asset marker, carried/delivered status, and outcome converge for clients/JIP.
- **Proof IDs:** `campaign_debug.logistics`, `external.logistics_specific_restart_mp_jip`
- **Blocking milestone:** Gate 7 Tier A missions

### mission-portfolio - Mission selection, shared lifecycle, tasking, expiry, cleanup, and consequences

- **Priority / disposition:** `P0` / `partial`
- **Upstream change history:** CE 3.11.1 mission behavior includes fixes inherited from 3.11.0 and 3.11.1; category labels alone are not parity.
- **Upstream evidence:** [evidence 1](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/reference_guide_index.html), [evidence 2](https://github.com/official-antistasi-community/A3-Antistasi/tree/6e4226d3863ca8673535386c2fff8b6e08a806c4)
- **Observable contract:** Every enabled mission has its own objective branches, assets, AI behavior, success, failure, expiry, cleanup, and strategic consequences.
- **Formula/pacing contract:** Mission selection, difficulty, duration, rewards, penalties, aggression, support, attack timing, and player scaling are pinned per mission.
- **Reforger adaptation:** Mission presentation and native adapter may change for Everon, while the strategic causal graph remains equivalent.
- **Partisan state owner:** HST_CampaignState mission, objective, asset, task, marker, event, and transaction records
- **Partisan service owner:** HST_MissionService, HST_MissionRuntimeService, and mission-specific operation/lifecycle services
- **Physical behavior:** Required native assets and actors exist, behave, and publish authoritative evidence without synthetic completion.
- **Virtual behavior:** Eligible missions and traveling forces progress off-screen where the upstream behavior permits it.
- **Transaction policy:** Success, each failure branch, expiry, salvage, and delayed outcomes settle once through typed authorities.
- **Persistence/migration:** Every nonterminal phase, asset binding, consequence window, and task state survives restart.
- **Client projection:** Tasks, markers, briefings, progress, outcomes, and JIP snapshots agree for every client.
- **Proof IDs:** `campaign_debug.missions`, `external.mission_specific_mp_jip_restart`
- **Blocking milestone:** Gate 7 mission-by-mission behavior parity

### mission-rescue - POW and refugee rescue missions

- **Priority / disposition:** `P0` / `partial`
- **Upstream change history:** Pinned to CE 3.11.1 per-captive rescue behavior and consequences.
- **Upstream evidence:** [evidence 1](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/reference_guide_index.html)
- **Observable contract:** Each captive has an exact identity and lifecycle from guard state through release, following/boarding, extraction, loss, reward, and cleanup.
- **Formula/pacing contract:** Money, HR, town support, aggression, points, and partial/failure outcome scale per rescued or lost captive where the baseline requires it.
- **Reforger adaptation:** Reforger following, compartment seating, and ownership callbacks replace Arma 3 captive locality behavior.
- **Partisan state owner:** Mission captive assets, exact guard operation, follow/seat bindings, extraction, objectives, and settlements
- **Partisan service owner:** HST_RescuePOWOperationService and HST_MissionRuntimeService
- **Physical behavior:** Captives and guards use exact native identities, seating/following, death/deletion, and extraction evidence.
- **Virtual behavior:** Guards remain strategic projections; player-bound freed captives follow an explicit off-screen policy.
- **Transaction policy:** Per-captive and mission terminal outcomes settle once without fixed-count invention.
- **Persistence/migration:** Captive status, carrier/seat, guard roster, extraction, and reward window restore exactly.
- **Client projection:** Captive count, state, task, marker, and outcome converge after reconnect/JIP/restart.
- **Proof IDs:** `focused.rescue_pow`, `campaign_debug.rescue`, `external.rescue_restart_mp_jip`
- **Blocking milestone:** Gate 7 Tier A missions

### mission-support - City supplies and support missions

- **Priority / disposition:** `P0` / `legacy`
- **Upstream change history:** Pinned to CE 3.11.1 support-mission political and aggression consequences.
- **Upstream evidence:** [evidence 1](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/concepts_info/concepts/gaining_losing_city_support.html), [evidence 2](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/reference_guide_index.html)
- **Observable contract:** Supplies reach a contacted town through a real delivery lifecycle and change the intended political/aggression state rather than acting as generic cash.
- **Formula/pacing contract:** Cargo, town support shifts, enemy support shifts, aggression, success, and failure are pinned to the baseline.
- **Reforger adaptation:** Everon delivery sites and Reforger inventory interactions adapt the physical layer only.
- **Partisan state owner:** Mission cargo, town influence, civilian consequence, objective, and settlement records
- **Partisan service owner:** HST_MissionRuntimeService and HST_TownInfluenceService
- **Physical behavior:** Exact supplies are collected and delivered to the target town.
- **Virtual behavior:** Expiry and political consequence timing continue without player proximity; cargo does not teleport.
- **Transaction policy:** Delivery or failure publishes one political/aggression settlement and any approved economy entry.
- **Persistence/migration:** Cargo, target town, delivery, expiry, and political effect restore exactly.
- **Client projection:** Task, cargo, town, support change, and result converge after JIP/restart.
- **Proof IDs:** `campaign_debug.support_mission`, `external.city_supplies_restart_mp_jip`
- **Blocking milestone:** Gate 7 Tier A missions

### persistence-network-admin - Save, restart, migration, reconnect, JIP, admin controls, security, backup, and rollback

- **Priority / disposition:** `P0` / `partial`
- **Upstream change history:** CE 3.11.1 remains a persistent multiplayer campaign baseline; Partisan adds explicit recovery and authority requirements for Reforger.
- **Upstream evidence:** [evidence 1](https://github.com/official-antistasi-community/A3-Antistasi/tree/6e4226d3863ca8673535386c2fff8b6e08a806c4), [evidence 2](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/reference_guide_index.html)
- **Observable contract:** Save/restart, reconnect, late join, duplicate request, fault recovery, backup, upgrade, and rollback never invent, duplicate, refill, lose, or replay campaign facts.
- **Formula/pacing contract:** Schema support windows, checkpoint cadence, history bounds, idempotency horizon, timeout, retry, and retention are explicit contracts.
- **Reforger adaptation:** Reforger persistence callbacks and RPC ownership are wrapped by server authority, a two-generation journal, and fail-closed source selection.
- **Partisan state owner:** HST_CampaignState and HST_CampaignSaveData plus journal, command receipt, projection epoch, and migration records
- **Partisan service owner:** HST_PersistenceService, HST_CampaignProfileSaveJournalService, HST_CampaignCommandService, and HST_ClientProjectionService
- **Physical behavior:** Native runtime is quiesced, captured, restored, rebound, or safely rejected around checkpoints and process changes.
- **Virtual behavior:** The full strategic graph serializes as one coherent authority snapshot.
- **Transaction policy:** Commands and settlements retain durable idempotency beyond detailed-history compaction.
- **Persistence/migration:** A declared public schema window has golden saves; unsupported or conflicting state fails with a safe actionable reason.
- **Client projection:** Clients resynchronize revisioned state after join, reconnect, restore, packet gaps, and stale epochs.
- **Proof IDs:** `focused.profile_journal`, `focused.controlled_shutdown`, `external.full_graph_fault_mp_jip`
- **Blocking milestone:** Gate 9 full graph persistence and fault tolerance

### resistance-forces-supports - Resistance squads, garrisons, training, supports, recall, statics, and construction

- **Priority / disposition:** `P0` / `partial`
- **Upstream change history:** CE 3.11.x includes persistent resistance forces, training, high command, support restrictions, and recent rebel-aircraft behavior.
- **Upstream evidence:** [evidence 1](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/concepts_info/concepts/supportSystem.html), [evidence 2](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/concepts_info/concepts/train_fia.html), [evidence 3](https://github.com/official-antistasi-community/A3-Antistasi/releases/tag/3.11.1)
- **Observable contract:** One accepted quote produces one immutable roster and exact cost; survivors retain duty, route, casualties, marker count, recall, and refund across projection and restart.
- **Formula/pacing contract:** Composition, training cap, cost, ETA, support cooldown, recall, and refund formulas require pinned fixtures.
- **Reforger adaptation:** Native AI movement and high-command UI may differ, but strategic assignment and exact force accounting may not.
- **Partisan state owner:** HST_CampaignState force manifests, quotes, spawn results, operations, routes, groups, supports, garrisons, and ledger rows
- **Partisan service owner:** HST_ForcePlanningService, HST_OperationService, HST_SupportRequestService, and family operation services
- **Physical behavior:** Exact living slots materialize once, run a bounded route, react to engagement, resume assignment, and retire on proven last death.
- **Virtual behavior:** Duty, route, contact, casualty, return, recall, and settlement advance independent of player proximity.
- **Transaction policy:** Quote, reserve, commit, cancel, failure, casualty, recall, return, and archive share stable identities and exact ceilings.
- **Persistence/migration:** Every supported force restores one manifest, living roster, assignment, route cursor, projection owner, marker, and settlement owner.
- **Client projection:** UI quantity/cost/ETA, marker living count, notification, recall choice, and disabled reason derive from the authoritative records.
- **Proof IDs:** `focused.force_authority`, `focused.paid_qrf`, `focused.search_destroy`, `external.figari_force_restart_mp_jip`
- **Blocking milestone:** Gate 3 one reliable force runtime

### respawn-travel - Respawn, incapacitation, revive, death penalties, and fast travel

- **Priority / disposition:** `P0` / `missing`
- **Upstream change history:** Pinned to CE 3.11.1 player lifecycle and travel restrictions.
- **Upstream evidence:** [evidence 1](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/reference_guide_index.html)
- **Observable contract:** Death, incapacitation, revive, equipment/economy penalty, respawn choice, and fast travel are predictable and cannot bypass combat or operations.
- **Formula/pacing contract:** Timers, penalties, eligibility, destination control, combat distance, operation restrictions, and player scaling require pinned fixtures.
- **Reforger adaptation:** Reforger character lifecycle and deployment UI replace Arma 3 respawn mechanics.
- **Partisan state owner:** Player lifecycle, incapacitation, penalty, spawn, location control, combat presence, and travel request records
- **Partisan service owner:** HST_PlayerSpawnService, HST_PlayerLifecycleService, and future travel authority
- **Physical behavior:** Native death/revive/respawn and travel placement are server-authoritative and safe.
- **Virtual behavior:** Eligibility and penalties use durable strategic state rather than client presence.
- **Transaction policy:** Death penalties and travel costs settle once per accepted lifecycle transition.
- **Persistence/migration:** Pending incapacitation, penalty, respawn, and travel requests restore or fail safely.
- **Client projection:** Players receive exact eligibility, cost, destination, denial, and lifecycle state.
- **Proof IDs:** `external.player_lifecycle_mp_restart`
- **Blocking milestone:** Gate 8 resistance progression

### setup-campaign - Setup, faction choice, campaign slots, parameters, solo and co-op

- **Priority / disposition:** `P0` / `partial`
- **Upstream change history:** CE 3.11.0 added true single-player behavior; CE 3.11.1 is the pinned baseline.
- **Upstream evidence:** [evidence 1](https://official-antistasi-community.github.io/A3-Antistasi-Docs/beginners_guide/raw_beginners_guide.html), [evidence 2](https://github.com/official-antistasi-community/A3-Antistasi/releases/tag/3.11.1)
- **Observable contract:** A new campaign exposes explicit faction, parameter, commander, membership, solo/co-op, and starting-position rules before durable play begins.
- **Formula/pacing contract:** Starting resources, player scaling, initial ownership, and setup restrictions must be recorded as pinned data rather than inferred from labels.
- **Reforger adaptation:** Use Reforger lobby, role, and pause capabilities; any missing single-player time-control behavior must be a visible scope decision.
- **Partisan state owner:** HST_CampaignState setup, player, faction, HQ, and preset fields
- **Partisan service owner:** HST_CampaignBootstrapService and HST_PlayerLifecycleService
- **Physical behavior:** Setup creates only the authored/runtime entities owned by the accepted campaign choices.
- **Virtual behavior:** The strategic graph exists before any render-bubble projection.
- **Transaction policy:** Starting balances and grants are typed once-only bootstrap facts.
- **Persistence/migration:** Setup completion, chosen factions, parameters, and identities survive restart without replay.
- **Client projection:** Every client and late joiner sees the same setup completion, commander, factions, and starting state.
- **Proof IDs:** `foundation.setup`, `campaign_debug.setup`, `external.setup_mp_jip`
- **Blocking milestone:** Gate 8 resistance progression and Gate 10 complete-campaign validation

### town-politics - Town population, three-way support, political flips, HR, tax, and police

- **Priority / disposition:** `P0` / `partial`
- **Upstream change history:** CE 3.11.x retains political town capture and enemy-town police; invader town capture is part of the current baseline.
- **Upstream evidence:** [evidence 1](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/concepts_info/concepts/gaining_losing_city_support.html), [evidence 2](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/concepts_info/concepts/benefits_of_controlling_a_town.html)
- **Observable contract:** Towns change side through population support rather than a generic military hold, and enemy police retire when resistance ownership is accepted.
- **Formula/pacing contract:** Population, faction support, event magnitude/expiry, flip threshold, HR, and tax formulas require golden fixtures.
- **Reforger adaptation:** The current policy intentionally creates no automatic resistance police; resistance security must come from explicit garrison policy unless a divergence is approved.
- **Partisan state owner:** HST_CampaignState town influence, civilian consequence, local-security, zone, and ownership records
- **Partisan service owner:** HST_TownInfluenceService, HST_StrategicService, and HST_LocalSecurityOperationService
- **Physical behavior:** Town civilians and enemy local security move, react, suffer exact casualties, and retire on owner change.
- **Virtual behavior:** Support events, income, political flips, and security epochs advance without player proximity.
- **Transaction policy:** Aid, casualties, missions, taxes, HR, and ownership consequences settle once through their domain authorities.
- **Persistence/migration:** Population, three-way support, event history, epoch, casualties, and owner transition restore exactly.
- **Client projection:** Map/War rows, town details, markers, and notifications use the same support and ownership revisions.
- **Proof IDs:** `focused.town_influence`, `focused.combat_presence`, `external.living_towns`, `external.figari_mp_jip_restart`
- **Blocking milestone:** Gate 5 strategic truth and living Everon

### ui-projection - Menus, map targeting, markers, tasks, reports, notifications, and rendered accessibility

- **Priority / disposition:** `P0` / `partial`
- **Upstream change history:** CE 3.11.1 supplies the player-observable behavior; Reforger requires a native UI and revisioned projection adaptation.
- **Upstream evidence:** [evidence 1](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/reference_guide_index.html)
- **Observable contract:** Every enabled action works end to end or has a precise disabled reason; modal input is atomic; every client sees the same authoritative campaign read model.
- **Formula/pacing contract:** Capacity, priority, pagination, text length, resolution, localization, input-state, revision, ACK, gap, and resync behavior are explicit.
- **Reforger adaptation:** Reforger widgets, input contexts, Game Master markers, RPCs, and client-native map markers implement the presentation layer.
- **Partisan state owner:** Server read models, marker records, projection epochs/revisions, tasks, notifications, and command receipts
- **Partisan service owner:** HST_CommandUIService, HST_ClientProjectionService, HST_MapMarkerService, and HST_ClientMarkerProjectionService
- **Physical behavior:** Rendered layouts, pointer/keyboard input, authored markers, and native client markers are exercised in ordinary packaged clients.
- **Virtual behavior:** The server read model remains authoritative regardless of whether a client UI is open.
- **Transaction policy:** UI retries and duplicate activations cannot reapply a command; presentation text never decides command success.
- **Persistence/migration:** Projection epochs and durable read-model sources restore; clients resnapshot rather than persisting stale local truth.
- **Client projection:** Host, two clients, reconnect, JIP, missing/duplicate/delayed/reordered packets, capacity, and restart converge.
- **Proof IDs:** `focused.marker_projection`, `campaign_debug.phase23`, `external.rendered_ui`, `external.projection_mp_jip`
- **Blocking milestone:** Gate 2 immediate regressions and Gate 4 one campaign view

### victory-loss-balance - Victory, loss, pacing, difficulty, and player-count scaling

- **Priority / disposition:** `P0` / `partial`
- **Upstream change history:** Pinned to CE 3.11.1 campaign completion and 3.11.x economy/pacing behavior.
- **Upstream evidence:** [evidence 1](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/concepts_info/concepts/war_level.html), [evidence 2](https://official-antistasi-community.github.io/A3-Antistasi-Docs/reference_guide/reference_guide_index.html)
- **Observable contract:** A weak resistance can progress through meaningful scarcity to a legitimate victory or loss without admin mutation, across declared player counts and difficulties.
- **Formula/pacing contract:** Income, HR, support, war level, aggression, mission cadence, enemy spend, training, unlocks, casualties, victory, loss, and player scaling require completed-campaign evidence.
- **Reforger adaptation:** Everon geography and available stock content may alter pacing inputs, with every tolerance or divergence recorded.
- **Partisan state owner:** Campaign phase/end, ownership, town, economy, war level, enemy, progression, and event records
- **Partisan service owner:** HST_StrategicService and the authoritative domain services feeding campaign outcome
- **Physical behavior:** Natural play outcomes, not admin commands or synthetic timers, produce the strategic state used for completion.
- **Virtual behavior:** Enemy and economy cadence continue off-screen so completion does not depend on player proximity.
- **Transaction policy:** Every progression and end-state mutation has a traceable typed source and once-only settlement.
- **Persistence/migration:** Long campaigns, completion, loss, and post-end state restore without reopening or replay.
- **Client projection:** All clients see the same progression, pacing read model, victory/loss transition, and final summary.
- **Proof IDs:** `campaign_debug.phase24`, `external.complete_campaigns`, `external.balance_profiles`
- **Blocking milestone:** Gate 10 balance and complete-campaign validation

## Mission inventory

All 39 configured mission IDs are mapped below. A row means the mission is inventoried; its disposition and gap determine whether it is release-ready.

| Mission ID | Display name | Category / runtime | Contract | Configured reward | Disposition | Remaining gap |
| --- | --- | --- | --- | --- | --- | --- |
| `assassinate_officer` | Assassinate Officer | `ASSASSINATION / assassination_hvt` | `mission-assassination` | $400; $400 FIA funds | `partial` | Exact guard authority is narrow; mission-specific native, restart, MP/JIP, points, and strategic consequence proof remains open. |
| `assassinate_specops` | Assassinate Spec Ops | `ASSASSINATION / assassination_hvt` | `mission-assassination` | $650; $650 FIA funds, 10 town support | `partial` | Exact guard authority is narrow; complete mission-specific formulas and full proof ladder remain open. |
| `assassinate_traitor` | Assassinate Traitor | `ASSASSINATION / assassination_hvt` | `mission-assassination` | $350; $350 FIA funds | `partial` | Exact guard authority is narrow; escape/failure-to-HQ-pressure behavior and full proof ladder remain open. |
| `conquest_airfield` | Capture Airfield | `CONQUEST / conquest_clear_hold` | `mission-conquest` | $1200; $1200 FIA funds, 60 capture progress | `legacy` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `conquest_factory` | Capture Factory | `CONQUEST / conquest_clear_hold` | `mission-conquest` | $850; $850 FIA funds, 60 capture progress | `legacy` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `conquest_outpost` | Capture Outpost | `CONQUEST / conquest_clear_hold` | `mission-conquest` | $900; $900 FIA funds, 60 capture progress | `legacy` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `conquest_resource` | Capture Resource | `CONQUEST / conquest_clear_hold` | `mission-conquest` | $700; $700 FIA funds, 60 capture progress | `legacy` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `conquest_seaport` | Capture Seaport | `CONQUEST / conquest_clear_hold` | `mission-conquest` | $1050; $1050 FIA funds, 60 capture progress | `legacy` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `conquest_town` | Liberate Town | `CONQUEST / conquest_clear_hold` | `mission-conquest` | $650; $650 FIA funds, 60 capture progress | `legacy` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `convoy_ammo` | Intercept Ammo Convoy | `CONVOY / convoy_route_intercept` | `mission-convoy` | $500; $500 FIA funds, 10 town support | `partial` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `convoy_armored` | Intercept Armored Convoy | `CONVOY / convoy_route_intercept` | `mission-convoy` | $800; $800 FIA funds, 10 town support | `partial` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `convoy_money` | Intercept Money Convoy | `CONVOY / convoy_route_intercept` | `mission-convoy` | $700; $700 FIA funds, 10 town support | `partial` | Current configured fixed money/town-support reward does not match the pinned money-convoy economy, attack-delay, aggression, and point consequences. |
| `convoy_prisoners` | Intercept Prisoner Convoy | `CONVOY / convoy_route_intercept` | `mission-convoy` | $450, 2 HR; $450 FIA funds, 2 HR, 10 town support | `partial` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `convoy_reinforcements` | Intercept Reinforcements | `CONVOY / convoy_route_intercept` | `mission-convoy` | $550; $550 FIA funds, 10 town support | `partial` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `convoy_supplies` | Intercept Supply Convoy | `CONVOY / convoy_route_intercept` | `mission-convoy` | $500; $500 FIA funds, 10 town support | `partial` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `destroy_airfield_asset` | Sabotage Airfield Asset | `DESTROY / destroy_asset` | `mission-destroy` | $750; $750 FIA funds, 25 capture progress | `partial` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `destroy_downed_helicopter` | Destroy Downed Helicopter | `DESTROY / destroy_asset` | `mission-destroy` | $500; $500 FIA funds, 25 capture progress | `partial` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `destroy_factory_asset` | Sabotage Factory Asset | `DESTROY / destroy_asset` | `mission-destroy` | $600; $600 FIA funds, 25 capture progress | `partial` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `destroy_or_steal_armor` | Steal or Destroy Armor | `DESTROY / destroy_asset` | `mission-destroy` | $700; $700 FIA funds, 25 capture progress | `legacy` | The current generic destroy runtime and fixed reward do not yet prove distinct steal/destroy branches, support, attack timing, aggression, points, and failure penalties. |
| `destroy_outpost_cache` | Destroy Outpost Cache | `DESTROY / destroy_asset` | `mission-destroy` | $550; $550 FIA funds, 25 capture progress | `partial` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `destroy_radio_tower` | Destroy Radio Tower | `DESTROY / destroy_asset` | `mission-destroy` | $450; $450 FIA funds, 25 capture progress | `partial` | A narrow exact radio lifecycle exists, but the configured generic cash/capture reward does not yet reproduce the pinned radio attack-timing and aggression purpose. |
| `destroy_seaport_asset` | Sabotage Seaport Asset | `DESTROY / destroy_asset` | `mission-destroy` | $700; $700 FIA funds, 25 capture progress | `partial` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `dynamic_city_flip_battle` | City Flip Battle | `DYNAMIC / dynamic_city_battle` | `mission-dynamic` | $500; $500 FIA funds, 50 capture progress | `partial` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `dynamic_defend_petros` | Defend Petros | `DYNAMIC / dynamic_defend_petros` | `mission-dynamic` | no direct money/HR; $0 FIA funds | `partial` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `dynamic_gun_shop` | Gun Shop | `DYNAMIC / gun_shop` | `mission-dynamic` | no direct money/HR; Purchased items are delivered to HQ after the shop closes. | `partial` | A bounded shop/inventory path exists, but CE 3.11.1 formula crosswalk and packaged persistence/client proof remain open. |
| `dynamic_minor_city_task` | Minor City Task | `DYNAMIC / dynamic_minor_task` | `mission-dynamic` | $250; $250 FIA funds | `partial` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `dynamic_stop_tower_rebuild` | Stop Tower Rebuild | `DYNAMIC / dynamic_tower_rebuild` | `mission-dynamic` | $350; $350 FIA funds | `partial` | A focused same-epoch radio rebuild cut exists; ordinary packaged admission, full consequences, restart, and MP/JIP remain open. |
| `logistics_airfield_intel` | Steal Airfield Intel | `LOGISTICS / logistics_extract` | `mission-logistics` | $700, 1 HR; $700 FIA funds, 1 HR | `legacy` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `logistics_ammo_truck` | Recover Ammo Truck | `LOGISTICS / logistics_extract` | `mission-logistics` | $600, 1 HR; $600 FIA funds, 1 HR | `legacy` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `logistics_bank` | Rob Bank | `LOGISTICS / logistics_extract` | `mission-logistics` | $550, 1 HR; $550 FIA funds, 1 HR | `legacy` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `logistics_factory_supplies` | Recover Factory Supplies | `LOGISTICS / logistics_extract` | `mission-logistics` | $550, 1 HR; $550 FIA funds, 1 HR | `legacy` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `logistics_resource_cache` | Recover Resource Cache | `LOGISTICS / logistics_extract` | `mission-logistics` | $500, 1 HR; $500 FIA funds, 1 HR | `legacy` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `logistics_salvage_supplies` | Salvage Supplies | `LOGISTICS / logistics_extract` | `mission-logistics` | $450, 1 HR; $450 FIA funds, 1 HR | `legacy` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `logistics_seaport_supplies` | Recover Seaport Supplies | `LOGISTICS / logistics_extract` | `mission-logistics` | $650, 1 HR; $650 FIA funds, 1 HR | `legacy` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `logistics_support_cache` | Recover Support Cache | `LOGISTICS / logistics_extract` | `mission-logistics` | $400, 1 HR; $400 FIA funds, 1 HR | `legacy` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `logistics_weapons_truck` | Recover Weapons Truck | `LOGISTICS / logistics_extract` | `mission-logistics` | $650, 1 HR; $650 FIA funds, 1 HR | `legacy` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `rescue_pows` | Rescue POWs | `RESCUE / rescue_extract` | `mission-rescue` | $500, 3 HR; $500 FIA funds, 3 HR, 15 town support | `partial` | Exact captive/guard authority exists, but the configured fixed three-captive reward does not yet implement per-captive CE 3.11.1 scaling and full consequences. |
| `rescue_refugees` | Rescue Refugees | `RESCUE / rescue_extract` | `mission-rescue` | $400, 4 HR; $400 FIA funds, 4 HR, 15 town support | `partial` | Mission-specific CE 3.11.1 semantics, formulas, native behavior, restart, MP/JIP, and soak proof remain open. |
| `support_city_supplies` | Deliver City Supplies | `SUPPORT / support_delivery` | `mission-support` | $300; $300 FIA funds, 25 town support | `legacy` | The current fixed cash/support configuration does not yet prove the pinned three-way support and aggression consequences or full failure branch. |

## Routed command/action inventory

This is the exact checked source/manifest set of command IDs handled by the command UI service. Development/admin and removal candidates are retained here so they cannot disappear from release review.

| Action ID | Surface | Contract | Disposition |
| --- | --- | --- | --- |
| `activate_zone` | `development-admin` | `development-diagnostics` | `development-only` |
| `add_storage_item` | `player` | `arsenal-garage-logistics` | `partial` |
| `admin_campaign_debug_cancel` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_campaign_debug_cleanup` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_campaign_debug_status` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_force_composition_report` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_force_self_commander` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_grant` | `player-or-authorized-admin` | `hq-command-economy` | `partial` |
| `admin_marker_native_report` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_persistence_smoke_report` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_persistence_smoke_test` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase14_report` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase14_seed_blocked` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase14_seed_finite` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase14_seed_threshold` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase15_report` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase15_seed_garage` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase15_seed_source` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase16_report` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase16_seed` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase16_train` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase17_force_counterattack` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase17_force_progress` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase17_report` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase17_seed_capture` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase18_report` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase18_resolve_now` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase18_seed_counterattack` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase18_seed_rebuild` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase18_seed_roadblock` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase19_force_eta` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase19_report` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase19_seed_enemy_ground` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase19_seed_fia_ground` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase19_seed_fia_supply` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase20_clear_heat` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase20_report` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase20_seed_heat` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase20_seed_town` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase20_seed_undercover` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase21_apply_undercover` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase21_clear_heat` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase21_military_vehicle` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase21_police` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase21_report` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase21_roadblock` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase21_weapon_fire` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase22_kill_petros` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase22_queue_attack` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase22_report` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase22_seed_hq_knowledge` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase22_start_defense` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase22_succeed_defense` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase23_failed_action_sample` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase23_marker_audit` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase23_ui_coverage` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase24_force_loss` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase24_force_victory` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase24_report` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase24_seed_early` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase24_seed_late` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_phase24_seed_mid` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_purge_hst_native_markers` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_run_campaign_debug` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_seed_persistence_test_state` | `development-admin` | `development-diagnostics` | `development-only` |
| `admin_spawn_placement_report` | `development-admin` | `development-diagnostics` | `development-only` |
| `apply` | `player` | `arsenal-garage-logistics` | `partial` |
| `award_small` | `development-admin` | `development-diagnostics` | `development-only` |
| `call_supply` | `player` | `resistance-forces-supports` | `partial` |
| `cancel_garrison_quote` | `player` | `resistance-forces-supports` | `partial` |
| `cancel_support` | `player` | `resistance-forces-supports` | `partial` |
| `cancel_support_quote` | `player` | `resistance-forces-supports` | `partial` |
| `capture_zone` | `development-admin` | `development-diagnostics` | `development-only` |
| `checkpoint` | `player-or-authorized-admin` | `persistence-network-admin` | `partial` |
| `civilian_aid` | `player` | `civilians-undercover` | `partial` |
| `complete_mission` | `player` | `mission-portfolio` | `partial` |
| `confirm_garrison_quote` | `player` | `resistance-forces-supports` | `partial` |
| `confirm_support_quote` | `player` | `resistance-forces-supports` | `partial` |
| `deactivate_zone` | `development-admin` | `development-diagnostics` | `development-only` |
| `debug_mission` | `development-admin` | `development-diagnostics` | `development-only` |
| `debug_mission_id` | `development-admin` | `development-diagnostics` | `development-only` |
| `delete_loadout` | `player` | `arsenal-garage-logistics` | `partial` |
| `foundation_status` | `player-observability` | `ui-projection` | `partial` |
| `garage_capture_nearby` | `player` | `arsenal-garage-logistics` | `partial` |
| `garage_redeploy` | `player` | `arsenal-garage-logistics` | `partial` |
| `gun_shop_buy` | `player` | `mission-dynamic` | `partial` |
| `gun_shop_open` | `player` | `mission-dynamic` | `partial` |
| `gun_shop_sell` | `player` | `mission-dynamic` | `partial` |
| `income_now` | `development-admin` | `development-diagnostics` | `development-only` |
| `inspect_active_missions` | `player` | `mission-portfolio` | `partial` |
| `inspect_arsenal` | `player` | `arsenal-garage-logistics` | `partial` |
| `inspect_balance` | `player` | `victory-loss-balance` | `partial` |
| `inspect_campaign` | `player-observability` | `ui-projection` | `partial` |
| `inspect_campaign_end` | `player` | `victory-loss-balance` | `partial` |
| `inspect_capture` | `player` | `locations-ownership` | `partial` |
| `inspect_civilians` | `player` | `civilians-undercover` | `partial` |
| `inspect_content` | `player` | `locations-ownership` | `partial` |
| `inspect_convoy_runtime` | `player` | `mission-portfolio` | `partial` |
| `inspect_economy` | `player-or-authorized-admin` | `hq-command-economy` | `partial` |
| `inspect_garage` | `player` | `arsenal-garage-logistics` | `partial` |
| `inspect_hq_threat` | `player-or-authorized-admin` | `hq-command-economy` | `partial` |
| `inspect_loadout_editor` | `player` | `arsenal-garage-logistics` | `partial` |
| `inspect_markers` | `player-observability` | `ui-projection` | `partial` |
| `inspect_mission` | `player` | `mission-portfolio` | `partial` |
| `inspect_mission_runtime` | `player` | `mission-portfolio` | `partial` |
| `inspect_mission_summary` | `player` | `mission-portfolio` | `partial` |
| `inspect_missions` | `player` | `mission-portfolio` | `partial` |
| `inspect_objectives` | `player` | `mission-portfolio` | `partial` |
| `inspect_persistence` | `player-or-authorized-admin` | `persistence-network-admin` | `partial` |
| `inspect_recruitment` | `player` | `resistance-forces-supports` | `partial` |
| `inspect_support` | `player` | `resistance-forces-supports` | `partial` |
| `inspect_town_support` | `player` | `town-politics` | `partial` |
| `inspect_undercover` | `player` | `civilians-undercover` | `partial` |
| `inspect_vehicle_cargo` | `player` | `arsenal-garage-logistics` | `partial` |
| `inspect_zone_composition` | `player` | `locations-ownership` | `partial` |
| `inspect_zones` | `player` | `locations-ownership` | `partial` |
| `load_loadout` | `player` | `arsenal-garage-logistics` | `partial` |
| `loadout_add_item` | `player` | `arsenal-garage-logistics` | `partial` |
| `loadout_apply` | `player` | `arsenal-garage-logistics` | `partial` |
| `loadout_clear_draft` | `player` | `arsenal-garage-logistics` | `partial` |
| `loadout_delete` | `player` | `arsenal-garage-logistics` | `partial` |
| `loadout_editor_close` | `player` | `arsenal-garage-logistics` | `partial` |
| `loadout_remove_slot` | `player` | `arsenal-garage-logistics` | `partial` |
| `loadout_save` | `player` | `arsenal-garage-logistics` | `partial` |
| `loadout_select` | `player` | `arsenal-garage-logistics` | `partial` |
| `loadout_set_quantity` | `player` | `arsenal-garage-logistics` | `partial` |
| `loot_nearby` | `player` | `arsenal-garage-logistics` | `partial` |
| `member_accept` | `player-or-authorized-admin` | `hq-command-economy` | `partial` |
| `member_promote_commander` | `player-or-authorized-admin` | `hq-command-economy` | `partial` |
| `member_promote_commander_choose` | `player-or-authorized-admin` | `hq-command-economy` | `partial` |
| `member_remove` | `player-or-authorized-admin` | `hq-command-economy` | `partial` |
| `mission_asset_deliver` | `player` | `mission-portfolio` | `partial` |
| `mission_asset_load` | `player` | `mission-portfolio` | `partial` |
| `mission_asset_sabotage` | `player` | `mission-portfolio` | `partial` |
| `mission_asset_unload` | `player` | `mission-portfolio` | `partial` |
| `mission_captive_extract` | `player` | `mission-portfolio` | `partial` |
| `mission_captive_follow` | `player` | `mission-portfolio` | `partial` |
| `mission_category` | `player` | `mission-portfolio` | `partial` |
| `mission_random` | `player` | `mission-portfolio` | `partial` |
| `mission_vehicle_capture` | `player` | `mission-portfolio` | `partial` |
| `mission_zone` | `player` | `mission-portfolio` | `partial` |
| `move_hq` | `player-or-authorized-admin` | `hq-command-economy` | `partial` |
| `move_hq_here` | `player-or-authorized-admin` | `hq-command-economy` | `partial` |
| `new_campaign` | `authorized-admin` | `setup-campaign` | `partial` |
| `noop` | `player-observability` | `ui-projection` | `partial` |
| `petros_deploy_hq_here` | `player-or-authorized-admin` | `hq-command-economy` | `partial` |
| `petros_relocate_hq` | `player-or-authorized-admin` | `hq-command-economy` | `partial` |
| `progress_mission` | `player` | `mission-portfolio` | `partial` |
| `progress_zone` | `development-admin` | `development-diagnostics` | `development-only` |
| `rebuild_hq_assets` | `player-or-authorized-admin` | `hq-command-economy` | `partial` |
| `recruit_zone` | `player` | `resistance-forces-supports` | `partial` |
| `remove_attachment` | `player` | `arsenal-garage-logistics` | `partial` |
| `remove_garrison` | `player` | `resistance-forces-supports` | `partial` |
| `remove_node_item` | `player` | `arsenal-garage-logistics` | `partial` |
| `remove_storage_item` | `player` | `arsenal-garage-logistics` | `partial` |
| `save_loadout` | `player` | `arsenal-garage-logistics` | `partial` |
| `set_attachment` | `player` | `arsenal-garage-logistics` | `partial` |
| `set_node_item` | `player` | `arsenal-garage-logistics` | `partial` |
| `setup_hideout` | `player-or-authorized-admin` | `hq-command-economy` | `partial` |
| `support_fire` | `player` | `resistance-forces-supports` | `partial` |
| `support_gbu` | `remove-before-release` | `resistance-forces-supports` | `legacy` |
| `support_kh55` | `remove-before-release` | `resistance-forces-supports` | `legacy` |
| `support_qrf` | `player` | `resistance-forces-supports` | `partial` |
| `support_recall` | `player` | `resistance-forces-supports` | `partial` |
| `support_recall_choose` | `player` | `resistance-forces-supports` | `partial` |
| `support_roadblock` | `player` | `resistance-forces-supports` | `partial` |
| `support_search` | `player` | `resistance-forces-supports` | `partial` |
| `support_umpk` | `remove-before-release` | `resistance-forces-supports` | `legacy` |
| `train_troops` | `player` | `resistance-forces-supports` | `partial` |
| `train_troops_report` | `player` | `resistance-forces-supports` | `partial` |
| `undercover_check` | `player` | `civilians-undercover` | `partial` |
| `undercover_clear` | `player` | `civilians-undercover` | `partial` |
| `undercover_eligibility` | `player` | `civilians-undercover` | `partial` |
| `undercover_request` | `player` | `civilians-undercover` | `partial` |
| `vehicle_collect_loot` | `player` | `arsenal-garage-logistics` | `partial` |
| `vehicle_unload_loot` | `player` | `arsenal-garage-logistics` | `partial` |
| `withdraw_arsenal` | `player` | `arsenal-garage-logistics` | `partial` |

## Contextual world-action inventory

| Action class | Contract | Disposition |
| --- | --- | --- |
| `HST_GunShopOpenAction` | `mission-dynamic` | `partial` |
| `HST_HQArsenalLoadoutEditorAction` | `arsenal-garage-logistics` | `partial` |
| `HST_MissionCaptiveExtractAction` | `mission-rescue` | `partial` |
| `HST_MissionCaptiveFollowAction` | `mission-rescue` | `partial` |
| `HST_MissionCaptiveFreeAction` | `mission-rescue` | `partial` |
| `HST_MissionCargoDeliverAction` | `mission-logistics` | `legacy` |
| `HST_MissionCargoLoadAction` | `mission-logistics` | `legacy` |
| `HST_MissionCargoUnloadAction` | `mission-logistics` | `legacy` |
| `HST_MissionConvoyCaptureVehicleAction` | `mission-convoy` | `partial` |
| `HST_MissionConvoyRecoverPayloadAction` | `mission-convoy` | `partial` |
| `HST_MissionDestroyTargetSabotageAction` | `mission-destroy` | `partial` |
| `HST_MissionHVTConfirmAction` | `mission-assassination` | `partial` |
| `HST_PetrosArsenalMenuAction` | `arsenal-garage-logistics` | `partial` |
| `HST_PetrosCommandMenuAction` | `ui-projection` | `partial` |
| `HST_PetrosMoveBaseHereAction` | `hq-command-economy` | `partial` |
| `HST_VehicleCollectLootAction` | `arsenal-garage-logistics` | `partial` |
| `HST_VehicleUnloadLootAction` | `arsenal-garage-logistics` | `partial` |

## Coverage rule

Generation fails if the runtime/config mission sets differ, a mission category lacks a contract, the command-ID source set differs from the explicit manifest, a concrete contextual action class differs from its manifest, an action lacks a mapping, or a row uses unknown vocabulary. Coverage is therefore reproducible, while certification remains package- and evidence-dependent.
