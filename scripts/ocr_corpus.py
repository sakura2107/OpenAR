"""Manage OCR evidence; no device connection, clicks or account operations.

init prepares annotation records from a capture.jsonl. validate strictly gates
human-reviewed evidence. No model output can set reviewed=true automatically.
Only Python's standard library is needed.
"""
from __future__ import annotations
import argparse
import collections
import hashlib
import json
from pathlib import Path
import struct
import sys

LANGUAGES = ("ja", "zh-Hant")
SCENES = ("login-notices", "home", "mail", "missions", "daily-reward", "cafe", "lessons", "club",
          "bounty", "commission", "scrimmage", "normal-hard-stages", "sweep-confirm", "arena-ranking",
          "arena-opponents", "arena-results", "crafting", "shop", "event-stages", "event-shop",
          "event-currency", "students", "student-details", "resources")
STYLES = ("outlined", "small", "translucent")
EXTRA_SCENES = ("settings", "formation", "enemy-details", "rewards", "other")
NUMBER_FORMS = ("integer", "thousands", "fraction", "multiplier", "ranking", "level", "percent")


def read_jsonl(path):
    def no_duplicates(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"duplicate JSON key: {key}")
            result[key] = value
        return result
    with path.open(encoding="utf-8-sig") as file:
        return [json.loads(line, object_pairs_hook=no_duplicates) for line in file if line.strip()]


def safe_image(base, relative):
    if not isinstance(relative, str) or not relative or Path(relative).is_absolute():
        raise ValueError("image must be a relative path")
    result = (base / relative).resolve()
    if not result.is_relative_to(base.resolve()):
        raise ValueError("image escapes corpus root")
    return result


def image_info(path):
    data = path.read_bytes()
    if len(data) < 24 or data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        raise ValueError(f"Not a PNG: {path}")
    return struct.unpack(">II", data[16:24]), hashlib.sha256(data).hexdigest()


def normalize_text(text, kind):
    # Width folding only; NFKC also folds unrelated compatibility characters.
    if kind == "numbers":
        return text
    return "".join(" " if c == "\u3000" else chr(ord(c) - 0xFEE0)
                   if 0xFF01 <= ord(c) <= 0xFF5E else c for c in text).strip()


def init(capture, output):
    records = read_jsonl(capture)
    out = []
    seen = set()
    for index, frame in enumerate(records, 1):
        image = safe_image(capture.parent, frame["image"])
        size, digest = image_info(image)
        if size != (1280, 720) or frame["language"] not in LANGUAGES:
            raise ValueError(f"Invalid capture frame: {image}")
        # Repeated frames never inflate coverage; retain the source files.
        if digest in seen:
            continue
        seen.add(digest)
        import os
        relative = Path(os.path.relpath(image, output.parent)).as_posix()
        safe_image(output.parent, relative)
        out.append({"schema_version": 1, "id": f"{frame['language']}-{digest[:12]}-region-001",
                    "language": frame["language"], "image": relative, "image_sha256": digest,
                    "scene": "", "split": "", "kind": "", "number_form": "", "styles": [],
                    "box": None, "text": None, "prediction": None,
                    "reviewed": False, "reviewer": "", "reviewed_at": "",
                    "audited_by": "", "audited_at": ""})
    with output.open("x", encoding="utf-8", newline="\n") as file:
        for item in out:
            file.write(json.dumps(item, ensure_ascii=False) + "\n")
    print(f"Prepared {len(out)} unique frame placeholders. Add one record per text region; none are reviewed.")


