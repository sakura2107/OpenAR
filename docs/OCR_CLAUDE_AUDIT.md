# 给 Claude 的 OpenAR OCR 独立审查交接

更新：已收到并读取 `tests/ocr_corpus/audits/B1-Claude抽查表.csv`，23 条 agree、1 条 text-ok-roi-fix。23 条已确认的原框审核信息导入 `annotations-b1b-v4.jsonl`；第17条新ROI仍待再次抽查。B1原始标注保持不变。当前结论、修订和后续新验收计划见 [B1b 报告](OCR_B1B.md)。以下内容保留为最初交接记录。

当前OCR验收未通过。请先阅读最新OCR_HEALTHCHECK，不以早期“零截图”报告或tune成绩判断验收。

1. 在本地解压抽查包并打开index.html，先独立抄写裁块，再展开现有标注；必要时打开对应整帧。
2. 填写audit-worksheet.csv的independent_text、decision、notes、audited_by与实际带时区audited_at。保留current_text，不按模型预测改真值。
3. 检查24条分层样本，特别是x/X/×、排行区间空格、日文小假名、ROI是否完整、图标是否误入。CPU/GPU差异不等于哪端是真值。
4. 还需检查完整语料的近似帧分组、scene标签、未检出文字、tune/test边界及数量是否足够；抽样包不足以证明这些全部通过。
5. 当前缺日服竞技场结算、繁中每日奖励和竞技场结算；不要为补图消耗游戏资源。
6. 提交发现与标注修订建议；审核前不要填写正式数据的audited字段。确认过的条目才可按id更新并记录版本/哈希，后续重新跑回归。

主语料：D:\OpenAR-main\tests\ocr_corpus\annotations.jsonl；1,357条/78张图，test1,030条。24条选择位于claude-audit-selection.jsonl。
本包由Codex整理，Claude审核尚未执行。selection.jsonl中的image路径相对于原语料根目录；包内整帧以SHA256命名，审阅页和CSV索引提供映射。
