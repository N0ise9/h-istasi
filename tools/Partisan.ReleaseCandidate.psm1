Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:ManifestSchemaVersion = 1
$script:PackageHashAlgorithm = 'sha256-manifest-v1'
$script:RequiredPackageFiles = @(
    'Partisan/addon.gproj',
    'Partisan/data.pak',
    'Partisan/resourceDatabase.rdb',
    'Partisan/thumbnail.png'
)

function Resolve-PartisanExistingPath {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][ValidateSet('Leaf', 'Container')][string]$Kind
    )

    $resolved = [IO.Path]::GetFullPath($Path)
    if (-not (Test-Path -LiteralPath $resolved -PathType $Kind)) {
        throw "A required release-candidate $Kind path does not exist."
    }
    return $resolved
}

function Test-PartisanContainedPath {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Candidate,
        [switch]$AllowEqual
    )

    $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    $candidateFull = [IO.Path]::GetFullPath($Candidate).TrimEnd('\', '/')
    if ($AllowEqual -and $candidateFull.Equals(
            $rootFull,
            [StringComparison]::OrdinalIgnoreCase)) {
        return $true
    }
    return $candidateFull.StartsWith(
        $rootFull + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)
}

function Assert-PartisanNoReparseAncestry {
    param([Parameter(Mandatory = $true)][string]$Path)

    $cursor = [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    while (-not [string]::IsNullOrWhiteSpace($cursor)) {
        if (Test-Path -LiteralPath $cursor) {
            $item = Get-Item -LiteralPath $cursor -Force -ErrorAction Stop
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw 'A release-candidate path ancestor must not be a reparse point.'
            }
        }
        $parent = Split-Path -Parent $cursor
        if ([string]::IsNullOrWhiteSpace($parent) -or
            $parent.Equals($cursor, [StringComparison]::OrdinalIgnoreCase)) {
            break
        }
        $cursor = $parent
    }
}

function Assert-PartisanNoReparseTree {
    param([Parameter(Mandatory = $true)][string]$Root)

    Assert-PartisanNoReparseAncestry -Path $Root
    foreach ($item in @(Get-ChildItem `
            -LiteralPath $Root `
            -Recurse `
            -Force `
            -ErrorAction Stop)) {
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw 'A release-candidate tree must not contain a reparse point.'
        }
    }
}

function Assert-PartisanObjectProperties {
    param(
        [Parameter(Mandatory = $true)]$Value,
        [Parameter(Mandatory = $true)][string[]]$Names,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if ($null -eq $Value) {
        throw "$Label is missing."
    }
    $actualNames = @($Value.PSObject.Properties.Name)
    foreach ($name in $Names) {
        if ($actualNames -cnotcontains $name) {
            throw "$Label is missing required property $name."
        }
    }
}

function Assert-PartisanRuntimeUseDisposition {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$Disposition,
        [ValidateSet('runtime', 'verification')]
        [string]$ConsumerIntent = 'runtime'
    )

    if ($Disposition -cnotin @(
            'active-runtime-candidate',
            'supersede-before-runtime',
            'rejected-after-runtime')) {
        throw 'The current release-candidate runtime-use disposition is invalid.'
    }
    if ($ConsumerIntent -ceq 'runtime' -and
        $Disposition -cne 'active-runtime-candidate') {
        throw 'The current release candidate is not eligible for runtime use.'
    }
    return $Disposition
}

