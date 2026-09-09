#!/usr/bin/env python3
"""Compare two valid, compatible detached AngelScript binding manifests."""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

from validate_bindings import load_json, validate_manifest


def _without_order_noise(value: dict[str, Any], *, member: bool = False) -> dict[str, Any]:
    result = dict(value)
    if member:
        # Recipe order is semantic. Source object field order is naturally ignored by dict equality.
        return result
    result["sources"] = sorted(result.get("sources", []), key=lambda row: json.dumps(row, sort_keys=True, ensure_ascii=False))
    result.pop("members", None)
    return result


def compare_manifests(before: dict[str, Any], after: dict[str, Any]) -> dict[str, Any]:
    changes: list[dict[str, Any]] = []
    before_types = {row["identity"]: row for row in before.get("types", []) if isinstance(row, dict) and "identity" in row}
    after_types = {row["identity"]: row for row in after.get("types", []) if isinstance(row, dict) and "identity" in row}
    for identity in sorted(before_types.keys() - after_types.keys()):
        changes.append({"kind": "type-removed", "symbol": identity})
    for identity in sorted(after_types.keys() - before_types.keys()):
        changes.append({"kind": "type-added", "symbol": identity})
    for identity in sorted(before_types.keys() & after_types.keys()):
        old, new = before_types[identity], after_types[identity]
        if _without_order_noise(old) != _without_order_noise(new):
            changes.append({"kind": "type-changed", "symbol": identity, "before": _without_order_noise(old), "after": _without_order_noise(new)})
        old_members = {row["identity"]: row for row in old.get("members", []) if isinstance(row, dict) and "identity" in row}
        new_members = {row["identity"]: row for row in new.get("members", []) if isinstance(row, dict) and "identity" in row}
        for symbol in sorted(old_members.keys() - new_members.keys()):
            changes.append({"kind": "member-removed", "symbol": symbol})
        for symbol in sorted(new_members.keys() - old_members.keys()):
            changes.append({"kind": "member-added", "symbol": symbol})
        for symbol in sorted(old_members.keys() & new_members.keys()):
            if _without_order_noise(old_members[symbol], member=True) != _without_order_noise(new_members[symbol], member=True):
                changes.append({"kind": "member-changed", "symbol": symbol,
                                "before": old_members[symbol], "after": new_members[symbol]})
    before_providers = {row.get("provider"): row for row in before.get("providers", []) if isinstance(row, dict)}
    after_providers = {row.get("provider"): row for row in after.get("providers", []) if isinstance(row, dict)}
    if before_providers != after_providers:
        changes.append({"kind": "providers-changed", "symbol": "providers", "before": before_providers, "after": after_providers})
    before_reflection = {row.get("nativePath"): row for row in before.get("reflection", []) if isinstance(row, dict)}
    after_reflection = {row.get("nativePath"): row for row in after.get("reflection", []) if isinstance(row, dict)}
    if before_reflection != after_reflection:
        changes.append({"kind": "reflection-changed", "symbol": "reflection"})
    return {"schemaVersion": 1, "equal": not changes, "changes": changes}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("before"); parser.add_argument("after"); parser.add_argument("--report")
    args = parser.parse_args(argv)
    try:
        before, after = load_json(args.before), load_json(args.after)
        before_errors, after_errors = validate_manifest(before), validate_manifest(after)
        if before_errors or after_errors or before.get("schemaVersion") != after.get("schemaVersion"):
            report = {"schemaVersion": 1, "compatible": False,
                      "beforeDiagnostics": before_errors, "afterDiagnostics": after_errors}
            exit_code = 2
        else:
            report = compare_manifests(before, after); report["compatible"] = True
            exit_code = 0 if report["equal"] else 1
    except (OSError, UnicodeError, json.JSONDecodeError, AttributeError) as exc:
        report = {"schemaVersion": 1, "compatible": False,
                  "diagnostics": [{"code": "invalid-input", "message": str(exc)}]}; exit_code = 2
    text = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
    if args.report:
        try:
            Path(args.report).write_text(text, encoding="utf-8")
        except OSError as exc:
            print(f"cannot write report: {exc}", file=sys.stderr); return 2
    print(text, end="")
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
