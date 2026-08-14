Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'JsonSchemaSubset.psm1')

function Get-R0GovernanceField {
    param($Object, [string]$Name)
    if ($null -eq $Object -or [string]::IsNullOrEmpty($Name)) { return $null }
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) { return $null }
    return $property.Value
}

function Test-R0GovernanceHasField {
    param($Object, [string]$Name)
    return $null -ne $Object -and $null -ne $Object.PSObject.Properties[$Name]
}

function Add-R0GovernanceIssue {
    param([System.Collections.Generic.List[string]]$Issues, [string]$Message)
    $Issues.Add($Message)
}

function Get-R0GovernanceSha256Hex {
    param([byte[]]$Bytes)
    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString(
                $algorithm.ComputeHash($Bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $algorithm.Dispose()
    }
}

function Get-R0GovernanceFileFact {
    param([string]$RepoRoot, [string]$RelativePath)
    $absolute = Join-Path $RepoRoot $RelativePath
    if (-not (Test-Path -LiteralPath $absolute -PathType Leaf)) { return $null }
    [byte[]]$bytes = [System.IO.File]::ReadAllBytes($absolute)
    return [pscustomobject][ordered]@{
        path = $RelativePath.Replace('\', '/')
        byte_length = $bytes.Length
        sha256 = Get-R0GovernanceSha256Hex $bytes
    }
}

function Get-R0GovernanceTrackedBlobFact {
    param([string]$RepoRoot, [string]$RelativePath)
    $fact = [pscustomobject][ordered]@{
        valid = $false
        mode = ''
        object_id = ''
        blob_bytes = [long]0
        blob_sha256 = ''
        worktree_bytes = [long]0
        worktree_object_id = ''
    }
    if (-not (Test-R0GovernanceLocatorSyntax $RelativePath)) { return $fact }
    $rows = @(& git -C $RepoRoot -c core.quotepath=false --literal-pathspecs `
        ls-files --stage -- $RelativePath)
    if ($LASTEXITCODE -ne 0 -or $rows.Count -ne 1) { return $fact }
    $match = [regex]::Match([string]$rows[0],
        '^(?<mode>100(?:644|755)) (?<object>[0-9a-f]{40,64}) 0\t(?<path>.+)$')
    if (-not $match.Success -or $match.Groups['path'].Value -cne $RelativePath) {
        return $fact
    }
    $objectId = $match.Groups['object'].Value
    $type = @(& git -C $RepoRoot cat-file -t $objectId 2>$null)
    if ($LASTEXITCODE -ne 0 -or $type.Count -ne 1 -or [string]$type[0] -cne 'blob') {
        return $fact
    }
    $size = @(& git -C $RepoRoot cat-file -s $objectId 2>$null)
    [long]$blobBytes = 0
    if ($LASTEXITCODE -ne 0 -or $size.Count -ne 1 -or
        -not [long]::TryParse([string]$size[0], [ref]$blobBytes) -or $blobBytes -le 0) {
        return $fact
    }
    $absolute = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $RelativePath))
    if (-not (Test-Path -LiteralPath $absolute -PathType Leaf)) { return $fact }
    $item = Get-Item -LiteralPath $absolute -Force
    if ($item -isnot [System.IO.FileInfo] -or
        ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0 -or
        $item.Length -le 0) { return $fact }
    $rootFull = [System.IO.Path]::GetFullPath($RepoRoot).TrimEnd('\', '/')
    $cursor = $item.Directory
    while ($null -ne $cursor -and
        $cursor.FullName.StartsWith($rootFull, [System.StringComparison]::OrdinalIgnoreCase)) {
        if (($cursor.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            return $fact
        }
        if ($cursor.FullName -ceq $rootFull) { break }
        $cursor = $cursor.Parent
    }
    $worktreeObject = @(& git -C $RepoRoot --literal-pathspecs `
        hash-object --path=$RelativePath -- $RelativePath 2>$null)
    if ($LASTEXITCODE -ne 0 -or $worktreeObject.Count -ne 1 -or
        [string]$worktreeObject[0] -cne $objectId) {
        return $fact
    }
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = 'git'
    $startInfo.Arguments = "-C `"$RepoRoot`" cat-file blob $objectId"
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    [void]$process.Start()
    $stream = [System.IO.MemoryStream]::new()
    try {
        $process.StandardOutput.BaseStream.CopyTo($stream)
        $errorText = $process.StandardError.ReadToEnd()
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) { throw $errorText }
        [byte[]]$bytes = $stream.ToArray()
        if ($bytes.Length -ne $blobBytes) { return $fact }
        $fact.valid = $true
        $fact.mode = $match.Groups['mode'].Value
        $fact.object_id = $objectId
        $fact.blob_bytes = $blobBytes
        $fact.blob_sha256 = Get-R0GovernanceSha256Hex $bytes
        $fact.worktree_bytes = [long]$item.Length
        $fact.worktree_object_id = [string]$worktreeObject[0]
        return $fact
    }
    finally {
        $stream.Dispose()
        $process.Dispose()
    }
}

function Get-R0GovernanceCachedBlobFact {
    param($Context, [string]$RelativePath)
    $fact = Get-R0GovernanceField $Context.tracked_blob_facts $RelativePath
    if ($null -ne $fact) { return $fact }
    return [pscustomobject][ordered]@{
        valid = $false
        mode = ''
        object_id = ''
        blob_bytes = [long]0
        blob_sha256 = ''
        worktree_bytes = [long]0
        worktree_object_id = ''
    }
}

function Test-R0GovernanceLocatorSyntax {
    param([string]$Value)
    if ([string]::IsNullOrWhiteSpace($Value)) { return $false }
    if ($Value -match '\\|%|\?|#' -or
        $Value -match '^[A-Za-z]:' -or
        $Value -match '^[A-Za-z][A-Za-z0-9+.-]*:' -or
        $Value.StartsWith('/') -or
        [System.IO.Path]::IsPathRooted($Value)) { return $false }
    foreach ($segment in @($Value -split '/')) {
        if ([string]::IsNullOrEmpty($segment) -or $segment -in @('.', '..')) { return $false }
    }
    return $true
}

function Read-R0GovernanceAsciiLine {
    param([System.IO.Stream]$Stream)
    $bytes = [System.Collections.Generic.List[byte]]::new()
    while ($true) {
        $value = $Stream.ReadByte()
        if ($value -lt 0) { throw 'Git object batch output ended before its header terminator.' }
        if ($value -eq 0x0A) { break }
        $bytes.Add([byte]$value)
    }
    return [System.Text.Encoding]::ASCII.GetString($bytes.ToArray())
}

function Get-R0GovernanceGitObjectFacts {
    param([string]$RepoRoot, [string[]]$ObjectIds)
    [string[]]$orderedObjectIds = @($ObjectIds | Sort-Object -Unique)
    [System.Array]::Sort($orderedObjectIds, [System.StringComparer]::Ordinal)
    $facts = [System.Collections.Generic.Dictionary[string,object]]::new(
        [System.StringComparer]::Ordinal)
    if ($orderedObjectIds.Count -eq 0) { return ,$facts }

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = 'git'
    $startInfo.Arguments = "-C `"$RepoRoot`" --no-replace-objects cat-file --batch"
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardInput = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.StandardOutputEncoding = [System.Text.UTF8Encoding]::new($false, $true)
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    [void]$process.Start()
    $errorTask = $process.StandardError.ReadToEndAsync()
    try {
        foreach ($requestedObjectId in $orderedObjectIds) {
            $process.StandardInput.WriteLine($requestedObjectId)
            $process.StandardInput.Flush()
            $header = Read-R0GovernanceAsciiLine -Stream $process.StandardOutput.BaseStream
            if ($header -cmatch '^([0-9a-f]{40,64}) missing$') {
                $facts[$requestedObjectId] = [pscustomobject][ordered]@{
                    object_id = $requestedObjectId
                    object_type = 'missing'
                    byte_length = [long]0
                    sha256 = ''
                    bytes = [byte[]]@()
                }
                continue
            }
            if ($header -cnotmatch '^([0-9a-f]{40,64}) (blob|tree|commit|tag) ([0-9]+)$' -or
                [string]$Matches[1] -cne $requestedObjectId) {
                throw "Git object batch output has an invalid header for '$requestedObjectId'."
            }
            $objectType = [string]$Matches[2]
            [long]$byteLength = 0
            if (-not [long]::TryParse([string]$Matches[3], [ref]$byteLength) -or
                $byteLength -lt 0 -or $byteLength -gt [int]::MaxValue) {
                throw "Git object '$requestedObjectId' has an unsupported byte length."
            }
            [byte[]]$bytes = [byte[]]::new([int]$byteLength)
            $offset = 0
            while ($offset -lt $bytes.Length) {
                $read = $process.StandardOutput.BaseStream.Read(
                    $bytes, $offset, $bytes.Length - $offset)
                if ($read -le 0) {
                    throw "Git object '$requestedObjectId' ended before its declared byte length."
                }
                $offset += $read
            }
            if ($process.StandardOutput.BaseStream.ReadByte() -ne 0x0A) {
                throw "Git object '$requestedObjectId' lacks its batch trailer."
            }
            $facts[$requestedObjectId] = [pscustomobject][ordered]@{
                object_id = $requestedObjectId
                object_type = $objectType
                byte_length = $byteLength
                sha256 = Get-R0GovernanceSha256Hex $bytes
                bytes = $bytes
            }
        }
        $process.StandardInput.Close()
        $process.WaitForExit()
        $errorText = $errorTask.Result
        if ($process.ExitCode -ne 0) { throw $errorText }
    }
    finally { $process.Dispose() }
    return ,$facts
}

function Get-R0GovernanceIndexEntries {
    param([string]$RepoRoot)
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = 'git'
    $startInfo.Arguments = "-C `"$RepoRoot`" -c core.quotepath=false ls-files --stage -z --"
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    [void]$process.Start()
    $stream = [System.IO.MemoryStream]::new()
    try {
        $process.StandardOutput.BaseStream.CopyTo($stream)
        $errorText = $process.StandardError.ReadToEnd()
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) { throw $errorText }
        [byte[]]$raw = $stream.ToArray()
    }
    finally {
        $stream.Dispose()
        $process.Dispose()
    }
    if ($raw.Length -eq 0 -or $raw[$raw.Length - 1] -ne 0) {
        throw 'Git index projection is empty or lacks its terminal NUL.'
    }
    $text = [System.Text.UTF8Encoding]::new($false, $true).GetString($raw)
    $entries = [System.Collections.Generic.List[object]]::new()
    foreach ($record in @($text.Split([char]0))) {
        if ($record.Length -eq 0) { continue }
        $match = [regex]::Match($record,
            '^(?<mode>[0-9]{6}) (?<object>[0-9a-f]{40,64}) (?<stage>[0-3])\t(?<path>.+)$',
            [System.Text.RegularExpressions.RegexOptions]::Singleline)
        if (-not $match.Success) { throw 'Git index projection contains an invalid entry.' }
        $entries.Add([pscustomobject][ordered]@{
                mode = $match.Groups['mode'].Value
                object_id = $match.Groups['object'].Value
                stage = [int]$match.Groups['stage'].Value
                path = $match.Groups['path'].Value.Replace('\', '/')
                object_type = ''
                blob_byte_length = [long]0
                blob_sha256 = ''
                blob_bytes = [byte[]]@()
            })
    }

    [string[]]$objectIds = @($entries | ForEach-Object { [string]$_.object_id } |
        Sort-Object -Unique)
    $objectFacts = Get-R0GovernanceGitObjectFacts -RepoRoot $RepoRoot `
        -ObjectIds $objectIds
    foreach ($entry in $entries) {
        $fact = if ($objectFacts.ContainsKey([string]$entry.object_id)) {
            $objectFacts[[string]$entry.object_id]
        } else { $null }
        $entry.object_type = if ($null -ne $fact) { $fact.object_type } else { 'missing' }
        if ($null -ne $fact -and $fact.object_type -ceq 'blob') {
            $entry.blob_byte_length = [long]$fact.byte_length
            $entry.blob_sha256 = [string]$fact.sha256
            $entry.blob_bytes = [byte[]]$fact.bytes
        }
    }
    return $entries.ToArray()
}

function Get-R0GovernanceTrackedPaths {
    param([string]$RepoRoot, [object[]]$IndexEntries)
    if ($null -eq $IndexEntries) {
        $IndexEntries = @(Get-R0GovernanceIndexEntries -RepoRoot $RepoRoot)
    }
    $paths = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::Ordinal)
    foreach ($entry in @($IndexEntries)) {
        [void]$paths.Add(([string](Get-R0GovernanceField $entry 'path')).Replace('\', '/'))
    }
    return ,$paths
}

function Test-R0GovernanceConsumerText {
    param([string]$Text)
    return $Text -cmatch '(?i)(?:scientific[-_]?context|ScientificContext|gnczmkn\.scientific-context|provenance[-_]?inventory|ProvenanceInventory|GNC-LIC-PROV-001)'
}

function Test-R0GovernanceRepositoryGrantText {
    param([string]$Text)
    return $Text -cmatch '(?im)(?:SPDX-(?:License-Identifier|License-Expression)\s*:|licensed\s+under\s+(?:the\s+)?[^\r\n]{1,120}|released\s+under\s+(?:the\s+)?[^\r\n]{1,120}|distributed\s+under\s+(?:the\s+)?terms\s+of\s+[^\r\n]{1,120}|permission\s+is\s+hereby\s+granted\b|["'']licenses?["'']\s*:\s*(?:["''][^"''\r\n]+["'']|\[|\{)|^\s*license(?:[-_]expression)?\s*[:=]\s*(?:["''][^"''\r\n]+["'']|[^\s#;][^\r\n#;]*)\s*(?:[#;].*)?$|(?:spec|s)\.licenses?\s*=\s*(?:["''][^"''\r\n]+["'']|\[[^\r\n]+\])|<(?:license|PackageLicenseExpression|PackageLicenseFile)(?:\s+[^>]*)?>\s*(?:<name(?:\s+[^>]*)?>\s*)?[^<\r\n]+)'
}

function Test-R0GovernanceRawSpdxSignal {
    param([byte[]]$Bytes)
    if ($null -eq $Bytes -or $Bytes.Length -eq 0) { return $false }
    $ascii = [System.Text.Encoding]::ASCII.GetString($Bytes)
    if ($ascii -cmatch '(?im)SPDX-(?:License-Identifier|License-Expression)\s*:') {
        return $true
    }
    foreach ($encoding in @([System.Text.Encoding]::Unicode,
            [System.Text.Encoding]::BigEndianUnicode)) {
        if ($encoding.GetString($Bytes) -cmatch
            '(?im)SPDX-(?:License-Identifier|License-Expression)\s*:') {
            return $true
        }
    }
    return $false
}

function Test-R0GovernanceReviewedBinaryIdentity {
    param(
        [string]$Path,
        [long]$ByteLength,
        [string]$RawSha256,
        [string]$StageObjectId
    )
    return $Path -ceq 'reference/legacy/legacy-source.zip' -and
        $ByteLength -eq 990450 -and
        $RawSha256 -ceq '2159a324fd897e4bd508c140a36c9165d744e4e4e61861c5b568201707f988e5' -and
        $StageObjectId -ceq 'd2aa60e7401b0a16d8eca61d62ee714e0745b9d5'
}

function New-R0GovernanceScanDocument {
    param(
        [string]$Path,
        [byte[]]$Bytes,
        [string]$StageObjectId = ''
    )
    $rawSha256 = Get-R0GovernanceSha256Hex $Bytes
    $reviewedBinary = Test-R0GovernanceReviewedBinaryIdentity -Path $Path `
        -ByteLength $Bytes.Length -RawSha256 $rawSha256 -StageObjectId $StageObjectId
    $text = ''
    $decodeError = $false
    $controlError = $false
    if (-not $reviewedBinary) {
        try {
            $text = [System.Text.UTF8Encoding]::new($false, $true).GetString($Bytes)
            if ($text.IndexOf([char]0) -ge 0) {
                $controlError = $true
            }
            if (-not $controlError) {
                foreach ($character in $text.ToCharArray()) {
                    $value = [int]$character
                    if (($value -lt 0x20 -and $value -notin @(0x09, 0x0a, 0x0d)) -or
                        $value -eq 0x7f) {
                        $controlError = $true
                        break
                    }
                }
            }
        }
        catch { $decodeError = $true }
    }
    return [pscustomobject][ordered]@{
        path = $Path
        grant_text_signal = Test-R0GovernanceRepositoryGrantText $text
        decode_error = $decodeError
        control_error = $controlError
        reviewed_binary = $reviewedBinary
        raw_sha256 = $rawSha256
        byte_length = [long]$Bytes.Length
        stage_object_id = $StageObjectId
        spdx_signal = Test-R0GovernanceRawSpdxSignal $Bytes
    }
}

function New-R0GovernanceTextScanDocument {
    param([string]$Path, [string]$Text)
    return New-R0GovernanceScanDocument -Path $Path `
        -Bytes ([System.Text.UTF8Encoding]::new($false).GetBytes($Text))
}

function Get-R0GovernanceTrackedBlobObjectIds {
    param([string]$RepoRoot, [object[]]$IndexEntries)
    if ($null -eq $IndexEntries) {
        $IndexEntries = @(Get-R0GovernanceIndexEntries -RepoRoot $RepoRoot)
    }
    $objects = [System.Collections.Generic.Dictionary[string,string]]::new(
        [System.StringComparer]::Ordinal)
    foreach ($entry in @($IndexEntries)) {
        if ((Get-R0GovernanceField $entry 'mode') -cin @('100644','100755') -and
            (Get-R0GovernanceField $entry 'stage') -eq 0 -and
            (Get-R0GovernanceField $entry 'object_type') -ceq 'blob') {
            $objects[[string](Get-R0GovernanceField $entry 'path')] =
                [string](Get-R0GovernanceField $entry 'object_id')
        }
    }
    return ,$objects
}

function Get-R0GovernanceIndexEntryMap {
    param([object[]]$IndexEntries)
    $entries = [System.Collections.Generic.Dictionary[string,object]]::new(
        [System.StringComparer]::Ordinal)
    foreach ($entry in @($IndexEntries)) {
        $path = [string](Get-R0GovernanceField $entry 'path')
        if (-not $entries.ContainsKey($path)) { $entries[$path] = $entry }
    }
    return ,$entries
}

function Get-R0GovernanceIndexBlobBytes {
    param([object[]]$IndexEntries, [string]$RelativePath)
    $entryMap = Get-R0GovernanceIndexEntryMap -IndexEntries $IndexEntries
    $entry = if ($entryMap.ContainsKey($RelativePath)) {
        $entryMap[$RelativePath]
    }
    else { $null }
    if ($null -eq $entry -or
        (Get-R0GovernanceField $entry 'mode') -cnotin @('100644','100755') -or
        (Get-R0GovernanceField $entry 'stage') -ne 0 -or
        (Get-R0GovernanceField $entry 'object_type') -cne 'blob') {
        throw "Reviewed stage-0 Git blob is unavailable: $RelativePath"
    }
    return ,[byte[]](Get-R0GovernanceField $entry 'blob_bytes')
}

function Get-R0GovernanceIndexBlobText {
    param([object[]]$IndexEntries, [string]$RelativePath)
    [byte[]]$bytes = Get-R0GovernanceIndexBlobBytes -IndexEntries $IndexEntries `
        -RelativePath $RelativePath
    return [System.Text.UTF8Encoding]::new($false, $true).GetString($bytes)
}

function ConvertFrom-R0GovernanceStrictJsonText {
    param([string]$Text, [string]$Label)
    $tempDir = Join-Path ([System.IO.Path]::GetTempPath()) (
        'gnczmkn-r0-gov-json-' + [guid]::NewGuid().ToString('N'))
    [void][System.IO.Directory]::CreateDirectory($tempDir)
    $tempPath = Join-Path $tempDir 'value.json'
    try {
        [System.IO.File]::WriteAllText(
            $tempPath, $Text, [System.Text.UTF8Encoding]::new($false))
        return Read-StrictJsonFile -Path $tempPath
    }
    catch { throw "$Label is invalid strict JSON: $($_.Exception.Message)" }
    finally {
        if (Test-Path -LiteralPath $tempDir) {
            [System.IO.Directory]::Delete($tempDir, $true)
        }
    }
}

function Get-R0GovernanceIndexJsonObject {
    param([object[]]$IndexEntries, [string]$RelativePath)
    return ConvertFrom-R0GovernanceStrictJsonText `
        -Text (Get-R0GovernanceIndexBlobText -IndexEntries $IndexEntries `
            -RelativePath $RelativePath) -Label $RelativePath
}

function Get-R0GovernanceIndexFlagTags {
    param([string]$RepoRoot)
    $tags = [ordered]@{}
    foreach ($kind in @(
            [pscustomobject]@{name='cached';arguments='-t'},
            [pscustomobject]@{name='assume_unchanged';arguments='-v'},
            [pscustomobject]@{name='fsmonitor';arguments='-f'})) {
        foreach ($line in @(& git -C $RepoRoot -c core.quotepath=false `
                ls-files $kind.arguments --)) {
            if ([string]$line -cmatch '^(.?) (.+)$') {
                $path = [string]$Matches[2]
                if (-not $tags.Contains($path)) {
                    $tags[$path] = [pscustomobject][ordered]@{}
                }
                Add-Member -InputObject $tags[$path] -NotePropertyName $kind.name `
                    -NotePropertyValue ([string]$Matches[1])
            }
        }
    }
    return [pscustomobject]$tags
}

function Get-R0GovernanceIndexDebugFlags {
    param([string]$RepoRoot)
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = 'git'
    $startInfo.Arguments = "-C `"$RepoRoot`" -c core.quotepath=false ls-files --debug -z --"
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    [void]$process.Start()
    $stream = [System.IO.MemoryStream]::new()
    try {
        $process.StandardOutput.BaseStream.CopyTo($stream)
        $errorText = $process.StandardError.ReadToEnd()
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) { throw $errorText }
        [byte[]]$raw = $stream.ToArray()
    }
    finally {
        $stream.Dispose()
        $process.Dispose()
    }
    if ($raw.Length -eq 0) {
        throw 'Git index debug projection is empty.'
    }
    $text = [System.Text.UTF8Encoding]::new($false, $true).GetString($raw)
    $records = @($text.Split([char]0))
    $debugFlags = [ordered]@{}
    if ($records.Count -lt 2 -or [string]::IsNullOrEmpty([string]$records[0])) {
        throw 'Git index debug projection contains no first path.'
    }
    $previousPath = ([string]$records[0]).Replace('\', '/')
    for ($index = 1; $index -lt $records.Count; ++$index) {
        $record = [string]$records[$index]
        if ($record.Length -eq 0) { continue }
        if ($record -cnotmatch '(?m)^  size: [0-9]+\s+flags: ([0-9a-f]+)$') {
            throw "Git index debug projection lacks flags for '$previousPath'."
        }
        $debugFlags[$previousPath] = [string]$Matches[1]
        $separator = $record.LastIndexOf("`n")
        $nextPath = if ($separator -ge 0 -and $separator + 1 -lt $record.Length) {
            $record.Substring($separator + 1).Replace('\', '/')
        }
        else { '' }
        $previousPath = if ([string]::IsNullOrEmpty($nextPath)) { $null } else { $nextPath }
    }
    if ($null -ne $previousPath) {
        throw "Git index debug projection lacks metadata for '$previousPath'."
    }
    return [pscustomobject]$debugFlags
}

function Get-R0GovernanceIndexFilterAttributes {
    param([string]$RepoRoot, [string[]]$Paths)
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = 'git'
    $startInfo.Arguments = "-C `"$RepoRoot`" -c core.quotepath=false check-attr --cached -z filter working-tree-encoding ident --stdin"
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardInput = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.StandardOutputEncoding = [System.Text.UTF8Encoding]::new($false, $true)
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    [void]$process.Start()
    $outputTask = $process.StandardOutput.ReadToEndAsync()
    $errorTask = $process.StandardError.ReadToEndAsync()
    $inputText = (@($Paths) -join [char]0) + [char]0
    [byte[]]$inputBytes = [System.Text.UTF8Encoding]::new($false).GetBytes($inputText)
    $process.StandardInput.BaseStream.Write($inputBytes, 0, $inputBytes.Length)
    $process.StandardInput.Close()
    $process.WaitForExit()
    $output = $outputTask.Result
    $errorText = $errorTask.Result
    try {
        if ($process.ExitCode -ne 0) { throw $errorText }
    }
    finally { $process.Dispose() }
    $parts = @($output.Split([char]0))
    $result = [ordered]@{}
    for ($index = 0; $index + 2 -lt $parts.Count; $index += 3) {
        $path = [string]$parts[$index]
        $attribute = [string]$parts[$index + 1]
        $value = [string]$parts[$index + 2]
        if ([string]::IsNullOrEmpty($path)) { continue }
        if (-not $result.Contains($path)) {
            $result[$path] = [pscustomobject][ordered]@{}
        }
        $existingProperty = $result[$path].PSObject.Properties[$attribute]
        if ($null -eq $existingProperty) {
            Add-Member -InputObject $result[$path] -NotePropertyName $attribute `
                -NotePropertyValue $value
        }
        elseif ([string]$existingProperty.Value -cne $value) {
            throw "Git index attribute projection is inconsistent for '$path' and '$attribute'."
        }
    }
    return [pscustomobject]$result
}

function Remove-R0GovernanceTempDirectory {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return }
    foreach ($item in @(Get-ChildItem -LiteralPath $Path -Recurse -Force `
            -ErrorAction SilentlyContinue)) {
        try { $item.Attributes = [System.IO.FileAttributes]::Normal } catch {}
    }
    try { (Get-Item -LiteralPath $Path -Force).Attributes = [System.IO.FileAttributes]::Normal } catch {}
    [System.IO.Directory]::Delete($Path, $true)
}

function Invoke-R0GovernanceGitProbe {
    param([string]$RepoRoot, [string[]]$Arguments)
    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = @(& git -C $RepoRoot @Arguments 2>$null)
        $exitCode = $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $previousErrorActionPreference }
    if ($exitCode -ne 0) {
        throw "Git probe failed: git $($Arguments -join ' ')"
    }
    return @($output)
}

function Test-R0GovernanceIntentToAddProbe {
    $tempDir = Join-Path ([System.IO.Path]::GetTempPath()) (
        'gnczmkn-r0-gov-ita-' + [guid]::NewGuid().ToString('N'))
    [void][System.IO.Directory]::CreateDirectory($tempDir)
    try {
        [void](Invoke-R0GovernanceGitProbe $tempDir @('init','-q'))
        [System.IO.File]::WriteAllBytes((Join-Path $tempDir 'probe.txt'), [byte[]]@())
        [void](Invoke-R0GovernanceGitProbe $tempDir @('add','-N','--','probe.txt'))
        $entries = @(Get-R0GovernanceIndexEntries -RepoRoot $tempDir)
        $tags = Get-R0GovernanceIndexFlagTags -RepoRoot $tempDir
        $debugFlags = Get-R0GovernanceIndexDebugFlags -RepoRoot $tempDir
        $worktreeFacts = @(Get-R0GovernanceWorktreeObjectFacts -RepoRoot $tempDir `
                -IndexEntries $entries)
        return $entries.Count -eq 1 -and $entries[0].path -ceq 'probe.txt' -and
            $entries[0].object_id -ceq 'e69de29bb2d1d6434b8b29ae775ad8c2e48c5391' -and
            $tags.'probe.txt'.cached -ceq 'H' -and
            $tags.'probe.txt'.assume_unchanged -ceq 'H' -and
            $tags.'probe.txt'.fsmonitor -ceq 'H' -and
            [string]$debugFlags.'probe.txt' -cne '0' -and
            $worktreeFacts.Count -eq 1 -and $worktreeFacts[0].matches_index -eq $true
    }
    finally { Remove-R0GovernanceTempDirectory $tempDir }
}

function Test-R0GovernanceContentFilterProbe {
    $tempDir = Join-Path ([System.IO.Path]::GetTempPath()) (
        'gnczmkn-r0-gov-filter-' + [guid]::NewGuid().ToString('N'))
    [void][System.IO.Directory]::CreateDirectory($tempDir)
    try {
        [void](Invoke-R0GovernanceGitProbe $tempDir @('init','-q'))
        [System.IO.File]::WriteAllText((Join-Path $tempDir '.gitattributes'),
            "probe.txt filter=inject`n", [System.Text.UTF8Encoding]::new($false))
        [System.IO.File]::WriteAllText((Join-Path $tempDir 'probe.txt'),
            "reviewed harmless text`n", [System.Text.UTF8Encoding]::new($false))
        $spdxLine = '// SPDX-License' + '-Identifier: MIT'
        [void](Invoke-R0GovernanceGitProbe $tempDir @(
                'config','filter.inject.clean',("printf '" + $spdxLine + "'")))
        [void](Invoke-R0GovernanceGitProbe $tempDir @(
                'config','filter.inject.required','true'))
        [void](Invoke-R0GovernanceGitProbe $tempDir @(
                'add','--','.gitattributes','probe.txt'))
        $entries = @(Get-R0GovernanceIndexEntries -RepoRoot $tempDir)
        [string[]]$paths = @($entries | ForEach-Object { [string]$_.path })
        $attributes = Get-R0GovernanceIndexFilterAttributes -RepoRoot $tempDir `
            -Paths $paths
        $worktreeFacts = @(Get-R0GovernanceWorktreeObjectFacts -RepoRoot $tempDir `
                -IndexEntries $entries)
        $probeEntry = @($entries | Where-Object { $_.path -ceq 'probe.txt' })[0]
        $probeWorktree = @($worktreeFacts | Where-Object { $_.path -ceq 'probe.txt' })[0]
        $probeText = [System.Text.Encoding]::ASCII.GetString([byte[]]$probeEntry.blob_bytes)
        return $attributes.'probe.txt'.filter -ceq 'inject' -and
            $probeWorktree.matches_index -eq $true -and
            $probeText -ceq $spdxLine
    }
    finally { Remove-R0GovernanceTempDirectory $tempDir }
}

function Test-R0GovernanceReplacementObjectProbe {
    $tempDir = Join-Path ([System.IO.Path]::GetTempPath()) (
        'gnczmkn-r0-gov-replace-' + [guid]::NewGuid().ToString('N'))
    [void][System.IO.Directory]::CreateDirectory($tempDir)
    try {
        [void](Invoke-R0GovernanceGitProbe $tempDir @('init','-q'))
        $originalPath = Join-Path $tempDir 'original.txt'
        $replacementPath = Join-Path $tempDir 'replacement.txt'
        $spdxLine = '// SPDX-License' + '-Identifier: MIT'
        [System.IO.File]::WriteAllText($originalPath,
            $spdxLine, [System.Text.UTF8Encoding]::new($false))
        [System.IO.File]::WriteAllText($replacementPath,
            'reviewed harmless text', [System.Text.UTF8Encoding]::new($false))
        $originalId = [string](@(Invoke-R0GovernanceGitProbe $tempDir @(
                    'hash-object','-w','--','original.txt'))[0])
        $replacementId = [string](@(Invoke-R0GovernanceGitProbe $tempDir @(
                    'hash-object','-w','--','replacement.txt'))[0])
        [void](Invoke-R0GovernanceGitProbe $tempDir @(
                'replace',$originalId,$replacementId))
        $facts = Get-R0GovernanceGitObjectFacts -RepoRoot $tempDir `
            -ObjectIds @($originalId)
        $exactText = [System.Text.Encoding]::UTF8.GetString(
            [byte[]]$facts[$originalId].bytes)
        $replacementText = (@(& git -C $tempDir cat-file blob $originalId) -join "`n")
        if ($LASTEXITCODE -ne 0) { return $false }
        return $exactText -ceq $spdxLine -and
            $replacementText -ceq 'reviewed harmless text'
    }
    finally { Remove-R0GovernanceTempDirectory $tempDir }
}

function Get-R0GovernanceWorktreeObjectFacts {
    param([string]$RepoRoot, [object[]]$IndexEntries)
    $facts = [System.Collections.Generic.List[object]]::new()
    $rootFull = [System.IO.Path]::GetFullPath($RepoRoot).TrimEnd('\','/')
    foreach ($entry in @($IndexEntries)) {
        $path = [string](Get-R0GovernanceField $entry 'path')
        $indexObjectId = [string](Get-R0GovernanceField $entry 'object_id')
        $absolute = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $path))
        $contained = $absolute.StartsWith($rootFull + [System.IO.Path]::DirectorySeparatorChar,
            [System.StringComparison]::OrdinalIgnoreCase)
        $regular = $contained -and (Test-Path -LiteralPath $absolute -PathType Leaf)
        $reparse = $false
        if ($regular) {
            $item = Get-Item -LiteralPath $absolute -Force
            $regular = $item -is [System.IO.FileInfo]
            $cursor = if ($regular) { $item } else { $null }
            while ($null -ne $cursor) {
                if (($cursor.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                    $reparse = $true
                    break
                }
                $cursor = if ($cursor -is [System.IO.FileInfo]) {
                    $cursor.Directory
                }
                else { $cursor.Parent }
                if ($null -ne $cursor -and
                    $cursor.FullName -ceq [System.IO.Path]::GetFullPath($RepoRoot).TrimEnd('\','/')) {
                    if (($cursor.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                        $reparse = $true
                    }
                    break
                }
            }
        }
        $worktreeObjectId = ''
        if ($regular -and -not $reparse) {
            $output = @(& git -C $RepoRoot --literal-pathspecs hash-object `
                    --path=$path -- $path 2>$null)
            if ($LASTEXITCODE -eq 0 -and $output.Count -eq 1) {
                $worktreeObjectId = [string]$output[0]
            }
        }
        $facts.Add([pscustomobject][ordered]@{
                path = $path
                index_object_id = $indexObjectId
                worktree_object_id = $worktreeObjectId
                worktree_byte_length = if ($regular) { [long]$item.Length } else { [long]0 }
                contained_in_repository = $contained
                regular_file = $regular
                reparse_path = $reparse
                matches_index = $regular -and -not $reparse -and
                    $worktreeObjectId -ceq $indexObjectId
            })
    }
    return $facts.ToArray()
}

function Test-R0GovernancePackageMetadataPath {
    param([string]$Path)
    return [regex]::IsMatch($Path,
        '(?:^|/)(?:package\.json|package\.ya?ml|composer\.json|pyproject\.toml|Cargo\.toml|vcpkg\.json|pom\.xml|go\.mod|deno\.json|conanfile(?:\.txt|\.py)?|setup\.cfg|setup\.py|PKG-INFO|METADATA|META\.(?:json|ya?ml)|[^/]+\.(?:nuspec|csproj|fsproj|vbproj|gemspec))$',
        [System.Text.RegularExpressions.RegexOptions]::IgnoreCase -bor
            [System.Text.RegularExpressions.RegexOptions]::CultureInvariant)
}

function Test-R0GovernanceImplicitGrantPath {
    param([string]$Path)
    if ([string]::Equals($Path, 'LICENSE-STATUS.md',
            [System.StringComparison]::OrdinalIgnoreCase) -or
        [string]::Equals($Path, 'docs/governance/license-and-provenance-policy.md',
            [System.StringComparison]::OrdinalIgnoreCase)) { return $false }
    return [regex]::IsMatch($Path,
            '(?:^|/)(?:(?:LICENSE|LICENCE|COPYING|UNLICENSE)(?:[._-][^/]+)?|REUSE\.toml)$|(?:^|/)(?:LICENSES|\.reuse)/',
            [System.Text.RegularExpressions.RegexOptions]::IgnoreCase -bor
                [System.Text.RegularExpressions.RegexOptions]::CultureInvariant) -or
        (Test-R0GovernancePackageMetadataPath $Path)
}

function Test-R0GovernanceGrantSurfacePath {
    param([string]$Path)
    return [regex]::IsMatch($Path,
            '(?:^|/)(?:(?:LICENSE|LICENCE|COPYING|UNLICENSE|README|CONTRIBUTING|NOTICE|COPYRIGHT|SECURITY)(?:[._-][^/]+)?|REUSE\.toml)$|(?:^|/)(?:LICENSES|\.reuse)/',
            [System.Text.RegularExpressions.RegexOptions]::IgnoreCase -bor
                [System.Text.RegularExpressions.RegexOptions]::CultureInvariant) -or
        (Test-R0GovernancePackageMetadataPath $Path)
}

function Get-R0GovernanceReportInputPaths {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [string]$EnvironmentRelativePath,
        [object[]]$IndexEntries
    )
    $paths = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::Ordinal)
    [string[]]$staticPaths = @(
        'LICENSE-STATUS.md',
        'README.md',
        'CONTRIBUTING.md',
        'docs/adr/0006-si-frame-and-simulation-time-conventions.md',
        'docs/adr/0007-passive-hamilton-quaternion-convention.md',
        'docs/adr/0008-internal-default-license-and-provenance-gate.md',
        'docs/governance/license-and-provenance-policy.md',
        'docs/governance/provenance-inventory.json',
        'docs/governance/toolchain-support-matrix.json',
        'docs/governance/r0-governance-review-contract.json',
        'docs/governance/r0-owner-authorization.json',
        'docs/governance/schemas/scientific-context.schema.json',
        'docs/handoff/r0-execution-state.md',
        'docs/quality/provenance-review-checklist.md',
        'docs/quality/hosted-ci-evidence-R0-GOV-001.json',
        'docs/quality/r0-first-wave-reconciliation-audit.md',
        'docs/quality/r0-g0-g1-readiness-audit.md',
        'docs/quality/scientific-conventions-cross-tool-report.json',
        'docs/quality/team-toolchain-readiness-report.json',
        'docs/tasks/backlog.json',
        'docs/tasks/work-packages/R0-GATE-001.md',
        'docs/tasks/work-packages/R0-GOV-001.md',
        'docs/tasks/work-packages/R0-GOV-002.md',
        'docs/tasks/work-packages/R0-SCI-001.md',
        'docs/team/role-assignments.json',
        'project-manifest.json',
        'design-notes/gnczmkn-architecture-roadmap/03-mathematics-and-numerical-foundation.md',
        'design-notes/gnczmkn-architecture-roadmap/06-simulation-kernel-time-and-lifecycle.md',
        'fixtures/ref-scientific-conventions/cases.json',
        'fixtures/ref-scientific-conventions/conventions.json',
        'fixtures/ref-scientific-conventions/fixture-manifest.json',
        'fixtures/ref-scientific-conventions/scientific-context.json',
        'reference/legacy/legacy-source.sha256',
        'reference/legacy/legacy-source.zip',
        'reference/legacy/reproduction/current.json',
        'reference/legacy/source-manifest.json',
        'specs/fixture-manifest.schema.json',
        'specs/oracle-manifest.schema.json',
        'specs/plan-proof-record.schema.json',
        'specs/r0-schema-contract-lock.json',
        'tests/scientific_conventions.cpp',
        'tools/modules/JsonSchemaSubset.psm1',
        'tools/modules/R0GovernanceReview.psm1',
        'tools/scientific_conventions_reference.py',
        'tools/validate-license-provenance.ps1',
        'tools/validate-scientific-conventions.ps1',
        'tools/verify-repository.ps1',
        '.github/workflows/ci.yml',
        'CMakePresets.json',
        'CMakeLists.txt')
    foreach ($path in $staticPaths) { [void]$paths.Add($path) }
    if (-not [string]::IsNullOrWhiteSpace($EnvironmentRelativePath)) {
        [void]$paths.Add($EnvironmentRelativePath.Replace('\', '/'))
    }

    $indexEntries = if ($null -ne $IndexEntries) {
        @($IndexEntries)
    }
    else { @(Get-R0GovernanceIndexEntries -RepoRoot $RepoRoot) }
    $tracked = Get-R0GovernanceTrackedPaths -RepoRoot $RepoRoot -IndexEntries $indexEntries
    # The SPDX scan reads every tracked regular blob, so the report input
    # closure includes that exact collection.
    foreach ($path in @($tracked)) {
        if ($path -cne 'docs/quality/license-provenance-conformance-report.json') {
            [void]$paths.Add($path)
        }
    }

    $inventory = Get-R0GovernanceIndexJsonObject -IndexEntries $indexEntries `
        -RelativePath 'docs/governance/provenance-inventory.json'
    foreach ($item in @(Get-R0GovernanceField $inventory 'items')) {
        foreach ($ref in @(
                @(Get-R0GovernanceField $item 'source_refs') +
                @(Get-R0GovernanceField (Get-R0GovernanceField $item 'license') 'evidence_refs'))) {
            $value = [string]$ref
            if (Test-R0GovernanceLocatorSyntax $value -and $tracked.Contains($value)) {
                [void]$paths.Add($value)
            }
        }
    }

    foreach ($sidecarRelativePath in @($tracked | Where-Object {
                $_ -cmatch '^fixtures/[^/]+/scientific-context\.json$'
            })) {
        $sidecar = Get-R0GovernanceIndexJsonObject -IndexEntries $indexEntries `
            -RelativePath $sidecarRelativePath
        foreach ($source in @(Get-R0GovernanceField $sidecar 'source_records')) {
            if ((Get-R0GovernanceField $source 'availability') -ceq 'repository-tracked') {
                [void]$paths.Add([string](Get-R0GovernanceField $source 'locator'))
            }
        }
        $comparison = Get-R0GovernanceField $sidecar 'comparison'
        foreach ($field in @('expected_ref','actual_report_ref','comparator_ref','tolerance_ref')) {
            [void]$paths.Add([string](Get-R0GovernanceField $comparison $field))
        }
        foreach ($lane in @(Get-R0GovernanceField $sidecar 'reference_lanes')) {
            foreach ($field in @('implementation_ref','result_ref')) {
                [void]$paths.Add([string](Get-R0GovernanceField $lane $field))
            }
            foreach ($ref in @(
                    @(Get-R0GovernanceField $lane 'shared_input_refs') +
                    @(Get-R0GovernanceField $lane 'basis_refs'))) {
                [void]$paths.Add([string]$ref)
            }
        }
    }

    [string[]]$result = @($paths | Where-Object {
            -not [string]::IsNullOrWhiteSpace($_) -and $tracked.Contains($_)
        })
    [System.Array]::Sort($result, [System.StringComparer]::Ordinal)
    return $result
}

function Get-R0GovernanceReportInputFacts {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [string]$EnvironmentRelativePath
    )
    $indexEntries = @(Get-R0GovernanceIndexEntries -RepoRoot $RepoRoot)
    $entryMap = Get-R0GovernanceIndexEntryMap -IndexEntries $indexEntries
    [string[]]$paths = @(Get-R0GovernanceReportInputPaths -RepoRoot $RepoRoot `
            -EnvironmentRelativePath $EnvironmentRelativePath -IndexEntries $indexEntries)
    $facts = [System.Collections.Generic.List[object]]::new()
    foreach ($path in $paths) {
        $entry = $entryMap[$path]
        [byte[]]$bytes = [byte[]](Get-R0GovernanceField $entry 'blob_bytes')
        $isBinary = [System.IO.Path]::GetExtension($path) -ceq '.zip'
        if ($isBinary) {
            $sha256 = Get-R0GovernanceSha256Hex $bytes
            $normalization = 'raw-bytes'
        }
        else {
            $offset = if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and
                $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) { 3 } else { 0 }
            $text = [System.Text.UTF8Encoding]::new($false, $true).GetString(
                $bytes, $offset, $bytes.Length - $offset)
            $normalized = $text.Replace("`r`n", "`n").Replace("`r", "`n")
            $sha256 = Get-R0GovernanceSha256Hex (
                [System.Text.UTF8Encoding]::new($false).GetBytes($normalized))
            $normalization = 'utf8-lf-no-bom'
        }
        $facts.Add([pscustomobject][ordered]@{
                path = $path
                sha256 = $sha256
                hash_normalization = $normalization
                stage_object_id = [string](Get-R0GovernanceField $entry 'object_id')
                stage_blob_byte_length = [long](Get-R0GovernanceField $entry 'blob_byte_length')
            })
    }
    return $facts.ToArray()
}

function Get-R0GovernancePathSetSha256 {
    param([string[]]$Paths)
    [string[]]$ordered = @($Paths)
    [System.Array]::Sort($ordered, [System.StringComparer]::Ordinal)
    $manifest = ($ordered -join "`n") + "`n"
    return Get-R0GovernanceSha256Hex (
        [System.Text.UTF8Encoding]::new($false).GetBytes($manifest))
}

function Get-R0GovernanceNormalizedTextSha256 {
    param([string]$Text)
    $normalized = $Text.Replace("`r`n", "`n").Replace("`r", "`n")
    return Get-R0GovernanceSha256Hex (
        [System.Text.UTF8Encoding]::new($false).GetBytes($normalized))
}

function Get-R0GovernanceCanonicalObjectSha256 {
    param($Value)
    $convertToJsonParameters = @{
        Depth = 100
        Compress = $true
    }
    # Windows PowerShell 5.1 defaults to EscapeHtml; align PowerShell 7 explicitly.
    if ((Get-Command ConvertTo-Json).Parameters.ContainsKey('EscapeHandling')) {
        $convertToJsonParameters.EscapeHandling = 'EscapeHtml'
    }
    $json = $Value | ConvertTo-Json @convertToJsonParameters
    return Get-R0GovernanceSha256Hex (
        [System.Text.UTF8Encoding]::new($false).GetBytes($json))
}

function Test-R0GovernanceExactSequence {
    param(
        [object[]]$Actual,
        [string[]]$Expected,
        [string]$Label,
        [System.Collections.Generic.List[string]]$Issues
    )
    [string[]]$actualText = @($Actual | ForEach-Object { [string]$_ })
    if (($actualText -join "`n") -cne (@($Expected) -join "`n")) {
        Add-R0GovernanceIssue $Issues "$Label differs from the reviewed exact sequence."
    }
}

function Test-R0GovernanceTextBytes {
    param([string]$Path, [System.Collections.Generic.List[string]]$Issues, [string]$Label)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        Add-R0GovernanceIssue $Issues "$Label is missing."
        return
    }
    [byte[]]$bytes = [System.IO.File]::ReadAllBytes($Path)
    try {
        [void][System.Text.UTF8Encoding]::new($false, $true).GetString($bytes)
    }
    catch {
        Add-R0GovernanceIssue $Issues "$Label is not strict UTF-8."
        return
    }
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and
        $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        Add-R0GovernanceIssue $Issues "$Label has a UTF-8 BOM."
    }
    for ($index = 0; $index -lt $bytes.Length; ++$index) {
        if ($bytes[$index] -eq 0x0D) {
            Add-R0GovernanceIssue $Issues "$Label must use LF line endings."
            break
        }
    }
    if ($bytes.Length -eq 0 -or $bytes[$bytes.Length - 1] -ne 0x0A) {
        Add-R0GovernanceIssue $Issues "$Label must end with LF."
    }
}

function Test-R0GovernanceExactSet {
    param(
        [object[]]$Actual,
        [string[]]$Expected,
        [string]$Label,
        [System.Collections.Generic.List[string]]$Issues
    )
    [string[]]$actualText = @($Actual | ForEach-Object { [string]$_ })
    [string[]]$expectedText = @($Expected)
    [System.Array]::Sort($actualText, [System.StringComparer]::Ordinal)
    [System.Array]::Sort($expectedText, [System.StringComparer]::Ordinal)
    if (($actualText -join "`n") -cne ($expectedText -join "`n")) {
        Add-R0GovernanceIssue $Issues "$Label differs from the reviewed exact set."
    }
}

function Get-R0GovernanceReviewMutationNames {
    return @(
        'missing-scientific-context-schema',
        'scientific-context-schema-identity-drift',
        'missing-scientific-context-instance',
        'duplicate-scientific-context-id',
        'scientific-context-schema-version-drift',
        'scientific-context-maturity-drift',
        'manifest-missing-scientific-context-artifact',
        'fact-missing-scientific-context-evidence',
        'scientific-context-unknown-fixture',
        'scientific-context-unknown-claim',
        'scientific-context-missing-source',
        'scientific-context-local-source-hash-drift',
        'scientific-context-local-source-byte-drift',
        'scientific-context-normalized-hash-without-canonicalizer',
        'scientific-context-unavailable-expected-evidence',
        'scientific-context-missing-units-domain',
        'scientific-context-missing-frames-domain',
        'scientific-context-missing-time-domain',
        'scientific-context-missing-assumption',
        'scientific-context-missing-comparison-ref',
        'scientific-context-product-runtime-dependency',
        'scientific-context-legacy-dependency',
        'scientific-context-cross-lane-dependency',
        'scientific-context-confirmed-implementation-without-basis',
        'scientific-context-false-source-independence',
        'scientific-context-unknown-rights-item',
        'scientific-context-unknown-lineage-parent',
        'scientific-context-unrelated-legacy-lineage',
        'scientific-context-distribution-downgrade',
        'scientific-context-rights-effect-drift',
        'scientific-context-runtime-consumer',
        'aggregate-inventory-independence-overlay',
        'protected-v1-schema-overlay',
        'product-consumes-scientific-context',
        'scientific-context-authority-actor-drift',
        'scientific-context-authority-task-drift',
        'scientific-context-decision-id-drift',
        'scientific-context-premature-scientific-acceptance',
        'technical-inventory-contract-drift',
        'accepted-state-contract-drift',
        'scientific-context-contract-drift',
        'governance-contract-boundary-drift',
        'scientific-context-extra-source',
        'scientific-context-comparison-ref-drift',
        'scientific-context-lane-ref-drift',
        'scientific-context-reviewer-role-drift',
        'executable-scientific-fixture-without-sidecar',
        'governance-task-premature-done',
        'governance-task-evidence-drift',
        'science-governance-dependency-premature',
        'adr-policy-premature-accepted',
        'reconciliation-premature-disposition',
        'work-package-premature-done',
        'gate-premature-advance',
        'future-stage-premature-advance',
        'readiness-criterion-premature-close',
        'science-task-authority-drift',
        'science-work-package-authority-drift',
        'ctest-governance-hook-missing',
        'repository-governance-hook-missing',
        'license-status-grant-contradiction',
        'readme-repository-license-grant',
        'adr-license-selection-contradiction',
        'policy-rights-contradiction',
        'handoff-state-contradiction',
        'checklist-rights-contradiction',
        'reconciliation-rights-contradiction',
        'readiness-verdict-premature-ready',
        'readiness-g1-premature-ready',
        'readiness-provenance-premature-closed',
        'readiness-official-gate-passed',
        'readiness-governance-task-premature-done',
        'readiness-science-task-premature-done',
        'gate-work-package-premature-done',
        'gate-work-package-owner-drift',
        'gate-work-package-count-contradiction',
        'gate-work-package-premature-qualified',
        'gate-work-package-g0-passed-append',
        'governance-work-package-owner-drift',
        'governance-work-package-count-contradiction',
        'science-work-package-premature-done',
        'gate-backlog-assignee-premature',
        'future-stage-assignee-premature',
        'ctest-governance-hook-disabled',
        'repository-governance-hook-dead-branch',
        'workflow-ps51-governance-hook-missing',
        'premature-adr8-disposition-any-date',
        'premature-recon005-disposition-any-date',
        'premature-governance-task-acceptance-any-date',
        'premature-gate-decision-record',
        'report-input-path-dropped',
        'report-input-path-extra',
        'report-input-policy-count-drift',
        'candidate-document-identity-drift',
        'consumer-detector-identifier-form-drift',
        'host-toolchain-report-identity-drift',
        'scientific-context-candidate-distribution-mismatch',
        'repository-grant-detector-drift',
        'repository-grant-suffixed-path-drift',
        'repository-grant-json-metadata-drift',
        'repository-grant-toml-metadata-drift',
        'repository-grant-xml-metadata-drift',
        'repository-grant-yaml-metadata-drift',
        'repository-grant-gemspec-metadata-drift',
        'repository-grant-csproj-metadata-drift',
        'repository-grant-ini-metadata-drift',
        'repository-grant-nested-license-drift',
        'repository-grant-nested-notice-drift',
        'repository-grant-source-spdx-drift',
        'repository-grant-invalid-utf8-source-spdx-drift',
        'repository-grant-raw-spdx-probe-drift',
        'repository-grant-utf16le-source-spdx-drift',
        'repository-grant-utf16be-source-spdx-drift',
        'repository-grant-unreviewed-text-encoding-drift',
        'repository-grant-utf32le-text-drift',
        'repository-grant-utf32be-text-drift',
        'repository-grant-nul-text-drift',
        'repository-grant-c0-text-drift',
        'reviewed-binary-arbitrary-flag-drift',
        'reviewed-binary-wrong-path-drift',
        'reviewed-binary-wrong-sha-drift',
        'reviewed-binary-wrong-byte-length-drift',
        'reviewed-binary-wrong-object-id-drift',
        'index-nested-license-symlink-drift',
        'index-arbitrary-symlink-drift',
        'index-gitlink-drift',
        'index-unmerged-stage1-drift',
        'index-unmerged-stage2-drift',
        'index-unmerged-stage3-drift',
        'index-duplicate-multistage-drift',
        'index-missing-object-drift',
        'index-nonblob-object-drift',
        'index-worktree-mismatch-drift',
        'index-case-alias-drift',
        'index-skip-worktree-flag-drift',
        'index-assume-unchanged-flag-drift',
        'index-fsmonitor-flag-drift',
        'index-intent-to-add-flag-drift',
        'index-flag-entry-missing-drift',
        'index-content-filter-attribute-drift',
        'index-working-tree-encoding-attribute-drift',
        'index-ident-attribute-drift',
        'index-replacement-object-byte-drift',
        'report-input-index-blob-byte-drift',
        'index-intent-to-add-probe-drift',
        'index-content-filter-probe-drift',
        'index-replacement-object-probe-drift',
        'index-worktree-object-id-drift',
        'index-worktree-reparse-drift',
        'index-worktree-ancestor-reparse-drift',
        'embedded-scientific-report-hash-drift',
        'embedded-team-report-hash-drift',
        'embedded-workflow-hash-drift',
        'provenance-inventory-duplicate-json-key',
        'provenance-inventory-escaped-duplicate-json-key',
        'provenance-inventory-utf8-bom',
        'provenance-inventory-crlf',
        'provenance-inventory-invalid-utf8',
        'governance-contract-duplicate-json-key',
        'governance-contract-escaped-duplicate-json-key',
        'governance-contract-utf8-bom',
        'governance-contract-crlf',
        'governance-contract-invalid-utf8',
        'scientific-context-schema-duplicate-json-key',
        'scientific-context-schema-escaped-duplicate-json-key',
        'scientific-context-schema-utf8-bom',
        'scientific-context-schema-crlf',
        'scientific-context-schema-invalid-utf8',
        'scientific-context-duplicate-json-key',
        'scientific-context-escaped-duplicate-json-key',
        'scientific-context-utf8-bom',
        'scientific-context-crlf',
        'scientific-context-invalid-utf8')
}

function Get-R0GovernanceReviewContext {
    param([string]$RepoRoot)
    $indexEntries = @(Get-R0GovernanceIndexEntries -RepoRoot $RepoRoot)
    $indexEntryMap = Get-R0GovernanceIndexEntryMap -IndexEntries $indexEntries
    $tracked = Get-R0GovernanceTrackedPaths -RepoRoot $RepoRoot -IndexEntries $indexEntries
    $trackedObjectIds = Get-R0GovernanceTrackedBlobObjectIds -RepoRoot $RepoRoot `
        -IndexEntries $indexEntries
    $indexFlagTags = Get-R0GovernanceIndexFlagTags -RepoRoot $RepoRoot
    $indexDebugFlags = Get-R0GovernanceIndexDebugFlags -RepoRoot $RepoRoot
    [string[]]$trackedPathArray = @($tracked)
    [System.Array]::Sort($trackedPathArray, [System.StringComparer]::Ordinal)
    $indexFilterAttributes = Get-R0GovernanceIndexFilterAttributes -RepoRoot $RepoRoot `
        -Paths $trackedPathArray
    $worktreeObjectFacts = @(Get-R0GovernanceWorktreeObjectFacts -RepoRoot $RepoRoot `
            -IndexEntries $indexEntries)
    $sidecars = [System.Collections.Generic.List[object]]::new()
    [string[]]$sidecarPaths = @($tracked | Where-Object {
                $_ -cmatch '^fixtures/[^/]+/scientific-context\.json$'
            })
    [System.Array]::Sort($sidecarPaths, [System.StringComparer]::Ordinal)
    foreach ($path in $sidecarPaths) {
        $sidecars.Add((Get-R0GovernanceIndexJsonObject -IndexEntries $indexEntries `
                    -RelativePath $path))
    }
    $consumerDocuments = [System.Collections.Generic.List[object]]::new()
    [string[]]$consumerPaths = @($tracked | Where-Object {
                $_ -cmatch '^(?:framework|packages|adapters|apps|user|specs|cmake|\.github)/' -or
                $_ -ceq 'CMakeLists.txt'
            })
    [System.Array]::Sort($consumerPaths, [System.StringComparer]::Ordinal)
    foreach ($path in $consumerPaths) {
        $text = Get-R0GovernanceIndexBlobText -IndexEntries $indexEntries `
            -RelativePath ([string]$path)
        $consumerDocuments.Add([pscustomobject][ordered]@{
                path = ([string]$path).Replace('\', '/')
                text = $text
            })
    }
    $grantSurfaceDocuments = [System.Collections.Generic.List[object]]::new()
    [string[]]$grantScanPaths = @($tracked | Where-Object {
            $_ -cne 'docs/quality/license-provenance-conformance-report.json'
        })
    [System.Array]::Sort($grantScanPaths, [System.StringComparer]::Ordinal)
    foreach ($path in $grantScanPaths) {
        $entry = $indexEntryMap[$path]
        $grantSurfaceDocuments.Add((New-R0GovernanceScanDocument -Path $path `
                    -Bytes ([byte[]](Get-R0GovernanceField $entry 'blob_bytes')) `
                    -StageObjectId ([string](Get-R0GovernanceField $entry 'object_id'))))
    }
    $pointer = Get-R0GovernanceIndexJsonObject -IndexEntries $indexEntries `
        -RelativePath 'reference/legacy/reproduction/current.json'
    $environmentRelativePath = ([string](Get-R0GovernanceField $pointer 'path')).Replace('\', '/') +
        '/environment-manifest.json'
    $reportInputPaths = Get-R0GovernanceReportInputPaths -RepoRoot $RepoRoot `
        -EnvironmentRelativePath $environmentRelativePath -IndexEntries $indexEntries

    $blobFactPaths = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::Ordinal)
    foreach ($sidecar in @($sidecars)) {
        foreach ($source in @(Get-R0GovernanceField $sidecar 'source_records')) {
            if ((Get-R0GovernanceField $source 'availability') -ceq 'repository-tracked') {
                [void]$blobFactPaths.Add([string](Get-R0GovernanceField $source 'locator'))
            }
        }
        $comparison = Get-R0GovernanceField $sidecar 'comparison'
        foreach ($field in @('expected_ref','actual_report_ref','comparator_ref','tolerance_ref')) {
            [void]$blobFactPaths.Add([string](Get-R0GovernanceField $comparison $field))
        }
        foreach ($lane in @(Get-R0GovernanceField $sidecar 'reference_lanes')) {
            foreach ($field in @('implementation_ref','result_ref')) {
                [void]$blobFactPaths.Add([string](Get-R0GovernanceField $lane $field))
            }
            foreach ($ref in @(
                    @(Get-R0GovernanceField $lane 'shared_input_refs') +
                    @(Get-R0GovernanceField $lane 'basis_refs'))) {
                [void]$blobFactPaths.Add([string]$ref)
            }
        }
    }
    [void]$blobFactPaths.Add('docs/quality/scientific-conventions-cross-tool-report.json')
    [void]$blobFactPaths.Add('docs/quality/team-toolchain-readiness-report.json')
    [void]$blobFactPaths.Add('.github/workflows/ci.yml')
    $blobFacts = [ordered]@{}
    foreach ($path in @($blobFactPaths)) {
        $entry = if ($indexEntryMap.ContainsKey($path)) { $indexEntryMap[$path] } else { $null }
        $worktreeFact = @($worktreeObjectFacts | Where-Object {
                (Get-R0GovernanceField $_ 'path') -ceq $path
            }) | Select-Object -First 1
        $valid = $null -ne $entry -and $null -ne $worktreeFact -and
            (Get-R0GovernanceField $entry 'mode') -cin @('100644','100755') -and
            (Get-R0GovernanceField $entry 'stage') -eq 0 -and
            (Get-R0GovernanceField $entry 'object_type') -ceq 'blob' -and
            (Get-R0GovernanceField $entry 'blob_byte_length') -gt 0 -and
            (Get-R0GovernanceField $worktreeFact 'matches_index') -eq $true
        $blobFacts[$path] = [pscustomobject][ordered]@{
            valid = $valid
            mode = if ($null -ne $entry) { [string]$entry.mode } else { '' }
            object_id = if ($null -ne $entry) { [string]$entry.object_id } else { '' }
            blob_bytes = if ($null -ne $entry) { [long]$entry.blob_byte_length } else { [long]0 }
            blob_sha256 = if ($null -ne $entry) { [string]$entry.blob_sha256 } else { '' }
            worktree_bytes = if ($null -ne $worktreeFact) {
                [long](Get-R0GovernanceField $worktreeFact 'worktree_byte_length')
            } else { [long]0 }
            worktree_object_id = if ($null -ne $worktreeFact) {
                [string](Get-R0GovernanceField $worktreeFact 'worktree_object_id')
            } else { '' }
        }
    }

    $candidateDocuments = [System.Collections.Generic.List[object]]::new()
    foreach ($relativePath in @(
            'LICENSE-STATUS.md',
            'README.md',
            'CONTRIBUTING.md',
            'docs/adr/0008-internal-default-license-and-provenance-gate.md',
            'docs/governance/license-and-provenance-policy.md',
            'docs/handoff/r0-execution-state.md',
            'docs/quality/provenance-review-checklist.md',
            'docs/quality/r0-first-wave-reconciliation-audit.md',
            'docs/quality/r0-g0-g1-readiness-audit.md',
            'docs/tasks/backlog.json',
            'docs/tasks/work-packages/R0-GATE-001.md',
            'docs/tasks/work-packages/R0-GOV-002.md',
            'docs/tasks/work-packages/R0-SCI-001.md',
            'fixtures/ref-scientific-conventions/fixture-manifest.json',
            'project-manifest.json',
            '.github/workflows/ci.yml',
            'CMakeLists.txt',
            'tools/verify-repository.ps1')) {
        $candidateDocuments.Add([pscustomobject][ordered]@{
                path = $relativePath
                text = Get-R0GovernanceIndexBlobText -IndexEntries $indexEntries `
                    -RelativePath $relativePath
            })
    }

    $prematureRecordPaths = [System.Collections.Generic.List[string]]::new()
    foreach ($record in @(Get-ChildItem -LiteralPath (
                Join-Path $RepoRoot 'docs\governance\adr-dispositions') -File `
            -ErrorAction SilentlyContinue | Where-Object { $_.Name -cmatch '^ADR-0008-.+\.json$' })) {
        $prematureRecordPaths.Add($record.FullName.Substring(
                $RepoRoot.TrimEnd('\', '/').Length + 1).Replace('\', '/'))
    }
    foreach ($record in @(Get-ChildItem -LiteralPath (
                Join-Path $RepoRoot 'docs\governance\reconciliation-dispositions') -File `
            -ErrorAction SilentlyContinue | Where-Object { $_.Name -cmatch '^RECON-DEC-005-.+\.json$' })) {
        $prematureRecordPaths.Add($record.FullName.Substring(
                $RepoRoot.TrimEnd('\', '/').Length + 1).Replace('\', '/'))
    }
    foreach ($record in @(Get-ChildItem -LiteralPath (Join-Path $RepoRoot 'docs\quality') `
            -File -ErrorAction SilentlyContinue | Where-Object {
                $_.Name -cmatch '^task-acceptance-R0-GOV-002(?:-.+)?\.json$'
            })) {
        $prematureRecordPaths.Add($record.FullName.Substring(
                $RepoRoot.TrimEnd('\', '/').Length + 1).Replace('\', '/'))
    }
    foreach ($record in @(Get-ChildItem -LiteralPath (
                Join-Path $RepoRoot 'docs\quality\gate-decisions') -File `
            -ErrorAction SilentlyContinue | Where-Object { $_.Name -cmatch '^G[01]-.+' })) {
        $prematureRecordPaths.Add($record.FullName.Substring(
                $RepoRoot.TrimEnd('\', '/').Length + 1).Replace('\', '/'))
    }
    return [pscustomobject][ordered]@{
        repo_root = $RepoRoot
        tracked_paths = $tracked
        index_entries = @($indexEntries | ForEach-Object {
                [pscustomobject][ordered]@{
                    mode = [string](Get-R0GovernanceField $_ 'mode')
                    object_id = [string](Get-R0GovernanceField $_ 'object_id')
                    stage = [int](Get-R0GovernanceField $_ 'stage')
                    path = [string](Get-R0GovernanceField $_ 'path')
                    object_type = [string](Get-R0GovernanceField $_ 'object_type')
                    blob_byte_length = [long](Get-R0GovernanceField $_ 'blob_byte_length')
                    blob_sha256 = [string](Get-R0GovernanceField $_ 'blob_sha256')
                }
            })
        index_flag_tags = [pscustomobject]$indexFlagTags
        index_debug_flags = [pscustomobject]$indexDebugFlags
        index_filter_attributes = [pscustomobject]$indexFilterAttributes
        intent_to_add_probe_match = Test-R0GovernanceIntentToAddProbe
        content_filter_probe_match = Test-R0GovernanceContentFilterProbe
        replacement_object_probe_match = Test-R0GovernanceReplacementObjectProbe
        worktree_object_facts = @($worktreeObjectFacts)
        schema = Get-R0GovernanceIndexJsonObject -IndexEntries $indexEntries `
            -RelativePath 'docs/governance/schemas/scientific-context.schema.json'
        sidecars = @($sidecars)
        fixture_manifests = @($tracked | Where-Object {
                $_ -cmatch '^fixtures/[^/]+/fixture-manifest\.json$'
            } | ForEach-Object {
                [pscustomobject][ordered]@{
                    path = [string]$_
                    value = Get-R0GovernanceIndexJsonObject -IndexEntries $indexEntries `
                        -RelativePath ([string]$_)
                }
            })
        fixture = Get-R0GovernanceIndexJsonObject -IndexEntries $indexEntries -RelativePath 'fixtures/ref-scientific-conventions/fixture-manifest.json'
        inventory = Get-R0GovernanceIndexJsonObject -IndexEntries $indexEntries -RelativePath 'docs/governance/provenance-inventory.json'
        authorization = Get-R0GovernanceIndexJsonObject -IndexEntries $indexEntries -RelativePath 'docs/governance/r0-owner-authorization.json'
        roles = Get-R0GovernanceIndexJsonObject -IndexEntries $indexEntries -RelativePath 'docs/team/role-assignments.json'
        backlog = Get-R0GovernanceIndexJsonObject -IndexEntries $indexEntries -RelativePath 'docs/tasks/backlog.json'
        project_manifest = Get-R0GovernanceIndexJsonObject -IndexEntries $indexEntries -RelativePath 'project-manifest.json'
        contract = Get-R0GovernanceIndexJsonObject -IndexEntries $indexEntries -RelativePath 'docs/governance/r0-governance-review-contract.json'
        scientific_report = Get-R0GovernanceIndexJsonObject -IndexEntries $indexEntries -RelativePath 'docs/quality/scientific-conventions-cross-tool-report.json'
        adr_text = Get-R0GovernanceIndexBlobText -IndexEntries $indexEntries -RelativePath 'docs/adr/0008-internal-default-license-and-provenance-gate.md'
        policy_text = Get-R0GovernanceIndexBlobText -IndexEntries $indexEntries -RelativePath 'docs/governance/license-and-provenance-policy.md'
        work_package_text = Get-R0GovernanceIndexBlobText -IndexEntries $indexEntries -RelativePath 'docs/tasks/work-packages/R0-GOV-002.md'
        science_work_package_text = Get-R0GovernanceIndexBlobText -IndexEntries $indexEntries -RelativePath 'docs/tasks/work-packages/R0-SCI-001.md'
        reconciliation_audit_text = Get-R0GovernanceIndexBlobText -IndexEntries $indexEntries -RelativePath 'docs/quality/r0-first-wave-reconciliation-audit.md'
        readiness_text = Get-R0GovernanceIndexBlobText -IndexEntries $indexEntries -RelativePath 'docs/quality/r0-g0-g1-readiness-audit.md'
        gate_work_package_text = Get-R0GovernanceIndexBlobText -IndexEntries $indexEntries -RelativePath 'docs/tasks/work-packages/R0-GATE-001.md'
        cmake_text = Get-R0GovernanceIndexBlobText -IndexEntries $indexEntries -RelativePath 'CMakeLists.txt'
        repository_verifier_text = Get-R0GovernanceIndexBlobText -IndexEntries $indexEntries -RelativePath 'tools/verify-repository.ps1'
        license_status_text = Get-R0GovernanceIndexBlobText -IndexEntries $indexEntries -RelativePath 'LICENSE-STATUS.md'
        handoff_text = Get-R0GovernanceIndexBlobText -IndexEntries $indexEntries -RelativePath 'docs/handoff/r0-execution-state.md'
        candidate_documents = @($candidateDocuments)
        report_input_paths = @($reportInputPaths)
        premature_record_paths = @($prematureRecordPaths)
        grant_surface_documents = @($grantSurfaceDocuments)
        repository_grant_probe_match = Test-R0GovernanceRepositoryGrantText `
            'This repository is licensed under MIT.'
        raw_spdx_probe_match = Test-R0GovernanceRawSpdxSignal `
            ([System.Text.Encoding]::ASCII.GetBytes(
                    ('// SPDX-License-Identifier' + ': MIT')))
        raw_spdx_invalid_utf8_probe_match = Test-R0GovernanceRawSpdxSignal `
            ([byte[]](@(0xff) + @([System.Text.Encoding]::ASCII.GetBytes(
                            ('// SPDX-License-Identifier' + ': MIT')))))
        raw_spdx_utf16le_probe_match = Test-R0GovernanceRawSpdxSignal `
            ([System.Text.Encoding]::Unicode.GetBytes(
                    ('// SPDX-License-Identifier' + ': MIT')))
        raw_spdx_utf16be_probe_match = Test-R0GovernanceRawSpdxSignal `
            ([System.Text.Encoding]::BigEndianUnicode.GetBytes(
                    ('// SPDX-License-Identifier' + ': MIT')))
        recon_disposition_exists = @($prematureRecordPaths | Where-Object {
                $_ -cmatch '/RECON-DEC-005-'
            }).Count -gt 0
        task_acceptance_exists = @($prematureRecordPaths | Where-Object {
                $_ -cmatch '/task-acceptance-R0-GOV-002'
            }).Count -gt 0
        consumer_documents = @($consumerDocuments)
        consumer_probe_match = Test-R0GovernanceConsumerText '#include "scientific_context.hpp" ScientificContext'
        tracked_blob_facts = [pscustomobject]$blobFacts
        synthetic_candidate_document_path = 'README.md'
        synthetic_candidate_document_text = [string](@($candidateDocuments | Where-Object {
                    (Get-R0GovernanceField $_ 'path') -ceq 'README.md'
                })[0].text)
        protected_v1_hashes = [pscustomobject][ordered]@{
            'specs/fixture-manifest.schema.json' = (
                Get-R0GovernanceFileFact $RepoRoot 'specs/fixture-manifest.schema.json').sha256
            'specs/oracle-manifest.schema.json' = (
                Get-R0GovernanceFileFact $RepoRoot 'specs/oracle-manifest.schema.json').sha256
            'specs/plan-proof-record.schema.json' = (
                Get-R0GovernanceFileFact $RepoRoot 'specs/plan-proof-record.schema.json').sha256
        }
    }
}

function Get-R0GovernanceReviewIssues {
    param($Context)
    $issues = [System.Collections.Generic.List[string]]::new()
    $repoRoot = [string]$Context.repo_root
    $schema = $Context.schema
    $sidecars = @($Context.sidecars)
    $fixture = $Context.fixture
    $fixtureManifests = @($Context.fixture_manifests)
    $inventory = $Context.inventory
    $contract = $Context.contract

    if ((Get-R0GovernanceCanonicalObjectSha256 $contract) -cne
        'bb9aeb55d0cf4e5f8bfd1936f2c3d64f514330973517971cc7e7d7faa0e27500') {
        Add-R0GovernanceIssue $issues 'R0 governance technical contract semantic projection drifted.'
    }

    $documentIdentities = @(Get-R0GovernanceField $contract 'candidate_document_identities')
    Test-R0GovernanceExactSequence -Actual @($Context.candidate_documents | ForEach-Object {
            [string](Get-R0GovernanceField $_ 'path')
        }) -Expected @($documentIdentities | ForEach-Object {
            [string](Get-R0GovernanceField $_ 'path')
        }) -Label 'R0 governance candidate document paths' -Issues $issues
    foreach ($document in @($Context.candidate_documents)) {
        $path = [string](Get-R0GovernanceField $document 'path')
        $expected = @($documentIdentities | Where-Object {
                (Get-R0GovernanceField $_ 'path') -ceq $path
            }) | Select-Object -First 1
        if ($null -eq $expected -or
            (Get-R0GovernanceNormalizedTextSha256 (
                    [string](Get-R0GovernanceField $document 'text'))) -cne
                (Get-R0GovernanceField $expected 'normalized_sha256')) {
            Add-R0GovernanceIssue $issues "R0 governance candidate document '$path' reviewed text drifted."
        }
    }

    $indexPolicy = Get-R0GovernanceField $contract 'index_entry_policy'
    $indexEntries = @($Context.index_entries)
    [string[]]$indexPaths = @($indexEntries | ForEach-Object {
            [string](Get-R0GovernanceField $_ 'path')
        })
    [string[]]$sortedIndexPaths = @($indexPaths)
    [System.Array]::Sort($sortedIndexPaths, [System.StringComparer]::Ordinal)
    Test-R0GovernanceExactSequence -Actual $indexPaths -Expected $sortedIndexPaths `
        -Label 'R0 governance Git index paths' -Issues $issues
    $invalidIndexEntries = @($indexEntries | Where-Object {
            (Get-R0GovernanceField $_ 'mode') -cnotin @('100644','100755') -or
            (Get-R0GovernanceField $_ 'stage') -ne 0 -or
            (Get-R0GovernanceField $_ 'object_type') -cne 'blob' -or
            [string](Get-R0GovernanceField $_ 'object_id') -cnotmatch '^[0-9a-f]{40,64}$' -or
            [string]::IsNullOrEmpty([string](Get-R0GovernanceField $_ 'path'))
        })
    if ($invalidIndexEntries.Count -ne 0) {
        Add-R0GovernanceIssue $issues 'Git index contains a non-regular, non-blob, or non-stage-0 entry.'
    }
    if (@($indexPaths | Group-Object -CaseSensitive | Where-Object { $_.Count -ne 1 }).Count -ne 0) {
        Add-R0GovernanceIssue $issues 'Git index contains a duplicate or multi-stage path.'
    }
    $caseInsensitivePaths = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase)
    $caseAliasExists = $false
    foreach ($path in $indexPaths) {
        if (-not $caseInsensitivePaths.Add($path)) { $caseAliasExists = $true }
    }
    if ($caseAliasExists) {
        Add-R0GovernanceIssue $issues 'Git index contains a case-insensitive worktree path collision.'
    }
    $weakeningIndexTags = @($indexPaths | Where-Object {
            $tags = Get-R0GovernanceField $Context.index_flag_tags $_
            $debugFlag = [string](Get-R0GovernanceField $Context.index_debug_flags $_)
            $null -eq $tags -or
            (Get-R0GovernanceField $tags 'cached') -cne 'H' -or
            (Get-R0GovernanceField $tags 'assume_unchanged') -cne 'H' -or
            (Get-R0GovernanceField $tags 'fsmonitor') -cne 'H' -or
            $debugFlag -cne '0'
        })
    $indexFlagProperties = @($Context.index_flag_tags.PSObject.Properties)
    $indexDebugFlagProperties = @($Context.index_debug_flags.PSObject.Properties)
    if ($weakeningIndexTags.Count -ne 0 -or
        $indexFlagProperties.Count -ne $indexEntries.Count -or
        $indexDebugFlagProperties.Count -ne $indexEntries.Count) {
        Add-R0GovernanceIssue $issues 'Git index contains a weakening extended flag.'
    }
    $transformingAttributes = @($indexPaths | Where-Object {
            $attributes = Get-R0GovernanceField $Context.index_filter_attributes $_
            $null -eq $attributes -or
            [string](Get-R0GovernanceField $attributes 'filter') -cne 'unspecified' -or
            [string](Get-R0GovernanceField $attributes 'working-tree-encoding') -cne 'unspecified' -or
            [string](Get-R0GovernanceField $attributes 'ident') -cne 'unspecified'
        })
    if ($transformingAttributes.Count -ne 0 -or
        @($Context.index_filter_attributes.PSObject.Properties).Count -ne $indexEntries.Count) {
        Add-R0GovernanceIssue $issues 'Git index contains a content-transforming attribute.'
    }
    $requiredRealProbes = Get-R0GovernanceField $indexPolicy 'required_real_probes'
    if ((Get-R0GovernanceField $requiredRealProbes 'intent_to_add') -ne $true -or
        $Context.intent_to_add_probe_match -ne $true) {
        Add-R0GovernanceIssue $issues 'Git intent-to-add detector failed its real repository probe.'
    }
    if ((Get-R0GovernanceField $requiredRealProbes 'content_filter') -ne $true -or
        $Context.content_filter_probe_match -ne $true) {
        Add-R0GovernanceIssue $issues 'Git content-filter detector failed its real repository probe.'
    }
    if ((Get-R0GovernanceField $requiredRealProbes 'replacement_object') -ne $true -or
        $Context.replacement_object_probe_match -ne $true) {
        Add-R0GovernanceIssue $issues 'Exact Git object reader failed its replacement-ref probe.'
    }
    $worktreeFacts = @($Context.worktree_object_facts)
    Test-R0GovernanceExactSequence -Actual @($worktreeFacts | ForEach-Object {
            [string](Get-R0GovernanceField $_ 'path')
        }) -Expected $indexPaths -Label 'R0 governance worktree object paths' -Issues $issues
    $worktreeIdentityDrift = @($worktreeFacts | Where-Object {
            (Get-R0GovernanceField $_ 'regular_file') -ne $true -or
            (Get-R0GovernanceField $_ 'contained_in_repository') -ne $true -or
            (Get-R0GovernanceField $_ 'reparse_path') -eq $true -or
            (Get-R0GovernanceField $_ 'matches_index') -ne $true -or
            [string](Get-R0GovernanceField $_ 'worktree_object_id') -cne
                [string](Get-R0GovernanceField $_ 'index_object_id')
        })
    if ($worktreeIdentityDrift.Count -ne 0 -or $worktreeFacts.Count -ne $indexEntries.Count) {
        Add-R0GovernanceIssue $issues 'A worktree regular-file object differs from the reviewed Git index.'
    }
    if ((Get-R0GovernanceField $indexPolicy 'entry_count') -ne $indexEntries.Count -or
        ((Get-R0GovernanceField $indexPolicy 'allowed_modes') -join ',') -cne '100644,100755' -or
        (Get-R0GovernanceField $indexPolicy 'required_stage') -ne 0 -or
        (Get-R0GovernanceField $indexPolicy 'required_object_type') -cne 'blob' -or
        (Get-R0GovernanceField $indexPolicy 'unique_path_per_entry') -ne $true -or
        (Get-R0GovernanceField $indexPolicy 'unique_case_insensitive_worktree_path') -ne $true -or
        (Get-R0GovernanceField $indexPolicy 'required_index_flag_tag') -cne 'H' -or
        (Get-R0GovernanceField $indexPolicy 'required_debug_flags') -cne '0' -or
        (Get-R0GovernanceField $indexPolicy 'intent_to_add_allowed') -ne $false -or
        (Get-R0GovernanceField $indexPolicy 'content_transforming_attributes_allowed') -ne $false -or
        (Get-R0GovernanceField $indexPolicy 'object_read_semantics') -cne
            'git --no-replace-objects cat-file reads the exact index object bytes' -or
        (Get-R0GovernanceField $indexPolicy 'worktree_object_identity') -cne
            'git hash-object --path for every entry equals the exact index object id' -or
        (Get-R0GovernanceField $indexPolicy 'worktree_reparse_points_allowed') -ne $false -or
        (Get-R0GovernanceField $indexPolicy 'worktree_must_match_index') -ne $true -or
        (Get-R0GovernanceField $indexPolicy 'derived_report_exclusion') -cne
            'docs/quality/license-provenance-conformance-report.json') {
        Add-R0GovernanceIssue $issues 'R0 governance Git index policy differs from the reviewed projection.'
    }
    Test-R0GovernanceExactSet -Actual @($Context.tracked_paths) -Expected $indexPaths `
        -Label 'R0 governance tracked paths' -Issues $issues

    $inputPolicy = Get-R0GovernanceField $contract 'report_input_policy'
    [string[]]$reportInputPaths = @($Context.report_input_paths)
    if ((Get-R0GovernanceField $inputPolicy 'path_count') -ne $reportInputPaths.Count -or
        (Get-R0GovernanceField $inputPolicy 'ordinal_path_set_sha256') -cne
            (Get-R0GovernancePathSetSha256 $reportInputPaths)) {
        Add-R0GovernanceIssue $issues 'R0 governance report input path set differs from the reviewed closure.'
    }
    [string[]]$expectedReportInputPaths = @($indexPaths | Where-Object {
            $_ -cne 'docs/quality/license-provenance-conformance-report.json'
        })
    Test-R0GovernanceExactSequence -Actual $reportInputPaths -Expected $expectedReportInputPaths `
        -Label 'R0 governance report input paths' -Issues $issues
    Test-R0GovernanceExactSequence -Actual @($Context.grant_surface_documents | ForEach-Object {
            [string](Get-R0GovernanceField $_ 'path')
        }) -Expected $reportInputPaths -Label 'R0 governance grant and SPDX scan paths' -Issues $issues
    $scanByteDrift = @($Context.grant_surface_documents | Where-Object {
            $path = [string](Get-R0GovernanceField $_ 'path')
            $entry = @($indexEntries | Where-Object {
                    [string](Get-R0GovernanceField $_ 'path') -ceq $path
                }) | Select-Object -First 1
            $null -eq $entry -or
            [long](Get-R0GovernanceField $_ 'byte_length') -ne
                [long](Get-R0GovernanceField $entry 'blob_byte_length') -or
            [string](Get-R0GovernanceField $_ 'raw_sha256') -cne
                [string](Get-R0GovernanceField $entry 'blob_sha256') -or
            [string](Get-R0GovernanceField $_ 'stage_object_id') -cne
                [string](Get-R0GovernanceField $entry 'object_id')
        })
    if ($scanByteDrift.Count -ne 0) {
        Add-R0GovernanceIssue $issues 'Grant and SPDX scan bytes differ from the exact stage-0 Git blob.'
    }
    if (@($Context.premature_record_paths).Count -ne 0) {
        Add-R0GovernanceIssue $issues 'A premature ADR-0008, RECON-DEC-005, R0-GOV-002 acceptance, or G0/G1 decision record exists.'
    }
    foreach ($embeddedPath in @(
            'docs/quality/scientific-conventions-cross-tool-report.json',
            'docs/quality/team-toolchain-readiness-report.json',
            '.github/workflows/ci.yml')) {
        $fact = Get-R0GovernanceCachedBlobFact -Context $Context -RelativePath $embeddedPath
        if (-not $fact.valid) {
            Add-R0GovernanceIssue $issues "Embedded authority '$embeddedPath' is not a reviewed regular stage-0 Git blob."
            continue
        }
        foreach ($statePath in @(
                'docs/quality/r0-g0-g1-readiness-audit.md',
                'docs/tasks/work-packages/R0-GATE-001.md')) {
            $stateDocument = @($Context.candidate_documents | Where-Object {
                    (Get-R0GovernanceField $_ 'path') -ceq $statePath
                }) | Select-Object -First 1
            if ($null -eq $stateDocument -or
                -not ([string](Get-R0GovernanceField $stateDocument 'text')).Contains(
                    [string]$fact.blob_sha256)) {
                Add-R0GovernanceIssue $issues "State document '$statePath' embedded Git-blob identity for '$embeddedPath' drifted."
            }
        }
    }
    $binaryIdentityDrift = @($Context.grant_surface_documents | Where-Object {
            $path = [string](Get-R0GovernanceField $_ 'path')
            $expectedReviewedBinary = Test-R0GovernanceReviewedBinaryIdentity -Path $path `
                -ByteLength ([long](Get-R0GovernanceField $_ 'byte_length')) `
                -RawSha256 ([string](Get-R0GovernanceField $_ 'raw_sha256')) `
                -StageObjectId ([string](Get-R0GovernanceField $_ 'stage_object_id'))
            ((Get-R0GovernanceField $_ 'reviewed_binary') -eq $true) -ne $expectedReviewedBinary
        })
    if ($binaryIdentityDrift.Count -ne 0 -or
        @($Context.grant_surface_documents | Where-Object {
                (Get-R0GovernanceField $_ 'reviewed_binary') -eq $true
            }).Count -ne 1) {
        Add-R0GovernanceIssue $issues 'The reviewed binary exception identity drifted.'
    }
    $grantSignals = @($Context.grant_surface_documents | Where-Object {
            $path = [string](Get-R0GovernanceField $_ 'path')
            $decodeError = (Get-R0GovernanceField $_ 'decode_error') -eq $true
            ((Test-R0GovernanceGrantSurfacePath $path) -and (
                    $decodeError -or
                    (Test-R0GovernanceImplicitGrantPath $path) -or
                    (Get-R0GovernanceField $_ 'grant_text_signal') -eq $true)) -or
                (Get-R0GovernanceField $_ 'spdx_signal') -eq $true
        })
    if ($grantSignals.Count -ne 0) {
        Add-R0GovernanceIssue $issues 'A repository license-grant signal exists while the distribution license remains unselected.'
    }
    $unreviewedTextEncodings = @($Context.grant_surface_documents | Where-Object {
            ((Get-R0GovernanceField $_ 'decode_error') -eq $true -or
                (Get-R0GovernanceField $_ 'control_error') -eq $true) -and
            (Get-R0GovernanceField $_ 'reviewed_binary') -ne $true
        })
    if ($unreviewedTextEncodings.Count -ne 0) {
        Add-R0GovernanceIssue $issues 'A tracked non-binary blob is not canonical reviewable UTF-8 text.'
    }
    if ($Context.repository_grant_probe_match -ne $true) {
        Add-R0GovernanceIssue $issues 'The repository license-grant signal detector does not recognize the reviewed grant form.'
    }
    if ($Context.raw_spdx_probe_match -ne $true -or
        $Context.raw_spdx_invalid_utf8_probe_match -ne $true -or
        $Context.raw_spdx_utf16le_probe_match -ne $true -or
        $Context.raw_spdx_utf16be_probe_match -ne $true) {
        Add-R0GovernanceIssue $issues 'The raw-byte SPDX detector does not recognize the reviewed ASCII and UTF-16 forms.'
    }

    if ($null -eq $schema) {
        Add-R0GovernanceIssue $issues 'Scientific context schema is missing.'
    }
    else {
        if ((Get-R0GovernanceField $schema '$id') -cne
            'https://internal.gnczmkn/schemas/scientific-context/1') {
            Add-R0GovernanceIssue $issues 'Scientific context schema identity drifted.'
        }
        if ((Get-R0GovernanceField $schema 'x-maturity') -cne
            'governance-evidence-no-runtime-consumer') {
            Add-R0GovernanceIssue $issues 'Scientific context schema maturity drifted.'
        }
    }
    if ($sidecars.Count -eq 0) {
        Add-R0GovernanceIssue $issues 'Scientific context instance is missing.'
        return $issues.ToArray()
    }
    foreach ($fixtureRecord in $fixtureManifests) {
        $manifest = $fixtureRecord.value
        if ((Get-R0GovernanceField $manifest 'status') -cne 'executable') { continue }
        $manifestPath = [string]$fixtureRecord.path
        $directory = [System.IO.Path]::GetDirectoryName($manifestPath).Replace('\', '/')
        $expectedContextPath = $directory + '/scientific-context.json'
        $matchingContext = @($sidecars | Where-Object {
                (Get-R0GovernanceField (Get-R0GovernanceField $_ 'subject') 'fixture_id') -ceq
                    (Get-R0GovernanceField $manifest 'fixture_id')
            })
        if ($matchingContext.Count -ne 1 -or
            $expectedContextPath -cnotin @(Get-R0GovernanceField $manifest 'required_artifacts') -or
            @((Get-R0GovernanceField $manifest 'expected_facts') | Where-Object {
                    $expectedContextPath -cnotin @(Get-R0GovernanceField $_ 'evidence_refs')
                }).Count -gt 0) {
            Add-R0GovernanceIssue $issues 'An executable scientific fixture lacks one bound scientific context sidecar.'
        }
    }
    $contextIds = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::Ordinal)
    foreach ($sidecar in $sidecars) {
        $contextId = [string](Get-R0GovernanceField $sidecar 'context_id')
        if (-not $contextIds.Add($contextId)) {
            Add-R0GovernanceIssue $issues "Duplicate scientific context id '$contextId'."
        }
    }
    if ($sidecars.Count -ne 1) {
        Add-R0GovernanceIssue $issues 'Scientific context instance set differs from the reviewed R0 set.'
    }
    $sidecar = $sidecars[0]
    if ((Get-R0GovernanceCanonicalObjectSha256 $sidecar) -cne
        'f6906cfc388af693b5ddaa6403cf916800b4f0eef25370c5e27e8d36d3bd3351') {
        Add-R0GovernanceIssue $issues 'Scientific context reviewed semantic projection drifted.'
    }
    if ((Get-R0GovernanceField $sidecar 'schema_version') -cne
        'gnczmkn.scientific-context/1') {
        Add-R0GovernanceIssue $issues 'Scientific context instance schema version drifted.'
    }
    if ((Get-R0GovernanceField $sidecar 'maturity') -cne
        'governance-evidence-no-runtime-consumer') {
        Add-R0GovernanceIssue $issues 'Scientific context instance maturity drifted.'
    }
    if ((Get-R0GovernanceField $sidecar 'review_state') -cne
        'registered-pending-scientific-acceptance') {
        Add-R0GovernanceIssue $issues 'Scientific context review state advanced without SCI task acceptance.'
    }

    $subject = Get-R0GovernanceField $sidecar 'subject'
    $fixtureId = [string](Get-R0GovernanceField $fixture 'fixture_id')
    if ((Get-R0GovernanceField $subject 'fixture_id') -cne $fixtureId -or
        $fixtureId -cne 'REF-SCIENTIFIC-CONVENTIONS-001') {
        Add-R0GovernanceIssue $issues 'Scientific context fixture identity is unknown or mismatched.'
    }
    [string[]]$fixtureClaims = @((Get-R0GovernanceField $fixture 'expected_facts') |
        ForEach-Object { [string](Get-R0GovernanceField $_ 'id') })
    Test-R0GovernanceExactSet -Actual @(Get-R0GovernanceField $subject 'claim_refs') `
        -Expected $fixtureClaims -Label 'Scientific context claim references' -Issues $issues

    $sidecarPath = 'fixtures/ref-scientific-conventions/scientific-context.json'
    if ($sidecarPath -cnotin @(Get-R0GovernanceField $fixture 'required_artifacts')) {
        Add-R0GovernanceIssue $issues 'Fixture manifest does not require the scientific context sidecar.'
    }
    foreach ($fact in @(Get-R0GovernanceField $fixture 'expected_facts')) {
        if ($sidecarPath -cnotin @(Get-R0GovernanceField $fact 'evidence_refs')) {
            Add-R0GovernanceIssue $issues (
                "Fixture fact '$([string](Get-R0GovernanceField $fact 'id'))' does not reference the scientific context sidecar.")
        }
    }

    $inventoryIds = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::Ordinal)
    foreach ($item in @(Get-R0GovernanceField $inventory 'items')) {
        [void]$inventoryIds.Add([string](Get-R0GovernanceField $item 'id'))
        foreach ($forbidden in @('scientific_context', 'independent_reference_confirmed')) {
            if (Test-R0GovernanceHasField $item $forbidden) {
                Add-R0GovernanceIssue $issues 'Aggregate inventory contains a forbidden scientific independence overlay.'
            }
        }
    }
    foreach ($forbidden in @('scientific_context', 'independent_reference_confirmed')) {
        if (Test-R0GovernanceHasField $inventory $forbidden) {
            Add-R0GovernanceIssue $issues 'Aggregate inventory contains a forbidden scientific independence overlay.'
        }
    }
    $hostToolchain = @((Get-R0GovernanceField $inventory 'items') | Where-Object {
            (Get-R0GovernanceField $_ 'id') -ceq 'host-validation-toolchain'
        }) | Select-Object -First 1
    $reportedEnvironment = Get-R0GovernanceField $Context.scientific_report 'environment'
    [string[]]$reportedToolIdentities = @(
        [string](Get-R0GovernanceField $reportedEnvironment 'cpp_runtime'),
        [string](Get-R0GovernanceField $reportedEnvironment 'python_runtime'),
        ('Windows PowerShell ' + [string](Get-R0GovernanceField $reportedEnvironment 'powershell')))
    $hostScope = @(Get-R0GovernanceField (
            Get-R0GovernanceField $hostToolchain 'scope') 'include')
    if ($null -eq $hostToolchain -or @($reportedToolIdentities | Where-Object {
                $_ -cnotin $hostScope
            }).Count -gt 0) {
        Add-R0GovernanceIssue $issues 'Scientific report tool identities differ from the host-validation-toolchain inventory scope.'
    }

    $sources = @(Get-R0GovernanceField $sidecar 'source_records')
    if ($sources.Count -eq 0) {
        Add-R0GovernanceIssue $issues 'Scientific context has no source records.'
    }
    $sourceIds = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::Ordinal)
    Test-R0GovernanceExactSequence -Actual @($sources | ForEach-Object {
            [string](Get-R0GovernanceField $_ 'source_id')
        }) -Expected @(
            'adr-0006',
            'adr-0007',
            'architecture-math',
            'architecture-time',
            'conventions-input',
            'cases-input') -Label 'Scientific context source ids' -Issues $issues
    foreach ($source in $sources) {
        $sourceId = [string](Get-R0GovernanceField $source 'source_id')
        if (-not $sourceIds.Add($sourceId)) {
            Add-R0GovernanceIssue $issues "Duplicate scientific context source id '$sourceId'."
        }
        foreach ($rightsRef in @(Get-R0GovernanceField $source 'rights_inventory_refs')) {
            if (-not $inventoryIds.Contains([string]$rightsRef)) {
                Add-R0GovernanceIssue $issues "Scientific context source '$sourceId' has an unknown rights inventory reference."
            }
        }
        $availability = [string](Get-R0GovernanceField $source 'availability')
        $locator = [string](Get-R0GovernanceField $source 'locator')
        $integrity = Get-R0GovernanceField $source 'integrity'
        if ($availability -eq 'repository-tracked') {
            if (-not (Test-R0GovernanceLocatorSyntax $locator) -or
                -not $Context.tracked_paths.Contains($locator)) {
                Add-R0GovernanceIssue $issues "Scientific context source '$sourceId' is not an exact tracked repository locator."
                continue
            }
            $fact = Get-R0GovernanceCachedBlobFact -Context $Context -RelativePath $locator
            if (-not $fact.valid) {
                Add-R0GovernanceIssue $issues "Scientific context source '$sourceId' is not a nonempty regular Git blob."
            }
            if (-not $fact.valid -or
                (Get-R0GovernanceField $integrity 'raw_sha256') -cne $fact.blob_sha256) {
                Add-R0GovernanceIssue $issues "Scientific context source '$sourceId' raw SHA-256 drifted."
            }
            if (-not $fact.valid -or
                (Get-R0GovernanceField $integrity 'byte_length') -ne $fact.blob_bytes) {
                Add-R0GovernanceIssue $issues "Scientific context source '$sourceId' raw byte length drifted."
            }
        }
        if ((Test-R0GovernanceHasField $integrity 'normalized_sha256') -and
            (-not (Test-R0GovernanceHasField $integrity 'canonicalizer'))) {
            Add-R0GovernanceIssue $issues "Scientific context source '$sourceId' normalized hash lacks a canonicalizer."
        }
    }

    $validity = Get-R0GovernanceField $sidecar 'validity_domain'
    [string[]]$dimensionIds = @((Get-R0GovernanceField $validity 'semantic_dimensions') |
        ForEach-Object { [string](Get-R0GovernanceField $_ 'id') })
    foreach ($dimension in @('units', 'frames', 'time')) {
        if ($dimension -cnotin $dimensionIds) {
            Add-R0GovernanceIssue $issues "Scientific context validity domain is missing '$dimension'."
        }
    }
    if (@(Get-R0GovernanceField $validity 'assumptions').Count -eq 0) {
        Add-R0GovernanceIssue $issues 'Scientific context validity domain has no assumptions.'
    }
    if (@(Get-R0GovernanceField $validity 'exclusions').Count -eq 0) {
        Add-R0GovernanceIssue $issues 'Scientific context validity domain has no exclusions.'
    }

    $comparison = Get-R0GovernanceField $sidecar 'comparison'
    foreach ($field in @('expected_ref', 'actual_report_ref', 'comparator_ref', 'tolerance_ref')) {
        if ([string]::IsNullOrWhiteSpace([string](Get-R0GovernanceField $comparison $field))) {
            Add-R0GovernanceIssue $issues "Scientific context comparison is missing '$field'."
        }
    }
    $expectedComparison = [ordered]@{
        expected_ref = 'fixtures/ref-scientific-conventions/cases.json'
        actual_report_ref = 'docs/quality/scientific-conventions-cross-tool-report.json'
        comparator_ref = 'tools/validate-scientific-conventions.ps1'
        tolerance_ref = 'fixtures/ref-scientific-conventions/conventions.json'
    }
    foreach ($field in $expectedComparison.Keys) {
        $value = [string](Get-R0GovernanceField $comparison $field)
        $fact = Get-R0GovernanceCachedBlobFact -Context $Context -RelativePath $value
        if ($value -cne $expectedComparison[$field] -or -not $fact.valid) {
            Add-R0GovernanceIssue $issues 'Scientific context comparison references differ from the reviewed tracked set.'
            break
        }
    }
    $expectedRef = [string](Get-R0GovernanceField $comparison 'expected_ref')
    $expectedSource = @($sources | Where-Object {
            (Get-R0GovernanceField $_ 'locator') -ceq $expectedRef
        }) | Select-Object -First 1
    if ($null -ne $expectedSource -and
        (Get-R0GovernanceField $expectedSource 'availability') -ceq 'external-unavailable') {
        Add-R0GovernanceIssue $issues 'An unavailable source cannot be the executable expected evidence.'
    }

    $lanes = @(Get-R0GovernanceField $sidecar 'reference_lanes')
    if ($lanes.Count -ne 2) {
        Add-R0GovernanceIssue $issues 'Scientific context reference lane set differs from the reviewed two-lane fixture.'
    }
    $expectedLanes = [ordered]@{
        'cpp17-property-spike' = [ordered]@{
            implementation_ref = 'tests/scientific_conventions.cpp'
            result_ref = 'docs/quality/scientific-conventions-cross-tool-report.json'
            basis_refs = @(
                'tests/scientific_conventions.cpp',
                'tools/validate-scientific-conventions.ps1',
                'docs/quality/scientific-conventions-cross-tool-report.json')
        }
        'cpython-stdlib-reference' = [ordered]@{
            implementation_ref = 'tools/scientific_conventions_reference.py'
            result_ref = 'docs/quality/scientific-conventions-cross-tool-report.json'
            basis_refs = @(
                'tools/scientific_conventions_reference.py',
                'tools/validate-scientific-conventions.ps1',
                'docs/quality/scientific-conventions-cross-tool-report.json')
        }
    }
    Test-R0GovernanceExactSequence -Actual @($lanes | ForEach-Object {
            [string](Get-R0GovernanceField $_ 'lane_id')
        }) -Expected @($expectedLanes.Keys) -Label 'Scientific context lane ids' -Issues $issues
    foreach ($lane in $lanes) {
        $laneId = [string](Get-R0GovernanceField $lane 'lane_id')
        if (-not $expectedLanes.Contains($laneId)) { continue }
        $expectedLane = $expectedLanes[$laneId]
        foreach ($field in @('implementation_ref', 'result_ref')) {
            $value = [string](Get-R0GovernanceField $lane $field)
            $fact = Get-R0GovernanceCachedBlobFact -Context $Context -RelativePath $value
            if ($value -cne $expectedLane[$field] -or -not $fact.valid) {
                Add-R0GovernanceIssue $issues "Scientific context lane '$laneId' references differ from the reviewed tracked set."
            }
        }
        Test-R0GovernanceExactSequence -Actual @(Get-R0GovernanceField $lane 'shared_input_refs') `
            -Expected @(
                'fixtures/ref-scientific-conventions/conventions.json',
                'fixtures/ref-scientific-conventions/cases.json') `
            -Label "Scientific context lane '$laneId' shared inputs" -Issues $issues
        Test-R0GovernanceExactSequence -Actual @(Get-R0GovernanceField $lane 'basis_refs') `
            -Expected @($expectedLane.basis_refs) `
            -Label "Scientific context lane '$laneId' basis references" -Issues $issues
        foreach ($ref in @(
                @(Get-R0GovernanceField $lane 'shared_input_refs') +
                @(Get-R0GovernanceField $lane 'basis_refs'))) {
            $fact = Get-R0GovernanceCachedBlobFact -Context $Context -RelativePath ([string]$ref)
            if (-not $fact.valid) {
                Add-R0GovernanceIssue $issues "Scientific context lane '$laneId' has a non-blob or empty tracked reference."
            }
        }
    }
    foreach ($lane in $lanes) {
        $laneId = [string](Get-R0GovernanceField $lane 'lane_id')
        $boundary = Get-R0GovernanceField $lane 'dependency_boundary'
        if ((Get-R0GovernanceField $boundary 'product_runtime_excluded') -ne $true) {
            Add-R0GovernanceIssue $issues "Scientific context lane '$laneId' consumes product runtime."
        }
        if ((Get-R0GovernanceField $boundary 'legacy_excluded') -ne $true) {
            Add-R0GovernanceIssue $issues "Scientific context lane '$laneId' consumes Legacy."
        }
        if ((Get-R0GovernanceField $boundary 'other_reference_lane_excluded') -ne $true) {
            Add-R0GovernanceIssue $issues "Scientific context lane '$laneId' consumes another reference lane."
        }
        if ((Get-R0GovernanceField $lane 'implementation_independence') -eq 'confirmed' -and
            @(Get-R0GovernanceField $lane 'basis_refs').Count -eq 0) {
            Add-R0GovernanceIssue $issues "Scientific context lane '$laneId' confirms implementation independence without basis."
        }
        if ((Get-R0GovernanceField $lane 'scientific_source_independence') -ne 'not-claimed') {
            Add-R0GovernanceIssue $issues "Scientific context lane '$laneId' falsely claims scientific source independence."
        }
    }

    $provenance = Get-R0GovernanceField $sidecar 'provenance'
    if (-not $inventoryIds.Contains(
            [string](Get-R0GovernanceField $provenance 'rights_inventory_item_ref'))) {
        Add-R0GovernanceIssue $issues 'Scientific context has an unknown rights inventory item.'
    }
    [string[]]$lineage = @(Get-R0GovernanceField $provenance 'lineage_parent_item_refs')
    foreach ($parent in $lineage) {
        if (-not $inventoryIds.Contains($parent)) {
            Add-R0GovernanceIssue $issues "Scientific context has an unknown lineage parent '$parent'."
        }
    }
    foreach ($unrelated in @(
            'legacy-source-archive',
            'eigen-3.4.0-legacy-reproduction',
            'w64devkit-2.9.1-legacy-reproduction')) {
        if ($unrelated -cin $lineage) {
            Add-R0GovernanceIssue $issues 'Scientific context uses an unrelated Legacy reproduction lineage parent.'
        }
    }
    $researchEvidence = @((Get-R0GovernanceField $inventory 'items') | Where-Object {
            (Get-R0GovernanceField $_ 'id') -ceq 'r0-research-evidence'
        }) | Select-Object -First 1
    if ((Get-R0GovernanceField $sidecar 'external_distribution') -cne
        'blocked-pending-accepted-adr' -or
        (Get-R0GovernanceField $researchEvidence 'external_distribution') -cne
        'blocked-pending-accepted-adr') {
        Add-R0GovernanceIssue $issues 'Scientific context external distribution differs from the candidate inventory state.'
    }
    if ((Get-R0GovernanceField $sidecar 'rights_effect') -cne 'no-change') {
        Add-R0GovernanceIssue $issues 'Scientific context changes rights or export state.'
    }
    if ((Get-R0GovernanceField $sidecar 'runtime_consumers') -ne 0) {
        Add-R0GovernanceIssue $issues 'Scientific context has a runtime consumer.'
    }

    $authority = Get-R0GovernanceField $sidecar 'authority'
    $expectedAuthority = [ordered]@{
        owner_role = 'scientific_authority'
        owner_actor_id = 'r0-science-agent'
        owner_task_path = '/root/r0_science_agent'
        reviewer_role = 'architecture_lead'
        authorization_ref = 'docs/governance/r0-owner-authorization.json'
        decision_id = 'RECON-DEC-005'
    }
    foreach ($field in $expectedAuthority.Keys) {
        if ((Get-R0GovernanceField $authority $field) -cne $expectedAuthority[$field]) {
            Add-R0GovernanceIssue $issues 'Scientific context authority projection drifted.'
            break
        }
    }
    if ((Get-R0GovernanceField $authority 'owner_actor_id') -cne 'r0-science-agent') {
        Add-R0GovernanceIssue $issues 'Scientific context authority actor is not the authorized Scientific Authority.'
    }
    if ((Get-R0GovernanceField $authority 'owner_task_path') -cne '/root/r0_science_agent') {
        Add-R0GovernanceIssue $issues 'Scientific context authority task binding drifted.'
    }
    if ((Get-R0GovernanceField $authority 'decision_id') -cne 'RECON-DEC-005') {
        Add-R0GovernanceIssue $issues 'Scientific context reconciliation decision id drifted.'
    }
    $authorizedActor = @((Get-R0GovernanceField $Context.authorization 'actors') |
        Where-Object { (Get-R0GovernanceField $_ 'id') -ceq 'r0-science-agent' }) |
        Select-Object -First 1
    if ($null -eq $authorizedActor -or
        (Get-R0GovernanceField $authorizedActor 'task_path') -cne '/root/r0_science_agent' -or
        'scientific_authority' -cnotin @(Get-R0GovernanceField $authorizedActor 'authorized_roles')) {
        Add-R0GovernanceIssue $issues 'Scientific context authority does not resolve through owner authorization.'
    }

    $expectedProtected = [ordered]@{
        'specs/fixture-manifest.schema.json' = 'd532903919d6583e7615b5598772f3b4576761f60bfe3118c2b108de03f91c56'
        'specs/oracle-manifest.schema.json' = '2818ce6169105e49b563ca995cd4becc2797267220bdc332711d0751fabf7ca9'
        'specs/plan-proof-record.schema.json' = 'f2047ff0369576ceae90d1b9556b85724daa487f89b83dc5fa4b4b3cbe5a0c78'
    }
    foreach ($path in $expectedProtected.Keys) {
        if ((Get-R0GovernanceField $Context.protected_v1_hashes $path) -cne
            $expectedProtected[$path]) {
            Add-R0GovernanceIssue $issues 'A protected Fixture/Oracle/PlanProof v1 schema drifted.'
        }
    }
    $consumerMatches = @($Context.consumer_documents | Where-Object {
            Test-R0GovernanceConsumerText ([string](Get-R0GovernanceField $_ 'text'))
        })
    if ($consumerMatches.Count -ne 0) {
        Add-R0GovernanceIssue $issues 'A product path consumes the governance scientific context contract.'
    }
    if ($Context.consumer_probe_match -ne $true) {
        Add-R0GovernanceIssue $issues 'The product scientific-context consumer detector does not recognize reviewed identifier forms.'
    }
    if ($Context.cmake_text -cnotmatch '(?s)NAME\s+r0\.license-provenance.*?-File\s+\$\{CMAKE_CURRENT_SOURCE_DIR\}/tools/validate-license-provenance\.ps1') {
        Add-R0GovernanceIssue $issues 'The CTest license/provenance governance hook is missing.'
    }
    if ($Context.repository_verifier_text -cnotmatch
        '(?s)licenseProvenanceValidatorPath\s*=\s*Join-Path\s+\$PSScriptRoot\s+''validate-license-provenance\.ps1''.*?-File\s+\$licenseProvenanceValidatorPath\s+-Quiet') {
        Add-R0GovernanceIssue $issues 'The repository license/provenance governance hook is missing.'
    }
    $workflowDocument = @($Context.candidate_documents | Where-Object {
            (Get-R0GovernanceField $_ 'path') -ceq '.github/workflows/ci.yml'
        }) | Select-Object -First 1
    if ($null -eq $workflowDocument -or
        (Get-R0GovernanceField $workflowDocument 'text') -cnotmatch
        '(?s)name:\s+Verify Windows PowerShell 5\.1 compatibility.*?shell:\s+powershell.*?validate-license-provenance\.ps1') {
        Add-R0GovernanceIssue $issues 'The Windows PowerShell 5.1 license/provenance CI hook is missing.'
    }

    $backlog = $Context.backlog
    $govTask = @((Get-R0GovernanceField $backlog 'tasks') | Where-Object {
            (Get-R0GovernanceField $_ 'id') -ceq 'R0-GOV-002'
        }) | Select-Object -First 1
    if ($null -eq $govTask -or
        (Get-R0GovernanceCanonicalObjectSha256 $govTask) -cne
            'd248bdb844ceb9955dd494cd9065e31cabaeee16c0cc8c3bdc21e23361de7f5a') {
        Add-R0GovernanceIssue $issues 'R0-GOV-002 technical-candidate backlog projection drifted.'
    }
    $scienceTask = @((Get-R0GovernanceField $backlog 'tasks') | Where-Object {
            (Get-R0GovernanceField $_ 'id') -ceq 'R0-SCI-001'
        }) | Select-Object -First 1
    if ($null -eq $scienceTask -or
        (Get-R0GovernanceCanonicalObjectSha256 $scienceTask) -cne
            '1bb54220e2e8b33d3a5d8e795f46390628a699ebd64d880e8e9622c6dd7d0c99') {
        Add-R0GovernanceIssue $issues 'R0-SCI-001 technical-candidate backlog projection drifted.'
    }
    foreach ($sciencePattern in @(
            '(?m)^- Assignee\uff1a`r0-science-agent`\uff08machine agent\uff0ctask `/root/r0_science_agent`\uff09$',
            '(?m)^- Reviewer\uff1a`r0-architecture-agent`\uff08machine agent\uff0ctask `/root/r0_architecture_agent`\uff09$')) {
        if ($Context.science_work_package_text -cnotmatch $sciencePattern) {
            Add-R0GovernanceIssue $issues 'R0-SCI-001 work-package authority is stale.'
            break
        }
    }
    $gateTask = @((Get-R0GovernanceField $backlog 'tasks') | Where-Object {
            (Get-R0GovernanceField $_ 'id') -ceq 'R0-GATE-001'
        }) | Select-Object -First 1
    if ($null -eq $gateTask -or
        (Get-R0GovernanceCanonicalObjectSha256 $gateTask) -cne
            '992d6c63d8633a78b1cc3074e2dffa04b362cc2d1c50b7eed2cf5bb125ef92f0') {
        Add-R0GovernanceIssue $issues 'R0-GATE-001 technical-candidate backlog projection drifted.'
    }
    if ((Get-R0GovernanceField $backlog 'current_gate') -cne 'R0' -or
        (Get-R0GovernanceField $Context.project_manifest 'current_gate') -cne 'R0' -or
        (Get-R0GovernanceField $gateTask 'status') -cne 'planned') {
        Add-R0GovernanceIssue $issues 'R0 gate or R0-GATE-001 advanced during the governance technical candidate.'
    }
    $futureTasks = @((Get-R0GovernanceField $backlog 'tasks') | Where-Object {
            [string](Get-R0GovernanceField $_ 'stage') -match '^R[1-8]$'
        })
    if (@($futureTasks | Where-Object {
                (Get-R0GovernanceField $_ 'status') -cne 'planned' -or
                $null -ne (Get-R0GovernanceField $_ 'assignee')
            }).Count -gt 0) {
        Add-R0GovernanceIssue $issues 'A future-stage task advanced before R0 gate acceptance.'
    }
    if ($Context.adr_text -cnotmatch '(?m)^- Status: Proposed$' -or
        $Context.policy_text -cnotmatch '(?m)^- \u72b6\u6001\uff1aProposed / enforced as an interim fail-closed safeguard$') {
        Add-R0GovernanceIssue $issues 'ADR-0008 or its policy advanced before commit-bound disposition.'
    }
    foreach ($requiredPattern in @(
            '(?m)^- \u72b6\u6001\uff1aReview$',
            '(?m)^- Assignee\uff1a`r0-po-agent`\uff08machine agent\uff0ctask `/root`\uff09$',
            '(?m)^- Reviewer\uff1a`r0-validation-agent`\uff08machine agent\uff0ctask `/root/r0_validation_agent`\uff09$',
            '\u4efb\u52a1\u4fdd\u6301 `review`')) {
        if ($Context.work_package_text -cnotmatch $requiredPattern) {
            Add-R0GovernanceIssue $issues 'R0-GOV-002 work-package technical-candidate state drifted.'
            break
        }
    }
    if ($Context.reconciliation_audit_text -cnotmatch
        '`RECON-DEC-004`\u3001`005` and `009` remain open' -or
        -not $Context.reconciliation_audit_text.Contains(
            'its formal disposition remains open until commit-bound owner and independent reviews complete')) {
        Add-R0GovernanceIssue $issues 'RECON-DEC-005 was closed before its commit-bound disposition.'
    }
    foreach ($criterion in @('`G0-D-002`', '`G0-D-012`')) {
        $pattern = '(?m)^\| ' + [regex]::Escape($criterion) + ' \|[^\r\n]*\| `missing` \|'
        if ($Context.readiness_text -cnotmatch $pattern) {
            Add-R0GovernanceIssue $issues 'G0 readiness criteria advanced during R0-GOV-002 technical review.'
            break
        }
    }
    if ($Context.gate_work_package_text -cnotmatch
        'R0-GATE-001 \u7ee7\u7eed `planned` \u4e14 assignee \u4e3a\u7a7a\uff0cG0/G1 \u4ecd\u672a\u8fdb\u5165\u6b63\u5f0f\u8bc4\u5ba1') {
        Add-R0GovernanceIssue $issues 'Gate work package no longer preserves the R0/G0/G1 boundary.'
    }
    if ($Context.recon_disposition_exists -or $Context.task_acceptance_exists) {
        Add-R0GovernanceIssue $issues 'R0-GOV-002 disposition or task acceptance exists before technical review completes.'
    }

    [string[]]$inventoryMutations = @(
        'duplicate-item-id','missing-source-provenance','noassertion-external-distribution',
        'legacy-hash-drift','unapproved-license-conclusion','license-ref-without-text',
        'repository-license-status-contradiction','checkout-action-pin-drift',
        'unknown-item-category','generated-classification-mismatch',
        'generated-missing-lineage','generated-unresolved-lineage',
        'generated-restriction-downgrade','generated-missing-integrity',
        'extra-inventory-item','inventory-item-order-drift',
        'inventory-vocabulary-extra','inventory-owner-drift',
        'inventory-purpose-empty','inventory-version-empty','inventory-scope-drift',
        'generated-valid-parent-dropped','generated-false-existing-parent-added',
        'local-source-absolute-locator','local-source-percent-locator',
        'local-evidence-untracked-locator')
    [string[]]$expectedMutationIds = @($inventoryMutations) +
        @(Get-R0GovernanceReviewMutationNames)
    Test-R0GovernanceExactSet -Actual @(Get-R0GovernanceField $contract 'mutation_ids') `
        -Expected $expectedMutationIds -Label 'R0 governance review mutation ids' -Issues $issues
    if ((Get-R0GovernanceField $contract 'prepared_from_commit') -cne
        'dfedf278d53fa65706b8a884be1896a162d9e34a') {
        Add-R0GovernanceIssue $issues 'R0 governance review contract baseline drifted.'
    }
    return $issues.ToArray()
}

function Copy-R0GovernanceReviewContext {
    param($Context, [string]$SerializedContext)
    $json = if ([string]::IsNullOrEmpty($SerializedContext)) {
        $Context | ConvertTo-Json -Depth 100 -Compress
    }
    else { $SerializedContext }
    $copy = $json | ConvertFrom-Json
    $copy.repo_root = $Context.repo_root
    $copy.tracked_paths = $Context.tracked_paths
    return $copy
}

function Set-R0GovernanceCandidateDocumentText {
    param($Context, [string]$Path, [string]$Text)
    $document = @($Context.candidate_documents | Where-Object {
            (Get-R0GovernanceField $_ 'path') -ceq $Path
        }) | Select-Object -First 1
    if ($null -eq $document) { throw "Candidate document '$Path' is unavailable." }
    $document.text = $Text
}

function Test-R0GovernanceReview {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [switch]$RunMutations
    )
    $fatal = [System.Collections.Generic.List[string]]::new()
    $schemaPath = Join-Path $RepoRoot 'docs\governance\schemas\scientific-context.schema.json'
    $instancePath = Join-Path $RepoRoot 'fixtures\ref-scientific-conventions\scientific-context.json'
    $contractPath = Join-Path $RepoRoot 'docs\governance\r0-governance-review-contract.json'
    $inventoryPath = Join-Path $RepoRoot 'docs\governance\provenance-inventory.json'
    foreach ($entry in @(
            [pscustomobject]@{ Path=$schemaPath; Label='Scientific context schema' },
            [pscustomobject]@{ Path=$instancePath; Label='Scientific context instance' },
            [pscustomobject]@{ Path=$contractPath; Label='R0 governance review contract' },
            [pscustomobject]@{ Path=$inventoryPath; Label='Technical provenance inventory' })) {
        Test-R0GovernanceTextBytes -Path $entry.Path -Issues $fatal -Label $entry.Label
    }
    try {
        $schemaResult = Test-JsonSchemaSubset -SchemaPath $schemaPath -InstancePath $instancePath
        if (-not $schemaResult.IsValid) {
            foreach ($errorMessage in @($schemaResult.Errors)) {
                $fatal.Add("Scientific context schema validation failed: $errorMessage")
            }
        }
        $schemaFact = Get-R0GovernanceFileFact $RepoRoot (
            'docs/governance/schemas/scientific-context.schema.json')
        $instanceFact = Get-R0GovernanceFileFact $RepoRoot (
            'fixtures/ref-scientific-conventions/scientific-context.json')
        $inventoryFact = Get-R0GovernanceFileFact $RepoRoot (
            'docs/governance/provenance-inventory.json')
        if ($schemaFact.byte_length -ne 8943 -or $schemaFact.sha256 -cne
            'be17ec2954be2bc9e07ff79df391cdfd6d98493c00729fdce8a6d81731ff6b7d') {
            $fatal.Add('Scientific context schema raw-byte identity drifted.')
        }
        if ($instanceFact.byte_length -ne 7607 -or $instanceFact.sha256 -cne
            '87844b2eb9f937492f45bc8e96c762818fd2e110a1f9c1a5d26cb8b54f3f05e2') {
            $fatal.Add('Scientific context first-instance raw-byte identity drifted.')
        }
        if ($inventoryFact.byte_length -ne 13504 -or $inventoryFact.sha256 -cne
            '0103ab7e4ef75c6818a32556e17302035b3db8edea93f11bfd8fd9ca09b17e4b') {
            $fatal.Add('Technical provenance inventory raw-byte identity drifted.')
        }
        $context = Get-R0GovernanceReviewContext -RepoRoot $RepoRoot
        foreach ($issue in @(Get-R0GovernanceReviewIssues -Context $context)) {
            $fatal.Add([string]$issue)
        }
    }
    catch {
        $fatal.Add("R0 governance review cannot be evaluated: $($_.Exception.Message)")
        return [pscustomobject][ordered]@{
            Issues = $fatal.ToArray(); MutationCount = 0; MutationResults = @();
            ContextCount = 0; RuntimeConsumerCount = 0
        }
    }

    $mutationResults = [System.Collections.Generic.List[object]]::new()
    if ($RunMutations -and $fatal.Count -eq 0) {
        $serializedMutationContext = $context | ConvertTo-Json -Depth 100 -Compress
        $mutations = @(
            [pscustomobject]@{Name='missing-scientific-context-schema';Expected='schema is missing';Apply={param($c)$c.schema=$null}},
            [pscustomobject]@{Name='scientific-context-schema-identity-drift';Expected='schema identity drifted';Apply={param($c)$c.schema.'$id'='urn:drift'}},
            [pscustomobject]@{Name='missing-scientific-context-instance';Expected='instance is missing';Apply={param($c)$c.sidecars=@()}},
            [pscustomobject]@{Name='duplicate-scientific-context-id';Expected='Duplicate scientific context id';Apply={param($c)$c.sidecars=@($c.sidecars)+@($c.sidecars[0])}},
            [pscustomobject]@{Name='scientific-context-schema-version-drift';Expected='instance schema version drifted';Apply={param($c)$c.sidecars[0].schema_version='gnczmkn.scientific-context/2'}},
            [pscustomobject]@{Name='scientific-context-maturity-drift';Expected='instance maturity drifted';Apply={param($c)$c.sidecars[0].maturity='runtime-contract'}},
            [pscustomobject]@{Name='manifest-missing-scientific-context-artifact';Expected='does not require';Apply={param($c)$c.fixture.required_artifacts=@($c.fixture.required_artifacts|Where-Object{$_ -ne 'fixtures/ref-scientific-conventions/scientific-context.json'})}},
            [pscustomobject]@{Name='fact-missing-scientific-context-evidence';Expected='does not reference';Apply={param($c)$c.fixture.expected_facts[0].evidence_refs=@($c.fixture.expected_facts[0].evidence_refs|Where-Object{$_ -ne 'fixtures/ref-scientific-conventions/scientific-context.json'})}},
            [pscustomobject]@{Name='scientific-context-unknown-fixture';Expected='fixture identity is unknown';Apply={param($c)$c.sidecars[0].subject.fixture_id='REF-UNKNOWN-001'}},
            [pscustomobject]@{Name='scientific-context-unknown-claim';Expected='claim references differs';Apply={param($c)$c.sidecars[0].subject.claim_refs[0]='FACT-UNKNOWN'}},
            [pscustomobject]@{Name='scientific-context-missing-source';Expected='no source records';Apply={param($c)$c.sidecars[0].source_records=@()}},
            [pscustomobject]@{Name='scientific-context-local-source-hash-drift';Expected='raw SHA-256 drifted';Apply={param($c)$c.sidecars[0].source_records[0].integrity.raw_sha256=('0'*64)}},
            [pscustomobject]@{Name='scientific-context-local-source-byte-drift';Expected='raw byte length drifted';Apply={param($c)$c.sidecars[0].source_records[0].integrity.byte_length=1}},
            [pscustomobject]@{Name='scientific-context-normalized-hash-without-canonicalizer';Expected='lacks a canonicalizer';Apply={param($c)Add-Member -InputObject $c.sidecars[0].source_records[0].integrity -NotePropertyName normalized_sha256 -NotePropertyValue ('0'*64)}},
            [pscustomobject]@{Name='scientific-context-unavailable-expected-evidence';Expected='unavailable source cannot';Apply={param($c)$c.sidecars[0].source_records[5].availability='external-unavailable'}},
            [pscustomobject]@{Name='scientific-context-missing-units-domain';Expected="missing 'units'";Apply={param($c)$c.sidecars[0].validity_domain.semantic_dimensions=@($c.sidecars[0].validity_domain.semantic_dimensions|Where-Object{$_.id -ne 'units'})}},
            [pscustomobject]@{Name='scientific-context-missing-frames-domain';Expected="missing 'frames'";Apply={param($c)$c.sidecars[0].validity_domain.semantic_dimensions=@($c.sidecars[0].validity_domain.semantic_dimensions|Where-Object{$_.id -ne 'frames'})}},
            [pscustomobject]@{Name='scientific-context-missing-time-domain';Expected="missing 'time'";Apply={param($c)$c.sidecars[0].validity_domain.semantic_dimensions=@($c.sidecars[0].validity_domain.semantic_dimensions|Where-Object{$_.id -ne 'time'})}},
            [pscustomobject]@{Name='scientific-context-missing-assumption';Expected='has no assumptions';Apply={param($c)$c.sidecars[0].validity_domain.assumptions=@()}},
            [pscustomobject]@{Name='scientific-context-missing-comparison-ref';Expected="missing 'expected_ref'";Apply={param($c)$c.sidecars[0].comparison.expected_ref=''}},
            [pscustomobject]@{Name='scientific-context-product-runtime-dependency';Expected='consumes product runtime';Apply={param($c)$c.sidecars[0].reference_lanes[0].dependency_boundary.product_runtime_excluded=$false}},
            [pscustomobject]@{Name='scientific-context-legacy-dependency';Expected='consumes Legacy';Apply={param($c)$c.sidecars[0].reference_lanes[0].dependency_boundary.legacy_excluded=$false}},
            [pscustomobject]@{Name='scientific-context-cross-lane-dependency';Expected='consumes another reference lane';Apply={param($c)$c.sidecars[0].reference_lanes[0].dependency_boundary.other_reference_lane_excluded=$false}},
            [pscustomobject]@{Name='scientific-context-confirmed-implementation-without-basis';Expected='without basis';Apply={param($c)$c.sidecars[0].reference_lanes[0].basis_refs=@()}},
            [pscustomobject]@{Name='scientific-context-false-source-independence';Expected='falsely claims';Apply={param($c)$c.sidecars[0].reference_lanes[0].scientific_source_independence='confirmed'}},
            [pscustomobject]@{Name='scientific-context-unknown-rights-item';Expected='unknown rights inventory item';Apply={param($c)$c.sidecars[0].provenance.rights_inventory_item_ref='missing'}},
            [pscustomobject]@{Name='scientific-context-unknown-lineage-parent';Expected='unknown lineage parent';Apply={param($c)$c.sidecars[0].provenance.lineage_parent_item_refs+=@('missing')}},
            [pscustomobject]@{Name='scientific-context-unrelated-legacy-lineage';Expected='unrelated Legacy';Apply={param($c)$c.sidecars[0].provenance.lineage_parent_item_refs+=@('legacy-source-archive')}},
            [pscustomobject]@{Name='scientific-context-distribution-downgrade';Expected='differs from the candidate inventory state';Apply={param($c)$c.sidecars[0].external_distribution='not-redistributed'}},
            [pscustomobject]@{Name='scientific-context-rights-effect-drift';Expected='changes rights';Apply={param($c)$c.sidecars[0].rights_effect='export-allowed'}},
            [pscustomobject]@{Name='scientific-context-runtime-consumer';Expected='has a runtime consumer';Apply={param($c)$c.sidecars[0].runtime_consumers=1}},
            [pscustomobject]@{Name='aggregate-inventory-independence-overlay';Expected='forbidden scientific independence overlay';Apply={param($c)Add-Member -InputObject $c.inventory.items[2] -NotePropertyName independent_reference_confirmed -NotePropertyValue $true}},
            [pscustomobject]@{Name='protected-v1-schema-overlay';Expected='protected Fixture';Apply={param($c)$c.protected_v1_hashes.'specs/fixture-manifest.schema.json'=('0'*64)}},
            [pscustomobject]@{Name='product-consumes-scientific-context';Expected='product path consumes';Apply={param($c)$c.consumer_documents=@($c.consumer_documents)+@([pscustomobject]@{path='framework/fake.cpp';text='#include "scientific_context.hpp" ScientificContext'})}},
            [pscustomobject]@{Name='scientific-context-authority-actor-drift';Expected='authority actor';Apply={param($c)$c.sidecars[0].authority.owner_actor_id='r0-po-agent'}},
            [pscustomobject]@{Name='scientific-context-authority-task-drift';Expected='task binding drifted';Apply={param($c)$c.sidecars[0].authority.owner_task_path='/root'}},
            [pscustomobject]@{Name='scientific-context-decision-id-drift';Expected='decision id drifted';Apply={param($c)$c.sidecars[0].authority.decision_id='RECON-DEC-004'}},
            [pscustomobject]@{Name='scientific-context-premature-scientific-acceptance';Expected='advanced without SCI task acceptance';Apply={param($c)$c.sidecars[0].review_state='scientifically-accepted'}},
            [pscustomobject]@{Name='technical-inventory-contract-drift';Expected='technical contract semantic projection drifted';Apply={param($c)$c.contract.technical_inventory_identity.raw_sha256=('0'*64)}},
            [pscustomobject]@{Name='accepted-state-contract-drift';Expected='technical contract semantic projection drifted';Apply={param($c)$c.contract.accepted_state_transition.runtime_consumers=1}},
            [pscustomobject]@{Name='scientific-context-contract-drift';Expected='technical contract semantic projection drifted';Apply={param($c)$c.contract.scientific_context_contract.schema_id='urn:drift'}},
            [pscustomobject]@{Name='governance-contract-boundary-drift';Expected='technical contract semantic projection drifted';Apply={param($c)$c.contract.boundaries[0]='external distribution allowed'}},
            [pscustomobject]@{Name='scientific-context-extra-source';Expected='source ids differs';Apply={param($c)$extra=$c.sidecars[0].source_records[0]|ConvertTo-Json -Depth 100|ConvertFrom-Json;$extra.source_id='extra-source';$c.sidecars[0].source_records=@($c.sidecars[0].source_records)+@($extra)}},
            [pscustomobject]@{Name='scientific-context-comparison-ref-drift';Expected='comparison references differ';Apply={param($c)$c.sidecars[0].comparison.actual_report_ref='docs/README.md'}},
            [pscustomobject]@{Name='scientific-context-lane-ref-drift';Expected='references differ from the reviewed tracked set';Apply={param($c)$c.sidecars[0].reference_lanes[0].implementation_ref='docs/README.md'}},
            [pscustomobject]@{Name='scientific-context-reviewer-role-drift';Expected='authority projection drifted';Apply={param($c)$c.sidecars[0].authority.reviewer_role='product_owner'}},
            [pscustomobject]@{Name='executable-scientific-fixture-without-sidecar';Expected='executable scientific fixture lacks';Apply={param($c)$candidate=@($c.fixture_manifests|Where-Object{$_.value.fixture_id -eq 'REF-MINIMAL-3DOF-001'})[0];$candidate.value.status='executable'}},
            [pscustomobject]@{Name='governance-task-premature-done';Expected='backlog projection drifted';Apply={param($c)$task=@($c.backlog.tasks|Where-Object{$_.id -eq 'R0-GOV-002'})[0];$task.status='done'}},
            [pscustomobject]@{Name='governance-task-evidence-drift';Expected='backlog projection drifted';Apply={param($c)$task=@($c.backlog.tasks|Where-Object{$_.id -eq 'R0-GOV-002'})[0];$task.evidence=@('bogus')}},
            [pscustomobject]@{Name='science-governance-dependency-premature';Expected='R0-SCI-001 technical-candidate backlog projection drifted';Apply={param($c)$task=@($c.backlog.tasks|Where-Object{$_.id -eq 'R0-SCI-001'})[0];$task.depends_on=@($task.depends_on)+@('R0-GOV-002')}},
            [pscustomobject]@{Name='adr-policy-premature-accepted';Expected='advanced before commit-bound disposition';Apply={param($c)$c.adr_text=$c.adr_text.Replace('- Status: Proposed','- Status: Accepted')}},
            [pscustomobject]@{Name='reconciliation-premature-disposition';Expected='before technical review completes';Apply={param($c)$c.recon_disposition_exists=$true}},
            [pscustomobject]@{Name='work-package-premature-done';Expected='work-package technical-candidate state drifted';Apply={param($c)$m=[regex]::Match($c.work_package_text,'(?m)^- \u72b6\u6001\uff1aReview$');$c.work_package_text=$c.work_package_text.Replace($m.Value,$m.Value.Replace('Review','Done'))}},
            [pscustomobject]@{Name='gate-premature-advance';Expected='advanced during the governance technical candidate';Apply={param($c)$c.project_manifest.current_gate='R1'}},
            [pscustomobject]@{Name='future-stage-premature-advance';Expected='future-stage task advanced';Apply={param($c)$task=@($c.backlog.tasks|Where-Object{$_.stage -eq 'R1'})[0];$task.status='ready'}},
            [pscustomobject]@{Name='readiness-criterion-premature-close';Expected='readiness criteria advanced';Apply={param($c)$c.readiness_text=[regex]::Replace($c.readiness_text,'(?m)^(\| `G0-D-002` \|[^\r\n]*?\| )`missing`( \|)','$1`closed`$2')}},
            [pscustomobject]@{Name='science-task-authority-drift';Expected='R0-SCI-001 technical-candidate backlog projection drifted';Apply={param($c)$task=@($c.backlog.tasks|Where-Object{$_.id -eq 'R0-SCI-001'})[0];$task.assignee='Codex'}},
            [pscustomobject]@{Name='science-work-package-authority-drift';Expected='R0-SCI-001 work-package authority is stale';Apply={param($c)$c.science_work_package_text=$c.science_work_package_text.Replace('r0-architecture-agent','r0-validation-agent')}},
            [pscustomobject]@{Name='ctest-governance-hook-missing';Expected='CTest license/provenance governance hook is missing';Apply={param($c)$c.cmake_text=$c.cmake_text.Replace('tools/validate-license-provenance.ps1','tools/validate-license-provenance-removed.ps1')}},
            [pscustomobject]@{Name='repository-governance-hook-missing';Expected='repository license/provenance governance hook is missing';Apply={param($c)$c.repository_verifier_text=$c.repository_verifier_text.Replace('validate-license-provenance.ps1','validate-license-provenance-removed.ps1')}},
            [pscustomobject]@{Name='license-status-grant-contradiction';Expected="candidate document 'LICENSE-STATUS.md' reviewed text drifted";Apply={param($c)$d=@($c.candidate_documents|Where-Object{$_.path -ceq 'LICENSE-STATUS.md'})[0];Set-R0GovernanceCandidateDocumentText $c 'LICENSE-STATUS.md' ($d.text+"`nThis repository is licensed under MIT.`n")}},
            [pscustomobject]@{Name='readme-repository-license-grant';Expected='repository license-grant signal exists';Apply={param($c)$d=@($c.candidate_documents|Where-Object{$_.path -ceq 'README.md'})[0];$d.text+="`nThis repository is licensed under MIT.`n";$g=@($c.grant_surface_documents|Where-Object{$_.path -ceq 'README.md'})[0];$g.grant_text_signal=Test-R0GovernanceRepositoryGrantText $d.text}},
            [pscustomobject]@{Name='adr-license-selection-contradiction';Expected="candidate document 'docs/adr/0008-internal-default-license-and-provenance-gate.md' reviewed text drifted";Apply={param($c)$p='docs/adr/0008-internal-default-license-and-provenance-gate.md';$d=@($c.candidate_documents|Where-Object{$_.path -ceq $p})[0];Set-R0GovernanceCandidateDocumentText $c $p ($d.text+"`nRepository distribution license selected: MIT.`n")}},
            [pscustomobject]@{Name='policy-rights-contradiction';Expected="candidate document 'docs/governance/license-and-provenance-policy.md' reviewed text drifted";Apply={param($c)$p='docs/governance/license-and-provenance-policy.md';$d=@($c.candidate_documents|Where-Object{$_.path -ceq $p})[0];Set-R0GovernanceCandidateDocumentText $c $p ($d.text+"`nExternal sharing is allowed.`n")}},
            [pscustomobject]@{Name='handoff-state-contradiction';Expected="candidate document 'docs/handoff/r0-execution-state.md' reviewed text drifted";Apply={param($c)$p='docs/handoff/r0-execution-state.md';$d=@($c.candidate_documents|Where-Object{$_.path -ceq $p})[0];Set-R0GovernanceCandidateDocumentText $c $p ($d.text+"`nADR-0008 Accepted; G1 passed; R1 unlocked.`n")}},
            [pscustomobject]@{Name='checklist-rights-contradiction';Expected="candidate document 'docs/quality/provenance-review-checklist.md' reviewed text drifted";Apply={param($c)$p='docs/quality/provenance-review-checklist.md';$d=@($c.candidate_documents|Where-Object{$_.path -ceq $p})[0];Set-R0GovernanceCandidateDocumentText $c $p ($d.text+"`nExternal distribution approved.`n")}},
            [pscustomobject]@{Name='reconciliation-rights-contradiction';Expected="candidate document 'docs/quality/r0-first-wave-reconciliation-audit.md' reviewed text drifted";Apply={param($c)$p='docs/quality/r0-first-wave-reconciliation-audit.md';$d=@($c.candidate_documents|Where-Object{$_.path -ceq $p})[0];Set-R0GovernanceCandidateDocumentText $c $p ($d.text+"`nRights and distribution cleared.`n")}},
            [pscustomobject]@{Name='readiness-verdict-premature-ready';Expected="candidate document 'docs/quality/r0-g0-g1-readiness-audit.md' reviewed text drifted";Apply={param($c)$p='docs/quality/r0-g0-g1-readiness-audit.md';$d=@($c.candidate_documents|Where-Object{$_.path -ceq $p})[0];Set-R0GovernanceCandidateDocumentText $c $p ($d.text.Replace('not_ready_for_authorized_review','ready_for_authorized_review'))}},
            [pscustomobject]@{Name='readiness-g1-premature-ready';Expected="candidate document 'docs/quality/r0-g0-g1-readiness-audit.md' reviewed text drifted";Apply={param($c)$p='docs/quality/r0-g0-g1-readiness-audit.md';$d=@($c.candidate_documents|Where-Object{$_.path -ceq $p})[0];Set-R0GovernanceCandidateDocumentText $c $p ($d.text+"`nCurrent G1 readiness: ready.`n")}},
            [pscustomobject]@{Name='readiness-provenance-premature-closed';Expected="candidate document 'docs/quality/r0-g0-g1-readiness-audit.md' reviewed text drifted";Apply={param($c)$p='docs/quality/r0-g0-g1-readiness-audit.md';$d=@($c.candidate_documents|Where-Object{$_.path -ceq $p})[0];Set-R0GovernanceCandidateDocumentText $c $p ($d.text.Replace('`G1-X-011`','`G1-X-011` closed'))}},
            [pscustomobject]@{Name='readiness-official-gate-passed';Expected="candidate document 'docs/quality/r0-g0-g1-readiness-audit.md' reviewed text drifted";Apply={param($c)$p='docs/quality/r0-g0-g1-readiness-audit.md';$d=@($c.candidate_documents|Where-Object{$_.path -ceq $p})[0];Set-R0GovernanceCandidateDocumentText $c $p ($d.text+"`nOfficial gate result: Passed.`n")}},
            [pscustomobject]@{Name='readiness-governance-task-premature-done';Expected="candidate document 'docs/quality/r0-g0-g1-readiness-audit.md' reviewed text drifted";Apply={param($c)$p='docs/quality/r0-g0-g1-readiness-audit.md';$d=@($c.candidate_documents|Where-Object{$_.path -ceq $p})[0];Set-R0GovernanceCandidateDocumentText $c $p ($d.text.Replace('`R0-GOV-002` | Product Owner | `review`','`R0-GOV-002` | Product Owner | `done`'))}},
            [pscustomobject]@{Name='readiness-science-task-premature-done';Expected="candidate document 'docs/quality/r0-g0-g1-readiness-audit.md' reviewed text drifted";Apply={param($c)$p='docs/quality/r0-g0-g1-readiness-audit.md';$d=@($c.candidate_documents|Where-Object{$_.path -ceq $p})[0];Set-R0GovernanceCandidateDocumentText $c $p ($d.text+"`nR0-SCI-001: done.`n")}},
            [pscustomobject]@{Name='gate-work-package-premature-done';Expected="candidate document 'docs/tasks/work-packages/R0-GATE-001.md' reviewed text drifted";Apply={param($c)$p='docs/tasks/work-packages/R0-GATE-001.md';$d=@($c.candidate_documents|Where-Object{$_.path -ceq $p})[0];Set-R0GovernanceCandidateDocumentText $c $p ($d.text+"`nGate status: Done.`n")}},
            [pscustomobject]@{Name='gate-work-package-owner-drift';Expected="candidate document 'docs/tasks/work-packages/R0-GATE-001.md' reviewed text drifted";Apply={param($c)$p='docs/tasks/work-packages/R0-GATE-001.md';$d=@($c.candidate_documents|Where-Object{$_.path -ceq $p})[0];Set-R0GovernanceCandidateDocumentText $c $p ($d.text+"`nOwner role: Architecture Lead.`n")}},
            [pscustomobject]@{Name='gate-work-package-count-contradiction';Expected="candidate document 'docs/tasks/work-packages/R0-GATE-001.md' reviewed text drifted";Apply={param($c)$p='docs/tasks/work-packages/R0-GATE-001.md';$d=@($c.candidate_documents|Where-Object{$_.path -ceq $p})[0];Set-R0GovernanceCandidateDocumentText $c $p ($d.text+"`nGovernance mutations: 8/80.`n")}},
            [pscustomobject]@{Name='gate-work-package-premature-qualified';Expected="candidate document 'docs/tasks/work-packages/R0-GATE-001.md' reviewed text drifted";Apply={param($c)$p='docs/tasks/work-packages/R0-GATE-001.md';$d=@($c.candidate_documents|Where-Object{$_.path -ceq $p})[0];Set-R0GovernanceCandidateDocumentText $c $p ($d.text+"`nR0-GOV-002 qualified.`n")}},
            [pscustomobject]@{Name='gate-work-package-g0-passed-append';Expected="candidate document 'docs/tasks/work-packages/R0-GATE-001.md' reviewed text drifted";Apply={param($c)$p='docs/tasks/work-packages/R0-GATE-001.md';$d=@($c.candidate_documents|Where-Object{$_.path -ceq $p})[0];Set-R0GovernanceCandidateDocumentText $c $p ($d.text+"`nG0 passed.`n")}},
            [pscustomobject]@{Name='governance-work-package-owner-drift';Expected="candidate document 'docs/tasks/work-packages/R0-GOV-002.md' reviewed text drifted";Apply={param($c)$p='docs/tasks/work-packages/R0-GOV-002.md';$d=@($c.candidate_documents|Where-Object{$_.path -ceq $p})[0];Set-R0GovernanceCandidateDocumentText $c $p ($d.text+"`nOwner role: Architecture Lead.`n")}},
            [pscustomobject]@{Name='governance-work-package-count-contradiction';Expected="candidate document 'docs/tasks/work-packages/R0-GOV-002.md' reviewed text drifted";Apply={param($c)$p='docs/tasks/work-packages/R0-GOV-002.md';$d=@($c.candidate_documents|Where-Object{$_.path -ceq $p})[0];Set-R0GovernanceCandidateDocumentText $c $p ($d.text.Replace('173/173','8/173'))}},
            [pscustomobject]@{Name='science-work-package-premature-done';Expected="candidate document 'docs/tasks/work-packages/R0-SCI-001.md' reviewed text drifted";Apply={param($c)$p='docs/tasks/work-packages/R0-SCI-001.md';$d=@($c.candidate_documents|Where-Object{$_.path -ceq $p})[0];Set-R0GovernanceCandidateDocumentText $c $p ($d.text+"`nTask status: Done.`n")}},
            [pscustomobject]@{Name='gate-backlog-assignee-premature';Expected='R0-GATE-001 technical-candidate backlog projection drifted';Apply={param($c)$t=@($c.backlog.tasks|Where-Object{$_.id -eq 'R0-GATE-001'})[0];$t.assignee='r0-po-agent'}},
            [pscustomobject]@{Name='future-stage-assignee-premature';Expected='future-stage task advanced';Apply={param($c)$t=@($c.backlog.tasks|Where-Object{$_.stage -eq 'R1'})[0];$t.assignee='r0-po-agent'}},
            [pscustomobject]@{Name='ctest-governance-hook-disabled';Expected="candidate document 'CMakeLists.txt' reviewed text drifted";Apply={param($c)$p='CMakeLists.txt';$d=@($c.candidate_documents|Where-Object{$_.path -ceq $p})[0];Set-R0GovernanceCandidateDocumentText $c $p ($d.text+"`nset_tests_properties(r0.license-provenance PROPERTIES DISABLED TRUE)`n")}},
            [pscustomobject]@{Name='repository-governance-hook-dead-branch';Expected="candidate document 'tools/verify-repository.ps1' reviewed text drifted";Apply={param($c)$p='tools/verify-repository.ps1';$d=@($c.candidate_documents|Where-Object{$_.path -ceq $p})[0];Set-R0GovernanceCandidateDocumentText $c $p ("if (`$false) {`n"+$d.text+"`n}`n")}},
            [pscustomobject]@{Name='workflow-ps51-governance-hook-missing';Expected='Windows PowerShell 5.1 license/provenance CI hook is missing';Apply={param($c)$p='.github/workflows/ci.yml';$d=@($c.candidate_documents|Where-Object{$_.path -ceq $p})[0];$d.text=$d.text.Replace('          ./tools/validate-license-provenance.ps1','          ./tools/validate-license-provenance-removed.ps1')}},
            [pscustomobject]@{Name='premature-adr8-disposition-any-date';Expected='premature ADR-0008';Apply={param($c)$c.premature_record_paths=@('docs/governance/adr-dispositions/ADR-0008-2099-01-01.json')}},
            [pscustomobject]@{Name='premature-recon005-disposition-any-date';Expected='premature ADR-0008';Apply={param($c)$c.premature_record_paths=@('docs/governance/reconciliation-dispositions/RECON-DEC-005-2099-01-01.json')}},
            [pscustomobject]@{Name='premature-governance-task-acceptance-any-date';Expected='premature ADR-0008';Apply={param($c)$c.premature_record_paths=@('docs/quality/task-acceptance-R0-GOV-002-2099-01-01.json')}},
            [pscustomobject]@{Name='premature-gate-decision-record';Expected='premature ADR-0008';Apply={param($c)$c.premature_record_paths=@('docs/quality/gate-decisions/G0-2099-01-01.json')}},
            [pscustomobject]@{Name='report-input-path-dropped';Expected='report input path set differs';Apply={param($c)$c.report_input_paths=@($c.report_input_paths|Select-Object -Skip 1)}},
            [pscustomobject]@{Name='report-input-path-extra';Expected='report input path set differs';Apply={param($c)$c.report_input_paths=@($c.report_input_paths)+@('bogus-input')}},
            [pscustomobject]@{Name='report-input-policy-count-drift';Expected='technical contract semantic projection drifted';Apply={param($c)$c.contract.report_input_policy.path_count++}},
            [pscustomobject]@{Name='candidate-document-identity-drift';Expected='technical contract semantic projection drifted';Apply={param($c)$c.contract.candidate_document_identities[0].normalized_sha256=('0'*64)}},
            [pscustomobject]@{Name='consumer-detector-identifier-form-drift';Expected='consumer detector does not recognize';Apply={param($c)$c.consumer_probe_match=$false}},
            [pscustomobject]@{Name='host-toolchain-report-identity-drift';Expected='report tool identities differ';Apply={param($c)$c.scientific_report.environment.cpp_runtime='GCC 99.0'}},
            [pscustomobject]@{Name='scientific-context-candidate-distribution-mismatch';Expected='differs from the candidate inventory state';Apply={param($c)$c.sidecars[0].external_distribution='blocked-pending-rights-and-license-decision'}},
            [pscustomobject]@{Name='repository-grant-detector-drift';Expected='repository license-grant signal exists';Apply={param($c)$c.grant_surface_documents=@($c.grant_surface_documents)+@(New-R0GovernanceTextScanDocument 'NOTICE.md' 'Permission is hereby granted, free of charge, to any person obtaining a copy.')}},
            [pscustomobject]@{Name='repository-grant-suffixed-path-drift';Expected='repository license-grant signal exists';Apply={param($c)$c.grant_surface_documents=@($c.grant_surface_documents)+@(New-R0GovernanceTextScanDocument 'license-isc' 'This repository is licensed under ISC terms.')}},
            [pscustomobject]@{Name='repository-grant-json-metadata-drift';Expected='repository license-grant signal exists';Apply={param($c)$c.grant_surface_documents=@($c.grant_surface_documents)+@(New-R0GovernanceTextScanDocument 'package.json' '{"licenses":[{"type":"MIT"}]}')}},
            [pscustomobject]@{Name='repository-grant-toml-metadata-drift';Expected='repository license-grant signal exists';Apply={param($c)$c.grant_surface_documents=@($c.grant_surface_documents)+@(New-R0GovernanceTextScanDocument 'pyproject.toml' 'license = "MIT"')}},
            [pscustomobject]@{Name='repository-grant-xml-metadata-drift';Expected='repository license-grant signal exists';Apply={param($c)$c.grant_surface_documents=@($c.grant_surface_documents)+@(New-R0GovernanceTextScanDocument 'package.nuspec' '<license type="expression">MIT</license>')}},
            [pscustomobject]@{Name='repository-grant-yaml-metadata-drift';Expected='repository license-grant signal exists';Apply={param($c)$c.grant_surface_documents=@($c.grant_surface_documents)+@(New-R0GovernanceTextScanDocument 'package.yaml' 'license: MIT')}},
            [pscustomobject]@{Name='repository-grant-gemspec-metadata-drift';Expected='repository license-grant signal exists';Apply={param($c)$c.grant_surface_documents=@($c.grant_surface_documents)+@(New-R0GovernanceTextScanDocument 'sample.gemspec' 'spec.license = "MIT"')}},
            [pscustomobject]@{Name='repository-grant-csproj-metadata-drift';Expected='repository license-grant signal exists';Apply={param($c)$c.grant_surface_documents=@($c.grant_surface_documents)+@(New-R0GovernanceTextScanDocument 'sample.csproj' '<PackageLicenseExpression>MIT</PackageLicenseExpression>')}},
            [pscustomobject]@{Name='repository-grant-ini-metadata-drift';Expected='repository license-grant signal exists';Apply={param($c)$c.grant_surface_documents=@($c.grant_surface_documents)+@(New-R0GovernanceTextScanDocument 'setup.cfg' 'license = MIT')}},
            [pscustomobject]@{Name='repository-grant-nested-license-drift';Expected='repository license-grant signal exists';Apply={param($c)$c.grant_surface_documents=@($c.grant_surface_documents)+@(New-R0GovernanceTextScanDocument 'packages/foo/LICENSE' 'MIT License')}},
            [pscustomobject]@{Name='repository-grant-nested-notice-drift';Expected='repository license-grant signal exists';Apply={param($c)$c.grant_surface_documents=@($c.grant_surface_documents)+@(New-R0GovernanceTextScanDocument 'vendor/foo/NOTICE.txt' 'Permission is hereby granted, free of charge.')}},
            [pscustomobject]@{Name='repository-grant-source-spdx-drift';Expected='repository license-grant signal exists';Apply={param($c)$bytes=[System.Text.Encoding]::ASCII.GetBytes(('// SPDX-License-Identifier'+': MIT'));$c.grant_surface_documents=@($c.grant_surface_documents)+@(New-R0GovernanceScanDocument -Path 'framework/fake.cpp' -Bytes $bytes)}},
            [pscustomobject]@{Name='repository-grant-invalid-utf8-source-spdx-drift';Expected='repository license-grant signal exists';Apply={param($c)$bytes=[byte[]](@(0xff)+@([System.Text.Encoding]::ASCII.GetBytes(('// SPDX-License-Identifier'+': MIT'))));$c.grant_surface_documents=@($c.grant_surface_documents)+@(New-R0GovernanceScanDocument -Path 'framework/invalid.cpp' -Bytes $bytes)}},
            [pscustomobject]@{Name='repository-grant-raw-spdx-probe-drift';Expected='raw-byte SPDX detector';Apply={param($c)$c.raw_spdx_invalid_utf8_probe_match=$false}},
            [pscustomobject]@{Name='repository-grant-utf16le-source-spdx-drift';Expected='repository license-grant signal exists';Apply={param($c)$bytes=[System.Text.Encoding]::Unicode.GetBytes(('// SPDX-License-Identifier'+': MIT'));$c.grant_surface_documents=@($c.grant_surface_documents)+@(New-R0GovernanceScanDocument -Path 'framework/utf16le.cpp' -Bytes $bytes)}},
            [pscustomobject]@{Name='repository-grant-utf16be-source-spdx-drift';Expected='repository license-grant signal exists';Apply={param($c)$bytes=[System.Text.Encoding]::BigEndianUnicode.GetBytes(('// SPDX-License-Identifier'+': MIT'));$c.grant_surface_documents=@($c.grant_surface_documents)+@(New-R0GovernanceScanDocument -Path 'framework/utf16be.cpp' -Bytes $bytes)}},
            [pscustomobject]@{Name='repository-grant-unreviewed-text-encoding-drift';Expected='not canonical reviewable UTF-8';Apply={param($c)$c.grant_surface_documents=@($c.grant_surface_documents)+@(New-R0GovernanceScanDocument -Path 'framework/opaque.cpp' -Bytes ([byte[]]@(0xff)))}},
            [pscustomobject]@{Name='repository-grant-utf32le-text-drift';Expected='not canonical reviewable UTF-8';Apply={param($c)$bytes=[System.Text.Encoding]::UTF32.GetBytes(('// SPDX-License-Identifier'+': MIT'));$c.grant_surface_documents=@($c.grant_surface_documents)+@(New-R0GovernanceScanDocument -Path 'framework/utf32le.cpp' -Bytes $bytes)}},
            [pscustomobject]@{Name='repository-grant-utf32be-text-drift';Expected='not canonical reviewable UTF-8';Apply={param($c)$bytes=[System.Text.UTF32Encoding]::new($true,$false,$true).GetBytes(('// SPDX-License-Identifier'+': MIT'));$c.grant_surface_documents=@($c.grant_surface_documents)+@(New-R0GovernanceScanDocument -Path 'framework/utf32be.cpp' -Bytes $bytes)}},
            [pscustomobject]@{Name='repository-grant-nul-text-drift';Expected='not canonical reviewable UTF-8';Apply={param($c)$bytes=[byte[]](@([System.Text.Encoding]::ASCII.GetBytes('reviewed text'))+@(0));$c.grant_surface_documents=@($c.grant_surface_documents)+@(New-R0GovernanceScanDocument -Path 'framework/nul.cpp' -Bytes $bytes)}},
            [pscustomobject]@{Name='repository-grant-c0-text-drift';Expected='not canonical reviewable UTF-8';Apply={param($c)$bytes=[byte[]](@([System.Text.Encoding]::ASCII.GetBytes('reviewed text'))+@(1));$c.grant_surface_documents=@($c.grant_surface_documents)+@(New-R0GovernanceScanDocument -Path 'framework/c0.cpp' -Bytes $bytes)}},
            [pscustomobject]@{Name='reviewed-binary-arbitrary-flag-drift';Expected='reviewed binary exception identity drifted';Apply={param($c)$d=@($c.grant_surface_documents|Where-Object{$_.path -ceq 'README.md'})[0];$d.reviewed_binary=$true}},
            [pscustomobject]@{Name='reviewed-binary-wrong-path-drift';Expected='reviewed binary exception identity drifted';Apply={param($c)$d=@($c.grant_surface_documents|Where-Object{$_.path -ceq 'reference/legacy/legacy-source.zip'})[0];$d.path='reference/legacy/reviewed-source.zip'}},
            [pscustomobject]@{Name='reviewed-binary-wrong-sha-drift';Expected='reviewed binary exception identity drifted';Apply={param($c)$d=@($c.grant_surface_documents|Where-Object{$_.path -ceq 'reference/legacy/legacy-source.zip'})[0];$d.raw_sha256=('0'*64)}},
            [pscustomobject]@{Name='reviewed-binary-wrong-byte-length-drift';Expected='reviewed binary exception identity drifted';Apply={param($c)$d=@($c.grant_surface_documents|Where-Object{$_.path -ceq 'reference/legacy/legacy-source.zip'})[0];$d.byte_length++}},
            [pscustomobject]@{Name='reviewed-binary-wrong-object-id-drift';Expected='reviewed binary exception identity drifted';Apply={param($c)$d=@($c.grant_surface_documents|Where-Object{$_.path -ceq 'reference/legacy/legacy-source.zip'})[0];$d.stage_object_id=('0'*40)}},
            [pscustomobject]@{Name='index-nested-license-symlink-drift';Expected='non-regular, non-blob, or non-stage-0';Apply={param($c)$c.index_entries=@($c.index_entries)+@([pscustomobject]@{mode='120000';object_id=('0'*40);stage=0;path='vendor/LICENSE';object_type='blob'})}},
            [pscustomobject]@{Name='index-arbitrary-symlink-drift';Expected='non-regular, non-blob, or non-stage-0';Apply={param($c)$c.index_entries=@($c.index_entries)+@([pscustomobject]@{mode='120000';object_id=('0'*40);stage=0;path='framework/link.hpp';object_type='blob'})}},
            [pscustomobject]@{Name='index-gitlink-drift';Expected='non-regular, non-blob, or non-stage-0';Apply={param($c)$c.index_entries=@($c.index_entries)+@([pscustomobject]@{mode='160000';object_id=('0'*40);stage=0;path='third_party/submodule';object_type='commit'})}},
            [pscustomobject]@{Name='index-unmerged-stage1-drift';Expected='non-regular, non-blob, or non-stage-0';Apply={param($c)$c.index_entries=@($c.index_entries)+@([pscustomobject]@{mode='100644';object_id=('0'*40);stage=1;path='conflict.txt';object_type='blob'})}},
            [pscustomobject]@{Name='index-unmerged-stage2-drift';Expected='non-regular, non-blob, or non-stage-0';Apply={param($c)$c.index_entries=@($c.index_entries)+@([pscustomobject]@{mode='100644';object_id=('0'*40);stage=2;path='conflict.txt';object_type='blob'})}},
            [pscustomobject]@{Name='index-unmerged-stage3-drift';Expected='non-regular, non-blob, or non-stage-0';Apply={param($c)$c.index_entries=@($c.index_entries)+@([pscustomobject]@{mode='100644';object_id=('0'*40);stage=3;path='conflict.txt';object_type='blob'})}},
            [pscustomobject]@{Name='index-duplicate-multistage-drift';Expected='duplicate or multi-stage path';Apply={param($c)$d=$c.index_entries[0];$c.index_entries=@($c.index_entries)+@([pscustomobject]@{mode=$d.mode;object_id=$d.object_id;stage=2;path=$d.path;object_type=$d.object_type})}},
            [pscustomobject]@{Name='index-missing-object-drift';Expected='non-regular, non-blob, or non-stage-0';Apply={param($c)$c.index_entries=@($c.index_entries)+@([pscustomobject]@{mode='100644';object_id=('0'*40);stage=0;path='missing-object.txt';object_type='missing'})}},
            [pscustomobject]@{Name='index-nonblob-object-drift';Expected='non-regular, non-blob, or non-stage-0';Apply={param($c)$c.index_entries=@($c.index_entries)+@([pscustomobject]@{mode='100644';object_id=('0'*40);stage=0;path='tree-object.txt';object_type='tree'})}},
            [pscustomobject]@{Name='index-worktree-mismatch-drift';Expected='worktree regular-file object differs';Apply={param($c)$c.worktree_object_facts[0].matches_index=$false}},
            [pscustomobject]@{Name='index-case-alias-drift';Expected='case-insensitive worktree path collision';Apply={param($c)$d=$c.index_entries[0];$c.index_entries=@($c.index_entries)+@([pscustomobject]@{mode=$d.mode;object_id=$d.object_id;stage=0;path=$d.path.ToUpperInvariant();object_type=$d.object_type})}},
            [pscustomobject]@{Name='index-skip-worktree-flag-drift';Expected='weakening extended flag';Apply={param($c)$p=[string]$c.index_entries[0].path;$c.index_flag_tags.PSObject.Properties[$p].Value.cached='S'}},
            [pscustomobject]@{Name='index-assume-unchanged-flag-drift';Expected='weakening extended flag';Apply={param($c)$p=[string]$c.index_entries[0].path;$c.index_flag_tags.PSObject.Properties[$p].Value.assume_unchanged='h'}},
            [pscustomobject]@{Name='index-fsmonitor-flag-drift';Expected='weakening extended flag';Apply={param($c)$p=[string]$c.index_entries[0].path;$c.index_flag_tags.PSObject.Properties[$p].Value.fsmonitor='h'}},
            [pscustomobject]@{Name='index-intent-to-add-flag-drift';Expected='weakening extended flag';Apply={param($c)$p=[string]$c.index_entries[0].path;$c.index_debug_flags.PSObject.Properties[$p].Value='20004000'}},
            [pscustomobject]@{Name='index-flag-entry-missing-drift';Expected='weakening extended flag';Apply={param($c)$p=[string]$c.index_entries[0].path;$c.index_flag_tags.PSObject.Properties.Remove($p)}},
            [pscustomobject]@{Name='index-content-filter-attribute-drift';Expected='content-transforming attribute';Apply={param($c)$p=[string]$c.index_entries[0].path;$c.index_filter_attributes.PSObject.Properties[$p].Value.filter='inject-license'}},
            [pscustomobject]@{Name='index-working-tree-encoding-attribute-drift';Expected='content-transforming attribute';Apply={param($c)$p=[string]$c.index_entries[0].path;$c.index_filter_attributes.PSObject.Properties[$p].Value.'working-tree-encoding'='UTF-16LE'}},
            [pscustomobject]@{Name='index-ident-attribute-drift';Expected='content-transforming attribute';Apply={param($c)$p=[string]$c.index_entries[0].path;$c.index_filter_attributes.PSObject.Properties[$p].Value.ident='set'}},
            [pscustomobject]@{Name='index-replacement-object-byte-drift';Expected='grant and SPDX scan bytes differ';Apply={param($c)$d=$c.grant_surface_documents[0];$d.raw_sha256=('0'*64)}},
            [pscustomobject]@{Name='report-input-index-blob-byte-drift';Expected='grant and SPDX scan bytes differ';Apply={param($c)$d=$c.grant_surface_documents[0];$d.byte_length++}},
            [pscustomobject]@{Name='index-intent-to-add-probe-drift';Expected='intent-to-add detector failed';Apply={param($c)$c.intent_to_add_probe_match=$false}},
            [pscustomobject]@{Name='index-content-filter-probe-drift';Expected='content-filter detector failed';Apply={param($c)$c.content_filter_probe_match=$false}},
            [pscustomobject]@{Name='index-replacement-object-probe-drift';Expected='replacement-ref probe';Apply={param($c)$c.replacement_object_probe_match=$false}},
            [pscustomobject]@{Name='index-worktree-object-id-drift';Expected='worktree regular-file object differs';Apply={param($c)$c.worktree_object_facts[0].worktree_object_id=('0'*40)}},
            [pscustomobject]@{Name='index-worktree-reparse-drift';Expected='worktree regular-file object differs';Apply={param($c)$c.worktree_object_facts[0].reparse_path=$true}},
            [pscustomobject]@{Name='index-worktree-ancestor-reparse-drift';Expected='worktree regular-file object differs';Apply={param($c)$c.worktree_object_facts[1].reparse_path=$true}},
            [pscustomobject]@{Name='embedded-scientific-report-hash-drift';Expected='embedded Git-blob identity';Apply={param($c)$p='docs/quality/r0-g0-g1-readiness-audit.md';$d=@($c.candidate_documents|Where-Object{$_.path -ceq $p})[0];$d.text=$d.text.Replace('c4742e99ca6065bba5d5824f8f42399b20ce1961e232ad598e31eefd724cf43b',('0'*64))}},
            [pscustomobject]@{Name='embedded-team-report-hash-drift';Expected='embedded Git-blob identity';Apply={param($c)$p='docs/quality/r0-g0-g1-readiness-audit.md';$d=@($c.candidate_documents|Where-Object{$_.path -ceq $p})[0];$d.text=$d.text.Replace('6c74b8ab2ee1027acede3deb2c4d7147526fc72692e424c0baabc118bc856f8a',('0'*64))}},
            [pscustomobject]@{Name='embedded-workflow-hash-drift';Expected='embedded Git-blob identity';Apply={param($c)$p='docs/tasks/work-packages/R0-GATE-001.md';$d=@($c.candidate_documents|Where-Object{$_.path -ceq $p})[0];$d.text=$d.text.Replace('652da759c39ae6b2847bbe5f5518efdb5183170d1ed9ee76c5d2b4550d1c6bf2',('0'*64))}})
        foreach ($mutation in $mutations) {
            $copy = Copy-R0GovernanceReviewContext -Context $context `
                -SerializedContext $serializedMutationContext
            & $mutation.Apply $copy
            $mutationIssues = @(Get-R0GovernanceReviewIssues -Context $copy)
            $matched = @($mutationIssues | Where-Object {
                    $_.IndexOf($mutation.Expected,
                        [System.StringComparison]::OrdinalIgnoreCase) -ge 0
                }).Count -gt 0
            if ($mutationIssues.Count -eq 0) {
                $fatal.Add("R0 governance review mutation was not rejected: $($mutation.Name)")
            }
            elseif (-not $matched) {
                $fatal.Add("R0 governance review mutation failed for the wrong reason: $($mutation.Name) :: $($mutationIssues -join ' | ')")
            }
            $mutationResults.Add([pscustomobject][ordered]@{
                    name = $mutation.Name
                    rejected = $mutationIssues.Count -gt 0 -and $matched
                    expected_diagnostic = $mutation.Expected
                    diagnostic_matched = $matched
                    detected_issue_count = $mutationIssues.Count
                })
        }

        $utf8 = [System.Text.UTF8Encoding]::new($false)
        $rawMutations = [System.Collections.Generic.List[object]]::new()
        foreach ($rawSource in @(
                [pscustomobject]@{Prefix='scientific-context';Path=$instancePath;Label='Mutated scientific context'},
                [pscustomobject]@{Prefix='provenance-inventory';Path=(Join-Path $RepoRoot 'docs\governance\provenance-inventory.json');Label='Mutated provenance inventory'},
                [pscustomobject]@{Prefix='governance-contract';Path=$contractPath;Label='Mutated governance contract'},
                [pscustomobject]@{Prefix='scientific-context-schema';Path=$schemaPath;Label='Mutated scientific context schema'})) {
            $rawText = [System.IO.File]::ReadAllText(
                $rawSource.Path, [System.Text.UTF8Encoding]::new($false, $true))
            $match = [regex]::Match($rawText,
                '(?m)^(?<indent>\s*)"(?<key>(?:\\.|[^"\\])*)"\s*:\s*"(?<value>[^"]+)"\s*,')
            if (-not $match.Success) {
                $fatal.Add("Cannot construct raw JSON mutations for $($rawSource.Prefix).")
                continue
            }
            $line = $match.Value
            $indent = $match.Groups['indent'].Value
            $key = $match.Groups['key'].Value
            $value = $match.Groups['value'].Value
            $escapedKey = ('\u{0:x4}{1}' -f ([int][char]$key[0]), $key.Substring(1))
            $duplicateText = $rawText.Remove($match.Index, $match.Length).Insert(
                $match.Index, $line + "`n" + $indent + '"' + $key + '": "' + $value + '",')
            $escapedDuplicateText = $rawText.Remove($match.Index, $match.Length).Insert(
                $match.Index, $line + "`n" + $indent + '"' + $escapedKey + '": "' + $value + '",')
            [byte[]]$normalBytes = $utf8.GetBytes($rawText)
            [byte[]]$bomBytes = [byte[]](0xEF, 0xBB, 0xBF) + $normalBytes
            [byte[]]$invalidBytes = [byte[]]$normalBytes.Clone()
            $invalidBytes[10] = 0xFF
            $rawMutations.Add([pscustomobject]@{Name="$($rawSource.Prefix)-duplicate-json-key";Expected='Duplicate JSON object key';Bytes=$utf8.GetBytes($duplicateText);Label=$rawSource.Label})
            $rawMutations.Add([pscustomobject]@{Name="$($rawSource.Prefix)-escaped-duplicate-json-key";Expected='Duplicate JSON object key';Bytes=$utf8.GetBytes($escapedDuplicateText);Label=$rawSource.Label})
            $rawMutations.Add([pscustomobject]@{Name="$($rawSource.Prefix)-utf8-bom";Expected='UTF-8 BOM';Bytes=$bomBytes;Label=$rawSource.Label})
            $rawMutations.Add([pscustomobject]@{Name="$($rawSource.Prefix)-crlf";Expected='LF line endings';Bytes=$utf8.GetBytes($rawText.Replace("`n", "`r`n"));Label=$rawSource.Label})
            $rawMutations.Add([pscustomobject]@{Name="$($rawSource.Prefix)-invalid-utf8";Expected='not strict UTF-8';Bytes=$invalidBytes;Label=$rawSource.Label})
        }
        $tempRoot = [System.IO.Path]::GetTempPath()
        $tempDir = Join-Path $tempRoot ('gnczmkn-r0-gov-' + [guid]::NewGuid().ToString('N'))
        [void][System.IO.Directory]::CreateDirectory($tempDir)
        try {
            foreach ($mutation in $rawMutations) {
                $tempPath = Join-Path $tempDir ($mutation.Name + '.json')
                [System.IO.File]::WriteAllBytes($tempPath, [byte[]]$mutation.Bytes)
                $rawIssues = [System.Collections.Generic.List[string]]::new()
                Test-R0GovernanceTextBytes -Path $tempPath -Issues $rawIssues -Label $mutation.Label
                try {
                    [void](Read-StrictJsonFile -Path $tempPath)
                }
                catch {
                    $rawIssues.Add("Strict JSON failure: $($_.Exception.Message)")
                }
                $matched = @($rawIssues | Where-Object {
                        $_.IndexOf($mutation.Expected,
                            [System.StringComparison]::OrdinalIgnoreCase) -ge 0
                    }).Count -gt 0
                if ($rawIssues.Count -eq 0) {
                    $fatal.Add("R0 governance raw mutation was not rejected: $($mutation.Name)")
                }
                elseif (-not $matched) {
                    $fatal.Add("R0 governance raw mutation failed for the wrong reason: $($mutation.Name) :: $($rawIssues -join ' | ')")
                }
                $mutationResults.Add([pscustomobject][ordered]@{
                        name = $mutation.Name
                        rejected = $rawIssues.Count -gt 0 -and $matched
                        expected_diagnostic = $mutation.Expected
                        diagnostic_matched = $matched
                        detected_issue_count = $rawIssues.Count
                    })
            }
        }
        finally {
            if (Test-Path -LiteralPath $tempDir) {
                [System.IO.Directory]::Delete($tempDir, $true)
            }
        }
    }
    return [pscustomobject][ordered]@{
        Issues = $fatal.ToArray()
        MutationCount = $mutationResults.Count
        MutationResults = @($mutationResults)
        ContextCount = @($context.sidecars).Count
        RuntimeConsumerCount = @($context.consumer_documents | Where-Object {
                Test-R0GovernanceConsumerText ([string](Get-R0GovernanceField $_ 'text'))
            }).Count
    }
}

Export-ModuleMember -Function Test-R0GovernanceReview, Get-R0GovernanceReportInputPaths, `
    Get-R0GovernanceReportInputFacts
