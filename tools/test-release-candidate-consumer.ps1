[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ManifestPath,
    [Parameter(Mandatory = $true)][string]$BundleRoot,
    [Parameter(Mandatory = $true)][string]$RuntimeAddonRoot,
    [Parameter(Mandatory = $true)][string]$Executable,
    [Parameter(Mandatory = $true)][ValidateSet('client', 'server')][string]$RuntimeRole
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-Rejected {
    param(
        [Parameter(Mandatory = $true)][string]$Label,
        [Parameter(Mandatory = $true)][scriptblock]$Action
    )

    $rejected = $false
    try {
        & $Action
    }
    catch {
        $rejected = $true
    }
    if (-not $rejected) {
        throw "Candidate-consumer negative self-test was accepted: $Label"
    }
}

function Flip-FirstByte {
    param([Parameter(Mandatory = $true)][string]$Path)

    $stream = [IO.File]::Open(
        $Path,
        [IO.FileMode]::Open,
        [IO.FileAccess]::ReadWrite,
        [IO.FileShare]::None)
    try {
        $value = $stream.ReadByte()
        if ($value -lt 0) {
            throw 'Cannot mutate an empty self-test fixture.'
        }
        $stream.Position = 0
        $stream.WriteByte($value -bxor 1)
        $stream.Flush($true)
    }
    finally {
        $stream.Dispose()
    }
}

$modulePath = Join-Path $PSScriptRoot 'Partisan.ReleaseCandidate.psm1'
Import-Module -Name $modulePath -Force -ErrorAction Stop
$manifestFull = [IO.Path]::GetFullPath($ManifestPath)
$bundleFull = [IO.Path]::GetFullPath($BundleRoot)
$runtimeAttemptError = $null
try {
    [void](Assert-PartisanReleaseCandidate `
        -ManifestPath $manifestFull `
        -BundleRoot $bundleFull `
        -RuntimeAddonRoot $RuntimeAddonRoot `
        -Executable $Executable `
        -RuntimeRole $RuntimeRole)
}
catch {
    $runtimeAttemptError = $_.Exception.Message
}
$valid = Assert-PartisanReleaseCandidate `
    -ManifestPath $manifestFull `
    -BundleRoot $bundleFull `
    -RuntimeAddonRoot $RuntimeAddonRoot `
    -Executable $Executable `
    -RuntimeRole $RuntimeRole `
    -ConsumerIntent verification

if ($valid.RuntimeUseDisposition -ceq 'supersede-before-runtime') {
    if ($runtimeAttemptError -cne
        'The current release candidate is not eligible for runtime use.') {
        throw 'A superseded candidate was not rejected at the runtime-use boundary.'
    }
}
elseif ($valid.RuntimeUseDisposition -ceq 'active-runtime-candidate') {
    if ($null -ne $runtimeAttemptError) {
        throw 'An active runtime candidate was rejected at the runtime-use boundary.'
    }
}
else {
    throw 'The candidate consumer returned an unknown runtime-use disposition.'
}

$tempBase = [IO.Path]::GetFullPath(
    (Join-Path ([IO.Path]::GetTempPath()) 'PartisanCandidateConsumerSelfTest'))
$nonce = [Guid]::NewGuid().ToString('N')
$guardRoot = [IO.Path]::GetFullPath((Join-Path $tempBase $nonce))
$expectedPrefix = $tempBase.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
if (-not $guardRoot.StartsWith(
        $expectedPrefix,
        [StringComparison]::OrdinalIgnoreCase) -or
    (Test-Path -LiteralPath $guardRoot)) {
    throw 'Candidate-consumer self-test guard containment failed.'
}

$checks = New-Object Collections.Generic.List[string]
[void]$checks.Add('runtime-use-disposition')
try {
    New-Item -ItemType Directory -Path $guardRoot -Force | Out-Null
    $sentinel = Join-Path $guardRoot '.partisan-candidate-consumer-owner'
    [IO.File]::WriteAllText($sentinel, $nonce)
    $fixtureBundle = Join-Path $guardRoot $valid.CandidateId
    New-Item -ItemType Directory -Path $fixtureBundle | Out-Null
    foreach ($item in @(Get-ChildItem -LiteralPath $bundleFull -Force)) {
        Copy-Item `
            -LiteralPath $item.FullName `
            -Destination $fixtureBundle `
            -Recurse `
            -Force `
            -ErrorAction Stop
    }

    $fixtureArguments = @{
        ManifestPath = $manifestFull
        BundleRoot = $fixtureBundle
        RuntimeAddonRoot = $RuntimeAddonRoot
        Executable = $Executable
        RuntimeRole = $RuntimeRole
        ConsumerIntent = 'verification'
    }
    [void](Assert-PartisanReleaseCandidate @fixtureArguments)
    [void]$checks.Add('copied-bundle-valid')

    $fixturePackage = Join-Path $fixtureBundle 'package\Partisan\data.pak'
    Flip-FirstByte -Path $fixturePackage
    Assert-Rejected -Label 'package-byte-tamper' -Action {
        [void](Assert-PartisanReleaseCandidate @fixtureArguments)
    }
    Copy-Item `
        -LiteralPath (Join-Path $valid.PackedAddonPath 'data.pak') `
        -Destination $fixturePackage `
        -Force
    [void]$checks.Add('package-byte-tamper')

    $fixtureManifest = Join-Path $fixtureBundle 'candidate.json'
    [IO.File]::AppendAllText($fixtureManifest, ' ')
    Assert-Rejected -Label 'external-manifest-tamper' -Action {
        [void](Assert-PartisanReleaseCandidate @fixtureArguments)
    }
    Copy-Item -LiteralPath $valid.TrackedManifestPath -Destination $fixtureManifest -Force
    [void]$checks.Add('external-manifest-tamper')

    $fixtureReady = Join-Path $fixtureBundle 'candidate.ready.json'
    [IO.File]::AppendAllText($fixtureReady, ' ')
    Assert-Rejected -Label 'external-ready-tamper' -Action {
        [void](Assert-PartisanReleaseCandidate @fixtureArguments)
    }
    Copy-Item -LiteralPath $valid.TrackedReadyPath -Destination $fixtureReady -Force
    [void]$checks.Add('external-ready-tamper')

    $extraTop = Join-Path $fixtureBundle 'unexpected.txt'
    [IO.File]::WriteAllText($extraTop, 'unexpected')
    Assert-Rejected -Label 'extra-top-level-file' -Action {
        [void](Assert-PartisanReleaseCandidate @fixtureArguments)
    }
    Remove-Item -LiteralPath $extraTop -Force
    [void]$checks.Add('extra-top-level-file')

    $extraPackage = Join-Path $fixtureBundle 'package\Partisan\unexpected.bin'
    [IO.File]::WriteAllBytes($extraPackage, [byte[]](1, 2, 3))
    Assert-Rejected -Label 'extra-package-file' -Action {
        [void](Assert-PartisanReleaseCandidate @fixtureArguments)
    }
    Remove-Item -LiteralPath $extraPackage -Force
    [void]$checks.Add('extra-package-file')

    $extraPackageDirectory = Join-Path $fixtureBundle 'package\unexpected-empty'
    New-Item -ItemType Directory -Path $extraPackageDirectory | Out-Null
    Assert-Rejected -Label 'extra-package-directory' -Action {
        [void](Assert-PartisanReleaseCandidate @fixtureArguments)
    }
    Remove-Item -LiteralPath $extraPackageDirectory -Force
    [void]$checks.Add('extra-package-directory')

    $extraEvidence = Join-Path $fixtureBundle 'evidence\unexpected.txt'
    [IO.File]::WriteAllText($extraEvidence, 'unexpected')
    Assert-Rejected -Label 'extra-evidence-file' -Action {
        [void](Assert-PartisanReleaseCandidate @fixtureArguments)
    }
    Remove-Item -LiteralPath $extraEvidence -Force
    [void]$checks.Add('extra-evidence-file')

    $stageGuard = Join-Path $guardRoot 'stage-guard'
    New-Item -ItemType Directory -Path $stageGuard | Out-Null
    $stage = New-PartisanReleaseCandidateStage `
        -Candidate $valid `
        -GuardRoot $stageGuard
    [void](Assert-PartisanReleaseCandidateStage `
        -Candidate $valid `
        -StageRoot $stage.StageRootPath)
    $stageSibling = Join-Path $stage.StageRootPath 'unexpected.txt'
    [IO.File]::WriteAllText($stageSibling, 'unexpected')
    Assert-Rejected -Label 'staged-root-sibling' -Action {
        [void](Assert-PartisanReleaseCandidateStage `
            -Candidate $valid `
            -StageRoot $stage.StageRootPath)
    }
    Remove-Item -LiteralPath $stageSibling -Force
    [void]$checks.Add('staged-root-sibling')

    Flip-FirstByte -Path (Join-Path $stage.PackedAddonPath 'data.pak')
    Assert-Rejected -Label 'staged-package-tamper' -Action {
        [void](Assert-PartisanReleaseCandidateStage `
            -Candidate $valid `
            -StageRoot $stage.StageRootPath)
    }
    [void]$checks.Add('staged-package-tamper')

    $commaRoot = Join-Path $guardRoot 'runtime,addons'
    New-Item `
        -ItemType Directory `
        -Path (Join-Path $commaRoot 'core'), (Join-Path $commaRoot 'data') `
        -Force | Out-Null
    [IO.File]::WriteAllBytes((Join-Path $commaRoot 'core\data.pak'), [byte[]](1))
    [IO.File]::WriteAllBytes((Join-Path $commaRoot 'data\data.pak'), [byte[]](1))
    $commaArguments = $fixtureArguments.Clone()
    $commaArguments.RuntimeAddonRoot = $commaRoot
    Assert-Rejected -Label 'comma-runtime-root' -Action {
        [void](Assert-PartisanReleaseCandidate @commaArguments)
    }
    [void]$checks.Add('comma-runtime-root')

    [void](Assert-PartisanReleaseCandidate @fixtureArguments)
    [void]$checks.Add('fixture-restored-valid')
}
finally {
    if (Test-Path -LiteralPath $guardRoot -PathType Container) {
        $sentinel = Join-Path $guardRoot '.partisan-candidate-consumer-owner'
        $sentinelValid = (Test-Path -LiteralPath $sentinel -PathType Leaf) -and
            ([IO.File]::ReadAllText($sentinel) -ceq $nonce)
        $guardFull = [IO.Path]::GetFullPath($guardRoot)
        if (-not $sentinelValid -or
            -not $guardFull.StartsWith(
                $expectedPrefix,
                [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Candidate-consumer self-test cleanup ownership failed.'
        }
        Remove-Item -LiteralPath $guardFull -Recurse -Force -ErrorAction Stop
    }
    if ((Test-Path -LiteralPath $tempBase -PathType Container) -and
        @(Get-ChildItem -LiteralPath $tempBase -Force).Count -eq 0) {
        Remove-Item -LiteralPath $tempBase -Force -ErrorAction Stop
    }
}

Write-Output ('SELFTEST ' + ([pscustomobject]@{
    Success = $true
    CandidateId = $valid.CandidateId
    CheckCount = $checks.Count
    Checks = $checks.ToArray()
} | ConvertTo-Json -Compress))
