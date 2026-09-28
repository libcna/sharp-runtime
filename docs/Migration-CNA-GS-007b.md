<!-- SPDX-License-Identifier: MIT -->
# GS-007b: optional pre-write checks for indexed collection proxies

ElementReference<T> gains an optional non-owning pointer to std::function<void()> and an additive
three-argument constructor. The existing two-argument constructor/signature/noexcept and every
existing collection API remain unchanged. No GamerServices/protocol logic belongs here. CNA needs
this generic primitive to enforce host-only/read-only writes before mutation through IList's proxy;
other owners may use it for their own write validation.

The check runs before all tracked assignments, compound writes, prefix/postfix changes, and before
the mutation-counter increment. If it throws, both value and version stay unchanged. Reads are
unaffected; the owner must outlive its temporary proxy and keep the function alive. Empty/no check
preserves earlier behavior and needs no allocation. Copying a proxy copies its non-owning guard;
assigning through a proxy uses the destination's guard.

On the verified 64-bit ABI size grows 16 to 24 bytes (2 to 3 pointers); alignment remains 8.
Existing layout assertions are updated and a legacy-representation/new-layout pin is added.
Standing approval SA-3 authorizes this private storage growth with the layout pin and full gate.
All consumers must rebuild against the new headers, including CNA and mobile-eggbert: mixing old
and new proxy layouts across a translation-unit boundary is an ODR/ABI violation. Source call sites
using the old constructors/indexers need no migration. No vtable/base/signature changes.

Reuse build/, at most two compile jobs; full component test gate results recorded after verification.
No push: this mission explicitly forbids pushing unless the user requests it, superseding the local
standing push workflow for this session. Feature branch: feature/gamer-services-collections.


The full gate includes two optional SoapChannelLiveTest cases. Run them against the unchanged
original Yacht fixture, without touching another agent's files or service port:

```sh
python3 scripts/run_component_tests_with_soap_fixture.py /rv/tmp/samples/SAMPLE-071-Yacht_4_0/xna4-build/bin build
```

The helper copies only the executable/config into build-probe, loads dependencies read-only through
MONO_PATH, chooses a free loopback port, runs the entire component corpus with the endpoint set,
and then stops/removes its own fixture. No sample game UI or owner desktop is used. Mono and that
legally available sample fixture are external test prerequisites, not production dependencies.

Verified full build: zero compiler warnings/errors, two jobs. Complete component gate:
**18,104 run / 18,104 passed / zero failed/skipped, 41 executables**, including six new guard
cases and 19/19 live ServiceModel cases. Logs are `build/gs-007b-build-final.log` and
`build/gs-007b-components-live.log`. No test-count reduction. The first gate's two SOAP skips
were resolved with the private fixture; five unrelated stale XML empty-tag expectations were
reproduced on the original header and corrected in a separate prerequisite commit.

An additional module-boundary checker reports two pre-existing edges: Xml.Serialization uses
Core.Base without a declared dependency, and ServiceModel declares Net.Http public while using
it only in implementation. Those existing module definitions were not changed; the new guard
adds only the standard-library functional header and no module dependency. This extra check
is not green and is not described as repaired by this checkpoint.
