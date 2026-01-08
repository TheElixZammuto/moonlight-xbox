$ErrorActionPreference = "Stop"
$script:Failures = @()

# ============================
# CONFIGURATION
# ============================

# vcpkg
$Triplet = "x64-uwp"
$VcpkgManifestMode = "on"

# SDK / Toolchain
$WindowsSDK = "10.0"
$ForceGenerator = $null   # Default: "Visual Studio 17 2022"

# Certificate
$CertSubject = "CN=MoonlightXbox"
$CertPassword = "moonlight"

function Step {
    param([string]$Message)
    Write-Host "`n=== $Message ===" -ForegroundColor Cyan
}

function Record-Failure {
    param([string]$Message)
    $script:Failures += $Message
    Write-Host "❌ $Message" -ForegroundColor Red
}

function Try-Step {
    param(
        [string]$Name,
        [scriptblock]$Action
    )

    Step $Name

    try {
        & $Action
    }
    catch {
        Record-Failure "$Name failed: $($_.Exception.Message)"
    }
}

# --- VISUAL STUDIO DETECTION --------------------------------------------------

function Detect-VS {
    Step "Detecting Visual Studio installation"

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        Record-Failure "vswhere.exe not found — Visual Studio Installer missing"
        return $null
    }

    $vsJson = & $vswhere -latest -format json | ConvertFrom-Json
    if (-not $vsJson) {
        Record-Failure "No Visual Studio installation detected"
        return $null
    }

    $installPath = $vsJson.installationPath
    $version     = $vsJson.catalog.productLineVersion

    Write-Host "Found VS at: $installPath (version $version)" -ForegroundColor Green

    # Required components
    $required = @(
        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",    # Desktop C++
        "Microsoft.VisualStudio.ComponentGroup.UWP.VC",         # UWP C++
        "Microsoft.VisualStudio.Component.Windows10SDK.19041"   # Windows 10 SDK
    )

    $missing = @()

    foreach ($comp in $required) {
        $found = & $vswhere -latest -requires $comp -property installationPath
        if (-not $found) {
            $missing += $comp
        }
    }

    if ($missing.Count -gt 0) {
        Write-Host "`n❌ Missing required Visual Studio components:" -ForegroundColor Red
        foreach ($m in $missing) {
            Write-Host "   - $m" -ForegroundColor Red
        }

        Write-Host "`nTo fix this, open the Visual Studio Installer and enable:" -ForegroundColor Yellow
        Write-Host "  • Universal Windows Platform development" -ForegroundColor Yellow
        Write-Host "  • Desktop development with C++" -ForegroundColor Yellow
        Write-Host "  • Windows 10 SDK (10.0.19041 or newer)" -ForegroundColor Yellow

        Record-Failure "Required Visual Studio components missing"
        return $null
    }

    if ($ForceGenerator) {
        Write-Host "`n⚙ Overriding generator: $ForceGenerator" -ForegroundColor Yellow
        return $ForceGenerator
    }

    switch ($version) {

        "2019" {
            Write-Host "Using Visual Studio 2019 generator" -ForegroundColor Yellow
            return "Visual Studio 16 2019"
        }

        "2022" {
            Write-Host "Using Visual Studio 2022 generator" -ForegroundColor Yellow
            return "Visual Studio 17 2022"
        }

        "2025" {
            Write-Host "`n⚠ Visual Studio 2025 detected" -ForegroundColor Yellow
            Write-Host "   CMake does not yet provide a VS2025 generator." -ForegroundColor Yellow
            Write-Host "   This is normal — the MSVC toolchain is fully compatible." -ForegroundColor Yellow
            Write-Host "   Using the VS2022 generator to target the VS2025 toolchain." -ForegroundColor Yellow
            return "Visual Studio 17 2022"
        }

        default {
            Write-Host "`n⚠ Visual Studio version $version detected" -ForegroundColor Yellow
            Write-Host "   CMake does not provide a generator for this version." -ForegroundColor Yellow
            Write-Host "   Falling back to the VS2022 generator." -ForegroundColor Yellow
            return "Visual Studio 17 2022"
        }
    }
}

