# ja OCR 真实语料

B1c当前参考语料为上一级 `annotations-b1c-v5.jsonl`，最终冻结为 `freeze-b1c-final.json`。新独立验收单独位于 `../acceptance-b1c/`，不得合并入旧回归后再称为独立集。见 `../../../docs/OCR_B1C.md`。

B1b：旧 test 仅作回归。新增版本为上一级 `annotations-b1b-v4.jsonl`，ROI/审核证据修订在 `annotation-amendments-b1b-v4.json`，最终候选冻结在 `freeze-b1b-v3.json`。B1 `annotations.jsonl` 和 `freeze-final.json` 不变。最新结果见 `../../../docs/OCR_B1B.md`。

主采集目录 `captures/`、本轮补采目录 `supplement/`。两批正式采集各 300 张；主批次和补采均已结束。原图为 OpenAR/MuMu 原生截图，统一 1280×720。

正式标注入口是上一级 `annotations.jsonl`；勿把历史 draft、未审核 proposals、复核用副本或多个版本重复合并。每条正式记录含图片 SHA256、ROI、正确文本、scene、kind、styles、split、实际复核时间。`freeze-final.json` 固定数据与模型哈希，`annotation-amendments-v2.json` 保留 11 处标注修正，`annotation-amendments-v3.json` 再保留 11 处 ROI 边界修正。

旧 tune 标注和冻结 test 分开保存；相同图像不能跨 split。近似场景分组由人工复核，工具检查完全相同帧，不声称能够自动证明所有近似画面独立。B1原版本的Claude字段为空；B1b新版本已导入抽查表中23条agree的实际记录，修过的ROI仍待复查。

验收结论与测试命令见 `../../../docs/OCR_HEALTHCHECK.md`。未审核模型预测不是测试真值。
