param(
    [Parameter(Mandatory=$true)]
    [string]$ProjectDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ProjectDir    = $ProjectDir.TrimEnd('\','/')
$HelpDir       = Join-Path $ProjectDir 'help'
$SourceHtmlDir = Join-Path $HelpDir 'source'

if (-not (Test-Path $SourceHtmlDir)) {
    New-Item -ItemType Directory -Path $SourceHtmlDir | Out-Null
}

function EscapeHtml([string]$text) {
    $text = $text.Replace('&', '&amp;')
    $text = $text.Replace('<', '&lt;')
    $text = $text.Replace('>', '&gt;')
    return $text
}

function WriteFile([string]$path, [string]$content) {
    [System.IO.File]::WriteAllText($path, $content, (New-Object System.Text.UTF8Encoding($false)))
}

# ---------------------------------------------------------------------------
# Locate cl.exe via vswhere, return $null if not found
# ---------------------------------------------------------------------------
function Find-ClExe {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { return $null }

    $vsPath = (& $vswhere -latest -products '*' `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath 2>$null) | Select-Object -First 1
    if (-not $vsPath) { return $null }

    $vcvarsall = Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
    if (-not (Test-Path $vcvarsall)) { return $null }

    return $vcvarsall   # caller uses vcvarsall to set env, then calls cl.exe
}

# ---------------------------------------------------------------------------
# Run cl.exe /P on a probe file; return path to .i file or $null on failure.
# /P keeps #line markers (no /EP) so we can extract included file paths.
# ---------------------------------------------------------------------------
function Invoke-Preprocess([string]$vcvarsall, [string]$ProjectDir) {
    # Probe includes framework.h (master include) + any system headers
    # included directly in .c files
    $includes = [System.Collections.Generic.LinkedList[string]]::new()
    [void]$includes.AddLast('#include "framework.h"')
    Get-ChildItem $ProjectDir -File -Filter '*.c' | ForEach-Object {
        $text = [System.IO.File]::ReadAllText($_.FullName)
        ([regex]'#include\s+<([^>]+)>').Matches($text) | ForEach-Object {
            [void]$includes.AddLast("#include <$($_.Groups[1].Value)>")
        }
    }

    $probeFile = Join-Path $ProjectDir '_probe_types.c'
    $iFile     = [System.IO.Path]::ChangeExtension($probeFile, '.i')

    ($includes | Select-Object -Unique) | Set-Content $probeFile -Encoding ASCII

    $batFile = Join-Path $env:TEMP 'mag_probe_cl.bat'
    $bat = "@echo off`r`ncall `"$vcvarsall`" x64 >nul 2>&1`r`ncd /d `"$ProjectDir`"`r`ncl.exe /P /nologo /w `"_probe_types.c`" >nul 2>&1`r`n"
    [System.IO.File]::WriteAllText($batFile, $bat, [System.Text.Encoding]::ASCII)

    Write-Host 'gen_source: preprocessing with cl.exe...'
    $null = Start-Process -FilePath 'cmd.exe' -ArgumentList "/c `"$batFile`"" `
        -Wait -PassThru -WindowStyle Hidden

    Remove-Item $batFile   -Force -ErrorAction SilentlyContinue
    Remove-Item $probeFile -Force -ErrorAction SilentlyContinue

    if (Test-Path $iFile) { return $iFile }
    return $null
}

# ---------------------------------------------------------------------------
# Parse a .i file (with #line markers) to get:
#   - All typedef-declared type names
#   - All unique header file paths that were included
# ---------------------------------------------------------------------------
function Parse-PreprocessedFile([string]$iPath) {
    $types       = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    $headerPaths = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)

    Write-Host "gen_source: parsing preprocessed output ($([int]((Get-Item $iPath).Length / 1KB)) KB)..."
    $content = [System.IO.File]::ReadAllText($iPath)

    # Collect all files referenced by #line markers
    $lineRe = [regex]'(?m)^#line\s+\d+\s+"([^"]+)"'
    foreach ($m in $lineRe.Matches($content)) {
        $p = $m.Groups[1].Value -replace '\\\\', '\' -replace '\\/', '/'
        if (Test-Path $p -PathType Leaf) {
            [void]$headerPaths.Add($p)
        }
    }

    # --- Typedef extraction ---
    # We need to handle all typedef forms correctly.
    # Strategy: find every "typedef" keyword, then grab the identifier(s) that
    # immediately precede the closing ";" of that declaration.
    #
    # Form 1 — struct/union/enum typedef (may have anonymous or named tag):
    #   typedef struct TAG { ... } NAME [, *LPNAME [, FAR *FARNAME]] ;
    #   Reliable anchor: the closing "}" of the brace body.
    #   After "}" there is one or more comma-separated declarators before ";".
    #   Each declarator is:  [*] [qualifier*] IDENTIFIER
    #
    # Form 2 — simple typedef (no braces):
    #   typedef <existing-type> [*] NAME ;
    #   The new name is the last bare identifier before ";"
    #
    # Form 3 — function-pointer typedef:
    #   typedef RET (CONV * NAME) (params) ;
    #   The new name is inside "( ... * NAME )"

    # Form 1: everything after } up to ; on the same "logical" line
    $form1Re = [regex]'(?m)\}([^;]+);'
    foreach ($m in $form1Re.Matches($content)) {
        $decl = $m.Groups[1].Value
        # Each word that is not a qualifier keyword is a potential type name
        $qualifiers = [System.Collections.Generic.HashSet[string]]::new(
            [string[]]@('FAR','NEAR','__far','__near','const','volatile','__cdecl','__stdcall'),
            [System.StringComparer]::Ordinal)
        ([regex]'\b([A-Za-z_]\w*)\b').Matches($decl) | ForEach-Object {
            $n = $_.Groups[1].Value
            if ($n.Length -gt 1 -and -not $qualifiers.Contains($n)) {
                [void]$types.Add($n)
            }
        }
    }

    # Form 2: typedef <stuff> NAME ; where there are no braces
    # Match typedef declarations that don't contain '{' or '}'
    $form2Re = [regex]'(?m)^\s*typedef\b([^{}]+);'
    foreach ($m in $form2Re.Matches($content)) {
        $decl = $m.Groups[1].Value
        # The last bare identifier before ';' is the new type name
        $ids = ([regex]'\b([A-Za-z_]\w*)\b').Matches($decl)
        if ($ids.Count -gt 0) {
            $last = $ids[$ids.Count - 1].Groups[1].Value
            if ($last.Length -gt 1) { [void]$types.Add($last) }
        }
        # Form 3 embedded: function pointer name is inside (* NAME)
        ([regex]'\(\s*\w+\s*\*\s*([A-Za-z_]\w*)\s*\)').Matches($decl) | ForEach-Object {
            $n = $_.Groups[1].Value
            if ($n.Length -gt 1) { [void]$types.Add($n) }
        }
    }

    Write-Host "gen_source: extracted $($types.Count) types, $($headerPaths.Count) included headers"
    return [PSCustomObject]@{ Types = $types; Headers = $headerPaths }
}

# ---------------------------------------------------------------------------
# Scan included header files for non-function-like macro definitions.
# Returns a HashSet of macro names.
# ---------------------------------------------------------------------------
function Get-MacroNames([System.Collections.Generic.HashSet[string]]$headerPaths) {
    $macros  = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    # Simple (non-function-like) #define:  #define NAME <non-paren or nothing>
    $defineRe = [regex]'(?m)^[ \t]*#[ \t]*define[ \t]+([A-Za-z_]\w*)(?![ \t]*\()'

    $count = 0
    foreach ($path in $headerPaths) {
        try {
            $text = [System.IO.File]::ReadAllText($path)
            foreach ($m in $defineRe.Matches($text)) {
                [void]$macros.Add($m.Groups[1].Value)
                $count++
            }
        } catch { }
    }
    Write-Host "gen_source: collected $($macros.Count) macro names from $($headerPaths.Count) headers"
    return $macros
}

# ---------------------------------------------------------------------------
# Fallback: extract typedef names from project .h files only (no cl.exe)
# ---------------------------------------------------------------------------
function Get-ProjectTypeNames([string[]]$headerPaths) {
    $names = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($path in $headerPaths) {
        $text = [System.IO.File]::ReadAllText($path)
        ([regex]'\}\s*([A-Z][A-Za-z0-9_]+)\s*(?:,\s*\*\s*([A-Z][A-Za-z0-9_]+))?\s*;').Matches($text) | ForEach-Object {
            if ($_.Groups[1].Success) { [void]$names.Add($_.Groups[1].Value) }
            if ($_.Groups[2].Success) { [void]$names.Add($_.Groups[2].Value) }
        }
        ([regex]'\(\s*\w+\s*\*\s*([A-Z][A-Za-z0-9_]+)\s*\)').Matches($text) | ForEach-Object {
            if ($_.Groups[1].Success) { [void]$names.Add($_.Groups[1].Value) }
        }
    }
    return $names | Where-Object { $_.Length -gt 2 }
}

# ---------------------------------------------------------------------------
# Build keyword / type / macro sets
# ---------------------------------------------------------------------------
$cKeywordList = @(
    'auto','break','case','char','const','continue','default','do','double',
    'else','enum','extern','float','for','goto','if','inline','int','long',
    'register','return','short','signed','sizeof','static','struct','switch',
    'typedef','union','unsigned','void','volatile','while',
    '__cdecl','__fastcall','__forceinline','__inline','__stdcall',
    '__declspec','__pragma',
    'WINAPI','CALLBACK','APIENTRY',
    'BEGIN','END','MENU','POPUP','MENUITEM','SEPARATOR','RCDATA','STRINGTABLE'
)

$kwSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
foreach ($kw in $cKeywordList) { [void]$kwSet.Add($kw) }

# Minimal static type supplement (in case cl.exe is unavailable)
$staticTypeList = @(
    'NTSTATUS','BOOLEAN',
    'GLbitfield','GLboolean','GLbyte','GLclampd','GLclampf',
    'GLdouble','GLenum','GLfloat','GLint','GLshort',
    'GLsizei','GLubyte','GLuint','GLushort','GLvoid'
)

$tpSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
foreach ($t in $staticTypeList) { [void]$tpSet.Add($t) }

$mcSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)

$vcvarsall = Find-ClExe

if ($vcvarsall) {
    $iFile = Invoke-Preprocess $vcvarsall $ProjectDir
    if ($iFile) {
        $result = Parse-PreprocessedFile $iFile
        Remove-Item $iFile -Force -ErrorAction SilentlyContinue

        foreach ($t in $result.Types) {
            if (-not $kwSet.Contains($t)) { [void]$tpSet.Add($t) }
        }
        foreach ($m in (Get-MacroNames $result.Headers)) {
            # Don't duplicate keywords or types as macros
            if (-not $kwSet.Contains($m) -and -not $tpSet.Contains($m)) {
                [void]$mcSet.Add($m)
            }
        }
    }
} else {
    Write-Host 'gen_source: cl.exe not found, falling back to project header scan'
    $projectHdrs = Get-ChildItem -Path $ProjectDir -File -Filter '*.h' |
        Select-Object -ExpandProperty FullName
    foreach ($t in (Get-ProjectTypeNames $projectHdrs)) {
        if (-not $kwSet.Contains($t)) { [void]$tpSet.Add($t) }
    }
}

Write-Host "gen_source: $($tpSet.Count) types, $($mcSet.Count) macros, $($kwSet.Count) keywords"

# ---------------------------------------------------------------------------
# Tokenizer
#
# Single base regex matches all syntactically distinct token classes.
# Identifiers (group 7) are resolved via HashSet lookup — no mega-alternation.
#
# Groups:
#   1  block comment   /* ... */
#   2  line comment    // ...
#   3  preprocessor directive  (line-leading #)
#   4  string literal  "..."
#   5  char literal    '...'
#   6  number
#   7  identifier      (resolved below via HashSet + lookahead)
# ---------------------------------------------------------------------------
$script:baseRe = [regex]::new(
    "(?s)(/\*.*?\*/)" +
    "|(//.+?(?=\r?\n|\z))" +
    "|(^[ \t]*#[^\r\n]*)" +
    "|(`"(?:[^`"\\]|\\.)*`")" +
    "|('(?:[^'\\]|\\.)*')" +
    "|\b(0x[0-9A-Fa-f]+[UuLl]*|\d+(?:\.\d*)?(?:[eE][+-]?\d+)?[fFlLuU]*)\b" +
    "|\b([A-Za-z_]\w*)\b",
    [System.Text.RegularExpressions.RegexOptions]::Multiline
)

function Highlight-C {
    param([string]$code)

    $sb  = [System.Text.StringBuilder]::new([int]($code.Length * 1.5))
    $pos = 0
    $len = $code.Length

    foreach ($m in $script:baseRe.Matches($code)) {
        if ($m.Index -gt $pos) {
            [void]$sb.Append((EscapeHtml $code.Substring($pos, $m.Index - $pos)))
        }

        $esc = EscapeHtml $m.Value

        if ($m.Groups[1].Success -or $m.Groups[2].Success) {
            [void]$sb.Append("<span class='cm'>$esc</span>")
        } elseif ($m.Groups[3].Success) {
            [void]$sb.Append("<span class='pp'>$esc</span>")
        } elseif ($m.Groups[4].Success -or $m.Groups[5].Success) {
            [void]$sb.Append("<span class='str'>$esc</span>")
        } elseif ($m.Groups[6].Success) {
            [void]$sb.Append("<span class='num'>$esc</span>")
        } elseif ($m.Groups[7].Success) {
            $id = $m.Value
            if ($kwSet.Contains($id)) {
                [void]$sb.Append("<span class='kw'>$esc</span>")
            } elseif ($tpSet.Contains($id)) {
                [void]$sb.Append("<span class='tp'>$esc</span>")
            } elseif ($mcSet.Contains($id)) {
                [void]$sb.Append("<span class='mc'>$esc</span>")
            } else {
                # Function: next non-whitespace char after identifier is '('
                $ni = $m.Index + $m.Length
                while ($ni -lt $len -and ($code[$ni] -eq ' ' -or $code[$ni] -eq "`t")) { $ni++ }
                if ($ni -lt $len -and $code[$ni] -eq '(') {
                    [void]$sb.Append("<span class='fn'>$esc</span>")
                } else {
                    [void]$sb.Append($esc)
                }
            }
        } else {
            [void]$sb.Append($esc)
        }

        $pos = $m.Index + $m.Length
    }

    if ($pos -lt $len) {
        [void]$sb.Append((EscapeHtml $code.Substring($pos)))
    }

    return $sb.ToString()
}

# ---------------------------------------------------------------------------
# Generate per-file source pages
# ---------------------------------------------------------------------------
$sourceFiles = Get-ChildItem -Path $ProjectDir -File |
    Where-Object { $_.Extension -match '^\.(c|h|rc)$' } |
    Sort-Object Name

$generated = @()

$fileTemplate = @'
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta http-equiv="X-UA-Compatible" content="IE=edge">
  <title>{0} - Source Code</title>
  <link rel="stylesheet" type="text/css" href="../help.css">
</head>
<body>
<p class="breadcrumb"><a href="../index.htm">MAG Help</a> &rsaquo; <a href="../source.htm">Source Code</a> &rsaquo; {0}</p>
<h1>{0}</h1>
<pre class="source-code">{1}</pre>
</body>
</html>
'@

foreach ($file in $sourceFiles) {
    $safeName    = $file.Name -replace '\.', '_'
    $relPath     = "source/$safeName.htm"
    $outPath     = Join-Path $SourceHtmlDir "$safeName.htm"

    $raw         = [System.IO.File]::ReadAllText($file.FullName)
    $highlighted = Highlight-C $raw
    $html        = [string]::Format($fileTemplate, $file.Name, $highlighted)

    WriteFile $outPath $html
    $generated += [PSCustomObject]@{ Name = $file.Name; RelPath = $relPath }
}

# ---------------------------------------------------------------------------
# source.htm index page
# ---------------------------------------------------------------------------
$listItems = ($generated | ForEach-Object {
    "  <li><a href=`"$($_.RelPath)`">$($_.Name)</a></li>"
}) -join "`n"

$indexTemplate = @'
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta http-equiv="X-UA-Compatible" content="IE=edge">
  <title>Source Code - MAG</title>
  <link rel="stylesheet" type="text/css" href="help.css">
</head>
<body>
<p class="breadcrumb"><a href="index.htm">MAG Help</a> &rsaquo; Source Code</p>
<h1>Source Code</h1>
<p>Complete source code for MAG. Each file is shown with its full, unmodified contents.</p>
<ul>
{0}
</ul>
</body>
</html>
'@

WriteFile (Join-Path $HelpDir 'source.htm') ([string]::Format($indexTemplate, $listItems))

# ---------------------------------------------------------------------------
# Rewrite mag.hhp
# ---------------------------------------------------------------------------
$hhpFiles = [System.Collections.Generic.List[string]]::new()
foreach ($f in @('help.css','index.htm','controls.htm','context_menu.htm','source.htm')) {
    $hhpFiles.Add($f)
}
foreach ($g in $generated) { $hhpFiles.Add($g.RelPath) }

$hhpTemplate = @'
[OPTIONS]
Compatibility=1.1 or later
Compiled file=mag.chm
Contents file=mag.hhc
Index file=mag.hhk
Default Window=Main
Default topic=index.htm
Display compile progress=No
Language=0x409 English (United States)
Title=MAG Screen Magnifier Help

[WINDOWS]
Main="MAG Screen Magnifier Help","mag.hhc","mag.hhk","index.htm","index.htm",,,,,0x23520,,0x387e,,,,,,,,0

[FILES]
{0}

[INFOTYPES]

'@

[System.IO.File]::WriteAllText(
    (Join-Path $HelpDir 'mag.hhp'),
    [string]::Format($hhpTemplate, ($hhpFiles -join "`r`n")),
    [System.Text.Encoding]::ASCII
)

# ---------------------------------------------------------------------------
# Rewrite mag.hhc
# ---------------------------------------------------------------------------
$hhcEntries = ($generated | ForEach-Object {
    "  <LI><OBJECT type=`"text/sitemap`">`r`n    <param name=`"Name`" value=`"$($_.Name)`">`r`n    <param name=`"Local`" value=`"$($_.RelPath)`">`r`n  </OBJECT>"
}) -join "`r`n"

$hhcTemplate = @'
<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">
<HTML>
<HEAD>
<meta name="GENERATOR" content="Microsoft&reg; HTML Help Workshop 4.1">
<!-- Sitemap 1.0 -->
</HEAD><BODY>
<OBJECT type="text/site properties">
  <param name="Window Styles" value="0x800025">
  <param name="ImageType" value="Folder">
</OBJECT>
<UL>
  <LI><OBJECT type="text/sitemap">
    <param name="Name" value="MAG Screen Magnifier">
    <param name="Local" value="index.htm">
  </OBJECT>
  <LI><OBJECT type="text/sitemap">
    <param name="Name" value="Controls &amp; Keyboard Shortcuts">
    <param name="Local" value="controls.htm">
  </OBJECT>
  <LI><OBJECT type="text/sitemap">
    <param name="Name" value="Context Menu">
    <param name="Local" value="context_menu.htm">
  </OBJECT>
  <LI><OBJECT type="text/sitemap">
    <param name="Name" value="Source Code">
    <param name="Local" value="source.htm">
  </OBJECT>
  <UL>
{0}
  </UL>
</UL>
</BODY></HTML>
'@

[System.IO.File]::WriteAllText(
    (Join-Path $HelpDir 'mag.hhc'),
    [string]::Format($hhcTemplate, $hhcEntries),
    [System.Text.Encoding]::ASCII
)

Write-Host "gen_source: done - $($generated.Count) pages generated"
