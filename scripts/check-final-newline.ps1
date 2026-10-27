<#
CHML final-newline check - Windows.

Mirrors scripts/check-final-newline.sh. Every tracked file whose name ends in
.c, .h, .md or .sh must end with a line feed (0x0A). A single rule covers both
repository conventions: an LF file ends with 0x0A and a CRLF file also ends with
0x0A. A file ending in a bare CR, in a space, or in any other byte is a
violation. Empty files are accepted: there is no last line to terminate.

CONTRACT - what is checked
  * Paths are enumerated from Git's index (`git ls-files -z`), never from a
    filesystem walk, so build output and untracked files cannot affect the
    result.
  * The bytes checked are the WORKING-TREE files at those paths. Committed blob
    contents are not read directly.
  * A violation introduced by an uncommitted working-tree edit is therefore
    reported, and a clean checkout can differ from the last commit only by
    line-ending conversion.
  * LF and CRLF both satisfy the rule, because both end with byte 0x0A.
  * A tracked file that is unreadable, or absent from the working tree, is a
    verification failure (exit 2), never a pass.

Filename decoding: PowerShell 5.1 decodes native-command stdout using the
console code page, which corrupts UTF-8 path bytes and made a tracked
non-ASCII file look absent. This implementation reads git's stdout as raw bytes
and decodes them as UTF-8, so no console state is read or written.

The two implementations must agree. Both `controls` modes build the same fixture
table and print the same case identifiers, so the outputs can be compared.
Paths are reported with forward slashes on both hosts.

Exit: 0 = every checked file ends with LF
      1 = at least one violation
      2 = usage, environment or internal error, including a skipped file
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('tracked', 'dir', 'controls')]
    [string]$Mode = 'tracked',

    [Parameter(Position = 1)]
    [string]$Path
)

$ErrorActionPreference = 'Stop'
$script:TargetExtensions = @('.c', '.h', '.md', '.sh')
$script:ControlFailures = 0
$script:ControlTmp = $null

function Get-ScriptPath {
    if ($PSCommandPath) { return $PSCommandPath }
    return $MyInvocation.MyCommand.Path
}

function Test-TargetExtension([string]$Name) {
    $ext = [System.IO.Path]::GetExtension($Name)
    return ($script:TargetExtensions -ccontains $ext)
}

# Returns 'lf', 'empty' or 'bad'. Reads one byte, never the whole file.
function Get-EndingKind([string]$FullPath) {
    $fs = [System.IO.File]::OpenRead($FullPath)
    try {
        if ($fs.Length -eq 0) { return 'empty' }
        [void]$fs.Seek(-1, [System.IO.SeekOrigin]::End)
        if ($fs.ReadByte() -eq 10) { return 'lf' }
        return 'bad'
    } finally {
        $fs.Dispose()
    }
}

function ConvertTo-DisplayPath([string]$Relative) {
    return ($Relative -replace '\\', '/')
}