function Get-PartisanSha256 {
    param([Parameter(Mandatory = $true)][string]$Path)

    return (Get-FileHash `
        -LiteralPath $Path `
        -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-PartisanSha256Text {
    param([AllowEmptyString()][Parameter(Mandatory = $true)][string]$Text)

    $algorithm = [Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [Text.Encoding]::UTF8.GetBytes($Text)
        return ([BitConverter]::ToString(
            $algorithm.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $algorithm.Dispose()
    }
}

function Get-PartisanExecutableIdentity {
    param([Parameter(Mandatory = $true)][string]$Path)

    $file = Get-Item -LiteralPath $Path -Force -ErrorAction Stop
    if ($file.Length -le 0) {
        throw 'A release-candidate runtime executable is empty.'
    }
    return [pscustomobject][ordered]@{
        fileName = $file.Name
        fileVersion = [string]$file.VersionInfo.FileVersion
        productVersion = [string]$file.VersionInfo.ProductVersion
        length = [long]$file.Length
        sha256 = Get-PartisanSha256 -Path $file.FullName
    }
}

function Assert-PartisanExecutableIdentity {
    param(
        [Parameter(Mandatory = $true)]$Expected,
        [Parameter(Mandatory = $true)]$Actual
    )

    Assert-PartisanObjectProperties `
        -Value $Expected `
        -Names @('fileName', 'fileVersion', 'productVersion', 'length', 'sha256') `
        -Label 'Recorded runtime executable identity'
    if ([string]$Expected.fileName -cne [string]$Actual.fileName -or
        [string]$Expected.fileVersion -cne [string]$Actual.fileVersion -or
        [string]$Expected.productVersion -cne [string]$Actual.productVersion -or
        [long]$Expected.length -ne [long]$Actual.length -or
        [string]$Expected.sha256 -cne [string]$Actual.sha256) {
        throw 'The installed runtime executable differs from the sealed candidate toolchain.'
    }
}

function Assert-PartisanPortableRelativePath {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Prefix
    )

    if ([string]::IsNullOrWhiteSpace($Path) -or
        $Path.Contains('\') -or
        $Path.Contains(':') -or
        $Path.StartsWith('/', [StringComparison]::Ordinal) -or
        $Path.Split('/') -contains '..' -or
        -not $Path.StartsWith($Prefix, [StringComparison]::Ordinal)) {
        throw 'A release-candidate manifest path is not portable and contained.'
    }
}

function Assert-PartisanRecordedFiles {
    param(
        [Parameter(Mandatory = $true)][string]$BundleRoot,
        [Parameter(Mandatory = $true)][object[]]$Rows,
        [Parameter(Mandatory = $true)][string]$Prefix,
        [switch]$PackageRows
    )

    $seen = New-Object Collections.Generic.HashSet[string](
        [StringComparer]::Ordinal)
    foreach ($row in $Rows) {
        $required = @('path', 'length', 'sha256')
        if ($PackageRows) {
            $required += 'indexPath'
        }
        Assert-PartisanObjectProperties `
            -Value $row `
            -Names $required `
            -Label 'Release-candidate file record'
        $portablePath = [string]$row.path
        Assert-PartisanPortableRelativePath -Path $portablePath -Prefix $Prefix
        if (-not $seen.Add($portablePath)) {
            throw 'A release-candidate manifest contains a duplicate file path.'
        }
        if ([long]$row.length -le 0 -or
            [string]$row.sha256 -cnotmatch '^[0-9a-f]{64}$') {
            throw 'A release-candidate file record has an invalid length or SHA-256.'
        }
        $resolved = [IO.Path]::GetFullPath(
            (Join-Path $BundleRoot $portablePath.Replace('/', '\')))
        if (-not (Test-PartisanContainedPath `
                -Root $BundleRoot `
                -Candidate $resolved) -or
            -not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
            throw 'A recorded release-candidate file is missing or outside the bundle.'
        }
        $file = Get-Item -LiteralPath $resolved -Force -ErrorAction Stop
        if ([long]$file.Length -ne [long]$row.length -or
            (Get-PartisanSha256 -Path $resolved) -cne [string]$row.sha256) {
            throw 'A recorded release-candidate file differs from its sealed identity.'
        }
    }
}

function Get-PartisanPublicCandidateIdentity {
    param([Parameter(Mandatory = $true)]$Candidate)

    return [pscustomobject][ordered]@{
        candidateId = $Candidate.CandidateId
        candidateVersion = $Candidate.CandidateVersion
        runtimeUseDisposition = $Candidate.RuntimeUseDisposition
        gitHead = $Candidate.GitHead
        embeddedBuildSha = $Candidate.EmbeddedBuildSha
        embeddedBuildUtc = $Candidate.EmbeddedBuildUtc
        embeddedBuildLabel = $Candidate.EmbeddedBuildLabel
        campaignSchema = $Candidate.CampaignSchema
        runtimeSettingsSchema = $Candidate.RuntimeSettingsSchema
        addonId = $Candidate.AddonId
        addonGuid = $Candidate.AddonGuid
        packageHashAlgorithm = $Candidate.PackageHashAlgorithm
        packageSha256 = $Candidate.PackageSha256
        manifestSha256 = $Candidate.ManifestSha256
        readySha256 = $Candidate.ReadySha256
        workbenchCrc = $Candidate.WorkbenchCrc
        runtimeRole = $Candidate.RuntimeRole
        diagnosticExecutable = $Candidate.DiagnosticExecutable
        recordedDiagnosticExecutable = $Candidate.RecordedDiagnosticExecutable
        recordedRuntimeExecutable = $Candidate.RecordedRuntimeExecutable
    }
}

function Assert-PartisanReleaseCandidate {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$ManifestPath,
        [Parameter(Mandatory = $true)][string]$BundleRoot,
        [Parameter(Mandatory = $true)][string]$RuntimeAddonRoot,
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][ValidateSet('client', 'server')][string]$RuntimeRole,
        [ValidateSet('runtime', 'verification')][string]$ConsumerIntent = 'runtime'
    )

    $trackedManifestPath = Resolve-PartisanExistingPath -Path $ManifestPath -Kind Leaf
    $trackedReadyPath = Resolve-PartisanExistingPath `
        -Path (Join-Path (Split-Path -Parent $trackedManifestPath) 'candidate.ready.json') `
        -Kind Leaf
    $bundleRootPath = Resolve-PartisanExistingPath -Path $BundleRoot -Kind Container
    $runtimeAddonPath = Resolve-PartisanExistingPath -Path $RuntimeAddonRoot -Kind Container
    $executablePath = Resolve-PartisanExistingPath -Path $Executable -Kind Leaf
    if ($runtimeAddonPath.Contains(',')) {
        throw 'RuntimeAddonRoot must not contain the add-on search-path separator.'
    }

    $repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
    $releaseStatusPath = Resolve-PartisanExistingPath `
        -Path (Join-Path $repositoryRoot 'docs\data\release_status.json') `
        -Kind Leaf
    try {
        $releaseStatus = [IO.File]::ReadAllText($releaseStatusPath) | ConvertFrom-Json
    }
    catch {
        throw 'The current release status is not valid JSON.'
    }
    Assert-PartisanObjectProperties `
        -Value $releaseStatus `
        -Names @('artifact') `
        -Label 'Current release status'
    Assert-PartisanObjectProperties `
        -Value $releaseStatus.artifact `
        -Names @(
            'releaseCandidateBuilt',
            'runtimeUseDisposition',
            'candidateId',
            'candidateSourceHead',
            'manifestPath',
            'manifestSha256',
            'readySha256',
            'packageHashAlgorithm',
            'packageSha256',
            'packageVersion',
            'addonGuid',
            'workbenchCrc') `
        -Label 'Current release artifact status'
    $runtimeUseDisposition = Assert-PartisanRuntimeUseDisposition `
        -Disposition ([string]$releaseStatus.artifact.runtimeUseDisposition) `
        -ConsumerIntent $ConsumerIntent
    $statusManifestPath = [IO.Path]::GetFullPath(
        (Join-Path $repositoryRoot ([string]$releaseStatus.artifact.manifestPath)))
    if (-not $releaseStatus.artifact.releaseCandidateBuilt -or
        -not $trackedManifestPath.Equals(
            $statusManifestPath,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw 'CandidateManifest is not the active tracked release-candidate record.'
    }
    foreach ($path in @(
            $trackedManifestPath,
            $trackedReadyPath,
            $runtimeAddonPath,
            $executablePath)) {
        Assert-PartisanNoReparseAncestry -Path $path
    }
    Assert-PartisanNoReparseTree -Root $bundleRootPath

    $topLevelNames = @(Get-ChildItem `
        -LiteralPath $bundleRootPath `
        -Force `
        -ErrorAction Stop | ForEach-Object { $_.Name } | Sort-Object)
    $requiredTopLevelNames = @(
        'candidate.json',
        'candidate.ready.json',
        'evidence',
        'package'
    ) | Sort-Object
    if (@(Compare-Object `
            -ReferenceObject $requiredTopLevelNames `
            -DifferenceObject $topLevelNames `
            -CaseSensitive).Count -ne 0) {
        throw 'The sealed candidate bundle has an unexpected top-level layout.'
    }

    $bundleManifestPath = Resolve-PartisanExistingPath `
        -Path (Join-Path $bundleRootPath 'candidate.json') `
        -Kind Leaf
    $bundleReadyPath = Resolve-PartisanExistingPath `
        -Path (Join-Path $bundleRootPath 'candidate.ready.json') `
        -Kind Leaf
    $manifestSha = Get-PartisanSha256 -Path $trackedManifestPath
    $readySha = Get-PartisanSha256 -Path $trackedReadyPath
    if ((Get-PartisanSha256 -Path $bundleManifestPath) -cne $manifestSha -or
        (Get-PartisanSha256 -Path $bundleReadyPath) -cne $readySha) {
        throw 'The supplied candidate bundle does not match the tracked manifest and ready seal.'
    }

    $manifestText = [IO.File]::ReadAllText($trackedManifestPath)
    if ($manifestText -match '(?i)(?:[A-Z]:[\\/]|\\\\|file://|/(?:Users|home|mnt)/)') {
        throw 'The tracked release-candidate manifest contains a local absolute path.'
    }
    try {
        $manifest = $manifestText | ConvertFrom-Json
        $ready = [IO.File]::ReadAllText($trackedReadyPath) | ConvertFrom-Json
    }
    catch {
        throw 'The tracked release-candidate manifest or ready seal is not valid JSON.'
    }

    Assert-PartisanObjectProperties `
        -Value $manifest `
        -Names @('manifestSchemaVersion', 'candidate', 'source', 'addon', 'toolchain', 'workbench', 'package', 'evidence') `
        -Label 'Release-candidate manifest'
    Assert-PartisanObjectProperties `
        -Value $manifest.candidate `
        -Names @('id', 'version', 'state') `
        -Label 'Candidate identity'
    Assert-PartisanObjectProperties `
        -Value $manifest.source `
        -Names @('gitHead', 'embeddedImplementation', 'campaignSchema', 'runtimeSettingsSchema') `
        -Label 'Candidate source identity'
    Assert-PartisanObjectProperties `
        -Value $manifest.source.embeddedImplementation `
        -Names @('sha', 'utc', 'label') `
        -Label 'Embedded build identity'
    Assert-PartisanObjectProperties `
        -Value $manifest.addon `
        -Names @('id', 'guid') `
        -Label 'Candidate add-on identity'
    Assert-PartisanObjectProperties `
        -Value $manifest.toolchain `
        -Names @('server', 'client') `
        -Label 'Candidate toolchain identity'
    Assert-PartisanObjectProperties `
        -Value $manifest.workbench `
        -Names @('crc') `
        -Label 'Candidate Workbench identity'
    Assert-PartisanObjectProperties `
        -Value $manifest.package `
        -Names @('root', 'hashAlgorithm', 'sha256', 'canonicalIndexPath', 'files') `
        -Label 'Candidate package identity'
    Assert-PartisanObjectProperties `
        -Value $manifest.evidence `
        -Names @('root', 'files') `
        -Label 'Candidate evidence identity'
    Assert-PartisanObjectProperties `
        -Value $ready `
        -Names @('schemaVersion', 'candidateId', 'gitHead', 'packageSha256', 'manifestSha256') `
        -Label 'Candidate ready seal'

    $candidateId = [string]$manifest.candidate.id
    if ([int]$manifest.manifestSchemaVersion -ne $script:ManifestSchemaVersion -or
        $candidateId -cnotmatch '^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$' -or
        [string]$manifest.candidate.version -cnotmatch '^[0-9A-Za-z][0-9A-Za-z._-]{0,95}$' -or
        [string]$manifest.candidate.state -cne 'retained-uncertified' -or
        (Split-Path -Leaf $bundleRootPath) -cne $candidateId -or
        [string]$manifest.source.gitHead -cnotmatch '^[0-9a-f]{40}$' -or
        [string]$manifest.source.embeddedImplementation.sha -cnotmatch '^[0-9a-f]{40}$' -or
        [int]$manifest.source.campaignSchema -le 0 -or
        [int]$manifest.source.runtimeSettingsSchema -le 0 -or
        [string]$manifest.addon.id -cnotmatch '^[A-Za-z_][A-Za-z0-9_]*$' -or
        [string]$manifest.addon.guid -cnotmatch '^[0-9A-F]{16}$' -or
        [string]$manifest.workbench.crc -cnotmatch '^[0-9a-f]{8}$' -or
        [string]$manifest.package.root -cne 'package/Partisan' -or
        [string]$manifest.package.hashAlgorithm -cne $script:PackageHashAlgorithm -or
        [string]$manifest.package.sha256 -cnotmatch '^[0-9a-f]{64}$' -or
        [string]$manifest.package.canonicalIndexPath -cne 'evidence/pack/files.sha256' -or
        [string]$manifest.evidence.root -cne 'evidence') {
        throw 'The tracked release-candidate identity or schema contract is invalid.'
    }
    if ([int]$ready.schemaVersion -ne 1 -or
        [string]$ready.candidateId -cne $candidateId -or
        [string]$ready.gitHead -cne [string]$manifest.source.gitHead -or
        [string]$ready.packageSha256 -cne [string]$manifest.package.sha256 -or
        [string]$ready.manifestSha256 -cne $manifestSha) {
        throw 'The tracked candidate ready seal does not match the manifest.'
    }
    if ([string]$releaseStatus.artifact.candidateId -cne $candidateId -or
        [string]$releaseStatus.artifact.candidateSourceHead -cne [string]$manifest.source.gitHead -or
        [string]$releaseStatus.artifact.manifestSha256 -cne $manifestSha -or
        [string]$releaseStatus.artifact.readySha256 -cne $readySha -or
        [string]$releaseStatus.artifact.packageHashAlgorithm -cne [string]$manifest.package.hashAlgorithm -or
        [string]$releaseStatus.artifact.packageSha256 -cne [string]$manifest.package.sha256 -or
        [string]$releaseStatus.artifact.packageVersion -cne [string]$manifest.candidate.version -or
        [string]$releaseStatus.artifact.addonGuid -cne [string]$manifest.addon.guid -or
        [string]$releaseStatus.artifact.workbenchCrc -cne [string]$manifest.workbench.crc) {
        throw 'The tracked candidate record differs from the current release status.'
    }

    $packageRows = @($manifest.package.files)
    $evidenceRows = @($manifest.evidence.files)
    if ($packageRows.Count -ne $script:RequiredPackageFiles.Count -or
        $evidenceRows.Count -eq 0) {
        throw 'The sealed candidate package or evidence inventory is incomplete.'
    }
    Assert-PartisanRecordedFiles `
        -BundleRoot $bundleRootPath `
        -Rows $packageRows `
        -Prefix 'package/Partisan/' `
        -PackageRows
    Assert-PartisanRecordedFiles `
        -BundleRoot $bundleRootPath `
        -Rows $evidenceRows `
        -Prefix 'evidence/'

    $packageInventoryRoot = Join-Path $bundleRootPath 'package'
    $packageInventoryPrefix = [IO.Path]::GetFullPath(
        $packageInventoryRoot).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    $expectedPackagePaths = @($packageRows | ForEach-Object {
        [string]$_.path
    } | Sort-Object)
    $actualPackagePaths = @(Get-ChildItem `
        -LiteralPath $packageInventoryRoot `
        -Recurse `
        -File `
        -Force `
        -ErrorAction Stop | ForEach-Object {
            $full = [IO.Path]::GetFullPath($_.FullName)
            if (-not $full.StartsWith(
                    $packageInventoryPrefix,
                    [StringComparison]::OrdinalIgnoreCase)) {
                throw 'An external candidate package file escaped its root.'
            }
            'package/' +
                $full.Substring($packageInventoryPrefix.Length).Replace('\', '/')
        } | Sort-Object)
    if (@(Compare-Object `
            -ReferenceObject $expectedPackagePaths `
            -DifferenceObject $actualPackagePaths `
            -CaseSensitive).Count -ne 0) {
        throw 'The external candidate does not contain the exact sealed package inventory.'
    }

    $expectedPackageDirectories = @('package/Partisan')
    $actualPackageDirectories = @(Get-ChildItem `
        -LiteralPath $packageInventoryRoot `
        -Recurse `
        -Directory `
        -Force `
        -ErrorAction Stop | ForEach-Object {
            $full = [IO.Path]::GetFullPath($_.FullName)
            if (-not $full.StartsWith(
                    $packageInventoryPrefix,
                    [StringComparison]::OrdinalIgnoreCase)) {
                throw 'An external candidate package directory escaped its root.'
            }
            'package/' +
                $full.Substring($packageInventoryPrefix.Length).Replace('\', '/')
        } | Sort-Object)
    if (@(Compare-Object `
            -ReferenceObject $expectedPackageDirectories `
            -DifferenceObject $actualPackageDirectories `
            -CaseSensitive).Count -ne 0) {
        throw 'The external candidate does not contain the exact sealed package directory layout.'
    }
    $evidenceRootPath = Join-Path $bundleRootPath 'evidence'
    $evidencePrefix = [IO.Path]::GetFullPath($evidenceRootPath).TrimEnd('\', '/') +
        [IO.Path]::DirectorySeparatorChar
    $expectedEvidencePaths = @($evidenceRows | ForEach-Object {
        [string]$_.path
    } | Sort-Object)
    $actualEvidencePaths = @(Get-ChildItem `
        -LiteralPath $evidenceRootPath `
        -Recurse `
        -File `
        -Force `
        -ErrorAction Stop | ForEach-Object {
            $full = [IO.Path]::GetFullPath($_.FullName)
            if (-not $full.StartsWith(
                    $evidencePrefix,
                    [StringComparison]::OrdinalIgnoreCase)) {
                throw 'An external candidate evidence file escaped its root.'
            }
            'evidence/' + $full.Substring($evidencePrefix.Length).Replace('\', '/')
        } | Sort-Object)
    if (@(Compare-Object `
            -ReferenceObject $expectedEvidencePaths `
            -DifferenceObject $actualEvidencePaths `
            -CaseSensitive).Count -ne 0) {
        throw 'The external candidate does not contain the exact sealed evidence inventory.'
    }

    $actualIndexPaths = @($packageRows | ForEach-Object {
        Assert-PartisanPortableRelativePath `
            -Path ([string]$_.indexPath) `
            -Prefix 'Partisan/'
        [string]$_.indexPath
    } | Sort-Object)
    if (@(Compare-Object `
            -ReferenceObject @($script:RequiredPackageFiles | Sort-Object) `
            -DifferenceObject $actualIndexPaths `
            -CaseSensitive).Count -ne 0) {
        throw 'The candidate manifest does not describe the exact four-file package.'
    }
    $canonicalRows = @($packageRows | Sort-Object indexPath | ForEach-Object {
        "{0}`t{1}`t{2}" -f $_.sha256, ([long]$_.length), $_.indexPath
    })
    $canonicalIndex = ($canonicalRows -join "`n") + "`n"
    if ((Get-PartisanSha256Text -Text $canonicalIndex) -cne
            [string]$manifest.package.sha256) {
        throw 'The candidate package digest does not match its canonical file inventory.'
    }
    $canonicalIndexPath = Join-Path $bundleRootPath 'evidence\pack\files.sha256'
    if (-not (Test-Path -LiteralPath $canonicalIndexPath -PathType Leaf) -or
        [IO.File]::ReadAllText($canonicalIndexPath).Replace("`r`n", "`n") -cne
            $canonicalIndex) {
        throw 'The candidate canonical package index is missing or stale.'
    }

    $packageParentPath = Resolve-PartisanExistingPath `
        -Path (Join-Path $bundleRootPath 'package') `
        -Kind Container
    $packedAddonPath = Resolve-PartisanExistingPath `
        -Path (Join-Path $packageParentPath 'Partisan') `
        -Kind Container
    $packedProjectPath = Resolve-PartisanExistingPath `
        -Path (Join-Path $packedAddonPath 'addon.gproj') `
        -Kind Leaf
    foreach ($baseMarker in @('core\data.pak', 'data\data.pak')) {
        if (-not (Test-Path `
                -LiteralPath (Join-Path $runtimeAddonPath $baseMarker) `
                -PathType Leaf)) {
            throw 'RuntimeAddonRoot must be the installed packed base-game add-on root.'
        }
    }
    foreach ($project in @(Get-ChildItem `
            -LiteralPath $runtimeAddonPath `
            -Filter 'addon.gproj' `
            -File `
            -Recurse `
            -Force `
            -ErrorAction Stop)) {
        $projectText = [IO.File]::ReadAllText($project.FullName)
        if ($projectText -match ('(?m)^\s*GUID\s+"' +
                [regex]::Escape([string]$manifest.addon.guid) + '"\s*$')) {
            throw 'RuntimeAddonRoot contains a competing copy of the sealed candidate GUID.'
        }
    }

    $expectedDiagnosticLeaf = if ($RuntimeRole -ceq 'client') {
        'ArmaReforgerSteamDiag.exe'
    }
    else {
        'ArmaReforgerServerDiag.exe'
    }
    $standardLeaf = if ($RuntimeRole -ceq 'client') {
        'ArmaReforgerSteam.exe'
    }
    else {
        'ArmaReforgerServer.exe'
    }
    if ((Split-Path -Leaf $executablePath) -cne $expectedDiagnosticLeaf) {
        throw 'The diagnostic runtime executable does not match the requested candidate role.'
    }
    $standardExecutablePath = Resolve-PartisanExistingPath `
        -Path (Join-Path (Split-Path -Parent $executablePath) $standardLeaf) `
        -Kind Leaf
    $diagnosticIdentity = Get-PartisanExecutableIdentity -Path $executablePath
    $standardIdentity = Get-PartisanExecutableIdentity -Path $standardExecutablePath
    $recordedIdentity = if ($RuntimeRole -ceq 'client') {
        $manifest.toolchain.client
    }
    else {
        $manifest.toolchain.server
    }
    Assert-PartisanExecutableIdentity `
        -Expected $recordedIdentity `
        -Actual $standardIdentity
    $serverDiagnosticProperty = $manifest.toolchain.PSObject.Properties['serverDiagnostic']
    $clientDiagnosticProperty = $manifest.toolchain.PSObject.Properties['clientDiagnostic']
    if (($null -eq $serverDiagnosticProperty) -xor
        ($null -eq $clientDiagnosticProperty)) {
        throw 'The candidate manifest has an incomplete diagnostic executable identity pair.'
    }
    if ($runtimeUseDisposition -ceq 'active-runtime-candidate' -and
        ($null -eq $serverDiagnosticProperty -or
            $null -eq $clientDiagnosticProperty)) {
        throw 'An active runtime candidate must seal both diagnostic executable identities.'
    }
    $recordedDiagnosticIdentity = if ($RuntimeRole -ceq 'client' -and
        $null -ne $clientDiagnosticProperty) {
        $clientDiagnosticProperty.Value
    }
    elseif ($RuntimeRole -ceq 'server' -and
        $null -ne $serverDiagnosticProperty) {
        $serverDiagnosticProperty.Value
    }
    else {
        $null
    }
    if ([string]$diagnosticIdentity.fileVersion -cne
        [string]$standardIdentity.fileVersion -or
        [string]$diagnosticIdentity.productVersion -cne
        [string]$standardIdentity.productVersion) {
        throw 'The diagnostic runtime version differs from the sealed standard runtime.'
    }
    if ($null -ne $recordedDiagnosticIdentity) {
        Assert-PartisanExecutableIdentity `
            -Expected $recordedDiagnosticIdentity `
            -Actual $diagnosticIdentity
    }

    return [pscustomobject][ordered]@{
        CandidateId = $candidateId
        CandidateVersion = [string]$manifest.candidate.version
        RuntimeUseDisposition = $runtimeUseDisposition
        GitHead = [string]$manifest.source.gitHead
        EmbeddedBuildSha = [string]$manifest.source.embeddedImplementation.sha
        EmbeddedBuildUtc = [string]$manifest.source.embeddedImplementation.utc
        EmbeddedBuildLabel = [string]$manifest.source.embeddedImplementation.label
        CampaignSchema = [int]$manifest.source.campaignSchema
        RuntimeSettingsSchema = [int]$manifest.source.runtimeSettingsSchema
        AddonId = [string]$manifest.addon.id
        AddonGuid = [string]$manifest.addon.guid
        PackageHashAlgorithm = [string]$manifest.package.hashAlgorithm
        PackageSha256 = [string]$manifest.package.sha256
        ManifestSha256 = $manifestSha
        ReadySha256 = $readySha
        WorkbenchCrc = [string]$manifest.workbench.crc
        RuntimeRole = $RuntimeRole
        DiagnosticExecutable = $diagnosticIdentity
        RecordedDiagnosticExecutable = $recordedDiagnosticIdentity
        RecordedRuntimeExecutable = $standardIdentity
        TrackedManifestPath = $trackedManifestPath
        TrackedReadyPath = $trackedReadyPath
        BundleRootPath = $bundleRootPath
        RuntimeAddonRootPath = $runtimeAddonPath
        ExecutablePath = $executablePath
        PackageParentPath = $packageParentPath
        PackedAddonPath = $packedAddonPath
        PackedProjectPath = $packedProjectPath
        AddonSearchPath = $runtimeAddonPath + ',' + $packageParentPath
        PackageFiles = $packageRows
    }
}

function Assert-PartisanReleaseCandidateStage {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]$Candidate,
        [Parameter(Mandatory = $true)][string]$StageRoot
    )

    $stageRootPath = Resolve-PartisanExistingPath -Path $StageRoot -Kind Container
    if ($Candidate.RuntimeAddonRootPath.Contains(',') -or
        $stageRootPath.Contains(',')) {
        throw 'A candidate stage path must not contain the add-on search-path separator.'
    }
    Assert-PartisanNoReparseTree -Root $stageRootPath
    $stageRootNames = @(Get-ChildItem `
        -LiteralPath $stageRootPath `
        -Force `
        -ErrorAction Stop | ForEach-Object { $_.Name } | Sort-Object)
    if (@(Compare-Object `
            -ReferenceObject @('Partisan') `
            -DifferenceObject $stageRootNames `
            -CaseSensitive).Count -ne 0) {
        throw 'The guarded candidate add-on search root must contain only Partisan.'
    }
    $stagedAddonPath = Resolve-PartisanExistingPath `
        -Path (Join-Path $stageRootPath 'Partisan') `
        -Kind Container
    $actualNames = @(Get-ChildItem `
        -LiteralPath $stagedAddonPath `
        -Force `
        -ErrorAction Stop | ForEach-Object { $_.Name } | Sort-Object)
    $expectedNames = @($script:RequiredPackageFiles | ForEach-Object {
        Split-Path -Leaf $_
    } | Sort-Object)
    if (@(Compare-Object `
            -ReferenceObject $expectedNames `
            -DifferenceObject $actualNames `
            -CaseSensitive).Count -ne 0) {
        throw 'The guarded candidate stage does not contain the exact four-file package.'
    }

    $indexRows = New-Object Collections.Generic.List[string]
    foreach ($record in @($Candidate.PackageFiles | Sort-Object indexPath)) {
        $name = Split-Path -Leaf ([string]$record.indexPath)
        $path = Resolve-PartisanExistingPath `
            -Path (Join-Path $stagedAddonPath $name) `
            -Kind Leaf
        $file = Get-Item -LiteralPath $path -Force -ErrorAction Stop
        $sha = Get-PartisanSha256 -Path $path
        if ([long]$file.Length -ne [long]$record.length -or
            $sha -cne [string]$record.sha256) {
            throw 'A guarded candidate-stage file differs from the sealed package.'
        }
        [void]$indexRows.Add(("{0}`t{1}`t{2}" -f
            $sha,
            [long]$file.Length,
            [string]$record.indexPath))
    }
    $digest = Get-PartisanSha256Text `
        -Text (($indexRows.ToArray() -join "`n") + "`n")
    if ($digest -cne $Candidate.PackageSha256) {
        throw 'The guarded candidate-stage digest differs from the sealed package.'
    }
    return [pscustomobject][ordered]@{
        StageRootPath = $stageRootPath
        PackedAddonPath = $stagedAddonPath
        PackedProjectPath = Join-Path $stagedAddonPath 'addon.gproj'
        AddonSearchPath = $Candidate.RuntimeAddonRootPath + ',' + $stageRootPath
        PackageSha256 = $digest
    }
}

function New-PartisanReleaseCandidateStage {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]$Candidate,
        [Parameter(Mandatory = $true)][string]$GuardRoot
    )

    $guardRootPath = Resolve-PartisanExistingPath -Path $GuardRoot -Kind Container
    Assert-PartisanNoReparseAncestry -Path $guardRootPath
    $stageRoot = [IO.Path]::GetFullPath(
        (Join-Path $guardRootPath 'candidate-addons'))
    if (-not (Test-PartisanContainedPath `
            -Root $guardRootPath `
            -Candidate $stageRoot) -or
        (Test-Path -LiteralPath $stageRoot)) {
        throw 'The guarded candidate stage must be a fresh contained directory.'
    }
    $stagedAddonPath = Join-Path $stageRoot 'Partisan'
    New-Item -ItemType Directory -Path $stagedAddonPath -Force | Out-Null
    foreach ($record in @($Candidate.PackageFiles)) {
        $name = Split-Path -Leaf ([string]$record.indexPath)
        $source = Join-Path $Candidate.PackedAddonPath $name
        Copy-Item `
            -LiteralPath $source `
            -Destination (Join-Path $stagedAddonPath $name) `
            -Force `
            -ErrorAction Stop
    }
    return Assert-PartisanReleaseCandidateStage `
        -Candidate $Candidate `
        -StageRoot $stageRoot
}

Export-ModuleMember `
    -Function Assert-PartisanReleaseCandidate, `
        Assert-PartisanRuntimeUseDisposition, `
        Assert-PartisanReleaseCandidateStage, `
        Get-PartisanPublicCandidateIdentity, `
        New-PartisanReleaseCandidateStage
