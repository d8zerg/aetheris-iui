"""
Aetheris-IUI Evaluation Harness.

Reads a JSONL golden dataset and evaluates Action Schema validity against
expected outcomes.  Can also be used for replay (comparing output of two
tool versions on the same dataset) and drift detection.

Usage:
    python3 runner.py --dataset datasets/golden.jsonl --lint-tool /path/to/aetheris-lint
    python3 runner.py --dataset datasets/golden.jsonl --cli /path/to/aetheris

Exit codes:
    0  all cases pass
    1  one or more cases fail
    2  configuration error
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


@dataclass
class EvalCase:
    id: str
    description: str
    schema_json: Any
    expected_valid: bool


@dataclass
class EvalResult:
    case_id: str
    description: str
    passed: bool
    actual_valid: bool
    expected_valid: bool
    detail: str = ""


def load_dataset(path: Path) -> list[EvalCase]:
    cases: list[EvalCase] = []
    for i, line in enumerate(path.read_text().splitlines(), start=1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        try:
            doc = json.loads(line)
            cases.append(EvalCase(
                id=doc["id"],
                description=doc.get("description", ""),
                schema_json=doc["schema_json"],
                expected_valid=doc.get("expected_valid", True),
            ))
        except (json.JSONDecodeError, KeyError) as exc:
            print(f"  warn  line {i}: {exc}", file=sys.stderr)
    return cases


def run_lint(schema_json: Any, lint_tool: str) -> tuple[bool, str]:
    """Run the lint tool on a schema; return (is_valid, stdout+stderr)."""
    text = json.dumps(schema_json)
    try:
        result = subprocess.run(
            [lint_tool],
            input=text,
            capture_output=True,
            text=True,
            timeout=10,
        )
        return result.returncode == 0, (result.stdout + result.stderr).strip()
    except FileNotFoundError:
        raise RuntimeError(f"lint tool not found: {lint_tool}")
    except subprocess.TimeoutExpired:
        return False, "timeout"


def run_cli_schema_lint(schema_json: Any, cli: str) -> tuple[bool, str]:
    """Run 'aetheris schema lint' on a schema."""
    text = json.dumps(schema_json)
    try:
        result = subprocess.run(
            [cli, "schema", "lint"],
            input=text,
            capture_output=True,
            text=True,
            timeout=10,
        )
        return result.returncode == 0, (result.stdout + result.stderr).strip()
    except FileNotFoundError:
        raise RuntimeError(f"CLI not found: {cli}")
    except subprocess.TimeoutExpired:
        return False, "timeout"


def evaluate(cases: list[EvalCase], tool: str, use_cli: bool) -> list[EvalResult]:
    results: list[EvalResult] = []
    for case in cases:
        if use_cli:
            actual_valid, detail = run_cli_schema_lint(case.schema_json, tool)
        else:
            actual_valid, detail = run_lint(case.schema_json, tool)
        passed = actual_valid == case.expected_valid
        results.append(EvalResult(
            case_id=case.id,
            description=case.description,
            passed=passed,
            actual_valid=actual_valid,
            expected_valid=case.expected_valid,
            detail=detail,
        ))
    return results


def print_report(results: list[EvalResult]) -> int:
    passed = sum(1 for r in results if r.passed)
    failed = len(results) - passed

    for r in results:
        status = "  ok  " if r.passed else " FAIL "
        print(f"{status}[{r.case_id}] {r.description}")
        if not r.passed:
            print(f"        expected valid={r.expected_valid}, got valid={r.actual_valid}")
            if r.detail:
                for line in r.detail.splitlines()[:3]:
                    print(f"        {line}")

    print(f"\n{passed}/{len(results)} eval cases passed")
    return 1 if failed else 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Aetheris-IUI evaluation harness")
    parser.add_argument("--dataset", required=True, type=Path,
                        help="JSONL golden dataset file")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--lint-tool", metavar="PATH",
                       help="Path to aetheris-lint binary")
    group.add_argument("--cli", metavar="PATH",
                       help="Path to unified aetheris CLI binary")
    args = parser.parse_args()

    if not args.dataset.exists():
        print(f"error: dataset not found: {args.dataset}", file=sys.stderr)
        return 2

    try:
        cases = load_dataset(args.dataset)
        if not cases:
            print("error: dataset is empty", file=sys.stderr)
            return 2

        tool = args.cli or args.lint_tool
        use_cli = args.cli is not None
        results = evaluate(cases, tool, use_cli)
        return print_report(results)
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
