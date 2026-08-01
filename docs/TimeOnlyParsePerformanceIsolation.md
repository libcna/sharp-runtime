# TimeOnly parse performance isolation (#1933)

**Status:** evidence complete; behavior-preserving optimization designed and
measured, but not implemented  
**Measured implementation:** local batch HEAD beginning at `0e1b47d6`  
**Historical change:** `83cfb10a` (`#1929` rows 5–6)  
**Audit numbering:** unchanged; #1933 is an inactive post-audit ticket and has
no SR-AUD identifier

## 1. Outcome

The retained pre/post binaries confirm that the parser shipped by #1929 costs
more on its original valid three-fraction-digit workload. The original
seven-round record measured a 1.353x median ratio with disjoint ranges. An
independent 28-round rerun of those exact binaries on 2026-08-01 measured a
1.287x ratio by aggregate medians and a 1.286x paired median; the current binary
was slower in 22 of 28 rounds. The host was substantially noisier than during
the retained record, so the exact 1.353x magnitude did not reproduce.

The original ticket's attribution needs a correction: those binaries compare
the whole #1929 rows 5–6 parser change, not an exact-tick-only change. The new
parser added invariant boundary trimming, expanded the fractional scan from
three to seven digits, scaled to ticks rather than milliseconds, and committed
four fields directly. The retained result is a real bundled parser delta, but
it cannot assign all of that delta to exact 100-nanosecond storage.

A controlled candidate replaced only the fractional power-of-ten loop with a
lookup multiplier. It was exhaustively checked over every possible fraction
accumulator, more than 1.9 million parser inputs, and targeted exact `Parse`
exception observations. Across 25 measured rounds at both `-O2` and `-O3`, the
candidate improved several successful paths, but did not improve the complete
surface stably: it was neutral or slower on other successful paths and failure
paths, and the result changed materially with optimization level. GCC already
turns the production loop into immediate multipliers at `-O3`.

Accordingly, #1933 is classified as **optimization designed but not
implemented**, with a material **compiler/toolchain-specific** component. The
performance cost is accepted for the current implementation. No semantic,
source, ABI, representation, symbol, `noexcept`, or `constexpr` change is made.

## 2. Authority and scope

This isolation uses:

- the pre-#1929 and post-#1929 `TimeOnly.cpp` bodies around `83cfb10a`;
- the live `TimeOnly::TryParse` and `TimeOnly::Parse` implementation;
- the shared `DateTimeTextScanner` and invariant boundary trim;
- permanent exact-tick, grammar, failure-output, and Parse/TryParse consistency
  tests in `modules/core/tests/System/TimeOnlyTests.cpp`;
- the retained pre/post benchmark binaries and logs under `build-probe/`;
- the controlled probe `build-probe/1933_timeonly_benchmark.cpp` and its raw
  results, summaries, and disassemblies;
- current .NET `System.TimeOnly` source in
  `src/libraries/System.Private.CoreLib/src/System/TimeOnly.cs`, its
  `DateTimeParse` path, and current `System.TimeOnlyTests.cs` in dotnet/runtime.

The benchmark does not compare sharp-runtime directly with .NET throughput.
.NET stores a single tick count and delegates parsing to its substantially
broader date/time parsing engine. Its source and tests are correctness
references: parsed values preserve 100-nanosecond ticks and the reference tests
pin one through seven fractional digits, whitespace, failures, round trips,
microseconds, and nanoseconds. Comparing those different engines would not
isolate this ticket's mechanism.

## 3. Corrected premises

The original ticket record is retained verbatim in `plan.sqlite3`. These notes
are additive corrections, not replacements.

### Correction 1: the retained comparison is bundled

The title says the "exact-tick remediation" increased valid parse cost. Commit
`83cfb10a` made four relevant success-path changes together:

1. it calls `trimDateTimeText`, scanning approved boundary whitespace without
   allocating;
2. it raises the fractional digit limit from three to seven;
3. it scales the accumulator to 10,000,000 ticks per second rather than 1,000
   milliseconds per second; and
4. it writes the already-validated four fields directly instead of invoking the
   millisecond constructor.

The retained binaries therefore prove the net cost of that bundle on
`10:20:30.123`. They do not prove that tick storage alone costs 1.353x.