function Invoke-Inventory([string]$Label, [string]$Root, [string[]]$RelativePaths) {
    $checked = 0
    $violations = 0
    $empty = 0
    $skipped = 0

    foreach ($rel in $RelativePaths) {
        if (-not (Test-TargetExtension $rel)) { continue }
        $full = Join-Path $Root $rel
        if (-not (Test-Path -LiteralPath $full -PathType Leaf)) {
            $skipped++
            Write-Host ("  not a regular file: " + (ConvertTo-DisplayPath $rel))
            continue
        }
        try {
            $kind = Get-EndingKind $full
        } catch {
            $skipped++
            Write-Host ("  unreadable: " + (ConvertTo-DisplayPath $rel))
            continue
        }
        $checked++
        if ($kind -eq 'empty') { $empty++; continue }
        if ($kind -eq 'lf') { continue }
        $violations++
        Write-Host ("  missing final newline: " + (ConvertTo-DisplayPath $rel))
    }

    Write-Host ("{0}: {1} checked ({2} empty), {3} violation(s), {4} skipped" -f `
            $Label, $checked, $empty, $violations, $skipped)

    # A file the check could not read is not a file the check passed.
    if ($skipped -ne 0) { return 2 }
    if ($violations -ne 0) { return 1 }
    return 0
}

# Reads git's stdout as bytes and decodes UTF-8 explicitly. The console code
# page is never consulted, so non-ASCII tracked paths survive intact and no
# process-wide encoding state is modified.
function Get-TrackedInventory([string]$Root) {
    $git = (Get-Command git -ErrorAction Stop).Source
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $git
    $clean = $Root.TrimEnd('\', '/')
    $psi.Arguments = '-C "' + $clean + '" ls-files -z'
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true

    $proc = [System.Diagnostics.Process]::Start($psi)
    $buffer = New-Object System.IO.MemoryStream
    try {
        $stderrTask = $proc.StandardError.ReadToEndAsync()
        $proc.StandardOutput.BaseStream.CopyTo($buffer)
        $proc.WaitForExit()
        # Capture everything before disposing: Process.ExitCode is not readable
        # after Dispose, and MemoryStream.ToArray is not readable either.
        $exitCode = $proc.ExitCode
        $stderr = $stderrTask.Result
        $bytes = $buffer.ToArray()
    } finally {
        $buffer.Dispose()
        $proc.Dispose()
    }

    if ($exitCode -ne 0) {
        throw ("git ls-files failed in {0}: {1}" -f $Root, $stderr.Trim())
    }
    $text = [System.Text.Encoding]::UTF8.GetString($bytes)
    if ([string]::IsNullOrEmpty($text)) { return @() }
    return ($text -split [char]0 | Where-Object { $_ -ne '' })
}

function Get-DirInventory([string]$Root) {
    # -Force so dotfiles are included; the extension filter decides the rest.
    $items = Get-ChildItem -LiteralPath $Root -Recurse -File -Force
    return ($items | ForEach-Object { $_.FullName.Substring($Root.Length).TrimStart('\', '/') })
}

# ------------------------------------------------------------------ controls --

function Control-Ok([string]$Label) { Write-Host "  ok   $Label" }

function Control-Bad([string]$Label) {
    $script:ControlFailures++
    Write-Host "  FAIL $Label"
}

function Cleanup-Control {
    if ($script:ControlTmp -and (Test-Path -LiteralPath $script:ControlTmp)) {
        Remove-Item -LiteralPath $script:ControlTmp -Recurse -Force -ErrorAction SilentlyContinue
    }
}

function Write-Fixture([string]$Dir, [string]$Name, [string]$Content) {
    $full = Join-Path $Dir $Name
    $parent = Split-Path -Parent $full
    if (-not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
    [System.IO.File]::WriteAllText($full, $Content, (New-Object System.Text.UTF8Encoding($false)))
}

# Runs a script as a child process so the exit code is a real process status.
function Invoke-Child {
    param([string]$ScriptPath, [string[]]$ChildArgs)
    $exe = (Get-Process -Id $PID).Path
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $out = (& $exe -NoProfile -ExecutionPolicy Bypass -File $ScriptPath @ChildArgs *>&1 | Out-String)
        $rc = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prev
    }
    return [pscustomobject]@{ Output = $out; Code = $rc }
}

function Invoke-Self {
    param([string]$ChildMode, [string]$ChildPath)
    $args = @('-Mode', $ChildMode)
    if ($ChildPath) { $args += @('-Path', $ChildPath) }
    return Invoke-Child -ScriptPath (Get-ScriptPath) -ChildArgs $args
}

# The meta-controls mutate this script and re-run its own controls. They must not
# recurse, so a mutated copy is run with CHML_HYGIENE_NO_META set.
function Invoke-MutationControl {
    param([string]$Name, [string]$From, [string]$To, [string]$Description)
    $src = Get-Content -LiteralPath (Get-ScriptPath) -Raw
    if ($src.IndexOf($From, [System.StringComparison]::Ordinal) -lt 0) {
        Control-Bad "mutation-$Name`: control pattern not found, the control is stale"
        return
    }
    $mut = Join-Path $script:ControlTmp ("mut-" + $Name + ".ps1")
    # BOM so Windows PowerShell parses the copy exactly as it parses this file.
    [System.IO.File]::WriteAllText($mut, $src.Replace($From, $To),
        (New-Object System.Text.UTF8Encoding($true)))

    $prev = $env:CHML_HYGIENE_NO_META
    $env:CHML_HYGIENE_NO_META = '1'
    $r = Invoke-Child -ScriptPath $mut -ChildArgs @('-Mode', 'controls')
    $env:CHML_HYGIENE_NO_META = $prev

    if ($r.Code -ne 0) { Control-Ok "mutation-$Name`: detected" }
    else { Control-Bad "mutation-$Name`: these controls did not detect a checker that $Description" }
}

