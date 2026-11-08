"""Render indexed real OCR crops for visual review; never changes review status."""
import argparse
import json
from pathlib import Path
import cv2
import numpy as np
from ocr_corpus import read_jsonl, safe_image


def render(source, output):
    rows = read_jsonl(source)
    output.mkdir(parents=True, exist_ok=False)
    frames = {}
    for page in range((len(rows) + 19) // 20):
        sheet = np.full((900, 1200, 3), 245, np.uint8)
        for local, row in enumerate(rows[page * 20:page * 20 + 20]):
            path = safe_image(source.parent, row["image"])
            if path not in frames:
                frames[path] = cv2.imdecode(np.frombuffer(path.read_bytes(), np.uint8), cv2.IMREAD_COLOR)
            image = frames[path]
            x, y, w, h = row["box"]
            if image is None or min(x, y) < 0 or min(w, h) < 1 or x + w > image.shape[1] or y + h > image.shape[0]:
                raise ValueError("Invalid review image/ROI")
            crop = image[y:y+h, x:x+w]
            scale = min(2, 575/w, 65/h)
            crop = cv2.resize(crop, (max(1, round(w*scale)), max(1, round(h*scale))))
            sx, sy = local % 2 * 600, local // 2 * 90
            sheet[sy+22:sy+22+crop.shape[0], sx+5:sx+5+crop.shape[1]] = crop
            cv2.putText(sheet, f"{page*20+local} / {path.stem}", (sx+5, sy+16), cv2.FONT_HERSHEY_SIMPLEX, .48, (0, 0, 0), 1)
        encoded = cv2.imencode(".png", sheet)[1]
        (output / f"crops-{page:02d}.png").write_bytes(encoded.tobytes())
    (output / "index.json").write_text(json.dumps([
        {"index": i, "id": row["id"], "image": row["image"], "box": row["box"], "prediction": row["prediction"]}
        for i, row in enumerate(rows)], ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Rendered {len(rows)} real crops. Review flags unchanged.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--annotations", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    render(args.annotations, args.output_dir)
