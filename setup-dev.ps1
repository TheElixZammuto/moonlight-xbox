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
$CertFileName = "moonlight-xbox-dx_TemporaryKey.pfx"
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

function Run-IfNeeded {
    param(
        [string]$Name,
        [scriptblock]$IsNeeded,
        [scriptblock]$Action
    )

    Step $Name

    try {
        $needed = & $IsNeeded
    }
    catch {
        # If the check fails for any reason, be conservative and run the step.
        $needed = $true
    }

    if (-not $needed) {
        Write-Host "Skipping — step not necessary." -ForegroundColor Yellow
        return
    }

    try {
        & $Action
    }
    catch {
        Record-Failure "$Name failed: $($_.Exception.Message)"
    }
}

# Simple yes/no prompt. Returns $true for yes, $false for no.
function Ask-YesNo {
    param(
        [string]$Question,
        [bool]$DefaultYes = $true
    )

    $yesNo = if ($DefaultYes) { "Y/n" } else { "y/N" }
    while ($true) {
        $resp = Read-Host "$Question [$yesNo]"
        if ([string]::IsNullOrWhiteSpace($resp)) { return $DefaultYes }
        switch ($resp.ToLower()) {
            'y' { return $true }
            'yes' { return $true }
            'n' { return $false }
            'no' { return $false }
            default { Write-Host "Please answer 'y' or 'n'." -ForegroundColor Yellow }
        }
    }
}

$script:RefreshCert = $false
$script:NeedsVSRefresh = $false

# --- VISUAL STUDIO DETECTION --------------------------------------------------

