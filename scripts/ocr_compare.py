"""Compare native CPU/GPU proposal outputs by image+box overlap, not row index.

This checks backend agreement only, never OCR accuracy. Greedy one-to-one IoU
matching is explicit; unmatched or ambiguous detections still need inspection.
"""
import argparse
import json
from pathlib import Path
from ocr_corpus import read_jsonl


def iou(a, b):
    x, y, w, h = a
    u, v, s, t = b
    overlap = max(0, min(x+w, u+s)-max(x, u)) * max(0, min(y+h, v+t)-max(y, v))
    union = w*h + s*t - overlap
    return overlap / union if union > 0 else 0


def compare(cpu, gpu):
    groups = {}
    for name, rows in (("cpu", cpu), ("gpu", gpu)):
        for row in rows:
            groups.setdefault(row["image_sha256"], {"cpu": [], "gpu": []})[name].append(row)
    output = {"acceptance": False, "purpose": "backend agreement, not ground-truth accuracy",
              "matching": "greedy one-to-one IoU >= .5; reading order compared separately",
              "cpu_regions": len(cpu), "gpu_regions": len(gpu), "matched": 0,
              "reading_order_differences": 0, "output_index_differences": 0,
              "text_differences": [], "box_differences": [], "unmatched": []}
    for digest, group in groups.items():
        a, b = group["cpu"], group["gpu"]
        pairs = sorted(((iou(x["box"], y["box"]), i, j) for i, x in enumerate(a)
                        for j, y in enumerate(b)), reverse=True)
        used_a, used_b = set(), set()
        matched_order = []
        for overlap, i, j in pairs:
            if overlap < .5:
                break
            if i in used_a or j in used_b:
                continue
            used_a.add(i); used_b.add(j)
            output["matched"] += 1
            output["output_index_differences"] += int(i != j)
            matched_order.append((i, j))
            item = {"image_sha256": digest, "cpu_box": a[i]["box"], "gpu_box": b[j]["box"],
                    "cpu_text": a[i]["prediction"], "gpu_text": b[j]["prediction"], "iou": overlap}
            if item["cpu_text"] != item["gpu_text"]:
                output["text_differences"].append(item)
            if item["cpu_box"] != item["gpu_box"]:
                output["box_differences"].append(item)
        # An extra detection shifts subsequent output indices without changing
        # the reading order of any common region. Compare relative order only.
        gpu_order = [j for i, j in sorted(matched_order)]
        output["reading_order_differences"] += sum(a != b for a, b in zip(gpu_order, sorted(gpu_order)))
        for name, rows, used in (("cpu", a, used_a), ("gpu", b, used_b)):
            output["unmatched"].extend({"backend": name, "image_sha256": digest, "box": row["box"],
                                        "text": row["prediction"]} for i, row in enumerate(rows) if i not in used)
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpu", type=Path, required=True)
    parser.add_argument("--gpu", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = compare(read_jsonl(args.cpu), read_jsonl(args.gpu))
    with args.output.open("x", encoding="utf-8", newline="\n") as file:
        json.dump(result, file, ensure_ascii=False, indent=2)
    print(f"matched={result['matched']} text_differences={len(result['text_differences'])} "
          f"box_differences={len(result['box_differences'])} order_differences={result['reading_order_differences']} "
          f"unmatched={len(result['unmatched'])}")
