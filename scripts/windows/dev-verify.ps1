<#
CHML verification gate - Windows.

Mirrors scripts/linux/dev-verify.sh. Verifies the production implementation with
the tools this host actually has. It never installs anything and never changes
the toolchain.

By default every required tool must be present (cmake, ctest, gcc, clang) and the
run reports FAILED if one is missing, because the gate requires cross-compiler
evidence. Use -AllowPartial for local iteration: missing tools are reported as
SKIPPED and the run ends INCOMPLETE rather than PASSED.

Exit codes: 0 = every available required step passed (and, without
-AllowPartial, nothing was skipped)
            1 = a step failed
            2 = required toolchain missing (strict mode)

.PARAMETER AllowPartial
Report missing tools as SKIPPED instead of failing immediately.
#>
[CmdletBinding()]
param([switch]$AllowPartial)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$BuildRoot = Join-Path $RepoRoot 'build\verify-win'
$LogDir = Join-Path $BuildRoot 'logs'
$StrictFlags = '-std=c11 -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wstrict-prototypes'

$script:Failed = $false
$script:Skipped = 0
$rows = New-Object System.Collections.ArrayList

function Record([string]$Name, [string]$Status, [string]$Detail) {
    $line = '{0,-40} {1,-8} {2}' -f $Name, $Status, $Detail
    [void]$rows.Add($line)
    Write-Host "  $line"
    if ($Status -eq 'FAIL') { $script:Failed = $true }
    if ($Status -eq 'SKIP') { $script:Skipped++ }
}
function Section([string]$Name) { Write-Host "`n== $Name ==" }

Section 'toolchain'
$required = @('cmake', 'ctest', 'gcc', 'clang')
$missing = @()
foreach ($tool in $required) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) { $missing += $tool }
}
foreach ($tool in @('cmake', 'ctest', 'gcc', 'clang')) {
    $cmd = Get-Command $tool -ErrorAction SilentlyContinue
    if ($cmd) {
        $v = (& $tool --version 2>&1 | Select-Object -First 1)
        Write-Host ("  {0,-8} {1}" -f $tool, $v)
    }
}
if ($missing.Count -gt 0) {
    Write-Host "  MISSING: $($missing -join ', ')" -ForegroundColor Yellow
    if (-not $AllowPartial) {
        Write-Host 'This script does not install dependencies. Re-run with -AllowPartial for a local subset.' -ForegroundColor Yellow
        exit 2
    }
    foreach ($tool in $missing) { Record "$tool available" SKIP 'not installed on this host' }
}

New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$GccBuild = Join-Path $BuildRoot 'gcc'

Section 'GCC strict'
Remove-Item -Recurse -Force $GccBuild -ErrorAction SilentlyContinue
$cfg = Join-Path $LogDir 'gcc.configure.log'
$bld = Join-Path $LogDir 'gcc.build.log'
$tst = Join-Path $LogDir 'gcc.ctest.log'
& cmake -S $RepoRoot -B $GccBuild -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="$StrictFlags" *> $cfg
if ($LASTEXITCODE -ne 0) {
    Record 'gcc configure' FAIL "see $cfg"
} else {
    & cmake --build $GccBuild *> $bld
    if ($LASTEXITCODE -ne 0) {
        Record 'gcc build' FAIL "see $bld"
    } else {
        Record 'gcc build' PASS 'strict warnings clean'
        & ctest --test-dir $GccBuild --output-on-failure *> $tst
        if ($LASTEXITCODE -ne 0) {
            Record 'gcc ctest' FAIL "see $tst"
        } else {
            $summary = (Select-String -Path $tst -Pattern 'tests passed' | Select-Object -Last 1)
            Record 'gcc ctest' PASS ($summary.Line.Trim())
        }
    }
}

Section 'replacement mechanism'
$IdBin = Join-Path $GccBuild 'chml_atomic_identity_test.exe'
$NegBin = Join-Path $GccBuild 'chml_atomic_identity_negative.exe'
if (Test-Path $IdBin) {
    $out = (& $IdBin 2>&1 | Out-String)
    $rc = $LASTEXITCODE
    $out.Trim() -split "`n" | ForEach-Object { Write-Host "    $_" }
    if ($rc -eq 0 -and $out -match 'replace backend: win32-movefileex') {
        Record 'atomic replacement executed' PASS 'identity changed, backend=win32-movefileex'
    } else {
        Record 'atomic replacement executed' FAIL "rc=$rc, backend line not observed"
    }
} else {
    Record 'atomic replacement executed' FAIL 'identity test binary missing'
}
if (Test-Path $NegBin) {
    $negOut = (& $NegBin 2>&1 | Out-String)
    $negRc = $LASTEXITCODE
    if ($negRc -eq 1 -and $negOut -match 'NEGATIVE_CONTROL_REASON=identity_unchanged') {
        Record 'negative control (in-place write)' PASS 'discriminated for the expected reason'
    } elseif ($negRc -ne 0) {
        Record 'negative control (in-place write)' FAIL "exited $negRc for the wrong reason"
    } else {
        Record 'negative control (in-place write)' FAIL 'in-place writer was not detected'
    }
} else {
    Record 'negative control (in-place write)' FAIL 'negative binary missing'
}

