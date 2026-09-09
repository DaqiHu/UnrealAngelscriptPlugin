#!/usr/bin/env python3
"""Validate version 1 detached AngelScript binding manifests."""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

SCHEMA_VERSION = 1
SCOPES = {"collection-only", "selected-family", "full-runtime"}
TYPE_KINDS = {"value", "object-handle", "enum", "interface", "template", "namespace", "alias"}
MEMBER_KINDS = {"constructor", "destructor", "method", "property", "behaviour", "global-function", "global-property", "enum-value"}
EXTERNAL_CATEGORIES = {"builtin", "template-parameter", "external", "opaque"}


def diagnostic(code: str, message: str, symbol: str = "", source: dict[str, Any] | None = None) -> dict[str, Any]:
    result: dict[str, Any] = {"code": code, "message": message}
    if symbol:
        result["symbol"] = symbol
    if source:
        result["source"] = {key: source.get(key) for key in ("provider", "file", "line")}
    return result


def _integer(value: Any) -> int | None:
    if isinstance(value, bool):
        return None
    try:
        text = str(value)
        if not text or any(character in text for character in ".eE"):
            return None
        return int(text, 10)
    except (TypeError, ValueError):
        return None


def _required_object(value: Any, field: str, errors: list[dict[str, Any]]) -> dict[str, Any]:
    if isinstance(value, dict):
        return value
    errors.append(diagnostic("invalid-schema", f"'{field}' must be an object."))
    return {}


def _required_list(value: Any, field: str, errors: list[dict[str, Any]]) -> list[Any]:
    if isinstance(value, list):
        return value
    errors.append(diagnostic("invalid-schema", f"'{field}' must be an array."))
    return []


def _cycles(edges: dict[str, set[str]]) -> list[list[str]]:
    found: list[list[str]] = []
    visiting: list[str] = []
    visited: set[str] = set()

    def visit(node: str) -> None:
        if node in visiting:
            cycle = visiting[visiting.index(node):] + [node]
            if cycle not in found:
                found.append(cycle)
            return
        if node in visited:
            return
        visiting.append(node)
        for target in sorted(edges.get(node, ())):
            visit(target)
        visiting.pop()
        visited.add(node)

    for node in sorted(edges):
        visit(node)
    return found


def symbols(manifest: dict[str, Any]) -> set[str]:
    result: set[str] = set()
    for row in manifest.get("types", []):
        if not isinstance(row, dict):
            continue
        identity = row.get("identity")
        if isinstance(identity, str):
            result.add(identity)
        for member in row.get("members", []):
            if isinstance(member, dict) and isinstance(member.get("identity"), str):
                result.add(member["identity"])
    return result


