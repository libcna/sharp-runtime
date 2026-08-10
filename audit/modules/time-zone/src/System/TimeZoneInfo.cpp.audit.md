# Audit: `modules/time-zone/src/System/TimeZoneInfo.cpp`

## Metadata

- AUDITED: 308-line POSIX/Windows system-zone implementation, including TZ
  serialization, IANA mapping, local/ID lookup, and zone enumeration.
- Validation: TimeZone focused suite passed 114/114; direct C++/.NET probes
  cover New York base offset and New York/Havana rule identity.

## Assessment

The internal mutex correctly serializes this implementation's temporary TZ
changes and POSIX helper reads.  The code nevertheless snapshots the currently
active `tm_gmtoff` as a base offset and represents every real rule set as one
offset plus one bool.  Those implementation choices cause SR-AUD-228 and
SR-AUD-229.

## Other missing assertions and diagnostics

- Test lookup under winter and summer invocation dates, POSIX `TZ` restoration
  after every error branch, and concurrent library lookup/Local calls under
  TSan.
- Exercise corrupt/missing zone files, zero-progress platform data, complete
  database enumeration/sorting, mapping aliases/territory behavior, and the
  Windows/Emscripten branches on their native platforms.

## Final assessment

SR-AUD-228 and SR-AUD-229 are confirmed in the paired header report.  No
production or test source was changed.

---

## Remediation record — tickets #2181, #2183, #2184 (2026-08-10)

**SR-AUD-229 → remediated. SR-AUD-228 → confirmed (design-complete), gated as ticket #2185.**
Original evidence above is retained unchanged.

**SR-AUD-229, and the premise correction that widens it.** This report names `BaseUtcOffset`. The
same `time(nullptr)` snapshot also fed `StandardName` and `DaylightName`, which took the *daylight*
abbreviation for any zone queried during its daylight period and were therefore always identical to
each other. Scale, measured over the whole installed database
(`build-probe/2176_probe4_allzones.log`): **499 TZif zones, 158 observing daylight time, 141 of them
reporting the wrong base offset on 2026-08-10**; the other 17 are the southern-hemisphere zones,
correct that day and wrong in January. The metadata is now derived by scanning twelve months of the
current year, so it is a property of the zone. `Africa/Casablanca` is why twelve samples and not
two: it is daylight time in *both* January and July, so the previous Jan/Jul probe saw no standard
sample at all. Cross-check over all 499 zones: "first `tm_isdst == 0` sample" and "minimum offset
across the year" never disagree, and no installed zone lacks a standard sample in 2025, so the
all-daylight fallback is defensive and currently unexercised — stated as such in the code.

**Two post-audit defects in the same bodies**, neither carrying an `SR-AUD-*` identifier:

- **#2183.** `zoneFileExists` checked only `S_ISREG` and then handed the identifier to
  `setenv("TZ", …)`, which glibc parses as a POSIX rule string when it is not a path. All seven
  non-TZif regular files shipped inside `/usr/share/zoneinfo` resolved to zones with offset 0 and a
  name taken from the filename, as did `America//New_York`, `America/./New_York`,
  `./America/New_York`, `America/New_York/`, and an identifier whose embedded NUL truncated the
  filesystem check while all 21 bytes were stored as the zone's `Id`. A `':'` prefix on the `TZ`
  value — the documented way to force file interpretation — was measured and does **not** help
  (`2176_probe3_design.log` §A), so the door now requires a well-formed identifier and the four-byte
  `TZif` magic. The six shipped plain-text data files serve as malformed-input fixtures, so nothing
  is written into system zoneinfo. `InvalidTimeZoneException` was deliberately **not** adopted for
  the non-zone-data case: which exception .NET raises there is unverified here.
- **#2184.** The `TZ` save/restore window was straight-line code, so an exception left the
  process-global zone clobbered; it is now a scope guard. The restore also branched on whether the
  saved value was *empty* rather than whether it was *set*, so a process running with `TZ=""` —
  which POSIX defines as UTC — came back with `TZ` deleted.

**Missing assertions this report asked for, now present:** lookup under winter and summer dates
(replaced by a whole-year scan whose result no longer depends on the invocation date), `TZ`
restoration after the success path, three distinct failure paths, and the empty-but-set case, and
concurrent library lookup/`Local()` calls under **TSan** — 8 threads × 40 rounds through
`Local()`, `FindSystemTimeZoneById()` and `CurrentTimeZone()`, **exit 0, zero reports**. Corrupt and
non-zone platform data is now exercised through the six shipped non-TZif files. Complete database
enumeration and the Windows/Emscripten branches remain untested here and are recorded as exclusions
in `docs/SystemTimeZoneNamespaceReviewPlan.md` §18.

**SR-AUD-228 remains open** because the port stores no adjustment rules and therefore cannot return
`false` where .NET does. The failure is one-directional — this implementation can only be too
permissive, never too strict. The selected repair and its measured gate
(`sizeof(TimeZoneInfo)` 160 → 184, no member ordering avoids it) are recorded in
`docs/SystemTimeZoneNamespaceReviewPlan.md` §18a and held by four `PIN_` tests plus a
`static_assert`.