Section 'temporary file cleanup'
$residue = @(Get-ChildItem -Path $BuildRoot -Recurse -File -Filter '*.tmp*' -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -match '\.tmp\d+$' })
if ($residue.Count -eq 0) { Record 'temporary cleanup' PASS 'no *.tmpN residue' }
else { Record 'temporary cleanup' FAIL ("residue: " + (($residue | ForEach-Object { $_.Name }) -join ', ')) }

Section 'example output'
$ExDir = Join-Path $BuildRoot 'example'
Remove-Item -Recurse -Force $ExDir -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $ExDir | Out-Null
$Hello = Join-Path $GccBuild 'chml_hello.exe'
if (Test-Path $Hello) {
    Push-Location $ExDir
    try { & $Hello *> (Join-Path $LogDir 'example.log'); $exRc = $LASTEXITCODE } finally { Pop-Location }
    if ($exRc -eq 0 -and (Test-Path (Join-Path $ExDir 'index.html')) -and (Test-Path (Join-Path $ExDir 'style.css'))) {
        $html = Get-Content (Join-Path $ExDir 'index.html') -Raw
        $charsets = ([regex]::Matches($html, 'charset')).Count
        if ($charsets -eq 1) { Record 'example ran' PASS 'index.html + style.css, 1 charset declaration' }
        else { Record 'example ran' FAIL "charset declarations: $charsets" }
    } else {
        Record 'example ran' FAIL 'output files missing'
    }
} else {
    Record 'example ran' FAIL 'chml_hello binary missing'
}

Section 'static analysis'
$anaOk = $true
foreach ($src in @('src\document.c', 'src\style.c', 'src\chml_io.c', 'src\builder.c')) {
    $obj = Join-Path $env:TEMP 'chml-an.o'
    & gcc -std=c11 -fanalyzer -Wall -Wextra -Wpedantic -I (Join-Path $RepoRoot 'include') -I (Join-Path $RepoRoot 'src') -c (Join-Path $RepoRoot $src) -o $obj *> (Join-Path $LogDir ("analyzer-" + (Split-Path $src -Leaf) + '.log'))
    if ($LASTEXITCODE -ne 0) { $anaOk = $false }
}
if ($anaOk) { Record 'gcc -fanalyzer' PASS 'document.c, style.c, chml_io.c, builder.c' }
else { Record 'gcc -fanalyzer' FAIL "see $LogDir" }

Section 'sanitizers'
# MinGW does not ship the runtime libraries, so this is a probe, not an assumption.
# Native stderr becomes a terminating error under $ErrorActionPreference='Stop',
# so the probe runs with that temporarily relaxed.
$probe = Join-Path $env:TEMP 'chml-sanprobe.c'
Set-Content -Path $probe -Value 'int main(void){return 0;}' -NoNewline
$prevEap = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$sanOut = (& gcc -std=c11 '-fsanitize=address,undefined' $probe -o (Join-Path $env:TEMP 'chml-sanprobe.exe') 2>&1 | Out-String)
$sanRc = $LASTEXITCODE
$ErrorActionPreference = $prevEap
Set-Content -Path (Join-Path $LogDir 'sanitizer-probe.log') -Value $sanOut
if ($sanRc -eq 0) {
    Record 'asan/ubsan available' PASS 'toolchain links sanitizer runtimes'
} else {
    Record 'asan/ubsan available' SKIP 'MinGW has no libasan/libubsan; run the Linux gate'
}
Remove-Item $probe -ErrorAction SilentlyContinue

Section 'summary'
$rows | ForEach-Object { Write-Host $_ }
Write-Host ''
Write-Host 'Not claimed by this gate: crash durability, concurrency safety, browser conformance, performance.'
if ($script:Failed) { Write-Host 'CHML verification (Windows): FAILED' ; exit 1 }
if ($script:Skipped -gt 0) { Write-Host "CHML verification (Windows): INCOMPLETE - $($script:Skipped) step(s) skipped"; exit 1 }
Write-Host 'CHML verification (Windows): PASSED on this host'
exit 0
