import copy
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOL_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOL_DIR))


def source(provider="Fixture.Provider", line=12):
    return {"ownerModule": "Fixture", "provider": provider, "file": "Fixture_绑定.cpp", "line": line, "phase": "explicit-bindings"}


def member(owner, kind, declaration, type_ref="int", category="builtin", **extra):
    value = {
        "identity": f"{owner}|{kind}|{declaration}", "owner": owner, "kind": kind,
        "declaration": declaration, "typeReference": type_ref, "typeCategory": category,
        "propertyOffset": "0", "enumValue": "", "constantValue": "", "storage": "none",
        "nativeTarget": "native-callable" if "function" in kind or kind == "method" else "none",
        "functionLifetime": "borrowed-host", "auxiliaryLifetime": "none", "traits": "0",
        "firstParameter": "none", "compileOut": "none", "canRead": True, "canWrite": True,
        "canEdit": True, "protected": False, "deprecationMessage": "", "documentation": "",
        "category": "", "recipes": [], "source": source(),
    }
    value.update(extra)
    return value


def type_row(identity, kind="value", base="", interfaces=None, members=None, size="8", alignment="4"):
    namespace, name = identity.rsplit("::", 1) if "::" in identity else ("", identity)
    return {
        "identity": identity, "name": name, "namespace": namespace, "kind": kind,
        "base": base, "interfaces": interfaces or [], "aliasTarget": "", "templateType": "",
        "templateDeclaration": "", "nativeSize": size, "nativeAlignment": alignment,
        "objectFlags": "0", "sources": [source()], "members": members or [],
    }


def valid_manifest(scope="full-runtime"):
    pair_members = [member("Pair", "property", "int X"), member("Pair", "method", "int Sum() const")]
    add = member("namespace:Test", "global-function", "int Add(int A, int B = 1)")
    answer = member("namespace:Test", "global-property", "const int Answer", constantValue="7", storage="copied-record", nativeTarget="copied-constant")
    # Namespace scopes use a distinct identity domain so they cannot collide with nominal types.
    namespace = type_row("namespace:Test", kind="namespace", size="0", alignment="0", members=[add, answer])
    namespace["name"] = ""; namespace["namespace"] = "Test"
    return {
        "schemaVersion": 1, "scope": scope, "stage": "sealed-store", "collectionOnly": False,
        "sealed": True, "recordingFailed": False, "validationAttempted": True, "valid": True,
        "diagnostic": "", "policy": {"editor": True, "test": False, "logging": True,
        "shipping": False, "simulateCooked": False, "forceConstWithinDevelopmentOnlyFunctions": False, "platform": "Win64"},
        "providers": [{**source(), "eligible": True, "disposition": "recorded"}],
        "types": [type_row("Base"), type_row("Pair", members=pair_members),
                  type_row("Derived", kind="object-handle", base="Base", size="8", alignment="8"), namespace],
        "reflection": [{"nativePath": "/Script/Test.EWide", "kind": "enum", "intendedIdentity": "EWide",
                        "eligible": True, "exclusionReason": "", "parentPath": "", "interfaces": [],
                        "properties": [], "hasGCReferences": False, "needsDestruction": False,
                        "lifetimeRecipes": [], "enumValues": [{"name": "Small", "value": "-3"},
                                                                {"name": "Large", "value": "1234567890123"}]}],
    }


