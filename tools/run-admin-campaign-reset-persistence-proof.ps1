[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,

    [Parameter(Mandatory = $true)]
    [string]$RuntimeAddonRoot,

    [Parameter(Mandatory = $true)]
    [string]$WorkbenchExecutable,

    [string]$ProjectPath = "",

    [string]$WorldResource = "Worlds/HST_Everon/HST_Everon.ent",

    [string[]]$WatchedRoots = @(),

    [string[]]$SpillRoots = @(),

    [ValidateRange(30, 3600)]
    [int]$StageTimeoutSeconds = 300,

    [ValidateRange(30, 900)]
    [int]$PackTimeoutSeconds = 180,

    [ValidateRange(100, 5000)]
    [int]$PollMilliseconds = 500,

    [ValidateRange(1, 60)]
    [int]$ResultGraceSeconds = 5,

    [switch]$PreflightOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# Import only the guarded process, packing, hashing, ownership, snapshot, and
# cleanup library. This seam also owns the single C# job/process helper type;
# the administrative proof must never carry a second copy of that machinery.
$ordinaryLibrary = Join-Path $PSScriptRoot `
    "run-ordinary-campaign-persistence-proof.ps1"
. $ordinaryLibrary `
    -Executable $Executable `
    -RuntimeAddonRoot $RuntimeAddonRoot `
    -WorkbenchExecutable $WorkbenchExecutable `
    -ProjectPath $ProjectPath `
    -WorldResource $WorldResource `
    -WatchedRoots $WatchedRoots `
    -SpillRoots $SpillRoots `
    -StageTimeoutSeconds $StageTimeoutSeconds `
    -PackTimeoutSeconds $PackTimeoutSeconds `
    -PollMilliseconds $PollMilliseconds `
    -ResultGraceSeconds $ResultGraceSeconds `
    -PreflightOnly:$PreflightOnly `
    -LibraryOnly

$script:Stages = @(
    "prepare_old_checkpoint",
    "reset_commit",
    "stale_native_no_save_verify")
$script:StageOrdinals = @{
    prepare_old_checkpoint = 0
    reset_commit = 1
    stale_native_no_save_verify = 2
}
$script:ExpectedSources = @{
    prepare_old_checkpoint = "new_campaign"
    reset_commit = "native"
    stale_native_no_save_verify = "profile_fallback"
}
$script:ExpectedSourceJournalGenerations = @{
    prepare_old_checkpoint = -1
    reset_commit = 1
    stale_native_no_save_verify = 3
}
$script:ExpectedSourceJournalSlots = @{
    prepare_old_checkpoint = ""
    reset_commit = "canonical"
    stale_native_no_save_verify = "canonical"
}
$script:ExpectedSourceJournalValidSlotCounts = @{
    prepare_old_checkpoint = 0
    reset_commit = 1
    stale_native_no_save_verify = 2
}
$script:ExpectedCommittedJournalGenerations = @{
    prepare_old_checkpoint = 1
    reset_commit = 3
    stale_native_no_save_verify = 3
}
$script:ExpectedCommittedJournalSlots = @{
    prepare_old_checkpoint = "canonical"
    reset_commit = "canonical"
    stale_native_no_save_verify = "canonical"
}
$script:ExpectedCommittedJournalValidSlotCounts = @{
    prepare_old_checkpoint = 1
    reset_commit = 2
    stale_native_no_save_verify = 2
}
$script:ExpectedSaveTypes = @{
    prepare_old_checkpoint = "manual"
    reset_commit = "manual"
    stale_native_no_save_verify = "none"
}
$script:ExpectedSaveNames = @{
    prepare_old_checkpoint = "Partisan manual checkpoint"
    reset_commit = "Partisan manual checkpoint"
    stale_native_no_save_verify = ""
}
$script:ExpectedCheckpointSequences = @{
    prepare_old_checkpoint = 2
    reset_commit = 5
    stale_native_no_save_verify = 6
}
$script:ExpectedRestoreSequences = @{
    prepare_old_checkpoint = 0
    reset_commit = 1
    stale_native_no_save_verify = 2
}

$script:MissionHeader = "Missions/HST_Everon.conf"
$script:ScenarioId = "{6985327711302100}Missions/HST_Everon.conf"
$script:ProjectId = "698532771130111D"
$script:OwnerMagic =
    "partisan_admin_campaign_reset_persistence_owner_v1"
$script:GuardMagic =
    "partisan_admin_campaign_reset_persistence_guard_v1"
$script:CarrierMagic =
    "partisan_admin_campaign_reset_persistence_carrier_v1"
$script:ResultMagic =
    "partisan_admin_campaign_reset_persistence_result_v1"
$script:OwnerPurpose = "admin_campaign_reset_persistence"
$script:AuthorityVersion = 1
$script:ExpectedStageCount = 3
$script:EnvelopeMagic = "partisan_campaign_profile_journal_v1"
$script:EnvelopeVersion = 1
$script:GuardBaseLeaf = "PartisanAdminCampaignResetPersistence"
$script:GuardLeafPrefix = "PartisanAdminCampaignResetPersistenceGuard_"
$script:GuardSentinelLeaf = ".partisan-admin-reset-persistence-owner"
$script:WorkspacePackScratchLeaf = ".tmp-native-pack"
$script:WorkspacePackScratchSentinelLeaf =
    ".partisan-native-pack-owner"
# Packing either proof uses the same checkout scratch boundary. Sharing the
# mutex prevents a check-then-create race between the two guarded runners.
$script:MutexName = "Local\PartisanOrdinaryCampaignPersistenceGuard"
$script:SentinelVersion = 1
$script:SnapshotHashMaximumBytes = 65536
$script:ArtifactPrefix = "HST_AdminCampaignResetPersistenceProof_"

function Assert-AdminProofOwner {
    param(
        [Parameter(Mandatory = $true)]$Owner,
        [Parameter(Mandatory = $true)][string]$SessionNonce,
        [Parameter(Mandatory = $true)][string]$RunId,
        [Parameter(Mandatory = $true)][string]$PayloadNonce,
        [Parameter(Mandatory = $true)]$ExpectedBuild,
        [Parameter(Mandatory = $true)][string]$ExpectedWorld
    )

    $label = "admin reset persistence engine owner"
    foreach ($property in @(
        "m_sMagic", "m_iVersion", "m_sPurpose", "m_sSessionNonce",
        "m_sRunId", "m_sPayloadNonce", "m_sBuildSha", "m_sBuildUtc",
        "m_sBuildLabel", "m_iCampaignSchemaVersion",
        "m_iSettingsSchemaVersion", "m_sWorld",
        "m_iExpectedStageCount", "m_bDisposableProfile")) {
        Assert-JsonProperty $Owner $property $label
    }
    Assert-LowerHexNonce $SessionNonce "session nonce"
    Assert-LowerHexNonce $PayloadNonce "payload nonce"
    if ([string]$Owner.m_sMagic -cne $script:OwnerMagic -or
        [int]$Owner.m_iVersion -ne $script:AuthorityVersion -or
        [string]$Owner.m_sPurpose -cne $script:OwnerPurpose -or
        [string]$Owner.m_sSessionNonce -cne $SessionNonce -or
        [string]$Owner.m_sRunId -cne $RunId -or
        [string]$Owner.m_sPayloadNonce -cne $PayloadNonce -or
        [string]$Owner.m_sWorld -cne $ExpectedWorld -or
        [int]$Owner.m_iExpectedStageCount -ne $script:ExpectedStageCount -or
        $Owner.m_bDisposableProfile -isnot [bool] -or
        -not [bool]$Owner.m_bDisposableProfile) {
        throw "$label identity is not exact."
    }
    Assert-BuildIdentity $Owner $ExpectedBuild $label
    return $Owner
}

function Assert-AdminProofGuard {
    param(
        [Parameter(Mandatory = $true)]$Guard,
        [Parameter(Mandatory = $true)][string]$SessionNonce,
        [Parameter(Mandatory = $true)][string]$StageNonce,
        [Parameter(Mandatory = $true)][string]$RunId,
        [Parameter(Mandatory = $true)][string]$PayloadNonce,
        [Parameter(Mandatory = $true)][string]$Stage,
        [AllowEmptyString()][string]$ExpectedLoadSavePointId,
        [Parameter(Mandatory = $true)]$ExpectedBuild,
        [Parameter(Mandatory = $true)][string]$ExpectedWorld
    )

    $label = "$Stage one-use admin reset persistence guard"
    foreach ($property in @(
        "m_sMagic", "m_iVersion", "m_sSessionNonce", "m_sStageNonce",
        "m_sRunId", "m_sPayloadNonce", "m_sRequestedStage",
        "m_iStageOrdinal", "m_sBuildSha", "m_sBuildUtc",
        "m_sBuildLabel", "m_iCampaignSchemaVersion",
        "m_iSettingsSchemaVersion", "m_sWorld", "m_sExpectedSource",
        "m_sExpectedLoadSavePointId", "m_iExpectedJournalGeneration",
        "m_bAllowCanonicalCampaignOverwrite", "m_bNoSaveStage")) {
        Assert-JsonProperty $Guard $property $label
    }
    $writeStage = $Stage -cne "stale_native_no_save_verify"
    $noSaveStage = -not $writeStage
    if ([string]$Guard.m_sMagic -cne $script:GuardMagic -or
        [int]$Guard.m_iVersion -ne $script:AuthorityVersion -or
        [string]$Guard.m_sSessionNonce -cne $SessionNonce -or
        [string]$Guard.m_sStageNonce -cne $StageNonce -or
        [string]$Guard.m_sRunId -cne $RunId -or
        [string]$Guard.m_sPayloadNonce -cne $PayloadNonce -or
        [string]$Guard.m_sRequestedStage -cne $Stage -or
        [int]$Guard.m_iStageOrdinal -ne $script:StageOrdinals[$Stage] -or
        [string]$Guard.m_sWorld -cne $ExpectedWorld -or
        [string]$Guard.m_sExpectedSource -cne
            $script:ExpectedSources[$Stage] -or
        [string]$Guard.m_sExpectedLoadSavePointId -cne
            $ExpectedLoadSavePointId -or
        [int]$Guard.m_iExpectedJournalGeneration -ne
            $script:ExpectedSourceJournalGenerations[$Stage] -or
        $Guard.m_bAllowCanonicalCampaignOverwrite -isnot [bool] -or
        [bool]$Guard.m_bAllowCanonicalCampaignOverwrite -ne $writeStage -or
        $Guard.m_bNoSaveStage -isnot [bool] -or
        [bool]$Guard.m_bNoSaveStage -ne $noSaveStage) {
        throw "$label identity is not exact."
    }
    Assert-BuildIdentity $Guard $ExpectedBuild $label
    return $Guard
}

function New-AdminProofOwnerValue {
    param(
        [Parameter(Mandatory = $true)][string]$SessionNonce,
        [Parameter(Mandatory = $true)][string]$RunId,
        [Parameter(Mandatory = $true)][string]$PayloadNonce,
        [Parameter(Mandatory = $true)]$BuildIdentity,
        [Parameter(Mandatory = $true)][string]$World
    )

    return [ordered]@{
        m_sMagic = $script:OwnerMagic
        m_iVersion = $script:AuthorityVersion
        m_sPurpose = $script:OwnerPurpose
        m_sSessionNonce = $SessionNonce
        m_sRunId = $RunId
        m_sPayloadNonce = $PayloadNonce
        m_sBuildSha = $BuildIdentity.BuildSha
        m_sBuildUtc = $BuildIdentity.BuildUtc
        m_sBuildLabel = $BuildIdentity.BuildLabel
        m_iCampaignSchemaVersion = $BuildIdentity.CampaignSchemaVersion
        m_iSettingsSchemaVersion = $BuildIdentity.SettingsSchemaVersion
        m_sWorld = $World
        m_iExpectedStageCount = $script:ExpectedStageCount
        m_bDisposableProfile = $true
    }
}

function Get-AdminProofStageArgumentVector {
    param(
        [Parameter(Mandatory = $true)][string]$RuntimeAddonPath,
        [Parameter(Mandatory = $true)][string]$PackedAddonsParent,
        [Parameter(Mandatory = $true)][string]$ServerConfigPath,
        [Parameter(Mandatory = $true)][string]$ProfileRoot,
        [Parameter(Mandatory = $true)][string]$SessionNonce,
        [Parameter(Mandatory = $true)][string]$StageNonce,
        [Parameter(Mandatory = $true)][string]$RunId,
        [Parameter(Mandatory = $true)][string]$Stage,
        [AllowEmptyString()][string]$LoadSavePointId
    )

    if ($script:Stages -cnotcontains $Stage) {
        throw "Unknown admin reset persistence stage."
    }
    $baseProject = Join-Path $RuntimeAddonPath "data\ArmaReforger.gproj"
    if (-not (Test-Path -LiteralPath $baseProject -PathType Leaf)) {
        throw "The packed base game project is unavailable."
    }
    $arguments = @(
        "-addonsDir", ($RuntimeAddonPath + "," + $PackedAddonsParent),
        "-gproj", $baseProject,
        "-config", $ServerConfigPath,
        "-profile", $ProfileRoot,
        "-rpl-timeout-disable",
        "-noThrow",
        "-backendLocalStorage",
        "-maxFPS", "30")
    if (-not [string]::IsNullOrWhiteSpace($LoadSavePointId)) {
        Assert-NativeSavePointId `
            -SavePointId $LoadSavePointId `
            -Label "$Stage load save point"
        $arguments += @("-loadSessionSave", $LoadSavePointId)
    }
    $arguments += @(
        "-hstAdminCampaignResetPersistenceProof", "true",
        "-hstAdminCampaignResetPersistenceStage", $Stage,
        "-hstAdminCampaignResetPersistenceRunId", $RunId,
        "-hstAdminCampaignResetPersistenceSessionNonce", $SessionNonce,
        "-hstAdminCampaignResetPersistenceStageNonce", $StageNonce)

    foreach ($forbidden in @(
        "-world", "-addons", "-forceupdate", "-autoshutdown",
        "-keepSessionSave")) {
        if ($arguments -ccontains $forbidden) {
            throw "$Stage contains forbidden argument $forbidden."
        }
    }
    if ($Stage -ceq "prepare_old_checkpoint") {
        if ($arguments -ccontains "-loadSessionSave") {
            throw "$Stage must start without native load authority."
        }
    }
    elseif ($arguments -cnotcontains "-loadSessionSave") {
        throw "$Stage requires one explicit native load UUID."
    }
    return $arguments
}

function Read-AdminProofJournalEnvelope {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][int]$ExpectedGeneration,
        [Parameter(Mandatory = $true)][int]$ExpectedSchema,
        [AllowEmptyString()][string]$ExpectedFingerprint = "",
        [Parameter(Mandatory = $true)][int]$ExpectedPreviousGeneration,
        [AllowEmptyString()][string]$ExpectedPreviousFingerprint = ""
    )

    $label = "campaign profile journal generation $ExpectedGeneration"
    $value = Read-JsonArtifact -Path $Path
    foreach ($property in @(
        "m_sMagic", "m_iEnvelopeVersion", "m_iGeneration",
        "m_iSnapshotSchemaVersion", "m_sSnapshotFingerprint",
        "m_iPreviousGeneration", "m_sPreviousSnapshotFingerprint",
        "m_sSnapshotPayload")) {
        Assert-JsonProperty $value $property $label
    }
    if ([string]$value.m_sMagic -cne $script:EnvelopeMagic -or
        [int]$value.m_iEnvelopeVersion -ne $script:EnvelopeVersion -or
        [int]$value.m_iGeneration -ne $ExpectedGeneration -or
        [int]$value.m_iSnapshotSchemaVersion -ne $ExpectedSchema -or
        [int]$value.m_iPreviousGeneration -ne $ExpectedPreviousGeneration -or
        [string]$value.m_sPreviousSnapshotFingerprint -cne
            $ExpectedPreviousFingerprint -or
        [string]::IsNullOrWhiteSpace([string]$value.m_sSnapshotPayload)) {
        throw "$label envelope metadata is not exact."
    }
    $fingerprint = [string]$value.m_sSnapshotFingerprint
    if ($fingerprint -cnotmatch
        '^uuidv8-sha256-v1:[1-9][0-9]*:[0-9a-f]{8}-[0-9a-f]{4}-8[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$') {
        throw "$label fingerprint is not a bounded exact-payload identity."
    }
    if (-not [string]::IsNullOrWhiteSpace($ExpectedFingerprint) -and
        $fingerprint -cne $ExpectedFingerprint) {
        throw "$label fingerprint does not match its engine receipt."
    }
    $fingerprintLength = [int](($fingerprint -split ':')[1])
    if ($fingerprintLength -ne ([string]$value.m_sSnapshotPayload).Length) {
        throw "$label payload length does not match its fingerprint."
    }
    return $value
}

