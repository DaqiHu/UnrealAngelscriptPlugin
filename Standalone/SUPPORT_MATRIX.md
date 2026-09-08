# Standalone support matrix

This matrix describes retained dormant tooling for compiler contract
`ue-as-standalone-v1`. Standalone is not currently build-certified against the
reconstructed SDK. Listed fixtures are source coverage, not current passing runs;
see README.md for the source audit and reconstruction boundary.

| Family | Native profile | UE-validation profile | Evidence / boundary |
| --- | --- | --- | --- |
| Lexing, directives, include/import graph | Supported | Supported offline-analysis subset | `AngelscriptStandalone.Frontend`, source-closure fixtures; UE retains its authoritative preprocessor |
| Portable functions, classes and globals | Compiler source retained; execution removed | Compile-only | Primitive compile fixtures retained; UE declarations are never executed |
| Script containers, strings and math library | Removed from distribution | Bundle surface only | No bundled runtime implementation or substitute library |
| UCLASS/USTRUCT/UENUM declarations | Not a native feature | Compile-only supported subset | UEAnalysis declaration/class-model fixtures |
| UPROPERTY/UFUNCTION/default/access metadata | Not a native feature | Compile-only supported subset | UEAnalysis fixtures; reflection allocation/default application is UE-required |
| Delegates and events | Not a native feature | Nominal compile-only proxy | UEAnalysis and `Example_Delegates.as`; every behavior traps |
| Blueprint overrides | Not a native feature | Host shadow compile shim | UEAnalysis and representative project scripts |
| `TArray`, `TMap`, `TSet` | Host-provided types only | UE compile adapter | Adapter tests and project Array/Map examples; layout is `non-ue-abi` |
| `TOptional` | Not promoted | UE compile adapter where exported traits suffice | Adapter matrix; unknown lifecycle/layout traits fail compatibility |
| UObject wrappers | Not promoted | Compile-only adapter surface | No load, resolve, GC or reflection execution |
| Typed offline resources | Not promoted | Found/redirected/missing/incompatible/unknown | Resource tests; stable callable/parameter markers; incomplete scope remains unknown |
| Exported availability | Not promoted | Used `editor-only`/`unavailable` stable symbols are unsupported | Semantic-observer stable-ID evidence; unused unavailable records do not poison a compile |
| Current-source baseline replacement | Not applicable | Project `Script/` roots map to `/Angelscript/Game/` identities | Exported stable module IDs replace only exact current-source modules; same-name/different-identity records fail closed |
| RPC, replication, ProcessEvent, CDO, GC, World | Unsupported for execution | `ue-required` classification | May be allowed as a partial analysis result; not simulated |
| Hot reload and reinstancing | Unsupported | `ue-required` classification | Owned by Unreal editor/runtime only |
| UE bytecode execution or UE-loadable output | Unsupported | Explicitly forbidden | CLI and artifact architecture gates |

The versioned corpus currently contains 11 retained entries: nine supported,
one `ue-required`, and one unsupported. Eight supported UE entries use
explicit complete-project evidence and cover reflected structs, properties,
delegate/event registration, arrays, maps, iterator support templates,
transitive UObject relationships, object-handle set/map keys, and an
authoritative known resource path. The latest resource producer evidence
contains 37 parameter-level markers and proves one strict, authoritative
`found` result without loading the asset. Other project scripts must be added to the
versioned corpus before their surface is promoted.

Bundle schema `1.0`, adapter protocol `1`, and compiler contract
`ue-as-standalone-v1` are required. A schema major, compiler contract, adapter
version, engine-property or surface-hash mismatch fails before registration.

Plugin- and memory-mounted source roots do not yet have a V1 CLI mount
declaration. They are not inferred from directory names; promote them only
after an explicit virtual-mount contract and external evidence are added.