function Detect-VS {
    Step "Detecting Visual Studio installation"

    $programFilesX86 = [System.Environment]::GetEnvironmentVariable("ProgramFiles(x86)")
    $vswhere = Join-Path $programFilesX86 "Microsoft Visual Studio\Installer\vswhere.exe"
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

        "2026" {
            Write-Host "`n⚠ Visual Studio 2026 detected" -ForegroundColor Yellow
            Write-Host "   CMake does not yet provide a VS2026 generator." -ForegroundColor Yellow
            Write-Host "   This is normal — the MSVC toolchain is fully compatible." -ForegroundColor Yellow
            Write-Host "   Using the VS2022 generator to target the VS2026 toolchain." -ForegroundColor Yellow
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

Run-IfNeeded "Updating git submodules" {
    # Needed if any submodule is uninitialized or needs updating.
    $out = git submodule status --recursive 2>&1
    if ($LASTEXITCODE -ne 0) { return $true }
    if (-not $out) { return $false }
    if ($out -match '^-|^\+') { return $true }
    return $false
} {
    git submodule update --init --recursive
    if ($LASTEXITCODE -ne 0) { throw "git submodule update failed" }
}

# --- STEP 3: Bootstrap vcpkg --------------------------------------------------

Run-IfNeeded "Bootstrapping vcpkg" {
    return -not (Test-Path ".\vcpkg\vcpkg.exe")
} {
    & .\vcpkg\bootstrap-vcpkg.bat
    if ($LASTEXITCODE -ne 0) { throw "vcpkg bootstrap failed" }
}

# --- STEP 4: Install vcpkg packages ------------------------------------------

Run-IfNeeded "Installing vcpkg packages for $Triplet" {
    $installed1 = Test-Path ".\vcpkg_installed\$Triplet"
    $installed2 = Test-Path ".\vcpkg\installed\$Triplet"
    return -not ($installed1 -or $installed2)
} {
    & .\vcpkg\vcpkg.exe install --triplet $Triplet --clean-after-build
    if ($LASTEXITCODE -ne 0) { throw "vcpkg install failed" }
}

# --- STEP 5: Generate third‑party projects ------------------------------------

Run-IfNeeded "Configuring moonlight-common-c" {
    return -not (Test-Path "third_party/moonlight-common-c/build/CMakeCache.txt")
} {
    $script:NeedsVSRefresh = $true
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

Run-IfNeeded "Configuring libgamestream" {
    return -not (Test-Path "libgamestream/build/CMakeCache.txt")
} {
    $script:NeedsVSRefresh = $true
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

Run-IfNeeded "Refreshing Visual Studio project cache" {
    $vsRunning = Get-Process devenv -ErrorAction SilentlyContinue
    if (-not $script:NeedsVSRefresh) { 
        return $false
    }
    return (Test-Path ".vs") -or $vsRunning
} {
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

Run-IfNeeded "Generating UWP signing certificate" {
    $hasPfx = Test-Path "$PSScriptRoot\$CertFileName"
    $hasCert = Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -eq $CertSubject }

    if ($hasPfx -or $hasCert) {
        # Ask user whether they want to refresh the existing cert
        if (Ask-YesNo "A signing certificate already exists. Refresh it?" $false) {
            $script:RefreshCert = $true
            return $true
        }
        return $false
    }

    return $true
} {
    $cert = New-SelfSignedCertificate `
        -Type CodeSigningCert `
        -Subject $CertSubject `
        -CertStoreLocation "Cert:\CurrentUser\My"

    $securePwd = ConvertTo-SecureString $CertPassword -AsPlainText -Force

    Export-PfxCertificate `
        -Cert $cert `
        -FilePath "$PSScriptRoot\$CertFileName" `
        -Password $securePwd
}

# --- STEP 8: Patch vcxproj ---------------------------------------------------

Run-IfNeeded "Updating .vcxproj with certificate info" {
    if (-not $script:RefreshCert) { return $false }
    $proj = Get-ChildItem -Recurse -Filter *.vcxproj | Select-Object -First 1
    if (-not $proj) { return $true }
    $content = Get-Content $proj.FullName -Raw
    if ($content -match '<PackageCertificateKeyFile>$CertFileName</PackageCertificateKeyFile>' -and $content -match '<PackageCertificatePassword>moonlight</PackageCertificatePassword>') {
        return $false
    }
    return $true
} {
    $proj = Get-ChildItem -Recurse -Filter *.vcxproj | Select-Object -First 1
    if (-not $proj) { throw "Could not find .vcxproj" }

    $pfxPath = "$PSScriptRoot\$CertFileName"
    $projXmlRaw = Get-Content $proj.FullName -Raw
    $existingKeyFile = $null
    if ($projXmlRaw -match '<PackageCertificateKeyFile>([^<]+)</PackageCertificateKeyFile>') { $existingKeyFile = $matches[1] }
    if ($existingKeyFile) {
        $existingKeyFull = Join-Path $PSScriptRoot $existingKeyFile
        if (Test-Path $existingKeyFull) {
            try {
                $securePwd = ConvertTo-SecureString $CertPassword -AsPlainText -Force
                $imported = Import-PfxCertificate -FilePath $existingKeyFull -CertStoreLocation Cert:\CurrentUser\My -Password $securePwd
                if ($imported) {
                    Write-Host "Imported existing keyfile $existingKeyFile into certificate store" -ForegroundColor Green
                    $cert = $imported
                }
            }
            catch {
                Record-Failure "Failed to import existing keyfile ${existingKeyFile}: $($_.Exception.Message)"
            }
        }
    }
    
    $pfxCert = $null
    if (Test-Path $pfxPath) {
        try {
            $pfxCert = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2($pfxPath, $CertPassword)
        } catch { $pfxCert = $null }
    }

    $storeCert = Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -eq $CertSubject } | Select-Object -First 1

    if ($script:RefreshCert) {
        if (Test-Path $pfxPath) {
            $securePwd = ConvertTo-SecureString $CertPassword -AsPlainText -Force
            $cert = Import-PfxCertificate -FilePath $pfxPath -CertStoreLocation Cert:\CurrentUser\My -Password $securePwd
        }
        elseif (-not $storeCert) {
            throw "No certificate or PFX available to import"
        }
        else {
            $cert = $storeCert
        }
    }
    else {
        if ($projXmlRaw -match '<PackageCertificateThumbprint>([^<]+)</PackageCertificateThumbprint>') { $existingThumb = $matches[1] }
        if ($pfxCert -and $storeCert) {
            if ($pfxCert.Thumbprint -ne $storeCert.Thumbprint) {
                if (Ask-YesNo "PFX file exists but does not match the certificate in the store. Import the PFX and update the project?" $false) {
                    $securePwd = ConvertTo-SecureString $CertPassword -AsPlainText -Force
                    $cert = Import-PfxCertificate -FilePath $pfxPath -CertStoreLocation Cert:\CurrentUser\My -Password $securePwd
                }
                else {
                    Record-Failure "Skipped updating .vcxproj due to PFX/store mismatch"
                    return
                }
            }
            else {
                $cert = $storeCert
            }
        }
        elseif ($pfxCert -and -not $storeCert) {
            $securePwd = ConvertTo-SecureString $CertPassword -AsPlainText -Force
            $cert = Import-PfxCertificate -FilePath $pfxPath -CertStoreLocation Cert:\CurrentUser\My -Password $securePwd
        }
        elseif ($storeCert -and -not $pfxCert) {
            $cert = $storeCert
        }
        else {
            throw "No signing certificate found"
        }
    }

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
        if (Test-Path $pfxPath) {
            try { $pfxCert = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2($pfxPath, $CertPassword) } catch { $pfxCert = $null }
            if ($pfxCert -and ($pfxCert.Thumbprint -eq $cert.Thumbprint)) {
                $keyFileNode.InnerText = "$CertFileName"
            }
        else {
            $keyFileNode.InnerText = ""
        }
    }
    else {
        $keyFileNode.InnerText = ""
    }

    $pwdNode = $pg.SelectSingleNode("msb:PackageCertificatePassword", $nsmgr)
    if (-not $pwdNode) {
        $pwdNode = $xml.CreateElement("PackageCertificatePassword", $nsmgr.LookupNamespace("msb"))
        $pg.AppendChild($pwdNode) | Out-Null
    }
    $pwdNode.InnerText = "moonlight"

    $xml.Save($proj.FullName)
}

# --- STEP 9: Patch appxmanifest ----------------------------------------------

Run-IfNeeded "Updating appxmanifest with certificate info" {
    if (-not $script:RefreshCert) { return $false }
    $manifestPath = "$PSScriptRoot\Package.appxmanifest"
    if (-not (Test-Path $manifestPath)) { return $false }
    [xml]$manifest = Get-Content $manifestPath
    $pub = $manifest.Package.Identity.Publisher
    $display = $manifest.Package.Properties.PublisherDisplayName
    if (($pub -eq $CertSubject) -and ($display -eq 'MoonlightXbox')) { return $false }
    return $true
} {
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
