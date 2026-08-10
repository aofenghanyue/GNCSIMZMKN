Set-StrictMode -Version Latest

function Add-ValidationError {
    param(
        [System.Collections.Generic.List[string]]$Errors,
        [string]$Path,
        [string]$Message
    )

    [void]$Errors.Add("${Path}: $Message")
}

function Get-JsonType {
    param($Value)

    if ($null -eq $Value) { return 'null' }
    if ($Value -is [bool]) { return 'boolean' }
    if ($Value -is [string]) { return 'string' }
    if ($Value -is [byte] -or
        $Value -is [sbyte] -or
        $Value -is [int16] -or
        $Value -is [uint16] -or
        $Value -is [int32] -or
        $Value -is [uint32] -or
        $Value -is [int64] -or
        $Value -is [uint64]) {
        return 'integer'
    }
    if ($Value -is [single] -or $Value -is [double] -or $Value -is [decimal]) {
        return 'number'
    }
    if ($Value -is [System.Array]) { return 'array' }
    if ($Value -is [System.Collections.IDictionary] -or
        $Value -is [System.Management.Automation.PSCustomObject]) {
        return 'object'
    }

    return 'unknown'
}

function Get-JsonProperty {
    param(
        $Object,
        [string]$Name
    )

    if ($null -ne $Object) {
        foreach ($property in $Object.PSObject.Properties) {
            if ([string]::Equals($property.Name, $Name, [System.StringComparison]::Ordinal)) {
                return [PSCustomObject]@{ Exists = $true; Value = $property.Value }
            }
        }
    }

    return [PSCustomObject]@{ Exists = $false; Value = $null }
}

function Get-JsonObjectProperties {
    param($Object)

    if ($null -eq $Object) { return @() }
    return @($Object.PSObject.Properties | Where-Object {
        $_.MemberType -in @('NoteProperty', 'Property')
    })
}

function ConvertTo-ComparableJson {
    param($Value)

    return ($Value | ConvertTo-Json -Depth 100 -Compress)
}

function Test-ExpectedJsonType {
    param(
        [string]$Expected,
        [string]$Actual
    )

    if ($Expected -eq $Actual) { return $true }
    return $Expected -eq 'number' -and $Actual -eq 'integer'
}

function Test-SchemaMatch {
    param(
        $Schema,
        $Instance,
        [string]$Path,
        $RootSchema
    )

    $probeErrors = [System.Collections.Generic.List[string]]::new()
    Test-JsonSchemaNode -Schema $Schema -Instance $Instance -Path $Path -Errors $probeErrors -RootSchema $RootSchema
    return $probeErrors.Count -eq 0
}

