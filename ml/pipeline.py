"""Reproducible wake-word demo pipeline.

Real datasets use ``<root>/<label>/*.npy`` files. Each file must contain one
40-element float32 feature vector. The synthetic mode is only a CI/demo smoke
test and its accuracy must never be represented as real-world accuracy.
"""

from __future__ import annotations

import argparse
import json
import os
import random
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

os.environ.setdefault("TF_CPP_MIN_LOG_LEVEL", "2")

import numpy as np
import tensorflow as tf

FEATURE_COUNT = 40
SEED = 20261004
DEFAULT_LABELS = ("no_wake", "wake_word")


@dataclass(frozen=True)
class DatasetSplits:
    train_x: np.ndarray
    train_y: np.ndarray
    validation_x: np.ndarray
    validation_y: np.ndarray
    test_x: np.ndarray
    test_y: np.ndarray
    labels: tuple[str, ...]
    synthetic: bool


def set_reproducible_seed(seed: int = SEED) -> None:
    random.seed(seed)
    np.random.seed(seed)
    tf.keras.utils.set_random_seed(seed)
    try:
        tf.config.experimental.enable_op_determinism()
    except RuntimeError:
        pass


def _split(features: np.ndarray, targets: np.ndarray, labels: tuple[str, ...],
           synthetic: bool, seed: int) -> DatasetSplits:
    generator = np.random.default_rng(seed)
    train_indices: list[int] = []
    validation_indices: list[int] = []
    test_indices: list[int] = []
    for label_index in range(len(labels)):
        indices = np.flatnonzero(targets == label_index)
        generator.shuffle(indices)
        if len(indices) < 10:
            raise ValueError(f"label {labels[label_index]!r} needs at least 10 samples")
        train_end = int(len(indices) * 0.70)
        validation_end = train_end + int(len(indices) * 0.15)
        train_indices.extend(indices[:train_end])
        validation_indices.extend(indices[train_end:validation_end])
        test_indices.extend(indices[validation_end:])

    def take(indices: list[int]) -> tuple[np.ndarray, np.ndarray]:
        ordered = np.asarray(indices, dtype=np.int64)
        generator.shuffle(ordered)
        return features[ordered].astype(np.float32), targets[ordered].astype(np.int64)

    train_x, train_y = take(train_indices)
    validation_x, validation_y = take(validation_indices)
    test_x, test_y = take(test_indices)
    return DatasetSplits(train_x, train_y, validation_x, validation_y,
                         test_x, test_y, labels, synthetic)


def load_feature_dataset(root: Path, seed: int = SEED) -> DatasetSplits:
    if not root.is_dir():
        raise ValueError(f"dataset directory does not exist: {root}")
    labels = tuple(sorted(path.name for path in root.iterdir() if path.is_dir()))
    if len(labels) < 2:
        raise ValueError("dataset needs at least two label directories")
    features: list[np.ndarray] = []
    targets: list[int] = []
    for label_index, label in enumerate(labels):
        for path in sorted((root / label).glob("*.npy")):
            sample = np.load(path, allow_pickle=False)
            if sample.shape != (FEATURE_COUNT,):
                raise ValueError(f"{path} has shape {sample.shape}; expected ({FEATURE_COUNT},)")
            if not np.isfinite(sample).all():
                raise ValueError(f"{path} contains a non-finite feature")
            features.append(sample.astype(np.float32))
            targets.append(label_index)
    if not features:
        raise ValueError("dataset contains no .npy feature files")
    return _split(np.stack(features), np.asarray(targets), labels, False, seed)


def make_synthetic_dataset(samples_per_class: int = 80,
                           seed: int = SEED) -> DatasetSplits:
    if samples_per_class < 10:
        raise ValueError("samples_per_class must be at least 10")
    generator = np.random.default_rng(seed)
    no_wake = generator.normal(-0.65, 0.20, (samples_per_class, FEATURE_COUNT))
    wake = generator.normal(0.65, 0.20, (samples_per_class, FEATURE_COUNT))
    features = np.concatenate((no_wake, wake)).astype(np.float32)
    targets = np.concatenate((np.zeros(samples_per_class), np.ones(samples_per_class))).astype(np.int64)
    return _split(features, targets, DEFAULT_LABELS, True, seed)


def build_model(class_count: int) -> tf.keras.Model:
    inputs = tf.keras.Input(shape=(FEATURE_COUNT,), name="features")
    hidden = tf.keras.layers.Dense(12, activation="relu", name="feature_dense")(inputs)
    outputs = tf.keras.layers.Dense(class_count, activation="softmax", name="class_scores")(hidden)
    model = tf.keras.Model(inputs, outputs)
    model.compile(optimizer="adam", loss="sparse_categorical_crossentropy", metrics=["accuracy"])
    return model


def _representative_samples(features: np.ndarray) -> Iterable[list[np.ndarray]]:
    for sample in features[: min(100, len(features))]:
        yield [sample[np.newaxis, :].astype(np.float32)]


