$tests = Get-ChildItem -Path "tests" -Recurse -Filter "*.c"
$passed = 0
$failed = 0

foreach ($test in $tests) {
    $name = $test.Name
    clang $test.FullName -o "t.exe" 2>$null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAIL $name" -ForegroundColor Red
        $failed++
        continue
    }
    $output = ./t.exe 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "PASS $name" -ForegroundColor Green
        $passed++
    } else {
        Write-Host "FAIL $name" -ForegroundColor Red
        Write-Host $output
        $failed++
    }
    Remove-Item "t.exe" -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host "$passed passati, $failed falliti"
