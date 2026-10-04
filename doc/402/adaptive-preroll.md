# Adaptive preview pre-roll: implementation and evidence

Base: `9307278e0ece3907dcf88bbcc3e7de0ca87d1a5a` (combined reviewed cancellation/audio integration), branch `codex/402-adaptive-preroll`. Implementation `b0af5b7f`; eviction recovery `fcb8d544`. No merge or push. Root integration supplies ABI/SOVERSION 33; consumers must rebuild against matching headers/wrappers because the native VideoCacheThread layout changed.

## Behavior

`Qt/AdaptivePreroll.h` is a reader-independent policy with injected monotonic microseconds. It initially uses the configured pre-roll count, then after three generation observations uses an EWMA of cost and absolute deviation. Cheap production takes two playback steps; variable or expensive production requests additional headroom. Increases are prompt; decreases require a two-frame difference. The configured minimum supplies the conservative initial default rather than a permanent floor; the configured maximum, frame limit, directional window, estimated video/audio bytes, observed frame bytes, and timeline boundary cap demand. An absolute 120-frame probe ceiling limits policy work. Existing cache settings remain authoritative.

After 500 ms the policy reduces **optional buffering** to one reachable playback step. This is an internal policy constant, not a new setting or an absolute startup guarantee: the current/next reachable step must still exist, and individual decoding, request cancellation, and reader handoff can take longer. A successful startup retains the minimal-step threshold until discontinuity; it does not latch `isReady=true`. Direct readers, explicitly disabled caching, and zero physical capacity retain established direct video/audio behavior. A one-frame cache or zero-ahead window still requires the current cached frame. Timeline endpoints clamp to the last valid frame.

Real seeks, reader replacement, actual speed/direction changes, preview byte-size changes, and Timeline cache epochs reset timing estimates/readiness. Ordinary playhead notifications and repeated assignments of the same speed do not reset the policy. The existing prefetch loop samples only actual current-generation decoding, publishes a contiguous range, and repairs evicted readiness holes behind its cursor without discarding valid cached suffixes. The hot shared gate checks generation/epoch plus at most two current/step cache memberships; large startup probing occurs only on the worker.

Video and audio retain the same `isReady()` gate. In active Timeline prefetch mode, audio obtains shared frames from the existing cache API instead of generating uncached Timeline frames inside the callback. A callback spanning beyond the minimal video step may miss: it leaves the missing tail silent and requests a shared hold. Cache refill cannot clear that request. PlayerPrivate's established hold seeks audio to the current video playhead, then acknowledges the request. Tests cover a three-frame block, a missing third frame, mandatory hold, and recovery at the resynchronized frame. Direct/disabled/zero-capacity audio decoding remains established behavior. `CacheDisk::GetFrame` can perform I/O; this change prevents Timeline generation on that path, not all possible cache I/O or legacy callback blocking.

Public native methods added: `VideoCacheThread::UsesCachedAudio()`, `NotifyAudioCacheMiss()`, and `AcknowledgeAudioCacheMiss()`. No application companion patch, project schema, save/load, or undo changes. SWIG regeneration and runtime import were checked. Raw reader ownership, cancellation generations, worker destruction, and audio sample-flush logic remain on the combined base.

## Lock and callback review

Policy/range updates use the existing seek-state mutex. Worker decoding remains outside that mutex. Cache membership/read operations use existing cache locks, with state-to-cache order matching existing seek operations; decoding releases cache/decoder work before taking state. No new decoder lock or lifecycle lock is acquired by the callback. Miss/ack coordination is atomic. Independent cancellation reviewer found no new lock inversion. This is not a claim that existing cache mutexes are lock-free or wait-free.

## Build and focused checks

