$content = Get-Content 'bug_report.md' -Raw -Encoding UTF8
$bugs = [regex]::Matches($content, '###\s+(BUG-\d+:[^\r\n]+)')
Write-Host "Total BUG headers found: $($bugs.Count)"

$missing = @()
$bugList = @()
for ($i = 0; $i -lt $bugs.Count; $i++) {
    $bugTitle = $bugs[$i].Groups[1].Value
    $startIndex = $bugs[$i].Index
    if ($i -lt ($bugs.Count - 1)) {
        $length = $bugs[$i+1].Index - $startIndex
        $block = $content.Substring($startIndex, $length)
    } else {
        $block = $content.Substring($startIndex)
    }
    
    $hasUbicacion = ($block -match '\*\*Ubicación:\*\*') -or ($block -match '\*\*Ubicaci.n:\*\*')
    $hasProblema = ($block -match '\*\*Problema:\*\*')
    $hasSolucion = ($block -match '\*\*Solución recomendada:\*\*') -or ($block -match '\*\*Soluci.n recomendada:\*\*')
    
    $bugList += [PSCustomObject]@{
        Bug = $bugTitle
        Ubicacion = $hasUbicacion
        Problema = $hasProblema
        Solucion = $hasSolucion
    }
    
    if (-not ($hasUbicacion -and $hasProblema -and $hasSolucion)) {
        $missing += [PSCustomObject]@{
            Bug = $bugTitle
            Ubicacion = $hasUbicacion
            Problema = $hasProblema
            Solucion = $hasSolucion
        }
    }
}

Write-Host "Cataloged bugs count: $($bugList.Count)"
if ($missing.Count -eq 0) {
    Write-Host "SUCCESS: ALL $($bugList.Count) bugs have Ubicación, Problema, and Solución recomendada!"
} else {
    Write-Host "FAILURE: Missing fields in $($missing.Count) bugs:"
    $missing | Format-Table -AutoSize
}
