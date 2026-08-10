<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Nullable-floating HashSet performance isolation (#1937)

## 1. Disposition

Ticket #1937 is evidence-complete with disposition **not reproducible; no
optimization implemented**. The historical 2.092x nullable-double finite
rehash-heavy lookup value is preserved as the observation that opened the
ticket, not as a current conclusion. A corrected 5-warm-up/25-measured-pair
campaign does not show a stable regression at either `-O2` or `-O3`, and the
controls reproduce the same wide system noise.

This batch changes no production source, behavior, public alias, iterator,
representation, layout, symbol, or ABI. It does not reopen #1926 and does not
weaken #1934/#1925's null, NaN, or signed-zero semantics.

## 2. Correction to the historical premise

The original #1925 performance paragraph and ticket #1937 record a 2.092x
median from 14 measured rows. That number is authentic to its retained logs,
but “stable” and “rehash-heavy isolation” were too strong.

**Correction (2026-08-01):** the retained source and analyzer prove that the
old experiment had these properties:

- element type: `std::optional<double>`;
- finite/“rehash” keys: `double(i) + 0.25`, with 40,000 inputs for ordinary
  finite and 120,000 for the case named `rehash`;
- construction: default `HashSet`, incremental `Add`; no `EnsureCapacity`,
  explicit `reserve`, explicit `rehash`, bucket history, load factor, or
  collision measurement;
- queries: the insertion vector itself, once per round, so hit-only in a
  correct implementation;
- hit/count checks: 120,000/120,000 in the finite large case for both forms;
- anti-optimization: a global volatile sink accumulated found and count;
- round shape: one unreported warm-up followed by seven reported rounds in
  each campaign. Combining two independently launched campaigns produced 14
  rows, not 14 alternating pre/current pairs;
- ordering: Dictionary, HashSet, and SortedSet ran in a fixed order inside a
  binary; pre and corrected binaries ran as separate campaigns rather than
  alternating within a pair;
- analysis: independent medians only; no paired ratios, p05/p95, min/max,
  wins/losses, or order stratification;
- compiler flags and exact build configuration: not recorded in the retained
  source, raw log, or summary and therefore not recoverable honestly.

The historical source, raw logs, analyzer, and combined summary remain under
`build-probe/1925_nullable_collection_benchmark*`. No historical line was
rewritten.

## 3. Corrected campaign and retained evidence

The corrected harness uses the exact retained pre-#1925 policy header and the
current header to compile four binaries:

- GCC 14.2.0 / libstdc++ 14 / x86-64;
- C++23, `-DNDEBUG`, warnings-as-errors;
- `-O2` pre/current and `-O3` pre/current;
- no `-march=native` or unsupported system-wide build;
- pre include root first only for
  `build-probe/1925-pre/modules/core/include/System/detail/ComparisonPolicy.hpp`.

`HashSet<T>::SetType` is public and documented as the wrapper's backing type.
The benchmark calls its `count(key) > 0`, exactly matching the lookup operation
inside `HashSet<T>::Contains`. This exposes bucket and load data without a
production test seam. The accepted key type, hasher, equality predicate, and
algorithm are therefore the production ones; version-counter work is absent
from `Contains` in either case.

Each optimization level runs five warm-up pairs and 25 measured pairs. Pair
order alternates `pre,current` / `current,pre`. There are:

- 20 warm-up binary executions and 360 retained warm-up case rows;
- 100 measured binary executions, 1,800 raw case rows, and 900 paired results;
- 18 cases at each optimization level, producing 36 summary rows;
- 120,000 finite inputs and 960,000 lookups per finite row (eight passes);
- 60,000 distribution inputs and 720,000 lookups per null/NaN/mixed row
  (twelve passes).

Hit vectors are deterministically shuffled. Miss vectors are disjoint finite
values. An out-of-line lookup function plus an assembly memory barrier and the
printed exact hit result prevent elimination. Every current-policy hit query
exists; every current miss query is absent. The output records accepted
count, single-pass and total finds, insert/lookup nanoseconds, bucket count and
every transition, load, nonempty buckets, collisions, maximum chain, key and
query hash digests, and container/iterator/predicate sizes.

Retained artifacts are:

- `build-probe/1937_hashset_benchmark.cpp`;
- `build-probe/1937_run_campaign.py` and `1937_analyze_campaign.py`;
- `build-probe/1937_warmup_raw.csv` (360 rows) and
  `1937_measured_raw.csv` (1,800 rows);
- `build-probe/1937_summary.csv` (36 result rows) and
  `1937_validation.txt`;
- four benchmark binaries, full O2 disassemblies, symbol inventories, and
  focused lookup symbol-size extracts;
- `1937_hashset_mechanism.cpp` and `.log`;
- the retained #1925 old source/header/log corpus.

No benchmark process overlapped another benchmark or compilation. Aggregate
compilation/benchmark parallelism was one for this campaign and never exceeded
the repository ceiling of two.

## 4. Correctness, buckets, and identical-work classification

For every finite case, pre/current accepted counts, hit counts, hash digests,
bucket histories, final buckets, load, nonempty buckets, collisions, and
maximum chains are identical. The primary nullable-double rehash case has:

- 120,000 accepted keys and 120,000 finds per pass;
- identical key/query hash digests;
- bucket history
  `1|13|29|59|127|257|541|1109|2357|5087|10273|20753|42043|85229|172933`;
- 172,933 buckets, load 0.69391036, 86,725 nonempty buckets, 33,275
  collisions beyond the first bucket member, and maximum chain 7.

The pre-reserved form has history `1|126271`, load 0.950336993, 77,447
nonempty buckets, 42,553 collisions, and maximum chain 8. Reserving prevents
growth but selects fewer final buckets than incremental growth; it is a stable
bucket-history case, not a claim that reservation must improve lookup.

Finite miss rows find zero. Direct double and optional int are unchanged
controls. Finite optional float and long double also have identical work.

Null-heavy accepted counts and finds are equal (12,001 and 60,000), and bucket
counts/loads are equal, but the exact hash corpus is intentionally different:
the corrected policy hashes null to zero while libstdc++'s old optional hash
uses its sentinel. Those rows are marked non-identical work.

NaN-heavy and mixed rows are correctness comparisons, not like-for-like
performance regressions. In NaN-heavy input, pre accepts 60,000 entries but
finds only the 12,000 finite queries; current collapses NaNs correctly to
12,001 logical keys and finds all 60,000 queries. Mixed pre/current counts are
22,504/15,005 and finds 52,500/60,000. Their fast current ratios cannot be
credited as an optimization because the accepted set and lookup results differ.

## 5. Complete result table

Ratios are paired `current/pre`. `W/L` is the number of 25 pairs won/lost by
current; there were no exact ties. `Same` means every exact-work field in
section 4 matched, not merely count. Full pre/current p05/p95 times,
order-stratified ratios, and direct/optional-int normalized ratios remain in
the CSV.

| Opt | Case | Same | pre/current median ms | paired median | p05–p95 | min–max | W/L |
|---|---|---:|---:|---:|---:|---:|---:|
| O2 | direct_double_finite_rehash_hit | yes | 27.431/36.089 | 1.148179 | 0.551813–3.721861 | 0.531233–4.348966 | 8/17 |
| O2 | direct_double_finite_reserved_hit | yes | 37.284/38.792 | 1.119831 | 0.511365–1.998023 | 0.436142–2.415933 | 11/14 |
| O2 | optional_double_finite_rehash_hit | yes | 34.149/38.734 | **1.026457** | 0.372660–2.444113 | 0.295673–2.999709 | 12/13 |
| O2 | optional_double_finite_rehash_miss | yes | 56.998/49.859 | 1.003177 | 0.376836–2.389611 | 0.341485–3.126380 | 12/13 |
| O2 | optional_double_finite_reserved_hit | yes | 34.796/40.428 | 1.206025 | 0.637467–2.031678 | 0.393023–3.434636 | 9/16 |
| O2 | optional_double_finite_reserved_miss | yes | 52.725/55.984 | 1.066191 | 0.708978–1.507808 | 0.471007–1.746226 | 10/15 |
| O2 | optional_double_mixed_rehash_hit | no | 12.165/9.909 | 0.850690 | 0.705132–1.212093 | 0.628936–1.420893 | 19/6 |
| O2 | optional_double_mixed_reserved_hit | no | 11.468/9.377 | 0.839984 | 0.610613–1.205035 | 0.537680–1.868224 | 20/5 |
| O2 | optional_double_nan_heavy_rehash_hit | no | 44.285/9.006 | 0.236972 | 0.158089–0.279500 | 0.094319–0.284379 | 25/0 |
| O2 | optional_double_nan_heavy_reserved_hit | no | 41.810/6.432 | 0.166724 | 0.100017–0.239941 | 0.084603–0.372238 | 25/0 |
| O2 | optional_double_null_heavy_rehash_hit | no | 5.079/4.542 | 0.889504 | 0.599321–1.226903 | 0.580043–1.994093 | 20/5 |
| O2 | optional_double_null_heavy_reserved_hit | no | 3.988/4.032 | 1.002511 | 0.812779–1.312495 | 0.730550–2.276791 | 10/15 |
| O2 | optional_float_finite_rehash_hit | yes | 43.356/43.943 | 1.146791 | 0.592524–2.751018 | 0.551061–4.042700 | 10/15 |
| O2 | optional_float_finite_reserved_hit | yes | 51.710/49.451 | 1.023068 | 0.663812–1.969416 | 0.525868–2.445360 | 12/13 |
| O2 | optional_int_finite_rehash_hit | yes | 4.091/4.790 | 1.092458 | 0.498286–3.162380 | 0.189440–4.974501 | 10/15 |
| O2 | optional_int_finite_reserved_hit | yes | 4.557/4.418 | 1.011775 | 0.244827–4.310221 | 0.133922–6.229124 | 12/13 |
| O2 | optional_long_double_finite_rehash_hit | yes | 365.836/312.850 | 0.861254 | 0.506793–1.203108 | 0.461412–1.446411 | 19/6 |
| O2 | optional_long_double_finite_reserved_hit | yes | 297.323/265.157 | 0.926690 | 0.573942–1.436975 | 0.399773–1.650149 | 15/10 |
| O3 | direct_double_finite_rehash_hit | yes | 30.558/21.182 | 0.952032 | 0.388669–1.938230 | 0.355514–2.685815 | 14/11 |
| O3 | direct_double_finite_reserved_hit | yes | 31.068/30.725 | 1.148503 | 0.498750–1.719659 | 0.405459–1.824498 | 11/14 |
| O3 | optional_double_finite_rehash_hit | yes | 24.012/28.385 | **0.964323** | 0.457956–2.150148 | 0.252542–2.950840 | 14/11 |
| O3 | optional_double_finite_rehash_miss | yes | 35.672/39.908 | 1.070454 | 0.597071–2.144460 | 0.491098–3.740999 | 9/16 |
| O3 | optional_double_finite_reserved_hit | yes | 29.821/31.223 | 0.940049 | 0.471719–2.145293 | 0.440764–2.605374 | 13/12 |
| O3 | optional_double_finite_reserved_miss | yes | 51.920/51.768 | 1.049401 | 0.769337–1.989258 | 0.713832–2.272590 | 11/14 |
| O3 | optional_double_mixed_rehash_hit | no | 10.069/8.996 | 0.921334 | 0.728030–1.038716 | 0.675850–1.073530 | 21/4 |
| O3 | optional_double_mixed_reserved_hit | no | 10.041/8.620 | 0.855198 | 0.743007–1.079085 | 0.583595–1.083712 | 20/5 |
| O3 | optional_double_nan_heavy_rehash_hit | no | 35.657/7.862 | 0.236217 | 0.179226–0.299517 | 0.158679–0.347364 | 25/0 |
| O3 | optional_double_nan_heavy_reserved_hit | no | 37.036/5.160 | 0.138110 | 0.106630–0.184500 | 0.103726–0.203286 | 25/0 |
| O3 | optional_double_null_heavy_rehash_hit | no | 4.295/3.457 | 0.800043 | 0.710592–1.002891 | 0.590050–1.155336 | 23/2 |
| O3 | optional_double_null_heavy_reserved_hit | no | 2.988/2.913 | 0.971899 | 0.861737–1.045715 | 0.827983–1.123970 | 16/9 |
| O3 | optional_float_finite_rehash_hit | yes | 33.129/29.970 | 1.064966 | 0.447059–2.223789 | 0.386614–2.486928 | 11/14 |
| O3 | optional_float_finite_reserved_hit | yes | 38.950/37.727 | 0.841363 | 0.484354–1.765991 | 0.261085–2.074588 | 15/10 |
| O3 | optional_int_finite_rehash_hit | yes | 3.535/3.184 | 0.932629 | 0.572797–1.982336 | 0.550508–2.146037 | 18/7 |
| O3 | optional_int_finite_reserved_hit | yes | 3.660/3.510 | 1.000863 | 0.393979–2.578012 | 0.263318–4.927707 | 12/13 |
| O3 | optional_long_double_finite_rehash_hit | yes | 329.425/299.845 | 0.862459 | 0.438336–1.375288 | 0.354107–1.477399 | 17/8 |
| O3 | optional_long_double_finite_reserved_hit | yes | 281.803/239.916 | 0.855983 | 0.481983–1.422461 | 0.415210–1.632750 | 15/10 |

The primary ratios are 1.026 at O2 and 0.964 at O3, with nearly even 12/13
and 14/11 win/loss counts and spreads crossing 1 by a wide margin. This is not
2.092x and not a stable direction. The unchanged direct-double and optional-int
controls also have wide ratios and opposite directions between configurations.
Order-stratified and control-normalized ranges remain wide. CPU frequency was
enabled and scaling was observed on the Ryzen 7 PRO 7840U host; the controls
demonstrate that scheduling/frequency/system-load noise dominates small
differences in this environment.

## 6. Mechanism analysis

### 6.1 Predicates and generated code

Before #1925, direct optional-floating keys selected
`std::hash<optional<F>>` and `std::equal_to<optional<F>>`. Current selects
`DefaultHash<optional<F>>` and `DefaultEqualTo<optional<F>>`. For a present
finite value, both compute the same libstdc++ floating hash and produce the
identical bucket distribution.

Current hashing adds the semantically required NaN-class test before calling
the underlying hash. Current equality first handles optional presence and raw
numeric equality; only a raw-unequal floating pair reaches the two NaN tests.
O2 disassembly confirms these branches. The out-of-line optional-double lookup
body is 0x188 bytes current versus 0x161 pre at O2, and 0x3ca versus 0x2ed at
O3. This establishes a plausible finite instruction-cost mechanism, not a
stable measured regression. Removing the checks would restore incorrect NaN
behavior and is prohibited.

### 6.2 Cache policy, nodes, iterators, and layout

The retained mechanism probe reports:

| Key | pre/current cache | pre/current node | regular iterator | local iterator | SetType / wrapper size |
|---|---:|---:|---|---|---:|
| optional<float> | off/off | 16/16 | same | different predicate-bearing spelling | 56 / 64 |
| optional<double> | off/off | 24/24 | same | different predicate-bearing spelling | 56 / 64 |
| optional<long double> | on/off | 64/48 | different | different | 56 / 64 |
| direct double control | off/off | 16/16 | same | same | 56 / 64 |
| optional<int> control | off/off | 16/16 | same | same | 56 / 64 |

Both optional-double hashers are fast and nonthrowing under libstdc++'s cache
selection, so neither node caches a hash code. Node size, ordinary iterator,
container size, load, collisions, and locality opportunity are identical. The
only optional-double mechanism change is predicate code/type identity. The
optional-long-double current speed tendency has a separate approved
representation explanation: #1925 moved it from a cached 64-byte node to an
uncached 48-byte node. That is not an optimization candidate for #1937 and
must not be generalized or reversed.

### 6.3 Causal classification

- The historical 2.092x value is non-reproducible under the required campaign.
- Small instruction differences are causal consequences of corrected
  semantics, but their finite cost is not stably measurable here.
- Optional-double representation/cache policy, outer layout, ordinary
  iterator, bucket distribution, collision count, and load factor do not
  explain a 2x result because they are identical.
- Null, NaN, and mixed ratios are contaminated by intentionally different hash
  values, accepted sets, or findability and cannot establish a finite-only
  optimization.
- The supported GCC/libstdc++ toolchain and host exhibit large
  frequency/scheduling noise, also visible in byte-equivalent controls.

The most accurate disposition is **not reproducible on the tested supported
toolchain/host; evidence only** rather than an accepted stable regression,
portable optimization opportunity, or #1926-style representation decision.

## 7. Optimization boundary, recommendation, and reopening conditions

No optimization was implemented. In particular, the batch did not specialize
reserved standard-library internals, add another hasher framework, change a
public alias, change any iterator, node, representation, layout, symbol, or
accepted key, add pair/tuple hashing, or restore raw NaN behavior.

There is no implementation approval request for #1937 because no candidate is
recommended. Close the investigation as `done` with the corrected
not-reproducible disposition.

Reopen only if at least one of these conditions is met:

1. On a supported toolchain, an idle/frequency-controlled campaign with at
   least five warm-up and 25 alternating measured pairs reproduces a material
   loss in the same direction at both O2 and O3 for finite hit and miss,
   rehash-growth and reserved-stable cases; exact-work fields must match and
   unchanged direct-double/optional-int controls must remain within a narrow
   noise band.
2. A relevant compiler or standard-library update changes inlining, hash-cache
   selection, nodes, or code generation and the complete campaign isolates a
   stable regression.
3. A portable candidate proves identical accepted keys, hash/equality
   outputs, aliases, all iterator spellings, nodes, layout, relevant symbols,
   and behavior; then proves stable improvement across optional float/double/
   long double finite hit/miss and both bucket histories, with no material
   regression for null, NaN, mixed, direct double, or optional int.

Any future semantic shortcut that removes NaN, null, or signed-zero checks is
out of scope regardless of speed. A merely toolchain-specific or noisy result
remains evidence-only.