function Assert-AdminSnapshotFingerprint {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$Fingerprint,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if ($Fingerprint -cnotmatch
        '^uuidv8-sha256-v1:[1-9][0-9]*:[0-9a-f]{8}-[0-9a-f]{4}-8[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$') {
        throw "$Label is not a bounded snapshot fingerprint."
    }
}

function Assert-AdminProofCarrier {
    param(
        [Parameter(Mandatory = $true)]$Carrier,
        [Parameter(Mandatory = $true)][string]$SessionNonce,
        [Parameter(Mandatory = $true)][string]$RunId,
        [Parameter(Mandatory = $true)][string]$PayloadNonce,
        [Parameter(Mandatory = $true)][string]$Stage,
        [Parameter(Mandatory = $true)]$ExpectedBuild,
        [Parameter(Mandatory = $true)][string]$ExpectedWorld
    )

    $label = "$Stage admin reset persistence carrier"
    foreach ($property in @(
        "m_sMagic", "m_iVersion", "m_sSessionNonce", "m_sRunId",
        "m_sPayloadNonce", "m_sBuildSha", "m_sBuildUtc",
        "m_sBuildLabel", "m_iCampaignSchemaVersion",
        "m_iSettingsSchemaVersion", "m_sWorld",
        "m_iCompletedStageOrdinal", "m_sOldSentinelTaskId",
        "m_sOldSentinelFingerprint", "m_sPreservedPlayerIdentityId",
        "m_sPreservedPlayerFingerprint",
        "m_sPreservedCommanderIdentityId",
        "m_iOldMarkerProjectionEpoch", "m_iResetMarkerProjectionEpoch",
        "m_sOldSavePointId", "m_sOldSnapshotFingerprint",
        "m_iOldCheckpointSequence", "m_iOldRestoreSequence",
        "m_iOldJournalGeneration", "m_sOldJournalSlot",
        "m_iOldJournalValidSlotCount", "m_bOldJournalChainExact",
        "m_sBlockerSavePointId", "m_sBlockerSnapshotFingerprint",
        "m_iBlockerCheckpointSequence", "m_iBlockerRestoreSequence",
        "m_iBlockerJournalGeneration", "m_sBlockerJournalSlot",
        "m_iBlockerJournalValidSlotCount",
        "m_bBlockerJournalChainExact", "m_sResetSavePointId",
        "m_sResetSnapshotFingerprint", "m_iResetCheckpointSequence",
        "m_iResetRestoreSequence", "m_iResetJournalGeneration",
        "m_sResetJournalSlot", "m_iResetJournalValidSlotCount",
        "m_bResetJournalChainExact", "m_bInFlightCheckpointObserved",
        "m_bInFlightResetRejected",
        "m_bRejectedResetReturnedNoCheckpoint",
        "m_sRejectedResetBeforeFingerprint",
        "m_sRejectedResetAfterFingerprint",
        "m_bRejectedResetStateExact", "m_bRejectedResetSentinelExact",
        "m_bRejectedResetIdentityExact", "m_bRejectedResetEpochExact",
        "m_bBlockerCompletionReceived",
        "m_bBlockerNativeCommitSucceeded",
        "m_bBlockerProfileMirrorSaved",
        "m_bBlockerOnAfterSaveSucceeded",
        "m_bBlockerOnSaveCreatedObserved",
        "m_bResetRemovedSentinel", "m_bResetPreservedIdentity",
        "m_bResetAdvancedEpoch", "m_bResetRetainedSequenceFloors")) {
        Assert-JsonProperty $Carrier $property $label
    }

    $expectedCompletedOrdinal = if ($Stage -ceq
        "prepare_old_checkpoint") { 0 } else { 1 }
    if ([string]$Carrier.m_sMagic -cne $script:CarrierMagic -or
        [int]$Carrier.m_iVersion -ne $script:AuthorityVersion -or
        [string]$Carrier.m_sSessionNonce -cne $SessionNonce -or
        [string]$Carrier.m_sRunId -cne $RunId -or
        [string]$Carrier.m_sPayloadNonce -cne $PayloadNonce -or
        [string]$Carrier.m_sWorld -cne $ExpectedWorld -or
        [int]$Carrier.m_iCompletedStageOrdinal -ne
            $expectedCompletedOrdinal -or
        [string]$Carrier.m_sOldSentinelTaskId -cne
            "hst_admin_campaign_reset_old_sentinel" -or
        [string]$Carrier.m_sPreservedPlayerIdentityId -cne
            ("admin_reset_proof_" + $PayloadNonce) -or
        [string]$Carrier.m_sPreservedCommanderIdentityId -cne
            [string]$Carrier.m_sPreservedPlayerIdentityId -or
        [string]::IsNullOrWhiteSpace(
            [string]$Carrier.m_sPreservedPlayerFingerprint) -or
        [int]$Carrier.m_iOldMarkerProjectionEpoch -lt 1) {
        throw "$label identity or durable old-campaign evidence is not exact."
    }
    Assert-BuildIdentity $Carrier $ExpectedBuild $label
    Assert-AdminSnapshotFingerprint `
        -Fingerprint ([string]$Carrier.m_sOldSentinelFingerprint) `
        -Label "$label old sentinel fingerprint"
    Assert-NativeSavePointId `
        -SavePointId ([string]$Carrier.m_sOldSavePointId) `
        -Label "$label old checkpoint"
    Assert-AdminSnapshotFingerprint `
        -Fingerprint ([string]$Carrier.m_sOldSnapshotFingerprint) `
        -Label "$label old snapshot fingerprint"
    if ([int]$Carrier.m_iOldCheckpointSequence -ne 2 -or
        [int]$Carrier.m_iOldRestoreSequence -ne 0 -or
        [int]$Carrier.m_iOldJournalGeneration -ne 1 -or
        [string]$Carrier.m_sOldJournalSlot -cne "canonical" -or
        [int]$Carrier.m_iOldJournalValidSlotCount -ne 1 -or
        $Carrier.m_bOldJournalChainExact -isnot [bool] -or
        -not [bool]$Carrier.m_bOldJournalChainExact) {
        throw "$label old checkpoint tuple is not exact."
    }

    $resetFlagProperties = @(
        "m_bInFlightCheckpointObserved", "m_bInFlightResetRejected",
        "m_bRejectedResetReturnedNoCheckpoint",
        "m_bRejectedResetStateExact", "m_bRejectedResetSentinelExact",
        "m_bRejectedResetIdentityExact", "m_bRejectedResetEpochExact",
        "m_bBlockerCompletionReceived",
        "m_bBlockerNativeCommitSucceeded",
        "m_bBlockerProfileMirrorSaved",
        "m_bBlockerOnAfterSaveSucceeded",
        "m_bBlockerOnSaveCreatedObserved",
        "m_bResetRemovedSentinel", "m_bResetPreservedIdentity",
        "m_bResetAdvancedEpoch", "m_bResetRetainedSequenceFloors")
    if ($Stage -ceq "prepare_old_checkpoint") {
        if ([int]$Carrier.m_iResetMarkerProjectionEpoch -ne 0 -or
            -not [string]::IsNullOrWhiteSpace(
                [string]$Carrier.m_sBlockerSavePointId) -or
            -not [string]::IsNullOrWhiteSpace(
                [string]$Carrier.m_sBlockerSnapshotFingerprint) -or
            [int]$Carrier.m_iBlockerCheckpointSequence -ne 0 -or
            [int]$Carrier.m_iBlockerRestoreSequence -ne 0 -or
            [int]$Carrier.m_iBlockerJournalGeneration -ne 0 -or
            -not [string]::IsNullOrWhiteSpace(
                [string]$Carrier.m_sBlockerJournalSlot) -or
            [int]$Carrier.m_iBlockerJournalValidSlotCount -ne 0 -or
            [bool]$Carrier.m_bBlockerJournalChainExact -or
            -not [string]::IsNullOrWhiteSpace(
                [string]$Carrier.m_sResetSavePointId) -or
            -not [string]::IsNullOrWhiteSpace(
                [string]$Carrier.m_sResetSnapshotFingerprint) -or
            [int]$Carrier.m_iResetCheckpointSequence -ne 0 -or
            [int]$Carrier.m_iResetRestoreSequence -ne 0 -or
            [int]$Carrier.m_iResetJournalGeneration -ne 0 -or
            -not [string]::IsNullOrWhiteSpace(
                [string]$Carrier.m_sResetJournalSlot) -or
            [int]$Carrier.m_iResetJournalValidSlotCount -ne 0 -or
            [bool]$Carrier.m_bResetJournalChainExact -or
            -not [string]::IsNullOrWhiteSpace(
                [string]$Carrier.m_sRejectedResetBeforeFingerprint) -or
            -not [string]::IsNullOrWhiteSpace(
                [string]$Carrier.m_sRejectedResetAfterFingerprint)) {
            throw "$label contains premature reset evidence."
        }
        foreach ($property in $resetFlagProperties) {
            if ($Carrier.$property -isnot [bool] -or
                [bool]$Carrier.$property) {
                throw "$label reset flag $property is not clear."
            }
        }
        return $Carrier
    }

    Assert-NativeSavePointId `
        -SavePointId ([string]$Carrier.m_sBlockerSavePointId) `
        -Label "$label blocker checkpoint"
    Assert-NativeSavePointId `
        -SavePointId ([string]$Carrier.m_sResetSavePointId) `
        -Label "$label reset checkpoint"
    Assert-AdminSnapshotFingerprint `
        -Fingerprint ([string]$Carrier.m_sBlockerSnapshotFingerprint) `
        -Label "$label blocker snapshot fingerprint"
    Assert-AdminSnapshotFingerprint `
        -Fingerprint ([string]$Carrier.m_sResetSnapshotFingerprint) `
        -Label "$label reset snapshot fingerprint"
    if ([string]$Carrier.m_sBlockerSavePointId -ceq
            [string]$Carrier.m_sOldSavePointId -or
        [string]$Carrier.m_sResetSavePointId -ceq
            [string]$Carrier.m_sOldSavePointId -or
        [string]$Carrier.m_sResetSavePointId -ceq
            [string]$Carrier.m_sBlockerSavePointId -or
        [string]$Carrier.m_sBlockerSnapshotFingerprint -ceq
            [string]$Carrier.m_sOldSnapshotFingerprint -or
        [string]$Carrier.m_sResetSnapshotFingerprint -ceq
            [string]$Carrier.m_sOldSnapshotFingerprint -or
        [string]$Carrier.m_sResetSnapshotFingerprint -ceq
            [string]$Carrier.m_sBlockerSnapshotFingerprint -or
        [int]$Carrier.m_iResetMarkerProjectionEpoch -ne
            ([int]$Carrier.m_iOldMarkerProjectionEpoch + 1) -or
        [int]$Carrier.m_iBlockerCheckpointSequence -ne 4 -or
        [int]$Carrier.m_iBlockerRestoreSequence -ne 1 -or
        [int]$Carrier.m_iBlockerJournalGeneration -ne 2 -or
        [string]$Carrier.m_sBlockerJournalSlot -cne "recovery" -or
        [int]$Carrier.m_iBlockerJournalValidSlotCount -ne 2 -or
        $Carrier.m_bBlockerJournalChainExact -isnot [bool] -or
        -not [bool]$Carrier.m_bBlockerJournalChainExact -or
        [int]$Carrier.m_iResetCheckpointSequence -ne 5 -or
        [int]$Carrier.m_iResetRestoreSequence -ne 1 -or
        [int]$Carrier.m_iResetJournalGeneration -ne 3 -or
        [string]$Carrier.m_sResetJournalSlot -cne "canonical" -or
        [int]$Carrier.m_iResetJournalValidSlotCount -ne 2 -or
        $Carrier.m_bResetJournalChainExact -isnot [bool] -or
        -not [bool]$Carrier.m_bResetJournalChainExact -or
        [string]::IsNullOrWhiteSpace(
            [string]$Carrier.m_sRejectedResetBeforeFingerprint) -or
        [string]$Carrier.m_sRejectedResetBeforeFingerprint -cne
            [string]$Carrier.m_sRejectedResetAfterFingerprint) {
        throw "$label reset transition evidence is not exact."
    }
    Assert-AdminSnapshotFingerprint `
        -Fingerprint ([string]$Carrier.m_sRejectedResetBeforeFingerprint) `
        -Label "$label rejected reset snapshot fingerprint"
    foreach ($property in $resetFlagProperties) {
        if ($Carrier.$property -isnot [bool] -or
            -not [bool]$Carrier.$property) {
            throw "$label reset flag $property is not exact."
        }
    }
    return $Carrier
}

