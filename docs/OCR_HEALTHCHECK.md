# OpenAR B1 OCR 体检与回归交付报告

日期：2026-11-01。代码与数据以 `D:\OpenAR-main` 为准。

**结论：本轮代码体检、采集、标注、候选接入和测试报告已完成；OCR 验收未通过，不能开始 BA 任务脚本。** 原任务允许未达标时据实报告，本报告没有把失败指标改成通过。

当前四组结果使用重新构建的 CPU/GPU DLL：`completion-v4-<语言>-<cpu|gpu>.json`。重新核对了每个结果的真值、ROI、样本集合以及 DLL、ORT、模型、字典、配置、标注 SHA256，均一致。原始 `verified-v3-*` 等历史结果保留。

## 验收结果

| CPU 指标 | 日服 | 繁中 | 要求 / 结论 |
|---|---:|---:|---|
| 界面文字整串准确率 | 371/411 = 90.27% | 333/362 = 91.99% | ≥97%；均未通过，分别差 6.73/5.01 个百分点 |
| 数字原串准确率 | 118/127 = 92.91% | 119/130 = 91.54% | ≥99.5%；均未通过，分别差 6.59/7.96 个百分点 |
| 文字调用总体 p50 | 10.148 ms | 10.183 ms | 总体中位数在预算内；不能代替逐裁块检查 |
| 文字逐裁块超时数 | 22/411 | 20/362 | 每裁块 p50 ≤15 ms；均未全部通过 |
| 文字最慢裁块 p50 | 43.398 ms | 40.454 ms | 慢区域明细见 summary JSON |
| 数字调用总体 p50 | 2.165 ms | 1.875 ms | 总体中位数在预算内；不能代替逐裁块检查 |
| 数字逐裁块超时数 | 29/127 | 43/130 | 每裁块 p50 ≤5 ms；均未全部通过 |
| 数字最慢裁块 p50 | 12.850 ms | 14.556 ms | 慢区域明细见 summary JSON |
| 本轮检测 p50 / p95 / 最大 | 80.04 / 103.54 / 118.66 ms | 79.83 / 105.30 / 116.54 ms | 本轮均 ≤120 ms；历史仍有超标 |
| 历史 verified-v3 检测最大 | 139.31 ms | 127.34 ms | 各一次 >120 ms；尚未证明稳定满足硬上限 |
| 当前 OpenARTests | 37/37 | GPU 为同一套件 39/39 | 全部通过；GPU 多两项 CUDA matcher 测试 |
| Python 标注、比较、汇总测试 | 11/11 | 同一套件 | 全部通过；包含慢裁块不能被总体中位数掩盖的回归 |
| 覆盖 / Claude 抽查 | 缺竞技场结算 | 缺每日奖励、竞技场结算 | 0 条 Claude 签署；总体验收仍关闭 |

此前续接消息把总体中位数当作“速度达标”，该说法不够严格。本轮逐个 ROI 取 5 次调用的中位数，再与预算比较；文字和数字都存在慢裁块。新的 `scripts/ocr_report.py` 会保留这些失败项。

UI 比较只裁掉首尾空白、折叠全角 ASCII/空格；不删内部空格、不改大小写、不作简繁转换。数字保留逗号、空格、`x/X/×`，拒识按错误计入。以当前 127/130 条数字样本，99.5% 目标都要求全对；日服还需纠正 9 条、繁中 11 条。文字达到 97% 至少需分别对 399/352 条，当前还差 28/19 条。

## 测试条件与边界

- CPU：Intel Core i5-14600KF；GPU：RTX 4070 SUPER。native ORT 文件版本 `1.26.20260508.3.8c546c3`。
- 同机顺序运行四组测试，没有并行跑两组基准；未操作、关闭模拟器。操作系统和模拟器后台负载未隔离，不把有限次观测当成实时保证。
- 每个 ROI / 每帧先预热 2 次，再测 5 次。计时在 native C++ 内，包含预处理、ORT、CTC，数字包含可能的 fallback；检测包含缩放和 DB 后处理。磁盘读取、DLL/模型加载不计入热态耗时，本轮未另行量化冷启动。
- 检测输入是 1280×720 整帧，profile 内按 max-side=960 缩放，再按 32 倍数取最近尺寸（此分辨率为 960×544）。日服 34 帧 / 170 次，繁中 32 帧 / 160 次。
- 准确率为人工指定 ROI 的识别准确率，不能作为端到端检测召回率；本轮未量化检测 recall。测试集中存在相同字符串及相关界面，不能将 ROI 数当成独立抽样数量。
- 参数由 tune 选择，首次 test 后未更改候选 profile / 模型；发现标注错误后保留 v2 和 v3 修订记录，最终数据哈希见 freeze-final。因已看过 test 且修过 ROI，后续若按这些错例调参，应把本集作为回归集，再新建未见过的验收集。

