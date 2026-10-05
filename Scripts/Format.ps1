# Formats all Carbon sources with clang-format. Run before every commit.
#   Scripts/Format.ps1          formats in place
#   Scripts/Format.ps1 -Check   fails if any file is not formatted (used by CI)
param([switch]$Check)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$folders = 'Framework', 'Extensions', 'Reflection', 'Examples', 'Tests' | ForEach-Object { Join-Path $root $_ }
$files = Get-ChildItem -Path $folders -Recurse -File -Include *.h, *.cpp | ForEach-Object { $_.FullName }

if (-not $files) {
    Write-Output 'No source files found.'
    exit 0
}

if ($Check) {
    & clang-format --dry-run --Werror $files
} else {
    & clang-format -i $files
}

exit $LASTEXITCODE
