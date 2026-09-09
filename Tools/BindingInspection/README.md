# Binding inspection tools

These standard-library-only tools validate and compare the version 1 JSON emitted by `FAngelscriptBindingDump`. They do not launch Unreal Engine, load native addresses, or parse AngelScript declarations again. Canonical type categories and identities come from the exporter.

Validate a manifest and optionally apply independent expectations:

```text
python Plugins/Angelscript/Tools/BindingInspection/validate_bindings.py MANIFEST --expect EXPECTATIONS --report REPORT
```

`--expect` and `--report` are optional. An expectations object may contain `requiredSymbols`, `forbiddenSymbols`, `providerDispositions`, and `requireFullRuntime`. The command exits 0 for valid input and satisfied expectations, 1 for schema, semantic, or expectation diagnostics, and 2 for command-line usage errors.

Compare two compatible manifests:

```text
python Plugins/Angelscript/Tools/BindingInspection/diff_bindings.py BEFORE AFTER --report REPORT
```

`--report` is optional. The command exits 0 when the manifests are semantically equal, 1 when semantic changes exist, and 2 when either input is malformed, invalid, or incompatible. Type, member, provider, reflection, constant, modifier, default, and recipe changes are semantic. Array ordering used only for presentation is ignored; native/lifecycle recipe order is preserved.

Run the independent fixture suite with:

```text
python -m unittest discover -s Plugins/Angelscript/Tools/BindingInspection/tests -p "test_*.py"
```
