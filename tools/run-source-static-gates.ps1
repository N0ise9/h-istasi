[CmdletBinding()]
param(
    [string]$EvidenceRoot = '',
    [string]$SourceGitHead = '',
    [switch]$Foundation,
    [switch]$Workbench,
    [string]$WorkbenchExecutable = '',
    [string[]]$AddonRoots = @(),
    [string[]]$DefaultLogRoots = @(),
    [string[]]$SpillRoots = @(),
    [ValidateRange(1, 3600)]
    [int]$WorkbenchTimeoutSeconds = 240,
    [ValidateRange(50, 5000)]
    [int]$WorkbenchPollMilliseconds = 500,
    [switch]$SelfTest
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:PublishInputScope = @(
    'Assets',
    'Configs',
    'Missions',
    'Prefabs',
    'Scripts',
    'UI',
    'Worlds',
    'addon.gproj',
    'thumbnail.png'
)
$script:RequiredWorkbenchTargets = @(
    'PC',
    'XBOX_ONE',
    'XBOX_SERIES',
    'PS4',
    'PS5'
)
$script:HarnessHashPolicy = 'sha256-git-blob-bytes-v1'

function Get-Sha256Bytes {
    param([Parameter(Mandatory = $true)][byte[]]$Bytes)

    $hasher = [Security.Cryptography.SHA256]::Create()
    try {
        $hash = $hasher.ComputeHash($Bytes)
    }
    finally {
        $hasher.Dispose()
    }
    return -join @($hash | ForEach-Object { $_.ToString('x2') })
}

function Get-Sha256Text {
    param([Parameter(Mandatory = $true)][AllowEmptyString()][string]$Text)

    $encoding = New-Object Text.UTF8Encoding($false)
    return Get-Sha256Bytes -Bytes $encoding.GetBytes($Text)
}

function ConvertTo-NativeArgument {
    param([Parameter(Mandatory = $true)][AllowEmptyString()][string]$Value)

    if ($Value.Length -gt 0 -and $Value -notmatch '[\s"]') {
        return $Value
    }
    $builder = New-Object Text.StringBuilder
    [void]$builder.Append('"')
    $backslashes = 0
    foreach ($character in $Value.ToCharArray()) {
        if ($character -eq '\') {
            $backslashes++
            continue
        }
        if ($character -eq '"') {
            [void]$builder.Append(('\' * (($backslashes * 2) + 1)))
            [void]$builder.Append('"')
            $backslashes = 0
            continue
        }
        if ($backslashes -gt 0) {
            [void]$builder.Append(('\' * $backslashes))
            $backslashes = 0
        }
        [void]$builder.Append($character)
    }
    if ($backslashes -gt 0) {
        [void]$builder.Append(('\' * ($backslashes * 2)))
    }
    [void]$builder.Append('"')
    return $builder.ToString()
}

function Invoke-GitLines {
    param(
        [Parameter(Mandatory = $true)][string]$CheckoutRoot,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    $output = @(& git -C $CheckoutRoot @Arguments 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw 'A required Git identity command failed.'
    }
    return @($output | ForEach-Object { [string]$_ })
}

function Invoke-NativeByteCapture {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory
    )

    $startInfo = New-Object Diagnostics.ProcessStartInfo
    $startInfo.FileName = $Executable
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.Arguments = @($Arguments | ForEach-Object {
        ConvertTo-NativeArgument -Value ([string]$_)
    }) -join ' '

    $process = New-Object Diagnostics.Process
    $process.StartInfo = $startInfo
    $stdout = New-Object IO.MemoryStream
    try {
        if (-not $process.Start()) {
            throw 'A native identity process did not start.'
        }
        $stderrTask = $process.StandardError.ReadToEndAsync()
        $copyTask = $process.StandardOutput.BaseStream.CopyToAsync($stdout)
        $process.WaitForExit()
        [void]$copyTask.GetAwaiter().GetResult()
        $stderr = $stderrTask.GetAwaiter().GetResult()
        if ($process.ExitCode -ne 0) {
            throw "A native identity process failed: $stderr"
        }
        return ,$stdout.ToArray()
    }
    finally {
        $stdout.Dispose()
        $process.Dispose()
    }
}

function ConvertFrom-NulTerminatedUtf8 {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyCollection()][byte[]]$Bytes
    )

    if ($Bytes.Length -eq 0) {
        return @()
    }
    if ($Bytes[$Bytes.Length - 1] -ne 0) {
        throw 'A NUL-delimited Git path stream was not terminated.'
    }
    $encoding = New-Object Text.UTF8Encoding($false, $true)
    try {
        $text = $encoding.GetString($Bytes)
    }
    catch {
        throw 'A NUL-delimited Git path stream was not valid UTF-8.'
    }
    $records = @($text.Split([char]0))
    if ($records.Count -lt 2 -or
        -not [string]::IsNullOrEmpty($records[$records.Count - 1])) {
        throw 'A NUL-delimited Git path stream was malformed.'
    }
    $result = New-Object Collections.Generic.List[string]
    for ($index = 0; $index -lt ($records.Count - 1); $index++) {
        if ([string]::IsNullOrEmpty($records[$index])) {
            throw 'A NUL-delimited Git path stream contained an empty path.'
        }
        $result.Add([string]$records[$index])
    }
    return $result.ToArray()
}

function Invoke-GitNulPaths {
    param(
        [Parameter(Mandatory = $true)][string]$CheckoutRoot,
        [Parameter(Mandatory = $true)][string]$GitExecutable,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    [byte[]]$bytes = Invoke-NativeByteCapture `
        -Executable $GitExecutable `
        -Arguments (@('-C', $CheckoutRoot) + $Arguments) `
        -WorkingDirectory $CheckoutRoot
    return @(ConvertFrom-NulTerminatedUtf8 -Bytes $bytes)
}

function Get-CheckoutRelativePath {
    param(
        [Parameter(Mandatory = $true)][string]$CheckoutRoot,
        [Parameter(Mandatory = $true)][string]$Path
    )

    $root = [IO.Path]::GetFullPath($CheckoutRoot).TrimEnd('\', '/')
    $full = [IO.Path]::GetFullPath($Path)
    $prefix = $root + [IO.Path]::DirectorySeparatorChar
    if (-not $full.StartsWith(
            $prefix,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw 'A source-gate path escaped the checkout.'
    }
    return $full.Substring($prefix.Length).Replace('\', '/')
}

function Get-WorktreeCleanBlobOid {
    param(
        [Parameter(Mandatory = $true)][string]$CheckoutRoot,
        [Parameter(Mandatory = $true)][string]$RelativePath
    )

    $normalized = $RelativePath.Replace('\', '/')
    if ([IO.Path]::IsPathRooted($normalized) -or
        $normalized -match '(^|/)\.\.(/|$)') {
        throw 'A clean-filter path escaped the checkout.'
    }
    $fullPath = [IO.Path]::GetFullPath((Join-Path $CheckoutRoot $normalized))
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
        throw "A clean-filter worktree file is missing: $normalized"
    }
    $rows = @(Invoke-GitLines `
        -CheckoutRoot $CheckoutRoot `
        -Arguments @(
            'hash-object',
            "--path=$normalized",
            '--',
            $fullPath))
    if ($rows.Count -ne 1 -or $rows[0] -cnotmatch '^[0-9a-f]{40}$') {
        throw "A clean-filter worktree identity is invalid: $normalized"
    }
    return $rows[0]
}

function Assert-WorktreeCleanBlobMatches {
    param(
        [Parameter(Mandatory = $true)][string]$CheckoutRoot,
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][string]$ExpectedOid
    )

    $actualOid = Get-WorktreeCleanBlobOid `
        -CheckoutRoot $CheckoutRoot `
        -RelativePath $RelativePath
    if ($actualOid -cne $ExpectedOid) {
        throw "Executed or published worktree bytes differ from Git: $RelativePath"
    }
    return $actualOid
}

function Get-GitBlobIdentity {
    param(
        [Parameter(Mandatory = $true)][string]$CheckoutRoot,
        [Parameter(Mandatory = $true)][string]$Head,
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][string]$GitExecutable
    )

    $normalizedPath = $RelativePath.Replace('\', '/')
    if ([IO.Path]::IsPathRooted($normalizedPath) -or
        $normalizedPath -match '(^|/)\.\.(/|$)') {
        throw 'A harness path escaped the checkout.'
    }
    $objectRows = @(Invoke-GitLines `
        -CheckoutRoot $CheckoutRoot `
        -Arguments @('rev-parse', "$Head`:$normalizedPath"))
    if ($objectRows.Count -ne 1 -or
        $objectRows[0] -cnotmatch '^[0-9a-f]{40}$') {
        throw "A harness Git blob could not be resolved: $normalizedPath"
    }
    $objectId = $objectRows[0]
    $typeRows = @(Invoke-GitLines `
        -CheckoutRoot $CheckoutRoot `
        -Arguments @('cat-file', '-t', $objectId))
    if ($typeRows.Count -ne 1 -or $typeRows[0] -cne 'blob') {
        throw "A harness identity is not a Git blob: $normalizedPath"
    }
    [byte[]]$bytes = Invoke-NativeByteCapture `
        -Executable $GitExecutable `
        -Arguments @('-C', $CheckoutRoot, 'cat-file', 'blob', $objectId) `
        -WorkingDirectory $CheckoutRoot
    return [pscustomobject][ordered]@{
        path = $normalizedPath
        gitBlobOid = $objectId
        hashPolicy = $script:HarnessHashPolicy
        sha256 = Get-Sha256Bytes -Bytes $bytes
        length = $bytes.Length
    }
}

function Get-GitBlobText {
    param(
        [Parameter(Mandatory = $true)][string]$CheckoutRoot,
        [Parameter(Mandatory = $true)][string]$Head,
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][string]$GitExecutable
    )

    $identity = Get-GitBlobIdentity `
        -CheckoutRoot $CheckoutRoot `
        -Head $Head `
        -RelativePath $RelativePath `
        -GitExecutable $GitExecutable
    [byte[]]$bytes = Invoke-NativeByteCapture `
        -Executable $GitExecutable `
        -Arguments @('-C', $CheckoutRoot, 'cat-file', 'blob', $identity.gitBlobOid) `
        -WorkingDirectory $CheckoutRoot
    $encoding = New-Object Text.UTF8Encoding($false, $true)
    try {
        return $encoding.GetString($bytes)
    }
    catch {
        throw "A required Git blob is not valid UTF-8 text: $RelativePath"
    }
}

function Get-PublishInputBinding {
    param(
        [Parameter(Mandatory = $true)][string]$CheckoutRoot,
        [Parameter(Mandatory = $true)][string]$Head
    )

    $rawRows = @(Invoke-GitLines `
        -CheckoutRoot $CheckoutRoot `
        -Arguments (@('ls-tree', '-r', '--full-tree', $Head, '--') +
            $script:PublishInputScope))
    if ($rawRows.Count -eq 0) {
        throw 'The canonical publish-input tree is empty.'
    }

    $rows = New-Object Collections.Generic.List[object]
    $seen = New-Object 'Collections.Generic.HashSet[string]' `
        ([StringComparer]::Ordinal)
    $seenIgnoreCase = New-Object 'Collections.Generic.HashSet[string]' `
        ([StringComparer]::OrdinalIgnoreCase)
    foreach ($rawRow in $rawRows) {
        if ($rawRow -cnotmatch
                '^(?<mode>[0-9]{6}) (?<type>blob) (?<oid>[0-9a-f]{40})\t(?<path>.+)$') {
            throw "A canonical publish-input row is malformed: $rawRow"
        }
        $mode = $Matches.mode
        $type = $Matches.type
        $oid = $Matches.oid
        $path = $Matches.path.Replace('\', '/')
        $inScope = $path -ceq 'addon.gproj' -or
            $path -ceq 'thumbnail.png' -or
            $path -cmatch '^(Assets|Configs|Missions|Prefabs|Scripts|UI|Worlds)/.+'
        if (-not $inScope -or
            -not $seen.Add($path) -or
            -not $seenIgnoreCase.Add($path)) {
            throw "A canonical publish-input path is invalid or duplicated: $path"
        }
        $rows.Add([pscustomobject]@{
            path = $path
            mode = $mode
            type = $type
            oid = $oid
            canonical = $mode + ' ' + $type + ' ' + $oid + "`t" + $path
        })
    }

    [string[]]$orderedPaths = @($rows.ToArray() |
        ForEach-Object { [string]$_.path })
    [Array]::Sort($orderedPaths, [StringComparer]::Ordinal)
    $canonicalByPath = @{}
    foreach ($row in $rows) {
        $canonicalByPath[[string]$row.path] = [string]$row.canonical
    }
    $canonicalText = (@($orderedPaths | ForEach-Object {
        $canonicalByPath[$_]
    }) -join "`n") + "`n"
    return [pscustomobject][ordered]@{
        policy = 'git-ls-tree-sha256-v1'
        rowCount = $rows.Count
        sha256 = Get-Sha256Text -Text $canonicalText
        canonicalText = $canonicalText
        files = @($orderedPaths | ForEach-Object { $canonicalByPath[$_] } |
            ForEach-Object {
                $canonicalRow = [string]$_
                if ($canonicalRow -cnotmatch
                        '^(?<mode>[0-9]{6}) (?<type>blob) (?<oid>[0-9a-f]{40})\t(?<path>.+)$') {
                    throw 'An ordered publish-input row became malformed.'
                }
                [pscustomobject][ordered]@{
                    path = $Matches.path
                    mode = $Matches.mode
                    type = $Matches.type
                    oid = $Matches.oid
                }
            })
    }
}

function Test-GeneratedArchivePath {
    param([Parameter(Mandatory = $true)][string]$Path)

    return [IO.Path]::GetExtension($Path).Equals(
        '.pak',
        [StringComparison]::OrdinalIgnoreCase)
}

function Get-ArchivePathCount {
    param([Parameter(Mandatory = $true)][string[]]$Paths)

    $archives = New-Object 'Collections.Generic.HashSet[string]' `
        ([StringComparer]::OrdinalIgnoreCase)
    foreach ($path in $Paths) {
        if (Test-GeneratedArchivePath -Path ([string]$path)) {
            [void]$archives.Add(([string]$path).Replace('\', '/'))
        }
    }
    return $archives.Count
}

function Get-CheckoutArchiveCensus {
    param(
        [Parameter(Mandatory = $true)][string]$CheckoutRoot,
        [Parameter(Mandatory = $true)][string]$SourceHead,
        [Parameter(Mandatory = $true)][string]$HarnessHead,
        [Parameter(Mandatory = $true)][string]$GitExecutable
    )

    $sourceTracked = @(Invoke-GitNulPaths `
        -CheckoutRoot $CheckoutRoot `
        -GitExecutable $GitExecutable `
        -Arguments @('ls-tree', '-rz', '--name-only', $SourceHead, '--'))
    $harnessTracked = @(Invoke-GitNulPaths `
        -CheckoutRoot $CheckoutRoot `
        -GitExecutable $GitExecutable `
        -Arguments @('ls-tree', '-rz', '--name-only', $HarnessHead, '--'))
    $indexPaths = @(Invoke-GitNulPaths `
        -CheckoutRoot $CheckoutRoot `
        -GitExecutable $GitExecutable `
        -Arguments @('ls-files', '-z', '--cached'))
    $untrackedPaths = @(Invoke-GitNulPaths `
        -CheckoutRoot $CheckoutRoot `
        -GitExecutable $GitExecutable `
        -Arguments @('ls-files', '-z', '--others', '--exclude-standard'))
    $ignoredPaths = @(Invoke-GitNulPaths `
        -CheckoutRoot $CheckoutRoot `
        -GitExecutable $GitExecutable `
        -Arguments @(
            'ls-files', '-z', '--others', '--ignored', '--exclude-standard'))
    $workspacePaths = @($indexPaths + $untrackedPaths + $ignoredPaths)
    return [pscustomobject][ordered]@{
        sourceTracked = Get-ArchivePathCount -Paths $sourceTracked
        harnessTracked = Get-ArchivePathCount -Paths $harnessTracked
        workspaceTrackedUntrackedIgnored =
            Get-ArchivePathCount -Paths $workspacePaths
    }
}

function Test-ArchiveCensusZero {
    param([Parameter(Mandatory = $true)]$Census)

    return [int]$Census.sourceTracked -eq 0 -and
        [int]$Census.harnessTracked -eq 0 -and
        [int]$Census.workspaceTrackedUntrackedIgnored -eq 0
}

function Get-ExtraPublishWorktreePathCount {
    param(
        [Parameter(Mandatory = $true)][string]$CheckoutRoot,
        [Parameter(Mandatory = $true)][string]$GitExecutable
    )

    $untracked = @(Invoke-GitNulPaths `
        -CheckoutRoot $CheckoutRoot `
        -GitExecutable $GitExecutable `
        -Arguments (@(
                'ls-files', '-z', '--others', '--exclude-standard', '--') +
            $script:PublishInputScope))
    $ignored = @(Invoke-GitNulPaths `
        -CheckoutRoot $CheckoutRoot `
        -GitExecutable $GitExecutable `
        -Arguments (@(
                'ls-files', '-z', '--others', '--ignored',
                '--exclude-standard', '--') + $script:PublishInputScope))
    $paths = New-Object 'Collections.Generic.HashSet[string]' `
        ([StringComparer]::OrdinalIgnoreCase)
    foreach ($path in @($untracked + $ignored)) {
        [void]$paths.Add(([string]$path).Replace('\', '/'))
    }
    return $paths.Count
}

function Get-ProjectIdentity {
    param([Parameter(Mandatory = $true)][string]$Text)

    $idMatches = [regex]::Matches(
        $Text,
        '(?m)^\s*ID\s+"(?<value>[^"]+)"')
    $guidMatches = [regex]::Matches(
        $Text,
        '(?m)^\s*GUID\s+"(?<value>[0-9A-Fa-f]{16})"')
    if ($idMatches.Count -ne 1 -or $guidMatches.Count -ne 1) {
        throw 'The source project identity is incomplete.'
    }
    return [pscustomobject][ordered]@{
        id = $idMatches[0].Groups['value'].Value
        guid = $guidMatches[0].Groups['value'].Value.ToUpperInvariant()
    }
}

function Get-ExactSourceSchema {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$ClassName,
        [Parameter(Mandatory = $true)][int]$Expected
    )

    $pattern = 'class\s+' + [regex]::Escape($ClassName) +
        '\s*\{[\s\S]*?static\s+const\s+int\s+SCHEMA_VERSION\s*=\s*' +
        '(?<value>\d+)\s*;'
    $matches = [regex]::Matches(
        $Text,
        $pattern,
        [Text.RegularExpressions.RegexOptions]::CultureInvariant)
    if ($matches.Count -ne 1 -or
        [int]$matches[0].Groups['value'].Value -ne $Expected) {
        throw "The $ClassName schema is not exactly $Expected."
    }
    return [int]$matches[0].Groups['value'].Value
}

function Get-ExactBuildIdentity {
    param([Parameter(Mandatory = $true)][string]$Text)

    $definitions = [ordered]@{
        sha = 'static\s+const\s+string\s+BUILD_SHA\s*=\s*"(?<value>[0-9a-f]{40})"\s*;'
        utc = 'static\s+const\s+string\s+BUILD_UTC\s*=\s*"(?<value>[^"\r\n]+)"\s*;'
        label = 'static\s+const\s+string\s+BUILD_LABEL\s*=\s*"(?<value>[^"\r\n]+)"\s*;'
    }
    $values = [ordered]@{}
    foreach ($name in $definitions.Keys) {
        $matches = [regex]::Matches(
            $Text,
            [string]$definitions[$name],
            [Text.RegularExpressions.RegexOptions]::CultureInvariant)
        if ($matches.Count -ne 1 -or
            [string]::IsNullOrWhiteSpace($matches[0].Groups['value'].Value)) {
            throw "The embedded build $name identity is not exact."
        }
        $values[$name] = $matches[0].Groups['value'].Value
    }
    $parsedUtc = [DateTime]::MinValue
    if (-not [DateTime]::TryParseExact(
            [string]$values.utc,
            'yyyy-MM-ddTHH:mm:ssZ',
            [Globalization.CultureInfo]::InvariantCulture,
            [Globalization.DateTimeStyles]::AssumeUniversal,
            [ref]$parsedUtc)) {
        throw 'The embedded build UTC is not canonical.'
    }
    return [pscustomobject][ordered]@{
        sha = [string]$values.sha
        utc = [string]$values.utc
        label = [string]$values.label
    }
}

function Get-SourceContractIdentity {
    param(
        [Parameter(Mandatory = $true)][string]$CheckoutRoot,
        [Parameter(Mandatory = $true)][string]$Head,
        [Parameter(Mandatory = $true)][string]$GitExecutable
    )

    $buildText = Get-GitBlobText `
        -CheckoutRoot $CheckoutRoot `
        -Head $Head `
        -RelativePath 'Scripts/Game/HST/HST_BuildInfo.c' `
        -GitExecutable $GitExecutable
    $campaignText = Get-GitBlobText `
        -CheckoutRoot $CheckoutRoot `
        -Head $Head `
        -RelativePath 'Scripts/Game/HST/State/HST_CampaignState.c' `
        -GitExecutable $GitExecutable
    $settingsText = Get-GitBlobText `
        -CheckoutRoot $CheckoutRoot `
        -Head $Head `
        -RelativePath 'Scripts/Game/HST/Config/HST_RuntimeSettings.c' `
        -GitExecutable $GitExecutable
    $build = Get-ExactBuildIdentity -Text $buildText
    & git -C $CheckoutRoot merge-base --is-ancestor $build.sha $Head 2>$null
    if ($LASTEXITCODE -ne 0) {
        throw 'The embedded build SHA is not an ancestor of the source checkpoint.'
    }
    return [pscustomobject][ordered]@{
        campaignSchema = Get-ExactSourceSchema `
            -Text $campaignText `
            -ClassName 'HST_CampaignState' `
            -Expected 71
        runtimeSettingsSchema = Get-ExactSourceSchema `
            -Text $settingsText `
            -ClassName 'HST_RuntimeSettings' `
            -Expected 24
        build = $build
    }
}

function Get-RepositoryBinding {
    param(
        [Parameter(Mandatory = $true)][string]$CheckoutRoot,
        [Parameter(Mandatory = $true)][string]$RunnerPath,
        [Parameter(Mandatory = $true)][string[]]$HelperPaths,
        [AllowEmptyString()][string]$RequestedSourceGitHead
    )

    $gitCommand = Get-Command git -ErrorAction Stop
    $gitExecutable = [IO.Path]::GetFullPath($gitCommand.Source)
    $status = @(Invoke-GitLines `
        -CheckoutRoot $CheckoutRoot `
        -Arguments @('status', '--porcelain=v1', '--untracked-files=all'))
    if ($status.Count -ne 0) {
        throw 'Source Gate 1 requires a clean, committed checkout and harness.'
    }
    [void](Invoke-GitLines `
        -CheckoutRoot $CheckoutRoot `
        -Arguments @('diff', '--check'))

    $harnessHead = @(Invoke-GitLines `
        -CheckoutRoot $CheckoutRoot `
        -Arguments @('rev-parse', 'HEAD'))[0].Trim()
    $harnessTree = @(Invoke-GitLines `
        -CheckoutRoot $CheckoutRoot `
        -Arguments @('rev-parse', 'HEAD^{tree}'))[0].Trim()
    $branch = @(Invoke-GitLines `
        -CheckoutRoot $CheckoutRoot `
        -Arguments @('branch', '--show-current'))[0].Trim()
    if ($harnessHead -cnotmatch '^[0-9a-f]{40}$' -or
        $harnessTree -cnotmatch '^[0-9a-f]{40}$' -or
        [string]::IsNullOrWhiteSpace($branch)) {
        throw 'The clean harness Git identity is invalid.'
    }

    $sourceHead = $harnessHead
    if (-not [string]::IsNullOrWhiteSpace($RequestedSourceGitHead)) {
        if ($RequestedSourceGitHead -cnotmatch '^[0-9a-f]{40}$') {
            throw 'SourceGitHead must be a lowercase full Git SHA.'
        }
        $resolvedSource = @(Invoke-GitLines `
            -CheckoutRoot $CheckoutRoot `
            -Arguments @('rev-parse', "$RequestedSourceGitHead^{commit}"))
        if ($resolvedSource.Count -ne 1 -or
            $resolvedSource[0] -cne $RequestedSourceGitHead) {
            throw 'SourceGitHead did not resolve to its exact requested commit.'
        }
        $sourceHead = $RequestedSourceGitHead
    }
    & git -C $CheckoutRoot merge-base --is-ancestor $sourceHead $harnessHead 2>$null
    if ($LASTEXITCODE -ne 0) {
        throw 'The frozen source checkpoint is not an ancestor of harness HEAD.'
    }

    $sourceTree = @(Invoke-GitLines `
        -CheckoutRoot $CheckoutRoot `
        -Arguments @('rev-parse', "$sourceHead^{tree}"))[0].Trim()
    $sourcePublish = Get-PublishInputBinding `
        -CheckoutRoot $CheckoutRoot `
        -Head $sourceHead
    $harnessPublish = Get-PublishInputBinding `
        -CheckoutRoot $CheckoutRoot `
        -Head $harnessHead
    if ($sourcePublish.policy -cne $harnessPublish.policy -or
        $sourcePublish.rowCount -ne $harnessPublish.rowCount -or
        $sourcePublish.sha256 -cne $harnessPublish.sha256 -or
        $sourcePublish.canonicalText -cne $harnessPublish.canonicalText) {
        throw 'Harness HEAD changed the frozen checkpoint publish-input tree.'
    }

    $archiveCensus = Get-CheckoutArchiveCensus `
        -CheckoutRoot $CheckoutRoot `
        -SourceHead $sourceHead `
        -HarnessHead $harnessHead `
        -GitExecutable $gitExecutable
    if (-not (Test-ArchiveCensusZero -Census $archiveCensus)) {
        throw ('The source checkpoint, harness, or checkout contains a ' +
            'tracked, untracked, or ignored generated archive.')
    }
    $extraPublishWorktreePathCount = Get-ExtraPublishWorktreePathCount `
        -CheckoutRoot $CheckoutRoot `
        -GitExecutable $gitExecutable
    if ($extraPublishWorktreePathCount -ne 0) {
        throw ('The checkout contains an untracked or ignored file under ' +
            'a canonical publish-input scope.')
    }

    $publishWorktreeVerifiedCount = 0
    foreach ($publishFile in @($sourcePublish.files)) {
        [void](Assert-WorktreeCleanBlobMatches `
            -CheckoutRoot $CheckoutRoot `
            -RelativePath ([string]$publishFile.path) `
            -ExpectedOid ([string]$publishFile.oid))
        $publishWorktreeVerifiedCount++
    }
    if ($publishWorktreeVerifiedCount -ne $sourcePublish.rowCount) {
        throw 'The publish-input worktree verification count is incomplete.'
    }

    $runnerRelative = Get-CheckoutRelativePath `
        -CheckoutRoot $CheckoutRoot `
        -Path $RunnerPath
    $runnerIdentity = Get-GitBlobIdentity `
        -CheckoutRoot $CheckoutRoot `
        -Head $harnessHead `
        -RelativePath $runnerRelative `
        -GitExecutable $gitExecutable
    $runnerCleanOid = Assert-WorktreeCleanBlobMatches `
        -CheckoutRoot $CheckoutRoot `
        -RelativePath $runnerRelative `
        -ExpectedOid $runnerIdentity.gitBlobOid
    $runnerIdentity | Add-Member `
        -NotePropertyName worktreeCleanBlobOid `
        -NotePropertyValue $runnerCleanOid
    $helperIdentities = New-Object Collections.Generic.List[object]
    foreach ($helperPath in $HelperPaths) {
        $helperRelative = Get-CheckoutRelativePath `
            -CheckoutRoot $CheckoutRoot `
            -Path $helperPath
        $helperIdentity = Get-GitBlobIdentity `
            -CheckoutRoot $CheckoutRoot `
            -Head $harnessHead `
            -RelativePath $helperRelative `
            -GitExecutable $gitExecutable
        $helperCleanOid = Assert-WorktreeCleanBlobMatches `
            -CheckoutRoot $CheckoutRoot `
            -RelativePath $helperRelative `
            -ExpectedOid $helperIdentity.gitBlobOid
        $helperIdentity | Add-Member `
            -NotePropertyName worktreeCleanBlobOid `
            -NotePropertyValue $helperCleanOid
        $helperIdentities.Add($helperIdentity)
    }
    $projectBlob = Get-GitBlobIdentity `
        -CheckoutRoot $CheckoutRoot `
        -Head $sourceHead `
        -RelativePath 'addon.gproj' `
        -GitExecutable $gitExecutable
    $projectText = Get-GitBlobText `
        -CheckoutRoot $CheckoutRoot `
        -Head $sourceHead `
        -RelativePath 'addon.gproj' `
        -GitExecutable $gitExecutable
    $sourceContract = Get-SourceContractIdentity `
        -CheckoutRoot $CheckoutRoot `
        -Head $sourceHead `
        -GitExecutable $gitExecutable

    return [pscustomobject][ordered]@{
        sourceHead = $sourceHead
        sourceTree = $sourceTree
        harnessHead = $harnessHead
        harnessTree = $harnessTree
        branch = $branch
        publishInput = $sourcePublish
        publishWorktreeVerifiedCount = $publishWorktreeVerifiedCount
        extraPublishWorktreePathCount = $extraPublishWorktreePathCount
        archiveCensus = $archiveCensus
        runner = $runnerIdentity
        helpers = $helperIdentities.ToArray()
        project = Get-ProjectIdentity -Text $projectText
        projectBlob = $projectBlob
        sourceContract = $sourceContract
        gitExecutable = $gitExecutable
    }
}

function Test-RepositoryBindingEqual {
    param(
        [Parameter(Mandatory = $true)]$Expected,
        [Parameter(Mandatory = $true)]$Actual
    )

    if ($Expected.sourceHead -cne $Actual.sourceHead -or
        $Expected.sourceTree -cne $Actual.sourceTree -or
        $Expected.harnessHead -cne $Actual.harnessHead -or
        $Expected.harnessTree -cne $Actual.harnessTree -or
        $Expected.branch -cne $Actual.branch -or
        $Expected.publishInput.policy -cne $Actual.publishInput.policy -or
        $Expected.publishInput.rowCount -ne $Actual.publishInput.rowCount -or
        $Expected.publishInput.sha256 -cne $Actual.publishInput.sha256 -or
        $Expected.publishInput.canonicalText -cne
            $Actual.publishInput.canonicalText -or
        $Expected.publishWorktreeVerifiedCount -ne
            $Actual.publishWorktreeVerifiedCount -or
        $Expected.extraPublishWorktreePathCount -ne 0 -or
        $Actual.extraPublishWorktreePathCount -ne 0 -or
        -not (Test-ArchiveCensusZero -Census $Expected.archiveCensus) -or
        -not (Test-ArchiveCensusZero -Census $Actual.archiveCensus) -or
        $Expected.runner.path -cne $Actual.runner.path -or
        $Expected.runner.gitBlobOid -cne $Actual.runner.gitBlobOid -or
        $Expected.runner.worktreeCleanBlobOid -cne
            $Actual.runner.worktreeCleanBlobOid -or
        $Expected.runner.sha256 -cne $Actual.runner.sha256 -or
        $Expected.project.guid -cne $Actual.project.guid -or
        $Expected.sourceContract.campaignSchema -ne
            $Actual.sourceContract.campaignSchema -or
        $Expected.sourceContract.runtimeSettingsSchema -ne
            $Actual.sourceContract.runtimeSettingsSchema -or
        $Expected.sourceContract.build.sha -cne
            $Actual.sourceContract.build.sha -or
        $Expected.sourceContract.build.utc -cne
            $Actual.sourceContract.build.utc -or
        $Expected.sourceContract.build.label -cne
            $Actual.sourceContract.build.label -or
        $Expected.helpers.Count -ne $Actual.helpers.Count) {
        return $false
    }
    for ($index = 0; $index -lt $Expected.helpers.Count; $index++) {
        if ($Expected.helpers[$index].path -cne $Actual.helpers[$index].path -or
            $Expected.helpers[$index].gitBlobOid -cne
                $Actual.helpers[$index].gitBlobOid -or
            $Expected.helpers[$index].worktreeCleanBlobOid -cne
                $Actual.helpers[$index].worktreeCleanBlobOid -or
            $Expected.helpers[$index].sha256 -cne
                $Actual.helpers[$index].sha256) {
            return $false
        }
    }
    return $true
}

function Get-PathBindingSha256 {
    param([Parameter(Mandatory = $true)][string]$Path)

    $normalized = [IO.Path]::GetFullPath($Path).TrimEnd('\', '/').
        Replace('\', '/').ToLowerInvariant() + "`n"
    return Get-Sha256Text -Text $normalized
}

function Get-PortableFileIdentity {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [AllowEmptyString()][string]$Role = ''
    )

    $resolved = [IO.Path]::GetFullPath($Path)
    if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
        throw 'A required tool identity file is missing.'
    }
    $item = Get-Item -LiteralPath $resolved -Force
    $version = $item.VersionInfo
    return [pscustomobject][ordered]@{
        role = $Role
        name = $item.Name
        pathBindingPolicy = 'normalized-lowercase-utf8-lf-sha256-v1'
        pathSha256 = Get-PathBindingSha256 -Path $resolved
        sha256 = (Get-FileHash -LiteralPath $resolved -Algorithm SHA256).Hash.
            ToLowerInvariant()
        length = $item.Length
        fileVersion = [string]$version.FileVersion
        productVersion = [string]$version.ProductVersion
    }
}

function Get-PortableContentIdentity {
    param([Parameter(Mandatory = $true)][string]$Path)

    $resolved = [IO.Path]::GetFullPath($Path)
    if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
        throw 'A required content-identity file is missing.'
    }
    $item = Get-Item -LiteralPath $resolved -Force
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or
        $item.Length -le 0) {
        throw 'A required content-identity file is empty or a reparse point.'
    }
    return [pscustomobject][ordered]@{
        sha256 = (Get-FileHash -LiteralPath $resolved -Algorithm SHA256).Hash.
            ToLowerInvariant()
        length = [long]$item.Length
    }
}