### Correction 2: 1.353x is a retained result, not a universal constant

The exact retained seven-round logs remain valid evidence for their run. The
independent 28-round rerun preserved direction but measured 1.287x by medians,
with overlap and large host noise. A CPU-0-pinned diagnostic was noisier still
and is retained but rejected as a stability basis. The recommendation therefore
uses the controlled 25-round experiments rather than claiming a universal
1.353x multiplier.

### Correction 3: the explicit scaling loop is not always generated

The C++ source contains `while (digits < 7)`. GCC 14.2 at `-O2` emits that loop.
At `-O3`, the same compiler unrolls the fractional digit scan and emits an
immediate multiplier for each exit. A source-level lookup is consequently not
a uniformly cheaper operation.

## 4. Current parser mechanism

The live grammar remains:

```text
W* H{1,2} ':' m{1,2} ':' s{1,2} [ '.' f{1,7} ] W*
```

`TryParse` performs the following work:

1. create an allocation-free `string_view` and remove approved outer ASCII
   whitespace;
2. scan the three clock fields using the shared cursor;
3. if a dot is present, scan one through seven fraction digits while
   accumulating base ten;
4. multiply the fraction by ten until it represents ticks within the second;
5. require complete consumption and validate the three component ranges;
6. commit four 32-bit fields to the output, or assign `TimeOnly.MinValue` on
   every failure.

`Parse` calls `TryParse`. On success it returns the value. On failure it creates
the exact `FormatException` message. Successful `TryParse` and `Parse` create no
substring and no temporary `std::string`; trimming is a view operation.

| Suspected mechanism | Finding |
|---|---|
| Additional digit loop | Present for fractions; maximum rises from 3 to 7 digits. |
| Power-of-ten scaling | Separate loop at `-O2`; unrolled immediate multipliers at `-O3`. |
| Checked arithmetic | Absent. Bounds make all `int` accumulator/scaling operations safe. |
| Temporary string / substring | Absent on successful parse and on `TryParse` failure. |
| Locale/provider handling | Absent; scanner accepts ASCII digits and fixed separators only. |
| Shared scanner abstraction | Inline cursor; its fraction loop and bounds branches are visible in generated code. |
| Millisecond conversion | Absent after #1929; exact ticks are committed directly. |
| Redundant validation | No proven redundant check: whole-input and range checks protect distinct failure classes. |
| Exception preparation | Only `Parse` failures allocate/build the message and throw; it dominates those measurements. |
| Boundary trim | Adds two short scans even when the input has no whitespace; no allocation. |
| Branch behavior | Fraction length selects loop exits/scaling; invalid eighth digits and ranges take different exits. |

## 5. Benchmark design

### 5.1 Compiler and build configuration

The controlled executable reports:

```text
compiler=GCC-14.2.0 standard=c++23 ndebug=1
optimization=-O2 or -O3
warmup_rounds=5 measured_rounds=25
success_iterations=100000
tryparse_failure_iterations=100000
parse_failure_iterations=2000
architecture=x86_64
```

The probe is a repository-local standalone optimized build using the live core
implementation. `-fno-access-control` is confined to the probe so its controlled
parser can commit the same private four-field representation. No production
header is modified. Implementations are measured in an interleaved order.
Checksums incorporate success, exact ticks, failure values, HResult/message
observations, and the iteration number, and are written through an atomic sink
to prevent dead-code elimination.

The lower iteration count for failing `Parse` is deliberate: exception
construction and unwinding are roughly two orders of magnitude slower. It is
identical for the two implementations.

### 5.2 Exact corpus

| Case | Escaped input | Expected |
|---|---|---|
| no fraction | `10:20:30` | success |
| 1 digit | `10:20:30.1` | success |
| 3 digits | `10:20:30.123` | success |
| 4 digits | `10:20:30.1234` | success |
| 6 digits | `10:20:30.123456` | success |
| 7 digits | `10:20:30.1234567` | success |
| approved whitespace | ` \x0910:20:30.1234567\x0d\x0a` | success |
| invalid fraction | `10:20:30.12345678` | failure |
| out of range | `24:00:00` | failure |

Every case is measured through `TryParse` and `Parse` separately.

### 5.3 Compared implementations