Linux x86_64, AMD Ryzen 9 5900XT, GCC 9.4.0, CMake 3.16.3, Qt 5.12.8, FFmpeg 4.2 libraries (avcodec 58.54.100), Python 3.8, SWIG 4.0.1, OpenCV 4.3.0, installed OpenShotAudio. Optimized `RelWithDebInfo` (`-O2 -g -DNDEBUG`), parallel 4. No sanitizer timings used as release timings.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_TESTS=ON
touch bindings/python/openshot.i
cmake --build build --target openshot-AdaptivePreroll-test openshot-VideoCacheThread-test openshot-QtPlayer-test openshot-AudioReaderSource-test openshot-AudioPlaybackThread-test pyopenshot openshot-preroll-workload -- -j4
(cd build && QT_QPA_PLATFORM=minimal ctest -L 'AdaptivePreroll|VideoCacheThread|AudioReaderSource|AudioPlaybackThread|QtPlayer' --output-on-failure)
PYTHONPATH=build/bindings/python LD_LIBRARY_PATH=build/src:/usr/local/lib python3 -c 'import openshot; print(openshot.__file__); t=openshot.VideoCacheThread(); print(t.UsesCachedAudio()); t.NotifyAudioCacheMiss(); t.AcknowledgeAudioCacheMiss()'
QT_QPA_PLATFORM=minimal valgrind --tool=memcheck --error-exitcode=99 --leak-check=full --show-leak-kinds=definite,indirect build/tests/openshot-AudioReaderSource-test '[preroll]'
QT_QPA_PLATFORM=minimal valgrind --tool=memcheck --error-exitcode=99 --leak-check=full --show-leak-kinds=definite,indirect build/tests/openshot-VideoCacheThread-test '[preroll]'
```

51/51 focused native CTest cases passed after eviction recovery, 9.91 s total. Includes five deterministic fake-clock cases and the prior cancellation/audio/QtPlayer seek, handoff, destruction and restart cases. Builds and regenerated matching Python import passed. Before eviction recovery, new audio/readiness Memcheck runs each reported zero errors, zero definite/indirect/possible loss, and 80,425 bytes reachable third-party/process state; the final eviction-recovery Memcheck rerun is recorded below when complete.

## Initial native workload comparison

`tests/PrerollWorkload.cpp` generates shared synthetic Timeline frames on the real VideoCacheThread with a serialized producer: cheap 2 ms/frame, variable 2 ms with 45 ms every seventh frame, overload 55 ms/frame at 30 fps. Eight independent startups/profile, 45 scripted playback steps/run, 33 ms sleep/step, frame limit 64, 16 MiB cache limit. The first comparison sampled the gate but deliberately continued scripted consumption to measure next-cache availability; it is **not** an actual QtPlayer audible-underrun count or a gate-honoring renderer latency measurement.

The initial baseline used the then-pristine root combined base with its ABI-only 33 bump, matching headers/library, before adaptive integration. Candidate was `b0af5b7f`; no workload evictions were induced in these runs. Startup timing is worker-start request to gate release. P95 uses the largest of eight observations, so this is a small local sample, not population tail confidence.

| Workload | Baseline startup median / p95 (ms) | Candidate median / p95 (ms) | Baseline / candidate next-cache misses out of 360 | Baseline / candidate gate-hold probes |
|---|---:|---:|---:|---:|
| Cheap | 80.45 / 88.45 | 6.57 / 7.44 | 0 / 0 | 0 / 0 |
| Variable | 253.46 / 260.34 | 6.55 / 6.74 | 0 / 0 | 0 / 0 |
| Overload | 1724.49 / 1724.91 | 500.64 / 501.17 | 0 / 30 | 352 / 30 |

Cache peaks: cheap/variable 654,150 bytes for both; overload 411,600 baseline vs 360,150 candidate. Process peak RSS approximately 56–58 MiB. These are measured reported frame-cache bytes and process RSS, not proof of all allocator bytes conforming to a limit. The pre-existing CacheMemory cleanup retains at least 20 frames even for tiny byte limits; that separate cache behavior is unchanged.

The overloaded producer demonstrates the startup/headroom tradeoff: starting earlier yielded more cache misses under forced consumption. Finite pre-roll cannot make a 55 ms producer sustain 33 ms consumption. The baseline's 30-frame fixed requirement also repeatedly failed its gate while scripted playback advanced. A second comparison honoring the gate, using an isolated pristine `9307278e` build with matching headers/library, will report actual shared-hold work tails below.

Reproduction helper compiles this exact workload against the selected worktree's configured headers, dependency flags, and native library:

```sh
python3 tests/run-preroll-workload.py /path/to/pristine-worktree /tmp/preroll-baseline
python3 tests/run-preroll-workload.py /path/to/candidate-worktree /tmp/preroll-candidate
# Honor each shared gate hold; select overload profile 2 (omit 2 for all profiles).
python3 tests/run-preroll-workload.py /path/to/pristine-worktree /tmp/preroll-baseline-gated --honor-gate 2
python3 tests/run-preroll-workload.py /path/to/candidate-worktree /tmp/preroll-candidate-gated --honor-gate 2
```

## Reviewer checklist and limits

- Verify matching regenerated wrapper and ABI33 integration library; old class layouts are incompatible.
- Re-run focused policy/cache/audio/player cases, including actual current/next eviction repair and multi-frame audio miss/ack recovery.
- Verify the first-frame requirement, speed direction, tiny/zero/disabled caches, timeline endpoints, and edit epoch/preview reset on representative real projects.
- Compare startup separately from shared-hold tails and sustained production. This synthetic workload is not an MP4 effects/project or physical audio-device qualification.
- Run combined application split/trim/undo/seek/project replacement and device reconnect checks; those broader release checks belong to root integration.

No macOS, Windows, translated execution, physical device reconnect, ThreadSanitizer, or full real-media effects benchmark was run in this task. These remain validation limits, not verified platform outcomes.