def validate_manifest(manifest: Any, expectations: dict[str, Any] | None = None,
                      require_full_runtime: bool = False) -> list[dict[str, Any]]:
    errors: list[dict[str, Any]] = []
    if not isinstance(manifest, dict):
        return [diagnostic("invalid-schema", "Manifest root must be an object.")]
    if manifest.get("schemaVersion") != SCHEMA_VERSION:
        errors.append(diagnostic("unsupported-schema", f"schemaVersion must be {SCHEMA_VERSION}."))
    scope = manifest.get("scope")
    if scope not in SCOPES:
        errors.append(diagnostic("invalid-scope", f"Unknown manifest scope '{scope}'."))
    if require_full_runtime and (scope != "full-runtime" or manifest.get("collectionOnly") is True):
        errors.append(diagnostic("full-runtime-required", "Full-runtime expectations require a full-runtime Store manifest."))
    for field in ("stage", "sealed", "recordingFailed", "validationAttempted", "valid", "diagnostic"):
        if field not in manifest:
            errors.append(diagnostic("invalid-schema", f"Missing required field '{field}'."))
    _required_object(manifest.get("policy"), "policy", errors)
    providers = _required_list(manifest.get("providers"), "providers", errors)
    types = _required_list(manifest.get("types"), "types", errors)
    reflection = _required_list(manifest.get("reflection"), "reflection", errors)

    provider_map: dict[str, str] = {}
    for row in providers:
        if not isinstance(row, dict):
            errors.append(diagnostic("invalid-provider", "Provider row must be an object.")); continue
        name = row.get("provider"); disposition = row.get("disposition")
        if not isinstance(name, str) or not name or not isinstance(row.get("file"), str) or _integer(row.get("line")) is None or _integer(row.get("line")) <= 0:
            errors.append(diagnostic("invalid-provider", "Provider identity requires provider, file and positive line.", str(name or ""), row)); continue
        if name in provider_map:
            errors.append(diagnostic("duplicate-provider", f"Duplicate provider '{name}'.", name, row))
        provider_map[name] = str(disposition)

    type_map: dict[str, dict[str, Any]] = {}
    member_map: dict[str, dict[str, Any]] = {}
    base_edges: dict[str, set[str]] = {}
    value_edges: dict[str, set[str]] = {}
    for row in types:
        if not isinstance(row, dict):
            errors.append(diagnostic("invalid-type", "Type row must be an object.")); continue
        identity = row.get("identity"); kind = row.get("kind")
        source = (row.get("sources") or [{}])[0] if isinstance(row.get("sources"), list) else {}
        if not isinstance(identity, str) or not identity:
            errors.append(diagnostic("invalid-type", "Type identity must be a non-empty string.", source=source)); continue
        if identity in type_map:
            errors.append(diagnostic("duplicate-identity", f"Duplicate type identity '{identity}'.", identity, source))
        type_map[identity] = row
        expected_identity = ((f"namespace:{row.get('namespace')}" if row.get("namespace") else "namespace:<root>") if kind == "namespace" and not row.get("name")
                             else (f"{row.get('namespace')}::{row.get('name')}" if row.get("namespace") else row.get("name")))
        if identity != expected_identity:
            errors.append(diagnostic("noncanonical-identity", f"Type identity '{identity}' must equal '{expected_identity}'.", identity, source))
        if kind not in TYPE_KINDS:
            errors.append(diagnostic("invalid-type-kind", f"Type '{identity}' has unknown kind '{kind}'.", identity, source))
        size, alignment = _integer(row.get("nativeSize")), _integer(row.get("nativeAlignment"))
        if size is None or size < 0:
            errors.append(diagnostic("invalid-layout", f"Type '{identity}' has invalid nativeSize.", identity, source))
            size = 0
        if alignment is None or alignment < 0 or (alignment and alignment & (alignment - 1)):
            errors.append(diagnostic("invalid-alignment", f"Type '{identity}' alignment must be zero or a positive power of two.", identity, source))
        base = row.get("base", "")
        if isinstance(base, str) and base:
            base_edges.setdefault(identity, set()).add(base)
        for reference in row.get("interfaces", []):
            if isinstance(reference, str):
                base_edges.setdefault(identity, set()).add(reference)
        alias = row.get("aliasTarget", "")
        if kind == "alias" and isinstance(alias, str) and alias and "@" not in alias:
            value_edges.setdefault(identity, set()).add(alias)
        for source_row in row.get("sources", []):
            provider = source_row.get("provider") if isinstance(source_row, dict) else None
            if provider not in provider_map:
                errors.append(diagnostic("dangling-provider", f"Type '{identity}' references missing provider '{provider}'.", identity, source_row if isinstance(source_row, dict) else None))
        for member_row in row.get("members", []):
            if not isinstance(member_row, dict):
                errors.append(diagnostic("invalid-member", f"Type '{identity}' contains a non-object member.", identity, source)); continue
            member_identity = member_row.get("identity"); member_source = member_row.get("source") if isinstance(member_row.get("source"), dict) else {}
            if not isinstance(member_identity, str) or not member_identity:
                errors.append(diagnostic("invalid-member", f"Type '{identity}' has a member without identity.", identity, member_source)); continue
            if member_identity in member_map:
                errors.append(diagnostic("duplicate-identity", f"Duplicate member identity '{member_identity}'.", member_identity, member_source))
            member_map[member_identity] = member_row
            canonical_identity = f"{identity}|{member_row.get('kind')}|{member_row.get('declaration')}"
            if member_identity != canonical_identity:
                errors.append(diagnostic("noncanonical-identity", f"Member identity '{member_identity}' must equal '{canonical_identity}'.", member_identity, member_source))
            if member_row.get("owner") != identity or member_row.get("kind") not in MEMBER_KINDS or not isinstance(member_row.get("declaration"), str):
                errors.append(diagnostic("invalid-member", f"Member '{member_identity}' has invalid owner, kind or declaration.", member_identity, member_source))
            if member_source.get("provider") not in provider_map:
                errors.append(diagnostic("dangling-provider", f"Member '{member_identity}' references missing provider '{member_source.get('provider')}'.", member_identity, member_source))
            offset = _integer(member_row.get("propertyOffset", "0"))
            if offset is None or offset < 0:
                errors.append(diagnostic("invalid-property-offset", f"Member '{member_identity}' has invalid property offset.", member_identity, member_source))
            elif member_row.get("kind") == "property" and size is not None and offset >= size:
                errors.append(diagnostic("property-out-of-bounds", f"Property '{member_identity}' offset {offset} is outside type size {size}.", member_identity, member_source))
            reference, category = member_row.get("typeReference", ""), member_row.get("typeCategory", "")
            if isinstance(reference, str) and reference and category not in EXTERNAL_CATEGORIES:
                nominal = reference.replace("const", "").replace("&", "").replace("@", "").strip()
                if nominal and nominal not in type_map:
                    # Forward references are checked once the complete map has been built below.
                    pass
                if member_row.get("kind") == "property" and "@" not in reference:
                    value_edges.setdefault(identity, set()).add(nominal)

    for identity, row in type_map.items():
        source = (row.get("sources") or [{}])[0]
        for reference in [row.get("base", ""), *row.get("interfaces", [])]:
            if reference and reference not in type_map:
                errors.append(diagnostic("dangling-type-reference", f"Type '{identity}' references missing nominal '{reference}'.", identity, source))
        for member_row in row.get("members", []):
            if not isinstance(member_row, dict):
                continue
            reference, category = member_row.get("typeReference", ""), member_row.get("typeCategory", "")
            nominal = str(reference).replace("const", "").replace("&", "").replace("@", "").strip()
            if reference and category == "template-instance":
                template = nominal.split("<", 1)[0].strip()
                if template not in type_map:
                    errors.append(diagnostic("dangling-type-reference", f"Member '{member_row.get('identity')}' references missing template '{template}'.", str(member_row.get("identity")), member_row.get("source")))
            elif reference and category not in EXTERNAL_CATEGORIES and nominal not in type_map:
                errors.append(diagnostic("dangling-type-reference", f"Member '{member_row.get('identity')}' references missing nominal '{nominal}'.", str(member_row.get("identity")), member_row.get("source")))
    for cycle in _cycles(base_edges):
        errors.append(diagnostic("base-cycle", "Base/interface cycle: " + " -> ".join(cycle), cycle[0]))
    value_edges = {owner: {target for target in targets if target in type_map} for owner, targets in value_edges.items()}
    for cycle in _cycles(value_edges):
        errors.append(diagnostic("value-cycle", "By-value cycle: " + " -> ".join(cycle), cycle[0]))

    for row in reflection:
        if not isinstance(row, dict):
            errors.append(diagnostic("invalid-reflection", "Reflection row must be an object.")); continue
        native_path = str(row.get("nativePath", ""))
        for enum_value in row.get("enumValues", []):
            if not isinstance(enum_value, dict) or _integer(enum_value.get("value")) is None:
                errors.append(diagnostic("invalid-enum-value", f"Reflection enum '{native_path}' has a non-decimal int64 value.", native_path))

    if expectations is not None:
        if not isinstance(expectations, dict):
            errors.append(diagnostic("invalid-expectations", "Expectations root must be an object."))
        else:
            available = symbols(manifest)
            for required in expectations.get("requiredSymbols", []):
                if required not in available:
                    errors.append(diagnostic("required-symbol-missing", f"Required symbol '{required}' is missing.", str(required)))
            for forbidden in expectations.get("forbiddenSymbols", []):
                if forbidden in available:
                    errors.append(diagnostic("forbidden-symbol-present", f"Forbidden symbol '{forbidden}' is present.", str(forbidden)))
            for provider, disposition in expectations.get("providerDispositions", {}).items():
                if provider_map.get(provider) != disposition:
                    errors.append(diagnostic("provider-disposition", f"Provider '{provider}' disposition is '{provider_map.get(provider)}', expected '{disposition}'.", provider))
            if expectations.get("requireFullRuntime") is True and (scope != "full-runtime" or manifest.get("collectionOnly") is True):
                errors.append(diagnostic("full-runtime-required", "Expectations require a full-runtime Store manifest."))
    return errors


def load_json(path: str | Path) -> Any:
    with Path(path).open("r", encoding="utf-8-sig") as handle:
        return json.load(handle)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest"); parser.add_argument("--expect"); parser.add_argument("--report")
    args = parser.parse_args(argv)
    try:
        manifest = load_json(args.manifest)
        expectations = load_json(args.expect) if args.expect else None
        errors = validate_manifest(manifest, expectations, bool(expectations and expectations.get("requireFullRuntime")))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        errors = [diagnostic("invalid-input", str(exc))]
    report = {"schemaVersion": 1, "valid": not errors, "diagnostics": errors}
    text = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
    if args.report:
        try:
            Path(args.report).write_text(text, encoding="utf-8")
        except OSError as exc:
            print(f"cannot write report: {exc}", file=sys.stderr); return 1
    print(text, end="")
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
