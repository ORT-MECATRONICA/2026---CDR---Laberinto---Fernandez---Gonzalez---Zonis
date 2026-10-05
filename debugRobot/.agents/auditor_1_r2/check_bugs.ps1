$raw = Get-Content 'bug_report.md' -Raw -Encoding utf8
$errors = 0
for ($i = 1; $i -le 34; $i++) {
    $id = ('BUG-{0:D2}' -f $i)
    $nextId = if ($i -lt 34) { ('#### BUG-{0:D2}' -f ($i + 1)) } else { '## 4\.' }
    $pattern = '#### ' + $id + '[\s\S]*?(?=' + $nextId + ')'
    if ($raw -match $pattern) {
        $section = $Matches[0]
        $hasUbic = $section -match '\*\*Ubicaci'
        $hasProb = $section -match '\*\*Problema:\*\*'
        $hasSolu = $section -match '\*\*Soluci'
        if (-not ($hasUbic -and $hasProb -and $hasSolu)) {
            Write-Output ("FAILED on $id : Ubic=$hasUbic Prob=$hasProb Solu=$hasSolu")
            $errors++
        } else {
            Write-Output ("$id : OK")
        }
    } else {
        Write-Output ("NOT FOUND: $id")
        $errors++
    }
}
Write-Output "Total errors: $errors"
