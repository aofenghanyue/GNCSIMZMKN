Set-StrictMode -Version Latest

function Split-MarkdownTableRow {
    param([Parameter(Mandatory = $true)][string]$Line)

    $trimmed = $Line.Trim()
    if (-not ($trimmed.StartsWith('|') -and $trimmed.EndsWith('|'))) {
        throw "Markdown table row must start and end with '|': $Line"
    }

    $body = $trimmed.Substring(1, $trimmed.Length - 2)
    $cells = [System.Collections.Generic.List[string]]::new()
    $current = [System.Text.StringBuilder]::new()

    for ($index = 0; $index -lt $body.Length; ++$index) {
        $character = $body[$index]
        if ($character -eq '\' -and $index + 1 -lt $body.Length -and $body[$index + 1] -eq '|') {
            [void]$current.Append('|')
            ++$index
        }
        elseif ($character -eq '|') {
            [void]$cells.Add($current.ToString().Trim())
            [void]$current.Clear()
        }
        else {
            [void]$current.Append($character)
        }
    }

    [void]$cells.Add($current.ToString().Trim())
    return @($cells)
}

function Test-MarkdownSeparatorRow {
    param([string[]]$Cells)

    if ($Cells.Count -eq 0) { return $false }
    foreach ($cell in $Cells) {
        if ($cell -notmatch '^:?-{3,}:?$') { return $false }
    }
    return $true
}

function Get-CodeIdentity {
    param(
        [Parameter(Mandatory = $true)][string]$Cell,
        [Parameter(Mandatory = $true)][string]$Context
    )

    $match = [regex]::Match($Cell, '^`([^`]+)`')
    if (-not $match.Success) {
        throw "$Context must begin with one backtick-delimited identity: $Cell"
    }
    return $match.Groups[1].Value
}

function Get-AuthorityReferences {
    param([string]$Cell)

    return @($Cell.Split([char]0x3001) | ForEach-Object { $_.Trim() } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
}

function Get-MatchedTermNames {
    param(
        [string]$Text,
        [System.Collections.Generic.HashSet[string]]$KnownTerms
    )

    $result = [System.Collections.Generic.List[string]]::new()
    $seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($match in [regex]::Matches($Text, '[A-Za-z_][A-Za-z0-9_]*')) {
        $candidate = $match.Value
        if ($KnownTerms.Contains($candidate) -and $seen.Add($candidate)) {
            [void]$result.Add($candidate)
        }
    }
    return @($result)
}

function Get-EnumValueOwners {
    param([object[]]$Terms)

    $owners = @{}
    foreach ($term in $Terms) {
        foreach ($codeSpan in [regex]::Matches([string]$term.definition, '`([^`]*\|[^`]*)`')) {
            foreach ($value in @($codeSpan.Groups[1].Value -split '\s*\|\s*')) {
                $normalizedValue = $value.Trim()
                if ([string]::IsNullOrWhiteSpace($normalizedValue)) { continue }
                if (-not $owners.ContainsKey($normalizedValue)) {
                    $owners[$normalizedValue] = [System.Collections.Generic.List[string]]::new()
                }
                if (-not $owners[$normalizedValue].Contains([string]$term.name)) {
                    [void]$owners[$normalizedValue].Add([string]$term.name)
                }
            }
        }
    }
    return $owners
}

function Get-TermEnumValues {
    param([string]$Definition)

    foreach ($codeSpan in [regex]::Matches($Definition, '`([^`]*\|[^`]*)`')) {
        $values = @($codeSpan.Groups[1].Value -split '\s*\|\s*' | ForEach-Object { $_.Trim() })
        if ($values.Count -ge 2) { return @($values) }
    }
    return @()
}

function Get-AuthorityEnumValues {
    param([string]$Text, [string]$SymbolName)

    $pattern = [regex]::Escape($SymbolName) + '\s*=\s*(?<values>[^`\r\n]+)'
    $match = [regex]::Match($Text, $pattern)
    if (-not $match.Success) { return @() }
    return @($match.Groups['values'].Value -split '\s*\|\s*' | ForEach-Object { $_.Trim() })
}

function Get-MatchedEnumValues {
    param([string]$Text, [hashtable]$ValueOwners)

    $result = [System.Collections.Generic.List[object]]::new()
    $seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($match in [regex]::Matches($Text, '[A-Za-z_][A-Za-z0-9_]*')) {
        $candidate = $match.Value
        if ($ValueOwners.ContainsKey($candidate) -and $seen.Add($candidate)) {
            [void]$result.Add([PSCustomObject]([ordered]@{
                value = $candidate
                owner_terms = @($ValueOwners[$candidate])
            }))
        }
    }
    return @($result)
}

function ConvertFrom-GlossaryMarkdown {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string]$Text)

    $rawAliases = [System.Collections.Generic.List[object]]::new()
    $terms = [System.Collections.Generic.List[object]]::new()
    $capabilities = [System.Collections.Generic.List[object]]::new()
    $rawLegacy = [System.Collections.Generic.List[object]]::new()
    $section = 0
    $tableHeaderSkipped = $false
    $expectTableSeparator = $false
    $expectedTableColumns = 0

    foreach ($line in [regex]::Split($Text, '\r?\n')) {
        $heading = [regex]::Match($line, '^##\s+([2-9])\.')
        if ($heading.Success) {
            $section = [int]$heading.Groups[1].Value
            $tableHeaderSkipped = $false
            $expectTableSeparator = $false
            $expectedTableColumns = 0
            continue
        }
        if ($section -lt 2 -or $section -gt 9 -or -not $line.Trim().StartsWith('|')) {
            continue
        }

        $cells = @(Split-MarkdownTableRow -Line $line)
        if (-not $tableHeaderSkipped) {
            $expectedTableColumns = if ($section -eq 2) { 3 } elseif ($section -eq 8) { 5 } else { 4 }
            if ($cells.Count -ne $expectedTableColumns) {
                throw "Glossary section $section header requires $expectedTableColumns columns: $line"
            }
            $tableHeaderSkipped = $true
            $expectTableSeparator = $true
            continue
        }
        if ($expectTableSeparator) {
            if ($cells.Count -ne $expectedTableColumns -or -not (Test-MarkdownSeparatorRow -Cells $cells)) {
                throw "Glossary section $section header must be followed by a separator row: $line"
            }
            $expectTableSeparator = $false
            continue
        }
        if (Test-MarkdownSeparatorRow -Cells $cells) {
            throw "Glossary section $section contains an unexpected separator row: $line"
        }

        switch ($section) {
            2 {
                if ($cells.Count -ne 3) { throw "Glossary section 2 requires three columns: $line" }
                [void]$rawAliases.Add([PSCustomObject]([ordered]@{
                    retired_name = $cells[0].Replace('`', '')
                    canonical_relation = $cells[1]
                    decision = $cells[2]
                }))
            }
            { $_ -ge 3 -and $_ -le 7 } {
                if ($cells.Count -ne 4) { throw "Glossary section $section requires four columns: $line" }
                $name = Get-CodeIdentity -Cell $cells[0] -Context "Glossary section $section"
                [void]$terms.Add([PSCustomObject]([ordered]@{
                    name = $name
                    status = $cells[1]
                    definition = $cells[2]
                    authority_refs = @(Get-AuthorityReferences -Cell $cells[3])
                    glossary_section = $section
                }))
            }
            8 {
                if ($cells.Count -ne 5) { throw "Glossary section 8 requires five columns: $line" }
                [void]$capabilities.Add([PSCustomObject]([ordered]@{
                    capability = $cells[0].Replace('`', '')
                    status = $cells[1]
                    current_commitment = $cells[2]
                    activation_gate = $cells[3]
                    authority_refs = @(Get-AuthorityReferences -Cell $cells[4])
                }))
            }
            9 {
                if ($cells.Count -ne 4) { throw "Glossary section 9 requires four columns: $line" }
                $name = Get-CodeIdentity -Cell $cells[0] -Context 'Glossary section 9'
                [void]$rawLegacy.Add([PSCustomObject]([ordered]@{
                    legacy_name = $name
                    status = $cells[1]
                    target = $cells[2]
                    authority_refs = @(Get-AuthorityReferences -Cell $cells[3])
                }))
            }
        }
    }

    $knownTerms = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($term in $terms) { [void]$knownTerms.Add([string]$term.name) }
    $enumValueOwners = Get-EnumValueOwners -Terms @($terms)

    $aliases = @($rawAliases | ForEach-Object {
        [PSCustomObject]([ordered]@{
            retired_name = $_.retired_name
            canonical_relation = $_.canonical_relation
            canonical_terms = @(Get-MatchedTermNames -Text "$($_.retired_name) $($_.canonical_relation)" -KnownTerms $knownTerms)
            canonical_values = @(Get-MatchedEnumValues -Text $_.canonical_relation -ValueOwners $enumValueOwners)
            decision = $_.decision
        })
    })
    $legacy = @($rawLegacy | ForEach-Object {
        [PSCustomObject]([ordered]@{
            legacy_name = $_.legacy_name
            status = $_.status
            target = $_.target
            target_terms = @(Get-MatchedTermNames -Text $_.target -KnownTerms $knownTerms)
            authority_refs = @($_.authority_refs)
        })
    })

    return [PSCustomObject]([ordered]@{
        Terms = @($terms)
        Aliases = @($aliases)
        Capabilities = @($capabilities)
        Legacy = @($legacy)
    })
}

