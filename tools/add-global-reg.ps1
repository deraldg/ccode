# add-global-reg.ps1
Set-StrictMode -Version Latest
$hdr = "include\command_registry.hpp"
if (-not (Test-Path $hdr)) { throw "Can't find $hdr" }

# Only add once
$hasReg = Select-String -Path $hdr -Pattern 'inline\s+CommandRegistry\s+reg' -SimpleMatch -ErrorAction SilentlyContinue
if (-not $hasReg) {
@'
namespace cli {
  // Global registry instance expected by legacy call sites (e.g., cli::reg.add(...))
  inline CommandRegistry reg{};
}
'@ | Add-Content $hdr
  Write-Host "Added inline global cli::reg to $hdr"
} else {
  Write-Host "cli::reg already present in $hdr"
}

# Clean + rebuild
if (Test-Path build) { Remove-Item -Recurse -Force build }
cmake -S . -B build
cmake --build build --config Release --target dottalkpp
