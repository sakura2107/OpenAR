"""Evaluate reviewed real crops through the native C++ OCR plugin.

Reports exact strings and native preprocess+ORT+decode timings. Incomplete or
unreviewed evidence never qualifies as acceptance. Disk I/O/model loading are excluded.
"""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import statistics
import cv2
import numpy as np
from ocr_corpus import read_jsonl, safe_image, image_info, normalize_text, validate
from ocr_prelabel import NativeOcr
from ocr_cpu_monitor import CpuMonitor


def latency(values):
    if not values:
        return None
    values = sorted(values)
    return {"runs": len(values), "p50_ms": statistics.median(values),
            "p95_ms": float(np.percentile(values, 95)), "max_ms": max(values)}


def benchmark(args):
    profile = json.loads(args.profile.read_text(encoding="utf-8"))
    coverage = validate(args.annotations)
    if coverage["record_errors"]:
        raise ValueError("Invalid evidence cannot be benchmarked, even with --allow-incomplete: "
                         + "; ".join(coverage["record_errors"][:5]))
    if not coverage["eligible_for_accuracy_evaluation"] and not args.allow_incomplete:
        raise ValueError("Corpus gate failed; use --allow-incomplete for explicitly non-acceptance diagnostics")
    rows = [row for row in read_jsonl(args.annotations) if row["language"] == profile["language"] and row["split"] == args.split]
    if args.kind != "all": rows = [row for row in rows if row["kind"] == args.kind]
    if not rows or any(row["reviewed"] is not True or not row["text"] or not row["reviewer"] or not row["reviewed_at"] for row in rows):
        raise ValueError("Evaluation requires nonempty individually reviewed ground truth")
    images = {}
    for row in rows:
        path = safe_image(args.annotations.parent, row["image"])
        if path not in images:
            size, digest = image_info(path)
            if size != (1280, 720):
                raise ValueError("Corpus frame is not 1280x720")
            images[path] = (cv2.imdecode(np.frombuffer(path.read_bytes(), np.uint8), cv2.IMREAD_COLOR), digest)
        pixels, digest = images[path]
        x, y, w, h = row["box"]
        if digest != row["image_sha256"] or min(x, y) < 0 or min(w, h) <= 0 or x+w > 1280 or y+h > 720:
            raise ValueError("Changed image or invalid ROI")
    monitor=CpuMonitor();monitor.start()
    engine=None
    results, detection = [], []
    try:
        engine = NativeOcr(args.plugin, profile=args.profile)
        # Whole-engine warmup before ANY timed measurement, not merely each crop.
        monitor.phase='global_warmup'
        warmup_frames=min(3,len(images))
        for pixels,_ in list(images.values())[:warmup_frames]:engine.recognize(pixels,3)
        for row in rows[:3]:
            pixels=images[safe_image(args.annotations.parent,row['image'])][0]
            x,y,w,h=row['box'];crop=pixels[y:y+h,x:x+w]
            engine.recognize(crop,2 if row['kind']=='numbers' else 1)
        monitor.sample();monitor.phase='measurement'
        for row in rows:
            pixels = images[safe_image(args.annotations.parent, row["image"])][0]
            x, y, w, h = row["box"]
            crop = pixels[y:y+h, x:x+w]
            mode = 2 if row["kind"] == "numbers" else 1
            for _ in range(args.warmup):
                engine.recognize(crop,mode)
            timings = []
            predictions = []
            for _ in range(args.repeats):
                items,_=engine.recognize(crop,mode)
                predictions.append(''.join(item['text'] for item in items))
                timings.append(engine.last_timings['recognition_ms'])
            prediction = predictions[0]
            results.append({"id": row["id"], "language": row["language"], "kind": row["kind"], "scene": row["scene"],
                            "image": row["image"], "box": row["box"],
                            "styles": row["styles"], "expected": row["text"], "prediction": prediction,
                            "correct": normalize_text(prediction, row["kind"]) == normalize_text(row["text"], row["kind"]),
                            "stable": len(set(predictions)) == 1, "native_recognition_ms": timings})
        for path, (pixels, digest) in ([] if args.skip_detection else images.items()):
            for _ in range(args.warmup):
                engine.recognize(pixels, 3)
            times = []
            for _ in range(args.repeats):
                engine.recognize(pixels, 3)
                times.append(engine.last_timings["detection_ms"])
            detection.append({"image_sha256": digest, "native_detection_ms": times})
    finally:
        if engine:engine.close()
        monitor.stop()
    summary = {}
    for kind, target, budget in (("ui", .97, 15), ("numbers", .995, 5)):
        group = [r for r in results if r["kind"] == kind]
        times = [t for row in group for t in row["native_recognition_ms"]]
        correct = sum(row["correct"] for row in group)
        summary[kind] = {"correct": correct, "total": len(group), "accuracy": correct/len(group) if group else None,
                         "target": target, "latency": latency(times), "p50_budget_ms": budget}
    summary["detection"] = latency([t for row in detection for t in row["native_detection_ms"]])
    assets = [args.plugin, args.profile, args.annotations, args.plugin.resolve().parent.parent / "onnxruntime.dll"]
    for section in ("detector", "recognizer", "numbers"):
        for key in ("model", "dictionary"):
            if key in profile[section]: assets.append((args.profile.parent / profile[section][key]).resolve())
    for key in ("model", "dictionary"):
        if key in profile["numbers"].get("fallback", {}): assets.append((args.profile.parent / profile["numbers"]["fallback"][key]).resolve())
        if key in profile["numbers"].get("fallback", {}).get("next", {}):assets.append((args.profile.parent/profile['numbers']['fallback']['next'][key]).resolve())
    return {"schema_version": 1, "acceptance": False,
            "note": "Measured subset only. Overall acceptance also needs coverage, independent test, GPU comparison, OpenARTests and all budgets.",
            "profile_id": profile["id"], "language": profile["language"], "split": args.split,
            "warmup_per_shape": args.warmup, "repeats": args.repeats,
            "global_warmup_detection_frames":warmup_frames,"cpu_usage":monitor.report(),
            "files_sha256": {str(p.resolve()): hashlib.sha256(p.read_bytes()).hexdigest() for p in assets},
            "coverage": coverage, "summary": summary, "results": results, "frames": detection}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("annotations", "profile", "plugin", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--warmup", type=int, default=2)
    parser.add_argument("--repeats", type=int, default=5)
    parser.add_argument("--split", choices=("tune", "test"), default="test")
    parser.add_argument("--allow-incomplete", action="store_true")
    parser.add_argument("--kind", choices=("all", "ui", "numbers"), default="all")
    parser.add_argument("--skip-detection", action="store_true")
    args = parser.parse_args()
    if not 0 <= args.warmup <= 100 or not 1 <= args.repeats <= 100:
        parser.error("Invalid warmup/repeats")
    if args.output.exists():
        parser.error("Output exists; use a new report name")
    report = benchmark(args)
    with args.output.open("x", encoding="utf-8", newline="\n") as file:
        json.dump(report, file, ensure_ascii=False, indent=2)
    print(json.dumps(report["summary"], ensure_ascii=False, indent=2))
