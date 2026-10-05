from __future__ import annotations

from pathlib import Path

import numpy as np
import pytest

from ml.pipeline import FEATURE_COUNT, load_feature_dataset, make_synthetic_dataset


def test_synthetic_dataset_is_deterministic_and_disjoint() -> None:
    first = make_synthetic_dataset(samples_per_class=20, seed=7)
    second = make_synthetic_dataset(samples_per_class=20, seed=7)
    np.testing.assert_array_equal(first.train_x, second.train_x)
    assert first.train_x.shape[1:] == (FEATURE_COUNT,)
    assert first.synthetic is True
    assert len(first.train_x) + len(first.validation_x) + len(first.test_x) == 40


def test_load_real_feature_directory() -> None:
    tmp_path = Path("tests/runtime/real_dataset")
    for label, offset in (("no_wake", -1.0), ("wake_word", 1.0)):
        directory = tmp_path / label
        directory.mkdir(parents=True, exist_ok=True)
        for index in range(10):
            np.save(directory / f"{index}.npy", np.full(FEATURE_COUNT, offset + index / 100,
                                                         dtype=np.float32))
    dataset = load_feature_dataset(tmp_path, seed=3)
    assert dataset.synthetic is False
    assert dataset.labels == ("no_wake", "wake_word")


def test_rejects_wrong_feature_shape() -> None:
    tmp_path = Path("tests/runtime/bad_dataset")
    for label in ("a", "b"):
        directory = tmp_path / label
        directory.mkdir(parents=True, exist_ok=True)
        for index in range(10):
            np.save(directory / f"{index}.npy", np.zeros(FEATURE_COUNT - 1, dtype=np.float32))
    with pytest.raises(ValueError, match="expected"):
        load_feature_dataset(tmp_path)