$CMakeGenerator = Detect-VS
if ($CMakeGenerator) {
    Write-Host "Using CMake generator: $CMakeGenerator" -ForegroundColor Yellow
}

# --- STEP 1: Validate environment ---------------------------------------------

Try-Step "Validating environment" {
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        throw "Git is not installed or not in PATH"
    }

    if (-not (Test-Path ".\vcpkg")) {
        throw "vcpkg directory not found — did you clone with submodules?"
    }
}

# --- STEP 2: Update submodules ------------------------------------------------

Try-Step "Updating git submodules" {
    git submodule update --init --recursive
    if ($LASTEXITCODE -ne 0) { throw "git submodule update failed" }
}

# --- STEP 3: Bootstrap vcpkg --------------------------------------------------

Try-Step "Bootstrapping vcpkg" {
    & .\vcpkg\bootstrap-vcpkg.bat
    if ($LASTEXITCODE -ne 0) { throw "vcpkg bootstrap failed" }
}

# --- STEP 4: Install vcpkg packages ------------------------------------------

Try-Step "Installing vcpkg packages for $Triplet" {
    & .\vcpkg\vcpkg.exe install --triplet $Triplet --clean-after-build
    if ($LASTEXITCODE -ne 0) { throw "vcpkg install failed" }
}

# --- STEP 5: Generate third‑party projects ------------------------------------

