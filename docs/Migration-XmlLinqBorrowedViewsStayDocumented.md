<!-- SPDX-License-Identifier: MIT -->
# Declaration — Xml.Linq borrowed views stay a documented contract (#1899 closed)

Ticket **#1899** (SR-AUD-333, CCF-019, probe cases X15/X17), closed 2026-08-19. **It changes no
production statement.**

## What #1899 wanted, and why it cannot be had

#1898 made the borrowed-view contract explicit and testable. #1899 would have made violating it
*impossible*. Its design was completed, and it **proves the original requirement impossible** — for
two independent reasons, either of which is sufficient on its own:

1. **An owning handle to an object no `shared_ptr` owns cannot be manufactured.** The topmost
   ancestor `Ancestors()` yields has no parent, so there is nothing inside the tree holding a share
   to hand out.
2. **`XElement`, `XDocument` and `XContainer` are routinely automatic-storage objects** — 51 such
   declarations in this repository's own tests alone. They have no control block, so
   `std::enable_shared_from_this` would throw `std::bad_weak_ptr` rather than rescue the caller. It
   would convert a latent use-after-free into a **guaranteed throw at a correct call site**.

## The alternative, offered and declined

Changing `Ancestors`/`AncestorsAndSelf` to return a non-owning view type that cannot outlive the
tree — a public source break under SA-2/SA-10. Declined on 2026-08-19
(`docs/StandingApprovals.md` SA-13). The contract stays as #1898 left it: stated as preconditions,
postconditions, invalidation and failure behaviour, and pinned.

## Where the proof lives, and why there

In `modules/xml-linq/tests/System/Xml/Linq/XLinqBorrowedViewTests.cpp`, **beside the contract it
justifies** — not only in the ticket. A future reader who sees "documented, not enforced" should not
have to assume nobody tried.

`Decl1899_AutomaticStorageNodesHaveNoControlBlockToShareFrom` and
`Decl1899_TheTopmostAncestorHasNoParentToOwnIt` pin the two **premises**, because a proof whose
premises silently stop being true is not a proof. They fail the day either premise does, which is
exactly when #1899 would be worth reopening.