- **current** is the exact live parser.
- **scale_lut** is a controlled copy whose only source-level parsing change is
  replacing repeated fraction scaling with the multiplier table
  `{0, 1000000, 100000, 10000, 1000, 100, 10, 1}`.
- **legacy_3digit** copies the pre-#1929 parser. It is timed only on successful
  no-fraction, one-digit, and three-digit texts where that input's result is
  identical. It is diagnostic only: its general grammar, boundary handling,
  failure-output contract, and construction path differ, so it cannot qualify
  an optimization.

### 5.4 Correctness controls for `scale_lut`

Before timing, the executable checks:

- all 11,111,110 possible accumulator states across fraction widths 1–7;
- all 86,400 valid clock seconds with no fraction and one representative at
  every fraction width: 691,200 texts;
- all 1,000,000 two-digit hour/minute/second combinations from `00` through
  `99`, covering valid and out-of-range components;
- 250,000 deterministic ASCII fuzz inputs of length 0–24;
- targeted empty, minimum, maximum, whitespace, separator, sign, suffix,
  fraction, and component-boundary cases;
- exact `TryParse` success/failure, parsed ticks, and failure output;
- targeted exact `Parse` success/ticks or exception kind, HResult, and message.

The executed totals were:

```text
verification=pass
try_inputs=1941233
parse_inputs=1030
scaling_states=11111110
sizeof_timeonly=16
alignof_timeonly=4
```

The permanent suite already pins one through seven fraction digits and ticks,
outer whitespace, rejection of an eighth digit and junk, min/max, round trips,
microsecond/nanosecond accessors, Parse/TryParse consistency, exact failure
identity, and failure-output normalization. Because no implementation is made,
no permanent test is changed merely for this measurement ticket.

## 6. Retained before/current reproduction

The retained binaries are the actual optimized prefix and post-#1929
executables, not reconstructed source substitutes. Each reported round performs
100,000 valid parses per implementation; the ticket's 200,000 figure is the
paired prefix-plus-current workload.

| Run | Prefix median ms | Prefix min / p05–p95 / max | Current median ms | Current min / p05–p95 / max | Ratio | Direction |
|---|---:|---:|---:|---:|---:|---:|
| retained original, 7+7 rounds | 12.638 | 11.998 / — / 14.735 | 17.103 | 16.024 / — / 18.945 | 1.353x | disjoint ranges |
| independent, 28+28 rounds | 26.343 | 16.747 / 17.590–33.800 / 40.501 | 33.907 | 21.304 / 21.796–39.471 / 44.703 | 1.287x medians; 1.286x paired median | current slower 22/28 |
| CPU-0 diagnostic, 28+28 rounds | 28.512 | 25.892 / 26.807–50.121 / 55.806 | 40.349 | 26.184 / 35.279–75.016 / 76.196 | 1.415x medians; 1.432x paired median | current slower 22/28; rejected as noisier |

For the independent unpinned run, sample variance was 31.089687 ms2 for the
prefix and 48.740302 ms2 for current; the paired ratio p05–p95 interval was
0.869997–1.963344. For the pinned diagnostic, variance rose to 75.230202 and
221.252108 ms2. CPU pinning did not isolate this shared host and is not used to
strengthen the conclusion.

The independent result reproduces the direction and a material median cost,
but not the original disjoint ranges or exact 1.353x magnitude.

## 7. Controlled `-O3` measurements

Times are nanoseconds per operation. `ratio` is the median of the 25 paired
`current / scale_lut` ratios. The bracket is paired p05–p95; `wins` counts
rounds in which the candidate was faster. Min/max are retained so outliers are
not hidden.

### 7.1 `TryParse`, GCC 14.2 `-O3`