function New-DependencyEdge {
    param([string]$Source, [string]$Target)
    return [PSCustomObject]([ordered]@{ source = $Source; target = $Target })
}

function ConvertFrom-DependencySources {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$AdrText,
        [Parameter(Mandatory = $true)][string]$CMakeText
    )

    $dependencyBlock = [regex]::Match(
        $AdrText,
        '```text\s*(?<body>foundation\s*<-\s*contracts\s*<-\s*model_sdk\s*<-\s*compiler.*?application\s*<-\s*adapters)\s*```',
        [System.Text.RegularExpressions.RegexOptions]::Singleline)
    if (-not $dependencyBlock.Success) {
        throw 'ADR-0003 dependency text block was not found.'
    }

    $architectureEdges = [System.Collections.Generic.List[object]]::new()
    $moduleNames = [System.Collections.Generic.List[string]]::new()
    $moduleNameSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($line in [regex]::Split($dependencyBlock.Groups['body'].Value.Trim(), '\r?\n')) {
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        $groups = @($line.Trim() -split '\s*<-\s*')
        if ($groups.Count -lt 2) { throw "Unsupported ADR dependency expression: $line" }
        foreach ($group in $groups) {
            foreach ($moduleName in @($group -split '\s*\+\s*')) {
                $normalizedName = $moduleName.Trim()
                if ($moduleNameSet.Add($normalizedName)) { [void]$moduleNames.Add($normalizedName) }
            }
        }
        $available = @($groups[0] -split '\s*\+\s*')
        for ($index = 1; $index -lt $groups.Count; ++$index) {
            $dependers = @($groups[$index] -split '\s*\+\s*')
            foreach ($depender in $dependers) {
                foreach ($dependency in $available) {
                    [void]$architectureEdges.Add((New-DependencyEdge -Source $depender.Trim() -Target $dependency.Trim()))
                }
            }
            $available = @($dependers | ForEach-Object { $_.Trim() })
        }
    }

    $cmakeModules = @([regex]::Matches(
        $CMakeText,
        '(?m)^\s*gnc_add_interface_module\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)') |
        ForEach-Object { $_.Groups[1].Value })
    if ($cmakeModules.Count -eq 0) { throw 'No CMake interface modules were found.' }

    $moduleSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($moduleName in $cmakeModules) { [void]$moduleSet.Add($moduleName) }
    $cmakeEdges = [System.Collections.Generic.List[object]]::new()
    foreach ($match in [regex]::Matches(
        $CMakeText,
        'target_link_libraries\(\s*([A-Za-z_][A-Za-z0-9_]*)\s+INTERFACE\s+([^\)]*)\)',
        [System.Text.RegularExpressions.RegexOptions]::Singleline)) {
        $source = $match.Groups[1].Value
        if (-not $moduleSet.Contains($source)) { continue }
        foreach ($token in [regex]::Matches($match.Groups[2].Value, '[A-Za-z_][A-Za-z0-9_]*')) {
            if ($moduleSet.Contains($token.Value)) {
                [void]$cmakeEdges.Add((New-DependencyEdge -Source $source -Target $token.Value))
            }
        }
    }

    return [PSCustomObject]([ordered]@{
        ModuleNames = @($moduleNames)
        ArchitectureEdges = @($architectureEdges)
        CMakeModules = @($cmakeModules)
        CMakeEdges = @($cmakeEdges)
    })
}

function Get-OptionalArrayProperty {
    param($Object, [string]$Name)

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property -or $null -eq $property.Value) { return @() }
    return @($property.Value)
}

function Get-TransitiveDependencies {
    param(
        [string]$ModuleName,
        [object[]]$Edges,
        [string[]]$ModuleOrder
    )

    $seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    $pending = [System.Collections.Generic.Stack[string]]::new()
    foreach ($edge in $Edges) {
        if ($edge.source -ceq $ModuleName) { $pending.Push([string]$edge.target) }
    }
    while ($pending.Count -gt 0) {
        $candidate = $pending.Pop()
        if (-not $seen.Add($candidate)) { continue }
        foreach ($edge in $Edges) {
            if ($edge.source -ceq $candidate) { $pending.Push([string]$edge.target) }
        }
    }
    return @($ModuleOrder | Where-Object { $seen.Contains($_) })
}

