<!-- SPDX-License-Identifier: MIT -->
# GS-007b full-gate prerequisite: stale XML empty-tag expectations

The initial full component gate exposed five XLinqNamespaceTests failures. An isolated baseline
build with the unchanged fc033a0e ElementReference header reproduced exactly those five failures
(48 namespace cases: 43 passed, 5 failed). The guard change did not cause them.

XmlEncodedRawTextWriter.WriteEndElement in the local .NET source
`/rv/tmp/runtime/src/libraries/System.Private.Xml/src/System/Xml/Core/XmlEncodedRawTextWriter.cs`
(lines 344–351) emits a separating space before an empty-tag slash. The existing writer already
matches that behavior. Correct the four stale expected strings and check each serialization door's
qualified element/attribute data independently; compact XElement serialization remains unchanged.
No production XML code or tests were removed.

Verification: clean full `cmake --build build --parallel 2`; full component gate with isolated
original Yacht SOAP fixture: **18,104 run, 18,104 passed, zero failures/skips, 41 executables**.
This includes the six additive generic proxy guard tests staged separately.
Logs: `build/gs-007b-baseline-xml.log`, `build/gs-007b-build-final.log`,
`build/gs-007b-components-live.log`. No push under the current user's instruction.