| Case | Current median (min–max) | LUT median (min–max) | Ratio [p05–p95] | LUT wins |
|---|---:|---:|---:|---:|
| no fraction | 8.573 (7.323–10.783) | 7.567 (6.496–11.457) | 1.116 [1.017–1.195] | 24/25 |
| 1 digit | 9.776 (8.356–12.372) | 8.742 (7.468–10.826) | 1.118 [1.003–1.232] | 23/25 |
| 3 digits | 11.514 (10.013–15.056) | 10.315 (8.953–14.781) | 1.118 [1.020–1.143] | 24/25 |
| 4 digits | 12.498 (10.691–13.805) | 10.923 (9.653–13.036) | 1.126 [1.083–1.231] | 25/25 |
| 6 digits | 14.263 (12.588–15.940) | 12.645 (11.277–14.739) | 1.132 [1.024–1.171] | 25/25 |
| 7 digits | 14.443 (12.804–20.597) | 12.709 (10.962–21.308) | 1.128 [1.051–1.172] | 24/25 |
| whitespace | 16.192 (14.162–31.239) | 14.563 (12.681–19.363) | 1.111 [0.980–1.189] | 23/25 |
| invalid fraction | 12.411 (10.905–30.515) | 13.014 (11.419–14.089) | 0.954 [0.900–1.026] | 3/25 |
| out of range | 6.897 (6.128–8.687) | 7.438 (6.517–8.922) | 0.922 [0.886–0.999] | 2/25 |

The valid `TryParse` paths favor this compiled candidate, while both requested
failure paths favor current. That is already insufficient for a universal
"stable improvement" claim.

### 7.2 `Parse`, GCC 14.2 `-O3`

| Case | Current median (min–max) | LUT median (min–max) | Ratio [p05–p95] | LUT wins |
|---|---:|---:|---:|---:|
| no fraction | 10.153 (8.821–12.699) | 10.156 (8.872–12.646) | 0.996 [0.909–1.096] | 10/25 |
| 1 digit | 10.589 (9.075–11.992) | 10.438 (9.179–11.600) | 0.990 [0.952–1.043] | 10/25 |
| 3 digits | 12.035 (10.353–18.687) | 12.224 (10.575–15.410) | 0.978 [0.941–1.110] | 6/25 |
| 4 digits | 12.318 (10.971–28.127) | 12.735 (10.734–15.454) | 0.978 [0.855–1.051] | 9/25 |
| 6 digits | 14.172 (12.569–17.624) | 14.263 (12.405–22.524) | 0.999 [0.931–1.046] | 12/25 |
| 7 digits | 14.570 (12.770–23.266) | 14.276 (12.309–27.855) | 1.008 [0.992–1.061] | 19/25 |
| whitespace | 16.314 (14.176–20.318) | 15.894 (14.050–19.459) | 1.033 [0.963–1.067] | 23/25 |
| invalid fraction | 928.971 (824.349–1815.026) | 926.727 (814.635–1106.305) | 1.000 [0.938–1.607] | 12/25 |
| out of range | 932.623 (805.318–1701.372) | 918.311 (802.142–1575.144) | 1.020 [0.902–1.535] | 18/25 |

Successful `Parse` delegates to the corresponding parser through one call in
both generated functions. Its extra function frame and return work reduce the
relative fraction-scaling signal. Failure timings are dominated by exception
message construction and unwinding and have wide spreads.

## 8. Controlled `-O2` measurements

The same source, corpus, verification, warm-up, and 25 rounds were rebuilt at
`-O2`. This configuration is important because generated code retains both
source loops.

### 8.1 `TryParse`, GCC 14.2 `-O2`

| Case | Current median (min–max) | LUT median (min–max) | Ratio [p05–p95] | LUT wins |
|---|---:|---:|---:|---:|
| no fraction | 10.020 (8.639–14.092) | 9.733 (8.952–14.302) | 1.023 [0.936–1.094] | 19/25 |
| 1 digit | 12.977 (11.747–21.511) | 10.347 (9.299–19.597) | 1.229 [1.066–1.402] | 25/25 |
| 3 digits | 13.574 (12.628–15.940) | 11.820 (10.944–16.075) | 1.146 [0.960–1.278] | 21/25 |
| 4 digits | 14.266 (13.120–20.625) | 12.917 (11.967–21.457) | 1.114 [0.978–1.182] | 23/25 |
| 6 digits | 15.595 (14.404–23.420) | 15.246 (13.923–26.287) | 1.027 [0.894–1.094] | 18/25 |
| 7 digits | 16.153 (14.742–25.860) | 15.505 (13.987–27.273) | 1.032 [0.944–1.202] | 16/25 |
| whitespace | 18.181 (16.575–25.835) | 18.137 (16.341–34.728) | 1.006 [0.894–1.063] | 13/25 |
| invalid fraction | 15.418 (13.603–22.394) | 15.062 (13.551–23.308) | 1.009 [0.941–1.119] | 16/25 |
| out of range | 8.611 (7.819–9.880) | 8.893 (8.191–9.706) | 0.981 [0.895–1.081] | 8/25 |

