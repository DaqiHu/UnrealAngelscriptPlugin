#!/usr/bin/env python3
"""Audit removed upstream packages and their dedicated Standalone consumers.

This is a read-only source/distribution audit, not a Standalone build gate.
Generated build/output directories below are excluded explicitly. There are no
historical source/document exclusions in this tree. Only this audit's actual
file is self-excluded; another file with the same basename is still checked.
Generic host array/string/math names and standard-library headers are allowed.
"""
from __future__ import annotations
import argparse
import os
from pathlib import Path
import re
import tempfile

PACKAGES = ("scriptarray", "scriptdictionary", "scriptmath", "scriptstdstring")
PACKAGE_ROOT = Path("ThirdParty/AngelScriptAddons")
IGNORED_DIRS = frozenset({".git", ".vs", "__pycache__", "build", "Build", "out", "output", "Saved", "Intermediate", "Binaries", "CMakeFiles", "Testing", "as-standalone-output"})
IGNORED_PREFIXES = ("cmake-build-", "build-")
HISTORICAL_PREFIXES: tuple[Path, ...] = ()
TEXT_SUFFIXES = frozenset({".c", ".cc", ".cpp", ".h", ".hpp", ".in", ".cmake", ".txt", ".md", ".json", ".as", ".py", ".ps1", ".psm1", ".psd1", ".bat", ".ini", ".toml", ".yml", ".yaml", ".html"})
FORBIDDEN = re.compile(
    r"\b(?:scriptarray|scriptdictionary|scriptmath|scriptstdstring|scriptstring_t|CScriptArray|CScriptDictionary|"
    r"RegisterScriptArray|RegisterScriptDictionary|RegisterScriptMath|RegisterStdString|RegisterStdStringUtils|"
    r"RegisterNativeStandardLibrary|FStandardLibraryOptions|ANGELSCRIPT_ADDON_ROOT|ANGELSCRIPT_STANDALONE_ADDON_SOURCES|"
    r"AngelscriptStandaloneAddons(?:Tests)?|AngelscriptStandaloneStdLib|RunNativeScript|FRunRequest)\b"
    r"|ThirdParty[/\\]AngelScriptAddons|(?:string|array|dictionary|math)-addon\b",
    re.IGNORECASE,
)

def audit(root: Path) -> tuple[list[str], int]:
    root = root.resolve()
    failures = []
    for package in PACKAGES:
        if (root / PACKAGE_ROOT / package).exists():
            failures.append(f"{PACKAGE_ROOT / package}: removed package still exists")
    inspected = 0
    for directory, names, files in os.walk(root):
        here = Path(directory)
        names[:] = sorted(n for n in names if n not in IGNORED_DIRS and not n.startswith(IGNORED_PREFIXES))
        relative_dir = here.relative_to(root)
        # Presence was checked above; scanning removed vendor implementations adds no evidence.
        if relative_dir == PACKAGE_ROOT or PACKAGE_ROOT in relative_dir.parents:
            names[:] = []
            continue
        if any(relative_dir == h or h in relative_dir.parents for h in HISTORICAL_PREFIXES):
            names[:] = []
            continue
        for filename in sorted(files):
            path = here / filename
            if path.suffix.lower() not in TEXT_SUFFIXES or path.resolve() == Path(__file__).resolve():
                continue
            relative = path.relative_to(root)
            try:
                source = path.read_text(encoding="utf-8-sig")
            except (UnicodeError, OSError) as error:
                failures.append(f"{relative}: cannot inspect text: {error}")
                continue
            inspected += 1
            for line_number, line in enumerate(source.splitlines(), 1):
                found = FORBIDDEN.search(line)
                if found:
                    failures.append(f"{relative}:{line_number}: removed dependency {found.group(0)}")
    return failures, inspected

def self_check() -> None:
    with tempfile.TemporaryDirectory(prefix="angelscript-removed-addons-") as temporary:
        root = Path(temporary).resolve()
        assert root.parent == Path(tempfile.gettempdir()).resolve()
        source = root / "Source"
        source.mkdir()
        (source / "Host.cpp").write_text("#include <string>\n#include <cmath>\nTArray<FString> Names; std::string Name;\n", encoding="utf-8")
        assert not audit(root)[0], "ordinary host/standard-library names must pass"
        bad = source / "Injected.cpp"
        bad.write_text('#include "scriptarray/scriptarray.h"\n', encoding="utf-8")
        assert any("Injected.cpp" in failure for failure in audit(root)[0]), "removed include must fail without vendor packages"
        bad.unlink()
        package = root / PACKAGE_ROOT / PACKAGES[0]
        package.mkdir(parents=True)
        assert any("removed package still exists" in failure for failure in audit(root)[0]), "reintroduced package must fail"
        package.rmdir()
        ignored = root / "build" / "generated.cpp"
        ignored.parent.mkdir()
        ignored.write_text('#include "scriptarray/scriptarray.h"\n', encoding="utf-8")
        assert not audit(root)[0], "explicit generated output exclusion must apply"
        fake_self = root / "Tests" / Path(__file__).name
        fake_self.parent.mkdir()
        fake_self.write_text('#include "scriptdictionary/scriptdictionary.h"\n', encoding="utf-8")
        assert any(str(fake_self.relative_to(root)) in failure for failure in audit(root)[0]), "self-exclusion must not hide another file"
    print("Audit self-controls: PASS (injected include, package return, host names, output exclusion, exact self-exclusion)")

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--self-test", action="store_true", help="run controls without auditing the repository")
    args = parser.parse_args()
    self_check()
    if args.self_test:
        return 0
    failures, inspected = audit(args.root)
    for failure in failures:
        print(failure)
    print(f"Removed add-ons audit: {'FAIL' if failures else 'PASS'}; {inspected} text files inspected; {len(failures)} violations")
    return 1 if failures else 0

if __name__ == "__main__":
    raise SystemExit(main())
