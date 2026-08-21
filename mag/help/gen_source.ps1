param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ProjectDir = [System.IO.Path]::GetFullPath($ProjectDir.TrimEnd('\', '/'))
$HelpDir = Join-Path $ProjectDir 'help'
$SourceHtmlDir = Join-Path $HelpDir 'source'
$helpRoot = [System.IO.Path]::GetFullPath($HelpDir).TrimEnd('\') + '\'
$sourceRoot = [System.IO.Path]::GetFullPath($SourceHtmlDir).TrimEnd('\') + '\'

if (-not $sourceRoot.StartsWith($helpRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to generate outside the Help directory: $SourceHtmlDir"
}
if (-not (Test-Path -LiteralPath $SourceHtmlDir)) {
    New-Item -ItemType Directory -Path $SourceHtmlDir | Out-Null
}

function Write-Utf8([string]$Path, [string]$Content) {
    [System.IO.File]::WriteAllText($Path, $Content, [System.Text.UTF8Encoding]::new($false))
}

function Escape-Html([string]$Text) {
    return $Text.Replace('&', '&amp;').Replace('<', '&lt;').Replace('>', '&gt;').Replace('"', '&quot;')
}

function Get-LineCount([string]$Text) {
    if (-not $Text.Length) { return 0 }
    return ([regex]::Matches($Text, "\r\n|\n|\r").Count + 1)
}

# Only compilation inputs owned by this project are embedded. The generator does
# not run the C preprocessor and never reads or emits Windows SDK header contents.
$sourceFiles = @(Get-ChildItem -LiteralPath $ProjectDir -File |
    Where-Object { $_.Extension -match '^\.(c|h|rc)$' } |
    Sort-Object Name)

if (-not $sourceFiles.Count) {
    throw "No project-owned C/header/resource inputs found in $ProjectDir"
}

foreach ($oldPage in Get-ChildItem -LiteralPath $SourceHtmlDir -File -Filter '*.htm') {
    $oldPath = [System.IO.Path]::GetFullPath($oldPage.FullName)
    if (-not $oldPath.StartsWith($sourceRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove an unexpected generated page: $oldPath"
    }
    Remove-Item -LiteralPath $oldPath -Force
}

$keywords = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
@(
    'auto','break','case','char','const','continue','default','do','double','else',
    'enum','extern','float','for','goto','if','inline','int','long','register','return',
    'short','signed','sizeof','static','struct','switch','typedef','union','unsigned',
    'void','volatile','while','_Alignas','_Alignof','_Atomic','_Bool','_Complex',
    '_Generic','_Imaginary','_Noreturn','_Static_assert','_Thread_local',
    '__cdecl','__fastcall','__forceinline','__inline','__stdcall','__declspec','__pragma',
    'WINAPI','CALLBACK','APIENTRY','BEGIN','END','MENU','POPUP','MENUITEM','SEPARATOR',
    'RCDATA','STRINGTABLE','DIALOGEX','CONTROL','CAPTION','FONT','STYLE','EXSTYLE'
) | ForEach-Object { [void]$keywords.Add($_) }

$types = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
@(
    'BOOL','BOOLEAN','BYTE','CHAR','COLORREF','DWORD','DWORD64','FLOAT','HANDLE','HBITMAP',
    'HBRUSH','HCURSOR','HDC','HDESK','HGDIOBJ','HGLOBAL','HICON','HINSTANCE','HMENU',
    'HMODULE','HMONITOR','HRESULT','HRSRC','HTHUMBNAIL','HWND','INT','INT64','LARGE_INTEGER',
    'LONG','LONG64','LPARAM','LPCSTR','LPCVOID','LPCWSTR','LPCTSTR','LPVOID','LPWSTR','LRESULT',
    'NTSTATUS','POINT','RECT','REFIID','SIZE','SIZE_T','TCHAR','UINT','UINT32','UINT64','ULONG',
    'ULONG64','WCHAR','WORD','WPARAM','GLbitfield','GLboolean','GLbyte','GLclampd','GLclampf',
    'GLdouble','GLenum','GLfloat','GLint','GLshort','GLsizei','GLubyte','GLuint','GLushort','GLvoid',
    'VkResult','VkInstance','VkDevice','VkQueue','VkImage','VkSurfaceKHR','VkSwapchainKHR'
) | ForEach-Object { [void]$types.Add($_) }

$macros = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)

foreach ($file in $sourceFiles) {
    $text = [System.IO.File]::ReadAllText($file.FullName)
    foreach ($match in ([regex]'(?m)^[ \t]*#[ \t]*define[ \t]+([A-Za-z_]\w*)').Matches($text)) {
        [void]$macros.Add($match.Groups[1].Value)
    }
    if ($file.Extension -ieq '.h') {
        foreach ($match in ([regex]'(?s)\btypedef\b.*?\b([A-Za-z_]\w*)\s*;').Matches($text)) {
            [void]$types.Add($match.Groups[1].Value)
        }
        foreach ($match in ([regex]'\}\s*([A-Za-z_]\w*)\s*(?:,\s*\*?\s*([A-Za-z_]\w*))?\s*;').Matches($text)) {
            [void]$types.Add($match.Groups[1].Value)
            if ($match.Groups[2].Success) { [void]$types.Add($match.Groups[2].Value) }
        }
    }
}

$tokenPattern = @'
(?s)(/\*.*?\*/)|(//[^\r\n]*)|(^[ \t]*#[^\r\n]*)|("(?:[^"\\]|\\.)*")|('(?:[^'\\]|\\.)*')|\b(0x[0-9A-Fa-f]+[UuLl]*|\d+(?:\.\d*)?(?:[eE][+-]?\d+)?[fFlLuU]*)\b|\b([A-Za-z_]\w*)\b
'@
$tokenRegex = [regex]::new($tokenPattern.Trim(), [System.Text.RegularExpressions.RegexOptions]::Multiline)

function ConvertTo-HighlightedC([string]$Code) {
    $builder = [System.Text.StringBuilder]::new([math]::Max(256, [int]($Code.Length * 1.4)))
    $position = 0
    foreach ($match in $tokenRegex.Matches($Code)) {
        if ($match.Index -gt $position) {
            [void]$builder.Append((Escape-Html $Code.Substring($position, $match.Index - $position)))
        }
        $escaped = Escape-Html $match.Value
        if ($match.Groups[1].Success -or $match.Groups[2].Success) {
            [void]$builder.Append("<span class='cm'>$escaped</span>")
        }
        elseif ($match.Groups[3].Success) {
            [void]$builder.Append("<span class='pp'>$escaped</span>")
        }
        elseif ($match.Groups[4].Success -or $match.Groups[5].Success) {
            [void]$builder.Append("<span class='str'>$escaped</span>")
        }
        elseif ($match.Groups[6].Success) {
            [void]$builder.Append("<span class='num'>$escaped</span>")
        }
        else {
            $identifier = $match.Groups[7].Value
            if ($keywords.Contains($identifier)) {
                [void]$builder.Append("<span class='kw'>$escaped</span>")
            }
            elseif ($macros.Contains($identifier)) {
                [void]$builder.Append("<span class='mc'>$escaped</span>")
            }
            elseif ($types.Contains($identifier)) {
                [void]$builder.Append("<span class='tp'>$escaped</span>")
            }
            else {
                $next = $match.Index + $match.Length
                while ($next -lt $Code.Length -and ($Code[$next] -eq ' ' -or $Code[$next] -eq "`t")) { ++$next }
                if ($next -lt $Code.Length -and $Code[$next] -eq '(') {
                    [void]$builder.Append("<span class='fn'>$escaped</span>")
                }
                else {
                    [void]$builder.Append($escaped)
                }
            }
        }
        $position = $match.Index + $match.Length
    }
    if ($position -lt $Code.Length) {
        [void]$builder.Append((Escape-Html $Code.Substring($position)))
    }
    $html = $builder.ToString()
    return [regex]::Replace(
        $html,
        '(?m)[ \t]+(?=\r?$)',
        {
            param($match)
            return (($match.Value.ToCharArray() | ForEach-Object {
                if ($_ -eq "`t") { '&#9;' } else { '&#32;' }
            }) -join '')
        })
}

$purposes = @{
    'main.c' = 'Process startup, DPI awareness, window class registration, message loop, and command-line entry.'
    'mag.c' = 'MAG window procedure, message crackers, modes, Settings dialog, persistence, and window behavior.'
    'render.c' = 'Capture orchestration, source/view geometry, rendering lifecycle, presentation, resize/move contracts, and verification.'
    'presentation.c' = 'Presentation models, surface/host/copy/adapter resolution, validation, and status reporting.'
    'presentation_observer.c' = 'Process-scoped DxgKrnl/Win32k ETW presentation observation, exact mode classification, and provider diagnostics.'
    'presentation_observer.h' = 'Presentation observer lifecycle, status, and exact observed-mode callback contract.'
    'dwmprivate.c' = 'Private DWM shared desktop/window visuals, retained DirectComposition tree, transform/clip, overlay reservoir, and atomic updates.'
    'graphics.c' = 'Graphics backend catalog and shared backend dispatch.'
    'graphics_d3d9.c' = 'Direct3D 9 renderer and presentation backend.'
    'graphics_d3d11.c' = 'Direct3D 11/DXGI renderer and presentation backend.'
    'graphics_d3d12.c' = 'Direct3D 12 command, resource, fence, and DXGI presentation backend.'
    'graphics_gdi.c' = 'GDI DIB/HDC renderer and CPU frame path.'
    'graphics_opengl.c' = 'WGL/OpenGL context, texture, overlay/text, and SwapBuffers path.'
    'graphics_vulkan.c' = 'Vulkan loader/device/swapchain, resource, command, synchronization, and present path.'
    'graphics_dcomp.c' = 'DirectComposition presentation host.'
    'graphics_layered.c' = 'Traditional User32 layered-window presenter and capacity reservoir.'
    'graphics_presentation_manager.c' = 'Windows 11 Presentation Manager host and surface lifecycle.'
    'ui_renderer.c' = 'Backend-neutral UI draw list, Direct2D UI, text selection, and overlay resources.'
    'dcompabi.h' = 'C ABI declarations for the DirectComposition interfaces used by MAG.'
    'd2dwriteabi.h' = 'C ABI declarations for the Direct2D/DirectWrite interfaces used by MAG.'
    'wingdix.c' = 'Extended Win32/GDI helpers and message-cracker support implementation.'
    'winuserx.c' = 'Extended User32/resource helpers.'
    'Resource.rc' = 'Dialogs, icons, strings, embedded CHM, and native resources.'
}

$generated = [System.Collections.Generic.List[object]]::new()
foreach ($file in $sourceFiles) {
    $raw = [System.IO.File]::ReadAllText($file.FullName)
    $safeName = ($file.Name -replace '[^A-Za-z0-9_-]', '_')
    $relativePath = "source/$safeName.htm"
    $purpose = if ($purposes.ContainsKey($file.Name)) { $purposes[$file.Name] } else {
        switch ($file.Extension.ToLowerInvariant()) {
            '.c' { 'Project-owned C implementation.' }
            '.h' { 'Project-owned C declarations, types, constants, and ABI surface.' }
            '.rc' { 'Project-owned Windows resource declarations.' }
        }
    }
    $generated.Add([pscustomobject]@{
        Name = $file.Name
        RelPath = $relativePath
        OutPath = (Join-Path $SourceHtmlDir "$safeName.htm")
        Raw = $raw
        Lines = Get-LineCount $raw
        Bytes = $file.Length
        Purpose = $purpose
    })
}

for ($index = 0; $index -lt $generated.Count; ++$index) {
    $item = $generated[$index]
    $previous = if ($index -gt 0) { "<a href='$([System.IO.Path]::GetFileName($generated[$index - 1].RelPath))'>&larr; $(Escape-Html $generated[$index - 1].Name)</a>" } else { '&larr; first file' }
    $next = if ($index + 1 -lt $generated.Count) { "<a href='$([System.IO.Path]::GetFileName($generated[$index + 1].RelPath))'>$(Escape-Html $generated[$index + 1].Name) &rarr;</a>" } else { 'last file &rarr;' }
    $highlighted = ConvertTo-HighlightedC $item.Raw
    $html = @"
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta http-equiv="X-UA-Compatible" content="IE=edge">
  <title>$(Escape-Html $item.Name) - MAG Source Code</title>
  <link rel="stylesheet" type="text/css" href="../help.css">
</head>
<body>
<p class="breadcrumb"><a href="../index.htm">MAG Help</a> &rsaquo; <a href="../source.htm">Source Code</a> &rsaquo; $(Escape-Html $item.Name)</p>
<h1>$(Escape-Html $item.Name)</h1>
<p>$(Escape-Html $item.Purpose)</p>
<div class="source-meta"><strong>$($item.Lines) lines</strong> &middot; $($item.Bytes) bytes &middot; project-owned source &middot; generated from the current checkout</div>
<p class="source-nav">$previous &nbsp; | &nbsp; <a href="../source.htm">file index</a> &nbsp; | &nbsp; $next</p>
<pre class="source-code">$highlighted</pre>
<p class="source-nav">$previous &nbsp; | &nbsp; <a href="../source.htm">file index</a> &nbsp; | &nbsp; $next</p>
</body>
</html>
"@
    Write-Utf8 $item.OutPath $html
}

$totalLines = ($generated | Measure-Object -Property Lines -Sum).Sum
$totalBytes = ($generated | Measure-Object -Property Bytes -Sum).Sum
$sourceRows = ($generated | ForEach-Object {
    "  <tr><td class='key'><a href='$($_.RelPath)'>$(Escape-Html $_.Name)</a></td><td>$($_.Lines)</td><td>$(Escape-Html $_.Purpose)</td></tr>"
}) -join "`r`n"

$sourceIndex = @"
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta http-equiv="X-UA-Compatible" content="IE=edge">
  <title>Embedded Source Code - MAG</title>
  <link rel="stylesheet" type="text/css" href="help.css">
</head>
<body>
<p class="breadcrumb"><a href="index.htm">MAG Help</a> &rsaquo; Embedded Source Code</p>
<h1>Embedded Source Code</h1>
<div class="hero">
  <p class="lead">The CHM embeds the complete current project-owned C, header, and resource inputs: $($generated.Count) files, $totalLines lines, and $totalBytes bytes.</p>
  <p>No preprocessed output and no Windows SDK header contents are included. COM-based Windows graphics APIs are consumed from C through their ABI declarations and vtables.</p>
</div>
<div class="help-figure figure-wide">
  <img src="images/mag-help-source.png" width="1000" alt="MAG compiled Help viewer open to the Embedded Source Code topic with the Developer Reference tree expanded.">
  <p class="caption">The embedded source browser in the compiled MAG Help viewer. Expand Developer Reference to open any current project compilation input.</p>
</div>
<h2>Start with the major subsystems</h2>
<table class="topic-grid">
  <tr><td><strong><a href="source/mag_c.htm">mag.c</a></strong><br>Window messages, Settings, modes, persistence, and interaction.</td><td><strong><a href="source/render_c.htm">render.c</a></strong><br>Capture/render orchestration, geometry, presentation, and live-resize contracts.</td></tr>
  <tr><td><strong><a href="source/dwmprivate_c.htm">dwmprivate.c</a></strong><br>Shared DWM sources, per-window visuals, wrapper transform/clip, overlay reservoir, and atomic commit.</td><td><strong><a href="source/presentation_c.htm">presentation.c</a></strong><br>Targets, surfaces, hosts, copy classes, adapter resolution, and complete-route validation.</td></tr>
  <tr><td><strong><a href="source/graphics_d3d12_c.htm">graphics_d3d12.c</a></strong><br>Explicit D3D12 resources, commands, fences, and DXGI presentation.</td><td><strong><a href="source/graphics_vulkan_c.htm">graphics_vulkan.c</a></strong><br>Vulkan loader, device, swapchain, command, synchronization, and present path.</td></tr>
</table>
<h2>All project compilation inputs</h2>
<table>
  <tr><th>File</th><th>Lines</th><th>Responsibility</th></tr>
$sourceRows
</table>
</body>
</html>
"@
Write-Utf8 (Join-Path $HelpDir 'source.htm') $sourceIndex

$staticTopics = @(
    'help.css',
    'index.htm',
    'controls.htm',
    'context_menu.htm',
    'settings.htm',
    'capture_rendering.htm',
    'presentation.htm',
    'graphics_architecture.htm',
    'troubleshooting.htm',
    'source.htm'
)
$imageFiles = @(Get-ChildItem -LiteralPath (Join-Path $HelpDir 'images') -File -Filter '*.png' |
    Sort-Object Name |
    ForEach-Object { 'images/' + $_.Name })
$hhpFiles = @($staticTopics) + @($imageFiles) + @($generated | ForEach-Object { $_.RelPath })

$hhp = @"
[OPTIONS]
Compatibility=1.1 or later
Compiled file=mag.chm
Contents file=mag.hhc
Index file=mag.hhk
Default Window=Main
Default topic=index.htm
Display compile progress=No
Full-text search=Yes
Language=0x409 English (United States)
Title=MAG Screen Magnifier Help

[WINDOWS]
Main="MAG Screen Magnifier Help","mag.hhc","mag.hhk","index.htm","index.htm",,,,,0x23520,,0x387e,,,,,,,,0

[FILES]
$($hhpFiles -join "`r`n")

[INFOTYPES]
"@
[System.IO.File]::WriteAllText((Join-Path $HelpDir 'mag.hhp'), $hhp, [System.Text.Encoding]::ASCII)

$sourceEntries = ($generated | ForEach-Object {
    "      <LI><OBJECT type=`"text/sitemap`"><param name=`"Name`" value=`"$(Escape-Html $_.Name)`"><param name=`"Local`" value=`"$($_.RelPath)`"></OBJECT></LI>"
}) -join "`r`n"

$hhc = @"
<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">
<HTML><HEAD>
<meta name="GENERATOR" content="MAG Help source generator">
<!-- Sitemap 1.0 -->
</HEAD><BODY>
<OBJECT type="text/site properties"><param name="Window Styles" value="0x800025"><param name="ImageType" value="Folder"></OBJECT>
<UL>
  <LI><OBJECT type="text/sitemap"><param name="Name" value="MAG Screen Magnifier"><param name="Local" value="index.htm"></OBJECT></LI>
  <LI><OBJECT type="text/sitemap"><param name="Name" value="Using MAG"></OBJECT>
    <UL>
      <LI><OBJECT type="text/sitemap"><param name="Name" value="Controls and Shortcuts"><param name="Local" value="controls.htm"></OBJECT></LI>
      <LI><OBJECT type="text/sitemap"><param name="Name" value="Context Menu and Help"><param name="Local" value="context_menu.htm"></OBJECT></LI>
    </UL>
  </LI>
  <LI><OBJECT type="text/sitemap"><param name="Name" value="Settings"><param name="Local" value="settings.htm"></OBJECT>
    <UL>
      <LI><OBJECT type="text/sitemap"><param name="Name" value="Capture and Rendering"><param name="Local" value="capture_rendering.htm"></OBJECT></LI>
      <LI><OBJECT type="text/sitemap"><param name="Name" value="Presentation and Composition"><param name="Local" value="presentation.htm"></OBJECT></LI>
      <LI><OBJECT type="text/sitemap"><param name="Name" value="Windows Graphics Architecture"><param name="Local" value="graphics_architecture.htm"></OBJECT></LI>
    </UL>
  </LI>
  <LI><OBJECT type="text/sitemap"><param name="Name" value="Troubleshooting"><param name="Local" value="troubleshooting.htm"></OBJECT></LI>
  <LI><OBJECT type="text/sitemap"><param name="Name" value="Developer Reference"></OBJECT>
    <UL>
      <LI><OBJECT type="text/sitemap"><param name="Name" value="Embedded Source Code"><param name="Local" value="source.htm"></OBJECT>
        <UL>
$sourceEntries
        </UL>
      </LI>
    </UL>
  </LI>
</UL>
</BODY></HTML>
"@
Write-Utf8 (Join-Path $HelpDir 'mag.hhc') $hhc

$indexDefinitions = @(
    'Adapters|presentation.htm',
    'Allow tearing|presentation.htm',
    'Alpha mode|presentation.htm',
    'Active pipeline DAG|settings.htm',
    'Immediate settings|settings.htm',
    'Interactive settings nodes|settings.htm',
    'Auto settings|settings.htm',
    'Blank window|troubleshooting.htm',
    'Buffers|settings.htm',
    'Capture API|capture_rendering.htm',
    'Chrome capture|troubleshooting.htm',
    'Color key|presentation.htm',
    'Composed Flip|presentation.htm',
    'Composed Copy with CPU GDI|presentation.htm',
    'Composed Copy with GPU GDI|presentation.htm',
    'Composition host|presentation.htm',
    'Configuration status|settings.htm',
    'Constant alpha|settings.htm',
    'Context menu|context_menu.htm',
    'Copy class|presentation.htm',
    'Copy policy|presentation.htm',
    'CPU round trip|presentation.htm',
    'Desktop Window Manager|graphics_architecture.htm',
    'Direct2D|graphics_architecture.htm',
    'Direct3D 9|graphics_architecture.htm',
    'Direct3D 11|graphics_architecture.htm',
    'Direct3D 12|graphics_architecture.htm',
    'DirectComposition|graphics_architecture.htm',
    'DirectWrite|graphics_architecture.htm',
    'Display adapter|presentation.htm',
    'Display driver|graphics_architecture.htm',
    'DWM Private Visual|graphics_architecture.htm',
    'DWM redirection surface|graphics_architecture.htm',
    'DWM shared desktop visual|graphics_architecture.htm',
    'DWM Thumbnail|capture_rendering.htm',
    'DXGI|graphics_architecture.htm',
    'DXGI Desktop Duplication|capture_rendering.htm',
    'Electron capture|troubleshooting.htm',
    'Escape key|controls.htm',
    'Exact presentation target|presentation.htm',
    'Exit|context_menu.htm',
    'Flicker|troubleshooting.htm',
    'Follow Mouse|controls.htm',
    'GDI|graphics_architecture.htm',
    'GDI BitBlt|capture_rendering.htm',
    'GDI+|graphics_architecture.htm',
    'GDI+ flat C API|graphics_architecture.htm',
    'GitHub Desktop capture|troubleshooting.htm',
    'Glyph atlas|capture_rendering.htm',
    'GPU-local copy|presentation.htm',
    'Graphics API|capture_rendering.htm',
    'Graphics architecture|graphics_architecture.htm',
    'Hardware adapter|presentation.htm',
    'Hardware Composed Independent Flip|presentation.htm',
    'Independent Flip|presentation.htm',
    'Help Contents|context_menu.htm',
    'Help Index|context_menu.htm',
    'Help Search|context_menu.htm',
    'Installable Client Driver (ICD)|graphics_architecture.htm',
    'Kernel-mode display driver|graphics_architecture.htm',
    'Keyboard shortcuts|controls.htm',
    'Latency|settings.htm',
    'Layered window|presentation.htm',
    'Legacy Copy to front buffer|presentation.htm',
    'Legacy Flip|presentation.htm',
    'Lens|controls.htm',
    'Minimap|controls.htm',
    'Missing application window|troubleshooting.htm',
    'Mouse-relative zoom|controls.htm',
    'No redirection surface|graphics_architecture.htm',
    'OpenGL|graphics_architecture.htm',
    'Per-pixel alpha|presentation.htm',
    'Presentation Manager|presentation.htm',
    'Presentation model|presentation.htm',
    'Presentation target|presentation.htm',
    'Private DWM API|graphics_architecture.htm',
    'Redirection surface|graphics_architecture.htm',
    'Reset settings|troubleshooting.htm',
    'Resize|controls.htm',
    'Resize flicker|troubleshooting.htm',
    'Scan-out|graphics_architecture.htm',
    'Settings|settings.htm',
    'Source code|source.htm',
    'Space key|controls.htm',
    'Stale frames|troubleshooting.htm',
    'Startup delay|troubleshooting.htm',
    'Strict zero-copy|presentation.htm',
    'Surface ownership|presentation.htm',
    'Sync interval|settings.htm',
    'Taskbar in Private Visual|graphics_architecture.htm',
    'Text renderer|capture_rendering.htm',
    'Traditional layered window|presentation.htm',
    'Troubleshooting|troubleshooting.htm',
    'UI API|capture_rendering.htm',
    'User-mode display driver|graphics_architecture.htm',
    'View transform and clip|graphics_architecture.htm',
    'Vulkan|graphics_architecture.htm',
    'Vulkan layers|graphics_architecture.htm',
    'Vulkan loader|graphics_architecture.htm',
    'Vulkan WSI|graphics_architecture.htm',
    'WARP|presentation.htm',
    'WDDM|graphics_architecture.htm',
    'WGL|graphics_architecture.htm',
    'Windows Graphics Capture|capture_rendering.htm',
    'Window mode|controls.htm',
    'Windows graphics kernel|graphics_architecture.htm',
    'Zero-copy|presentation.htm',
    'Zoom|controls.htm'
)
$indexItems = [System.Collections.Generic.List[object]]::new()
foreach ($definition in $indexDefinitions) {
    $parts = $definition.Split('|', 2)
    $indexItems.Add([pscustomobject]@{ Name = $parts[0]; Local = $parts[1] })
}
foreach ($item in $generated) {
    $indexItems.Add([pscustomobject]@{ Name = $item.Name; Local = $item.RelPath })
}
$indexRows = ($indexItems | Sort-Object Name, Local | ForEach-Object {
    "  <LI><OBJECT type='text/sitemap'><param name='Name' value='$(Escape-Html $_.Name)'><param name='Local' value='$($_.Local)'></OBJECT>"
}) -join "`r`n"
$hhk = @"
<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">
<HTML><HEAD>
<meta name="GENERATOR" content="MAG Help source generator">
<!-- Sitemap 1.0 -->
</HEAD><BODY>
<UL>
$indexRows
</UL>
</BODY></HTML>
"@
Write-Utf8 (Join-Path $HelpDir 'mag.hhk') $hhk

Write-Host "gen_source: embedded $($generated.Count) project files, $totalLines lines; no SDK preprocessing"
Write-Host "gen_source: listed $($staticTopics.Count) topics and $($imageFiles.Count) PNG images"