### 8.2 `Parse`, GCC 14.2 `-O2`

| Case | Current median (min–max) | LUT median (min–max) | Ratio [p05–p95] | LUT wins |
|---|---:|---:|---:|---:|
| no fraction | 12.956 (11.545–20.040) | 11.865 (10.626–19.994) | 1.109 [1.002–1.184] | 24/25 |
| 1 digit | 15.296 (13.789–18.365) | 12.756 (11.836–16.504) | 1.167 [1.048–1.256] | 25/25 |
| 3 digits | 15.860 (14.704–21.742) | 14.802 (13.656–19.559) | 1.076 [1.002–1.117] | 23/25 |
| 4 digits | 16.491 (15.617–25.254) | 15.536 (14.158–25.415) | 1.071 [0.995–1.144] | 22/25 |
| 6 digits | 17.858 (16.648–29.307) | 16.721 (15.579–29.165) | 1.038 [0.965–1.120] | 19/25 |
| 7 digits | 17.310 (16.044–26.706) | 18.722 (17.208–28.444) | 0.941 [0.874–1.009] | 2/25 |
| whitespace | 20.784 (19.244–24.766) | 20.891 (18.804–26.861) | 0.997 [0.918–1.077] | 10/25 |
| invalid fraction | 1064.626 (976.004–1956.888) | 1036.018 (964.092–2000.385) | 1.017 [0.960–1.260] | 16/25 |
| out of range | 1048.692 (947.696–1176.072) | 1058.851 (962.314–1578.946) | 0.988 [0.750–1.041] | 10/25 |

The most important counterexample is successful seven-digit `Parse`: the
candidate is 5.9% slower by paired median and loses 23 of 25 rounds at `-O2`.
This is a currently approved and correctness-critical path.

## 9. Generated-code isolation

The retained disassemblies show:

- **current `-O2`:** a base-ten fraction accumulation loop followed by a
  second loop multiplying by ten until seven digits;
- **candidate `-O2`:** the accumulation loop followed by one indexed
  multiplication, but GCC materializes the small local table in the stack frame;
- **current `-O3`:** the compiler unrolls the fraction scan and routes each exit
  to an immediate multiplier (`10`, `100`, `1000`, `10000`, `100000`, or
  `1000000`); there is no runtime scaling loop;
- **candidate `-O3`:** the compiler also unrolls scanning, but retains a larger
  stack frame and indexed multiplier path;
- both `Parse` success paths call their matching TryParse core; neither inlines
  away that boundary in the measured executable.

This explains both the valid-`TryParse` opportunity at `-O2` and the lack of a
portable whole-surface win. Source-level operation counts alone are misleading
once optimization and code layout change.

## 10. Options investigated

### A. Fixed lookup multiplier

Measured above. It is mathematically equivalent and passed the complete probe
verification, but performance is mixed across APIs, fraction widths, failures,
and optimization levels. **Reject for production now.**

### B. `switch` or manually unrolled scaling

This can force immediate multipliers at lower optimization levels. At `-O3`,
GCC already generates that shape from current source. Encoding compiler work in
the source would increase branches/code size and has no evidence of a stable
whole-surface advantage. **Design-only; do not implement without a new stable
cross-toolchain result.**

### C. Combine digit validation, accumulation, and scaling

One pass could accumulate directly in tick positions, but it either needs the
final digit count in advance or introduces per-digit weights/branches. It also
duplicates the shared scanner. The measured cost does not justify that
maintenance and validation surface. **Reject now.**

### D. Fast path for zero or three fraction digits

The no-fraction path already skips fraction parsing. A three-digit special case
would add a branch to every fractional input and would not address 4–7 digits.
No stable complete-corpus evidence supports it. **Reject now.**

### E. Remove trimming, whole-input validation, range checks, failure reset, or
exact ticks

Each changes approved observable behavior or correctness. Reverting to
millisecond truncation is explicitly prohibited. **Not an optimization option.**

### F. Change `TimeOnly` representation or public API

Unnecessary and outside approval. The current four-int representation is 16
bytes, alignment 4, and already retains exact ticks. **Not an option.**