function Test-JsonSchemaNode {
    param(
        $Schema,
        $Instance,
        [string]$Path,
        [System.Collections.Generic.List[string]]$Errors,
        $RootSchema
    )

    $actualType = Get-JsonType $Instance
    $typeRule = Get-JsonProperty $Schema 'type'
    if ($typeRule.Exists) {
        $typeMatched = $false
        foreach ($expectedType in @($typeRule.Value)) {
            if (Test-ExpectedJsonType -Expected ([string]$expectedType) -Actual $actualType) {
                $typeMatched = $true
                break
            }
        }
        if (-not $typeMatched) {
            Add-ValidationError $Errors $Path "expected type '$($typeRule.Value)' but found '$actualType'"
            return
        }
    }

    $constRule = Get-JsonProperty $Schema 'const'
    if ($constRule.Exists -and
        (ConvertTo-ComparableJson $Instance) -cne (ConvertTo-ComparableJson $constRule.Value)) {
        Add-ValidationError $Errors $Path "value does not match const '$($constRule.Value)'"
    }

    $enumRule = Get-JsonProperty $Schema 'enum'
    if ($enumRule.Exists) {
        $instanceJson = ConvertTo-ComparableJson $Instance
        $enumMatched = $false
        foreach ($candidate in @($enumRule.Value)) {
            if ($instanceJson -ceq (ConvertTo-ComparableJson $candidate)) {
                $enumMatched = $true
                break
            }
        }
        if (-not $enumMatched) {
            Add-ValidationError $Errors $Path 'value is not in the declared enum'
        }
    }

    if ($actualType -eq 'object') {
        $instanceProperties = @(Get-JsonObjectProperties $Instance)
        $minPropertiesRule = Get-JsonProperty $Schema 'minProperties'
        if ($minPropertiesRule.Exists -and $instanceProperties.Count -lt [int]$minPropertiesRule.Value) {
            Add-ValidationError $Errors $Path "expected at least $($minPropertiesRule.Value) properties"
        }

        $requiredRule = Get-JsonProperty $Schema 'required'
        if ($requiredRule.Exists) {
            foreach ($requiredName in @($requiredRule.Value)) {
                $requiredProperty = Get-JsonProperty $Instance ([string]$requiredName)
                if (-not $requiredProperty.Exists) {
                    Add-ValidationError $Errors $Path "missing required property '$requiredName'"
                }
            }
        }

        $propertiesRule = Get-JsonProperty $Schema 'properties'
        $knownProperties = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
        if ($propertiesRule.Exists) {
            foreach ($schemaProperty in @(Get-JsonObjectProperties $propertiesRule.Value)) {
                [void]$knownProperties.Add($schemaProperty.Name)
                $instanceProperty = Get-JsonProperty $Instance $schemaProperty.Name
                if ($instanceProperty.Exists) {
                    Test-JsonSchemaNode -Schema $schemaProperty.Value -Instance $instanceProperty.Value -Path "$Path.$($schemaProperty.Name)" -Errors $Errors -RootSchema $RootSchema
                }
            }
        }

        $additionalRule = Get-JsonProperty $Schema 'additionalProperties'
        if ($additionalRule.Exists) {
            foreach ($instanceProperty in $instanceProperties) {
                if ($knownProperties.Contains($instanceProperty.Name)) { continue }
                $additionalType = Get-JsonType $additionalRule.Value
                if ($additionalType -eq 'boolean' -and -not [bool]$additionalRule.Value) {
                    Add-ValidationError $Errors "$Path.$($instanceProperty.Name)" 'additional property is not allowed'
                }
                elseif ($additionalType -eq 'object') {
                    Test-JsonSchemaNode -Schema $additionalRule.Value -Instance $instanceProperty.Value -Path "$Path.$($instanceProperty.Name)" -Errors $Errors -RootSchema $RootSchema
                }
            }
        }
    }

    if ($actualType -eq 'array') {
        $items = @($Instance)
        $minItemsRule = Get-JsonProperty $Schema 'minItems'
        if ($minItemsRule.Exists -and $items.Count -lt [int]$minItemsRule.Value) {
            Add-ValidationError $Errors $Path "expected at least $($minItemsRule.Value) items"
        }
        $maxItemsRule = Get-JsonProperty $Schema 'maxItems'
        if ($maxItemsRule.Exists -and $items.Count -gt [int]$maxItemsRule.Value) {
            Add-ValidationError $Errors $Path "expected at most $($maxItemsRule.Value) items"
        }

        $uniqueItemsRule = Get-JsonProperty $Schema 'uniqueItems'
        if ($uniqueItemsRule.Exists -and [bool]$uniqueItemsRule.Value) {
            $seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
            for ($index = 0; $index -lt $items.Count; ++$index) {
                $itemJson = ConvertTo-ComparableJson $items[$index]
                if (-not $seen.Add($itemJson)) {
                    Add-ValidationError $Errors "$Path[$index]" 'array item is not unique'
                }
            }
        }

        $itemsRule = Get-JsonProperty $Schema 'items'
        if ($itemsRule.Exists) {
            for ($index = 0; $index -lt $items.Count; ++$index) {
                Test-JsonSchemaNode -Schema $itemsRule.Value -Instance $items[$index] -Path "$Path[$index]" -Errors $Errors -RootSchema $RootSchema
            }
        }
    }

    if ($actualType -eq 'string') {
        $minLengthRule = Get-JsonProperty $Schema 'minLength'
        if ($minLengthRule.Exists -and $Instance.Length -lt [int]$minLengthRule.Value) {
            Add-ValidationError $Errors $Path "expected a string of at least $($minLengthRule.Value) characters"
        }
        $patternRule = Get-JsonProperty $Schema 'pattern'
        if ($patternRule.Exists -and $Instance -cnotmatch [string]$patternRule.Value) {
            Add-ValidationError $Errors $Path "value does not match pattern '$($patternRule.Value)'"
        }
    }

    $allOfRule = Get-JsonProperty $Schema 'allOf'
    if ($allOfRule.Exists) {
        foreach ($subschema in @($allOfRule.Value)) {
            Test-JsonSchemaNode -Schema $subschema -Instance $Instance -Path $Path -Errors $Errors -RootSchema $RootSchema
        }
    }

    $ifRule = Get-JsonProperty $Schema 'if'
    if ($ifRule.Exists) {
        $matchesCondition = Test-SchemaMatch -Schema $ifRule.Value -Instance $Instance -Path $Path -RootSchema $RootSchema
        $branchName = if ($matchesCondition) { 'then' } else { 'else' }
        $branchRule = Get-JsonProperty $Schema $branchName
        if ($branchRule.Exists) {
            Test-JsonSchemaNode -Schema $branchRule.Value -Instance $Instance -Path $Path -Errors $Errors -RootSchema $RootSchema
        }
    }
}