class BindingInspectionTests(unittest.TestCase):
    def setUp(self):
        from validate_bindings import validate_manifest
        from diff_bindings import compare_manifests
        self.validate = validate_manifest
        self.compare = compare_manifests

    def test_hand_authored_pair_derived_globals_and_wide_enum_pass(self):
        data = valid_manifest()
        data["types"][3]["members"].append(member("namespace:Test", "global-function", "int Add(float A, float B)"))
        self.assertEqual([], self.validate(data))

    def test_missing_base_and_dangling_member_provider_are_structured(self):
        data = valid_manifest(); data["types"] = [row for row in data["types"] if row["identity"] != "Base"]
        data["types"][0]["members"][0]["source"]["provider"] = "Missing.Provider"
        messages = "\n".join(item["message"] for item in self.validate(data))
        self.assertIn("Derived", messages); self.assertIn("Base", messages); self.assertIn("Missing.Provider", messages)

    def test_duplicate_identity_and_illegal_layout_bounds_fail(self):
        data = valid_manifest(); data["types"].append(copy.deepcopy(data["types"][0]))
        data["types"][1]["nativeAlignment"] = "3"; data["types"][1]["members"][0]["propertyOffset"] = "99"
        codes = {item["code"] for item in self.validate(data)}
        self.assertTrue({"duplicate-identity", "invalid-alignment", "property-out-of-bounds"} <= codes)

    def test_base_and_by_value_cycles_fail_but_mutual_handles_pass(self):
        data = valid_manifest(); data["types"][0]["base"] = "Derived"
        self.assertIn("base-cycle", {item["code"] for item in self.validate(data)})
        handles = valid_manifest(); handles["types"] += [type_row("A", kind="object-handle", members=[member("A", "property", "B@ Other", "B@", "nominal")], size="8", alignment="8"),
                                                        type_row("B", kind="object-handle", members=[member("B", "property", "A@ Other", "A@", "nominal")], size="8", alignment="8")]
        self.assertEqual([], self.validate(handles))
        values = valid_manifest(); values["types"] += [type_row("A", members=[member("A", "property", "B Other", "B", "nominal")]),
                                                       type_row("B", members=[member("B", "property", "A Other", "A", "nominal")])]
        self.assertIn("value-cycle", {item["code"] for item in self.validate(values)})

    def test_schema_enum_and_partial_scope_fail_precisely(self):
        data = valid_manifest(); data["schemaVersion"] = 99; data["reflection"][0]["enumValues"][1]["value"] = 1.5
        codes = {item["code"] for item in self.validate(data, require_full_runtime=True)}
        self.assertTrue({"unsupported-schema", "invalid-enum-value"} <= codes)
        partial = valid_manifest("collection-only"); partial["collectionOnly"] = True
        self.assertIn("full-runtime-required", {item["code"] for item in self.validate(partial, require_full_runtime=True)})

    def test_expectations_are_independent_required_forbidden_and_provider_rules(self):
        expectations = {"requiredSymbols": ["Pair", "Pair|property|int X", "Missing"],
                        "forbiddenSymbols": ["Derived"], "providerDispositions": {"Fixture.Provider": "excluded-by-condition"}}
        codes = {item["code"] for item in self.validate(valid_manifest(), expectations=expectations)}
        self.assertTrue({"required-symbol-missing", "forbidden-symbol-present", "provider-disposition"} <= codes)

    def test_diff_reports_added_removed_and_changed_semantics(self):
        before = valid_manifest(); after = copy.deepcopy(before)
        after["types"].append(type_row("Added")); after["types"][1]["members"] = after["types"][1]["members"][:1]
        after["types"][2]["base"] = "Pair"; after["types"][3]["members"][0]["declaration"] = "int Add(int A, int B = 2)"
        report = self.compare(before, after)
        kinds = {change["kind"] for change in report["changes"]}
        self.assertTrue({"type-added", "member-removed", "type-changed", "member-changed"} <= kinds)

    def test_diff_constant_and_recipe_order_change_but_cosmetic_order_does_not(self):
        before = valid_manifest(); after = copy.deepcopy(before)
        after["types"] = list(reversed(after["types"])); after["providers"] = list(reversed(after["providers"]))
        self.assertTrue(self.compare(before, after)["equal"])
        changed = copy.deepcopy(before); changed["types"][3]["members"][1]["constantValue"] = "8"
        changed["types"][1]["members"][1]["recipes"] = ["copy:a", "destroy:b"]
        self.assertFalse(self.compare(before, changed)["equal"])

    def test_cli_exit_codes_reports_unicode_and_malformed_input(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); good = root / "绑定.json"; bad = root / "bad.json"; report = root / "report.json"
            good.write_text(json.dumps(valid_manifest(), ensure_ascii=False), encoding="utf-8")
            bad.write_text("{broken", encoding="utf-8")
            valid_run = subprocess.run([sys.executable, str(TOOL_DIR / "validate_bindings.py"), str(good), "--report", str(report)], check=False)
            invalid_run = subprocess.run([sys.executable, str(TOOL_DIR / "validate_bindings.py"), str(bad)], check=False)
            diff_invalid = subprocess.run([sys.executable, str(TOOL_DIR / "diff_bindings.py"), str(good), str(bad)], check=False)
            self.assertEqual(0, valid_run.returncode); self.assertEqual(1, invalid_run.returncode); self.assertEqual(2, diff_invalid.returncode)
            self.assertTrue(report.exists())


if __name__ == "__main__":
    unittest.main()
