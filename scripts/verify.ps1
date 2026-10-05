param([switch]$SkipIdf, [switch]$SkipMl)
$ErrorActionPreference = "Stop"

python -m compileall -q ml tests scripts
$env:PYTEST_DISABLE_PLUGIN_AUTOLOAD = "1"
$runtimeDirectory = (Resolve-Path "tests/runtime").Path
$env:TEMP = $runtimeDirectory
$env:TMP = $runtimeDirectory
python -m pytest
if (-not $SkipMl) { python -m ml.pipeline --synthetic-demo --epochs 1 --embed-c }
npm run lint
npm test
python scripts/secret_scan.py
python scripts/check_docs.py

if (-not $SkipIdf) {
    if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
        throw "idf.py is not available; source/export ESP-IDF or pass -SkipIdf"
    }
    idf.py set-target esp32s3
    idf.py fullclean
    idf.py build
}