function Test-PortableContentIdentityEqual {
    param(
        [AllowNull()]$First,
        [AllowNull()]$Second
    )

    return $null -ne $First -and $null -ne $Second -and
        [string]$First.sha256 -ceq [string]$Second.sha256 -and
        [long]$First.length -eq [long]$Second.length
}

function Test-PathContained {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Candidate,
        [switch]$AllowEqual
    )

    $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    $candidateFull = [IO.Path]::GetFullPath($Candidate).TrimEnd('\', '/')
    if ($AllowEqual -and
        $candidateFull.Equals($rootFull, [StringComparison]::OrdinalIgnoreCase)) {
        return $true
    }
    $prefix = $rootFull + [IO.Path]::DirectorySeparatorChar
    return $candidateFull.StartsWith(
        $prefix, [StringComparison]::OrdinalIgnoreCase)
}

function Test-PathOverlap {
    param(
        [Parameter(Mandatory = $true)][string]$First,
        [Parameter(Mandatory = $true)][string]$Second
    )

    return (Test-PathContained -Root $First -Candidate $Second -AllowEqual) -or
        (Test-PathContained -Root $Second -Candidate $First -AllowEqual)
}

function Assert-NoReparseAncestry {
    param([Parameter(Mandatory = $true)][string]$Path)

    $cursor = [IO.Path]::GetFullPath($Path)
    while (-not [string]::IsNullOrWhiteSpace($cursor)) {
        if (Test-Path -LiteralPath $cursor) {
            $item = Get-Item -LiteralPath $cursor -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw 'An evidence path uses a reparse-point ancestor.'
            }
        }
        $parent = Split-Path -Parent $cursor
        if ([string]::IsNullOrWhiteSpace($parent) -or $parent -ceq $cursor) {
            break
        }
        $cursor = $parent
    }
}

