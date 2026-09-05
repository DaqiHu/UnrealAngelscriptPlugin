[CmdletBinding()]
param(
    [switch] $SnippetsOnly,
    [string] $WorkspaceRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-WorkspaceRoot {
    param([string] $StartPath)
    if ($WorkspaceRoot) { return (Resolve-Path -LiteralPath $WorkspaceRoot).Path }
    $candidate = [System.IO.DirectoryInfo]::new($StartPath)
    while ($null -ne $candidate) {
        if ((Test-Path -LiteralPath (Join-Path $candidate.FullName 'AngelscriptProject.uproject') -PathType Leaf) -and
            (Test-Path -LiteralPath (Join-Path $candidate.FullName '.git'))) {
            return $candidate.FullName
        }
        $candidate = $candidate.Parent
    }
    throw "Unable to locate the AngelscriptProject workspace above '$StartPath'."
}

function Get-CodeWithoutCommentsOrStrings {
    param([Parameter(Mandatory = $true)][string] $Text)

    $chars = $Text.ToCharArray()
    $out = New-Object System.Text.StringBuilder
    $i = 0
    $n = $chars.Length
    $inLine = $false
    $inBlock = $false
    $inString = $false
    $inChar = $false
    $inRaw = $false
    $stringDelim = [char]0
    while ($i -lt $n) {
        $c = $chars[$i]
        $next = if ($i + 1 -lt $n) { $chars[$i + 1] } else { [char]0 }
        if ($inLine) {
            if ($c -eq "`n") { [void]$out.Append("`n"); $inLine = $false }
            else { [void]$out.Append(' ') }
            $i++
            continue
        }
        if ($inBlock) {
            if ($c -eq '*' -and $next -eq '/') { [void]$out.Append('  '); $i += 2; $inBlock = $false; continue }
            if ($c -eq "`n") { [void]$out.Append("`n") } else { [void]$out.Append(' ') }
            $i++
            continue
        }
        if ($inRaw) {
            if ($c -eq ')' -and $next -eq '"') { [void]$out.Append('  '); $i += 2; $inRaw = $false; continue }
            if ($c -eq "`n") { [void]$out.Append("`n") } else { [void]$out.Append(' ') }
            $i++
            continue
        }
        if ($inString) {
            if ($c -eq '\' -and $next -ne [char]0) { [void]$out.Append('  '); $i += 2; continue }
            if ($c -eq $stringDelim) { [void]$out.Append(' '); $inString = $false }
            else { [void]$out.Append(' ') }
            $i++
            continue
        }
        if ($inChar) {
            if ($c -eq '\' -and $next -ne [char]0) { [void]$out.Append('  '); $i += 2; continue }
            if ($c -eq "'") { [void]$out.Append(' '); $inChar = $false }
            else { [void]$out.Append(' ') }
            $i++
            continue
        }
        if ($c -eq '/' -and $next -eq '/') { [void]$out.Append('  '); $i += 2; $inLine = $true; continue }
        if ($c -eq '/' -and $next -eq '*') { [void]$out.Append('  '); $i += 2; $inBlock = $true; continue }
        if ($c -eq 'R' -and $next -eq '"') { [void]$out.Append('  '); $i += 2; $inRaw = $true; continue }
        if ($c -eq '"' -or $c -eq [char]0x22) { [void]$out.Append(' '); $inString = $true; $stringDelim = $c; $i++; continue }
        if ($c -eq "'") { [void]$out.Append(' '); $inChar = $true; $i++; continue }
        [void]$out.Append($c)
        $i++
    }
    return $out.ToString()
}

function Get-CodeWithoutIncludes {
    param([Parameter(Mandatory = $true)][string] $Text)
    return [regex]::Replace($Text, '(?m)^\s*#\s*include\s*["<][^">]+[">].*$', { param($m) ' ' * $m.Value.Length })
}

function Find-RetiredFrontendTokens {
    param(
        [Parameter(Mandatory = $true)][string] $Text,
        [Parameter(Mandatory = $true)][string] $Origin
    )

    $code = Get-CodeWithoutIncludes (Get-CodeWithoutCommentsOrStrings $Text)
    $hits = [System.Collections.Generic.List[string]]::new()
    $patterns = [ordered]@{
        'namespace frontend' = '(?m)(?<![A-Za-z0-9_])namespace\s+frontend\b'
        'nested AngelScript::frontend' = 'AngelScript\s*::\s*frontend\b'
        'frontend-qualified use' = '(?<![A-Za-z0-9_/"''])frontend\s*::'
        'using namespace frontend' = '(?m)(?<![A-Za-z0-9_])using\s+namespace\s+frontend\b'
        'using frontend declaration' = '(?m)(?<![A-Za-z0-9_])using\s+frontend\s*::'
    }
    foreach ($name in $patterns.Keys) {
        foreach ($match in [regex]::Matches($code, $patterns[$name])) {
            $line = ($code.Substring(0, $match.Index) -split "`n").Count
            $hits.Add(('{0}:{1}: {2}' -f $Origin, $line, $name))
        }
    }
    return $hits
}

function Assert-Snippet {
    param(
        [Parameter(Mandatory = $true)][string] $Name,
        [Parameter(Mandatory = $true)][string] $Source,
        [Parameter(Mandatory = $true)][bool] $ShouldFail
    )
    $hits = @(Find-RetiredFrontendTokens -Text $Source -Origin $Name)
    $failed = $hits.Count -gt 0
    if ($ShouldFail -and -not $failed) {
        throw "Hermetic snippet '$Name' should reject retired frontend C++ syntax, but no tokens were reported."
    }
    if (-not $ShouldFail -and $failed) {
        throw "Hermetic snippet '$Name' should be accepted, but reported: $($hits -join '; ')"
    }
}

$root = Get-WorkspaceRoot -StartPath $PSScriptRoot
$failures = [System.Collections.Generic.List[string]]::new()

Assert-Snippet -Name 'reject-namespace-frontend' -ShouldFail $true -Source @'
BEGIN_AS_NAMESPACE
namespace frontend {
class asCParser {};
}
END_AS_NAMESPACE
'@
Assert-Snippet -Name 'reject-nested-angelscript-frontend' -ShouldFail $true -Source @'
namespace AngelScript { namespace frontend { class asCParser {}; } }
'@
Assert-Snippet -Name 'reject-frontend-qualified' -ShouldFail $true -Source @'
BEGIN_AS_NAMESPACE
frontend::asCParser Parser(Sema);
END_AS_NAMESPACE
'@
Assert-Snippet -Name 'reject-using-namespace' -ShouldFail $true -Source @'
using namespace frontend;
'@
Assert-Snippet -Name 'reject-using-declaration' -ShouldFail $true -Source @'
using frontend::asCParser;
'@
Assert-Snippet -Name 'accept-begin-as-namespace' -ShouldFail $false -Source @'
BEGIN_AS_NAMESPACE
class asCParser {};
class asCTokenizer {};
END_AS_NAMESPACE
'@
Assert-Snippet -Name 'accept-include-path' -ShouldFail $false -Source @'
#include "frontend/as_parser.h"
#include "frontend/as_tokenizer.h"
'@
Assert-Snippet -Name 'accept-comment-and-string' -ShouldFail $false -Source @'
// The frontend directory remains useful organization.
const char* Note = "frontend::is not C++ syntax here";
'@
Assert-Snippet -Name 'accept-anonymous-helper' -ShouldFail $false -Source @'
namespace {
int Helper() { return 1; }
}
'@
Assert-Snippet -Name 'protect-outer-macros' -ShouldFail $false -Source @'
#define BEGIN_AS_NAMESPACE namespace AngelScript {
#define END_AS_NAMESPACE }
#define AS_NAMESPACE_QUALIFIER AngelScript::
#define AS_USE_NAMESPACE
'@

if (-not $SnippetsOnly) {
    $scanRoots = @(
        (Join-Path $root 'Plugins/Angelscript/Source/AngelscriptRuntime/ThirdParty/angelscript/source'),
        (Join-Path $root 'Plugins/Angelscript/Source/AngelscriptRuntime/Core'),
        (Join-Path $root 'Plugins/Angelscript/Source/AngelscriptTest/NewVersion/NativeEngine')
    )
    foreach ($scanRoot in $scanRoots) {
        if (-not (Test-Path -LiteralPath $scanRoot)) {
            throw "Required maintained C++ root is missing: $scanRoot"
        }
        Get-ChildItem -LiteralPath $scanRoot -Recurse -File -Include *.h,*.hpp,*.inl,*.cpp,*.cc | ForEach-Object {
            $relative = $_.FullName.Substring($root.Length).TrimStart('\','/')
            if ($relative -match '[\\/]Legacy[\\/]') { return }
            $text = [System.IO.File]::ReadAllText($_.FullName)
            foreach ($hit in @(Find-RetiredFrontendTokens -Text $text -Origin $relative)) {
                if ($hit) { $failures.Add([string]$hit) }
            }
        }
    }

    $macroHeader = Join-Path $root 'Plugins/Angelscript/Source/AngelscriptRuntime/Core/angelscript.h'
    $macroText = [System.IO.File]::ReadAllText($macroHeader)
    foreach ($macro in @('BEGIN_AS_NAMESPACE','END_AS_NAMESPACE','AS_NAMESPACE_QUALIFIER','AS_USE_NAMESPACE')) {
        if ($macroText -notmatch [regex]::Escape($macro)) {
            $failures.Add("outer namespace macro '$macro' is missing from Core/angelscript.h")
        }
    }
}

if ($failures.Count -gt 0) {
    Write-Output ("FAILED {0} retired frontend C++ token(s):" -f $failures.Count)
    $failures | Select-Object -First 80 | ForEach-Object { Write-Output $_ }
    if ($failures.Count -gt 80) {
        Write-Output ("... {0} more" -f ($failures.Count - 80))
    }
    exit 1
}

Write-Output 'PASSED: no retired nested frontend C++ namespace, qualification, alias or using declaration in the maintained Runtime/Core/NativeEngine roots.'
exit 0