function Assert-AdminProofResult {
    param(
        [Parameter(Mandatory = $true)]$Result,
        [Parameter(Mandatory = $true)]$Carrier,
        [Parameter(Mandatory = $true)][string]$SessionNonce,
        [Parameter(Mandatory = $true)][string]$StageNonce,
        [Parameter(Mandatory = $true)][string]$RunId,
        [Parameter(Mandatory = $true)][string]$PayloadNonce,
        [Parameter(Mandatory = $true)][string]$Stage,
        [Parameter(Mandatory = $true)]$ExpectedBuild,
        [Parameter(Mandatory = $true)][string]$ExpectedWorld
    )

    $label = "$Stage admin reset persistence result"
    $properties = @(
        "m_sMagic", "m_iVersion", "m_sSessionNonce", "m_sStageNonce",
        "m_sRunId", "m_sPayloadNonce", "m_sStage", "m_iStageOrdinal",
        "m_sBuildSha", "m_sBuildUtc", "m_sBuildLabel",
        "m_iCampaignSchemaVersion", "m_iSettingsSchemaVersion",
        "m_sWorld", "m_bSuccess", "m_sExpectedSource", "m_sSource",
        "m_bSourceExact", "m_bPersistenceSystemAvailable",
        "m_bPersistenceSystemLoadedData", "m_bNativeRecordPresent",
        "m_bNativeRecordValid", "m_bProfileFallbackPresent",
        "m_bProfileFallbackRead", "m_bDegradedNativeRecovery",
        "m_sDegradedNativeRecoveryReason",
        "m_sNativeSnapshotFingerprint",
        "m_sProfileFallbackSnapshotFingerprint",
        "m_sSelectedSnapshotFingerprint",
        "m_iSourceJournalGeneration", "m_sSourceJournalSlot",
        "m_iSourceJournalValidSlotCount", "m_bSourceJournalLegacyRaw",
        "m_bSourceJournalChainExact", "m_iCommittedJournalGeneration",
        "m_sCommittedJournalSlot", "m_iCommittedJournalValidSlotCount",
        "m_bCommittedJournalLegacyRaw",
        "m_bCommittedJournalChainExact",
        "m_sCommittedJournalFingerprint", "m_iLiveOldSentinelCount",
        "m_sLiveOldSentinelFingerprint",
        "m_iStaleJournalOldSentinelCount",
        "m_sStaleJournalOldSentinelFingerprint",
        "m_bOldSentinelRejected", "m_sExpectedPlayerIdentityId",
        "m_sObservedPlayerFingerprint", "m_bPlayerIdentityExact",
        "m_sExpectedCommanderIdentityId",
        "m_sObservedCommanderIdentityId", "m_bCommanderIdentityExact",
        "m_iExpectedMarkerProjectionEpoch",
        "m_iObservedMarkerProjectionEpoch",
        "m_bMarkerProjectionEpochExact", "m_iExpectedCheckpointSequence",
        "m_iObservedCheckpointSequence", "m_iExpectedRestoreSequence",
        "m_iObservedRestoreSequence", "m_bDurableOrderExact",
        "m_sExpectedPriorSavePointId", "m_sObservedPriorSavePointId",
        "m_sCreatedSavePointId", "m_sActiveSavePointId",
        "m_sExpectedSaveType", "m_sCreatedSaveType",
        "m_sExpectedSaveName", "m_sCreatedSaveName",
        "m_bCampaignCaptured", "m_bTransientStateStaged",
        "m_bSavePointRequested", "m_bCompletionReceived",
        "m_bNativeCommitSucceeded", "m_bProfileMirrorSaved",
        "m_bCompletionObserverSucceeded", "m_bOnAfterSaveObserved",
        "m_bOnAfterSaveSucceeded", "m_bOnSaveCreatedObserved",
        "m_bActiveSaveExact", "m_bInFlightCheckpointObserved",
        "m_bInFlightResetRejected",
        "m_bRejectedResetReturnedNoCheckpoint",
        "m_sRejectedResetBeforeFingerprint",
        "m_sRejectedResetAfterFingerprint", "m_bRejectedResetStateExact",
        "m_bRejectedResetSentinelExact", "m_bRejectedResetIdentityExact",
        "m_bRejectedResetEpochExact", "m_bNoSaveStage",
        "m_bSavingDisabledBeforeClose", "m_bNoCheckpointRequested",
        "m_bNoSaveEventsObserved", "m_bActiveSaveUnchanged",
        "m_sEvidence")
    foreach ($property in $properties) {
        Assert-JsonProperty $Result $property $label
    }
    $booleanProperties = @($properties | Where-Object {
        $_ -clike "m_b*"
    })
    foreach ($property in $booleanProperties) {
        if ($Result.$property -isnot [bool]) {
            throw "$label property $property is not a boolean."
        }
    }

    if ([string]$Result.m_sMagic -cne $script:ResultMagic -or
        [int]$Result.m_iVersion -ne $script:AuthorityVersion -or
        [string]$Result.m_sSessionNonce -cne $SessionNonce -or
        [string]$Result.m_sStageNonce -cne $StageNonce -or
        [string]$Result.m_sRunId -cne $RunId -or
        [string]$Result.m_sPayloadNonce -cne $PayloadNonce -or
        [string]$Result.m_sStage -cne $Stage -or
        [int]$Result.m_iStageOrdinal -ne $script:StageOrdinals[$Stage] -or
        [string]$Result.m_sWorld -cne $ExpectedWorld -or
        -not [bool]$Result.m_bSuccess -or
        [string]::IsNullOrWhiteSpace([string]$Result.m_sEvidence)) {
        throw "$label identity or success receipt is not exact."
    }
    Assert-BuildIdentity $Result $ExpectedBuild $label

    if ([string]$Result.m_sExpectedSource -cne
            $script:ExpectedSources[$Stage] -or
        [string]$Result.m_sSource -cne $script:ExpectedSources[$Stage] -or
        -not [bool]$Result.m_bSourceExact -or
        -not [bool]$Result.m_bPersistenceSystemAvailable -or
        [int]$Result.m_iSourceJournalGeneration -ne
            $script:ExpectedSourceJournalGenerations[$Stage] -or
        [string]$Result.m_sSourceJournalSlot -cne
            $script:ExpectedSourceJournalSlots[$Stage] -or
        [int]$Result.m_iSourceJournalValidSlotCount -ne
            $script:ExpectedSourceJournalValidSlotCounts[$Stage] -or
        [bool]$Result.m_bSourceJournalLegacyRaw) {
        throw "$label startup source metadata is not exact."
    }
    if ($Stage -ceq "prepare_old_checkpoint") {
        if ([bool]$Result.m_bPersistenceSystemLoadedData -or
            [bool]$Result.m_bNativeRecordPresent -or
            [bool]$Result.m_bNativeRecordValid -or
            [bool]$Result.m_bProfileFallbackPresent -or
            [bool]$Result.m_bProfileFallbackRead -or
            [bool]$Result.m_bDegradedNativeRecovery -or
            -not [string]::IsNullOrWhiteSpace(
                [string]$Result.m_sDegradedNativeRecoveryReason) -or
            -not [string]::IsNullOrWhiteSpace(
                [string]$Result.m_sNativeSnapshotFingerprint) -or
            -not [string]::IsNullOrWhiteSpace(
                [string]$Result.m_sProfileFallbackSnapshotFingerprint) -or
            -not [string]::IsNullOrWhiteSpace(
                [string]$Result.m_sSelectedSnapshotFingerprint) -or
            [bool]$Result.m_bSourceJournalChainExact) {
            throw "$label fresh-campaign source evidence is not exact."
        }
    }
    elseif ($Stage -ceq "reset_commit") {
        if (-not [bool]$Result.m_bPersistenceSystemLoadedData -or
            -not [bool]$Result.m_bNativeRecordPresent -or
            -not [bool]$Result.m_bNativeRecordValid -or
            -not [bool]$Result.m_bProfileFallbackPresent -or
            -not [bool]$Result.m_bProfileFallbackRead -or
            [bool]$Result.m_bDegradedNativeRecovery -or
            -not [string]::IsNullOrWhiteSpace(
                [string]$Result.m_sDegradedNativeRecoveryReason) -or
            [string]$Result.m_sNativeSnapshotFingerprint -cne
                [string]$Carrier.m_sOldSnapshotFingerprint -or
            [string]$Result.m_sProfileFallbackSnapshotFingerprint -cne
                [string]$Carrier.m_sOldSnapshotFingerprint -or
            [string]$Result.m_sSelectedSnapshotFingerprint -cne
                [string]$Carrier.m_sOldSnapshotFingerprint -or
            -not [bool]$Result.m_bSourceJournalChainExact) {
            throw "$label native source evidence is not exact."
        }
    }
    else {
        if (-not [bool]$Result.m_bPersistenceSystemLoadedData -or
            -not [bool]$Result.m_bNativeRecordPresent -or
            -not [bool]$Result.m_bNativeRecordValid -or
            -not [bool]$Result.m_bProfileFallbackPresent -or
            -not [bool]$Result.m_bProfileFallbackRead -or
            -not [bool]$Result.m_bDegradedNativeRecovery -or
            -not ([string]$Result.m_sDegradedNativeRecoveryReason).Contains(
                "profile journal ordering is newer than the valid native record") -or
            [string]$Result.m_sNativeSnapshotFingerprint -cne
                [string]$Carrier.m_sBlockerSnapshotFingerprint -or
            [string]$Result.m_sProfileFallbackSnapshotFingerprint -cne
                [string]$Carrier.m_sResetSnapshotFingerprint -or
            [string]$Result.m_sSelectedSnapshotFingerprint -cne
                [string]$Carrier.m_sResetSnapshotFingerprint -or
            -not [bool]$Result.m_bSourceJournalChainExact) {
            throw "$label newer-journal recovery evidence is not exact."
        }
    }

    $noSaveStage = $Stage -ceq "stale_native_no_save_verify"
    if ($noSaveStage) {
        if ([int]$Result.m_iCommittedJournalGeneration -ne -1 -or
            -not [string]::IsNullOrWhiteSpace(
                [string]$Result.m_sCommittedJournalSlot) -or
            [int]$Result.m_iCommittedJournalValidSlotCount -ne 0 -or
            [bool]$Result.m_bCommittedJournalLegacyRaw -or
            [bool]$Result.m_bCommittedJournalChainExact -or
            -not [string]::IsNullOrWhiteSpace(
                [string]$Result.m_sCommittedJournalFingerprint)) {
            throw "$label incorrectly claims a journal commit."
        }
    }
    else {
        $expectedCommittedFingerprint = if ($Stage -ceq
            "prepare_old_checkpoint") {
            [string]$Carrier.m_sOldSnapshotFingerprint
        }
        else { [string]$Carrier.m_sResetSnapshotFingerprint }
        if ([int]$Result.m_iCommittedJournalGeneration -ne
                $script:ExpectedCommittedJournalGenerations[$Stage] -or
            [string]$Result.m_sCommittedJournalSlot -cne
                $script:ExpectedCommittedJournalSlots[$Stage] -or
            [int]$Result.m_iCommittedJournalValidSlotCount -ne
                $script:ExpectedCommittedJournalValidSlotCounts[$Stage] -or
            [bool]$Result.m_bCommittedJournalLegacyRaw -or
            -not [bool]$Result.m_bCommittedJournalChainExact -or
            [string]$Result.m_sCommittedJournalFingerprint -cne
                $expectedCommittedFingerprint) {
            throw "$label committed journal evidence is not exact."
        }
    }

    if ([string]$Result.m_sExpectedPlayerIdentityId -cne
            [string]$Carrier.m_sPreservedPlayerIdentityId -or
        [string]$Result.m_sObservedPlayerFingerprint -cne
            [string]$Carrier.m_sPreservedPlayerFingerprint -or
        -not [bool]$Result.m_bPlayerIdentityExact -or
        [string]$Result.m_sExpectedCommanderIdentityId -cne
            [string]$Carrier.m_sPreservedCommanderIdentityId -or
        [string]$Result.m_sObservedCommanderIdentityId -cne
            [string]$Carrier.m_sPreservedCommanderIdentityId -or
        -not [bool]$Result.m_bCommanderIdentityExact -or
        -not [bool]$Result.m_bMarkerProjectionEpochExact -or
        [int]$Result.m_iExpectedCheckpointSequence -ne
            $script:ExpectedCheckpointSequences[$Stage] -or
        [int]$Result.m_iObservedCheckpointSequence -ne
            $script:ExpectedCheckpointSequences[$Stage] -or
        [int]$Result.m_iExpectedRestoreSequence -ne
            $script:ExpectedRestoreSequences[$Stage] -or
        [int]$Result.m_iObservedRestoreSequence -ne
            $script:ExpectedRestoreSequences[$Stage] -or
        -not [bool]$Result.m_bDurableOrderExact) {
        throw "$label durable state observation is not exact."
    }
    if ($Stage -ceq "prepare_old_checkpoint") {
        if ([int]$Result.m_iLiveOldSentinelCount -ne 1 -or
            [string]$Result.m_sLiveOldSentinelFingerprint -cne
                [string]$Carrier.m_sOldSentinelFingerprint -or
            [int]$Result.m_iStaleJournalOldSentinelCount -ne 0 -or
            -not [string]::IsNullOrWhiteSpace(
                [string]$Result.m_sStaleJournalOldSentinelFingerprint) -or
            [bool]$Result.m_bOldSentinelRejected -or
            [int]$Result.m_iExpectedMarkerProjectionEpoch -ne
                [int]$Carrier.m_iOldMarkerProjectionEpoch -or
            [int]$Result.m_iObservedMarkerProjectionEpoch -ne
                [int]$Carrier.m_iOldMarkerProjectionEpoch) {
            throw "$label old-campaign sentinel evidence is not exact."
        }
    }
    else {
        if ([int]$Result.m_iLiveOldSentinelCount -ne 0 -or
            -not [string]::IsNullOrWhiteSpace(
                [string]$Result.m_sLiveOldSentinelFingerprint) -or
            [int]$Result.m_iStaleJournalOldSentinelCount -ne 1 -or
            [string]$Result.m_sStaleJournalOldSentinelFingerprint -cne
                [string]$Carrier.m_sOldSentinelFingerprint -or
            -not [bool]$Result.m_bOldSentinelRejected -or
            [int]$Result.m_iExpectedMarkerProjectionEpoch -ne
                [int]$Carrier.m_iResetMarkerProjectionEpoch -or
            [int]$Result.m_iObservedMarkerProjectionEpoch -ne
                [int]$Carrier.m_iResetMarkerProjectionEpoch) {
            throw "$label reset sentinel rejection evidence is not exact."
        }
    }

    $expectedPriorId = ""
    $expectedCreatedId = [string]$Carrier.m_sOldSavePointId
    $expectedActiveId = $expectedCreatedId
    if ($Stage -ceq "reset_commit") {
        $expectedPriorId = [string]$Carrier.m_sBlockerSavePointId
        $expectedCreatedId = [string]$Carrier.m_sResetSavePointId
        $expectedActiveId = $expectedCreatedId
    }
    elseif ($noSaveStage) {
        $expectedPriorId = [string]$Carrier.m_sBlockerSavePointId
        $expectedCreatedId = ""
        $expectedActiveId = $expectedPriorId
    }
    if ([string]$Result.m_sExpectedPriorSavePointId -cne
            $expectedPriorId -or
        [string]$Result.m_sObservedPriorSavePointId -cne
            $expectedPriorId -or
        [string]$Result.m_sCreatedSavePointId -cne $expectedCreatedId -or
        [string]$Result.m_sActiveSavePointId -cne $expectedActiveId -or
        [string]$Result.m_sExpectedSaveType -cne
            $script:ExpectedSaveTypes[$Stage] -or
        [string]$Result.m_sCreatedSaveType -cne
            $script:ExpectedSaveTypes[$Stage] -or
        [string]$Result.m_sExpectedSaveName -cne
            $script:ExpectedSaveNames[$Stage] -or
        [string]$Result.m_sCreatedSaveName -cne
            $script:ExpectedSaveNames[$Stage] -or
        -not [bool]$Result.m_bActiveSaveExact) {
        throw "$label native checkpoint identity is not exact."
    }
    if (-not $noSaveStage) {
        Assert-NativeSavePointId `
            -SavePointId ([string]$Result.m_sCreatedSavePointId) `
            -Label "$label created checkpoint"
    }

    $saveSuccessProperties = @(
        "m_bCampaignCaptured", "m_bTransientStateStaged",
        "m_bSavePointRequested", "m_bCompletionReceived",
        "m_bNativeCommitSucceeded", "m_bProfileMirrorSaved",
        "m_bCompletionObserverSucceeded", "m_bOnAfterSaveObserved",
        "m_bOnAfterSaveSucceeded", "m_bOnSaveCreatedObserved")
    foreach ($property in $saveSuccessProperties) {
        if ([bool]$Result.$property -eq $noSaveStage) {
            throw "$label save lifecycle property $property is not exact."
        }
    }
    $resetRejectionProperties = @(
        "m_bInFlightCheckpointObserved", "m_bInFlightResetRejected",
        "m_bRejectedResetReturnedNoCheckpoint",
        "m_bRejectedResetStateExact", "m_bRejectedResetSentinelExact",
        "m_bRejectedResetIdentityExact", "m_bRejectedResetEpochExact")
    $resetStage = $Stage -ceq "reset_commit"
    foreach ($property in $resetRejectionProperties) {
        if ([bool]$Result.$property -ne $resetStage) {
            throw "$label reset rejection property $property is not exact."
        }
    }
    if ($resetStage) {
        if ([string]$Result.m_sRejectedResetBeforeFingerprint -cne
                [string]$Carrier.m_sRejectedResetBeforeFingerprint -or
            [string]$Result.m_sRejectedResetAfterFingerprint -cne
                [string]$Carrier.m_sRejectedResetAfterFingerprint) {
            throw "$label rejected reset fingerprints are not exact."
        }
    }
    elseif (-not [string]::IsNullOrWhiteSpace(
            [string]$Result.m_sRejectedResetBeforeFingerprint) -or
        -not [string]::IsNullOrWhiteSpace(
            [string]$Result.m_sRejectedResetAfterFingerprint)) {
        throw "$label contains unexpected rejected reset fingerprints."
    }

    foreach ($property in @(
        "m_bNoSaveStage", "m_bSavingDisabledBeforeClose",
        "m_bNoCheckpointRequested", "m_bNoSaveEventsObserved",
        "m_bActiveSaveUnchanged")) {
        if ([bool]$Result.$property -ne $noSaveStage) {
            throw "$label no-save property $property is not exact."
        }
    }
    return $Result
}

