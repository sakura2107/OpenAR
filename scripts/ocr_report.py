"""Summarize native OCR evidence without hiding slow crops in pooled timings.

This is a diagnostic report, never an acceptance signature. It preserves exact
accuracy, lists CPU/GPU differences, and checks the recorded asset hashes.
"""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import statistics
import re
import unicodedata
from decimal import Decimal

SCORING_VERSION = "b1b-v2"


def normalize_ui(text):
    text = unicodedata.normalize("NFKC", text).casefold()
    # Only these three visually identical kana pairs; no broad kana folding.
    text = text.translate(str.maketrans("ヘベペ", "へべぺ"))
    text = "".join(c for c in text if not c.isspace() and c not in "•●※")
    text = text.replace("×", "x").replace("～", "~").replace("〜", "~")
    return re.sub(r"(?<=\d)[一—－ー−–](?=\d)", "-", text)


def parse_number(text):
    """Return a typed semantic value; never infer a missing slash/x/percent.

    Also supports decimals in percentages and the corpus's open-ended ranking
    label '100 -'. Unsupported/ambiguous strings return None and count as wrong.
    """
    text = "".join(c for c in unicodedata.normalize("NFKC", text).casefold() if not c.isspace()).replace("×", "x")
    text = re.sub(r"(?<=\d)[一—－ー−–](?=\d)", "-", text)
    number = r"(?:\d{1,3}(?:,\d{3})+|\d+)(?:\.\d+)?"
    def value(s):
        return str(Decimal(s.replace(",", "")).normalize())
    for pattern, kind in ((rf"({number})/({number})", "fraction"),
                          (rf"({number})-({number})", "range"),
                          (rf"x({number})", "multiplier"),
                          (rf"([+-]?{number})%", "percent"),
                          (rf"({number})-", "range_from"),
                          (rf"([+-]?{number})", "number")):
        match = re.fullmatch(pattern, text)
        if match:
            return (kind, *(value(part) for part in match.groups()))
    return None


def edit_distance(a, b):
    previous = list(range(len(b) + 1))
    for i, ca in enumerate(a, 1):
        current = [i]
        for j, cb in enumerate(b, 1):
            current.append(min(current[-1] + 1, previous[j] + 1, previous[j-1] + (ca != cb)))
        previous = current
    return previous[-1]


def b1b_scores(report, annotations=None):
    # Supplying the complete annotation file makes the scene candidate set
    # explicit. Without it, use the report subset and disclose that limitation.
    candidates = collections.defaultdict(set)
    source = annotations if annotations is not None else report["results"]
    for row in source:
        if row["kind"] == "ui" and row.get("language", report["language"]) == report["language"]:
            candidates[row["scene"]].add(normalize_ui(row.get("text", row.get("expected", ""))))
    candidates = {scene: sorted(text for text in labels if text) for scene, labels in candidates.items()}
    scored = []
    for row in report["results"]:
        truth, prediction = row["expected"], row["prediction"]
        entry = {"id": row["id"], "kind": row["kind"], "expected": truth, "prediction": prediction}
        if row["kind"] == "ui":
            expected, predicted = normalize_ui(truth), normalize_ui(prediction)
            distances = [(edit_distance(predicted, c), c) for c in candidates.get(row["scene"], [])] if predicted else []
            best = min((d for d, _ in distances), default=None)
            nearest = [c for d, c in distances if d == best]
            entry.update(normalized_expected=expected, normalized_prediction=predicted,
                         correct=bool(expected) and expected == predicted, nearest=nearest,
                         closed_set_correct=len(nearest) == 1 and nearest[0] == expected,
                         closed_set_tie=len(nearest) > 1)
        else:
            expected, predicted = parse_number(truth), parse_number(prediction)
            entry.update(parsed_expected=expected, parsed_prediction=predicted,
                         correct=expected is not None and expected == predicted)
        scored.append(entry)
    result = {"version": SCORING_VERSION, "candidate_source": "full annotations" if annotations is not None else "report subset only",
              "closed_set_note": "Oracle scene, unique normalized text candidates; ties and empty output fail. Candidate labels include ground truth by the requested definition. This is not measured scene classification.",
              "candidate_counts": {k: len(v) for k, v in candidates.items()}, "results": scored}
    for kind, target in (("ui", .95 if report["language"] == "ja" else .97), ("numbers", .995)):
        group = [r for r in scored if r["kind"] == kind]
        correct = sum(r["correct"] for r in group)
        result[kind] = {"correct": correct, "total": len(group), "accuracy": correct/len(group) if group else None, "target": target}
    ui = [r for r in scored if r["kind"] == "ui"]
    correct = sum(r["closed_set_correct"] for r in ui)
    result["closed_set"] = {"correct": correct, "total": len(ui), "accuracy": correct/len(ui) if ui else None,
                            "ties": sum(r["closed_set_tie"] for r in ui), "target": .99}
    result["unparsed_number_truth_ids"] = [r["id"] for r in scored if r["kind"] == "numbers" and r["parsed_expected"] is None]
    result["literal_exact"] = {}
    for kind in ("ui", "numbers"):
        group = [r for r in report["results"] if r["kind"] == kind]
        count = sum(r["expected"] == r["prediction"] for r in group)
        result["literal_exact"][kind] = {"correct": count, "total": len(group), "accuracy": count/len(group) if group else None}
    timing = {}
    for kind, p50_budget, max_budget in (("ui", 15, 50), ("numbers", 5, 15)):
        values = [v for r in report["results"] if r["kind"] == kind for v in r["native_recognition_ms"]]
        # B1c: the cap applies to the slowest crop's repeated median.
        # Single-call maxima remain reference measurements.
        crop_medians = [statistics.median(r["native_recognition_ms"]) for r in report["results"] if r["kind"] == kind]
        timing[kind] = {"p50_ms": statistics.median(values) if values else None,
                        "max_ms": max(values) if values else None,
                        "max_crop_p50_ms": max(crop_medians) if crop_medians else None,
                        "p50_budget_ms": p50_budget, "max_budget_ms": max_budget,
                        "cap_basis": "maximum per-crop median",
                        "pass": bool(values) and statistics.median(values) <= p50_budget and max(crop_medians) <= max_budget}
    detection = report["summary"].get("detection")
    timing["detection"] = {"p95_ms": detection["p95_ms"] if detection else None, "budget_ms": 120,
                           "pass": bool(detection) and detection["p95_ms"] <= 120}
    result["timing"] = timing
    return result