function Test-SchemaKeywords {
    param(
        $Schema,
        [string]$Path,
        [System.Collections.Generic.List[string]]$Errors
    )

    $allowedKeywords = @(
        '$schema', '$id', 'title', 'description', 'type', 'required',
        'properties', 'additionalProperties', 'items', 'minItems', 'maxItems',
        'uniqueItems', 'minLength', 'pattern', 'enum', 'const', 'minProperties',
        'allOf', 'if', 'then', 'else'
    )

    foreach ($property in @(Get-JsonObjectProperties $Schema)) {
        if ($property.Name -notin $allowedKeywords -and -not $property.Name.StartsWith('x-', [System.StringComparison]::Ordinal)) {
            Add-ValidationError $Errors "$Path.$($property.Name)" 'schema keyword is not supported by the R0 validator'
            continue
        }

        switch -CaseSensitive ($property.Name) {
            'properties' {
                foreach ($child in @(Get-JsonObjectProperties $property.Value)) {
                    Test-SchemaKeywords -Schema $child.Value -Path "$Path.properties.$($child.Name)" -Errors $Errors
                }
            }
            'items' {
                Test-SchemaKeywords -Schema $property.Value -Path "$Path.items" -Errors $Errors
            }
            'additionalProperties' {
                if ((Get-JsonType $property.Value) -eq 'object') {
                    Test-SchemaKeywords -Schema $property.Value -Path "$Path.additionalProperties" -Errors $Errors
                }
            }
            'allOf' {
                $children = @($property.Value)
                for ($index = 0; $index -lt $children.Count; ++$index) {
                    Test-SchemaKeywords -Schema $children[$index] -Path "$Path.allOf[$index]" -Errors $Errors
                }
            }
            'if' { Test-SchemaKeywords -Schema $property.Value -Path "$Path.if" -Errors $Errors }
            'then' { Test-SchemaKeywords -Schema $property.Value -Path "$Path.then" -Errors $Errors }
            'else' { Test-SchemaKeywords -Schema $property.Value -Path "$Path.else" -Errors $Errors }
        }
    }
}

function Test-JsonSchemaSubset {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][string]$SchemaPath,
        [Parameter(Mandatory = $true)][string]$InstancePath
    )

    $errors = [System.Collections.Generic.List[string]]::new()
    $schema = $null
    $instance = $null
    $schemaLoaded = $false
    $instanceLoaded = $false

    try {
        $resolvedSchema = (Resolve-Path -LiteralPath $SchemaPath -ErrorAction Stop).Path
        $schema = Get-Content -LiteralPath $resolvedSchema -Raw -Encoding utf8 -ErrorAction Stop | ConvertFrom-Json
        if ((Get-JsonType $schema) -ne 'object') {
            Add-ValidationError $errors '$schema' 'schema root must be a JSON object'
        }
        else {
            $schemaLoaded = $true
        }
    }
    catch {
        Add-ValidationError $errors '$schema' "cannot read or parse '$SchemaPath': $($_.Exception.Message)"
    }

    try {
        $resolvedInstance = (Resolve-Path -LiteralPath $InstancePath -ErrorAction Stop).Path
        $instance = Get-Content -LiteralPath $resolvedInstance -Raw -Encoding utf8 -ErrorAction Stop | ConvertFrom-Json
        $instanceLoaded = $true
    }
    catch {
        Add-ValidationError $errors '$' "cannot read or parse '$InstancePath': $($_.Exception.Message)"
    }

    if ($schemaLoaded) {
        Test-SchemaKeywords -Schema $schema -Path '$schema' -Errors $errors
    }
    if ($schemaLoaded -and $instanceLoaded -and $errors.Count -eq 0) {
        Test-JsonSchemaNode -Schema $schema -Instance $instance -Path '$' -Errors $errors -RootSchema $schema
    }

    return [PSCustomObject]@{
        IsValid = $errors.Count -eq 0
        Errors = @($errors)
    }
}

Export-ModuleMember -Function Test-JsonSchemaSubset
