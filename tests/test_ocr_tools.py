"""Contract tests for the corpus gate. Synthetic pixels are NOT a BA corpus."""
import copy
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import ocr_corpus as corpus
import ocr_compare


def png():
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 1280, 720, 8, 2, 0, 0, 0)) + chunk(
        b"IDAT", zlib.compress((b"\0" + b"\x21\x43\x65" * 1280) * 720)) + chunk(b"IEND", b"")


class CorpusTests(unittest.TestCase):
    def setUp(self):
        (ROOT / "out").mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=ROOT / "out")
        self.base = Path(self.temp.name)
        (self.base / "images").mkdir()
        (self.base / "images/1.png").write_bytes(png())
        self.digest = corpus.image_info(self.base / "images/1.png")[1]
        self.row = {"schema_version": 1, "id": "ja-1", "language": "ja", "image": "images/1.png",
                    "image_sha256": self.digest, "scene": "home", "split": "test", "kind": "ui",
                    "number_form": "", "styles": ["outlined"], "box": [10, 10, 100, 30],
                    "text": "任務", "prediction": "任務", "reviewed": True, "reviewer": "human-reviewer",
                    "reviewed_at": "2026-11-01T12:00:00-08:00", "audited_by": "", "audited_at": ""}

    def tearDown(self):
        self.temp.cleanup()

    def check_rows(self, rows):
        path = self.base / "annotations.jsonl"
        path.write_text("".join(json.dumps(row, ensure_ascii=False) + "\n" for row in rows), encoding="utf-8")
        return corpus.validate(path)

    def test_unreviewed_predictions_never_become_truth(self):
        row = copy.deepcopy(self.row)
        row["reviewed"] = False
        result = self.check_rows([row])
        self.assertFalse(result["eligible_for_accuracy_evaluation"])
        self.assertTrue(any("human-reviewed" in e for e in result["errors"]))
        self.assertEqual(result["counts"], {})

    def test_percent_metadata_keeps_truth_operators(self):
        row=dict(self.row,kind='numbers',number_form='percent',text='742%')
        result=self.check_rows([row])
        self.assertEqual(result['record_errors'],[])
        self.assertEqual(corpus.read_jsonl(self.base/'annotations.jsonl')[0]['text'],'742%')

    def test_changed_image_and_invalid_box_rejected(self):
        for key, value, message in [("image_sha256", "wrong", "image changed"), ("box", [1270, 710, 100, 30], "outside image")]:
            row = dict(self.row, **{key: value})
            self.assertTrue(any(message in e for e in self.check_rows([row])["errors"]))

    def test_duplicate_region_and_tune_test_leak_rejected(self):
        duplicate = dict(self.row, id="ja-2")
        self.assertTrue(any("duplicate image/region" in e for e in self.check_rows([self.row, duplicate])["errors"]))
        shifted = dict(duplicate, box=[11, 10, 100, 30])
        self.assertTrue(any("near-duplicate" in e for e in self.check_rows([self.row, shifted])["errors"]))
        leaked = dict(duplicate, box=[200, 10, 100, 30], split="tune")
        self.assertTrue(any("leaks" in e for e in self.check_rows([self.row, leaked])["errors"]))

    def test_path_escape_and_width_folding(self):
        with self.assertRaises(ValueError):
            corpus.safe_image(self.base, "../outside.png")
        self.assertEqual(corpus.normalize_text("　ＡＢ１２／２０　", "ui"), "AB12/20")
        self.assertEqual(corpus.normalize_text(" １２／２０ ", "numbers"), " １２／２０ ")
        self.assertEqual(corpus.normalize_text("①", "ui"), "①")

    def test_init_deduplicates_frames_and_keeps_review_false(self):
        capture = self.base / "capture.jsonl"
        frame = {"image": "images/1.png", "language": "ja"}
        capture.write_text(json.dumps(frame) + "\n" + json.dumps(frame) + "\n", encoding="utf-8")
        output = self.base / "draft.jsonl"
        corpus.init(capture, output)
        rows = corpus.read_jsonl(output)
        self.assertEqual(len(rows), 1)
        self.assertFalse(rows[0]["reviewed"])
        self.assertIsNone(rows[0]["text"])
        with self.assertRaises(FileExistsError):
            corpus.init(capture, output)

    def test_empty_set_fails_minimums(self):
        result = self.check_rows([])
        self.assertFalse(result["eligible_for_accuracy_evaluation"])
        self.assertTrue(any("0 < 150" in e for e in result["errors"]))

    def test_incomplete_coverage_is_distinct_from_invalid_evidence(self):
        valid = self.check_rows([self.row])
        self.assertEqual(valid["record_errors"], [])
        self.assertTrue(valid["coverage_errors"])
        invalid = self.check_rows([dict(self.row, reviewed=False)])
        self.assertTrue(invalid["record_errors"])
        self.assertEqual(invalid["errors"], invalid["record_errors"] + invalid["coverage_errors"])

    def test_extra_gpu_detection_does_not_invent_a_reading_order_error(self):
        def row(x):
            return {"image_sha256": "same-frame", "box": [x, 10, 10, 10], "prediction": str(x)}
        cpu = [row(10), row(50)]
        gpu = [row(10), row(30), row(50)]
        result = ocr_compare.compare(cpu, gpu)
        self.assertEqual(result["reading_order_differences"], 0)
        self.assertEqual(result["output_index_differences"], 1)
        self.assertEqual(len(result["unmatched"]), 1)
        self.assertEqual(ocr_compare.compare(cpu, [gpu[2], gpu[0]])["reading_order_differences"], 2)

    def test_merge_rebases_images_preserves_review_and_rejects_collisions(self):
        source = self.base / "one.jsonl"
        source.write_text(json.dumps(self.row) + "\n", encoding="utf-8")
        corpus.merge([source], self.base / "merged.jsonl")
        row = corpus.read_jsonl(self.base / "merged.jsonl")[0]
        self.assertEqual(row, self.row)
        with self.assertRaises(ValueError):
            corpus.merge([source, source], self.base / "duplicate.jsonl")


if __name__ == "__main__":
    unittest.main()