Try-Step "Configuring moonlight-common-c" {
    cmake -B "third_party/moonlight-common-c/build" `
          -S "third_party/moonlight-common-c" `
          -DCMAKE_TOOLCHAIN_FILE="$PSScriptRoot/vcpkg/scripts/buildsystems/vcpkg.cmake" `
          -DVCPKG_TARGET_TRIPLET="$Triplet" `
          -G "$CMakeGenerator" `
          -DCMAKE_SYSTEM_NAME="WindowsStore" `
          -DCMAKE_SYSTEM_VERSION="$WindowsSDK" `
          -DTARGET_UWP=ON `
          -DVCPKG_MANIFEST_MODE=$VcpkgManifestMode `
          -DVCPKG_MANIFEST_DIR="."
    if ($LASTEXITCODE -ne 0) { throw "moonlight-common-c CMake configuration failed" }
}

Try-Step "Configuring libgamestream" {
    cmake -B "libgamestream/build" `
          -S "libgamestream" `
          -DCMAKE_TOOLCHAIN_FILE="$PSScriptRoot/vcpkg/scripts/buildsystems/vcpkg.cmake" `
          -DVCPKG_TARGET_TRIPLET="$Triplet" `
          -G "$CMakeGenerator" `
          -DCMAKE_SYSTEM_NAME="WindowsStore" `
          -DCMAKE_SYSTEM_VERSION="$WindowsSDK" `
          -DTARGET_UWP=ON `
          -DBUILD_SHARED_LIBS=off `
          -DVCPKG_MANIFEST_MODE=$VcpkgManifestMode `
          -DVCPKG_MANIFEST_DIR="."
    if ($LASTEXITCODE -ne 0) { throw "libgamestream CMake configuration failed" }
}

# --- STEP 6: Refresh Visual Studio project cache ------------------------------

Try-Step "Refreshing Visual Studio project cache" {

    $vsRunning = Get-Process devenv -ErrorAction SilentlyContinue

    if ($vsRunning) {
        Write-Host "⚠ Visual Studio is currently open — project reload will not take effect until you close and reopen the solution." -ForegroundColor Yellow

        # Touch the solution file so VS reloads when reopened
        $sln = Get-ChildItem -Filter *.sln | Select-Object -First 1
        if ($sln) {
            (Get-Item $sln.FullName).LastWriteTime = Get-Date
        }
    }
    else {
        # Safe to delete .vs folder
        Remove-Item -Recurse -Force ".vs" -ErrorAction Ignore
    }
}

# --- STEP 7: Generate UWP signing certificate ---------------------------------

Try-Step "Generating UWP signing certificate" {
    $cert = New-SelfSignedCertificate `
        -Type CodeSigningCert `
        -Subject $CertSubject `
        -CertStoreLocation "Cert:\CurrentUser\My"

    $pwd = ConvertTo-SecureString $CertPassword -AsPlainText -Force

    Export-PfxCertificate `
        -Cert $cert `
        -FilePath "$PSScriptRoot\SigningCert.pfx" `
        -Password $pwd
}

# --- STEP 8: Patch vcxproj ---------------------------------------------------

Try-Step "Updating .vcxproj with certificate info" {
    $proj = Get-ChildItem -Recurse -Filter *.vcxproj | Select-Object -First 1
    if (-not $proj) { throw "Could not find .vcxproj" }

    [xml]$xml = Get-Content $proj.FullName

    $nsmgr = New-Object System.Xml.XmlNamespaceManager($xml.NameTable)
    $nsmgr.AddNamespace("msb", "http://schemas.microsoft.com/developer/msbuild/2003")

    $pg = $xml.SelectSingleNode("//msb:PropertyGroup[@Label='UserMacros']", $nsmgr)
    if (-not $pg) { throw "Could not find UserMacros PropertyGroup" }

    $thumbNode = $pg.SelectSingleNode("msb:PackageCertificateThumbprint", $nsmgr)
    if (-not $thumbNode) {
        $thumbNode = $xml.CreateElement("PackageCertificateThumbprint", $nsmgr.LookupNamespace("msb"))
        $pg.AppendChild($thumbNode) | Out-Null
    }
    $thumbNode.InnerText = $cert.Thumbprint

    $keyFileNode = $pg.SelectSingleNode("msb:PackageCertificateKeyFile", $nsmgr)
    if (-not $keyFileNode) {
        $keyFileNode = $xml.CreateElement("PackageCertificateKeyFile", $nsmgr.LookupNamespace("msb"))
        $pg.AppendChild($keyFileNode) | Out-Null
    }
    $keyFileNode.InnerText = "SigningCert.pfx"

    $pwdNode = $pg.SelectSingleNode("msb:PackageCertificatePassword", $nsmgr)
    if (-not $pwdNode) {
        $pwdNode = $xml.CreateElement("PackageCertificatePassword", $nsmgr.LookupNamespace("msb"))
        $pg.AppendChild($pwdNode) | Out-Null
    }
    $pwdNode.InnerText = "moonlight"

    $xml.Save($proj.FullName)
}

# --- STEP 9: Patch appxmanifest ----------------------------------------------

Try-Step "Updating appxmanifest with certificate info" {
    $manifestPath = "$PSScriptRoot\Package.appxmanifest"
    if (Test-Path $manifestPath) {
        [xml]$manifest = Get-Content $manifestPath

        $manifest.Package.Identity.Publisher = "CN=MoonlightXbox"
        $manifest.Package.Properties.PublisherDisplayName = "MoonlightXbox"

        $manifest.Save($manifestPath)
    }
}

# --- SUMMARY ------------------------------------------------------------------

Write-Host "`n====================================================="
Write-Host "                 SETUP SUMMARY"
Write-Host "====================================================="

if ($script:Failures.Count -gt 0) {
    Write-Host "`n❌ The following errors occurred:" -ForegroundColor Red
    foreach ($f in $script:Failures) {
        Write-Host "   - $f" -ForegroundColor Red
    }
    Write-Host "`nSetup completed with errors." -ForegroundColor Red
    exit 1
}
else {
    Write-Host "`n🎉 All steps completed successfully!" -ForegroundColor Green
}