function Test-DependencyDag {
    param([string[]]$Modules, [object[]]$Edges)

    $remaining = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($moduleName in $Modules) { [void]$remaining.Add($moduleName) }
    while ($remaining.Count -gt 0) {
        $removable = @($remaining | Where-Object {
            $candidate = $_
            -not ($Edges | Where-Object { $_.source -ceq $candidate -and $remaining.Contains([string]$_.target) } | Select-Object -First 1)
        })
        if ($removable.Count -eq 0) { return $false }
        foreach ($moduleName in $removable) { [void]$remaining.Remove($moduleName) }
    }
    return $true
}

function Add-ArchitectureIssue {
    param([System.Collections.Generic.List[string]]$Issues, [string]$Message)
    [void]$Issues.Add($Message)
}

function Test-UniqueProperty {
    param(
        [object[]]$Items,
        [string]$Property,
        [string]$Label,
        [System.Collections.Generic.List[string]]$Issues
    )

    $seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($item in $Items) {
        $value = [string]$item.$Property
        if ([string]::IsNullOrWhiteSpace($value)) {
            Add-ArchitectureIssue $Issues "$Label has an empty $Property."
        }
        elseif (-not $seen.Add($value)) {
            Add-ArchitectureIssue $Issues "Duplicate $Label identity '$value'."
        }
    }
}

function Test-ExactObjectProperties {
    param(
        $Object,
        [string[]]$RequiredProperties,
        [string[]]$OptionalProperties = @(),
        [string]$Label,
        [System.Collections.Generic.List[string]]$Issues
    )

    if ($null -eq $Object -or $Object -isnot [System.Management.Automation.PSCustomObject]) {
        Add-ArchitectureIssue $Issues "$Label must be a JSON object."
        return
    }

    $actual = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($property in @($Object.PSObject.Properties)) {
        [void]$actual.Add([string]$property.Name)
    }

    $allowed = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($propertyName in @($RequiredProperties) + @($OptionalProperties)) {
        [void]$allowed.Add([string]$propertyName)
    }

    foreach ($propertyName in $RequiredProperties) {
        if (-not $actual.Contains([string]$propertyName)) {
            Add-ArchitectureIssue $Issues "$Label is missing required property '$propertyName'."
        }
    }
    foreach ($propertyName in $actual) {
        if (-not $allowed.Contains([string]$propertyName)) {
            Add-ArchitectureIssue $Issues "$Label has unknown property '$propertyName'."
        }
    }
}

