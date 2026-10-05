# Contributing

Keep changes small and evidence-based. Do not add secrets, generated `sdkconfig`, clinical claims, hardware measurements, or security claims without reproducible evidence.

Before opening a change, run `scripts/verify.ps1` on Windows or `scripts/verify.sh` on POSIX. If ESP-IDF or TensorFlow is unavailable, use the documented skip flag, state exactly what was skipped, and rely on CI for the missing check. New pure signal processing needs host tests; new dashboard behavior needs Node tests; protocol changes require documentation and serialization tests.

Use ESP-IDF 5.1+ APIs and ESP32-S3 as the firmware target. Optional dependencies must remain cleanly disabled by default.
