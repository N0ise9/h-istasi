# Partisan Current Status

> Generated from `docs/data/release_status.json` and `docs/data/antistasi_ce311_parity.json`. Do not hand-edit this file; run `tools/update-release-docs.ps1`.

## Release decision

**NO-GO - development alpha.** No release-candidate package is certified.

The retained candidate identity below binds its exact source HEAD, manifest, canonical four-file package index, addon identity, and validation tools. The generator verifies that the candidate source is between the audited gameplay revision and the live checkout HEAD.

## Identity

| Field | Current value |
| --- | --- |
| Status data as of | `2026-07-19T00:14:27.8147977Z` |
| Audited gameplay Git HEAD | `25dedb3e82ad516c28826830bc1e06a2d3940f53` |
| Embedded implementation identity | `32727238d74b29905c68e5a80bb5897dfdc783c0` |
| Embedded build UTC / label | `2026-07-18T16:34:38Z` / `schema71-settings24-focused-force-authority` |
| Campaign / runtime-settings schema | `71` / `24` |
| Workbench CRC | `f27e637b` |
| Release candidate / source HEAD | `partisan-rc-b8deddc4b631-20260718T213322Z` / `b8deddc4b6314936b7ea04f36a35784622a46da6` |
| Runtime use disposition | `active-runtime-candidate` |
| Candidate manifest | `docs/evidence/release-candidates/partisan-rc-b8deddc4b631-20260718T213322Z/candidate.json` |
| Manifest / ready-seal SHA-256 | `c88d363423008bcba2366afa8d458613bf539c549969bcae73cd82e5cd9402a5` / `6518c536e7104f21af81c94c4959a66587c4efe67bf2339001489dcddad00d87` |
| Aggregate package SHA-256 | `82e1fd0bf7c3404b7fe842fa84efd10f225bf82fc76c11502b9a684b63f4f329` (sha256-manifest-v1 over the canonical four-file package index) |
| Addon GUID / revision / version | `698532771130111D` / `unpublished-local-pack` / `0.1.0-rc.20260718T213322Z.b8deddc4` |
| Workbench/tool identity | version `1.7.0.54` / SHA-256 `59ee98c352932c1aa8fb29970a377c1a9ea2f839e31d9ab072239212909d54c0` / validation CRC `f27e637b` |
| Server / client versions | `1.7.0.54` / `1.7.0.54` |

## Proof ladder

A pass never inherits upward. `partial` means some scoped evidence exists but the rung is not passed for the enabled campaign.

| Rung | Status | Honest scope |
| --- | --- | --- |
| Static/source/resource contracts | `passed` | Retained Foundation evidence is green inside the immutable candidate build for its exact source HEAD. |
| Enforce compile and configuration | `passed` | All five explicit Workbench targets are retained and green for the candidate source and canonical four-file package build. |
| Deterministic service contracts | `passed-noncertifying` | All five release-critical service suites passed individually against the exact staged packed candidate with 5/0/0/0 JUnit totals, exact classified diagnostics, and zero residue. This scoped rung does not inherit into native-world or later gates. |
| Native engine-world behavior | `failed` | The corrected package-bound force-authority canary failed closed on one unapproved MapLocator runtime diagnostic, so the unchanged candidate requires a source-fixed replacement. The older full report remains preliminary, unaccepted, and red. Scoped passing cuts remain useful, but this rung cannot be partial-green while a current subset fails. |
| Packaged dedicated server | `not-run` | The sealed candidate is retained but rejected for further promotion by the corrected canary. Its source-fixed replacement has not yet been built or launched through the standard dedicated-server runtime gate. |
| Multiple clients, reconnect, and JIP | `not-run` | Host, two-client, reconnect, late-join, and packet-disruption convergence are not certified. |
| Fresh-process restart and fault injection | `partial` | Selected journal, shutdown, field-vehicle, exact-QRF, counterattack, and rebuild cuts pass; the arbitrary full campaign graph and fault matrix remain open. |
| Performance and long soak | `not-run` | The reported one-second hitch and long-campaign capacity limits have no current immutable-package soak evidence. |
| Canary release | `blocked` | Blocked by every lower unproven release rung. |
| Stable certification | `blocked` | No current matrix row is certified. |

## Retained evidence