## 可疑点核实、修改与实证

| 项目 | 核实结果与处理 | 实证 / 限制 |
|---|---|---|
| 输入高度 48 | 本地 rec.sim 和 rec_ja 的 ONNX 输入 H 均为 48；没有证据支持“本地日文模型需要 32”。识别器现从模型读 H，动态 H 必须显式给值；冲突即拒绝。 | 模型清单；真实主页数字裁块回归；高度冲突测试。新增四个官方识别候选也都是 H=48。 |
| 补边与对齐 | 旧路径确为居中、原图灰色 114。候选 profile 使用左对齐、归一化后右补 0；旧行为可由配置复现。 | 日服 123 条 tune 单变量对照：官方预处理104对、仅居中105对、仅灰114为104对。不能声称左对齐已证明提高本集准确率。 |
| BGR/RGB | 旧代码交换为 RGB，候选明确使用 BGR；均值/std与交换次序也进配置。 | 同一 tune：BGR104/123，单改RGB102/123；是本批局部证据，不外推所有旧模型。 |
| 字典/类别 | 日文字典与官方一致；旧简中字典虽行数匹配，却有29处字符差异，类别数检查无法发现这些内容损坏。未覆盖用户旧文件；新候选使用同源 inference.yml 原始字典。 | 启动和推理时检查 C=字典行数+blank+配置space，保留UTF-8/GBK兼容测试、类别冲突失败测试。 |
| 检测参数 | bitmap阈值和box阈值分离；profile明确缩放、取整、通道、阈值、unclip、score模式。CPU/GPU共用实现。 | 当前阈值0.3/0.6、unclip1.5、关闭相邻框merge；检测热态时延及整帧CPU/GPU对照见证据。未验证全部检测召回。 |
| CTC与低置信 | 支持sequence_mean及旧逐字符过滤；保持CTC重复规则，数字只扫描blank与允许类别。 | tune官方均值104/123、单改旧字符过滤101/123；不作词典替换或纠错补分。 |
| 阅读顺序 | 修正后处理比较器将 y 比较自身的错误；比较工具按位置配对，不把GPU多框造成的数组偏移算成阅读顺序变化。 | 两服各3张真实整帧对照；新增额外框不制造顺序误报的Python回归。 |
| 插件与失败路径 | 增加可选profile工厂和crop/数字/检测模式导出，v1/v2 C布局保持不变；旧插件不支持时显式拒绝。 | profile、ROI坐标、UTF-8、错误配置与旧插件拒绝测试；C++接口/vtable已扩展，旧C++消费者应重编译。 |

单变量对照文件：`out/ocr-audit/ablation/recognition-tune-comparison.json`。这些是调参诊断，非最终test成绩。不是每一项候选改动都已用真实数据证明了正向收益，报告保留这一限制。

## 多语言配置与模型选择

正式运行候选：`models/ocr/ja.json` 和 `models/ocr/zh-Hant.json`，均标记 `candidate`。旧行为对照及其他候选也保留在该目录。配置schema_version=1；运行时只依赖C++/ONNX Runtime，不依赖Python或Paddle。

| 配置段 | 主要字段 / 当前值 |
|---|---|
| 元数据 | id、language、status；language仅元数据，不在C++中按语言硬编码模型 |
| detector | model、channel_order、normalization_order、scale、mean/std、limit_type/side_len、resize_rounding、bitmap/box_threshold、unclip_ratio、score_mode、line_merge_ratio |
| recognizer | model/dictionary、input_height=model、dynamic_height、min/max_width、width_multiple、batch、归一化、alignment、padding_stage/value、blank_index、use_space_char、confidence_mode、text_threshold |
| numbers | allowed_characters、minimum_width、独立model/dictionary、线程/优化、fallback模型及below_confidence |
| runtime | detector 8线程+extended；recognizer 2线程+all；数字两模型均4线程+all |

路径相对于profile解析；未知字段、缺资产、类型范围错误明确拒绝。固定模型输入H与配置冲突拒绝；字典类别数在初始化及输出解码时检查。增加一种兼容的CTC/NCHW模型语言，只需放模型/字典和profile、再测语料；当前不支持任意非CTC架构。

两服文字暂选 PP-OCRv5_mobile_rec + PP-OCRv5_mobile_det。繁中同一批115条tune：v5为106/115（92.17%），繁中专用v3为77/115（66.96%）；日服123条tune：v5为104/123，日文v3为76/123。选v5有相对优势，仍未达到97%，不称为验收模型。