def convert_integer_tflite(model: tf.keras.Model, representative: np.ndarray) -> bytes:
    @tf.function(input_signature=[tf.TensorSpec([1, FEATURE_COUNT], tf.float32)])
    def serving(features: tf.Tensor) -> tf.Tensor:
        return model(features, training=False)

    concrete = serving.get_concrete_function()
    converter = tf.lite.TFLiteConverter.from_concrete_functions([concrete], model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = lambda: _representative_samples(representative)
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8
    return converter.convert()


def inspect_and_evaluate(model_content: bytes, features: np.ndarray,
                         targets: np.ndarray) -> tuple[dict[str, object], float]:
    interpreter = tf.lite.Interpreter(model_content=model_content)
    interpreter.allocate_tensors()
    input_detail = interpreter.get_input_details()[0]
    output_detail = interpreter.get_output_details()[0]
    if input_detail["dtype"] != np.int8 or output_detail["dtype"] != np.int8:
        raise ValueError("model is not full-integer int8")
    input_scale, input_zero = input_detail["quantization"]
    output_scale, output_zero = output_detail["quantization"]
    if input_scale <= 0 or output_scale <= 0:
        raise ValueError("invalid TFLite quantization scale")
    correct = 0
    for sample, target in zip(features, targets, strict=True):
        quantized = np.clip(np.rint(sample / input_scale + input_zero), -128, 127).astype(np.int8)
        interpreter.set_tensor(input_detail["index"], quantized[np.newaxis, :])
        interpreter.invoke()
        output = interpreter.get_tensor(output_detail["index"])[0]
        correct += int(np.argmax(output) == target)
    tensors = {
        "input": {"shape": input_detail["shape"].tolist(), "dtype": "int8",
                  "scale": float(input_scale), "zero_point": int(input_zero)},
        "output": {"shape": output_detail["shape"].tolist(), "dtype": "int8",
                   "scale": float(output_scale), "zero_point": int(output_zero)},
    }
    return tensors, correct / len(targets)


def generate_c_model(model_content: bytes, header_path: Path, source_path: Path) -> None:
    header_path.write_text(
        "#pragma once\n#include <stddef.h>\n#include <stdint.h>\n\n"
        "extern const uint8_t aiot_wake_word_model[];\n"
        "extern const size_t aiot_wake_word_model_len;\n",
        encoding="utf-8",
    )
    lines = ["#include \"wake_word_model.h\"", "", "const uint8_t aiot_wake_word_model[] = {"]
    for offset in range(0, len(model_content), 12):
        chunk = model_content[offset:offset + 12]
        lines.append("    " + ", ".join(f"0x{value:02x}" for value in chunk) + ",")
    lines.extend(("};", "", f"const size_t aiot_wake_word_model_len = {len(model_content)}U;", ""))
    source_path.write_text("\n".join(lines), encoding="utf-8")


def run_pipeline(dataset: DatasetSplits, output_dir: Path, epochs: int = 3,
                 embed_c: bool = False, seed: int = SEED) -> dict[str, object]:
    set_reproducible_seed(seed)
    output_dir.mkdir(parents=True, exist_ok=True)
    model = build_model(len(dataset.labels))
    model.fit(dataset.train_x, dataset.train_y,
              validation_data=(dataset.validation_x, dataset.validation_y),
              epochs=epochs, batch_size=16, verbose=0, shuffle=False)
    model_content = convert_integer_tflite(model, dataset.train_x)
    tensors, accuracy = inspect_and_evaluate(model_content, dataset.test_x, dataset.test_y)
    model_path = output_dir / "wake_word.tflite"
    model_path.write_bytes(model_content)
    (output_dir / "labels.txt").write_text("\n".join(dataset.labels) + "\n", encoding="utf-8")
    evaluation = {
        "dataset": "synthetic_demo" if dataset.synthetic else "user_dataset",
        "synthetic_result_is_not_real_world_accuracy": dataset.synthetic,
        "test_samples": int(len(dataset.test_y)),
        "host_tflite_accuracy": accuracy,
    }
    metadata = {
        "schema": 1,
        "seed": seed,
        "feature_count": FEATURE_COUNT,
        "labels": list(dataset.labels),
        "model_size_bytes": len(model_content),
        "tensors": tensors,
        "hardware_latency_ms": None,
    }
    (output_dir / "evaluation.json").write_text(json.dumps(evaluation, indent=2) + "\n", encoding="utf-8")
    (output_dir / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    if embed_c:
        generate_c_model(model_content, output_dir / "wake_word_model.h",
                         output_dir / "wake_word_model.c")
    return {**metadata, **evaluation}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--dataset", type=Path)
    source.add_argument("--synthetic-demo", action="store_true")
    parser.add_argument("--output-dir", type=Path, default=Path("ml/models"))
    parser.add_argument("--epochs", type=int, default=3)
    parser.add_argument("--seed", type=int, default=SEED)
    parser.add_argument("--embed-c", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.epochs < 1:
        raise SystemExit("--epochs must be positive")
    dataset = (make_synthetic_dataset(seed=args.seed) if args.synthetic_demo
               else load_feature_dataset(args.dataset, seed=args.seed))
    result = run_pipeline(dataset, args.output_dir, args.epochs, args.embed_c, args.seed)
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
