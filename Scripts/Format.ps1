# Formats all Carbon sources with clang-format. Run before every commit.
#   Scripts/Format.ps1          formats in place
#   Scripts/Format.ps1 -Check   fails if any file is not formatted (used by CI)
param([switch]$Check)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$folders = 'Framework', 'Extensions', 'Reflection', 'Examples', 'Tests', 'Benchmarks' | ForEach-Object { Join-Path $root $_ }
$files = Get-ChildItem -Path $folders -Recurse -File -Include *.h, *.cpp | ForEach-Object { $_.FullName }

if (-not $files) {
    Write-Output 'No source files found.'
    exit 0
}

# In batches: all files at once are more than a Windows command line can hold.
$batchSize = 100
$exitCode = 0
for ($start = 0; $start -lt $files.Count; $start += $batchSize) {
    $batch = $files[$start..([Math]::Min($start + $batchSize, $files.Count) - 1)]
    if ($Check) {
        & clang-format --dry-run --Werror $batch
    } else {
        & clang-format -i $batch
    }
    if ($LASTEXITCODE -ne 0) {
        $exitCode = $LASTEXITCODE
    }
}

exit $exitCode
