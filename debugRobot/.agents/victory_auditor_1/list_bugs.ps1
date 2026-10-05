$content = Get-Content 'bug_report.md' -Raw -Encoding UTF8
$bugs = [regex]::Matches($content, '###\s+(BUG-\d+:[^\r\n]+)')
foreach ($b in $bugs) {
    Write-Host $b.Groups[1].Value
}