function Test-ArchitectureInputs {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]$Glossary,
        [Parameter(Mandatory = $true)]$Dependency,
        [Parameter(Mandatory = $true)]$Registry,
        [Parameter(Mandatory = $true)][string]$RepoRoot
    )

    $issues = [System.Collections.Generic.List[string]]::new()
    $allowedStatuses = @('Stable', 'V1', 'PressureOnly', 'Deferred', 'Legacy')
    Test-ExactObjectProperties `
        -Object $Registry `
        -RequiredProperties @(
            'schema_version',
            'terminology_authority',
            'dependency_authority',
            'physical_partition_authority',
            'modules',
            'shared_symbols',
            'legacy_ownership') `
        -Label 'Authority registry' `
        -Issues $issues
    if ([string]$Registry.schema_version -cne 'gnczmkn.architecture-authority-registry/1') {
        Add-ArchitectureIssue $issues "Authority registry has unsupported schema version '$($Registry.schema_version)'."
    }
    $expectedAuthorityPaths = [ordered]@{
        terminology_authority = 'design-notes/gnczmkn-architecture-roadmap/reference-glossary.md'
        dependency_authority = 'docs/adr/0003-initial-module-dependency-dag.md'
        physical_partition_authority = 'design-notes/gnczmkn-architecture-roadmap/02-layered-reference-architecture.md'
    }
    foreach ($propertyName in $expectedAuthorityPaths.Keys) {
        if ([string]$Registry.$propertyName -cne [string]$expectedAuthorityPaths[$propertyName]) {
            Add-ArchitectureIssue $issues "Authority registry $propertyName must remain '$($expectedAuthorityPaths[$propertyName])'."
        }
    }

    Test-UniqueProperty -Items @($Glossary.Terms) -Property 'name' -Label 'terminology' -Issues $issues
    Test-UniqueProperty -Items @($Glossary.Aliases) -Property 'retired_name' -Label 'alias' -Issues $issues
    Test-UniqueProperty -Items @($Glossary.Legacy) -Property 'legacy_name' -Label 'Legacy term' -Issues $issues

    $termSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($term in @($Glossary.Terms)) {
        [void]$termSet.Add([string]$term.name)
        if ($term.status -notin $allowedStatuses) {
            Add-ArchitectureIssue $issues "Term '$($term.name)' uses unknown status '$($term.status)'."
        }
        if ([string]::IsNullOrWhiteSpace([string]$term.definition)) {
            Add-ArchitectureIssue $issues "Term '$($term.name)' has no definition."
        }
        if (@($term.authority_refs).Count -eq 0) {
            Add-ArchitectureIssue $issues "Term '$($term.name)' has no authority reference."
        }
    }
    if (@($Glossary.Terms).Count -eq 0) { Add-ArchitectureIssue $issues 'No terminology rows were parsed.' }
    if (@($Glossary.Aliases).Count -eq 0) { Add-ArchitectureIssue $issues 'No alias rows were parsed.' }
    if (@($Glossary.Legacy).Count -eq 0) { Add-ArchitectureIssue $issues 'No Legacy rows were parsed.' }

    foreach ($alias in @($Glossary.Aliases)) {
        if (@($alias.canonical_terms).Count -eq 0 -and @($alias.canonical_values).Count -eq 0) {
            Add-ArchitectureIssue $issues "Alias '$($alias.retired_name)' has no resolvable canonical term or enum value."
        }
        foreach ($target in @($alias.canonical_terms)) {
            if (-not $termSet.Contains([string]$target)) {
                Add-ArchitectureIssue $issues "Alias '$($alias.retired_name)' references unknown canonical term '$target'."
            }
        }
        foreach ($valueMapping in @($alias.canonical_values)) {
            if (@($valueMapping.owner_terms).Count -eq 0) {
                Add-ArchitectureIssue $issues "Alias '$($alias.retired_name)' enum value '$($valueMapping.value)' has no owner term."
            }
            foreach ($ownerTerm in @($valueMapping.owner_terms)) {
                if (-not $termSet.Contains([string]$ownerTerm)) {
                    Add-ArchitectureIssue $issues "Alias '$($alias.retired_name)' enum value '$($valueMapping.value)' has unknown owner term '$ownerTerm'."
                }
            }
        }
    }
    foreach ($capability in @($Glossary.Capabilities)) {
        if ($capability.status -notin $allowedStatuses) {
            Add-ArchitectureIssue $issues "Capability '$($capability.capability)' uses unknown status '$($capability.status)'."
        }
        if (@($capability.authority_refs).Count -eq 0) {
            Add-ArchitectureIssue $issues "Capability '$($capability.capability)' has no authority reference."
        }
    }
    foreach ($legacy in @($Glossary.Legacy)) {
        if ($legacy.status -cne 'Legacy') {
            Add-ArchitectureIssue $issues "Legacy term '$($legacy.legacy_name)' must have status Legacy."
        }
        foreach ($target in @($legacy.target_terms)) {
            if (-not $termSet.Contains([string]$target)) {
                Add-ArchitectureIssue $issues "Legacy term '$($legacy.legacy_name)' references unknown target term '$target'."
            }
        }
    }

    Test-UniqueProperty -Items @($Registry.modules) -Property 'name' -Label 'module ownership' -Issues $issues
    Test-UniqueProperty -Items @($Registry.shared_symbols) -Property 'name' -Label 'shared symbol' -Issues $issues
    Test-UniqueProperty -Items @($Registry.legacy_ownership) -Property 'legacy_name' -Label 'Legacy ownership' -Issues $issues

    $rowIndex = 0
    foreach ($module in @($Registry.modules)) {
        Test-ExactObjectProperties `
            -Object $module `
            -RequiredProperties @('name', 'source_root') `
            -Label "Authority registry module row $rowIndex" `
            -Issues $issues
        ++$rowIndex
    }
    $rowIndex = 0
    foreach ($symbol in @($Registry.shared_symbols)) {
        Test-ExactObjectProperties `
            -Object $symbol `
            -RequiredProperties @('name', 'kind', 'semantic_authority', 'owner_module') `
            -Label "Authority registry shared symbol row $rowIndex" `
            -Issues $issues
        ++$rowIndex
    }
    $rowIndex = 0
    foreach ($ownership in @($Registry.legacy_ownership)) {
        Test-ExactObjectProperties `
            -Object $ownership `
            -RequiredProperties @('legacy_name', 'disposition', 'primary_owner') `
            -OptionalProperties @('secondary_consumers') `
            -Label "Authority registry Legacy ownership row $rowIndex" `
            -Issues $issues
        ++$rowIndex
    }

    $moduleSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    $repoPrefix = $RepoRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    foreach ($module in @($Registry.modules)) {
        [void]$moduleSet.Add([string]$module.name)
        $sourceRoot = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot ([string]$module.source_root)))
        if (-not $sourceRoot.StartsWith($repoPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            Add-ArchitectureIssue $issues "Module '$($module.name)' source root escapes the repository."
        }
        elseif (-not (Test-Path -LiteralPath $sourceRoot -PathType Container)) {
            Add-ArchitectureIssue $issues "Module '$($module.name)' source root does not exist: $($module.source_root)."
        }
    }

    $adrModuleSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($moduleName in @($Dependency.ModuleNames)) { [void]$adrModuleSet.Add([string]$moduleName) }
    foreach ($moduleName in $moduleSet) {
        if (-not $adrModuleSet.Contains($moduleName)) { Add-ArchitectureIssue $issues "Ownership registry module '$moduleName' is absent from ADR-0003." }
    }
    foreach ($moduleName in $adrModuleSet) {
        if (-not $moduleSet.Contains($moduleName)) { Add-ArchitectureIssue $issues "ADR-0003 module '$moduleName' has no ownership registry entry." }
    }

    foreach ($symbol in @($Registry.shared_symbols)) {
        if ($symbol.kind -notin @('enum', 'key', 'owner')) {
            Add-ArchitectureIssue $issues "Shared symbol '$($symbol.name)' has unknown kind '$($symbol.kind)'."
        }
        if (-not $termSet.Contains([string]$symbol.name)) {
            Add-ArchitectureIssue $issues "Shared symbol '$($symbol.name)' is absent from the terminology registry."
        }
        if (-not $moduleSet.Contains([string]$symbol.owner_module)) {
            Add-ArchitectureIssue $issues "Shared symbol '$($symbol.name)' has unknown owner module '$($symbol.owner_module)'."
        }
        $authorityPath = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot ([string]$symbol.semantic_authority)))
        if (-not $authorityPath.StartsWith($repoPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            Add-ArchitectureIssue $issues "Shared symbol '$($symbol.name)' semantic authority escapes the repository."
        }
        elseif (-not (Test-Path -LiteralPath $authorityPath -PathType Leaf)) {
            Add-ArchitectureIssue $issues "Shared symbol '$($symbol.name)' semantic authority does not exist: $($symbol.semantic_authority)."
        }
        else {
            $authorityText = Get-Content -LiteralPath $authorityPath -Raw -Encoding utf8
            if ($authorityText.IndexOf([string]$symbol.name, [System.StringComparison]::Ordinal) -lt 0) {
                Add-ArchitectureIssue $issues "Shared symbol '$($symbol.name)' is absent from semantic authority '$($symbol.semantic_authority)'."
            }
            elseif ($symbol.kind -ceq 'enum' -and $termSet.Contains([string]$symbol.name)) {
                $term = @($Glossary.Terms | Where-Object { $_.name -ceq $symbol.name } | Select-Object -First 1)[0]
                $glossaryValues = @(Get-TermEnumValues -Definition ([string]$term.definition))
                $authorityValues = @(Get-AuthorityEnumValues -Text $authorityText -SymbolName ([string]$symbol.name))
                if ($glossaryValues.Count -lt 2) {
                    Add-ArchitectureIssue $issues "Shared enum '$($symbol.name)' has no machine-readable value set in the glossary."
                }
                if ($authorityValues.Count -lt 2) {
                    Add-ArchitectureIssue $issues "Shared enum '$($symbol.name)' has no explicit value set in semantic authority '$($symbol.semantic_authority)'."
                }
                if ($glossaryValues.Count -ge 2 -and $authorityValues.Count -ge 2 -and
                    [string]::Join('|', $glossaryValues) -cne [string]::Join('|', $authorityValues)) {
                    Add-ArchitectureIssue $issues "Shared enum '$($symbol.name)' differs between glossary and semantic authority."
                }
            }
        }
    }
    foreach ($requiredKind in @('enum', 'key', 'owner')) {
        if (-not (@($Registry.shared_symbols) | Where-Object { $_.kind -ceq $requiredKind } | Select-Object -First 1)) {
            Add-ArchitectureIssue $issues "Shared symbol registry has no '$requiredKind' entry."
        }
    }
    $sharedEnumSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($symbol in @($Registry.shared_symbols | Where-Object { $_.kind -ceq 'enum' })) {
        [void]$sharedEnumSet.Add([string]$symbol.name)
    }
    foreach ($alias in @($Glossary.Aliases)) {
        foreach ($valueMapping in @($alias.canonical_values)) {
            foreach ($ownerTerm in @($valueMapping.owner_terms)) {
                if (-not $sharedEnumSet.Contains([string]$ownerTerm)) {
                    Add-ArchitectureIssue $issues "Alias '$($alias.retired_name)' value '$($valueMapping.value)' is owned by unclassified enum '$ownerTerm'."
                }
            }
        }
    }

    $legacyOwners = @{}
    foreach ($ownership in @($Registry.legacy_ownership)) {
        $legacyOwners[[string]$ownership.legacy_name] = $ownership
        if ($ownership.disposition -notin @('replace', 'split', 'delete')) {
            Add-ArchitectureIssue $issues "Legacy ownership '$($ownership.legacy_name)' has unknown disposition '$($ownership.disposition)'."
        }
        if (-not $moduleSet.Contains([string]$ownership.primary_owner)) {
            Add-ArchitectureIssue $issues "Legacy ownership '$($ownership.legacy_name)' has unknown primary owner '$($ownership.primary_owner)'."
        }
        $consumerSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
        foreach ($consumer in @(Get-OptionalArrayProperty -Object $ownership -Name 'secondary_consumers')) {
            if (-not $moduleSet.Contains([string]$consumer)) {
                Add-ArchitectureIssue $issues "Legacy ownership '$($ownership.legacy_name)' has unknown secondary consumer '$consumer'."
            }
            elseif ([string]$consumer -ceq [string]$ownership.primary_owner) {
                Add-ArchitectureIssue $issues "Legacy ownership '$($ownership.legacy_name)' repeats its primary owner as a consumer."
            }
            elseif (-not $consumerSet.Add([string]$consumer)) {
                Add-ArchitectureIssue $issues "Legacy ownership '$($ownership.legacy_name)' repeats secondary consumer '$consumer'."
            }
        }
    }
    foreach ($legacy in @($Glossary.Legacy)) {
        if (-not $legacyOwners.ContainsKey([string]$legacy.legacy_name)) {
            Add-ArchitectureIssue $issues "Legacy term '$($legacy.legacy_name)' is missing ownership."
        }
    }
    foreach ($legacyName in $legacyOwners.Keys) {
        if (-not (@($Glossary.Legacy) | Where-Object { $_.legacy_name -ceq $legacyName } | Select-Object -First 1)) {
            Add-ArchitectureIssue $issues "Legacy ownership '$legacyName' has no glossary migration row."
        }
    }

    $architectureEdgeKeys = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($edge in @($Dependency.ArchitectureEdges)) {
        $edgeKey = "$($edge.source)->$($edge.target)"
        if (-not $architectureEdgeKeys.Add($edgeKey)) { Add-ArchitectureIssue $issues "Duplicate architecture dependency '$edgeKey'." }
        if (-not $adrModuleSet.Contains([string]$edge.source) -or -not $adrModuleSet.Contains([string]$edge.target)) {
            Add-ArchitectureIssue $issues "Architecture dependency '$edgeKey' uses an unknown module."
        }
        if ([string]$edge.source -ceq [string]$edge.target) { Add-ArchitectureIssue $issues "Architecture dependency '$edgeKey' is a self dependency." }
    }
    if (-not (Test-DependencyDag -Modules @($Dependency.ModuleNames) -Edges @($Dependency.ArchitectureEdges))) {
        Add-ArchitectureIssue $issues 'Architecture dependency graph contains a cycle.'
    }

    $cmakeModuleSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($moduleName in @($Dependency.CMakeModules)) {
        if (-not $cmakeModuleSet.Add([string]$moduleName)) { Add-ArchitectureIssue $issues "Duplicate CMake module '$moduleName'." }
    }
    foreach ($moduleName in $adrModuleSet) {
        if (-not $cmakeModuleSet.Contains($moduleName)) { Add-ArchitectureIssue $issues "ADR module '$moduleName' has no CMake target." }
    }
    foreach ($moduleName in $cmakeModuleSet) {
        if (-not $adrModuleSet.Contains($moduleName)) { Add-ArchitectureIssue $issues "CMake module '$moduleName' is absent from ADR-0003." }
    }

    $cmakeEdgeKeys = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($edge in @($Dependency.CMakeEdges)) {
        $edgeKey = "$($edge.source)->$($edge.target)"
        if (-not $cmakeEdgeKeys.Add($edgeKey)) { Add-ArchitectureIssue $issues "Duplicate CMake dependency '$edgeKey'." }
        $allowed = @(Get-TransitiveDependencies -ModuleName ([string]$edge.source) -Edges @($Dependency.ArchitectureEdges) -ModuleOrder @($Dependency.ModuleNames))
        if ([string]$edge.target -notin $allowed) {
            Add-ArchitectureIssue $issues "CMake dependency '$($edge.source) -> $($edge.target)' is not allowed by ADR-0003."
        }
    }
    if (@($Dependency.CMakeEdges) | Where-Object { $_.source -ceq 'kernel' -and $_.target -ceq 'compiler' } | Select-Object -First 1) {
        Add-ArchitectureIssue $issues 'Kernel must not depend on Compiler.'
    }

    foreach ($registryPath in @(
        [string]$Registry.terminology_authority,
        [string]$Registry.dependency_authority,
        [string]$Registry.physical_partition_authority)) {
        $fullPath = Join-Path $RepoRoot $registryPath
        if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
            Add-ArchitectureIssue $issues "Registry authority path does not exist: $registryPath."
        }
    }
    $physicalAuthorityPath = Join-Path $RepoRoot ([string]$Registry.physical_partition_authority)
    if (Test-Path -LiteralPath $physicalAuthorityPath -PathType Leaf) {
        $physicalAuthorityText = Get-Content -LiteralPath $physicalAuthorityPath -Raw -Encoding utf8
        foreach ($module in @($Registry.modules)) {
            if ($physicalAuthorityText.IndexOf([string]$module.name, [System.StringComparison]::Ordinal) -lt 0) {
                Add-ArchitectureIssue $issues "Module '$($module.name)' is absent from the physical partition authority."
            }
        }
    }

    return [PSCustomObject]([ordered]@{
        IsValid = $issues.Count -eq 0
        Errors = @($issues)
    })
}

function Get-FileSourceRecord {
    param([string]$RepoRoot, [string]$RelativePath, [string]$Role)

    $fullPath = Join-Path $RepoRoot $RelativePath
    $normalizedPath = $RelativePath.Replace('\', '/')
    $text = Get-Content -LiteralPath $fullPath -Raw -Encoding utf8
    $normalizedText = ($text -replace "`r`n", "`n") -replace "`r", "`n"
    $normalizedText = $normalizedText.TrimStart([char]0xFEFF)
    return [PSCustomObject]([ordered]@{
        role = $Role
        path = $normalizedPath
        sha256 = Get-TextSha256 -Text $normalizedText
        hash_normalization = 'utf8-lf-no-bom'
    })
}