function Invoke-Git([string]$Dir, [string[]]$GitArgs) {
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & git -C $Dir @GitArgs *> $null
        return $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prev
    }
}

function Invoke-Controls {
    $script:ControlTmp = Join-Path $env:TEMP ('chml-newline-ctl-' + [guid]::NewGuid().ToString('N'))
    $pass = Join-Path $script:ControlTmp 'pass'
    $fail = Join-Path $script:ControlTmp 'fail'
    # Built from a code point: this file stays ASCII-only, so Windows PowerShell
    # cannot misparse a literal regardless of BOM state.
    $uniOk = 'unicod' + [char]0x00E9 + '.md'
    $uniBad = 'unicod' + [char]0x00E9 + '-bad.md'

    try {
        New-Item -ItemType Directory -Force -Path (Join-Path $pass 'sub dir') | Out-Null
        New-Item -ItemType Directory -Force -Path (Join-Path $fail 'sub') | Out-Null

        # Accepted: LF, CRLF, empty, a non-ASCII name, an out-of-scope extension,
        # and a space in a directory name.
        Write-Fixture $pass 'lf.md' "line`n"
        Write-Fixture $pass 'crlf.c' "line`r`n"
        Write-Fixture $pass 'empty.h' ''
        Write-Fixture $pass $uniOk "line`n"
        Write-Fixture $pass 'ignore.txt' 'no newline here'
        Write-Fixture $pass 'sub dir/nested.sh' "x`n"

        # Refused: missing LF, bare CR, and names containing a space, a quote or
        # non-ASCII bytes.
        Write-Fixture $fail 'missing.md' 'no newline'
        Write-Fixture $fail 'bare-cr.c' "line`r"
        Write-Fixture $fail 'space name.md' 'no newline'
        Write-Fixture $fail "quote'name.md" 'no newline'
        Write-Fixture $fail $uniBad 'no newline'
        Write-Fixture $fail 'sub/missing.h' 'no newline'
        Write-Fixture $fail 'ignore.txt' 'no newline'

        $r = Invoke-Self -ChildMode 'dir' -ChildPath $pass
        if ($r.Code -eq 0) { Control-Ok 'accept-lf+crlf+empty+unicode: positive fixture passes' }
        else { Control-Bad "accept-lf+crlf+empty+unicode: refused with rc=$($r.Code): $($r.Output)" }

        if ($r.Output -match '0 violation') { Control-Ok 'positive-report: zero violations' }
        else { Control-Bad "positive-report: $($r.Output)" }

        if ($r.Output -match '1 empty') { Control-Ok 'accept-empty: empty file counted, not refused' }
        else { Control-Bad "accept-empty: $($r.Output)" }

        if ($r.Output -like '*unicod*') { Control-Bad 'accept-unicode: a compliant non-ASCII name was reported' }
        else { Control-Ok 'accept-unicode: compliant non-ASCII name not reported' }

        $r = Invoke-Self -ChildMode 'dir' -ChildPath $fail
        if ($r.Code -eq 1) { Control-Ok 'reject-missing-lf: nonzero exit status' }
        else { Control-Bad "reject-missing-lf: expected exit 1, got $($r.Code)" }

        foreach ($case in @('missing.md', 'bare-cr.c', 'space name.md', "quote'name.md", 'sub/missing.h')) {
            if ($r.Output -like "*$case*") { Control-Ok "reject-$case`: reported" }
            else { Control-Bad "reject-$case`: not reported" }
        }

        # Labelled in ASCII so the case identifiers stay comparable across hosts;
        # the console may render the non-ASCII name itself as a replacement char.
        if ($r.Output -like "*$uniBad*") { Control-Ok 'reject-unicode: non-ASCII name without final LF reported' }
        else { Control-Bad 'reject-unicode: non-ASCII name without final LF not reported' }

        if ($r.Output -like '*ignore.txt*') { Control-Bad 'ignore-foreign-extension: reported an out-of-scope file' }
        else { Control-Ok 'ignore-foreign-extension: not reported' }

        if ($r.Output -match '6 violation') { Control-Ok 'reject-count: exactly six violations' }
        else { Control-Bad "reject-count: $($r.Output)" }

        # Tracked mode, three-way classification. Git is the checker's own
        # inventory source, so the control uses it too.
        $repo = Join-Path $script:ControlTmp 'repo'
        New-Item -ItemType Directory -Force -Path $repo | Out-Null
        [void](Invoke-Git $repo @('init', '-q', '.'))
        Write-Fixture $repo 'present.md' "ok`n"
        Write-Fixture $repo $uniOk "ok`n"
        [void](Invoke-Git $repo @('add', '-A'))

        $r = Invoke-Self -ChildMode 'tracked' -ChildPath $repo
        if ($r.Code -eq 0 -and $r.Output -match '2 checked') {
            Control-Ok 'tracked-unicode-ok: compliant non-ASCII tracked file accepted (exit 0)'
        } else { Control-Bad "tracked-unicode-ok: rc=$($r.Code) $($r.Output)" }

        Write-Fixture $repo 'missing-lf.md' 'no newline'
        [void](Invoke-Git $repo @('add', '-A'))
        $r = Invoke-Self -ChildMode 'tracked' -ChildPath $repo
        if ($r.Code -eq 1 -and $r.Output -like '*missing-lf.md*') {
            Control-Ok 'tracked-missing-lf: exit 1 and reported'
        } else { Control-Bad "tracked-missing-lf: rc=$($r.Code) $($r.Output)" }

        Write-Fixture $repo 'gone.md' 'x'
        [void](Invoke-Git $repo @('add', '-A'))
        Remove-Item -LiteralPath (Join-Path $repo 'gone.md') -Force
        $r = Invoke-Self -ChildMode 'tracked' -ChildPath $repo
        if ($r.Code -eq 2 -and $r.Output -like '*not a regular file: gone.md*') {
            Control-Ok 'tracked-absent-file: exit 2, classified as absent rather than clean'
        } else { Control-Bad "tracked-absent-file: rc=$($r.Code) $($r.Output)" }

        if (-not $env:CHML_HYGIENE_NO_META) {
            Invoke-MutationControl -Name 'always-accept' `
                -From "if (`$kind -eq 'lf') { continue }" `
                -To "if (`$kind -ne 'empty') { continue }" `
                -Description 'accepts everything'
            Invoke-MutationControl -Name 'always-reject' `
                -From "if (`$kind -eq 'lf') { continue }" `
                -To "if (`$false) { continue }" `
                -Description 'rejects everything'
        }
    } finally {
        Cleanup-Control
    }

    if ($script:ControlFailures -ne 0) {
        Write-Host "$($script:ControlFailures) final-newline control check(s) failed"
        return 1
    }
    Write-Host 'final-newline controls passed'
    return 0
}

# ---------------------------------------------------------------------- main --

switch ($Mode) {
    'tracked' {
        $root = $Path
        if (-not $root) { $root = Split-Path -Parent (Split-Path -Parent (Get-ScriptPath)) }
        if (-not (Test-Path -LiteralPath $root -PathType Container)) {
            Write-Host "error: no such directory: $root"
            exit 2
        }
        try {
            $inventory = Get-TrackedInventory $root
        } catch {
            Write-Host "error: $_"
            exit 2
        }
        exit (Invoke-Inventory 'final newline (tracked)' $root @($inventory))
    }
    'dir' {
        if (-not $Path -or -not (Test-Path -LiteralPath $Path -PathType Container)) {
            Write-Host 'error: dir mode needs an existing directory'
            exit 2
        }
        $root = (Resolve-Path -LiteralPath $Path).ProviderPath.TrimEnd('\', '/')
        $inventory = Get-DirInventory $root
        exit (Invoke-Inventory 'final newline (fixture)' $root @($inventory))
    }
    'controls' {
        exit (Invoke-Controls)
    }
}