- Foundation: **passed** at 874 references for `b8deddc4b6314936b7ea04f36a35784622a46da6`.
- Workbench: **passed** at 5846 files / 11899 classes / CRC `f27e637b` for `b8deddc4b6314936b7ea04f36a35784622a46da6`.
- Packaged focused autotests: **5/5** cases and JUnit **5/0/0/0** tests/failures/errors/skips against exact candidate `partisan-rc-b8deddc4b631-20260718T213322Z`. Hard diagnostics are explicitly not free: 11 total = 10 approved stock + 1 approved intentional + 0 unapproved, with 40 envelope files rehashed and zero cleanup/spill residue. Summary: `docs/evidence/focused-autotest/partisan-rc-b8deddc4b631-20260718T213322Z.json` / SHA-256 `8bb36919f0649e0f48fad50305878ec883cf98a0021323ba1442017f1aa113b8`; harness `b3fc1e6f56d9cf8805bac1702a54e0b5284e0043`. This is passed non-certifying service evidence.
- Focused force-authority profile: **35/35** cases and **87/87** counted conditions for `32727238d74b29905c68e5a80bb5897dfdc783c0`, with `CertificationPassed:false`. This is historical state-only, non-package, non-certifying evidence.
- Corrected force-authority canary: **rejected fail-closed** on exact candidate `partisan-rc-b8deddc4b631-20260718T213322Z`. The focused proof remained 35/35 assertions and 87/87 counted certification conditions, but the 33-check classifier found 3 hard diagnostics = 2 approved stock + 0 approved intentional + 1 unapproved `virtual-machine-exception`. All 10 envelope files rehashed with zero cleanup/spill residue. Summary: `docs/evidence/campaign-debug/partisan-rc-b8deddc4b631-20260718T213322Z-corrected-canary-20260719T001339Z.json` / SHA-256 `2f9656ecf6e8640f4203e231dfe531d0b5eda017619e4f0d83a3e823e66d6951`; harness `38a094fe223232145801bd60e707adf0c80c13d2`. The unchanged candidate is not accepted for further promotion.
- Earlier Full Campaign Debug capture: **preliminary and unaccepted** on exact candidate `partisan-rc-b8deddc4b631-20260718T213322Z`: 584 PASS, 49 WARN, 46 FAIL, 7 BLOCKED, and 1 SKIPPED; 5562/5688 required assertions proven, with 112 failed and 14 blocked. Candidate identity, packed mount, artifact stability, cleanup, and envelope rehash were mechanically verified, but the original wrapper missed timestamp-prefixed errors. Its corrected overlay found 3 canary diagnostics with 1 unapproved and 25 full-run diagnostics with 10 unapproved; wrapper-reported success is not acceptance. Summary: `docs/evidence/campaign-debug/partisan-rc-b8deddc4b631-20260718T213322Z.json` / SHA-256 `0198f244322fceae63bc0d345a0fa3330e52af3886c540530189054e425538f9`; capture harness `1bff1890830db08159826f63b550227aa7bb0da3`.

## Specification coverage

- CE 3.11.1 product contract rows: **24** (development-only 1; legacy 3; missing 2; partial 18).
- Configured/runtime mission rows: **39/39**, all mapped to a product contract row.
- Routed command/action IDs: **177**, exact source/manifest set match and all mapped.
- Concrete contextual action classes: **17**, exact source/manifest set match and all mapped.
- Product specification: `docs/ANTISTASI_CE311_PARITY_MATRIX.md`.

Coverage means the surface is named and classified. It does not mean the behavior is exact or certified.

## Active blockers

| ID | Category | Blocker |
| --- | --- | --- |
| `STATUS-001` | `AUTH` | The corrected fail-closed canary rejected the unchanged candidate on one MapLocator runtime diagnostic. Fix that source defect and seal a new candidate before rerunning Campaign Debug; dedicated, multiplayer/JIP, restart breadth, performance, and soak remain pending. |
| `STATUS-002` | `TEST` | The clean corrected-harness force-authority canary preserved 35/35 focused assertions and 87/87 counted conditions but correctly failed closed on one unapproved MapLocator virtual-machine exception. The older full capture remains preliminary and unaccepted with 46 failed cases, 7 blocked cases, 112 failed required assertions, and 14 blocked required assertions. |
| `STATUS-003` | `UI` | Known command-menu and modal-map defects remain open until source correction plus rendered packaged-client proof. |
| `STATUS-004` | `MOVE` | Natural sustained infantry and convoy travel, identical-waypoint suppression, and measured no-stutter behavior are not proven. |
| `STATUS-005` | `PROJ` | Campaign read-model convergence is not proven with host, two clients, reconnect, JIP, restart, and marker-cap boundaries. |
| `STATUS-006` | `PERF` | The one-second hitch and bounded-history admission cliffs remain unclosed. |
| `STATUS-007` | `MISSION` | Configured mission breadth still exceeds mission-specific CE 3.11.1 behavioral parity and proof. |
| `STATUS-008` | `SEC` | Development proof and destructive diagnostic surfaces have not yet been separated from the release package. |

## Next release-closure step

The corrected canary rejected exact candidate `partisan-rc-b8deddc4b631-20260718T213322Z` for further promotion. Fix the single MapLocator runtime lifecycle defect, seal one source-fixed replacement candidate with a new package identity, and rerun the corrected force-authority canary before any full profile. Preserve this candidate and its earlier full capture as immutable rejected evidence.