function Invoke-AdminCampaignResetPersistenceStage {
    param(
        [Parameter(Mandatory = $true)][string]$ExecutablePath,
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string]$RuntimeAddonPath,
        [Parameter(Mandatory = $true)][string]$PackedAddonsParent,
        [Parameter(Mandatory = $true)][string]$ServerConfigPath,
        [Parameter(Mandatory = $true)][string]$ProfileRoot,
        [Parameter(Mandatory = $true)][string]$DebugDirectory,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory,
        [Parameter(Mandatory = $true)][string]$TempDirectory,
        [Parameter(Mandatory = $true)][string]$SessionNonce,
        [Parameter(Mandatory = $true)][string]$RunId,
        [Parameter(Mandatory = $true)][string]$PayloadNonce,
        [Parameter(Mandatory = $true)][string]$Stage,
        [Parameter(Mandatory = $true)]$ExpectedBuild,
        [Parameter(Mandatory = $true)][string]$ExpectedWorld,
        [AllowEmptyString()][string]$LoadSavePointId,
        [Parameter(Mandatory = $true)]$UnclaimedEngineProcessesObserved
    )

    if ($script:Stages -cnotcontains $Stage) {
        throw "Unknown admin reset persistence stage."
    }
    Assert-LowerHexNonce $SessionNonce "session nonce"
    $stageNonce = [Guid]::NewGuid().ToString("N")
    $resultPath = Join-Path $DebugDirectory (
        "{0}{1}.{2}.json" -f $script:ArtifactPrefix, $RunId, $Stage)
    $guardPath = Join-Path $DebugDirectory (
        "{0}{1}.guard.json" -f $script:ArtifactPrefix, $RunId)
    $carrierPath = Join-Path $DebugDirectory (
        "{0}{1}.carrier.json" -f $script:ArtifactPrefix, $RunId)
    if (Test-Path -LiteralPath $resultPath) {
        throw "$Stage result path was not fresh."
    }
    if (Test-Path -LiteralPath $guardPath) {
        throw "$Stage guard path was not fresh."
    }
    $firstStage = $Stage -ceq "prepare_old_checkpoint"
    if ($firstStage -and (Test-Path -LiteralPath $carrierPath)) {
        throw "$Stage carrier path was not fresh."
    }
    if (-not $firstStage -and
        -not (Test-Path -LiteralPath $carrierPath -PathType Leaf)) {
        throw "$Stage requires the prior exact carrier."
    }

    $allowCanonicalWrite = -not (
        $Stage -ceq "stale_native_no_save_verify")
    $guard = [ordered]@{
        m_sMagic = $script:GuardMagic
        m_iVersion = $script:AuthorityVersion
        m_sSessionNonce = $SessionNonce
        m_sStageNonce = $stageNonce
        m_sRunId = $RunId
        m_sPayloadNonce = $PayloadNonce
        m_sRequestedStage = $Stage
        m_iStageOrdinal = $script:StageOrdinals[$Stage]
        m_sBuildSha = $ExpectedBuild.BuildSha
        m_sBuildUtc = $ExpectedBuild.BuildUtc
        m_sBuildLabel = $ExpectedBuild.BuildLabel
        m_iCampaignSchemaVersion = $ExpectedBuild.CampaignSchemaVersion
        m_iSettingsSchemaVersion = $ExpectedBuild.SettingsSchemaVersion
        m_sWorld = $ExpectedWorld
        m_sExpectedSource = $script:ExpectedSources[$Stage]
        m_sExpectedLoadSavePointId = $LoadSavePointId
        m_iExpectedJournalGeneration =
            $script:ExpectedSourceJournalGenerations[$Stage]
        m_bAllowCanonicalCampaignOverwrite = $allowCanonicalWrite
        m_bNoSaveStage = -not $allowCanonicalWrite
    }
    Write-JsonUtf8NoBom -Path $guardPath -Value $guard
    [void](Assert-AdminProofGuard `
        -Guard (Read-JsonArtifact -Path $guardPath) `
        -SessionNonce $SessionNonce `
        -StageNonce $stageNonce `
        -RunId $RunId `
        -PayloadNonce $PayloadNonce `
        -Stage $Stage `
        -ExpectedLoadSavePointId $LoadSavePointId `
        -ExpectedBuild $ExpectedBuild `
        -ExpectedWorld $ExpectedWorld)

    $canonicalPath = Join-Path $ProfileRoot (
        "profile\Partisan\HST_CampaignSaveData.json")
    $recoveryPath = Join-Path $ProfileRoot (
        "profile\Partisan\HST_CampaignSaveData.recovery.json")
    $journalBefore = $null
    $carrierBeforeSignature = ""
    if (-not $allowCanonicalWrite) {
        $journalBefore = Get-CampaignJournalFileState `
            -CanonicalPath $canonicalPath `
            -RecoveryPath $recoveryPath
        if ([int]$journalBefore.FileCount -ne 2) {
            throw "$Stage requires both profile journal generations."
        }
        $carrierBeforeSignature = Get-FileSignature -Path $carrierPath
    }

    $arguments = Get-AdminProofStageArgumentVector `
        -RuntimeAddonPath $RuntimeAddonPath `
        -PackedAddonsParent $PackedAddonsParent `
        -ServerConfigPath $ServerConfigPath `
        -ProfileRoot $ProfileRoot `
        -SessionNonce $SessionNonce `
        -StageNonce $stageNonce `
        -RunId $RunId `
        -Stage $Stage `
        -LoadSavePointId $LoadSavePointId
    $assertCarrierCommand = ${function:Assert-AdminProofCarrier}
    $assertResultCommand = ${function:Assert-AdminProofResult}
    $readJsonArtifactCommand = ${function:Read-JsonArtifact}
    $validator = {
        param($candidate)
        $candidateCarrier = & $readJsonArtifactCommand -Path $carrierPath
        $candidateCarrier = & $assertCarrierCommand `
            -Carrier $candidateCarrier `
            -SessionNonce $SessionNonce `
            -RunId $RunId `
            -PayloadNonce $PayloadNonce `
            -Stage $Stage `
            -ExpectedBuild $ExpectedBuild `
            -ExpectedWorld $ExpectedWorld
        & $assertResultCommand `
            -Result $candidate `
            -Carrier $candidateCarrier `
            -SessionNonce $SessionNonce `
            -StageNonce $stageNonce `
            -RunId $RunId `
            -PayloadNonce $PayloadNonce `
            -Stage $Stage `
            -ExpectedBuild $ExpectedBuild `
            -ExpectedWorld $ExpectedWorld
    }.GetNewClosure()
    $processOutcome = Invoke-GuardedProcess `
        -Label "admin reset persistence/$Stage" `
        -ExecutablePath $ExecutablePath `
        -Arguments $arguments `
        -WorkingDirectory $WorkingDirectory `
        -TempDirectory $TempDirectory `
        -TimeoutSeconds $StageTimeoutSeconds `
        -ResultPath $resultPath `
        -ResultValidator $validator `
        -DiagnosticProjectDirectory $RepositoryRoot `
        -DiagnosticAddonRoots @($RuntimeAddonPath) `
        -UnclaimedEngineProcessesObserved $UnclaimedEngineProcessesObserved

    if (Test-Path -LiteralPath $guardPath) {
        throw "$Stage did not consume its one-use engine guard."
    }
    $journalAfter = Get-CampaignJournalFileState `
        -CanonicalPath $canonicalPath `
        -RecoveryPath $recoveryPath
    $journalUnchanged = $null
    $carrierUnchanged = $null
    if (-not $allowCanonicalWrite) {
        $journalUnchanged = Test-CampaignJournalFileStateExact `
            -Expected $journalBefore `
            -Actual $journalAfter
        if (-not $journalUnchanged) {
            throw "$Stage changed the byte-hashed campaign journal."
        }
        $carrierUnchanged = (Get-FileSignature -Path $carrierPath) -ceq
            $carrierBeforeSignature
        if (-not $carrierUnchanged) {
            throw "$Stage changed the completed reset carrier."
        }
    }
    $carrier = Read-JsonArtifact -Path $carrierPath
    $carrier = Assert-AdminProofCarrier `
        -Carrier $carrier `
        -SessionNonce $SessionNonce `
        -RunId $RunId `
        -PayloadNonce $PayloadNonce `
        -Stage $Stage `
        -ExpectedBuild $ExpectedBuild `
        -ExpectedWorld $ExpectedWorld
    $result = Assert-AdminProofResult `
        -Result $processOutcome.Result `
        -Carrier $carrier `
        -SessionNonce $SessionNonce `
        -StageNonce $stageNonce `
        -RunId $RunId `
        -PayloadNonce $PayloadNonce `
        -Stage $Stage `
        -ExpectedBuild $ExpectedBuild `
        -ExpectedWorld $ExpectedWorld

    return [pscustomobject]@{
        Result = $result
        Carrier = $carrier
        ExitCode = $processOutcome.ExitCode
        JournalBefore = $journalBefore
        JournalAfter = $journalAfter
        CampaignJournalUnchanged = $journalUnchanged
        CarrierUnchanged = $carrierUnchanged
        SafeSummary = [pscustomobject]@{
            Stage = $Stage
            Success = [bool]$result.m_bSuccess
            Exit = $processOutcome.ExitCode
            Build = ([string]$result.m_sBuildSha).Substring(0, 12)
            Schema = [int]$result.m_iCampaignSchemaVersion
            Settings = [int]$result.m_iSettingsSchemaVersion
            Source = [string]$result.m_sSource
            SourceExact = [bool]$result.m_bSourceExact
            DegradedNativeRecovery =
                [bool]$result.m_bDegradedNativeRecovery
            SourceJournalGeneration =
                [int]$result.m_iSourceJournalGeneration
            CommittedJournalGeneration =
                [int]$result.m_iCommittedJournalGeneration
            CheckpointSequence =
                [int]$result.m_iObservedCheckpointSequence
            RestoreSequence = [int]$result.m_iObservedRestoreSequence
            LiveOldSentinelCount =
                [int]$result.m_iLiveOldSentinelCount
            StaleOldSentinelCount =
                [int]$result.m_iStaleJournalOldSentinelCount
            ActiveSave = [string]$result.m_sActiveSavePointId
            CommittedSave = [string]$result.m_sCreatedSavePointId
            NoSaveStage = [bool]$result.m_bNoSaveStage
            JournalFileCount = [int]$journalAfter.FileCount
            JournalReadOnly = $journalUnchanged
            CarrierReadOnly = $carrierUnchanged
        }
    }
}

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$expectedProjectPath = [IO.Path]::GetFullPath(
    (Join-Path $repositoryRoot "addon.gproj"))
$workspacePackScratchPath = Join-Path `
    $repositoryRoot `
    $script:WorkspacePackScratchLeaf
$workspacePackScratchSentinelPath = Join-Path `
    $workspacePackScratchPath `
    $script:WorkspacePackScratchSentinelLeaf
$nonce = [Guid]::NewGuid().ToString("N")
$payloadNonce = [Guid]::NewGuid().ToString("N")
$runId = "adminreset_{0}_{1}" -f
    [DateTime]::UtcNow.ToString(
        "yyyyMMddHHmmss", [Globalization.CultureInfo]::InvariantCulture),
    $nonce.Substring(0, 20)
$guardBase = [IO.Path]::GetFullPath(
    (Join-Path ([IO.Path]::GetTempPath()) $script:GuardBaseLeaf))
$guardLeaf = $script:GuardLeafPrefix + $nonce
$guardRoot = [IO.Path]::GetFullPath((Join-Path $guardBase $guardLeaf))
$guardSentinelPath = Join-Path $guardRoot $script:GuardSentinelLeaf
$packProfileRoot = Join-Path $guardRoot "pack-profile"
$packedAddonsParent = Join-Path $guardRoot "packed-addons"
$packedAddonPath = Join-Path $packedAddonsParent "Partisan"
$profileRoot = Join-Path $guardRoot "proof-backend"
$partisanRoot = Join-Path $profileRoot "profile\Partisan"
$debugRoot = Join-Path $partisanRoot "debug"
$workingRoot = Join-Path $guardRoot "working"
$tempRoot = Join-Path $guardRoot "temp"
$serverConfigPath = Join-Path $guardRoot "admin-reset-server-config.json"
$ownerPath = Join-Path $debugRoot (
    "{0}{1}.owner.json" -f $script:ArtifactPrefix, $runId)
$carrierPath = Join-Path $debugRoot (
    "{0}{1}.carrier.json" -f $script:ArtifactPrefix, $runId)
$canonicalPath = Join-Path $partisanRoot "HST_CampaignSaveData.json"
$recoveryPath = Join-Path $partisanRoot (
    "HST_CampaignSaveData.recovery.json")
$settingsPath = Join-Path $partisanRoot "HST_Settings.json"

$mutex = $null
$mutexAcquired = $false
$guardOwnership = $null
$workspacePackScratchCreated = $false
$workspacePackScratchOwnership = $null
$runError = $null
$runSucceeded = $false
$cleanupErrors = New-Object Collections.Generic.List[string]
$watchSnapshots = New-Object Collections.Generic.List[object]
$spillSnapshots = New-Object Collections.Generic.List[object]
$stageOutcomes = New-Object Collections.Generic.List[object]
$unclaimedEngineProcessesObserved =
    New-Object Collections.Generic.HashSet[string]
$cleanupResult = $null
$executablePath = ""
$workbenchPath = ""
$runtimeAddonPath = ""
$projectFile = ""
$buildIdentity = $null
$wrapperStartUtc = [DateTime]::MinValue
$packSummary = $null
$proofSummary = $null

try {
    $executablePath = Resolve-ExistingPath $Executable Leaf
    if ((Split-Path -Leaf $executablePath) -cne
        "ArmaReforgerServerDiag.exe") {
        throw "Admin reset persistence proof requires the dedicated diagnostic server."
    }
    $workbenchPath = Resolve-ExistingPath $WorkbenchExecutable Leaf
    if ((Split-Path -Leaf $workbenchPath) -cne
        "ArmaReforgerWorkbenchSteamDiag.exe") {
        throw "Admin reset persistence proof requires diagnostic Steam Workbench."
    }
    $runtimeAddonPath = Resolve-ExistingPath $RuntimeAddonRoot Container
    foreach ($marker in @("core\data.pak", "data\data.pak")) {
        $runtimeMarkerPath = Join-Path $runtimeAddonPath $marker
        if (-not (Test-Path -LiteralPath $runtimeMarkerPath -PathType Leaf)) {
            throw "RuntimeAddonRoot is not an installed packed game add-on root."
        }
    }
    $projectFile = if ([string]::IsNullOrWhiteSpace($ProjectPath)) {
        Resolve-ExistingPath $expectedProjectPath Leaf
    }
    else {
        Resolve-ExistingPath $ProjectPath Leaf
    }
    if (-not $projectFile.Equals(
        $expectedProjectPath, [StringComparison]::OrdinalIgnoreCase)) {
        throw "The proof must pack this checkout's addon.gproj."
    }
    if ($WorldResource -cne "Worlds/HST_Everon/HST_Everon.ent") {
        throw "Admin reset persistence proof requires the canonical Everon world."
    }
    foreach ($resource in @($WorldResource, $script:MissionHeader)) {
        $resourcePath = Join-Path $repositoryRoot (
            $resource.Replace('/', [IO.Path]::DirectorySeparatorChar))
        [void](Resolve-ExistingPath $resourcePath Leaf)
    }
    $buildIdentity = Read-CheckoutBuildIdentity $repositoryRoot

    $normalizedWatchedRoots = @($WatchedRoots | Where-Object {
        -not [string]::IsNullOrWhiteSpace([string]$_)
    })
    $normalizedSpillRoots = @($SpillRoots | Where-Object {
        -not [string]::IsNullOrWhiteSpace([string]$_)
    })
    if (-not $PreflightOnly -and
        ($normalizedWatchedRoots.Count -eq 0 -or
            $normalizedSpillRoots.Count -eq 0)) {
        throw "A real proof requires explicit watched and spill roots."
    }
    $checkoutCovered = $false
    foreach ($root in $normalizedWatchedRoots) {
        $resolvedWatchRoot = Resolve-ExistingPath $root Container
        if (Test-ContainedPath `
            -Root $resolvedWatchRoot `
            -Candidate $repositoryRoot `
            -AllowEqual) {
            $checkoutCovered = $true
            break
        }
    }
    if (-not $PreflightOnly -and -not $checkoutCovered) {
        throw "A real proof requires watched-root coverage of this checkout."
    }
    if (-not $PreflightOnly) {
        $documentsRoot = [Environment]::GetFolderPath(
            [Environment+SpecialFolder]::MyDocuments)
        if ([string]::IsNullOrWhiteSpace($documentsRoot)) {
            throw "The external game-state monitoring boundary is unavailable."
        }
        $expectedSpillBoundary = Resolve-ExistingPath `
            -Path (Join-Path $documentsRoot "My Games") `
            -Kind Container
        $spillBoundaryCovered = $false
        foreach ($root in $normalizedSpillRoots) {
            $resolvedSpillRoot = Resolve-ExistingPath $root Container
            if (Test-ContainedPath `
                -Root $resolvedSpillRoot `
                -Candidate $expectedSpillBoundary `
                -AllowEqual) {
                $spillBoundaryCovered = $true
                break
            }
        }
        if (-not $spillBoundaryCovered) {
            throw "A real proof requires spill-root coverage of the external game-state boundary."
        }
    }
    if (-not $PreflightOnly -and
        (Test-Path -LiteralPath $workspacePackScratchPath)) {
        throw "Workbench packing requires a fresh workspace scratch boundary."
    }

    $mutex = New-Object Threading.Mutex($false, $script:MutexName)
    try {
        $mutexAcquired = $mutex.WaitOne(0)
    }
    catch [Threading.AbandonedMutexException] {
        $mutexAcquired = $true
    }
    if (-not $mutexAcquired) {
        throw "Another guarded persistence proof wrapper is active."
    }
    if (@(Get-EngineProcessRows).Count -ne 0) {
        throw "Refusing launch while an engine or Workbench process is active."
    }

    foreach ($root in $normalizedWatchedRoots) {
        $watchRoot = Resolve-ExistingPath -Path $root -Kind Container
        $watchExclusions = @(Get-CheckoutWatchExclusions `
            -RepositoryRoot $repositoryRoot `
            -WatchRoot $watchRoot)
        $watchSnapshots.Add((New-RootSnapshot `
            -Root $watchRoot `
            -ExcludedRoots $watchExclusions))
    }
    $spillExclusions = New-Object Collections.Generic.List[string]
    $spillExclusions.Add((Split-Path -Parent $projectFile))
    $spillExclusions.Add($guardBase)
    foreach ($snapshot in $watchSnapshots) {
        if (-not $spillExclusions.Contains($snapshot.Root)) {
            $spillExclusions.Add($snapshot.Root)
        }
    }
    foreach ($root in $normalizedSpillRoots) {
        $spillSnapshots.Add((New-RootSnapshot `
            -Root $root `
            -ExcludedRoots $spillExclusions.ToArray()))
    }

    Assert-NoReparsePathAncestry -Path $guardBase
    if (-not (Test-Path -LiteralPath $guardBase -PathType Container)) {
        [void](New-Item -ItemType Directory -Path $guardBase)
    }
    Assert-NoReparsePathAncestry -Path $guardBase
    if (Test-Path -LiteralPath $guardRoot) {
        throw "The nonce-owned guard root was not fresh."
    }
    [void](New-Item -ItemType Directory -Path $guardRoot)
    $wrapper = Get-Process -Id $PID -ErrorAction Stop
    $wrapperStartUtc = $wrapper.StartTime.ToUniversalTime()
    Write-JsonUtf8NoBom -Path $guardSentinelPath -Value ([ordered]@{
        version = $script:SentinelVersion
        purpose = $script:OwnerPurpose
        nonce = $nonce
        guardLeaf = $guardLeaf
        ownerPid = $PID
        ownerStartUtc = $wrapperStartUtc.ToString(
            "o", [Globalization.CultureInfo]::InvariantCulture)
    })
    $guardOwnership = Read-GuardOwnership $guardRoot $guardBase
    if (-not $guardOwnership -or
        $guardOwnership.Nonce -cne $nonce -or
        $guardOwnership.OwnerPid -ne $PID -or
        $guardOwnership.OwnerStartUtc.Ticks -ne $wrapperStartUtc.Ticks) {
        throw "Guard ownership validation failed."
    }

    if (-not $PreflightOnly) {
        if (Test-Path -LiteralPath $workspacePackScratchPath) {
            throw "Workbench packing requires a fresh workspace scratch boundary."
        }
        [void](New-Item `
            -ItemType Directory `
            -Path $workspacePackScratchPath)
        $workspacePackScratchCreated = $true
        Assert-NoReparsePathAncestry -Path $workspacePackScratchPath
        Write-JsonUtf8NoBom `
            -Path $workspacePackScratchSentinelPath `
            -Value ([ordered]@{
                version = $script:SentinelVersion
                purpose = "native_workbench_pack_scratch"
                nonce = $nonce
                ownerPid = $PID
                ownerStartUtc = $wrapperStartUtc.ToString(
                    "o", [Globalization.CultureInfo]::InvariantCulture)
            })
        $workspacePackScratchOwnership =
            Read-WorkspacePackScratchOwnership `
                -Directory $workspacePackScratchPath `
                -RepositoryRoot $repositoryRoot
        if (-not $workspacePackScratchOwnership -or
            $workspacePackScratchOwnership.Nonce -cne $nonce -or
            $workspacePackScratchOwnership.OwnerPid -ne $PID -or
            $workspacePackScratchOwnership.OwnerStartUtc.Ticks -ne
                $wrapperStartUtc.Ticks) {
            throw "Workspace pack scratch ownership validation failed."
        }
    }

    foreach ($directory in @(
        $packProfileRoot,
        $packedAddonsParent,
        $debugRoot,
        $workingRoot,
        $tempRoot)) {
        [void](New-Item -ItemType Directory -Path $directory -Force)
        Assert-NoReparsePathAncestry -Path $directory
    }
    if (Test-Path -LiteralPath $packedAddonPath) {
        throw "The packed add-on output path was not fresh."
    }

    $schedulerSettings = [ordered]@{
        schemaVersion = $buildIdentity.SettingsSchemaVersion
        persistence = [ordered]@{
            autosaveIntervalSeconds =
                $script:HSTAutosaveSchedulerIntervalSeconds
            majorChangeDebounceSeconds =
                $script:HSTAutosaveSchedulerDebounceSeconds
        }
    }
    Write-JsonUtf8NoBom -Path $settingsPath -Value $schedulerSettings
    $validatedSchedulerSettings = Read-JsonArtifact -Path $settingsPath
    if ([int]$validatedSchedulerSettings.schemaVersion -ne
            $buildIdentity.SettingsSchemaVersion -or
        [int]$validatedSchedulerSettings.persistence.autosaveIntervalSeconds -ne
            $script:HSTAutosaveSchedulerIntervalSeconds -or
        [int]$validatedSchedulerSettings.persistence.majorChangeDebounceSeconds -ne
            $script:HSTAutosaveSchedulerDebounceSeconds) {
        throw "The guarded autosave scheduler settings failed their exact gate."
    }

    $serverConfig = [ordered]@{
        game = [ordered]@{
            name = "Partisan admin campaign reset persistence proof"
            password = ""
            passwordAdmin = ""
            scenarioId = $script:ScenarioId
            maxPlayers = 1
            visible = $false
            gameProperties = [ordered]@{
                fastValidation = $true
                battlEye = $false
                persistence = [ordered]@{
                    autoSaveInterval = 60
                    saveRetention = 10
                    loadSessionSave = $true
                    keepSessionSave = $false
                }
                missionHeader = [ordered]@{
                    m_eSaveTypes = 15
                }
            }
            mods = @(
                [ordered]@{
                    modId = $script:ProjectId
                    name = "Partisan"
                    required = $true
                })
        }
    }
    Write-JsonUtf8NoBom -Path $serverConfigPath -Value $serverConfig
    $validatedConfig = Read-JsonArtifact -Path $serverConfigPath
    $validatedMods = @($validatedConfig.game.mods)
    foreach ($property in @(
        "autoSaveInterval", "saveRetention", "loadSessionSave",
        "keepSessionSave")) {
        Assert-JsonProperty `
            -Value $validatedConfig.game.gameProperties.persistence `
            -PropertyName $property `
            -ArtifactLabel "guarded server persistence config"
    }
    if ([string]$validatedConfig.game.scenarioId -cne $script:ScenarioId -or
        [int]$validatedConfig.game.gameProperties.missionHeader.m_eSaveTypes -ne
            15 -or
        [bool]$validatedConfig.game.visible -or
        [int]$validatedConfig.game.gameProperties.persistence.autoSaveInterval -ne 60 -or
        [int]$validatedConfig.game.gameProperties.persistence.saveRetention -ne 10 -or
        $validatedConfig.game.gameProperties.persistence.loadSessionSave -isnot [bool] -or
        -not [bool]$validatedConfig.game.gameProperties.persistence.loadSessionSave -or
        $validatedConfig.game.gameProperties.persistence.keepSessionSave -isnot [bool] -or
        [bool]$validatedConfig.game.gameProperties.persistence.keepSessionSave -or
        $validatedMods.Count -ne 1 -or
        [string]$validatedMods[0].modId -cne $script:ProjectId -or
        [string]$validatedMods[0].name -cne "Partisan" -or
        $validatedMods[0].required -isnot [bool] -or
        -not [bool]$validatedMods[0].required) {
        throw "The guarded server config failed its exact gate."
    }

    $owner = New-AdminProofOwnerValue `
        -SessionNonce $nonce `
        -RunId $runId `
        -PayloadNonce $payloadNonce `
        -BuildIdentity $buildIdentity `
        -World $WorldResource
    Write-JsonUtf8NoBom -Path $ownerPath -Value $owner
    [void](Assert-AdminProofOwner `
        -Owner (Read-JsonArtifact -Path $ownerPath) `
        -SessionNonce $nonce `
        -RunId $runId `
        -PayloadNonce $payloadNonce `
        -ExpectedBuild $buildIdentity `
        -ExpectedWorld $WorldResource)

    $packArguments = Get-PackArgumentVector `
        -ProjectFile $projectFile `
        -RuntimeAddonPath $runtimeAddonPath `
        -PackProfilePath $packProfileRoot `
        -PackedAddonPath $packedAddonPath
    $placeholderSaveIds = @(
        "11111111-1111-1111-1111-111111111111",
        "22222222-2222-2222-2222-222222222222")
    $preflightVectors = New-Object Collections.Generic.List[object]
    for ($index = 0; $index -lt $script:Stages.Count; $index++) {
        $stage = $script:Stages[$index]
        $loadId = if ($index -eq 1) {
            $placeholderSaveIds[0]
        }
        elseif ($index -eq 2) {
            $placeholderSaveIds[1]
        }
        else { "" }
        $stageNonce = [Guid]::NewGuid().ToString("N")
        $preflightVectors.Add([pscustomobject]@{
            Stage = $stage
            Arguments = @(Get-AdminProofStageArgumentVector `
                -RuntimeAddonPath $runtimeAddonPath `
                -PackedAddonsParent $packedAddonsParent `
                -ServerConfigPath $serverConfigPath `
                -ProfileRoot $profileRoot `
                -SessionNonce $nonce `
                -StageNonce $stageNonce `
                -RunId $runId `
                -Stage $stage `
                -LoadSavePointId $loadId)
        })
    }
    $keepSessionSaveOverrideCount = @($preflightVectors | Where-Object {
        $_.Arguments -icontains "-keepSessionSave"
    }).Count
    $autoShutdownCount = @($preflightVectors | Where-Object {
        $_.Arguments -icontains "-autoshutdown"
    }).Count
    $loadStages = @($preflightVectors | Where-Object {
        $_.Arguments -ccontains "-loadSessionSave"
    } | ForEach-Object { $_.Stage })
    if ($keepSessionSaveOverrideCount -ne 0 -or
        $autoShutdownCount -ne 0 -or
        $loadStages.Count -ne 2 -or
        [string]$loadStages[0] -cne "reset_commit" -or
        [string]$loadStages[1] -cne "stale_native_no_save_verify") {
        throw "Preflight stage vectors failed their native-load or retention gate."
    }

    if ($PreflightOnly) {
        $runSucceeded = $true
        Write-Output ("PREFLIGHT " + ([pscustomobject]@{
            Build = $buildIdentity.BuildSha.Substring(0, 12)
            Schema = $buildIdentity.CampaignSchemaVersion
            Settings = $buildIdentity.SettingsSchemaVersion
            World = $WorldResource
            Scenario = $script:ScenarioId
            PackArgumentTokenCount = $packArguments.Count
            StageCount = $preflightVectors.Count
            PackedLaunch = $true
            KeepSessionSave = $false
            KeepSessionSaveCLIAbsent =
                $keepSessionSaveOverrideCount -eq 0
            LoadSessionSaveConfig =
                [bool]$validatedConfig.game.gameProperties.persistence.loadSessionSave
            StageLoadCount = $loadStages.Count
            NoAutoShutdown = $autoShutdownCount -eq 0
            BackendLocalStorage = $true
        } | ConvertTo-Json -Compress))
    }
    else {
        $packOutcome = Invoke-GuardedProcess `
            -Label "admin reset persistence Workbench pack" `
            -ExecutablePath $workbenchPath `
            -Arguments $packArguments `
            -WorkingDirectory $workingRoot `
            -TempDirectory $tempRoot `
            -TimeoutSeconds $PackTimeoutSeconds `
            -DiagnosticProjectDirectory $repositoryRoot `
            -DiagnosticAddonRoots @($runtimeAddonPath) `
            -UnclaimedEngineProcessesObserved $unclaimedEngineProcessesObserved
        $workspacePackScratchOwnership =
            Read-WorkspacePackScratchOwnership `
                -Directory $workspacePackScratchPath `
                -RepositoryRoot $repositoryRoot
        if (-not $workspacePackScratchOwnership -or
            $workspacePackScratchOwnership.Nonce -cne $nonce -or
            $workspacePackScratchOwnership.OwnerPid -ne $PID -or
            $workspacePackScratchOwnership.OwnerStartUtc.Ticks -ne
                $wrapperStartUtc.Ticks) {
            throw "Workbench packing changed its owned workspace scratch boundary."
        }
        $packedLayout = Assert-PackedAddonLayout `
            -GuardRoot $guardRoot `
            -PackedAddonsParent $packedAddonsParent
        $packSummary = [pscustomobject]@{
            Exit = $packOutcome.ExitCode
            EngineAfter = $packOutcome.EngineAfter
            OwnedProcessesRemaining = $packOutcome.OwnedProcessesRemaining
            FileCount = $packedLayout.FileCount
            PakCount = $packedLayout.PakCount
            ProjectCount = $packedLayout.ProjectCount
            ResourceDatabaseCount = $packedLayout.ResourceDatabaseCount
        }
        Write-Output ("PACK " + ($packSummary | ConvertTo-Json -Compress))

        $prepare = Invoke-AdminCampaignResetPersistenceStage `
            -ExecutablePath $executablePath `
            -RepositoryRoot $repositoryRoot `
            -RuntimeAddonPath $runtimeAddonPath `
            -PackedAddonsParent $packedAddonsParent `
            -ServerConfigPath $serverConfigPath `
            -ProfileRoot $profileRoot `
            -DebugDirectory $debugRoot `
            -WorkingDirectory $workingRoot `
            -TempDirectory $tempRoot `
            -SessionNonce $nonce `
            -RunId $runId `
            -PayloadNonce $payloadNonce `
            -Stage "prepare_old_checkpoint" `
            -ExpectedBuild $buildIdentity `
            -ExpectedWorld $WorldResource `
            -LoadSavePointId "" `
            -UnclaimedEngineProcessesObserved $unclaimedEngineProcessesObserved
        $stageOutcomes.Add($prepare.SafeSummary)
        Write-Output ("STAGE " +
            ($prepare.SafeSummary | ConvertTo-Json -Compress))
        $u0 = [string]$prepare.Carrier.m_sOldSavePointId
        $oldFingerprint =
            [string]$prepare.Carrier.m_sOldSnapshotFingerprint
        Assert-NativeSavePointId -SavePointId $u0 -Label "old checkpoint"
        if ([int]$prepare.JournalAfter.FileCount -ne 1 -or
            -not [bool]$prepare.JournalAfter.CanonicalPresent -or
            [bool]$prepare.JournalAfter.RecoveryPresent) {
            throw "Old checkpoint did not create one canonical journal generation."
        }
        $oldCanonicalSignature =
            [string]$prepare.JournalAfter.CanonicalSignature
        $oldEnvelope = Read-AdminProofJournalEnvelope `
            -Path $canonicalPath `
            -ExpectedGeneration 1 `
            -ExpectedSchema $buildIdentity.CampaignSchemaVersion `
            -ExpectedFingerprint $oldFingerprint `
            -ExpectedPreviousGeneration 0 `
            -ExpectedPreviousFingerprint ""

        $reset = Invoke-AdminCampaignResetPersistenceStage `
            -ExecutablePath $executablePath `
            -RepositoryRoot $repositoryRoot `
            -RuntimeAddonPath $runtimeAddonPath `
            -PackedAddonsParent $packedAddonsParent `
            -ServerConfigPath $serverConfigPath `
            -ProfileRoot $profileRoot `
            -DebugDirectory $debugRoot `
            -WorkingDirectory $workingRoot `
            -TempDirectory $tempRoot `
            -SessionNonce $nonce `
            -RunId $runId `
            -PayloadNonce $payloadNonce `
            -Stage "reset_commit" `
            -ExpectedBuild $buildIdentity `
            -ExpectedWorld $WorldResource `
            -LoadSavePointId $u0 `
            -UnclaimedEngineProcessesObserved $unclaimedEngineProcessesObserved
        $stageOutcomes.Add($reset.SafeSummary)
        Write-Output ("STAGE " +
            ($reset.SafeSummary | ConvertTo-Json -Compress))
        $u1 = [string]$reset.Carrier.m_sBlockerSavePointId
        $u2 = [string]$reset.Carrier.m_sResetSavePointId
        $blockerFingerprint =
            [string]$reset.Carrier.m_sBlockerSnapshotFingerprint
        $resetFingerprint =
            [string]$reset.Carrier.m_sResetSnapshotFingerprint
        Assert-NativeSavePointId `
            -SavePointId $u1 `
            -Label "stale native blocker checkpoint"
        Assert-NativeSavePointId `
            -SavePointId $u2 `
            -Label "reset checkpoint"
        if ($u0 -ceq $u1 -or $u0 -ceq $u2 -or $u1 -ceq $u2) {
            throw "The three native checkpoint UUIDs are not pairwise distinct."
        }
        if ([int]$reset.JournalAfter.FileCount -ne 2 -or
            -not [bool]$reset.JournalAfter.CanonicalPresent -or
            -not [bool]$reset.JournalAfter.RecoveryPresent -or
            [string]$reset.JournalAfter.CanonicalSignature -ceq
                $oldCanonicalSignature) {
            throw "Reset did not replace canonical generation 1 and retain recovery."
        }
        $recoveryEnvelope = Read-AdminProofJournalEnvelope `
            -Path $recoveryPath `
            -ExpectedGeneration 2 `
            -ExpectedSchema $buildIdentity.CampaignSchemaVersion `
            -ExpectedFingerprint $blockerFingerprint `
            -ExpectedPreviousGeneration 1 `
            -ExpectedPreviousFingerprint $oldFingerprint
        $resetEnvelope = Read-AdminProofJournalEnvelope `
            -Path $canonicalPath `
            -ExpectedGeneration 3 `
            -ExpectedSchema $buildIdentity.CampaignSchemaVersion `
            -ExpectedFingerprint $resetFingerprint `
            -ExpectedPreviousGeneration 2 `
            -ExpectedPreviousFingerprint $blockerFingerprint
        $journalBeforeStaleVerify = Get-CampaignJournalFileState `
            -CanonicalPath $canonicalPath `
            -RecoveryPath $recoveryPath

        $staleVerify = Invoke-AdminCampaignResetPersistenceStage `
            -ExecutablePath $executablePath `
            -RepositoryRoot $repositoryRoot `
            -RuntimeAddonPath $runtimeAddonPath `
            -PackedAddonsParent $packedAddonsParent `
            -ServerConfigPath $serverConfigPath `
            -ProfileRoot $profileRoot `
            -DebugDirectory $debugRoot `
            -WorkingDirectory $workingRoot `
            -TempDirectory $tempRoot `
            -SessionNonce $nonce `
            -RunId $runId `
            -PayloadNonce $payloadNonce `
            -Stage "stale_native_no_save_verify" `
            -ExpectedBuild $buildIdentity `
            -ExpectedWorld $WorldResource `
            -LoadSavePointId $u1 `
            -UnclaimedEngineProcessesObserved $unclaimedEngineProcessesObserved
        $stageOutcomes.Add($staleVerify.SafeSummary)
        Write-Output ("STAGE " +
            ($staleVerify.SafeSummary | ConvertTo-Json -Compress))
        $journalAfterStaleVerify = Get-CampaignJournalFileState `
            -CanonicalPath $canonicalPath `
            -RecoveryPath $recoveryPath
        if (-not (Test-CampaignJournalFileStateExact `
            -Expected $journalBeforeStaleVerify `
            -Actual $journalAfterStaleVerify)) {
            throw "Final verification changed one or both byte-hashed journal files."
        }
        [void](Read-AdminProofJournalEnvelope `
            -Path $recoveryPath `
            -ExpectedGeneration 2 `
            -ExpectedSchema $buildIdentity.CampaignSchemaVersion `
            -ExpectedFingerprint $blockerFingerprint `
            -ExpectedPreviousGeneration 1 `
            -ExpectedPreviousFingerprint $oldFingerprint)
        [void](Read-AdminProofJournalEnvelope `
            -Path $canonicalPath `
            -ExpectedGeneration 3 `
            -ExpectedSchema $buildIdentity.CampaignSchemaVersion `
            -ExpectedFingerprint $resetFingerprint `
            -ExpectedPreviousGeneration 2 `
            -ExpectedPreviousFingerprint $blockerFingerprint)

        if ($stageOutcomes.Count -ne 3 -or
            $unclaimedEngineProcessesObserved.Count -ne 0 -or
            -not [bool]$staleVerify.CampaignJournalUnchanged -or
            -not [bool]$staleVerify.CarrierUnchanged) {
            throw "The three-process proof did not complete its exact read-only stage set."
        }
        $proofSummary = [pscustomobject]@{
            Success = $true
            Build = $buildIdentity.BuildSha.Substring(0, 12)
            Schema = $buildIdentity.CampaignSchemaVersion
            Settings = $buildIdentity.SettingsSchemaVersion
            World = $WorldResource
            StageCount = $stageOutcomes.Count
            OldUuid = $u0
            BlockerUuid = $u1
            ResetUuid = $u2
            OldOrder = "cp2/r0"
            BlockerOrder = "cp4/r1"
            ResetOrder = "cp5/r1"
            VerifiedLiveOrder = "cp6/r2"
            InFlightResetRejected =
                [bool]$reset.Result.m_bInFlightResetRejected
            RejectedResetStateExact =
                [bool]$reset.Result.m_bRejectedResetStateExact
            ResetRemovedOldSentinel =
                [bool]$reset.Carrier.m_bResetRemovedSentinel
            ResetPreservedIdentity =
                [bool]$reset.Carrier.m_bResetPreservedIdentity
            RecoveryGeneration =
                [int]$recoveryEnvelope.m_iGeneration
            CanonicalGeneration = [int]$resetEnvelope.m_iGeneration
            FinalSource = [string]$staleVerify.Result.m_sSource
            DegradedNewerJournalRecovery =
                [bool]$staleVerify.Result.m_bDegradedNativeRecovery
            FinalNoSave = [bool]$staleVerify.Result.m_bNoSaveStage
            FinalJournalReadOnly =
                [bool]$staleVerify.CampaignJournalUnchanged
            FinalCarrierReadOnly = [bool]$staleVerify.CarrierUnchanged
            CanonicalByteHashDigest = Get-FileNameSafeDigest (
                [string]$journalAfterStaleVerify.CanonicalSignature)
            RecoveryByteHashDigest = Get-FileNameSafeDigest (
                [string]$journalAfterStaleVerify.RecoverySignature)
        }
        $runSucceeded = $true
    }
}
catch {
    $runError = $_.Exception.Message
}
finally {
    try {
        if ($workspacePackScratchCreated) {
            if (@(Get-EngineProcessRows).Count -ne 0) {
                throw "Owned workspace scratch cannot be removed while an engine process is active."
            }
            $candidateScratchOwnership =
                Read-WorkspacePackScratchOwnership `
                    -Directory $workspacePackScratchPath `
                    -RepositoryRoot $repositoryRoot
            if ($candidateScratchOwnership -and
                $candidateScratchOwnership.Nonce -ceq $nonce -and
                $candidateScratchOwnership.OwnerPid -eq $PID -and
                $wrapperStartUtc -ne [DateTime]::MinValue -and
                $candidateScratchOwnership.OwnerStartUtc.Ticks -eq
                    $wrapperStartUtc.Ticks) {
                $workspacePackScratchOwnership = $candidateScratchOwnership
            }
            else {
                $workspacePackScratchOwnership = $null
            }
            if (-not $workspacePackScratchOwnership -or
                -not (Remove-WorkspacePackScratch `
                    -Ownership $workspacePackScratchOwnership `
                    -RepositoryRoot $repositoryRoot)) {
                throw "Owned Workbench workspace scratch removal failed."
            }
        }
    }
    catch {
        [void]$cleanupErrors.Add("remove-owned-workspace-pack-scratch")
    }
    try {
        if ($guardOwnership -and
            -not (Remove-ExactOwnedGuard $guardOwnership $guardBase)) {
            throw "The nonce-owned guard could not be removed."
        }
    }
    catch { [void]$cleanupErrors.Add("remove-owned-guard") }
    try {
        if (Test-Path -LiteralPath $guardBase -PathType Container) {
            Assert-NoReparsePathAncestry -Path $guardBase
            if (@(Get-ChildItem -LiteralPath $guardBase -Force).Count -eq 0) {
                Remove-Item -LiteralPath $guardBase -Force -ErrorAction Stop
            }
        }
    }
    catch { [void]$cleanupErrors.Add("remove-empty-guard-base") }

    $watchNew = 0
    $watchModified = 0
    $watchDeleted = 0
    $watchMissing = 0
    $spillNew = 0
    $spillModified = 0
    $spillDeleted = 0
    $spillMissing = 0
    try {
        foreach ($snapshot in $watchSnapshots) {
            $delta = Get-RootSnapshotDelta $snapshot
            $watchNew += $delta.NewEntries
            $watchModified += $delta.ModifiedFiles
            $watchDeleted += $delta.DeletedEntries
            $watchMissing += $delta.MissingRoot
        }
    }
    catch { [void]$cleanupErrors.Add("audit-watched-roots") }
    try {
        foreach ($snapshot in $spillSnapshots) {
            $delta = Get-RootSnapshotDelta $snapshot
            $spillNew += $delta.NewEntries
            $spillModified += $delta.ModifiedFiles
            $spillDeleted += $delta.DeletedEntries
            $spillMissing += $delta.MissingRoot
        }
    }
    catch { [void]$cleanupErrors.Add("audit-spill-roots") }

    $workspacePackScratchRemaining = -1
    $guardRemaining = -1
    $guardBaseRemaining = -1
    $engineProcessesRemaining = -1
    $unclaimedEngineProcessCount = -1
    try {
        $workspacePackScratchRemaining = [int](
            $workspacePackScratchCreated -and
            (Test-Path -LiteralPath $workspacePackScratchPath))
        $guardRemaining = [int](Test-Path -LiteralPath $guardRoot)
        $guardBaseRemaining = [int](Test-Path -LiteralPath $guardBase)
        $engineProcessesRemaining = @(Get-EngineProcessRows).Count
        $unclaimedEngineProcessCount =
            $unclaimedEngineProcessesObserved.Count
    }
    catch { [void]$cleanupErrors.Add("audit-final-boundaries") }
    try {
        if ($mutexAcquired -and $mutex) {
            $mutex.ReleaseMutex()
            $mutexAcquired = $false
        }
        if ($mutex) {
            $mutex.Dispose()
            $mutex = $null
        }
    }
    catch { [void]$cleanupErrors.Add("release-mutex") }

    $cleanupResult = [pscustomobject]@{
        WorkspacePackScratchRemaining = $workspacePackScratchRemaining
        GuardRemaining = $guardRemaining
        GuardBaseRemaining = $guardBaseRemaining
        EngineProcessesRemaining = $engineProcessesRemaining
        UnclaimedEngineProcessesObserved = $unclaimedEngineProcessCount
        NewWatchedEntries = $watchNew
        ModifiedWatchedFiles = $watchModified
        DeletedWatchedEntries = $watchDeleted
        MissingWatchedRoots = $watchMissing
        NewSpillEntries = $spillNew
        ModifiedSpillFiles = $spillModified
        DeletedSpillEntries = $spillDeleted
        MissingSpillRoots = $spillMissing
        CleanupPhaseErrorCount = $cleanupErrors.Count
        CleanupPhaseErrors = $cleanupErrors.ToArray()
    }
    Write-Output ("CLEANUP " + ($cleanupResult | ConvertTo-Json -Compress))
}

