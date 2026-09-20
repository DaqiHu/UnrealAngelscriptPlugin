# Unreal AngelScript Standalone

This directory is `Plugins/Angelscript/AngelscriptLSP/`. The product remains
dormant Standalone (`as-standalone`, CMake project `AngelscriptStandalone`,
compiler contract `ue-as-standalone-v1`). It is not the TypeScript language
server in `Extensions/AngelscriptVSCode/` and not an in-process JSON-RPC
language-server process.

Standalone is retained as dormant tooling during reconstruction of the UE language
stack. Its CMake manifest and compiler adapters still reference retired SDK files
and interfaces. This change does not restore a buildable Standalone executable;
historical CTest/package results do not prove the current tree.

The upstream runtime library packages and their dedicated execution operation
have been removed. Native compilation registers no bundled container, string or
math library. Its profile identity is `native-compiler-v2`, with host-provided
libraries only. The retained CLI parser rejects `run` with usage exit `2` and
`AS-NATIVE-UNSUPPORTED-LIBRARY` before creating an engine. There is no replacement
runtime library in this distribution.

The independent private frontend, offline bundle selection, UE analysis adapters,
resource validation and artifact contracts remain as source. Their implementation
boundaries below describe retained tooling, not current executable certification.

## Source distribution check

From the repository root:

```powershell
python Plugins/Angelscript/AngelscriptLSP/Tests/CheckRemovedAddons.py
python Plugins/Angelscript/AngelscriptLSP/Tests/CheckRemovedAddons.py --self-test
```

This read-only audit proves package/dependency absence. It excludes explicit
generated build/output directories and its own exact file; it has no historical
source/document exclusions. Negative controls introduce a removed include and a
package into temporary fixtures; ordinary host and C++ library names remain valid.
Active plugin builds and Automation tests use Harness for the selected workspace.

## Retained compiler command shapes

Analyze UE AngelScript with the packaged default contract:

```powershell
as-standalone compile --dialect ue --script-root Examples/UEValidation --entry basic.as --output out/ue --diagnostics text --emit-bytecode
```

Analyze against a complete project bundle exported from the matching Unreal
project:

```powershell
as-standalone compile --dialect ue --script-root Script --entry Examples/Core/Example_Array.as --bundle D:/Exports/MyProjectAS --output out/project --diagnostics json --emit-bytecode --allow-ue-required
```

An explicit `--bundle` replaces the packaged default in full. Bundles are
never merged, no cache or environment variable is searched, and an invalid
explicit bundle fails with infrastructure exit code `2` without falling back.

For UE validation, `--script-root` is the project `Script/` root. Relative
logical source paths map to UE virtual identities below `/Angelscript/Game/`.
For example, `Foo/Bar.as` uses `/Angelscript/Game/Foo/Bar.as`; the offline
module ID is SHA-256 over `module-id-v1`, the logical module name, and that
virtual identity. This is distinct from the private Standalone frontend's
internal source identity and allows exact replacement of the same module's
exported script baseline. That internal identity is not a UE/Standalone ABI.
V1 does not guess plugin or memory mounts from arbitrary filesystem roots.

## UE-validation profile boundary

The packaged `contracts/default-engine/` bundle is the complete snapshot
exported from the checked-in Unreal Engine 5.8 `AngelscriptProject` host and
its normally enabled plugins. It contains that host's project, Blueprint,
optional-plugin, active script-baseline, and declared `/Game` asset scope.
`default-engine` describes its packaged selection role; it does not mean
engine-only or minimal-host. Use a matching explicit project bundle whenever
the target project's exact surface differs. Both bundle kinds are complete
independent snapshots, not deltas.

The source distribution stores this large contract compressed at
`Contracts/UE5.8/default-engine.zip`. CMake extracts and verifies fixed
per-file SHA-256 values before placing the normal three-file directory beside
the executable or in an installed package. The CLI never reads ZIP files at
runtime.

Imported callables and adapter behaviors are registered with a compile-only
generic trap. Static analysis may type-check them; execution is forbidden.
Artifact metadata marks the profile `ue-validation`, `ueValidationOnly: true`,
and template layouts `non-ue-abi`.

Resource analysis recognizes typed soft object/class paths, reviewed
load-like contexts, and parameter-level resource semantics exported in the
selected bundle. Parameter markers are consumed only after the compiler has
resolved the owning callable to its stable symbol ID and supplied the exact
argument span; a same-named function or unmarked string is ignored. It
normalizes paths and reports:

| State | Default meaning |
| --- | --- |
| `found` | Path and requested type are proven by the selected complete scope. |
| `redirected` | A recorded redirect reaches a compatible final asset. |
| `missing` | An authoritative included scope proves absence; soft contexts warn and hard contexts fail. |
| `incompatible` | The asset exists but is not assignable to the requested type. |
| `unknown` | The bundle scope or hierarchy cannot decide; never promoted to a fabricated missing result. |

`--strict-resources` promotes authoritative soft missing only. It does not turn
unknown scope into missing. Standalone never loads, resolves, cooks, or
executes an asset.

## Output and exit contract

Compile output is replaced as one artifact directory and contains
`result.json`, `diagnostics.jsonl`, and optional module `.asbc` files.
UE-validation also writes `.classes.jsonl`. Identity fields are canonical and
deterministic; elapsed time and process-local runtime IDs are not compile
identity.

- `0`: requested compilation completed successfully
- `1`: source validation or compilation failed
- `2`: usage, bundle, I/O, schema, integrity, or other infrastructure failure

Use `as-standalone --version` for product, upstream lineage and profile/schema
versions. See `Schemas/` for the shipped machine-readable contracts and
`SUPPORT_MATRIX.md` for the evidence-scoped support statement.

## Licenses and provenance

Package license files live under `LICENSES/`. The maintained engine sources
remain under the plugin runtime rather than being copied into this directory.
Retained third-party distribution information is in `ThirdParty/README.md`.
Historical removed-library provenance remains available in Git history.
