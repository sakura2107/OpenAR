import sys
import tempfile
from pathlib import Path
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from ocr_build_ctc_mask import build_mask

class MaskTests(unittest.TestCase):
    def test_simplified_exclusion_retains_ambiguous_and_valid_traditional(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);st=root/'ST.txt';tw=root/'TW.txt'
            st.write_text('扫\t掃\n装\t裝\n群\t羣\n床\t牀\n后\t后 後\n',encoding='utf-8')
            tw.write_text('羣\t群\n牀\t床\n',encoding='utf-8')
            mask=build_mask(st,'fixture-v1',tw)
            self.assertEqual(set(mask['blocked_characters']),{'扫','装'})
            self.assertEqual(len(mask['source_sha256']),64)

if __name__=='__main__':unittest.main()