def crop_latency(rows, budget_ms):
    medians = {row["id"]: statistics.median(row["native_recognition_ms"]) for row in rows}
    return {"budget_ms": budget_ms, "crops": len(rows),
            "over_budget": [{"id": key, "p50_ms": value} for key, value in medians.items() if value > budget_ms],
            "max_crop_p50_ms": max(medians.values()) if medians else None,
            "all_crops_pass": bool(rows) and all(value <= budget_ms for value in medians.values())}


def summarize(report, annotations=None):
    result = {"acceptance": False, "language": report["language"], "profile_id": report["profile_id"],
              "split": report["split"], "summary": report["summary"], "coverage": report["coverage"],
              "per_crop_latency": {}, "styles": {}, "number_forms_note": "See annotations by id.",
              "unstable_ids": [row["id"] for row in report["results"] if not row["stable"]],
              "asset_mismatches": [], "scenes": {}}
    for kind, budget in (("ui", 15), ("numbers", 5)):
        rows = [r for r in report["results"] if r["kind"] == kind]
        result["per_crop_latency"][kind] = crop_latency(rows, budget)
        for style in ("outlined", "small", "translucent"):
            group = [r for r in rows if style in r["styles"]]
            result["styles"][f"{kind}/{style}"] = {"correct": sum(r["correct"] for r in group), "total": len(group)}
    scenes = collections.defaultdict(list)
    for row in report["results"]:
        scenes[row["scene"]].append(row)
    result["scenes"] = {key: {"correct": sum(r["correct"] for r in rows), "total": len(rows)} for key, rows in sorted(scenes.items())}
    for name, expected in report["files_sha256"].items():
        path = Path(name)
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            result["asset_mismatches"].append(name)
    result["detector_over_120ms"] = [
        {"image_sha256": frame["image_sha256"], "times_ms": [t for t in frame["native_detection_ms"] if t > 120]}
        for frame in report["frames"] if any(t > 120 for t in frame["native_detection_ms"])]
    result["b1b"] = b1b_scores(report, annotations)
    return result


def compare(cpu, gpu):
    if (cpu["language"], cpu["split"], cpu["profile_id"]) != (gpu["language"], gpu["split"], gpu["profile_id"]):
        raise ValueError("Cannot compare different languages, splits or profiles")
    a = {row["id"]: row for row in cpu["results"]}
    b = {row["id"]: row for row in gpu["results"]}
    if len(a) != len(cpu["results"]) or len(b) != len(gpu["results"]) or a.keys() != b.keys():
        raise ValueError("CPU/GPU records differ or contain duplicate ids")
    differences = []
    for key, row in a.items():
        other = b[key]
        if any(row[field] != other[field] for field in ("expected", "image", "box", "kind")):
            raise ValueError("CPU/GPU ground truth or ROI differs: " + key)
        if row["prediction"] != other["prediction"]:
            differences.append({"id": key, "kind": row["kind"], "expected": row["expected"],
                                "cpu": row["prediction"], "gpu": other["prediction"],
                                "image": row["image"], "box": row["box"]})
    return {"compared_crops": len(a), "differences": differences}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpu", type=Path, required=True)
    parser.add_argument("--gpu", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--annotations", type=Path, help="Full candidate labels for scene-specific closed-set scoring")
    args = parser.parse_args()
    reports = [json.loads(p.read_text(encoding="utf-8")) for p in (args.cpu, args.gpu)]
    annotations = None
    if args.annotations:
        from ocr_corpus import read_jsonl
        annotations = read_jsonl(args.annotations)
    report = {"acceptance": False, "cpu": summarize(reports[0], annotations), "gpu": summarize(reports[1], annotations),
              "cpu_gpu": compare(*reports), "sources_sha256": {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in (args.cpu, args.gpu)}}
    if args.annotations:
        report["sources_sha256"][str(args.annotations)] = hashlib.sha256(args.annotations.read_bytes()).hexdigest()
    with args.output.open("x", encoding="utf-8") as file:
        json.dump(report, file, ensure_ascii=False, indent=2)
    print("Diagnostic summary written; acceptance remains false.")