function Write-Utf8LfText {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Text
    )

    $normalized = $Text.Replace("`r`n", "`n").Replace("`r", "`n")
    [IO.File]::WriteAllText(
        $Path,
        $normalized,
        (New-Object Text.UTF8Encoding($false)))
}

function Write-PortableJson {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)]$Value
    )

    $json = $Value | ConvertTo-Json -Depth 32
    Write-Utf8LfText -Path $Path -Text ($json + "`n")
}

function Get-ArtifactInventory {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [string[]]$ExcludedRelativePaths = @('summary.json')
    )

    $resolvedRoot = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    $excluded = New-Object 'Collections.Generic.HashSet[string]' `
        ([StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in $ExcludedRelativePaths) {
        [void]$excluded.Add($entry.Replace('\', '/'))
    }
    $files = New-Object Collections.Generic.List[object]
    $pending = New-Object 'Collections.Generic.Stack[string]'
    $pending.Push($resolvedRoot)
    while ($pending.Count -gt 0) {
        $directoryPath = $pending.Pop()
        $directory = Get-Item -LiteralPath $directoryPath -Force
        if (($directory.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw 'Artifact inventory refuses a reparse-point directory.'
        }
        foreach ($child in @(Get-ChildItem `
                -LiteralPath $directoryPath `
                -Force `
                -ErrorAction Stop)) {
            if (($child.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw 'Artifact inventory refuses a reparse-point descendant.'
            }
            if ($child.PSIsContainer) {
                $pending.Push([IO.Path]::GetFullPath($child.FullName))
                continue
            }
            $relative = $child.FullName.Substring($resolvedRoot.Length).
                TrimStart('\', '/').Replace('\', '/')
            if ([string]::IsNullOrWhiteSpace($relative) -or
                $relative -match '(^|/)\.\.(/|$)') {
                throw 'Artifact inventory produced an invalid relative path.'
            }
            if ($excluded.Contains($relative)) {
                continue
            }
            $files.Add([pscustomobject][ordered]@{
                path = $relative
                length = [long]$child.Length
                sha256 = (Get-FileHash `
                    -LiteralPath $child.FullName `
                    -Algorithm SHA256).Hash.ToLowerInvariant()
            })
        }
    }
    $fileByPath = @{}
    [string[]]$orderedPaths = @($files.ToArray() | ForEach-Object {
        $fileByPath[[string]$_.path] = $_
        [string]$_.path
    })
    [Array]::Sort($orderedPaths, [StringComparer]::Ordinal)
    $ordered = @($orderedPaths | ForEach-Object { $fileByPath[$_] })
    $canonical = (@($ordered | ForEach-Object {
        $_.path + "`t" + $_.length + "`t" + $_.sha256
    }) -join "`n")
    if ($ordered.Count -gt 0) {
        $canonical += "`n"
    }
    return [pscustomobject][ordered]@{
        hashAlgorithm = 'sha256-file-set-v1'
        artifactCount = $ordered.Count
        artifactSetSha256 = Get-Sha256Text -Text $canonical
        files = $ordered
    }
}