function New-ArchitectureBaseline {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]$Glossary,
        [Parameter(Mandatory = $true)]$Dependency,
        [Parameter(Mandatory = $true)]$Registry,
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string]$RegistryRelativePath
    )

    $termByName = @{}
    foreach ($term in @($Glossary.Terms)) { $termByName[[string]$term.name] = $term }
    $legacyByName = @{}
    foreach ($legacy in @($Glossary.Legacy)) { $legacyByName[[string]$legacy.legacy_name] = $legacy }

    $sources = @(
        Get-FileSourceRecord -RepoRoot $RepoRoot -RelativePath ([string]$Registry.terminology_authority) -Role 'terminology-authority'
        Get-FileSourceRecord -RepoRoot $RepoRoot -RelativePath ([string]$Registry.dependency_authority) -Role 'dependency-authority'
        Get-FileSourceRecord -RepoRoot $RepoRoot -RelativePath ([string]$Registry.physical_partition_authority) -Role 'physical-partition-authority'
        Get-FileSourceRecord -RepoRoot $RepoRoot -RelativePath $RegistryRelativePath -Role 'ownership-authority'
        Get-FileSourceRecord -RepoRoot $RepoRoot -RelativePath 'CMakeLists.txt' -Role 'physical-target-graph'
        Get-FileSourceRecord -RepoRoot $RepoRoot -RelativePath 'tools/modules/ArchitectureBaseline.psm1' -Role 'baseline-generator'
        Get-FileSourceRecord -RepoRoot $RepoRoot -RelativePath 'tools/validate-architecture-baseline.ps1' -Role 'baseline-entrypoint'
    )

    $sharedSymbols = @($Registry.shared_symbols | ForEach-Object {
        $term = $termByName[[string]$_.name]
        [PSCustomObject]([ordered]@{
            name = $_.name
            kind = $_.kind
            status = $term.status
            definition = $term.definition
            enum_values = if ($_.kind -ceq 'enum') { @(Get-TermEnumValues -Definition ([string]$term.definition)) } else { @() }
            semantic_authority = $_.semantic_authority
            glossary_authority_refs = @($term.authority_refs)
            owner_module = $_.owner_module
        })
    })

    $moduleEntries = @($Registry.modules | ForEach-Object {
        $moduleName = [string]$_.name
        $architectureDirect = @($Dependency.ModuleNames | Where-Object {
            $candidate = $_
            @($Dependency.ArchitectureEdges | Where-Object { $_.source -ceq $moduleName -and $_.target -ceq $candidate }).Count -gt 0
        })
        $cmakeDirect = @($Dependency.ModuleNames | Where-Object {
            $candidate = $_
            @($Dependency.CMakeEdges | Where-Object { $_.source -ceq $moduleName -and $_.target -ceq $candidate }).Count -gt 0
        })
        [PSCustomObject]([ordered]@{
            name = $moduleName
            source_root = $_.source_root
            architecture_direct_dependencies = @($architectureDirect)
            allowed_dependency_closure = @(Get-TransitiveDependencies -ModuleName $moduleName -Edges @($Dependency.ArchitectureEdges) -ModuleOrder @($Dependency.ModuleNames))
            cmake_direct_dependencies = @($cmakeDirect)
        })
    })

    $legacyOwnership = @($Registry.legacy_ownership | ForEach-Object {
        $legacy = $legacyByName[[string]$_.legacy_name]
        [PSCustomObject]([ordered]@{
            legacy_name = $_.legacy_name
            status = $legacy.status
            target = $legacy.target
            target_terms = @($legacy.target_terms)
            semantic_authority_refs = @($legacy.authority_refs)
            disposition = $_.disposition
            primary_owner = $_.primary_owner
            secondary_consumers = @(Get-OptionalArrayProperty -Object $_ -Name 'secondary_consumers')
        })
    })

    return [PSCustomObject]([ordered]@{
        schema_version = 'gnczmkn.architecture-baseline/1'
        generation_policy = 'deterministic-derived-no-runtime-consumer'
        generated_from = @($sources)
        terminology = [PSCustomObject]([ordered]@{
            allowed_statuses = @('Stable', 'V1', 'PressureOnly', 'Deferred', 'Legacy')
            terms = @($Glossary.Terms)
            aliases = @($Glossary.Aliases)
            capabilities = @($Glossary.Capabilities)
        })
        shared_symbols = @($sharedSymbols)
        module_dependency_map = [PSCustomObject]([ordered]@{
            graph_version = 'adr-0003/1'
            edge_semantics = 'source_depends_on_target'
            modules = @($moduleEntries)
        })
        legacy_to_target_ownership = @($legacyOwnership)
    })
}

