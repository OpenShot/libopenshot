# JSON keyframe edit benchmark

Build `openshot-benchmark`, then run from the repository root:

```sh
cmake --build build --target openshot-benchmark -j2
QT_QPA_PLATFORM=offscreen build/tests/openshot-benchmark --test 'Timeline JSON transforms (0 existing keys)'
QT_QPA_PLATFORM=offscreen build/tests/openshot-benchmark --test 'Timeline JSON transforms (1000 existing keys)'
```

Each case uses an image clip on layer 5 of a 1280×720, 24 fps timeline. It
starts with the specified number of keys **per axis**, then adds 100 successive
`location_x` / `location_y` samples with Bezier interpolation and handles. Each
JSON diff contains the complete growing curves, matching the editor's transform
update format. Both axes are sent in one diff. The result verifies the final
point counts and values so a discarded update cannot appear faster.

Payload generation, serialization, initial population, and clip/timeline setup
are outside the timer. The measured operation is `Timeline::ApplyJsonDiff`,
including JSON parsing, native keyframe replacement, and cache invalidation.
There is no concurrent player or frame rendering. This isolates application cost;
it is not a measurement of end-to-end GUI responsiveness or lock contention.

The benchmark reports the median operations/second of three trials. One operation
is a JSON update here; for the existing rendering trials it is a frame.
Milliseconds per update = `1000 / operations_per_second`.

## Local comparison

Baseline: release-20260919 at cfa5dfa3, before native JSON/keyframe optimizations.
Optimized build: local changes described below. Both use the same benchmark,
build configuration (`-O3 -DNDEBUG`), system JsonCpp, and machine. These timings
are local measurements, not cross-machine performance guarantees.

| Existing keys per axis | Baseline ms/update | Optimized ms/update |
| --- | ---: | ---: |
| 0 (grows to 100) | 0.82 | 0.33 |
| 1,000 (grows to 1,100) | 17.99 | 6.73 |

The changes transfer already-owned JSON values through the existing by-value
interfaces, avoid copying derived animation data just to load base clip fields,
and retain keyframe storage with a fast append path for sorted input. Unsorted
and duplicate frame coordinates still use the existing `AddPoint` behavior.
Existing public signatures, virtual method layout, and the JSON format are unchanged.
The complete incoming animation still has to be parsed on every update.
