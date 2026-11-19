# Does a freshly built HaloMCCVRInject.exe hold the same program as the file Microsoft cleared?
#
# A rebuild of unchanged source still differs from the cleared file in its build stamps: the compiler's Rich header
# (which also moves the PE header), the link timestamp and /Brepro hash in the header and debug directory, and the
# import "hints" that come from the Windows SDK's import libraries (a lookup speed-up the loader may ignore). This
# compares every section byte for byte with only those fields masked, so any change to the code, its data, strings,
# imports, exception tables, resources or relocations still counts as a different program.
#
# Exit code 0: same program. 1: different (the reasons are printed). 2: a file could not be read as a 64-bit PE.
param(
    [Parameter(Mandatory = $true)][string]$Cleared,
    [Parameter(Mandatory = $true)][string]$Built
)

$ErrorActionPreference = 'Stop'

function Read-Pe([string]$path) {
    $b = [IO.File]::ReadAllBytes($path)
    if ($b.Length -lt 0x40 -or [BitConverter]::ToUInt16($b, 0) -ne 0x5A4D) { throw "$path is not an executable." }
    $pe = [BitConverter]::ToInt32($b, 0x3C)
    if ([BitConverter]::ToUInt32($b, $pe) -ne 0x4550) { throw "$path has no PE header." }
    $sectionCount = [BitConverter]::ToUInt16($b, $pe + 6)
    $optSize = [BitConverter]::ToUInt16($b, $pe + 20)
    $opt = $pe + 24
    if ([BitConverter]::ToUInt16($b, $opt) -ne 0x20B) { throw "$path is not a 64-bit program." }
    $sections = @()
    for ($i = 0; $i -lt $sectionCount; $i++) {
        $o = $opt + $optSize + 40 * $i
        $sections += [pscustomobject]@{
            Name   = [Text.Encoding]::ASCII.GetString($b, $o, 8).TrimEnd([char]0)
            VSize  = [BitConverter]::ToUInt32($b, $o + 8)
            Rva    = [BitConverter]::ToUInt32($b, $o + 12)
            RawSize = [BitConverter]::ToUInt32($b, $o + 16)
            RawPtr = [BitConverter]::ToUInt32($b, $o + 20)
        }
    }
    $pe = [pscustomobject]@{ Bytes = $b; Coff = $pe + 4; Opt = $opt; OptSize = $optSize; Sections = $sections }
    $pe | Add-Member ScriptMethod Dir { param($i) [BitConverter]::ToUInt32($this.Bytes, $this.Opt + 112 + 8 * $i) }
    $pe | Add-Member ScriptMethod DirSize { param($i) [BitConverter]::ToUInt32($this.Bytes, $this.Opt + 116 + 8 * $i) }
    $pe | Add-Member ScriptMethod Offset {
        param([uint32]$rva)
        foreach ($s in $this.Sections) {
            if ($rva -ge $s.Rva -and $rva -lt $s.Rva + [Math]::Max($s.VSize, $s.RawSize)) { return [int]($rva - $s.Rva + $s.RawPtr) }
        }
        return -1
    }
    return $pe
}

