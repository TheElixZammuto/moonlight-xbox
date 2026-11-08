<#
.SYNOPSIS
    Apply the w10m-port changes to the upstream submodules.

.DESCRIPTION
    The published repository keeps submodule gitlinks at their upstream commits.
    The w10m changes are stored as patches under patches\ and applied with this
    script. It is idempotent: already-applied patches are skipped.

.PARAMETER Check
    Only verify whether the patches are applicable/already applied; do not modify
    the submodule working trees.
#>
[CmdletBinding()]
param(
    [switch]$Check
)

$ErrorActionPreference = "Stop"
$Root = $PSScriptRoot

$Patches = @(
    @{ Path = "third_party\moonlight-common-c"; Patch = "patches\moonlight-common-c.patch" },
    @{ Path = "vcpkg";                         Patch = "patches\vcpkg.patch" },
    @{ Path = "third_party\DirectXTK";         Patch = "patches\DirectXTK.patch" },
    @{ Path = "third_party\tracy-xbox";        Patch = "patches\tracy-xbox.patch" }
)

function Invoke-Git {
    param(
        [string]$Repository,
        [string[]]$Arguments
    )

    $oldPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        & git -C $Repository @Arguments 2>$null | Out-Null
        return $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $oldPreference
    }
}

foreach ($item in $Patches) {
    $submodule = Join-Path $Root $item.Path
    $patch = Join-Path $Root $item.Patch

    if (-not (Test-Path $submodule)) {
        throw "Submodule path not found: $($item.Path). Run 'git submodule update --init --recursive' first."
    }
    if (-not (Test-Path $patch)) {
        throw "Patch not found: $($item.Patch)"
    }

    $inside = Invoke-Git $submodule @("rev-parse", "--is-inside-work-tree") 2>$null
    if ($inside -ne 0) {
        throw "Not a git worktree: $($item.Path). Initialize submodules first."
    }

    # If the reverse check succeeds, the patch is already applied.
    $reverse = Invoke-Git $submodule @("apply", "--reverse", "--check", $patch) 2>$null
    if ($reverse -eq 0) {
        Write-Host "[skip]   $($item.Path) (already applied)"
        continue
    }

    if ($Check) {
        $forward = Invoke-Git $submodule @("apply", "--check", $patch) 2>$null
        if ($forward -ne 0) {
            throw "Patch does not apply cleanly: $($item.Patch)"
        }
        Write-Host "[ok]     $($item.Path) (patch applicable)"
        continue
    }

    $forward = Invoke-Git $submodule @("apply", "--check", $patch)
    if ($forward -ne 0) {
        throw "Patch does not apply cleanly: $($item.Patch). Reset the submodule to its recorded commit first."
    }

    $apply = Invoke-Git $submodule @("apply", $patch)
    if ($apply -ne 0) {
        throw "Failed to apply patch: $($item.Patch)"
    }

    Write-Host "[apply]  $($item.Path)"
}

Write-Host "Submodule patches are applied."
exit 0
