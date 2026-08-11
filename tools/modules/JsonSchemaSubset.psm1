Set-StrictMode -Version Latest

function Throw-StrictJsonError {
    param(
        [int]$Offset,
        [string]$Message
    )

    throw [System.FormatException]::new("strict JSON error at offset ${Offset}: $Message")
}

function Move-PastJsonWhitespace {
    param(
        [string]$Text,
        [ref]$Index
    )

    while ($Index.Value -lt $Text.Length) {
        $code = [int][char]$Text[$Index.Value]
        if ($code -notin @(0x20, 0x09, 0x0A, 0x0D)) { return }
        $Index.Value = [int]$Index.Value + 1
    }
}

function Test-JsonDigit {
    param([char]$Character)

    $code = [int]$Character
    return $code -ge [int][char]'0' -and $code -le [int][char]'9'
}

function Test-JsonTokenAt {
    param(
        [string]$Text,
        [int]$Offset,
        [string]$Token
    )

    if ($Offset + $Token.Length -gt $Text.Length) { return $false }
    return [string]::CompareOrdinal($Text, $Offset, $Token, 0, $Token.Length) -eq 0
}

function Read-StrictJsonString {
    param(
        [string]$Text,
        [ref]$Index
    )

    if ($Index.Value -ge $Text.Length -or $Text[$Index.Value] -ne '"') {
        Throw-StrictJsonError -Offset $Index.Value -Message 'expected a JSON string'
    }

    $Index.Value = [int]$Index.Value + 1
    $builder = [System.Text.StringBuilder]::new()
    while ($Index.Value -lt $Text.Length) {
        $character = [char]$Text[$Index.Value]
        $Index.Value = [int]$Index.Value + 1

        if ($character -eq '"') {
            return $builder.ToString()
        }
        if ([int]$character -lt 0x20) {
            Throw-StrictJsonError -Offset ([int]$Index.Value - 1) -Message 'unescaped control character in string'
        }
        if ($character -ne '\') {
            [void]$builder.Append($character)
            continue
        }

        if ($Index.Value -ge $Text.Length) {
            Throw-StrictJsonError -Offset $Index.Value -Message 'unterminated string escape'
        }
        $escape = [char]$Text[$Index.Value]
        $Index.Value = [int]$Index.Value + 1
        switch -CaseSensitive ($escape) {
            '"' { [void]$builder.Append('"') }
            '\' { [void]$builder.Append([char]0x5C) }
            '/' { [void]$builder.Append('/') }
            'b' { [void]$builder.Append([char]0x08) }
            'f' { [void]$builder.Append([char]0x0C) }
            'n' { [void]$builder.Append([char]0x0A) }
            'r' { [void]$builder.Append([char]0x0D) }
            't' { [void]$builder.Append([char]0x09) }
            'u' {
                if ($Index.Value + 4 -gt $Text.Length) {
                    Throw-StrictJsonError -Offset $Index.Value -Message 'incomplete Unicode escape'
                }
                $hex = $Text.Substring($Index.Value, 4)
                if ($hex -cnotmatch '^[0-9A-Fa-f]{4}$') {
                    Throw-StrictJsonError -Offset $Index.Value -Message "invalid Unicode escape '\u$hex'"
                }
                [void]$builder.Append([char][Convert]::ToInt32($hex, 16))
                $Index.Value = [int]$Index.Value + 4
            }
            default {
                Throw-StrictJsonError -Offset ([int]$Index.Value - 1) -Message "invalid string escape '\$escape'"
            }
        }
    }

    Throw-StrictJsonError -Offset $Index.Value -Message 'unterminated JSON string'
}