# File offsets of the build-stamp bytes inside the sections.
function Get-StampOffsets($pe) {
    $mask = New-Object 'System.Collections.Generic.HashSet[int]'
    $b = $pe.Bytes
    # Debug directory (6): each entry's timestamp, and the whole payload of the REPRO entry (type 16, the /Brepro
    # hash) and of the CodeView entry (type 2, the PDB GUID and path) if present.
    $dbg = $pe.Offset($pe.Dir(6))
    if ($pe.Dir(6) -ne 0 -and $dbg -ge 0) {
        for ($e = 0; $e -lt [int]($pe.DirSize(6) / 28); $e++) {
            $o = $dbg + 28 * $e
            for ($k = 4; $k -lt 8; $k++) { [void]$mask.Add($o + $k) }
            $type = [BitConverter]::ToUInt32($b, $o + 12)
            if ($type -eq 16 -or $type -eq 2) {
                $size = [BitConverter]::ToUInt32($b, $o + 16)
                $ptr = [BitConverter]::ToUInt32($b, $o + 24)
                for ($k = 0; $k -lt $size; $k++) { [void]$mask.Add([int]$ptr + $k) }
            }
        }
    }
    # Import directory (1): the 2-byte hint in front of each imported name.
    $imp = $pe.Offset($pe.Dir(1))
    if ($pe.Dir(1) -ne 0 -and $imp -ge 0) {
        for ($d = $imp; [BitConverter]::ToUInt32($b, $d + 12) -ne 0; $d += 20) {
            $lookupRva = [BitConverter]::ToUInt32($b, $d)
            if ($lookupRva -eq 0) { $lookupRva = [BitConverter]::ToUInt32($b, $d + 16) }
            for ($t = $pe.Offset($lookupRva); $t -ge 0; $t += 8) {
                $entry = [BitConverter]::ToUInt64($b, $t)
                if ($entry -eq 0) { break }
                if (($entry -band 0x8000000000000000) -eq 0) {
                    $name = $pe.Offset([uint32]($entry -band 0x7FFFFFFF))
                    [void]$mask.Add($name); [void]$mask.Add($name + 1)
                }
            }
        }
    }
    return , $mask
}

try {
    $a = Read-Pe $Cleared
    $c = Read-Pe $Built
} catch {
    Write-Host $_.Exception.Message
    exit 2
}

$reasons = New-Object System.Collections.Generic.List[string]

# Headers: everything but the link timestamp and the checksum.
$coffA = $a.Bytes[$a.Coff..($a.Coff + 19)]; $coffC = $c.Bytes[$c.Coff..($c.Coff + 19)]
for ($k = 0; $k -lt 20; $k++) {
    if ($k -ge 4 -and $k -lt 8) { continue }
    if ($coffA[$k] -ne $coffC[$k]) { $reasons.Add("file header differs at byte $k"); break }
}
if ($a.OptSize -ne $c.OptSize) { $reasons.Add('optional header sizes differ') }
else {
    for ($k = 0; $k -lt $a.OptSize; $k++) {
        if ($k -ge 64 -and $k -lt 68) { continue }
        if ($a.Bytes[$a.Opt + $k] -ne $c.Bytes[$c.Opt + $k]) { $reasons.Add("optional header differs at byte $k"); break }
    }
}

if ($a.Sections.Count -ne $c.Sections.Count) { $reasons.Add('section counts differ') }
else {
    $maskA = Get-StampOffsets $a
    $maskC = Get-StampOffsets $c
    for ($i = 0; $i -lt $a.Sections.Count; $i++) {
        $sa = $a.Sections[$i]; $sc = $c.Sections[$i]
        if ($sa.Name -ne $sc.Name -or $sa.VSize -ne $sc.VSize -or $sa.Rva -ne $sc.Rva -or $sa.RawSize -ne $sc.RawSize) {
            $reasons.Add("section $($sa.Name) has a different size or place"); continue
        }
        $diffs = 0; $first = -1
        for ($k = 0; $k -lt $sa.RawSize; $k++) {
            $oa = [int]$sa.RawPtr + $k; $oc = [int]$sc.RawPtr + $k
            if ($a.Bytes[$oa] -eq $c.Bytes[$oc]) { continue }
            if ($maskA.Contains($oa) -and $maskC.Contains($oc)) { continue }
            if ($first -lt 0) { $first = $sa.Rva + $k }
            $diffs++
        }
        if ($diffs) { $reasons.Add(('section {0}: {1} byte(s) differ, first at RVA 0x{2:X}' -f $sa.Name, $diffs, $first)) }
    }
}

if ($reasons.Count) {
    Write-Host 'The built injector is a different program from the cleared one:'
    $reasons | ForEach-Object { Write-Host "  $_" }
    exit 1
}
Write-Host 'The built injector is the same program as the cleared one (only build stamps differ).'
exit 0
