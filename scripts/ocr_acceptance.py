"""Run a frozen OCR candidate on a new reviewed batch, with exclusive outputs.

No device operations or tuning. Capture and blind annotation happen separately.
"""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
from types import SimpleNamespace
from ocr_corpus import read_jsonl, safe_image, validate
from ocr_report import b1b_scores


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_freeze(root, manifest):
    for name, expected in manifest["files"].items():
        path = (root / name).resolve()
        if not path.is_relative_to(root.resolve()) or not path.is_file() or sha(path) != expected:
            raise ValueError("Frozen asset changed or missing: " + name)


def prepare(root, manifest, annotations):
    verify_freeze(root, manifest)
    rows = read_jsonl(annotations)
    coverage = validate(annotations, minimum_ui=60, minimum_numbers=40)
    if coverage["record_errors"]:
        raise ValueError("Invalid annotation: " + "; ".join(coverage["record_errors"][:5]))
    for language in ("ja", "zh-Hant"):
        for kind, minimum in (("ui", 60), ("numbers", 40)):
            if sum(r["language"] == language and r["kind"] == kind and r["split"] == "test" for r in rows) < minimum:
                raise ValueError(f"New batch needs {language} {kind} >= {minimum}")
    if any(r["split"] != "test" for r in rows) or coverage["audited_records"] < 20:
        raise ValueError("New batch must be test-only and have approximately 20 independent audits (default minimum 20)")
    reference = read_jsonl(root / manifest["reference_annotations"])
    old_hashes = {r["image_sha256"] for r in reference}
    frozen_ms = datetime.datetime.fromisoformat(manifest["frozen_at"]).timestamp() * 1000
    captures = {}
    for row in rows:
        if row["image_sha256"] in old_hashes:
            raise ValueError("Exact old frame reused: " + row["id"])
        image = safe_image(annotations.parent, row["image"])
        capture = image.parent.parent / "capture.jsonl"
        if capture not in captures:
            captures[capture] = {str((capture.parent / r["image"]).resolve()): r for r in read_jsonl(capture)}
        source = captures[capture].get(str(image))
        if not source or source["source"] != "OpenAR/MuMu/native" or source["language"] != row["language"] or source["utc_unix_ms"] < frozen_ms:
            raise ValueError("Frame was not captured by the native tool after freeze: " + row["id"])
    return rows, reference, coverage


if __name__ == "__main__":
    from ocr_benchmark import benchmark
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--freeze", type=Path, required=True)
    parser.add_argument("--annotations", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    manifest = json.loads(args.freeze.read_text(encoding="utf-8"))
    rows, reference, coverage = prepare(root, manifest, args.annotations)
    annotation_sha = sha(args.annotations)
    args.output_dir.mkdir(parents=True, exist_ok=False)
    run = {"freeze_sha256": sha(args.freeze), "annotations_sha256": annotation_sha,
           "role": "new independent acceptance; no post-result tuning", "coverage": coverage, "results": {}}
    (args.output_dir / "run-input.json").write_text(json.dumps(run, ensure_ascii=False, indent=2), encoding="utf-8")
    for lang in ("ja", "zh-Hant"):
        options = SimpleNamespace(annotations=args.annotations, profile=root / manifest["profiles"][lang],
                                  plugin=root / "out/cpu/Release/plugins/OcrPaddleCpu.dll",
                                  allow_incomplete=True, split="test", kind="all", warmup=2, repeats=5, skip_detection=False)
        report = benchmark(options)
        with (args.output_dir / f"{lang}-cpu.json").open("x", encoding="utf-8") as file:
            json.dump(report, file, ensure_ascii=False, indent=2)
        verify_freeze(root, manifest)
        if sha(args.annotations) != annotation_sha:
            raise ValueError("Annotations changed during acceptance")
        score = b1b_scores(report, reference + rows)
        run["results"][lang] = score
    run["acceptance_pass"] = all(
        all(score[key]["accuracy"] >= score[key]["target"] for key in ("ui", "closed_set", "numbers")) and
        all(score["timing"][key]["pass"] for key in ("ui", "numbers", "detection"))
        for score in run["results"].values())
    with (args.output_dir / "acceptance.json").open("x", encoding="utf-8") as file:
        json.dump(run, file, ensure_ascii=False, indent=2)
    print("Acceptance complete; pass=" + str(run["acceptance_pass"]))