function Read-StrictJsonNumber {
    param(
        [string]$Text,
        [ref]$Index
    )

    $start = [int]$Index.Value
    if ($Text[$Index.Value] -eq '-') {
        $Index.Value = [int]$Index.Value + 1
        if ($Index.Value -ge $Text.Length) {
            Throw-StrictJsonError -Offset $start -Message 'incomplete JSON number'
        }
    }

    if ($Text[$Index.Value] -eq '0') {
        $Index.Value = [int]$Index.Value + 1
        if ($Index.Value -lt $Text.Length -and (Test-JsonDigit $Text[$Index.Value])) {
            Throw-StrictJsonError -Offset $Index.Value -Message 'leading zero in JSON number'
        }
    }
    elseif ([int][char]$Text[$Index.Value] -ge [int][char]'1' -and
            [int][char]$Text[$Index.Value] -le [int][char]'9') {
        do {
            $Index.Value = [int]$Index.Value + 1
        } while ($Index.Value -lt $Text.Length -and (Test-JsonDigit $Text[$Index.Value]))
    }
    else {
        Throw-StrictJsonError -Offset $Index.Value -Message 'invalid JSON number'
    }

    if ($Index.Value -lt $Text.Length -and $Text[$Index.Value] -eq '.') {
        $Index.Value = [int]$Index.Value + 1
        if ($Index.Value -ge $Text.Length -or -not (Test-JsonDigit $Text[$Index.Value])) {
            Throw-StrictJsonError -Offset $Index.Value -Message 'fraction requires at least one digit'
        }
        while ($Index.Value -lt $Text.Length -and (Test-JsonDigit $Text[$Index.Value])) {
            $Index.Value = [int]$Index.Value + 1
        }
    }

    if ($Index.Value -lt $Text.Length -and $Text[$Index.Value] -in @('e', 'E')) {
        $Index.Value = [int]$Index.Value + 1
        if ($Index.Value -lt $Text.Length -and $Text[$Index.Value] -in @('+', '-')) {
            $Index.Value = [int]$Index.Value + 1
        }
        if ($Index.Value -ge $Text.Length -or -not (Test-JsonDigit $Text[$Index.Value])) {
            Throw-StrictJsonError -Offset $Index.Value -Message 'exponent requires at least one digit'
        }
        while ($Index.Value -lt $Text.Length -and (Test-JsonDigit $Text[$Index.Value])) {
            $Index.Value = [int]$Index.Value + 1
        }
    }
}

function Read-StrictJsonLiteral {
    param(
        [string]$Text,
        [ref]$Index,
        [string]$Literal
    )

    if (-not (Test-JsonTokenAt -Text $Text -Offset $Index.Value -Token $Literal)) {
        Throw-StrictJsonError -Offset $Index.Value -Message "invalid JSON token; expected '$Literal'"
    }
    $Index.Value = [int]$Index.Value + $Literal.Length
}

function Read-StrictJsonObject {
    param(
        [string]$Text,
        [ref]$Index,
        [int]$Depth
    )

    $Index.Value = [int]$Index.Value + 1
    Move-PastJsonWhitespace -Text $Text -Index $Index
    if ($Index.Value -lt $Text.Length -and $Text[$Index.Value] -eq '}') {
        $Index.Value = [int]$Index.Value + 1
        return
    }

    $keys = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    while ($true) {
        Move-PastJsonWhitespace -Text $Text -Index $Index
        $keyOffset = [int]$Index.Value
        $key = [string](Read-StrictJsonString -Text $Text -Index $Index)
        if (-not $keys.Add($key)) {
            Throw-StrictJsonError -Offset $keyOffset -Message "duplicate JSON object key '$key'"
        }

        Move-PastJsonWhitespace -Text $Text -Index $Index
        if ($Index.Value -ge $Text.Length -or $Text[$Index.Value] -ne ':') {
            Throw-StrictJsonError -Offset $Index.Value -Message "expected ':' after object key"
        }
        $Index.Value = [int]$Index.Value + 1
        Read-StrictJsonValue -Text $Text -Index $Index -Depth ([int]$Depth + 1)
        Move-PastJsonWhitespace -Text $Text -Index $Index

        if ($Index.Value -ge $Text.Length) {
            Throw-StrictJsonError -Offset $Index.Value -Message 'unterminated JSON object'
        }
        if ($Text[$Index.Value] -eq '}') {
            $Index.Value = [int]$Index.Value + 1
            return
        }
        if ($Text[$Index.Value] -ne ',') {
            Throw-StrictJsonError -Offset $Index.Value -Message "expected ',' or '}' in object"
        }
        $Index.Value = [int]$Index.Value + 1
    }
}

function Read-StrictJsonArray {
    param(
        [string]$Text,
        [ref]$Index,
        [int]$Depth
    )

    $Index.Value = [int]$Index.Value + 1
    Move-PastJsonWhitespace -Text $Text -Index $Index
    if ($Index.Value -lt $Text.Length -and $Text[$Index.Value] -eq ']') {
        $Index.Value = [int]$Index.Value + 1
        return
    }

    while ($true) {
        Read-StrictJsonValue -Text $Text -Index $Index -Depth ([int]$Depth + 1)
        Move-PastJsonWhitespace -Text $Text -Index $Index
        if ($Index.Value -ge $Text.Length) {
            Throw-StrictJsonError -Offset $Index.Value -Message 'unterminated JSON array'
        }
        if ($Text[$Index.Value] -eq ']') {
            $Index.Value = [int]$Index.Value + 1
            return
        }
        if ($Text[$Index.Value] -ne ',') {
            Throw-StrictJsonError -Offset $Index.Value -Message "expected ',' or ']' in array"
        }
        $Index.Value = [int]$Index.Value + 1
    }
}

