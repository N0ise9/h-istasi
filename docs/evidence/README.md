# Release-Candidate Identity Evidence

This directory retains portable, byte-identical copies of each published
candidate's `candidate.json` manifest and `candidate.ready.json` seal. The
generated current-status check verifies their hashes and cross-checks their
source, package, addon, toolchain, Workbench, and ready-seal identities.

These small tracked records are not substitutes for the immutable package and
raw evidence bundle. Proof attached to an active runtime candidate must consume
the exact candidate ID and aggregate package SHA-256 named by its manifest.
Rebuilding starts a new candidate directory and evidence chain; existing
records are not rewritten.
Repository attributes keep both JSON records on canonical LF endings so their
byte hashes remain stable across checkouts.

Current status retains `partisan-rc-e11e7ea88a44-20260719T040154Z` as a
`rejected-after-runtime`, verification-only candidate. Version
`0.1.0-rc.20260719T040154Z.e11e7ea8` was built from
clean source HEAD `e11e7ea88a44ea07d7a81c0b4009f029f0b297e1`; its exact
four-file package SHA-256 is
`75b61eb19513de00e56a43ad3778885f89a7497c0eebe4d870bf3b11e62a0dad`,
manifest SHA-256 is
`daed6876ce839a7fc6551257e4a4dd9bb0c92772c7e2d07be595acddde19e714`,
and ready-seal SHA-256 is
`0ca7a5e2fbe6bf298baa542250cc7b47bf2b135a5382e032fc5febdddf579acc`.
Foundation passed all 874 checks. All five Workbench targets passed at 5,848
files/11,901 classes, common CRC `e4cde465`, zero hard errors, and exact-zero
cleanup. The manifest binds four package files and 50 evidence files.

The then-active candidate's package-bound focused set is accepted as
`passed-noncertifying`. Clean harness `b1940f2` ran all five cases against the
exact staged packed candidate from `2026-07-19T04:44:01.2295133Z` through
`2026-07-19T04:45:58.8756237Z`: JUnit 5/0/0/0, all 40 envelope files rehashed,
11 hard diagnostics = ten approved stock + one approved intentional + zero
unapproved, and zero cleanup/spill residue. The portable summary is
`focused-autotest/partisan-rc-e11e7ea88a44-20260719T040154Z.json`, SHA-256
`9ddade1cb86a209acf4aae02ded6f1a7713fe1e25ba577ae00ef1980e3de149a`.
This closes only the deterministic-service rung. The native-engine/world rung
failed for this package at the corrected `force_authority` canary. Clean harness
HEAD `937c86c5d2259a9da270ea76371001ac1d4c6eed` retained run
`seed1985_t0_p1_u1784437399` under leaf
`20260719T050302Z-0bbd740b4f0149baa0f34944dbd70fc9` from
`2026-07-19T05:03:02.0611638Z` through
`2026-07-19T05:03:41.5393020Z`. Candidate identity, packed mount, package
bytes, and artifacts remained exact and stable. The 11 cases ended at 8 PASS,
1 WARN, 1 FAIL, 1 BLOCKED, and 0 SKIPPED; focused proof was 33/35 and 85/87,
with `ownership_transition.aggregate` and `ownership_transition.causes` failed.
State restoration was 18/0, final orphan cleanup passed, all ten envelope files
rehashed, and cleanup/spill residue was zero. The classifier was valid with two
approved stock diagnostics and zero unapproved diagnostics. Envelope SHA-256 is
`8deca62633394025bfa976f6d883f9b500d56519fd13e875f241679f4799cd21`.
The rejected portable summary is
`campaign-debug/partisan-rc-e11e7ea88a44-20260719T040154Z-corrected-canary-20260719T050302Z.json`,
SHA-256 `af0aca25a84d8f757dbba8010950a658ce09937aa4048c35b2e372f1183eec69`.
The full profile was correctly stopped and remains `not-run`; release remains
`NO-GO`.

The canary isolated a stale proof-fixture contract rather than package drift.
Production now requires `FindActiveMission` to resolve the supplied source
before classifying a capture as mission-caused, but the ownership proof fixtures
still supplied unbacked mission IDs. The source correction seeds the exact
active missions in both fixtures, pins mission cause/type/ID provenance, retains
an unresolved-source negative case as military capture, and separates political
from mission diagnostics. Because that correction changes source, it requires a
new immutable candidate and a fresh focused/canary/full evidence chain. It does
not repair or transfer evidence from this rejected package.

`partisan-rc-c2b16c4a2d85-20260718T201442Z`,
`partisan-rc-b8deddc4b631-20260718T213322Z`, and
`partisan-rc-0e632ec4f63e-20260719T004133Z` remain sealed as superseded artifact
evidence. The last of those retains its accepted focused and corrected-canary
results plus its rejected full result as immutable historical evidence. None of
their package-bound results can be attached to this rejected package or any
replacement.

Runtime results are not appended to these sealed candidate directories. Each
candidate-aware runner writes a fresh external sidecar run whose portable
envelope binds raw-file hashes to candidate, package, manifest, ready seal,
harness, runtime-tool, result, and cleanup identities. Only a deliberately
selected portable summary may later be tracked here.

The accepted classifier-aware five-case summary for the prior candidate is
`focused-autotest/partisan-rc-b8deddc4b631-20260718T213322Z.json`. It binds the
five external envelope hashes, exact candidate and harness identities, per-case
JUnit and diagnostic counts, aggregate totals, rehash status, and cleanup/spill
status without copying machine-local paths or mutable raw logs into the
repository. The raw sidecars remain external and immutable.
Repository attributes also keep these tracked summary JSON files on canonical
LF endings so the status-recorded digest remains stable across checkouts.

