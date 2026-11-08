import sys
from pathlib import Path
import unittest
import tempfile
import hashlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from ocr_report import crop_latency, compare, normalize_ui, parse_number, b1b_scores
from ocr_acceptance import verify_freeze


class ReportTests(unittest.TestCase):
    def test_frozen_asset_mutation_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            asset = root / 'profile.json'
            asset.write_bytes(b'original')
            manifest = {'files': {'profile.json': hashlib.sha256(asset.read_bytes()).hexdigest()}}
            verify_freeze(root, manifest)
            asset.write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError, 'Frozen asset changed'):
                verify_freeze(root, manifest)
    def test_b1c_only_three_kana_pairs_and_crop_median_cap(self):
        self.assertEqual(normalize_ui('へべぺ'), normalize_ui('ヘベペ'))
        for a, b in [('パ', 'バ'), ('ぁ', 'あ'), ('ィ', 'イ'), ('ミ', '三')]:
            self.assertNotEqual(normalize_ui(a), normalize_ui(b))
        report = {'language': 'ja', 'summary': {'detection': {'p95_ms': 100}}, 'results': [
            {'id': 'a', 'kind': 'ui', 'scene': 'home', 'expected': 'へ', 'prediction': 'ヘ',
             'native_recognition_ms': [40, 41, 42, 43, 80]},
            {'id': 'b', 'kind': 'ui', 'scene': 'home', 'expected': 'へ', 'prediction': 'ヘ',
             'native_recognition_ms': [1, 2, 3, 4, 5]},
            {'id': 'c', 'kind': 'ui', 'scene': 'home', 'expected': 'へ', 'prediction': 'ヘ',
             'native_recognition_ms': [1, 2, 3, 4, 5]}]}
        result = b1b_scores(report)
        self.assertTrue(result['timing']['ui']['pass'])
        self.assertEqual(result['timing']['ui']['max_ms'], 80)
        self.assertEqual(result['timing']['ui']['max_crop_p50_ms'], 42)
        self.assertEqual(result['ui']['target'], .95)
    def test_b1b_ui_normalization_and_real_character_errors(self):
        for a, b in [('篩選 OFF', '篩選OFF'), ('Ｌｖ.９０', 'LV.90'), ('x５', '×5'),
                     ('ムツキ（正月）', 'ムツキ (正月)'), ('課程表•', '課程表'),
                     ('三一', '三—'), ('12 一 20', '12-20'), ('10～20', '10~20')]:
            if a == '三一':
                self.assertNotEqual(normalize_ui(a), normalize_ui(b))
            else:
                self.assertEqual(normalize_ui(a), normalize_ui(b))
        self.assertNotEqual(normalize_ui('ミカ'), normalize_ui('三力'))
        self.assertNotEqual(normalize_ui('パ'), normalize_ui('バ'))

    def test_b1b_numeric_semantics_preserve_operators(self):
        for a, b in [('1,000', '1000'), ('01 - 24', '1—24'), ('×03', 'X3'), ('+15%', '15.0 %')]:
            self.assertEqual(parse_number(a), parse_number(b))
        for a, b in [('6/6', '66'), ('4830/4830', '483014830'), ('+15%', '+1500'),
                     ('x20', '20'), ('63', '.63')]:
            self.assertNotEqual(parse_number(a), parse_number(b))
        for text in ['', '.63', '1,2', '●1', '6//6']:
            self.assertIsNone(parse_number(text))

    def test_b1b_closed_set_ties_fail_and_preserve_legacy_result(self):
        rows = [{'id': 'a', 'kind': 'ui', 'scene': 'home', 'expected': 'ab', 'prediction': 'a',
                 'native_recognition_ms': [1, 2, 3]}]
        report = {'language': 'ja', 'results': rows, 'summary': {'detection': {'p95_ms': 121}}}
        candidates = [{'kind': 'ui', 'language': 'ja', 'scene': 'home', 'text': label} for label in ['ab', 'ac']]
        result = b1b_scores(report, candidates)
        self.assertEqual(result['closed_set']['ties'], 1)
        self.assertEqual(result['closed_set']['correct'], 0)
        self.assertFalse(result['timing']['detection']['pass'])
        self.assertEqual(rows[0]['prediction'], 'a')

    def test_pooled_median_must_not_hide_slow_crop(self):
        rows = [{"id": "fast", "native_recognition_ms": [1, 1, 1]},
                {"id": "slow", "native_recognition_ms": [2, 9, 10]}]
        result = crop_latency(rows, 5)
        self.assertFalse(result["all_crops_pass"])
        self.assertEqual(result["over_budget"], [{"id": "slow", "p50_ms": 9}])
        self.assertEqual(result["max_crop_p50_ms"], 9)

    def test_backend_comparison_rejects_changed_ground_truth(self):
        a = {"language": "ja", "split": "test", "profile_id": "candidate", "results": [
            {"id": "crop", "expected": "1", "prediction": "1", "image": "a.png", "box": [0, 0, 10, 10], "kind": "numbers"}]}
        b = {**a, "results": [{**a["results"][0], "box": [1, 0, 10, 10]}]}
        with self.assertRaisesRegex(ValueError, "ground truth or ROI differs"):
            compare(a, b)


if __name__ == "__main__":
    unittest.main()
