param(
    [string]$OutputDir = (Join-Path $PSScriptRoot 'images')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$OutputDir = [System.IO.Path]::GetFullPath($OutputDir)
if (-not (Test-Path -LiteralPath $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir | Out-Null
}

$edgeCandidates = @(
    (Join-Path ${env:ProgramFiles(x86)} 'Microsoft\Edge\Application\msedge.exe'),
    (Join-Path $env:ProgramFiles 'Microsoft\Edge\Application\msedge.exe')
)
$edge = $edgeCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $edge) {
    throw 'Microsoft Edge is required to rasterize the architecture diagrams.'
}

function Escape-Xml([string]$Value) {
    return [System.Security.SecurityElement]::Escape($Value)
}

function New-Layer([string]$Label, [string]$Detail, [string]$Kind) {
    return [pscustomobject]@{ Label = $Label; Detail = $Detail; Kind = $Kind }
}

function Get-Fill([string]$Kind) {
    switch ($Kind) {
        'mag'        { return '#0067c0' }
        'api'        { return '#2563eb' }
        'runtime'    { return '#4f46e5' }
        'composition'{ return '#7c3aed' }
        'user'       { return '#0f766e' }
        'driver'     { return '#b45309' }
        'kernel'     { return '#b91c1c' }
        'gpu'        { return '#047857' }
        'output'     { return '#334155' }
        default      { return '#475569' }
    }
}

function Get-TextLines([string]$Value, [int]$Maximum = 54) {
    $words = $Value -split '\s+'
    $lines = [System.Collections.Generic.List[string]]::new()
    $line = ''
    foreach ($word in $words) {
        if (-not $line) {
            $line = $word
        }
        elseif (($line.Length + 1 + $word.Length) -le $Maximum) {
            $line += " $word"
        }
        else {
            $lines.Add($line)
            $line = $word
        }
    }
    if ($line) { $lines.Add($line) }
    return $lines
}

function Add-MultilineText(
    [System.Text.StringBuilder]$Svg,
    [double]$X,
    [double]$Y,
    [string]$Value,
    [int]$Maximum,
    [string]$Class,
    [double]$LineHeight = 24,
    [string]$Anchor = 'start') {
    $lines = @(Get-TextLines $Value $Maximum)
    [void]$Svg.Append("<text x='$X' y='$Y' text-anchor='$Anchor' class='$Class'>")
    for ($index = 0; $index -lt $lines.Count; ++$index) {
        $dy = if ($index -eq 0) { 0 } else { $LineHeight }
        [void]$Svg.Append("<tspan x='$X' dy='$dy'>$(Escape-Xml $lines[$index])</tspan>")
    }
    [void]$Svg.Append('</text>')
}

function New-Svg([int]$Width, [int]$Height, [string]$Title, [string]$Subtitle) {
    $svg = [System.Text.StringBuilder]::new()
    [void]$svg.Append("<svg xmlns='http://www.w3.org/2000/svg' width='$Width' height='$Height' viewBox='0 0 $Width $Height'>")
    [void]$svg.Append(@'
<defs>
  <filter id="shadow" x="-20%" y="-20%" width="140%" height="140%">
    <feDropShadow dx="0" dy="4" stdDeviation="5" flood-color="#0f172a" flood-opacity="0.16"/>
  </filter>
  <marker id="arrow" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse">
    <path d="M 0 0 L 10 5 L 0 10 z" fill="#64748b"/>
  </marker>
  <style>
    .title { font: 700 38px 'Segoe UI', Arial, sans-serif; fill: #0f172a; }
    .subtitle { font: 400 19px 'Segoe UI', Arial, sans-serif; fill: #475569; }
    .box-title { font: 700 21px 'Segoe UI', Arial, sans-serif; fill: white; }
    .box-detail { font: 400 15px 'Segoe UI', Arial, sans-serif; fill: #e2e8f0; }
    .panel-title { font: 700 20px 'Segoe UI', Arial, sans-serif; fill: #0f172a; }
    .note { font: 400 16px 'Segoe UI', Arial, sans-serif; fill: #334155; }
    .small { font: 400 14px 'Segoe UI', Arial, sans-serif; fill: #475569; }
    .lane { font: 700 16px 'Segoe UI', Arial, sans-serif; fill: #475569; letter-spacing: 1px; }
  </style>
</defs>
<rect width="100%" height="100%" fill="#f8fafc"/>
<rect x="24" y="24" width="calc(100% - 48px)" height="calc(100% - 48px)" rx="24" fill="#ffffff" stroke="#cbd5e1" stroke-width="2"/>
'@)
    [void]$svg.Append("<text x='70' y='82' class='title'>$(Escape-Xml $Title)</text>")
    Add-MultilineText $svg 70 116 $Subtitle 118 'subtitle' 24
    return $svg
}

function Complete-Svg([System.Text.StringBuilder]$Svg) {
    [void]$Svg.Append('</svg>')
    return $Svg.ToString()
}

function Add-Arrow([System.Text.StringBuilder]$Svg, [double]$X1, [double]$Y1, [double]$X2, [double]$Y2) {
    [void]$Svg.Append("<path d='M $X1 $Y1 L $X2 $Y2' fill='none' stroke='#64748b' stroke-width='4' marker-end='url(#arrow)'/>")
}

function Add-Box(
    [System.Text.StringBuilder]$Svg,
    [double]$X,
    [double]$Y,
    [double]$Width,
    [double]$Height,
    [string]$Label,
    [string]$Detail,
    [string]$Kind) {
    $fill = Get-Fill $Kind
    $titleSize = if ($Width -lt 300) { 17 } else { 21 }
    $detailMaximum = [math]::Max(18, [math]::Floor(($Width - 48) / 8.2))
    [void]$Svg.Append("<rect x='$X' y='$Y' width='$Width' height='$Height' rx='14' fill='$fill' filter='url(#shadow)'/>")
    [void]$Svg.Append("<text x='$($X + 24)' y='$($Y + 32)' class='box-title' style='font-size:$($titleSize)px'>$(Escape-Xml $Label)</text>")
    Add-MultilineText $Svg ($X + 24) ($Y + 58) $Detail $detailMaximum 'box-detail' 19
}

function Write-Png([string]$Name, [string]$SvgText) {
    $tempSvg = Join-Path ([System.IO.Path]::GetTempPath()) ("mag-help-$([guid]::NewGuid().ToString('N')).svg")
    $profileDir = Join-Path ([System.IO.Path]::GetTempPath()) ("mag-help-edge-$([guid]::NewGuid().ToString('N'))")
    try {
        [System.IO.File]::WriteAllText($tempSvg, $SvgText, [System.Text.UTF8Encoding]::new($false))
        $output = Join-Path $OutputDir "$Name.png"
        $widthMatch = [regex]::Match($SvgText, "<svg[^>]+width='(\d+)'[^>]+height='(\d+)'", 'IgnoreCase')
        if (-not $widthMatch.Success) {
            throw "Could not read the SVG dimensions for $Name."
        }
        $windowSize = "$($widthMatch.Groups[1].Value),$($widthMatch.Groups[2].Value)"
        $sourceUri = [uri]::new($tempSvg).AbsoluteUri
        $arguments = @(
            '--headless=new',
            '--disable-gpu',
            '--hide-scrollbars',
            '--force-device-scale-factor=1',
            "--window-size=$windowSize",
            "--user-data-dir=$profileDir",
            "--screenshot=$output",
            $sourceUri
        )
        $process = Start-Process -FilePath $edge -ArgumentList $arguments -Wait -PassThru -WindowStyle Hidden
        if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $output)) {
            throw "Failed to render $Name.png"
        }
        Write-Host "diagram: $Name.png"
    }
    finally {
        Remove-Item -LiteralPath $tempSvg -Force -ErrorAction SilentlyContinue
        if (Test-Path -LiteralPath $profileDir) {
            $resolvedProfile = [System.IO.Path]::GetFullPath($profileDir)
            $resolvedTemp = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
            if ($resolvedProfile.StartsWith($resolvedTemp, [System.StringComparison]::OrdinalIgnoreCase)) {
                Remove-Item -LiteralPath $resolvedProfile -Recurse -Force -ErrorAction SilentlyContinue
            }
        }
    }
}

function Write-StackDiagram(
    [string]$Name,
    [string]$Title,
    [string]$Subtitle,
    [object[]]$Layers,
    [string[]]$Notes,
    [string]$Setting) {
    $svg = New-Svg 1400 1000 $Title $Subtitle
    $boxX = 70
    $boxWidth = 870
    $startY = 170
    $available = 710
    $gap = 22
    $boxHeight = [math]::Floor(($available - (($Layers.Count - 1) * $gap)) / $Layers.Count)
    if ($boxHeight -gt 92) { $boxHeight = 92 }

    for ($index = 0; $index -lt $Layers.Count; ++$index) {
        $y = $startY + $index * ($boxHeight + $gap)
        $layer = $Layers[$index]
        Add-Box $svg $boxX $y $boxWidth $boxHeight $layer.Label $layer.Detail $layer.Kind
        if ($index -lt ($Layers.Count - 1)) {
            Add-Arrow $svg ($boxX + $boxWidth / 2) ($y + $boxHeight + 3) ($boxX + $boxWidth / 2) ($y + $boxHeight + $gap - 4)
        }
    }

    [void]$svg.Append("<rect x='980' y='170' width='350' height='520' rx='18' fill='#f1f5f9' stroke='#cbd5e1' stroke-width='2'/>")
    [void]$svg.Append("<text x='1010' y='212' class='panel-title'>What matters in MAG</text>")
    $noteY = 252
    foreach ($note in $Notes) {
        [void]$svg.Append("<circle cx='1017' cy='$($noteY - 5)' r='5' fill='#0067c0'/>")
        Add-MultilineText $svg 1032 $noteY $note 35 'note' 21
        $noteY += 82
    }
    [void]$svg.Append("<rect x='980' y='720' width='350' height='150' rx='18' fill='#e8f3ff' stroke='#60a5fa' stroke-width='2'/>")
    [void]$svg.Append("<text x='1010' y='760' class='panel-title'>Settings mapping</text>")
    Add-MultilineText $svg 1010 796 $Setting 36 'note' 22
    [void]$svg.Append("<text x='70' y='940' class='small'>Original MAG documentation diagram. Public and private interfaces are identified in the accompanying Help topic.</text>")
    Write-Png $Name (Complete-Svg $svg)
}

function Write-OverviewDiagram {
    $svg = New-Svg 1600 1120 'Windows graphics architecture used by MAG' 'Capture, rendering, text, composition, presentation, drivers, and scan-out are separate choices that converge in the Windows desktop.'
    [void]$svg.Append("<text x='70' y='170' class='lane'>MAG APPLICATION</text>")
    Add-Box $svg 70 190 1460 90 'MAG frame pipeline' 'Capture a source; calculate view geometry; draw overlay and text; submit through the selected presentation host.' 'mag'

    [void]$svg.Append("<text x='70' y='325' class='lane'>USER-MODE GRAPHICS APIS</text>")
    $labels = @(
        @('GDI / GDI+','HDC, DIB, BitBlt'),
        @('Direct2D + DWrite','2D drawing and text'),
        @('Direct3D 9','D3D9Ex runtime'),
        @('D3D11 + D3D12','Devices and DXGI'),
        @('OpenGL','WGL and vendor ICD'),
        @('Vulkan','Loader, WSI, and ICD')
    )
    for ($index = 0; $index -lt $labels.Count; ++$index) {
        $x = 70 + $index * 245
        Add-Box $svg $x 345 215 110 $labels[$index][0] $labels[$index][1] 'api'
        Add-Arrow $svg ($x + 108) 458 ($x + 108) 500
    }

    [void]$svg.Append("<text x='70' y='525' class='lane'>PRESENTATION AND COMPOSITION</text>")
    Add-Box $svg 70 545 450 105 'DXGI / WSI / WGL present' 'Swap chains, Win32 surfaces, buffer rotation, synchronization, and adapter/output selection.' 'runtime'
    Add-Box $svg 570 545 450 105 'DirectComposition and DWM' 'Retained visual trees, redirection/no-redirection content, desktop composition, and Private Visual sharing.' 'composition'
    Add-Box $svg 1070 545 460 105 'GDI window presentation' 'User32/GDI window surfaces, redirected content, layered-window updates, and compositor handoff.' 'user'
    Add-Arrow $svg 295 655 295 700
    Add-Arrow $svg 795 655 795 700
    Add-Arrow $svg 1300 655 1300 700

    [void]$svg.Append("<text x='70' y='725' class='lane'>WINDOWS DISPLAY DRIVER MODEL</text>")
    Add-Box $svg 70 745 700 105 'User-mode display drivers' 'Vendor D3D/OpenGL/Vulkan implementations translate API work into GPU command streams.' 'driver'
    Add-Box $svg 830 745 700 105 'Windows graphics kernel' 'win32k, dxgkrnl, VidSch, VidMm, kernel-mode driver, memory, scheduling, and present control.' 'kernel'
    Add-Arrow $svg 420 855 420 900
    Add-Arrow $svg 1180 855 1180 900

    Add-Box $svg 70 920 700 105 'GPU engines and video memory' 'Render, copy, compute, composition, overlays, and shared surfaces.' 'gpu'
    Add-Box $svg 830 920 700 105 'DWM composition to display' 'Desktop visual tree, independent/composed presentation, display engine, monitor scan-out.' 'output'
    Add-Arrow $svg 775 972 820 972
    Write-Png 'architecture-windows-overview' (Complete-Svg $svg)
}

function Write-PrivateVisualDiagram {
    $svg = New-Svg 1600 1180 'MAG DWM Private Visual architecture' 'MAG retains a stable shared source and changes only its own wrapper transform and clip; per-window visuals preserve desktop, taskbar, and application content.'
    [void]$svg.Append("<text x='70' y='170' class='lane'>COMPOSED SOURCES OWNED BY DWM</text>")
    Add-Box $svg 70 190 440 105 'Shared desktop visual' 'DWM-provided composed desktop source, excluding the MAG destination and retained supplemental HWNDs.' 'composition'
    Add-Box $svg 580 190 440 105 'Per-HWND source visuals' 'Desktop, taskbar, and visible application thumbnails, including child-composed window content.' 'composition'
    Add-Box $svg 1090 190 440 105 'MAG overlay surface reservoir' 'Premultiplied BGRA DirectComposition surface sized with spare resize capacity.' 'mag'
    Add-Arrow $svg 290 300 570 380
    Add-Arrow $svg 800 300 800 380
    Add-Arrow $svg 1310 300 1030 380

    [void]$svg.Append("<text x='70' y='355' class='lane'>APP-OWNED RETAINED VISUAL TREE</text>")
    Add-Box $svg 310 385 980 95 'viewVisual' 'Orders wallpaper, shared desktop fallback, application windows, and taskbar. The view transform lives here.' 'mag'
    Add-Arrow $svg 800 485 800 525
    Add-Box $svg 310 535 980 95 'captureVisual' 'Owns the destination clip. The shared source is not recreated for pan, zoom, move, or resize.' 'mag'
    Add-Arrow $svg 800 635 800 675
    Add-Box $svg 310 685 980 95 'rootVisual' 'Contains captureVisual plus overlayVisual; one DirectComposition target is attached to MAG HWND.' 'mag'
    Add-Arrow $svg 800 785 800 825
    Add-Box $svg 310 835 980 95 'Atomic DirectComposition Commit' 'Window ordering, source placement, transform, clip, overlay pixels, and geometry publish as one batch.' 'composition'
    Add-Arrow $svg 800 935 800 975
    Add-Box $svg 310 985 980 95 'DWM composition and scan-out' 'DWM consumes the retained tree without MAG copying the desktop through CPU memory.' 'output'

    [void]$svg.Append("<rect x='70' y='385' width='200' height='545' rx='18' fill='#f1f5f9' stroke='#cbd5e1' stroke-width='2'/>")
    [void]$svg.Append("<text x='95' y='425' class='panel-title'>Stable resources</text>")
    Add-MultilineText $svg 95 462 'Shared visual and source thumbnails persist across ordinary geometry changes.' 22 'note' 22
    Add-MultilineText $svg 95 600 'Only a larger-than-reservoir resize allocates a replacement overlay surface.' 22 'note' 22
    Add-MultilineText $svg 95 760 'Window positions and z-order are refreshed before the atomic commit.' 22 'note' 22

    [void]$svg.Append("<rect x='1330' y='385' width='200' height='545' rx='18' fill='#fff7ed' stroke='#fdba74' stroke-width='2'/>")
    [void]$svg.Append("<text x='1355' y='425' class='panel-title'>Private boundary</text>")
    Add-MultilineText $svg 1355 462 'The shared visual entry points are undocumented DWM exports resolved at runtime.' 22 'note' 22
    Add-MultilineText $svg 1355 620 'DirectComposition, D3D11, DXGI, and DWM thumbnail operations underneath are Windows components.' 22 'note' 22
    Add-MultilineText $svg 1355 800 'Availability and the complete configuration are validated before an atomic live-route change.' 22 'note' 22
    Write-Png 'architecture-dwm-private-visual' (Complete-Svg $svg)
}

Write-OverviewDiagram
Write-PrivateVisualDiagram

Write-StackDiagram 'architecture-wddm-driver-stack' 'Windows Display Driver Model stack' 'The same WDDM scheduling, memory-management, and display infrastructure serves DirectX, OpenGL, Vulkan, GDI, DWM, and hardware scan-out.' @(
    (New-Layer 'MAG and the Desktop Window Manager' 'Application rendering/capture queues coexist with the session-wide DWM composition workload.' 'mag'),
    (New-Layer 'API runtimes and system clients' 'D3D9/11/12, DXGI, Direct2D, OpenGL, Vulkan, GDI32, DirectComposition, and dwmcore.' 'runtime'),
    (New-Layer 'Vendor user-mode display driver (UMD)' 'Validates and translates commands, builds GPU work, manages API-specific state, and submits through D3DKMT.' 'driver'),
    (New-Layer 'Windows graphics kernel' 'dxgkrnl plus video scheduler and video memory manager coordinate contexts, residency, synchronization, and presents.' 'kernel'),
    (New-Layer 'Kernel-mode display miniport driver' 'Hardware-specific memory, DMA, interrupts, power, display topology, and present-path control.' 'driver'),
    (New-Layer 'GPU and display engine' 'Render/copy/compute engines, video memory, hardware overlays, composition, scan-out, and connected monitors.' 'gpu')
) @(
    'Display adapter selects the output owner.',
    'Hardware adapter selects the render device.',
    'Cross-adapter routes can add copies.',
    'DWM owns the composed desktop scan-out.'
) 'Display adapter + Hardware adapter + Target + Copy policy'

Write-StackDiagram 'architecture-dxgi' 'DXGI architecture and presentation path' 'DXGI connects Direct3D resources to adapters, outputs, swap chains, composition surfaces, and the Windows present scheduler.' @(
    (New-Layer 'MAG graphics backend' 'D3D11, D3D12, capture bridge, or presentation host requests an adapter-compatible frame.' 'mag'),
    (New-Layer 'DXGI factory, adapter, and output' 'Enumerates hardware, maps monitors to outputs, negotiates formats, and exposes presentation capabilities.' 'api'),
    (New-Layer 'DXGI resources and swap chain' 'Back buffers, shared surfaces, buffer count, flip/copy effect, scaling, alpha, waitable latency, and tearing flags.' 'runtime'),
    (New-Layer 'Present / Present1 / composition submission' 'Queues the current buffer and metadata to redirected HWND, DirectComposition, or a presentation surface.' 'runtime'),
    (New-Layer 'DWM and Windows present scheduler' 'Chooses composed, hardware-composed, or independent presentation according to visibility and platform conditions.' 'composition'),
    (New-Layer 'WDDM driver and display engine' 'Schedules GPU work and scans out the final plane or composed desktop image.' 'output')
) @(
    'Flip models rotate buffer identities.',
    'ResizeBuffers changes buffer resources.',
    'Observed target can differ from requested.',
    'Exact-target mode rejects that mismatch.'
) 'Target + Buffers + Latency + Sync + Allow tearing + adapters'

Write-StackDiagram 'architecture-directcomposition' 'DirectComposition and DWM stack' 'DirectComposition is a retained, asynchronous composition API whose batches are consumed by the session-wide Desktop Window Manager.' @(
    (New-Layer 'MAG visual and surface updates' 'Creates app-owned visuals, attaches content, changes transforms/clips, and batches related geometry.' 'mag'),
    (New-Layer 'dcomp.dll application library' 'COM visual-tree API; Commit publishes a batch of retained property changes.' 'api'),
    (New-Layer 'win32k.sys composition object database' 'Marshals retained objects and committed batches across the application/DWM process boundary.' 'kernel'),
    (New-Layer 'dwm.exe / dwmcore.dll composition engine' 'Combines application trees with DWM window visuals and evaluates the desktop once per composition frame.' 'composition'),
    (New-Layer 'Shared Direct3D composition device' 'Composes surfaces, transforms, clips, effects, overlays, and occlusion-aware content on the GPU.' 'gpu'),
    (New-Layer 'WDDM present and scan-out' 'Submits the final desktop or eligible hardware planes to the display engine.' 'output')
) @(
    'Visual properties are retained.',
    'Commit publishes one atomic batch.',
    'No-redirection content needs a host.',
    'MAG keeps resize-capacity reservoirs.'
) 'Host: DirectComposition; Surface: No redirection or Auto'

Write-StackDiagram 'architecture-d3d9' 'Direct3D 9 renderer stack' 'MAG uses the native Direct3D 9/9Ex runtime and a windowed presentation path; D3D9 does not expose DXGI swap-chain APIs.' @(
    (New-Layer 'MAG Direct3D 9 backend' 'Uploads/composes the captured BGRA frame, overlay primitives, and selected text output.' 'mag'),
    (New-Layer 'd3d9.dll and IDirect3DDevice9Ex' 'Device state, textures, shaders/fixed-function operations, back buffers, reset, and PresentEx.' 'api'),
    (New-Layer 'Vendor Direct3D 9 user-mode driver' 'Translates the D3D9 command stream for the selected adapter under WDDM.' 'driver'),
    (New-Layer 'dxgkrnl / scheduler / memory manager' 'Schedules GPU contexts and manages residency, synchronization, and windowed presentation.' 'kernel'),
    (New-Layer 'GPU render target and DWM surface' 'The rendered back buffer is presented to the HWND and composed or promoted by Windows.' 'gpu'),
    (New-Layer 'Display engine and monitor' 'DWM composition or an eligible hardware path reaches scan-out.' 'output')
) @(
    'Legacy API; separate from DXGI.',
    '9Ex integrates with WDDM/DWM.',
    'Reset/lost-device rules still matter.',
    'MAG validates exact target support.'
) 'Graphics API: Direct3D 9'

Write-StackDiagram 'architecture-d3d11' 'Direct3D 11 renderer stack' 'D3D11 provides device/context resource management while DXGI owns adapter/output discovery and the swap-chain presentation contract.' @(
    (New-Layer 'MAG Direct3D 11 backend' 'Composes capture, overlay, and text into a BGRA render target on the selected adapter.' 'mag'),
    (New-Layer 'd3d11.dll device and context' 'Resources, views, shaders, pipeline state, copies, immediate execution, and optional deferred command recording.' 'api'),
    (New-Layer 'DXGI device, surfaces, and swap chain' 'Adapter identity, shared textures, flip-model buffers, composition targets, latency, tearing, and Present.' 'runtime'),
    (New-Layer 'Vendor D3D11 user-mode driver' 'Builds and submits hardware command buffers through WDDM.' 'driver'),
    (New-Layer 'Windows graphics kernel and GPU' 'Schedules contexts, maintains residency, executes render/copy work, and tracks synchronization.' 'kernel'),
    (New-Layer 'DWM / presentation manager / scan-out' 'Consumes the submitted surface through the selected host and presentation model.' 'output')
) @(
    'Best-integrated MAG GPU path.',
    'BGRA enables D2D interoperation.',
    'Shared surfaces avoid CPU copies.',
    'Adapters must remain compatible.'
) 'Graphics API: Direct3D 11'

Write-StackDiagram 'architecture-d3d12' 'Direct3D 12 renderer stack' 'D3D12 makes command recording, resource states, synchronization, and frames-in-flight explicit; DXGI still supplies swap chains.' @(
    (New-Layer 'MAG Direct3D 12 backend' 'Records capture composition, overlay, and text work while retaining frame resources by buffer index.' 'mag'),
    (New-Layer 'D3D12 device, resources, and descriptors' 'Explicit heaps, states, render targets, upload resources, barriers, root signatures, and pipeline objects.' 'api'),
    (New-Layer 'Command allocators, lists, queue, and fences' 'The application records work, submits ordered lists to a direct queue, and limits frames in flight with fences.' 'runtime'),
    (New-Layer 'DXGI flip-model swap chain' 'Back-buffer rotation is explicit; Present runs on the direct queue supplied at swap-chain creation.' 'runtime'),
    (New-Layer 'Vendor D3D12 UMD and WDDM kernel' 'Translates and schedules explicit GPU work, memory residency, synchronization, and presentation.' 'driver'),
    (New-Layer 'DWM / presentation manager / display engine' 'Composes or promotes the buffer and completes scan-out on the selected output.' 'output')
) @(
    'Explicit back-buffer index.',
    'Present state is mandatory.',
    'Fence-based frame limiting matters.',
    'Resize recreates buffer resources only.'
) 'Graphics API: Direct3D 12'

Write-StackDiagram 'architecture-direct2d' 'Direct2D renderer stack' 'Direct2D is a hardware-accelerated immediate-mode 2D API layered over Direct3D, with a software fallback when hardware rendering is unavailable.' @(
    (New-Layer 'MAG Direct2D UI renderer' 'Emits minimap fills, strokes, controls, and compatible DirectWrite text into the active overlay target.' 'mag'),
    (New-Layer 'd2d1.dll factory, device, and device context' 'Transforms, geometry, brushes, bitmaps, clipping, antialiasing, effects, and DrawGlyphRun.' 'api'),
    (New-Layer 'DirectWrite and WIC interoperation' 'Text layout/glyph analysis and image decoding can feed the same Direct2D target.' 'runtime'),
    (New-Layer 'Direct3D 11.1 / DXGI surface' 'On modern Windows, Direct2D records GPU work against a DXGI-compatible Direct3D resource.' 'runtime'),
    (New-Layer 'D3D user-mode driver and WDDM' 'Executes accelerated 2D operations or uses the Direct2D software rasterizer when required.' 'driver'),
    (New-Layer 'MAG presentation host and DWM' 'The completed overlay surface joins the selected renderer/presentation path.' 'output')
) @(
    'UI selection is independent.',
    'DirectWrite is the text partner.',
    'BGRA/DXGI enables interoperation.',
    'Software fallback preserves availability.'
) 'UI API: Direct2D'

Write-StackDiagram 'architecture-directwrite' 'DirectWrite text stack' 'DirectWrite separates font discovery, shaping, layout, hit testing, glyph analysis, and rasterization from the API that finally draws the glyphs.' @(
    (New-Layer 'MAG text and layout requests' 'Overlay labels and values are measured once and reused where geometry/text has not changed.' 'mag'),
    (New-Layer 'dwrite.dll factory and font system' 'Font collections, fallback, font faces, OpenType tables, localized properties, and system font data.' 'api'),
    (New-Layer 'Text format, analyzer, and layout' 'Unicode shaping, bidi, line breaking, glyph runs, metrics, clipping, and pixel snapping.' 'runtime'),
    (New-Layer 'Glyph rendering choice' 'Direct2D DrawText/DrawGlyphRun for GPU surfaces, or DirectWrite bitmap targets for GDI-compatible memory.' 'runtime'),
    (New-Layer 'Direct2D/D3D or GDI target' 'Antialiased glyph coverage is blended into the active MAG overlay/render target.' 'driver'),
    (New-Layer 'Presentation host and DWM' 'Text travels with the same frame and atomic presentation update as the magnified content.' 'output')
) @(
    'Layout and drawing are separate.',
    'Glyph positions can be cached.',
    'GPU and bitmap render targets exist.',
    'MAG exposes text choice separately.'
) 'Text renderer: DirectWrite'

Write-StackDiagram 'architecture-opengl' 'OpenGL on Windows stack' 'The Win32 WGL layer binds an OpenGL context to a window device context; opengl32.dll dispatches supported work to the vendor installable client driver.' @(
    (New-Layer 'MAG OpenGL backend' 'Uploads/composes the capture texture, draws overlay geometry and glyph-atlas text, and swaps the window buffers.' 'mag'),
    (New-Layer 'Win32 HDC, pixel format, and WGL context' 'Choose/SetPixelFormat, create and make current a rendering context, and resolve extension entry points.' 'api'),
    (New-Layer 'opengl32.dll runtime' 'Exports the Windows OpenGL/WGL ABI and dispatches commands for the current context.' 'runtime'),
    (New-Layer 'Vendor OpenGL ICD' 'Implements modern OpenGL and WGL extensions, translating calls to hardware work under WDDM.' 'driver'),
    (New-Layer 'WDDM kernel and GPU' 'Schedules the ICD command stream, manages memory, synchronization, and the window surface.' 'kernel'),
    (New-Layer 'SwapBuffers, DWM, and monitor' 'The double-buffered window surface is handed to Windows composition and display.' 'output')
) @(
    'No DXGI API in the MAG GL path.',
    'Pixel format is set once per HWND.',
    'The ICD comes from the GPU vendor.',
    'Swap interval controls pacing.'
) 'Graphics API: OpenGL; Text: OpenGL glyph atlas or another choice'

Write-StackDiagram 'architecture-vulkan' 'Vulkan on Windows stack' 'Vulkan uses an explicit loader/layer/driver chain and Win32 WSI extensions to connect VkImage rendering to a window-system swap chain.' @(
    (New-Layer 'MAG Vulkan backend' 'Selects a physical device, creates explicit resources, records command buffers, and synchronizes capture composition.' 'mag'),
    (New-Layer 'vulkan-1.dll loader' 'Discovers drivers, builds dispatch tables, exposes core/Win32 WSI entry points, and routes calls by Vulkan object.' 'api'),
    (New-Layer 'Optional Vulkan layers' 'Validation, tracing, profiling, and overlays can intercept calls during development; release paths need not enable them.' 'runtime'),
    (New-Layer 'Vendor Vulkan ICD' 'Implements Vulkan for one or more physical devices and translates explicit queues/resources to hardware work.' 'driver'),
    (New-Layer 'VK_KHR_win32_surface and swapchain' 'The HWND-backed VkSurfaceKHR supplies images that vkQueuePresentKHR submits to the Windows presentation path.' 'runtime'),
    (New-Layer 'WDDM, GPU, DWM, and display' 'Executes queues, tracks residency and synchronization, then composes or scans out the presented image.' 'output')
) @(
    'Loader supports multiple ICDs.',
    'Layers are optional and ordered.',
    'Physical device maps to adapter.',
    'WSI is separate from core Vulkan.'
) 'Graphics API: Vulkan; Hardware adapter selects the device'

Write-StackDiagram 'architecture-gdi' 'GDI window graphics stack' 'MAG uses a memory DIB and classic GDI operations; the CPU-owned result reaches the HWND through GDI/User32 and the DWM redirection surface.' @(
    (New-Layer 'MAG GDI backend' 'Captures or consumes BGRA pixels, scales into a DIB section, and draws overlay/text using an HDC.' 'mag'),
    (New-Layer 'gdi32.dll API' 'BitBlt, StretchBlt, brushes, pens, fonts, device contexts, regions, and DIB transfers.' 'api'),
    (New-Layer 'User-mode batching and win32k' 'GDI calls are batched/thunked to the window manager and kernel graphics subsystem as required.' 'runtime'),
    (New-Layer 'HWND redirection or layered bitmap' 'Ordinary windows update DWM-owned redirected content; layered windows use the explicit User32 bitmap path.' 'composition'),
    (New-Layer 'DWM composition under WDDM' 'The window surface is combined with the rest of the desktop; some operations remain CPU-bound.' 'kernel'),
    (New-Layer 'Display engine and monitor' 'The composed desktop is scanned out on the selected output.' 'output')
) @(
    'Predictable CPU-owned frame path.',
    'GDI BitBlt is also a capture API.',
    'Layered hosts require redirection.',
    'CPU routes cannot be strict zero-copy.'
) 'Graphics API: GDI; Capture API: GDI BitBlt when selected'

Write-StackDiagram 'architecture-gdiplus' 'MAG GDI+ flat C graphics stack' 'MAG calls the Gdiplus.dll flat function ABI directly from C and keeps its reservoir-sized drawing resources alive across ordinary resize epochs.' @(
    (New-Layer 'MAG GDI+ backend in C' 'Retained source bitmap, target DIB bitmap, graphics context, and reusable brush; GdiplusStartup and Gdip* calls only.' 'mag'),
    (New-Layer 'Gdiplus.dll flat API' 'Native flat functions implement the service beneath the documented C++ wrapper classes.' 'api'),
    (New-Layer 'Retained CPU raster target' 'Nearest-neighbor image draw plus overlay fills produce one completed BGRA frame without per-resize object recreation.' 'runtime'),
    (New-Layer 'Redirected HWND or PM bridge' 'One final BitBlt, or one D3D11 upload to Presentation Manager on the selected adapter.' 'composition'),
    (New-Layer 'WDDM display path and monitor' 'DWM composes the window with the desktop for scan-out.' 'output')
) @(
    'Pure C: no wrapper classes.',
    'Resources survive ordinary resize.',
    'CPU path; never strict zero-copy.',
    'Selectable beside every backend.'
) 'Graphics API: GDI+ (flat C ABI)'