The prior candidate's original package-bound Campaign Debug evidence is
summarized in
`campaign-debug/partisan-rc-b8deddc4b631-20260718T213322Z.json`. It binds the
retained original focused canary and full run to the exact candidate, clean
harness identity, runner/module, guarded settings, envelope hashes, result
totals, category counts, rehash/cleanup predicates, corrected diagnostic audit,
and historical aggregate comparison. The original immutable raw files are
mechanically complete and stable, but their wrapper-reported success and zero
census are not acceptance. The timestamp-aware audit records three canary
diagnostics = two exact approved stock + one unapproved. The full capture has 25
diagnostics with a 19-line Partisan subset; across all 25, two are exact approved
stock, 13 are proof-bound intentional, and ten are unapproved.

The separate corrected-canary summary is
`campaign-debug/partisan-rc-b8deddc4b631-20260718T213322Z-corrected-canary-20260719T001339Z.json`.
Clean classifier harness `38a094f` reran only the `force_authority` canary
against the unchanged package. Its proof recorded 35/35 and 87/87, while the
corrected census was three = two approved stock + one unapproved map-locator VM
exception. Cleanup/spill residue was zero, but the runner failed closed. The
full profile was correctly not rerun after that rejection. All candidate-bound
captures remain preliminary-unaccepted, and the retained full report remains
failed.

The accepted classifier-aware five-case summary for the superseded
`partisan-rc-0e632ec4f63e-20260719T004133Z` candidate is
`focused-autotest/partisan-rc-0e632ec4f63e-20260719T004133Z.json`, SHA-256
`961ef6b0a84c26446468b31dd7ac5120448b21a442e9a823de4ff5dc804da7f9`.
Clean harness HEAD `d4d8f29cda9896ce2c6a5b073dac2cbd03757700` completed the set from
`2026-07-19T01:08:50.9577409Z` through
`2026-07-19T01:09:44.4465092Z`. All five cases passed with JUnit 5/0/0/0; all
40 envelope files rehashed, every run passed 12 classifier checks, cleanup and
spill residue were zero, and the exact census was 11 = ten approved stock + one
approved intentional + zero unapproved. This is a `passed-noncertifying`
deterministic-service rung, not corrected-canary, Full Campaign Debug, or
runtime proof of the map-locator correction.

The accepted corrected-canary summary for that superseded candidate is
`campaign-debug/partisan-rc-0e632ec4f63e-20260719T004133Z-corrected-canary-20260719T012319Z.json`,
SHA-256 `f47fa5f0539c0c8c6024e096f3e034699bc6bfaf656734a0a2b32c9fee7b4aa8`.
Clean harness HEAD `20375141f840f74316ca46e7df047fcba3e6e344` retained run
`seed1985_t0_p1_u1784424219` under leaf
`20260719T012319Z-47423d741d0e4690b3c7dbbbab68cebd`. The run started at
`2026-07-19T01:23:19.5837772Z`, completed at
`2026-07-19T01:24:02.2143579Z`, and recorded 40 canonical runtime seconds. Its
11 cases were 9 PASS/1 WARN/0 FAIL/1 BLOCKED/0 SKIPPED; focused proof passed
35/35 assertions and 87/87 certification-counting conditions. State
restoration was 18/0, final orphan cleanup passed, all ten envelope files
rehashed, and every cleanup/spill counter was zero. The exact diagnostic census
was two approved stock + zero approved intentional + zero unapproved, with all
33 classifier self-tests passing. Envelope SHA-256 is
`e3705a849590b9fd3086fdb0caf5659df6e0c1029784612965c848a0f8f0a851`.
The prior map-locator exception is absent for this scoped path. This is accepted
`passed-noncertifying` native-engine/world canary evidence, not full
certification.

The immutable rejected full-profile summary for that superseded candidate is
`campaign-debug/partisan-rc-0e632ec4f63e-20260719T004133Z-full-20260719T014151Z.json`,
SHA-256 `ed225ba2acb6932437af55219ff0b6ba69f4a2111880acd11c2555c875819ca7`.
Clean harness HEAD `27052811bb192835fc09ab3cb052b36cabad5df4` retained run
`seed1985_t0_p1_u1784425330` under leaf
`20260719T014151Z-470870c9cc7e4493afb9a6ceb6ff2bce`. The wrapper completed a
mechanically valid capture with stable artifacts, ten rehashed envelope files,
18/0 state restoration, final orphan cleanup, and zero cleanup/spill residue.
Runtime acceptance and certification remained false: 584 PASS/49 WARN/46
FAIL/7 BLOCKED/1 SKIPPED and 5,561/5,687 required assertions proven, with 112
failed and 14 blocked. The classifier found 25 hard diagnostics = two approved
stock + 13 approved intentional + ten unapproved. Envelope SHA-256 is
`f61bd05fcc5c95c5d0ddbbeb46a9220771d116b86bad1ad4f26340f4853ec825`.
This is a rejected red historical full-profile boundary. That candidate's
native-engine/world rung is `failed`; its map-locator source fix is sealed only
in the retained rejected replacement and may not be attached to this immutable
package.

Canonical LF is required for the tracked summaries, and the release generator
rehashes and cross-checks them against current status without transferring the
prior package's evidence to the new package.

If a package defect requires a rebuild, the older record remains byte-identical
and is described as retained superseded evidence. It remains available for
archive validation and consumer preflight, but not current runtime proof. No
result from that package may be combined with the replacement candidate's
evidence chain.