$cleanupPassed = $cleanupResult -and
    $cleanupResult.WorkspacePackScratchRemaining -eq 0 -and
    $cleanupResult.GuardRemaining -eq 0 -and
    $cleanupResult.GuardBaseRemaining -eq 0 -and
    $cleanupResult.EngineProcessesRemaining -eq 0 -and
    $cleanupResult.UnclaimedEngineProcessesObserved -eq 0 -and
    $cleanupResult.NewWatchedEntries -eq 0 -and
    $cleanupResult.ModifiedWatchedFiles -eq 0 -and
    $cleanupResult.DeletedWatchedEntries -eq 0 -and
    $cleanupResult.MissingWatchedRoots -eq 0 -and
    $cleanupResult.NewSpillEntries -eq 0 -and
    $cleanupResult.ModifiedSpillFiles -eq 0 -and
    $cleanupResult.DeletedSpillEntries -eq 0 -and
    $cleanupResult.MissingSpillRoots -eq 0 -and
    $cleanupResult.CleanupPhaseErrorCount -eq 0

if (-not $cleanupPassed) {
    [Console]::Error.WriteLine(
        "Admin reset persistence proof cleanup did not return every boundary to zero.")
    exit 2
}
if (-not $runSucceeded) {
    if ([string]::IsNullOrWhiteSpace($runError)) {
        $runError = "Admin reset persistence proof did not complete."
    }
    $projectDirectory = ""
    if (-not [string]::IsNullOrWhiteSpace($projectFile)) {
        $projectDirectory = Split-Path -Parent $projectFile
    }
    $safeRunError = ConvertTo-SafeEvidenceLine `
        -Line $runError `
        -GuardRoot $guardRoot `
        -ProjectDirectory $projectDirectory `
        -ResolvedAddonRoots @($runtimeAddonPath)
    if ([string]::IsNullOrWhiteSpace($safeRunError)) {
        $safeRunError =
            "Admin reset persistence proof failed without safe evidence."
    }
    [Console]::Error.WriteLine($safeRunError)
    exit 1
}
if (-not $PreflightOnly) {
    if (-not $proofSummary) {
        [Console]::Error.WriteLine(
            "Admin reset persistence proof completed without a terminal summary.")
        exit 1
    }
    Write-Output ("PROOF " + ($proofSummary | ConvertTo-Json -Compress))
}
exit 0