function Assert-NoAbsolutePaths {
    param([Parameter(Mandatory = $true)]$Value)

    if ($null -eq $Value) {
        return
    }
    if ($Value -is [string]) {
        if ([IO.Path]::IsPathRooted([string]$Value) -or
            [string]$Value -match '(?i)[a-z]:[\\/]' -or
            [string]$Value -match '\\\\[^\\/]+[\\/]') {
            throw 'A portable summary contains an absolute path.'
        }
        return
    }
    if ($Value -is [Collections.IDictionary]) {
        foreach ($key in $Value.Keys) {
            Assert-NoAbsolutePaths -Value $Value[$key]
        }
        return
    }
    if ($Value -is [Collections.IEnumerable] -and
        $Value -isnot [Management.Automation.PSCustomObject]) {
        foreach ($entry in $Value) {
            Assert-NoAbsolutePaths -Value $entry
        }
        return
    }
    foreach ($property in $Value.PSObject.Properties) {
        Assert-NoAbsolutePaths -Value $property.Value
    }
}

function Get-PortableSourceSummary {
    param([Parameter(Mandatory = $true)]$Binding)

    return [pscustomobject][ordered]@{
        sourceGitHead = $Binding.sourceHead
        publishInputPolicy = $Binding.publishInput.policy
        publishInputTreeSha256 = $Binding.publishInput.sha256
        publishInputRowCount = $Binding.publishInput.rowCount
        addonGuid = $Binding.project.guid
        campaignSchema = $Binding.sourceContract.campaignSchema
        runtimeSettingsSchema = $Binding.sourceContract.runtimeSettingsSchema
        embeddedImplementationSha = $Binding.sourceContract.build.sha
        embeddedBuildUtc = $Binding.sourceContract.build.utc
        embeddedBuildLabel = $Binding.sourceContract.build.label
    }
}

function Get-PortableHarnessSummary {
    param([Parameter(Mandatory = $true)]$Binding)

    return [pscustomobject][ordered]@{
        gitHead = $Binding.harnessHead
        dirty = $false
        runnerPath = $Binding.runner.path
        runnerHashPolicy = $script:HarnessHashPolicy
        runnerSha256 = $Binding.runner.sha256
    }
}

function Get-CurrentPowerShellExecutable {
    $process = Get-Process -Id $PID -ErrorAction Stop
    try {
        return [IO.Path]::GetFullPath($process.MainModule.FileName)
    }
    finally {
        $process.Dispose()
    }
}

function Invoke-ChildPowerShell {
    param(
        [Parameter(Mandatory = $true)][string]$PowerShellExecutable,
        [Parameter(Mandatory = $true)][hashtable]$Payload,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory
    )

    $childSource = @'
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
[Console]::OutputEncoding = New-Object Text.UTF8Encoding($false)
try {
    $payloadText = [Environment]::GetEnvironmentVariable('PARTISAN_SOURCE_GATE_PAYLOAD')
    if ([string]::IsNullOrWhiteSpace($payloadText)) {
        throw 'The source-gate child payload is missing.'
    }
    $payload = $payloadText | ConvertFrom-Json
    Set-Location ([string]$payload.checkoutRoot)
    if ([string]$payload.kind -ceq 'foundation') {
        & ([string]$payload.scriptPath)
    }
    elseif ([string]$payload.kind -ceq 'workbench') {
        $parameters = @{
            Executable = [string]$payload.executable
            ProjectPath = [string]$payload.projectPath
            AddonRoots = @($payload.addonRoots)
            Target = [string]$payload.target
            EvidenceDirectory = [string]$payload.evidenceDirectory
            DefaultLogRoots = @($payload.defaultLogRoots)
            SpillRoots = @($payload.spillRoots)
            TimeoutSeconds = [int]$payload.timeoutSeconds
            PollMilliseconds = [int]$payload.pollMilliseconds
            ReturnToCaller = $true
        }
        & ([string]$payload.scriptPath) @parameters
    }
    else {
        throw 'The source-gate child kind is invalid.'
    }
    exit 0
}
catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}
'@
    $encoded = [Convert]::ToBase64String(
        [Text.Encoding]::Unicode.GetBytes($childSource))
    $startInfo = New-Object Diagnostics.ProcessStartInfo
    $startInfo.FileName = $PowerShellExecutable
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.Arguments = @(
        '-NoLogo',
        '-NoProfile',
        '-NonInteractive',
        '-ExecutionPolicy', 'Bypass',
        '-EncodedCommand', $encoded
    ) | ForEach-Object {
        ConvertTo-NativeArgument -Value ([string]$_)
    }
    $startInfo.Arguments = $startInfo.Arguments -join ' '
    $startInfo.EnvironmentVariables['PARTISAN_SOURCE_GATE_PAYLOAD'] =
        ($Payload | ConvertTo-Json -Depth 8 -Compress)

    $process = New-Object Diagnostics.Process
    $process.StartInfo = $startInfo
    $stdout = New-Object IO.MemoryStream
    $stderr = New-Object IO.MemoryStream
    $startedUtc = [DateTime]::UtcNow
    try {
        if (-not $process.Start()) {
            throw 'The source-gate child process did not start.'
        }
        $stdoutTask = $process.StandardOutput.BaseStream.CopyToAsync($stdout)
        $stderrTask = $process.StandardError.BaseStream.CopyToAsync($stderr)
        $process.WaitForExit()
        [void]$stdoutTask.GetAwaiter().GetResult()
        [void]$stderrTask.GetAwaiter().GetResult()
        $completedUtc = [DateTime]::UtcNow
        [byte[]]$stdoutBytes = $stdout.ToArray()
        [byte[]]$stderrBytes = $stderr.ToArray()
        $utf8 = New-Object Text.UTF8Encoding($false)
        return [pscustomobject][ordered]@{
            startedUtc = $startedUtc
            completedUtc = $completedUtc
            exitCode = $process.ExitCode
            stdoutBytes = $stdoutBytes
            stderrBytes = $stderrBytes
            stdoutText = $utf8.GetString($stdoutBytes)
            stderrText = $utf8.GetString($stderrBytes)
        }
    }
    finally {
        $stdout.Dispose()
        $stderr.Dispose()
        $process.Dispose()
    }
}