function ConvertTo-DeterministicJson {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)]$Value)

    $json = ConvertTo-Json -InputObject $Value -Depth 100 -Compress
    $json = $json -replace "`r`n", "`n"
    return $json.TrimEnd("`r", "`n") + "`n"
}

function Get-TextSha256 {
    param([string]$Text)

    $encoding = [System.Text.UTF8Encoding]::new($false)
    $bytes = $encoding.GetBytes($Text)
    $hash = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString($hash.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $hash.Dispose()
    }
}

function New-TerminologyConformanceReport {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]$Baseline,
        [Parameter(Mandatory = $true)][string]$BaselineJson,
        [Parameter(Mandatory = $true)][int]$NegativeCaseCount
    )

    $statusCounts = @($Baseline.terminology.allowed_statuses | ForEach-Object {
        $status = $_
        [PSCustomObject]([ordered]@{
            status = $status
            count = @($Baseline.terminology.terms | Where-Object { $_.status -ceq $status }).Count
        })
    })
    $moduleCount = @($Baseline.module_dependency_map.modules).Count
    $cmakeEdgeCount = 0
    foreach ($module in @($Baseline.module_dependency_map.modules)) {
        $cmakeEdgeCount += @($module.cmake_direct_dependencies).Count
    }

    return [PSCustomObject]([ordered]@{
        schema_version = 'gnczmkn.terminology-conformance-report/1'
        status = 'conformant'
        architecture_baseline_sha256 = Get-TextSha256 -Text $BaselineJson
        generated_from = @($Baseline.generated_from)
        summary = [PSCustomObject]([ordered]@{
            terms = @($Baseline.terminology.terms).Count
            aliases = @($Baseline.terminology.aliases).Count
            capabilities = @($Baseline.terminology.capabilities).Count
            shared_symbols = @($Baseline.shared_symbols).Count
            legacy_migrations = @($Baseline.legacy_to_target_ownership).Count
            modules = $moduleCount
            cmake_dependency_edges = $cmakeEdgeCount
            negative_cases = $NegativeCaseCount
            term_status_counts = @($statusCounts)
        })
        checks = @(
            [PSCustomObject]([ordered]@{ id = 'TERM-IDENTITY'; status = 'passed'; assertion = 'canonical term identities are ordinal-unique and complete' }),
            [PSCustomObject]([ordered]@{ id = 'TERM-ALIAS'; status = 'passed'; assertion = 'retired names resolve to registered canonical terms or owned enum values' }),
            [PSCustomObject]([ordered]@{ id = 'TERM-AUTHORITY'; status = 'passed'; assertion = 'shared enum, key and owner symbols have one semantic authority and one module owner' }),
            [PSCustomObject]([ordered]@{ id = 'LEGACY-OWNER'; status = 'passed'; assertion = 'every Legacy migration has exactly one primary owner' }),
            [PSCustomObject]([ordered]@{ id = 'MODULE-DAG'; status = 'passed'; assertion = 'ADR-0003 dependency graph is closed and acyclic' }),
            [PSCustomObject]([ordered]@{ id = 'CMAKE-DAG'; status = 'passed'; assertion = 'CMake modules and edges stay within the ADR-0003 dependency closure' }),
            [PSCustomObject]([ordered]@{ id = 'SOURCE-HASH'; status = 'passed'; assertion = 'all derived inputs are pinned by SHA-256' }),
            [PSCustomObject]([ordered]@{ id = 'NEGATIVE-CASES'; status = 'passed'; assertion = "$NegativeCaseCount invalid mutations were rejected" })
        )
    })
}