数字通道跳过检测，先日文v3+CTC字符屏蔽，置信度<0.85或无结果时改用英文v4；fallback不是投票，也不保证比首个结果正确。数字tune日服29/29，最终test仅118/127，显示小样本调参成功不能替代独立测试。字体模板原型跨图仅12/29，未接入。

五个官方候选固定revision、SHA256、导出命令、Paddle3.1.1/Paddle2ONNX2.1.0/ONNX1.17.0与实际opset17都在各自provenance。官方模型卡与Apache-2.0全文已存档于 `docs/licenses/PaddleOCR/`，具体来源和分发条件见 [THIRD_PARTY](THIRD_PARTY.md)。旧四个ONNX来源仍缺记录，未把新模型许可追溯套给它们。当前未制作分发包。

导出核对：五个模型各用一个真实BA输入与Paddle原模型对照，四个识别模型逐时步argmax一致，检测阈值mask一致；识别最大绝对差约1.2e-7至2.9e-6，检测2.6e-5。输入覆盖很有限，不构成对所有输入的等价性证明。见 `export-equivalence-real.json`。

## 数据规模、覆盖与独立审查

正式主采集与补采两服各600帧，共1200帧；另保留145张日服早期启动/更新截图。最终标注1,357条，来自78张选定1280×720原图；其中test1,030条、tune327条。

| 语言 | tune文字 / 数字 | test文字 / 数字 | test选图数 |
|---|---:|---:|---:|
| 日服 | 123 / 29 | 411 / 127 | 34 |
| 繁中 | 124 / 51 | 362 / 130 | 32 |

正式入口 `tests/ocr_corpus/annotations.jsonl`；图像在各语言的captures/supplement下。历史草稿、proposals和复核副本不得重复合并。冻结清单 `freeze-final.json` 的18项哈希全部匹配；标注SHA256为 `dd1445af8466d1f1a07b70013e03826d1961d2a9eabdce89f0aa6c877c094aa2`。

逐条Codex核对记录已在原会话完成并存入标注；本次续接核对文件完整性和回归，不重新签署1,357条。v2记录11处拆字段/文字等修正，v3再记录11处ROI边界修正；未改模型参数。相同帧跨split、重复ROI、越界和图像hash检查均无错误，但近似界面分组与主观空格标注仍需独立审查。

覆盖尚缺：日服竞技场结算、繁中每日奖励和竞技场结算。没有为补这些画面领取奖励或开战。已含描边、小字、半透明背景及各类数字；这些标签可重叠。

| CPU test子类 | 日服正确/总数 | 繁中正确/总数 |
|---|---:|---:|
| ui/outlined | 144/165 | 94/107 |
| ui/small | 163/185 | 96/115 |
| ui/translucent | 39/40 | 42/42 |
| numbers/outlined | 52/54 | 19/21 |
| numbers/small | 62/67 | 86/96 |
| numbers/translucent | 13/13 | 20/23 |

Claude抽查仍未完成，`audited_by/audited_at`保持为空。本轮提供24条固定分层样本（含两处CPU/GPU差异）、逐条裁块、对应整帧、HTML审阅页及空白复核CSV；该抽查包不会自动改标注或代签。Claude还应检查近似帧分组、内部空格约定及是否遗漏未检出的文字。

## CPU / GPU差异

| 语言 / id | 真值 | CPU | GPU |
|---|---|---|---|
| ja-c91f5e9712ea-region-027 | Lv.90 | LV.90 | Lv.90 |
| zh-Hant-209d92a15c5c-region-026 | 調查那前所未見的AI「十字神名」的真面目。 | 調查那前所未見的AI「字神名」的真面目。 | 調查那前所未見的AI「十字神名」的真面目。 |

当前日服538条、繁中492条test裁块分别有1条CPU/GPU原串差异；数字完全一致。四组各自5次重复均稳定。GPU准确率：日服文字372/411（90.51%）、繁中文字334/362（92.27%），数字与CPU相同。原串差异不作归一化后隐藏。

另有较早的各3张整帧对照：日服CPU131框/GPU133框，131框按位置匹配，有1处Lv大小写差异、1处框偏移、2个GPU额外图标小框；繁中各123框，全部一致。修正比较逻辑后无阅读顺序差异。该小批整帧对照未在本轮扩大，不能声称所有整帧结果一致。

## 卡点和后续边界

