param(
    [Parameter(Mandatory = $true)]
    [string]$VcpkgTriplet
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
Push-Location $repoRoot
try {
    vcpkg install --triplet $VcpkgTriplet
}
finally {
    Pop-Location
}