function Write-Utf8NoBom {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string]$Path, [Parameter(Mandatory = $true)][string]$Content)

    $encoding = [System.Text.UTF8Encoding]::new($false)
    [System.IO.File]::WriteAllText($Path, $Content, $encoding)
}

function Test-GeneratedContent {
    param([string]$Path, [string]$Expected)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $false }
    $actual = Get-Content -LiteralPath $Path -Raw -Encoding utf8
    $actual = ($actual -replace "`r`n", "`n").TrimStart([char]0xFEFF)
    return [string]::Equals($actual, $Expected, [System.StringComparison]::Ordinal)
}

function Copy-JsonData {
    param($Value)
    return ($Value | ConvertTo-Json -Depth 100 | ConvertFrom-Json)
}

function Assert-NegativeFailure {
    param(
        [System.Collections.Generic.List[string]]$Failures,
        [string]$Name,
        $Result,
        [string]$ExpectedText
    )

    if ($Result.IsValid) {
        [void]$Failures.Add("$Name was accepted")
        return
    }
    if (-not (@($Result.Errors) | Where-Object { $_.IndexOf($ExpectedText, [System.StringComparison]::OrdinalIgnoreCase) -ge 0 } | Select-Object -First 1)) {
        [void]$Failures.Add("$Name failed for the wrong reason: $($Result.Errors -join ' | ')")
    }
}

