from __future__ import annotations

import json
import shutil
import subprocess
from pathlib import Path

import pytest

from ml.pipeline import make_synthetic_dataset, run_pipeline


@pytest.mark.ml
def test_integer_conversion_inference_and_complete_c_export() -> None:
    tmp_path = Path("tests/runtime/ml_output")
    tmp_path.mkdir(parents=True, exist_ok=True)
    result = run_pipeline(make_synthetic_dataset(24, seed=11), tmp_path,
                          epochs=1, embed_c=True, seed=11)
    model = tmp_path / "wake_word.tflite"
    source = tmp_path / "wake_word_model.c"
    assert model.stat().st_size == result["model_size_bytes"]
    assert result["tensors"]["input"]["dtype"] == "int8"
    assert result["tensors"]["output"]["dtype"] == "int8"
    assert 0.0 <= result["host_tflite_accuracy"] <= 1.0
    evaluation = json.loads((tmp_path / "evaluation.json").read_text(encoding="utf-8"))
    assert evaluation["synthetic_result_is_not_real_world_accuracy"] is True
    assert "aiot_wake_word_model_len" in source.read_text(encoding="utf-8")
    compiler = shutil.which("gcc")
    assert compiler, "gcc is required to validate generated C syntax"
    subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-c",
                    str(source), "-I", str(tmp_path), "-o", str(tmp_path / "model.o")], check=True)