def validate(path, minimum_ui=150, minimum_numbers=100):
    import datetime
    rows = read_jsonl(path)
    errors = []
    counts = collections.Counter()
    scenes = collections.Counter()
    styles = collections.Counter()
    forms = collections.Counter()
    seen_ids, seen_regions, hashes = set(), set(), {}
    splits = collections.defaultdict(set)
    audited = 0
    allowed = {"schema_version", "id", "language", "image", "image_sha256", "scene", "split", "kind",
               "number_form", "styles", "box", "text", "prediction", "reviewed", "reviewer", "reviewed_at",
               "audited_by", "audited_at"}
    for index, row in enumerate(rows, 1):
        try:
            if not isinstance(row, dict) or set(row) != allowed or row["schema_version"] != 1:
                raise ValueError("annotation schema differs; use init template")
            if not isinstance(row["id"], str) or not row["id"] or row["id"] in seen_ids:
                raise ValueError("missing/duplicate id")
            seen_ids.add(row["id"])
            if row["language"] not in LANGUAGES or row["kind"] not in ("ui", "numbers"):
                raise ValueError("language/kind is invalid")
            if row["scene"] not in SCENES + EXTRA_SCENES or row["split"] not in ("tune", "test"):
                raise ValueError("scene/split is missing or invalid")
            if type(row["reviewed"]) is not bool or not row["reviewed"] or not isinstance(row["text"], str) or not row["text"]:
                raise ValueError("ground truth must be nonempty and individually human-reviewed")
            if not isinstance(row["reviewer"], str) or not row["reviewer"].strip():
                raise ValueError("reviewer is missing")
            for field in ("reviewed_at", "audited_at"):
                if row[field]:
                    stamp = datetime.datetime.fromisoformat(row[field].replace("Z", "+00:00"))
                    if stamp.utcoffset() is None:
                        raise ValueError(f"{field} must have a timezone")
                elif field == "reviewed_at":
                    raise ValueError("review timestamp is missing")
            if bool(row["audited_by"]) != bool(row["audited_at"]):
                raise ValueError("audit requires both auditor and timestamp")
            if row["audited_by"]:
                if row["audited_by"] == row["reviewer"]:
                    raise ValueError("auditor must differ from reviewer")
                audited += 1
            image = safe_image(path.parent, row["image"])
            if image not in hashes:
                hashes[image] = image_info(image)
            size, digest = hashes[image]
            if size != (1280, 720) or digest != row["image_sha256"]:
                raise ValueError("image changed or is not 1280x720")
            box = row["box"]
            if not isinstance(box, list) or len(box) != 4 or any(type(v) is not int for v in box):
                raise ValueError("box must be integer [x,y,w,h]")
            x, y, w, h = box
            if min(x, y) < 0 or min(w, h) <= 0 or x + w > 1280 or y + h > 720:
                raise ValueError("box is outside image")
            region = (digest, tuple(box))
            if region in seen_regions:
                raise ValueError("duplicate image/region")
            seen_regions.add(region)
            # Prevent near-identical, slightly shifted boxes from inflating counts.
            for old_digest, old_box in seen_regions:
                if old_digest != digest or old_box == tuple(box):
                    continue
                ox, oy, ow, oh = old_box
                intersection = max(0, min(x+w, ox+ow)-max(x, ox)) * max(0, min(y+h, oy+oh)-max(y, oy))
                if intersection / (w*h + ow*oh - intersection) > 0.9:
                    raise ValueError("near-duplicate image/region (IoU > .9)")
            splits[digest].add(row["split"])
            if len(splits[digest]) > 1:
                raise ValueError("same frame leaks between tune/test")
            if not isinstance(row["styles"], list) or any(s not in STYLES for s in row["styles"]):
                raise ValueError("invalid style tag")
            if row["kind"] == "numbers":
                if row["number_form"] not in NUMBER_FORMS:
                    raise ValueError("number_form is missing")
                if any(c not in "0123456789,/xX×.:%+- " for c in row["text"]) or not any(c.isdigit() for c in row["text"]):
                    raise ValueError("numbers ground truth includes unsupported nonnumeric characters")
                forms[row["language"], row["number_form"]] += 1
            elif row["number_form"]:
                raise ValueError("ui record must have empty number_form")
            if not normalize_text(row["text"], row["kind"]):
                raise ValueError("text is empty after normalization")
            counts[row["language"], row["kind"], row["split"]] += 1
            scenes[row["language"], row["scene"]] += 1
            for style in set(row["styles"]):
                styles[row["language"], style] += 1
        except (KeyError, TypeError, ValueError, OSError) as exc:
            errors.append(f"line {index}: {exc}")
    record_errors = list(errors)
    for language in LANGUAGES:
        for kind, minimum in (("ui", minimum_ui), ("numbers", minimum_numbers)):
            count = counts[language, kind, "test"]
            if count < minimum:
                errors.append(f"{language}/{kind}: reviewed held-out test {count} < {minimum}")
        for name in SCENES:
            if not scenes[language, name]:
                errors.append(f"{language}: scene missing: {name}")
        for style in STYLES:
            if not styles[language, style]:
                errors.append(f"{language}: style missing: {style}")
        for form in NUMBER_FORMS:
            if not forms[language, form]:
                errors.append(f"{language}: number form missing: {form}")
    report = {"schema_version": 1, "eligible_for_accuracy_evaluation": not errors,
              "record_errors": record_errors, "coverage_errors": errors[len(record_errors):],
              "records": len(rows), "unique_images": len(hashes), "audited_records": audited,
              "counts": {"/".join(key): value for key, value in sorted(counts.items())}, "errors": errors,
              "note": "Coverage is necessary, not accuracy acceptance. Human review and Claude audit are external evidence."}
    return report


def merge(inputs, output):
    import os
    records, ids = [], set()
    for source in inputs:
        for row in read_jsonl(source):
            if row["id"] in ids:
                raise ValueError(f"Duplicate id while merging: {row['id']}")
            ids.add(row["id"])
            image = safe_image(source.parent, row["image"])
            row["image"] = Path(os.path.relpath(image, output.parent)).as_posix()
            safe_image(output.parent, row["image"])
            records.append(row)
    with output.open("x", encoding="utf-8", newline="\n") as file:
        for row in records:
            file.write(json.dumps(row, ensure_ascii=False) + "\n")
    print(f"Merged {len(records)} records; review status preserved. Run validate before evaluation.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    prepare = sub.add_parser("init")
    prepare.add_argument("--capture", type=Path, required=True)
    prepare.add_argument("--output", type=Path, required=True)
    check = sub.add_parser("validate")
    check.add_argument("--annotations", type=Path, required=True)
    combine = sub.add_parser("merge")
    combine.add_argument("--annotations", type=Path, nargs="+", required=True)
    combine.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == "init":
            init(args.capture, args.output)
        elif args.command == "merge":
            merge(args.annotations, args.output)
        else:
            result = validate(args.annotations)
            print(json.dumps(result, ensure_ascii=False, indent=2))
            sys.exit(0 if result["eligible_for_accuracy_evaluation"] else 2)
    except (ValueError, OSError) as exc:
        parser.exit(2, f"error: {exc}\n")