- 文字错例含日文小假名/浊点、ミカ→三力、Lv→LV、繁简字混淆与漏字、图标干扰；内部空格是另一类严格比较失败，不能为了达标统一删除。
- 数字含x1拒识、倍率前缀x/X/×、排行区间空格及描边斜体问题；高置信误识别可能绕过fallback。完整错误列表保留原串，不凭猜测给错例归因。
- 逐ROI超时集中在宽文本与部分数字fallback路径；这只是路径和耗时上的线索，本轮没有逐算子profiling，未宣称根因已确认。
- 若继续提高准确率，应在tune/新训练数据比较ROI预处理、模型或数字方案，本次test保留作回归；需要新的未见过验收集，不能反复看test后仍宣称独立。
- 用户目前不用再翻页。Claude可先审查现有包；仅在自然可见且无需消耗资源时补3个缺场景。当前没有采集进程或测试任务留在后台。

## 复现命令

以下命令均在 `D:\OpenAR-main` 运行。重新构建会改变DLL哈希，重跑时使用新输出名；不要覆盖已有证据。

```powershell
Set-Location D:\OpenAR-main
$ocrPython = 'C:\Users\caiji\Documents\Codex\2026-11-01\b1-openar-ocr-gpt-codex-ba\work\ocr-venv\Scripts\python.exe'
./scripts/build.ps1 -Profile cpu -Tests
./scripts/build.ps1 -Profile gpu -Tests
& $ocrPython -m unittest discover -s tests -p 'test_ocr_*.py' -v
& $ocrPython scripts/ocr_corpus.py validate --annotations tests/ocr_corpus/annotations.jsonl
# validate当前因3个场景缺项返回2；record_errors应为空，不能将退出2视为通过。
& $ocrPython scripts/ocr_benchmark.py --annotations tests/ocr_corpus/annotations.jsonl --profile models/ocr/ja.json --plugin out/cpu/Release/plugins/OcrPaddleCpu.dll --output out/ocr-audit/rerun-ja-cpu.json --warmup 2 --repeats 5 --allow-incomplete
# 其余三组：改语言为zh-Hant，或插件为out/gpu/Release/plugins/OcrPaddleGpu.dll；每次改输出名。
& $ocrPython scripts/ocr_report.py --cpu out/ocr-audit/rerun-ja-cpu.json --gpu out/ocr-audit/rerun-ja-gpu.json --output out/ocr-audit/rerun-ja-summary.json
```

`--allow-incomplete`仅允许测缺场景的有效已审核数据；标注记录错误仍会拒绝，且输出acceptance=false。原生采集命令及完整两服路线见 [OCR_CAPTURE](OCR_CAPTURE.md)。当前无需重新采集或安装依赖。

## 改动范围与交付文件

| 范围 | 文件 |
|---|---|
| 截图CLI/契约 | apps/cli/main.cpp、Capture.h；scripts/capture-ocr.ps1；tests/CaptureTests.h |
| OCR核心 | plugins/ocr/paddle_common/{OcrConfig.h,TextRecognizer.*,TextDetector.*,PaddleEngine.h,OrtSession.hpp,Dictionary.*}；3rdparty/src/ppocr/utility.cpp |
| 配置和接口 | ar/include/{vision/Ocr.h,plugin/PluginApiV2.h}；ar/src/plugin/{PluginAdapters.hpp,PluginLoader.cpp}；ar/src/runtime/Resource.cpp；plugins/common/OcrExports.h |
| 模型/语料/工具 | models/ocr、models/official；tests/ocr_corpus；scripts/ocr_{inspect,prepare_models,prelabel,corpus,benchmark,compare,review_sheets,report}.py；两个requirements文件 |
| 构建和回归 | tests/main.cpp、test_ocr_tools.py、test_ocr_report.py；projects/vs/OpenARTests/OpenARTests.vcxproj；scripts/stage-runtime.ps1 |
| 文档 | README.md；docs/OCR_CAPTURE.md、OCR_HEALTHCHECK.md、OCR_DICTIONARY.md、OCR_CLAUDE_AUDIT.md、THIRD_PARTY.md、licenses |

以上是整个B1工作区改动，含前轮已有的字典兼容修改；不是本次续接独立创作清单。当前没有提交git commit或发布包；`.vs/`和`work/`不是交付代码。完整路径状态另存completion-git-status.txt。未访问或复用D:\BAAX，没有编写游戏任务。

本次续接新增逐ROI汇总脚本及2项回归，重新构建和跑四组基准，替换过时体检报告，整理Claude抽查材料及证据哈希。输出目录提供可独立阅读的报告、证据JSON、错误CSV和抽查zip；大体量原始语料保留在OpenAR本地目录。