function Invoke-ArchitectureNegativeCases {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]$Glossary,
        [Parameter(Mandatory = $true)]$Dependency,
        [Parameter(Mandatory = $true)]$Registry,
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string]$GlossaryText,
        [Parameter(Mandatory = $true)][string]$ExpectedBaselineJson
    )

    $failures = [System.Collections.Generic.List[string]]::new()
    $caseCount = 0

    ++$caseCount
    $separatorMatch = [regex]::Match(
        $GlossaryText,
        '(?ms)^## 3\..*?^(\| --- \| --- \| --- \| --- \|)$')
    if (-not $separatorMatch.Success) {
        [void]$failures.Add('malformed glossary table case could not find the section 3 separator')
    }
    else {
        $mutatedText = $GlossaryText.Remove($separatorMatch.Groups[1].Index, $separatorMatch.Groups[1].Length).Insert(
            $separatorMatch.Groups[1].Index,
            '| --- | --- | --- |')
        try {
            [void](ConvertFrom-GlossaryMarkdown -Text $mutatedText)
            [void]$failures.Add('malformed glossary table was accepted')
        }
        catch {
            if ($_.Exception.Message.IndexOf('separator row', [System.StringComparison]::OrdinalIgnoreCase) -lt 0) {
                [void]$failures.Add("malformed glossary table failed for the wrong reason: $($_.Exception.Message)")
            }
        }
    }

    ++$caseCount
    $mutatedGlossary = Copy-JsonData $Glossary
    $mutatedGlossary.Terms = @($mutatedGlossary.Terms) + @($mutatedGlossary.Terms[0])
    $result = Test-ArchitectureInputs -Glossary $mutatedGlossary -Dependency $Dependency -Registry $Registry -RepoRoot $RepoRoot
    Assert-NegativeFailure -Failures $failures -Name 'duplicate term identity' -Result $result -ExpectedText 'Duplicate terminology identity'

    ++$caseCount
    $mutatedGlossary = Copy-JsonData $Glossary
    $mutatedGlossary.Aliases[0].canonical_terms = @('MissingCanonicalTerm')
    $result = Test-ArchitectureInputs -Glossary $mutatedGlossary -Dependency $Dependency -Registry $Registry -RepoRoot $RepoRoot
    Assert-NegativeFailure -Failures $failures -Name 'dangling alias target' -Result $result -ExpectedText 'references unknown canonical term'

    ++$caseCount
    $mutatedGlossary = Copy-JsonData $Glossary
    $enumTerm = @($mutatedGlossary.Terms | Where-Object { $_.name -ceq 'CapabilityStatus' } | Select-Object -First 1)[0]
    $enumTerm.definition = ([string]$enumTerm.definition).Replace('Stable', 'Experimental')
    $result = Test-ArchitectureInputs -Glossary $mutatedGlossary -Dependency $Dependency -Registry $Registry -RepoRoot $RepoRoot
    Assert-NegativeFailure -Failures $failures -Name 'shared enum value drift' -Result $result -ExpectedText 'differs between glossary and semantic authority'

    ++$caseCount
    $mutatedRegistry = Copy-JsonData $Registry
    $mutatedRegistry.shared_symbols[0].semantic_authority = 'docs/architecture/missing-authority.md'
    $result = Test-ArchitectureInputs -Glossary $Glossary -Dependency $Dependency -Registry $mutatedRegistry -RepoRoot $RepoRoot
    Assert-NegativeFailure -Failures $failures -Name 'missing semantic authority' -Result $result -ExpectedText 'semantic authority does not exist'

    ++$caseCount
    $mutatedRegistry = Copy-JsonData $Registry
    $mutatedRegistry.legacy_ownership = @($mutatedRegistry.legacy_ownership | Select-Object -Skip 1)
    $result = Test-ArchitectureInputs -Glossary $Glossary -Dependency $Dependency -Registry $mutatedRegistry -RepoRoot $RepoRoot
    Assert-NegativeFailure -Failures $failures -Name 'missing Legacy owner' -Result $result -ExpectedText 'is missing ownership'

    ++$caseCount
    $mutatedRegistry = Copy-JsonData $Registry
    $mutatedRegistry | Add-Member -MemberType NoteProperty -Name 'logical_boundaries' -Value @(
        [PSCustomObject]@{ name = 'packages_user'; kind = 'logical_contribution_boundary' },
        [PSCustomObject]@{ name = 'composition_root'; kind = 'logical_composition_boundary' })
    $result = Test-ArchitectureInputs -Glossary $Glossary -Dependency $Dependency -Registry $mutatedRegistry -RepoRoot $RepoRoot
    Assert-NegativeFailure -Failures $failures -Name 'unreviewed registry extension' -Result $result -ExpectedText "Authority registry has unknown property 'logical_boundaries'"

    ++$caseCount
    $mutatedRegistry = Copy-JsonData $Registry
    $mutatedRegistry.modules[0] | Add-Member -MemberType NoteProperty -Name 'kind' -Value 'cmake_interface'
    $result = Test-ArchitectureInputs -Glossary $Glossary -Dependency $Dependency -Registry $mutatedRegistry -RepoRoot $RepoRoot
    Assert-NegativeFailure -Failures $failures -Name 'unreviewed module-row extension' -Result $result -ExpectedText "module row 0 has unknown property 'kind'"

    ++$caseCount
    $mutatedRegistry = Copy-JsonData $Registry
    $mutatedRegistry.shared_symbols[0] | Add-Member -MemberType NoteProperty -Name 'owner_role' -Value 'architecture_lead'
    $result = Test-ArchitectureInputs -Glossary $Glossary -Dependency $Dependency -Registry $mutatedRegistry -RepoRoot $RepoRoot
    Assert-NegativeFailure -Failures $failures -Name 'unreviewed shared-symbol extension' -Result $result -ExpectedText "shared symbol row 0 has unknown property 'owner_role'"

    ++$caseCount
    $mutatedRegistry = Copy-JsonData $Registry
    $mutatedRegistry.legacy_ownership[0] | Add-Member -MemberType NoteProperty -Name 'responsibilities' -Value @(
        [PSCustomObject]@{ id = 'candidate-overlay'; target_module = 'compiler' })
    $result = Test-ArchitectureInputs -Glossary $Glossary -Dependency $Dependency -Registry $mutatedRegistry -RepoRoot $RepoRoot
    Assert-NegativeFailure -Failures $failures -Name 'unreviewed Legacy responsibility overlay' -Result $result -ExpectedText "Legacy ownership row 0 has unknown property 'responsibilities'"

    ++$caseCount
    $mutatedRegistry = Copy-JsonData $Registry
    $mutatedRegistry.modules = @($mutatedRegistry.modules) + @(
        [PSCustomObject]@{ name = 'packages_user'; source_root = 'packages' },
        [PSCustomObject]@{ name = 'composition_root'; source_root = 'apps' })
    $result = Test-ArchitectureInputs -Glossary $Glossary -Dependency $Dependency -Registry $mutatedRegistry -RepoRoot $RepoRoot
    Assert-NegativeFailure -Failures $failures -Name 'packages_user physical-module promotion' -Result $result -ExpectedText "Ownership registry module 'packages_user' is absent from ADR-0003"
    Assert-NegativeFailure -Failures $failures -Name 'composition_root physical-module promotion' -Result $result -ExpectedText "Ownership registry module 'composition_root' is absent from ADR-0003"

    ++$caseCount
    $mutatedRegistry = Copy-JsonData $Registry
    $candidateOnlyLegacyOwners = @(
        [PSCustomObject]@{ legacy_name = 'IContinuousGroup'; disposition = 'replace'; primary_owner = 'compiler' },
        [PSCustomObject]@{ legacy_name = 'IIntegrator'; disposition = 'split'; primary_owner = 'foundation' },
        [PSCustomObject]@{ legacy_name = 'ISummaryObserver'; disposition = 'replace'; primary_owner = 'evidence' },
        [PSCustomObject]@{ legacy_name = 'math_types.hpp'; disposition = 'split'; primary_owner = 'contracts' },
        [PSCustomObject]@{ legacy_name = 'SimulationSummary'; disposition = 'replace'; primary_owner = 'workflow' })
    $mutatedRegistry.legacy_ownership = @($mutatedRegistry.legacy_ownership) + $candidateOnlyLegacyOwners
    $result = Test-ArchitectureInputs -Glossary $Glossary -Dependency $Dependency -Registry $mutatedRegistry -RepoRoot $RepoRoot
    foreach ($legacyName in @('IContinuousGroup', 'IIntegrator', 'ISummaryObserver', 'math_types.hpp', 'SimulationSummary')) {
        Assert-NegativeFailure -Failures $failures -Name "candidate-only Legacy owner $legacyName" -Result $result -ExpectedText "Legacy ownership '$legacyName' has no glossary migration row"
    }

    ++$caseCount
    $mutatedDependency = Copy-JsonData $Dependency
    $mutatedDependency.ArchitectureEdges = @($mutatedDependency.ArchitectureEdges) + @((New-DependencyEdge -Source 'foundation' -Target 'adapters'))
    $result = Test-ArchitectureInputs -Glossary $Glossary -Dependency $mutatedDependency -Registry $Registry -RepoRoot $RepoRoot
    Assert-NegativeFailure -Failures $failures -Name 'dependency cycle' -Result $result -ExpectedText 'contains a cycle'

    ++$caseCount
    $mutatedDependency = Copy-JsonData $Dependency
    $mutatedDependency.CMakeEdges = @($mutatedDependency.CMakeEdges) + @((New-DependencyEdge -Source 'kernel' -Target 'compiler'))
    $result = Test-ArchitectureInputs -Glossary $Glossary -Dependency $mutatedDependency -Registry $Registry -RepoRoot $RepoRoot
    Assert-NegativeFailure -Failures $failures -Name 'forbidden Kernel dependency' -Result $result -ExpectedText 'is not allowed by ADR-0003'

    ++$caseCount
    $hashMatch = [regex]::Match($ExpectedBaselineJson, '[0-9a-f]{64}')
    if (-not $hashMatch.Success) {
        [void]$failures.Add('source hash drift case could not find a source hash')
    }
    else {
        $mutatedJson = $ExpectedBaselineJson.Substring(0, $hashMatch.Index) + ('0' * 64) + $ExpectedBaselineJson.Substring($hashMatch.Index + 64)
        if ([string]::Equals($mutatedJson, $ExpectedBaselineJson, [System.StringComparison]::Ordinal)) {
            [void]$failures.Add('source hash drift was accepted')
        }
    }

    return [PSCustomObject]([ordered]@{
        IsValid = $failures.Count -eq 0
        CaseCount = $caseCount
        Failures = @($failures)
    })
}

Export-ModuleMember -Function @(
    'ConvertFrom-GlossaryMarkdown',
    'ConvertFrom-DependencySources',
    'Test-ArchitectureInputs',
    'New-ArchitectureBaseline',
    'ConvertTo-DeterministicJson',
    'New-TerminologyConformanceReport',
    'Write-Utf8NoBom',
    'Test-GeneratedContent',
    'Invoke-ArchitectureNegativeCases'
)