function Write-ChildCapture {
    param(
        [Parameter(Mandatory = $true)]$Capture,
        [Parameter(Mandatory = $true)][string]$Directory,
        [Parameter(Mandatory = $true)][string]$Kind,
        [AllowEmptyString()][string]$Target = ''
    )

    [IO.File]::WriteAllBytes(
        (Join-Path $Directory 'stdout.log'),
        [byte[]]$Capture.stdoutBytes)
    [IO.File]::WriteAllBytes(
        (Join-Path $Directory 'stderr.log'),
        [byte[]]$Capture.stderrBytes)
    Write-PortableJson `
        -Path (Join-Path $Directory 'invocation.json') `
        -Value ([pscustomobject][ordered]@{
            schemaVersion = 1
            kind = $Kind
            target = $Target
            startedUtc = $Capture.startedUtc.ToString(
                'o', [Globalization.CultureInfo]::InvariantCulture)
            completedUtc = $Capture.completedUtc.ToString(
                'o', [Globalization.CultureInfo]::InvariantCulture)
            exitCode = $Capture.exitCode
        })
}

function Read-TaggedJsonRecord {
    param(
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Text,
        [Parameter(Mandatory = $true)][string]$Tag
    )

    $prefix = $Tag + ' '
    $lines = @($Text.Replace("`r`n", "`n").Replace("`r", "`n").
        Split("`n") | Where-Object { $_.StartsWith($prefix) })
    if ($lines.Count -ne 1) {
        return $null
    }
    try {
        return $lines[0].Substring($prefix.Length) | ConvertFrom-Json
    }
    catch {
        return $null
    }
}

function Get-FoundationConsoleContract {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()][string]$Text
    )

    $completionMatches = [regex]::Matches(
        $Text,
        '(?m)^Partisan foundation validation passed\s*$')
    $referenceMatches = [regex]::Matches(
        $Text,
        '(?m)^Script symbol references OK:\s*(?<count>[0-9]+)\s*$')
    $referenceCount = 0
    if ($referenceMatches.Count -eq 1) {
        $referenceCount = [int]$referenceMatches[0].Groups['count'].Value
    }
    return [pscustomobject][ordered]@{
        valid = $completionMatches.Count -eq 1 -and
            $referenceMatches.Count -eq 1 -and
            $referenceCount -gt 0
        completionMarkerCount = $completionMatches.Count
        referenceMarkerCount = $referenceMatches.Count
        referenceCount = $referenceCount
    }
}

function Test-WorkbenchCleanupZero {
    param($Cleanup)

    if ($null -eq $Cleanup) {
        return $false
    }
    foreach ($name in @(
        'GuardRemaining',
        'GuardBaseRemaining',
        'OwnedGuardRootsRemaining',
        'OwnedProcessesRemaining',
        'NewEngineProcessesRemaining',
        'UnclaimedEngineProcessesObserved',
        'NewDefaultEntriesRemaining',
        'ModifiedDefaultFiles',
        'DeletedDefaultEntries',
        'MissingDefaultRoots',
        'ExternalSpillEntriesRemaining',
        'ModifiedSpillFiles',
        'DeletedSpillEntries',
        'MissingSpillRoots',
        'CleanupPhaseErrorCount')) {
        if ($Cleanup.PSObject.Properties.Name -cnotcontains $name -or
            [int]$Cleanup.$name -ne 0) {
            return $false
        }
    }
    return $true
}

function Get-WorkbenchAggregateContract {
    param([Parameter(Mandatory = $true)][object[]]$TargetRows)

    $exactTargets = $TargetRows.Count -eq $script:RequiredWorkbenchTargets.Count
    if ($exactTargets) {
        for ($index = 0;
            $index -lt $script:RequiredWorkbenchTargets.Count;
            $index++) {
            if ([string]$TargetRows[$index].target -cne
                    $script:RequiredWorkbenchTargets[$index]) {
                $exactTargets = $false
                break
            }
        }
    }
    $crcs = @($TargetRows | ForEach-Object { [string]$_.crc } |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        Sort-Object -Unique -CaseSensitive)
    $passedTargets = @($TargetRows | Where-Object { [bool]$_.success }).Count
    $positiveCounts = @($TargetRows | Where-Object {
        [int]$_.files -gt 0 -and [int]$_.classes -gt 0
    }).Count -eq $script:RequiredWorkbenchTargets.Count
    $hardErrorTotal = 0
    foreach ($row in $TargetRows) {
        if ([int]$row.hardErrorCount -lt 0) {
            $hardErrorTotal = -1
            break
        }
        $hardErrorTotal += [int]$row.hardErrorCount
    }
    $cleanupZeroCount = @($TargetRows |
        Where-Object { [bool]$_.cleanupAndSpillZero }).Count
    $pakZeroCount = @($TargetRows |
        Where-Object { [bool]$_.pakCensusZero }).Count
    $resourceDatabaseStableCount = @($TargetRows |
        Where-Object { [bool]$_.sourceResourceDatabaseStable }).Count
    return [pscustomobject][ordered]@{
        valid = $exactTargets -and
            $passedTargets -eq $script:RequiredWorkbenchTargets.Count -and
            $crcs.Count -eq 1 -and
            $positiveCounts -and
            $hardErrorTotal -eq 0 -and
            $cleanupZeroCount -eq $script:RequiredWorkbenchTargets.Count -and
            $pakZeroCount -eq $script:RequiredWorkbenchTargets.Count -and
            $resourceDatabaseStableCount -eq
                $script:RequiredWorkbenchTargets.Count
        exactRequiredTargets = $exactTargets
        commonCrc = if ($crcs.Count -eq 1) { $crcs[0] } else { '' }
        passedTargets = $passedTargets
        positiveFileAndClassCounts = $positiveCounts
        hardErrorCount = $hardErrorTotal
        cleanupAndSpillZeroTargets = $cleanupZeroCount
        pakCensusZeroTargets = $pakZeroCount
        sourceResourceDatabaseStableTargets = $resourceDatabaseStableCount
    }
}

