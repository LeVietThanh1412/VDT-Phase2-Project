# Production refactor report

Status: **IN_PROGRESS**. Tested pre-refactor baseline: `b9fb0e0`; see `CODEBASE_AUDIT.md`. Tools/Docs cleanup is not evidence of completed production-source cleanup.

| Refactor | Concrete problem | Scope | Validation / result |
|---|---|---|---|
| R1: connectivity rebuild | Two copies of the same eight-neighbor flood fill can diverge on diagonal clearance or region numbering. | `SeaNavigationMap` and battlefield operational survey. Preserve algorithms, coordinate frame and iteration order. | C++ build,39 Blueprint compiles, all4 native automation tests (including connectivity split/reopen assertions), and natural seed20261029 match passed in251.65s. See `Docs/Testing/R1_NATURAL_MATCH.json`. |
| R2: tactical/query separation | Prematch survey/placement and tactical search/convoy state transitions share one large file; water-query filtering is repeated across runtime systems. | Existing subsystem method definitions and shared internal queries; explicit includes/forward declarations. No reflected class or asset relocation. | C++ build,39 Blueprint compiles, all4 native tests and a natural seed20261030 match passed (RED WINS,605.36s). The five moved mission bodies match the baseline exactly after the ocean-helper rename. Broader final regression matrix remains pending. |

No production class/function has been deleted. `bProfileApplied` is public Blueprint-readable despite no C++ reads, so it is retained. Legacy navigation/projectile code has explicit absence-of-shared-combat guards and is retained. Final acceptance requires C++, affected Blueprints, native tests, real patrol/contact/player regressions and natural full matches on the refactored implementation.
