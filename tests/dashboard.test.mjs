import test from "node:test";
import assert from "node:assert/strict";
import {boundedHistory, csvForHistory, validateTelemetry} from "../www/app.js";

test("validates versioned telemetry", () => {
  const parsed = validateTelemetry({schema: 1, timestamp_ms: 1, simulated: false, heart_rate_bpm: 72});
  assert.equal(parsed.heart_rate_bpm, 72);
  assert.equal(validateTelemetry({schema: 2, timestamp_ms: 1}), null);
  assert.equal(validateTelemetry({schema: 1, timestamp_ms: 1, heart_rate_bpm: 999}), null);
});

test("bounds history", () => {
  assert.deepEqual(boundedHistory([1, 2], 3, 2), [2, 3]);
});

test("exports simulation label without credentials", () => {
  const csv = csvForHistory([{timestamp_ms: 1, simulated: true, heart_rate_bpm: 70}]);
  assert.match(csv, /1,simulation,70/);
});