function Invoke-FoundationGate {
    param(
        [Parameter(Mandatory = $true)][string]$CheckoutRoot,
        [Parameter(Mandatory = $true)][string]$GateRoot,
        [Parameter(Mandatory = $true)][string]$RunId,
        [Parameter(Mandatory = $true)]$Binding,
        [Parameter(Mandatory = $true)][string]$RunnerPath,
        [Parameter(Mandatory = $true)][string]$HelperPath,
        [Parameter(Mandatory = $true)][string[]]$HarnessHelperPaths,
        [Parameter(Mandatory = $true)][string]$PowerShellExecutable,
        [AllowEmptyString()][string]$RequestedSourceGitHead
    )

    $capture = Invoke-ChildPowerShell `
        -PowerShellExecutable $PowerShellExecutable `
        -WorkingDirectory $CheckoutRoot `
        -Payload @{
            kind = 'foundation'
            checkoutRoot = $CheckoutRoot
            scriptPath = $HelperPath
        }
    Write-ChildCapture `
        -Capture $capture `
        -Directory $GateRoot `
        -Kind 'foundation'

    $consoleContract = Get-FoundationConsoleContract -Text $capture.stdoutText
    $completionMarker = $consoleContract.completionMarkerCount -eq 1
    $referenceCount = [int]$consoleContract.referenceCount
    $bindingStable = $false
    try {
        $postBinding = Get-RepositoryBinding `
            -CheckoutRoot $CheckoutRoot `
            -RunnerPath $RunnerPath `
            -HelperPaths $HarnessHelperPaths `
            -RequestedSourceGitHead $RequestedSourceGitHead
        $bindingStable = Test-RepositoryBindingEqual `
            -Expected $Binding `
            -Actual $postBinding
    }
    catch {
        $bindingStable = $false
    }
    $failures = New-Object Collections.Generic.List[string]
    if ($capture.exitCode -ne 0) { $failures.Add('child-exit-nonzero') }
    if ($consoleContract.completionMarkerCount -ne 1) {
        $failures.Add('completion-marker-count-not-one')
    }
    if ($consoleContract.referenceMarkerCount -ne 1 -or $referenceCount -le 0) {
        $failures.Add('reference-marker-contract-invalid')
    }
    if (-not (Test-ArchiveCensusZero -Census $Binding.archiveCensus)) {
        $failures.Add('generated-archive-present')
    }
    if (-not $bindingStable) { $failures.Add('source-or-harness-drift') }

    $inventory = Get-ArtifactInventory -Root $GateRoot
    if ($inventory.artifactCount -le 0) {
        $failures.Add('artifact-inventory-empty')
    }
    $passed = $failures.Count -eq 0
    $summary = [pscustomobject][ordered]@{
        schemaVersion = 1
        evidenceKind = 'source-gate1-foundation'
        source = Get-PortableSourceSummary -Binding $Binding
        harness = Get-PortableHarnessSummary -Binding $Binding
        toolchain = [pscustomobject][ordered]@{
            powershell = Get-PortableFileIdentity `
                -Path $PowerShellExecutable `
                -Role 'foundation-host'
            harnessHelpers = @($Binding.helpers)
            sourceWorktree = [pscustomobject][ordered]@{
                cleanFilterPolicy = 'git-hash-object-path-v1'
                verifiedTrackedPublishInputCount =
                    $Binding.publishWorktreeVerifiedCount
                extraUntrackedOrIgnoredPublishInputCount =
                    $Binding.extraPublishWorktreePathCount
                archiveCensus = $Binding.archiveCensus
            }
        }
        capture = [pscustomobject][ordered]@{
            runId = $RunId
            startedUtc = $capture.startedUtc.ToString(
                'o', [Globalization.CultureInfo]::InvariantCulture)
            completedUtc = $capture.completedUtc.ToString(
                'o', [Globalization.CultureInfo]::InvariantCulture)
            rawStdout = 'stdout.log'
            rawStderr = 'stderr.log'
        }
        result = [pscustomobject][ordered]@{
            status = if ($passed) { 'passed' } else { 'failed' }
            success = $passed
            exitCode = $capture.exitCode
            completionMarker = $completionMarker
            referenceCount = $referenceCount
            trackedPakCount = [int]$Binding.archiveCensus.sourceTracked
            workspacePakCount =
                [int]$Binding.archiveCensus.workspaceTrackedUntrackedIgnored
            verifiedTrackedPublishInputCount =
                $Binding.publishWorktreeVerifiedCount
            extraUntrackedOrIgnoredPublishInputCount =
                $Binding.extraPublishWorktreePathCount
            sourceAndHarnessStable = $bindingStable
            failureCodes = $failures.ToArray()
        }
        integrity = $inventory
    }
    Assert-NoAbsolutePaths -Value $summary
    $summaryPath = Join-Path $GateRoot 'summary.json'
    Write-PortableJson -Path $summaryPath -Value $summary
    return [pscustomobject]@{
        summary = $summary
        summaryPath = $summaryPath
        summarySha256 = (Get-FileHash `
            -LiteralPath $summaryPath `
            -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

function Get-AddonRootBindings {
    param([Parameter(Mandatory = $true)][string[]]$Roots)

    $result = New-Object Collections.Generic.List[object]
    $ordinal = 0
    foreach ($root in $Roots) {
        $ordinal++
        $result.Add([pscustomobject][ordered]@{
            role = 'installed-dependency-addon-root'
            ordinal = $ordinal
            pathBindingPolicy = 'normalized-lowercase-utf8-lf-sha256-v1'
            pathSha256 = Get-PathBindingSha256 -Path $root
        })
    }
    return $result.ToArray()
}

function Invoke-WorkbenchGate {
    param(
        [Parameter(Mandatory = $true)][string]$CheckoutRoot,
        [Parameter(Mandatory = $true)][string]$GateRoot,
        [Parameter(Mandatory = $true)][string]$RunId,
        [Parameter(Mandatory = $true)]$Binding,
        [Parameter(Mandatory = $true)][string]$RunnerPath,
        [Parameter(Mandatory = $true)][string]$HelperPath,
        [Parameter(Mandatory = $true)][string[]]$HarnessHelperPaths,
        [Parameter(Mandatory = $true)][string]$PowerShellExecutable,
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string[]]$ResolvedAddonRoots,
        [Parameter(Mandatory = $true)][string[]]$ResolvedDefaultLogRoots,
        [Parameter(Mandatory = $true)][string[]]$ResolvedSpillRoots,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds,
        [Parameter(Mandatory = $true)][int]$PollMilliseconds,
        [AllowEmptyString()][string]$RequestedSourceGitHead
    )

    $projectPath = Join-Path $CheckoutRoot 'addon.gproj'
    $workbenchBefore = Get-PortableFileIdentity `
        -Path $Executable `
        -Role 'diagnostic-workbench'
    $targetRows = New-Object Collections.Generic.List[object]
    $resourceDatabasePath = Join-Path $CheckoutRoot 'resourceDatabase.rdb'
    $resourceDatabaseBaseline = $null
    $targetOrdinal = 0
    $gateStartedUtc = [DateTime]::UtcNow
    foreach ($target in $script:RequiredWorkbenchTargets) {
        $targetRoot = Join-Path $GateRoot $target
        $rawEvidence = Join-Path $targetRoot 'raw-evidence'
        New-Item -ItemType Directory -Path $rawEvidence -Force | Out-Null
        $capture = Invoke-ChildPowerShell `
            -PowerShellExecutable $PowerShellExecutable `
            -WorkingDirectory $CheckoutRoot `
            -Payload @{
                kind = 'workbench'
                checkoutRoot = $CheckoutRoot
                scriptPath = $HelperPath
                executable = $Executable
                projectPath = $projectPath
                addonRoots = @($ResolvedAddonRoots)
                target = $target
                evidenceDirectory = $rawEvidence
                defaultLogRoots = @($ResolvedDefaultLogRoots)
                spillRoots = @($ResolvedSpillRoots)
                timeoutSeconds = $TimeoutSeconds
                pollMilliseconds = $PollMilliseconds
            }
        Write-ChildCapture `
            -Capture $capture `
            -Directory $targetRoot `
            -Kind 'workbench' `
            -Target $target
        $result = Read-TaggedJsonRecord -Text $capture.stdoutText -Tag 'RESULT'
        $cleanup = Read-TaggedJsonRecord -Text $capture.stdoutText -Tag 'CLEANUP'
        $cleanupZero = Test-WorkbenchCleanupZero -Cleanup $cleanup
        $resourceDatabaseIdentity = $null
        try {
            $resourceDatabaseIdentity = Get-PortableContentIdentity `
                -Path $resourceDatabasePath
        }
        catch {
            $resourceDatabaseIdentity = $null
        }
        $resourceDatabaseStable = $false
        if ($targetOrdinal -eq 0) {
            if ($null -ne $resourceDatabaseIdentity) {
                $resourceDatabaseBaseline = $resourceDatabaseIdentity
                $resourceDatabaseStable = $true
            }
        }
        else {
            $resourceDatabaseStable = Test-PortableContentIdentityEqual `
                -First $resourceDatabaseBaseline `
                -Second $resourceDatabaseIdentity
        }
        $pakCensusZero = $false
        try {
            $targetArchiveCensus = Get-CheckoutArchiveCensus `
                -CheckoutRoot $CheckoutRoot `
                -SourceHead $Binding.sourceHead `
                -HarnessHead $Binding.harnessHead `
                -GitExecutable $Binding.gitExecutable
            $pakCensusZero = Test-ArchiveCensusZero `
                -Census $targetArchiveCensus
        }
        catch {
            $pakCensusZero = $false
        }
        $recordValid = $null -ne $result -and
            $result.PSObject.Properties.Name -ccontains 'Target' -and
            $result.PSObject.Properties.Name -ccontains 'Success' -and
            $result.PSObject.Properties.Name -ccontains 'Files' -and
            $result.PSObject.Properties.Name -ccontains 'Classes' -and
            $result.PSObject.Properties.Name -ccontains 'Crc' -and
            $result.PSObject.Properties.Name -ccontains 'HardErrorCount'
        $targetSuccess = $recordValid -and
            $capture.exitCode -eq 0 -and
            [string]$result.Target -ceq $target -and
            [bool]$result.Success -and
            [int]$result.Files -gt 0 -and
            [int]$result.Classes -gt 0 -and
            [string]$result.Crc -cmatch '^[0-9a-f]{8}$' -and
            [int]$result.HardErrorCount -eq 0 -and
            $cleanupZero -and
            $pakCensusZero -and
            $resourceDatabaseStable
        $targetRows.Add([pscustomobject][ordered]@{
            target = $target
            success = [bool]$targetSuccess
            exitCode = $capture.exitCode
            files = if ($recordValid) { [int]$result.Files } else { 0 }
            classes = if ($recordValid) { [int]$result.Classes } else { 0 }
            crc = if ($recordValid) { [string]$result.Crc } else { '' }
            hardErrorCount = if ($recordValid) {
                [int]$result.HardErrorCount
            }
            else { -1 }
            cleanupAndSpillZero = $cleanupZero
            pakCensusZero = $pakCensusZero
            sourceResourceDatabase = $resourceDatabaseIdentity
            sourceResourceDatabaseStable = $resourceDatabaseStable
            startedUtc = $capture.startedUtc.ToString(
                'o', [Globalization.CultureInfo]::InvariantCulture)
            completedUtc = $capture.completedUtc.ToString(
                'o', [Globalization.CultureInfo]::InvariantCulture)
            rawStdout = "$target/stdout.log"
            rawStderr = "$target/stderr.log"
            rawEvidence = "$target/raw-evidence"
        })
        $targetOrdinal++
    }
    $gateCompletedUtc = [DateTime]::UtcNow

    $bindingStable = $false
    try {
        $postBinding = Get-RepositoryBinding `
            -CheckoutRoot $CheckoutRoot `
            -RunnerPath $RunnerPath `
            -HelperPaths $HarnessHelperPaths `
            -RequestedSourceGitHead $RequestedSourceGitHead
        $bindingStable = Test-RepositoryBindingEqual `
            -Expected $Binding `
            -Actual $postBinding
    }
    catch {
        $bindingStable = $false
    }
    $workbenchAfter = Get-PortableFileIdentity `
        -Path $Executable `
        -Role 'diagnostic-workbench'
    $toolStable = $workbenchBefore.sha256 -ceq $workbenchAfter.sha256 -and
        $workbenchBefore.length -eq $workbenchAfter.length -and
        $workbenchBefore.pathSha256 -ceq $workbenchAfter.pathSha256

    $finalResourceDatabaseIdentity = $null
    try {
        $finalResourceDatabaseIdentity = Get-PortableContentIdentity `
            -Path $resourceDatabasePath
    }
    catch {
        $finalResourceDatabaseIdentity = $null
    }
    $finalResourceDatabaseStable = Test-PortableContentIdentityEqual `
        -First $resourceDatabaseBaseline `
        -Second $finalResourceDatabaseIdentity
    $aggregate = Get-WorkbenchAggregateContract `
        -TargetRows $targetRows.ToArray()
    $failures = New-Object Collections.Generic.List[string]
    if (-not $aggregate.exactRequiredTargets) {
        $failures.Add('required-target-order-or-membership-invalid')
    }
    if ($aggregate.passedTargets -ne 5) {
        $failures.Add('one-or-more-targets-failed')
    }
    if ([string]::IsNullOrWhiteSpace($aggregate.commonCrc)) {
        $failures.Add('common-crc-missing')
    }
    if (-not $aggregate.positiveFileAndClassCounts) {
        $failures.Add('nonpositive-module-count')
    }
    if ($aggregate.hardErrorCount -ne 0) {
        $failures.Add('hard-error-count-not-zero')
    }
    if ($aggregate.cleanupAndSpillZeroTargets -ne 5) {
        $failures.Add('cleanup-or-spill-not-zero')
    }
    if ($aggregate.pakCensusZeroTargets -ne 5) {
        $failures.Add('generated-archive-observed')
    }
    if ($aggregate.sourceResourceDatabaseStableTargets -ne 5 -or
        -not $finalResourceDatabaseStable) {
        $failures.Add('source-resource-database-missing-empty-or-unstable')
    }
    if (-not $bindingStable) { $failures.Add('source-or-harness-drift') }
    if (-not $toolStable) { $failures.Add('workbench-tool-drift') }

    $inventory = Get-ArtifactInventory -Root $GateRoot
    if ($inventory.artifactCount -le 0) {
        $failures.Add('artifact-inventory-empty')
    }
    $passed = $failures.Count -eq 0
    $summary = [pscustomobject][ordered]@{
        schemaVersion = 1
        evidenceKind = 'source-gate1-workbench-all-targets'
        source = Get-PortableSourceSummary -Binding $Binding
        harness = Get-PortableHarnessSummary -Binding $Binding
        toolchain = [pscustomobject][ordered]@{
            powershell = Get-PortableFileIdentity `
                -Path $PowerShellExecutable `
                -Role 'workbench-wrapper-host'
            workbench = $workbenchBefore
            workbenchStable = $toolStable
            addonRoots = @(Get-AddonRootBindings -Roots $ResolvedAddonRoots)
            harnessHelpers = @($Binding.helpers)
            sourceWorktree = [pscustomobject][ordered]@{
                cleanFilterPolicy = 'git-hash-object-path-v1'
                verifiedTrackedPublishInputCount =
                    $Binding.publishWorktreeVerifiedCount
                extraUntrackedOrIgnoredPublishInputCount =
                    $Binding.extraPublishWorktreePathCount
                archiveCensus = $Binding.archiveCensus
            }
            sourceResourceDatabase = $finalResourceDatabaseIdentity
        }
        capture = [pscustomobject][ordered]@{
            runId = $RunId
            startedUtc = $gateStartedUtc.ToString(
                'o', [Globalization.CultureInfo]::InvariantCulture)
            completedUtc = $gateCompletedUtc.ToString(
                'o', [Globalization.CultureInfo]::InvariantCulture)
            targets = @($targetRows.ToArray())
        }
        result = [pscustomobject][ordered]@{
            status = if ($passed) { 'passed' } else { 'failed' }
            success = $passed
            targetCount = $targetRows.Count
            passedTargets = $aggregate.passedTargets
            exactRequiredTargets = $aggregate.exactRequiredTargets
            commonCrc = $aggregate.commonCrc
            positiveFileAndClassCounts =
                $aggregate.positiveFileAndClassCounts
            hardErrorCount = $aggregate.hardErrorCount
            cleanupAndSpillZeroTargets =
                $aggregate.cleanupAndSpillZeroTargets
            pakCensusZeroTargets = $aggregate.pakCensusZeroTargets
            sourceResourceDatabaseStableTargets =
                $aggregate.sourceResourceDatabaseStableTargets
            sourceResourceDatabaseFinalStable = $finalResourceDatabaseStable
            sourceAndHarnessStable = $bindingStable
            failureCodes = $failures.ToArray()
        }
        integrity = $inventory
    }
    Assert-NoAbsolutePaths -Value $summary
    $summaryPath = Join-Path $GateRoot 'summary.json'
    Write-PortableJson -Path $summaryPath -Value $summary
    return [pscustomobject]@{
        summary = $summary
        summaryPath = $summaryPath
        summarySha256 = (Get-FileHash `
            -LiteralPath $summaryPath `
            -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

function Invoke-SelfTest {
    $checkoutRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
    $head = @(Invoke-GitLines `
        -CheckoutRoot $checkoutRoot `
        -Arguments @('rev-parse', 'HEAD'))[0].Trim()
    $publish = Get-PublishInputBinding `
        -CheckoutRoot $checkoutRoot `
        -Head $head
    if ($publish.policy -cne 'git-ls-tree-sha256-v1' -or
        $publish.rowCount -le 0 -or
        $publish.sha256 -cnotmatch '^[0-9a-f]{64}$') {
        throw 'Source-static publish-input self-test failed.'
    }
    $gitExecutable = [IO.Path]::GetFullPath(
        (Get-Command git -ErrorAction Stop).Source)
    $blobIdentity = Get-GitBlobIdentity `
        -CheckoutRoot $checkoutRoot `
        -Head $head `
        -RelativePath 'addon.gproj' `
        -GitExecutable $gitExecutable
    $cleanBlobOid = Get-WorktreeCleanBlobOid `
        -CheckoutRoot $checkoutRoot `
        -RelativePath 'addon.gproj'
    $archiveCensus = Get-CheckoutArchiveCensus `
        -CheckoutRoot $checkoutRoot `
        -SourceHead $head `
        -HarnessHead $head `
        -GitExecutable $gitExecutable
    $extraPublishCount = Get-ExtraPublishWorktreePathCount `
        -CheckoutRoot $checkoutRoot `
        -GitExecutable $gitExecutable
    $sourceContract = Get-SourceContractIdentity `
        -CheckoutRoot $checkoutRoot `
        -Head $head `
        -GitExecutable $gitExecutable
    if ($blobIdentity.hashPolicy -cne $script:HarnessHashPolicy -or
        $blobIdentity.sha256 -cnotmatch '^[0-9a-f]{64}$' -or
        $blobIdentity.length -le 0 -or
        $cleanBlobOid -cne $blobIdentity.gitBlobOid -or
        -not (Test-ArchiveCensusZero -Census $archiveCensus) -or
        $extraPublishCount -ne 0 -or
        -not (Test-GeneratedArchivePath -Path 'nested/EXAMPLE.PAK') -or
        (Test-GeneratedArchivePath -Path 'nested/example.txt') -or
        $sourceContract.campaignSchema -ne 71 -or
        $sourceContract.runtimeSettingsSchema -ne 24) {
        throw 'Source-static Git-blob or archive-census self-test failed.'
    }

    $tempRoot = Join-Path ([IO.Path]::GetTempPath()) `
        ('PartisanSourceStaticSelfTest_' + [Guid]::NewGuid().ToString('N'))
    try {
        New-Item -ItemType Directory -Path $tempRoot | Out-Null
        Write-Utf8LfText -Path (Join-Path $tempRoot 'alpha.txt') -Text "a`r`n"
        New-Item -ItemType Directory -Path (Join-Path $tempRoot 'nested') |
            Out-Null
        Write-Utf8LfText `
            -Path (Join-Path $tempRoot 'nested\beta.txt') `
            -Text "b`n"
        $inventory = Get-ArtifactInventory -Root $tempRoot
        if ($inventory.artifactCount -ne 2 -or
            $inventory.hashAlgorithm -cne 'sha256-file-set-v1' -or
            $inventory.artifactSetSha256 -cnotmatch '^[0-9a-f]{64}$' -or
            @($inventory.files | Where-Object {
                $_.length -le 0 -or $_.sha256 -cnotmatch '^[0-9a-f]{64}$'
            }).Count -ne 0) {
            throw 'Source-static artifact-inventory self-test failed.'
        }

        $resultRecord = Read-TaggedJsonRecord `
            -Text "RESULT {`"Target`":`"PC`",`"Success`":true}`n" `
            -Tag 'RESULT'
        $cleanupProperties = [ordered]@{}
        foreach ($name in @(
            'GuardRemaining', 'GuardBaseRemaining',
            'OwnedGuardRootsRemaining', 'OwnedProcessesRemaining',
            'NewEngineProcessesRemaining', 'UnclaimedEngineProcessesObserved',
            'NewDefaultEntriesRemaining', 'ModifiedDefaultFiles',
            'DeletedDefaultEntries', 'MissingDefaultRoots',
            'ExternalSpillEntriesRemaining', 'ModifiedSpillFiles',
            'DeletedSpillEntries', 'MissingSpillRoots',
            'CleanupPhaseErrorCount')) {
            $cleanupProperties[$name] = 0
        }
        if ($null -eq $resultRecord -or
            [string]$resultRecord.Target -cne 'PC' -or
            -not (Test-WorkbenchCleanupZero `
                -Cleanup ([pscustomobject]$cleanupProperties))) {
            throw 'Source-static tagged-record self-test failed.'
        }

        $greenRows = New-Object Collections.Generic.List[object]
        foreach ($target in $script:RequiredWorkbenchTargets) {
            $greenRows.Add([pscustomobject]@{
                target = $target
                success = $true
                files = 1
                classes = 1
                crc = '1234abcd'
                hardErrorCount = 0
                cleanupAndSpillZero = $true
                pakCensusZero = $true
                sourceResourceDatabaseStable = $true
            })
        }
        $greenAggregate = Get-WorkbenchAggregateContract `
            -TargetRows $greenRows.ToArray()
        $wrongOrder = @($greenRows.ToArray())
        $wrongOrder[0] = [pscustomobject]@{
            target = 'PS5'; success = $true; files = 1; classes = 1
            crc = '1234abcd'; hardErrorCount = 0
            cleanupAndSpillZero = $true; pakCensusZero = $true
            sourceResourceDatabaseStable = $true
        }
        $wrongOrderAggregate = Get-WorkbenchAggregateContract `
            -TargetRows $wrongOrder
        $wrongCrc = @($greenRows.ToArray())
        $wrongCrc[4] = [pscustomobject]@{
            target = 'PS5'; success = $true; files = 1; classes = 1
            crc = 'ffffffff'; hardErrorCount = 0
            cleanupAndSpillZero = $true; pakCensusZero = $true
            sourceResourceDatabaseStable = $true
        }
        $wrongCrcAggregate = Get-WorkbenchAggregateContract `
            -TargetRows $wrongCrc
        $dirtyDiagnostic = @($greenRows.ToArray())
        $dirtyDiagnostic[2] = [pscustomobject]@{
            target = 'XBOX_SERIES'; success = $false; files = 1; classes = 1
            crc = '1234abcd'; hardErrorCount = 1
            cleanupAndSpillZero = $true; pakCensusZero = $true
            sourceResourceDatabaseStable = $true
        }
        $dirtyDiagnosticAggregate = Get-WorkbenchAggregateContract `
            -TargetRows $dirtyDiagnostic
        $dirtyBoundaries = @($greenRows.ToArray())
        $dirtyBoundaries[1] = [pscustomobject]@{
            target = 'XBOX_ONE'; success = $false; files = 1; classes = 1
            crc = '1234abcd'; hardErrorCount = 0
            cleanupAndSpillZero = $false; pakCensusZero = $false
            sourceResourceDatabaseStable = $false
        }
        $dirtyBoundariesAggregate = Get-WorkbenchAggregateContract `
            -TargetRows $dirtyBoundaries
        if (-not $greenAggregate.valid -or
            $wrongOrderAggregate.valid -or
            $wrongCrcAggregate.valid -or
            $dirtyDiagnosticAggregate.valid -or
            $dirtyBoundariesAggregate.valid) {
            throw 'Source-static Workbench aggregate self-test failed.'
        }

        $greenFoundation = Get-FoundationConsoleContract -Text (
            "Script symbol references OK: 1`n" +
            "Partisan foundation validation passed`n")
        $duplicateFoundation = Get-FoundationConsoleContract -Text (
            "Script symbol references OK: 1`n" +
            "Partisan foundation validation passed`n" +
            "Partisan foundation validation passed`n")
        if (-not $greenFoundation.valid -or $duplicateFoundation.valid -or
            $duplicateFoundation.completionMarkerCount -ne 2) {
            throw 'Source-static Foundation exact-marker self-test failed.'
        }

        $fakeFoundation = Join-Path $tempRoot 'fake-foundation.ps1'
        Write-Utf8LfText `
            -Path $fakeFoundation `
            -Text @'
Write-Host 'Script symbol references OK: 1'
Write-Host 'Partisan foundation validation passed'
'@
        $selfTestPowerShell = Get-CurrentPowerShellExecutable
        $selfTestHostIdentity = Get-PortableFileIdentity `
            -Path $selfTestPowerShell `
            -Role 'self-test-host'
        $childCaptureItems = @(Invoke-ChildPowerShell `
            -PowerShellExecutable $selfTestPowerShell `
            -WorkingDirectory $tempRoot `
            -Payload @{
                kind = 'foundation'
                checkoutRoot = $tempRoot
                scriptPath = $fakeFoundation
            })
        if ($childCaptureItems.Count -ne 1 -or
            $childCaptureItems[0].PSObject.Properties.Name -cnotcontains
                'exitCode') {
            throw ('Source-static child-process capture shape failed: ' +
                $childCaptureItems.Count + ' item(s), ' +
                (@($childCaptureItems | ForEach-Object {
                    $_.GetType().FullName
                }) -join ', '))
        }
        $childCapture = $childCaptureItems[0]
        $childFoundationContract = Get-FoundationConsoleContract `
            -Text $childCapture.stdoutText
        if ($selfTestHostIdentity.sha256 -cnotmatch '^[0-9a-f]{64}$' -or
            $childCapture.exitCode -ne 0 -or
            -not $childFoundationContract.valid) {
            throw 'Source-static child-process self-test failed.'
        }

        Assert-NoAbsolutePaths -Value ([pscustomobject]@{
            path = 'PC/raw-evidence/console.log'
            sha256 = ('0' * 64)
        })
        $absoluteRejected = $false
        try {
            Assert-NoAbsolutePaths -Value ([pscustomobject]@{
                path = [IO.Path]::GetPathRoot(
                    [Environment]::SystemDirectory) + 'forbidden\local.txt'
            })
        }
        catch {
            $absoluteRejected = $true
        }
        if (-not $absoluteRejected) {
            throw 'Source-static portable-path self-test failed.'
        }

        $junctionTarget = Join-Path $tempRoot 'junction-target'
        $junctionPath = Join-Path $tempRoot 'junction-child'
        New-Item -ItemType Directory -Path $junctionTarget | Out-Null
        Write-Utf8LfText `
            -Path (Join-Path $junctionTarget 'outside.txt') `
            -Text "outside`n"
        New-Item `
            -ItemType Junction `
            -Path $junctionPath `
            -Target $junctionTarget | Out-Null
        $reparseRejected = $false
        try {
            [void](Get-ArtifactInventory -Root $tempRoot)
        }
        catch {
            $reparseRejected = $true
        }
        finally {
            if (Test-Path -LiteralPath $junctionPath) {
                [IO.Directory]::Delete($junctionPath)
            }
        }
        if (-not $reparseRejected) {
            throw 'Source-static reparse-descendant self-test failed.'
        }
    }
    finally {
        $expectedPrefix = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).
            TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
        $resolvedTemp = [IO.Path]::GetFullPath($tempRoot)
        if ($resolvedTemp.StartsWith(
                $expectedPrefix,
                [StringComparison]::OrdinalIgnoreCase) -and
            (Split-Path -Leaf $resolvedTemp) -match
                '^PartisanSourceStaticSelfTest_[0-9a-f]{32}$' -and
            (Test-Path -LiteralPath $resolvedTemp -PathType Container)) {
            Remove-Item -LiteralPath $resolvedTemp -Recurse -Force
        }
    }

    Write-Output ('SELFTEST ' + ([pscustomobject][ordered]@{
        success = $true
        publishInputPolicy = $publish.policy
        publishInputRowCount = $publish.rowCount
        publishInputTreeSha256 = $publish.sha256
        artifactInventoryChecks = 5
        portablePathChecks = 2
        workbenchParserChecks = 6
        workbenchAggregateChecks = 5
        foundationMarkerChecks = 2
        childProcessChecks = 3
        gitIdentityChecks = 10
    } | ConvertTo-Json -Compress))
}

if ($SelfTest) {
    Invoke-SelfTest
    return
}

$runFoundation = $Foundation.IsPresent
$runWorkbench = $Workbench.IsPresent
if (-not $runFoundation -and -not $runWorkbench) {
    $runFoundation = $true
    $runWorkbench = $true
}
if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    throw 'EvidenceRoot is required for a source-static Gate 1 run.'
}

$checkoutRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$runnerPath = [IO.Path]::GetFullPath($PSCommandPath)
$foundationPath = Join-Path $PSScriptRoot 'validate-foundation.ps1'
$workbenchRunnerPath = Join-Path $PSScriptRoot `
    'run-guarded-workbench-validation.ps1'
$helperPaths = New-Object Collections.Generic.List[string]
if ($runFoundation) { $helperPaths.Add($foundationPath) }
if ($runWorkbench) { $helperPaths.Add($workbenchRunnerPath) }
foreach ($helperPath in $helperPaths) {
    if (-not (Test-Path -LiteralPath $helperPath -PathType Leaf)) {
        throw 'A required source-static helper script is missing.'
    }
}

$binding = Get-RepositoryBinding `
    -CheckoutRoot $checkoutRoot `
    -RunnerPath $runnerPath `
    -HelperPaths $helperPaths.ToArray() `
    -RequestedSourceGitHead $SourceGitHead
$powerShellExecutable = Get-CurrentPowerShellExecutable
[void](Get-PortableFileIdentity `
    -Path $powerShellExecutable `
    -Role 'source-static-host')

$resolvedEvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
if (-not (Test-Path -LiteralPath $resolvedEvidenceRoot -PathType Container)) {
    throw 'EvidenceRoot must be an existing external directory.'
}
Assert-NoReparseAncestry -Path $resolvedEvidenceRoot
if (Test-PathOverlap -First $checkoutRoot -Second $resolvedEvidenceRoot) {
    throw 'EvidenceRoot must not overlap the source checkout.'
}

$resolvedWorkbenchExecutable = ''
$resolvedAddonRoots = @()
$resolvedDefaultLogRoots = @()
$resolvedSpillRoots = @()
if ($runWorkbench) {
    if ([string]::IsNullOrWhiteSpace($WorkbenchExecutable)) {
        throw 'WorkbenchExecutable is required when Workbench is selected.'
    }
    $resolvedWorkbenchExecutable = [IO.Path]::GetFullPath(
        $WorkbenchExecutable)
    [void](Get-PortableFileIdentity `
        -Path $resolvedWorkbenchExecutable `
        -Role 'diagnostic-workbench')
    $resolvedAddonRoots = @($AddonRoots | Where-Object {
        -not [string]::IsNullOrWhiteSpace([string]$_)
    } | ForEach-Object { [IO.Path]::GetFullPath([string]$_) } |
        Sort-Object -Unique)
    $resolvedDefaultLogRoots = @($DefaultLogRoots | Where-Object {
        -not [string]::IsNullOrWhiteSpace([string]$_)
    } | ForEach-Object { [IO.Path]::GetFullPath([string]$_) } |
        Sort-Object -Unique)
    $resolvedSpillRoots = @($SpillRoots | Where-Object {
        -not [string]::IsNullOrWhiteSpace([string]$_)
    } | ForEach-Object { [IO.Path]::GetFullPath([string]$_) } |
        Sort-Object -Unique)
    if ($resolvedAddonRoots.Count -lt 1 -or
        $resolvedDefaultLogRoots.Count -lt 1 -or
        $resolvedSpillRoots.Count -lt 1) {
        throw 'Workbench requires nonempty addon, default-log, and spill roots.'
    }
    foreach ($inputRoot in @(
        $resolvedAddonRoots + $resolvedDefaultLogRoots + $resolvedSpillRoots)) {
        if (-not (Test-Path -LiteralPath $inputRoot -PathType Container)) {
            throw 'A Workbench input or monitoring root does not exist.'
        }
        Assert-NoReparseAncestry -Path $inputRoot
        if (Test-PathOverlap -First $resolvedEvidenceRoot -Second $inputRoot) {
            throw 'EvidenceRoot must not overlap a Workbench input or monitoring root.'
        }
    }
    if (Test-PathOverlap `
            -First $resolvedEvidenceRoot `
            -Second (Split-Path -Parent $resolvedWorkbenchExecutable)) {
        throw 'EvidenceRoot must not overlap the Workbench installation.'
    }
}

$utcToken = [DateTime]::UtcNow.ToString(
    'yyyyMMddTHHmmssZ',
    [Globalization.CultureInfo]::InvariantCulture)
$runId = 'source-gate1-static-' + $utcToken + '-' +
    [Guid]::NewGuid().ToString('N').Substring(0, 12)
$runRoot = Join-Path $resolvedEvidenceRoot $runId
if (Test-Path -LiteralPath $runRoot) {
    throw 'The source-static run identity is not fresh.'
}
New-Item -ItemType Directory -Path $runRoot | Out-Null
Assert-NoReparseAncestry -Path $runRoot

$receipts = New-Object Collections.Generic.List[object]
if ($runFoundation) {
    $foundationRoot = Join-Path $runRoot 'foundation'
    New-Item -ItemType Directory -Path $foundationRoot | Out-Null
    $foundationReceipt = Invoke-FoundationGate `
        -CheckoutRoot $checkoutRoot `
        -GateRoot $foundationRoot `
        -RunId $runId `
        -Binding $binding `
        -RunnerPath $runnerPath `
        -HelperPath $foundationPath `
        -HarnessHelperPaths $helperPaths.ToArray() `
        -PowerShellExecutable $powerShellExecutable `
        -RequestedSourceGitHead $SourceGitHead
    $receipts.Add([pscustomobject][ordered]@{
        evidenceKind = $foundationReceipt.summary.evidenceKind
        status = $foundationReceipt.summary.result.status
        relativePath = 'foundation/summary.json'
        sha256 = $foundationReceipt.summarySha256
    })
    Write-Output ('FOUNDATION_SUMMARY ' + ($receipts[$receipts.Count - 1] |
        ConvertTo-Json -Compress))
    if (-not $foundationReceipt.summary.result.success) {
        throw 'Source Gate 1 Foundation validation failed; see retained evidence.'
    }
}

if ($runWorkbench) {
    $workbenchRoot = Join-Path $runRoot 'workbench'
    New-Item -ItemType Directory -Path $workbenchRoot | Out-Null
    $workbenchReceipt = Invoke-WorkbenchGate `
        -CheckoutRoot $checkoutRoot `
        -GateRoot $workbenchRoot `
        -RunId $runId `
        -Binding $binding `
        -RunnerPath $runnerPath `
        -HelperPath $workbenchRunnerPath `
        -HarnessHelperPaths $helperPaths.ToArray() `
        -PowerShellExecutable $powerShellExecutable `
        -Executable $resolvedWorkbenchExecutable `
        -ResolvedAddonRoots $resolvedAddonRoots `
        -ResolvedDefaultLogRoots $resolvedDefaultLogRoots `
        -ResolvedSpillRoots $resolvedSpillRoots `
        -TimeoutSeconds $WorkbenchTimeoutSeconds `
        -PollMilliseconds $WorkbenchPollMilliseconds `
        -RequestedSourceGitHead $SourceGitHead
    $receipts.Add([pscustomobject][ordered]@{
        evidenceKind = $workbenchReceipt.summary.evidenceKind
        status = $workbenchReceipt.summary.result.status
        relativePath = 'workbench/summary.json'
        sha256 = $workbenchReceipt.summarySha256
    })
    Write-Output ('WORKBENCH_SUMMARY ' + ($receipts[$receipts.Count - 1] |
        ConvertTo-Json -Compress))
    if (-not $workbenchReceipt.summary.result.success) {
        throw 'Source Gate 1 Workbench validation failed; see retained evidence.'
    }
}

Write-Output ('SOURCE_STATIC_GATES ' + ([pscustomobject][ordered]@{
    success = $true
    sourceGitHead = $binding.sourceHead
    harnessGitHead = $binding.harnessHead
    publishInputPolicy = $binding.publishInput.policy
    publishInputTreeSha256 = $binding.publishInput.sha256
    publishInputRowCount = $binding.publishInput.rowCount
    runId = $runId
    summaries = $receipts.ToArray()
} | ConvertTo-Json -Depth 8 -Compress))
