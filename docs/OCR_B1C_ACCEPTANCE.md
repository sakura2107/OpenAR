> B1d 更新：本页采图命令仅为历史记录。以后不再为OCR专门采图，下一独立批从B2/B4自然录制帧抽取；B1c批次已转回归。B1有条件通过，B2尚未启动。详见[最新B1d报告](OCR_B1D.md)。

# B1c 独立验收操作

最终冻结入口是 `tests/ocr_corpus/freeze-b1c-final.json`；`freeze-b1c.json` 是提前生成、随后发现日服留白回归退化的候选快照，不是独立验收使用版本。最终冻结前没有采集或查看任何新验收截图。

采前和推理前都核对冻结清单。用户手动翻页，工具只截图；每服约3–5分钟、60条文字/40条数字，数据放在全新的 `tests/ocr_corpus/acceptance-b1c/` 批次下。优先主页/任务、商店价格及倍率、学生/技能等级、关卡与扫荡确认等可自然打开的页面，保留千分位、斜杠、百分比、x1和重复数字。日服竞技场结算、繁中每日奖励及竞技场结算只在自然可见时补，不为采集开战或领奖。

```powershell
Set-Location D:\OpenAR-main
# 日服：用户确认实例0仍为日服并就绪后运行
./scripts/capture-ocr.ps1 -Language ja -MuMuIndex 0 -Serial 127.0.0.1:16384 -MuMuPath 'D:\software\mumu\MuMuPlayer' -Count 100 -IntervalMs 3000 -Output tests/ocr_corpus/acceptance-b1c/ja
# 繁中：用户确认实例1仍为繁中并就绪后运行
./scripts/capture-ocr.ps1 -Language zh-Hant -MuMuIndex 1 -Serial 127.0.0.1:16416 -MuMuPath 'D:\software\mumu\MuMuPlayer' -Count 100 -IntervalMs 3000 -Output tests/ocr_corpus/acceptance-b1c/zh-Hant
```

每个命令最多100帧、约5分钟；可提前结束并保留已存帧。不要自动启动/关闭游戏或点击界面。实例/语言映射若已变化，应先由用户确认当前映射。

采集后先从不同页面选择清晰且有代表性的原始帧，记录加载/重复帧的排除理由。Codex看原图逐条标注，真值不从OCR输出抄写；新标注只用split=test。Claude独立抽查约20条，按语言、文字/数字、样式分层，填写实际审核信息。新图与旧语料完全相同的像素帧不得作为独立样本；近似页面分组仍需人工检查。未完成标注与抽查之前，不跑新集OCR准确率。

审核完成后运行：

```powershell
$ocrPython = 'C:\Users\caiji\Documents\Codex\2026-11-01\b1-openar-ocr-gpt-codex-ba\work\ocr-venv\Scripts\python.exe'
& $ocrPython scripts/ocr_acceptance.py --freeze tests/ocr_corpus/freeze-b1c-final.json --annotations tests/ocr_corpus/acceptance-b1c/annotations-b1c-accept-v2-frozen-schema.jsonl --output-dir out/ocr-audit/b1c-independent-once
```

该命令先核对资产SHA256、样本数、审核信息、原生采集时间晚于冻结、旧帧复用，再顺序运行两服CPU。结果目录独占创建，不覆盖既有结果；每裁块预热2次、记录5次用于时延，“一次验收”指一个固定版本的一轮完整评估。封闭集按语言/已知scene从参考语料与新标注的候选文字中匹配，并列/拒识失败；这仍是已知词表诊断，不是未知画面的场景分类率。

主要指标：两服封闭集≥99%，日文规范化文字≥95%、繁中≥97%，数字语义≥99.5%。文字总体p50≤15ms、最慢裁块5次中位数≤50ms；数字总体p50≤5ms、最慢裁块5次中位数≤15ms；检测p95≤120ms。单次最大值保留作参考。评分版本为b1b-v2，只增加へ/ヘ、べ/ベ、ぺ/ペ三对等同。

约60条文字意味着封闭集需60/60，日文规范化至少57/60、繁中至少59/60；40条数字需40/40。按实际分母计算，不把小样本点估计当成总体保证。

跑完不再针对该集调整模型、预处理或评分；未通过就记录差距，作为下一轮起点。只有独立验收完成并通过，B1才结束；此前不启动B2或游戏任务。

## 本批执行状态

Claude 20条审核已导入，正式修订标注为 b1c-accept-v2；percent 通过仅元数据不同的旧 schema 运行副本兼容，文字与ROI完全一致，不改冻结校验器/评分。上述命令已执行一次，结果未通过，不应重复执行。详情见[独立验收报告](OCR_B1C_INDEPENDENT_ACCEPTANCE.md)。本批已看过结果，只作回归资料；B1尚未结束，B2仅在用户另行通知后开始。
