#!/usr/bin/env sh
set -eu

skip_idf=0
skip_ml=0
for argument in "$@"; do
  case "$argument" in
    --skip-idf) skip_idf=1 ;;
    --skip-ml) skip_ml=1 ;;
    *) echo "unknown argument: $argument" >&2; exit 2 ;;
  esac
done

python -m compileall -q ml tests scripts
PYTEST_DISABLE_PLUGIN_AUTOLOAD=1 python -m pytest
if [ "$skip_ml" -eq 0 ]; then python -m ml.pipeline --synthetic-demo --epochs 1 --embed-c; fi
npm run lint
npm test
python scripts/secret_scan.py
python scripts/check_docs.py

if [ "$skip_idf" -eq 0 ]; then
  command -v idf.py >/dev/null 2>&1 || { echo "idf.py unavailable; export ESP-IDF or pass --skip-idf" >&2; exit 1; }
  idf.py set-target esp32s3
  idf.py fullclean
  idf.py build
fi