function Read-StrictJsonValue {
    param(
        [string]$Text,
        [ref]$Index,
        [int]$Depth
    )

    if ($Depth -gt 128) {
        Throw-StrictJsonError -Offset $Index.Value -Message 'nesting exceeds the R0 limit of 128'
    }
    Move-PastJsonWhitespace -Text $Text -Index $Index
    if ($Index.Value -ge $Text.Length) {
        Throw-StrictJsonError -Offset $Index.Value -Message 'expected a JSON value'
    }

    $character = [char]$Text[$Index.Value]
    switch -CaseSensitive ($character) {
        '{' { Read-StrictJsonObject -Text $Text -Index $Index -Depth $Depth; return }
        '[' { Read-StrictJsonArray -Text $Text -Index $Index -Depth $Depth; return }
        '"' { [void](Read-StrictJsonString -Text $Text -Index $Index); return }
        't' { Read-StrictJsonLiteral -Text $Text -Index $Index -Literal 'true'; return }
        'f' { Read-StrictJsonLiteral -Text $Text -Index $Index -Literal 'false'; return }
        'n' { Read-StrictJsonLiteral -Text $Text -Index $Index -Literal 'null'; return }
        'N' {
            if (Test-JsonTokenAt -Text $Text -Offset $Index.Value -Token 'NaN') {
                Throw-StrictJsonError -Offset $Index.Value -Message "non-finite JSON token 'NaN' is forbidden"
            }
        }
        'I' {
            if (Test-JsonTokenAt -Text $Text -Offset $Index.Value -Token 'Infinity') {
                Throw-StrictJsonError -Offset $Index.Value -Message "non-finite JSON token 'Infinity' is forbidden"
            }
        }
        '-' {
            if (Test-JsonTokenAt -Text $Text -Offset $Index.Value -Token '-Infinity') {
                Throw-StrictJsonError -Offset $Index.Value -Message "non-finite JSON token '-Infinity' is forbidden"
            }
            Read-StrictJsonNumber -Text $Text -Index $Index
            return
        }
        default {
            if (Test-JsonDigit $character) {
                Read-StrictJsonNumber -Text $Text -Index $Index
                return
            }
        }
    }

    Throw-StrictJsonError -Offset $Index.Value -Message "unexpected token '$character'"
}

function Assert-StrictJsonText {
    param([string]$Text)

    $index = 0
    if ($Text.Length -gt 0 -and [int][char]$Text[0] -eq 0xFEFF) {
        $index = 1
    }
    Read-StrictJsonValue -Text $Text -Index ([ref]$index) -Depth 0
    Move-PastJsonWhitespace -Text $Text -Index ([ref]$index)
    if ($index -ne $Text.Length) {
        Throw-StrictJsonError -Offset $index -Message 'unexpected content after the root value'
    }
}

function Read-StrictJsonFile {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string]$Path)

    $resolved = (Resolve-Path -LiteralPath $Path -ErrorAction Stop).Path
    $text = Get-Content -LiteralPath $resolved -Raw -Encoding utf8 -ErrorAction Stop
    Assert-StrictJsonText -Text $text
    $parseText = $text.TrimStart([char]0xFEFF)
    # Parse through an object property because PowerShell 7 enumerates JSON array
    # roots by default while Windows PowerShell 5.1 does not expose -NoEnumerate.
    $envelope = ('{"value":' + $parseText + '}') | ConvertFrom-Json -ErrorAction Stop
    $value = $envelope.value
    # The unary comma preserves array roots without the PowerShell 7.6
    # List<object> wrapper produced by Write-Output -NoEnumerate for PSCustomObject.
    return ,$value
}

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
        $schema = Read-StrictJsonFile -Path $resolvedSchema
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
        $instance = Read-StrictJsonFile -Path $resolvedInstance
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

Export-ModuleMember -Function Test-JsonSchemaSubset, Read-StrictJsonFile