### G. Preserve current source and let release toolchains optimize it

This is the recommendation. It keeps the simple audited parser and lets `-O3`
eliminate the scaling loop where profitable. A future ticket may revisit it
only with stable results across the complete corpus and supported toolchains.

## 11. Compatibility and consequences

Because this ticket makes no production change:

| Dimension | Consequence |
|---|---|
| Accepted/rejected inputs | unchanged |
| Parsed ticks/values | unchanged; exact 100 ns retained |
| Parse/TryParse consistency | unchanged |
| Exception type/HResult/message/order | unchanged |
| Public declarations and parameter/return types | unchanged |
| ABI, layout, field offsets, vtable, mangled symbols | unchanged |
| `sizeof(TimeOnly)` / `alignof(TimeOnly)` | 16 / 4, unchanged |
| `noexcept` / `constexpr` | unchanged |
| Module graph and dependencies | unchanged |
| Runtime performance | current measured behavior retained |
| Migration | none |

No rollback is needed. If a later optimization is attempted, rollback is the
single parser-body commit and the qualifying test/benchmark must compare the
reverted body with the candidate under the same corpus and toolchains.

## 12. Permanent qualification bar for any future implementation

A future candidate must, before merge:

1. match success/failure and exact ticks over the complete retained probe,
   permanent tests, and targeted Parse exception observations;
2. preserve failure output, exact error type/HResult/message, and ordering;
3. preserve declarations, symbols, layout, field offsets, size/alignment,
   `noexcept`, and `constexpr` state;
4. run at least 25 post-warm-up interleaved rounds at supported release
   optimization levels;
5. show stable improvement for both Parse and TryParse across successful and
   failed corpora, with no material regression in any requested fraction width;
6. show no material regression in DateOnly, DateTime, DateTimeOffset, or
   TimeSpan parsing if shared scanner code changes;
7. pass focused ASan/UBSan using objects newer than the source, followed by the
   full repository gate.

A small result, a noisy result, or a win obtained by doing less correctness
work remains insufficient.

## 13. Retained evidence manifest

The following repository-local ignored artifacts are retained:

```text
build-probe/1929_parse_benchmark_prefix.bin
build-probe/1929_parse_benchmark_after.bin
build-probe/1929_parse_benchmark_prefix_rerun.log
build-probe/1929_parse_benchmark_after_stable.log
build-probe/1933_timeonly_benchmark.cpp
build-probe/1933_analyze.py
build-probe/1933_analyze_retained.py
build-probe/1933_timeonly_benchmark_o2.bin
build-probe/1933_timeonly_benchmark_o3.bin
build-probe/1933_timeonly_benchmark_o2_raw.log
build-probe/1933_timeonly_benchmark_o2_summary.log
build-probe/1933_timeonly_benchmark_raw.log
build-probe/1933_timeonly_benchmark_summary.log
build-probe/1933_timeonly_benchmark_raw2.log
build-probe/1933_timeonly_benchmark_summary2.log
build-probe/1933_retained_binary_28round_raw.log
build-probe/1933_retained_binary_28round_summary.log
build-probe/1933_retained_binary_cpu0_28round_raw.log
build-probe/1933_retained_binary_cpu0_28round_summary.log
build-probe/1933_o2_current_tryparse_disassembly.log
build-probe/1933_o2_scale_lut_disassembly.log
build-probe/1933_o3_current_tryparse_disassembly.log
build-probe/1933_o3_current_parse_disassembly.log
build-probe/1933_o3_scale_lut_disassembly.log
build-probe/1933_o3_scale_lut_parse_disassembly.log
```

`1933_timeonly_benchmark_raw.log` and its first summary are retained as a
rejected harness iteration: the controlled `Parse` wrapper had an extra
non-inline boundary that the production wrapper did not. The corrected source
splits an inline core from equal no-inline public probe wrappers; all conclusions
use `raw2`/`summary2` and the `-O2` pair.

## 14. Ticket disposition

Close #1933 as **done, evidence-only: optimization designed but not
implemented**. This disposition approves no semantic change and needs no user
decision. If performance becomes operationally material under a named compiler
and release configuration, open a new bounded implementation measurement from
this evidence rather than reopening #1929 correctness or weakening exact-tick
behavior.
