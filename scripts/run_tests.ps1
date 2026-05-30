$env:PATH = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64;C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64;C:\Qt\6.11.1\msvc2022_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
$tests = @("test_database", "test_labelio", "test_geometry", "test_ipc", "test_taxonomy", "test_snapshot", "test_training", "test_model", "test_inference", "test_export")
$passed = 0
$failed = 0
$notFound = 0
foreach ($t in $tests) {
    $exe = "E:\z\project\my\LabelTorchV\out\build\x64-release\$t.exe"
    if (Test-Path $exe) {
        $proc = Start-Process -FilePath $exe -NoNewWindow -Wait -PassThru -RedirectStandardOutput "E:\z\project\my\LabelTorchV\out\build\x64-release\$t.out.txt" -RedirectStandardError "E:\z\project\my\LabelTorchV\out\build\x64-release\$t.err.txt"
        $summary = Get-Content "E:\z\project\my\LabelTorchV\out\build\x64-release\$t.out.txt" | Select-String "Totals:" | Select-Object -Last 1
        if ($proc.ExitCode -eq 0) {
            $passed++
            Write-Host "$t : PASSED ($summary)"
        } else {
            $failed++
            Write-Host "$t : FAILED (exit=$($proc.ExitCode))"
        }
    } else {
        $notFound++
        Write-Host "$t : NOT FOUND"
    }
}
Write-Host ""
Write-Host "=== Summary: $passed passed, $failed failed, $notFound not found ==="
