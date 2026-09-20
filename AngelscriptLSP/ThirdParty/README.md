# Standalone third-party sources

No upstream AngelScript runtime library packages are distributed here. Their
dedicated registrations, targets, execution operations and fixtures have been
removed; historical provenance is retained in Git history.

The engine source remains in the maintained plugin fork under
`Source/AngelscriptRuntime/ThirdParty/angelscript`. Standalone tooling is dormant
while the SDK is reconstructed; this distribution cleanup does not restore its
old compiler build path.

RapidJSON remains the independent JSON dependency under `RapidJSON/`; its license
is `RapidJSON/LICENSE.txt`. No runtime container or string implementation replaces
the removed packages.

## RapidJSON

- Upstream: `https://github.com/Tencent/rapidjson`
- Pinned release: `v1.1.0`
- Local source input: Unreal Engine 5.8
  `Engine/Source/ThirdParty/RapidJSON/1.1.0/rapidjson`
- Imported destination: `RapidJSON/include/rapidjson`
- License: MIT; retained verbatim as `RapidJSON/LICENSE.txt`

RapidJSON is header-only and is exposed only through the standalone CMake
host target. Unreal Build Tool modules do not include or link this vendored
copy. It parses the small manifest as a DOM and one JSONL record at a time;
the bundle loader never builds a DOM for an entire symbol or asset file.
