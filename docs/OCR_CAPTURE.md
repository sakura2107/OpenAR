# B1：只截图的 OCR 采集

2026-11-01：已用 MuMu 实例 0（日服）及实例 1（国际服繁中）验证原生截图。两服主批次和补采各 300 帧，采集均已自动结束。它只连接现有 MuMu、调用原生截图接口、把截图保存到本机，不运行 TaskRunner，不调用点击、按键、滑动、启停应用或修改模拟器设置。用户手动翻页。OCR 未验收，不写游戏任务。

## 用户运行

在 PowerShell 进入 `D:\OpenAR-main`。先手动启动对应 MuMu 实例和游戏；核实当前是日服或国际服繁中，记录实例编号与 ADB serial。下面的编号和 serial 是示例，必须改成对应实例的实际值；安装目录是本机已找到的路径。

```powershell
Set-Location D:\OpenAR-main
# 已构建好 CPU Release；修改代码后可以重新构建：
./scripts/build.ps1 -Profile cpu -Tests

# 日服：手动浏览约 15 分钟，默认每 3 秒保存 1 张，共 300 张。
./scripts/capture-ocr.ps1 -Language ja -MuMuPath 'D:\software\mumu\MuMuPlayer' -MuMuIndex 0 -Serial '127.0.0.1:16384'

# 国际服繁体中文：修改为这个服务器对应的实例与 serial 后，再走一遍路线。
./scripts/capture-ocr.ps1 -Language zh-Hant -MuMuPath 'D:\software\mumu\MuMuPlayer' -MuMuIndex 1 -Serial '127.0.0.1:16416'
```

在想保留的界面停留约 3–6 秒。Ctrl+C 随时停止，已经保存的图片会保留。可用 `-IntervalMs 5000 -Count 180` 改成每 5 秒采集 15 分钟。不需要额外安装 Python 就能截图。

输出到 `tests/ocr_corpus/ja/captures/<批次>/` 或 `tests/ocr_corpus/zh-Hant/captures/<批次>/`。每个批次有 `images/000001.png` 等图片与 `capture.jsonl`，记录语言、UTC 毫秒时间、来源、原始尺寸、输出尺寸、旋转、viewport、去重用像素 hash。每次使用新目录，不覆盖前一次采集。运行中逐张显示路径。

图片会在本地缩放成 1280×720；这不会修改模拟器分辨率。输入必须为横屏 16:9（1% 容差），比例不符会停止并报错，不拉伸成错误比例。需要 rotation/viewport 的特殊实例，改用设备 JSON 和底层 CLI，参照 `examples/device.mumu.json`：

```powershell
./out/cpu/Release/OpenAR.exe capture --device ./device.local.json --language ja --output ./tests/ocr_corpus/ja/captures --interval-ms 3000 --count 300
```

采集复用 OpenAR 的 ControllerMuMu 原生截图及 DeviceSession 坐标归一化；连接时现有 MuMu 后端会连接 ADB transport，图像来自 MuMu IPC。它不创建生命周期控制器，也不访问资源包或任务脚本。配置只写在 `out/ocr-capture-configs/`，不会改游戏或模拟器配置。

## 两服各走一次的路线（约 15 分钟）

时间用于分配，不要求精确卡点。各界面注意保留可读的静止画面，避免整批都是过场。

| 分钟 | 手动浏览内容 |
|---|---|
| 0–2 | 登录和公告、主页（资源栏）、邮件、任务、每日奖励 |
| 2–4 | 咖啡厅、课程表、社团 |
| 4–7 | 悬赏通缉、特殊任务、学院交流会、普通关和困难关的关卡列表；可不消耗资源地查看扫荡确认界面时再打开，停在确认前 |
| 7–8 | 战术大赛排名、对手列表；结算仅采集已有记录或自然出现的界面，缺失就标记待补，不为截图额外参赛 |
| 8–10 | 制造页面、商店各分页；只看界面 |
| 10–12 | 当期活动关卡列表、活动商店、活动货币 |
| 12–15 | 学生列表和详情：等级、星级、技能、装备；体力、青辉石、信用点和各种票券的数字显示 |

全程不领取、购买、兑换、扫荡、制造、升级或额外开战，不消耗游戏资源。每日奖励或结算如果只能通过领取/开战显示，就记录缺失，保留为后续自然出现时补采的覆盖项。不要为本次采集消耗青辉石。

刻意保留以下变化：带千分位逗号的数量、`12/20`、`x3`/`×3`、排名、等级；有描边字、小字、半透明背景字的页面；不同资源量位数、不同商店分页、学生技能和装备文字。UID、昵称等仅本地保留，默认分发包不收录测试截图。

## 采集完成后

把两批输出目录告诉 Codex。Codex 负责预标与逐条目视核对，Claude 负责覆盖审查及抽查；用户不需要逐条标注。模型预测永远不是自动通过的真值。

离线维护工具需要 Python 3.12 x64。使用独立 venv：

```powershell
python -m venv ./out/ocr-tools-venv
./out/ocr-tools-venv/Scripts/python.exe -m pip install -r ./scripts/ocr-tools.requirements.txt
```

以下命令由负责标注的 Codex 执行，`BATCH` 替换成实际批次目录：

```powershell
# 无模型也能生成每张唯一截图的空标注模板，不覆盖已有文件。
python ./scripts/ocr_corpus.py init --capture BATCH/capture.jsonl --output BATCH/annotations.draft.jsonl

# 日服预标：调用原有 C++ OCR DLL，保留旧行为作为基线；无需 Python ORT。
# 与 init 二选一，或使用不同输出名。预标只能给候选框，必须人工补漏框、拆错框。
python ./scripts/ocr_prelabel.py --capture BATCH/capture.jsonl --output BATCH/native.draft.jsonl --plugin ./out/cpu/Release/plugins/OcrPaddleCpu.dll --det-model ./models/det_ja.onnx --rec-model ./models/rec_ja.onnx --dictionary ./models/japan_dict.txt
```

官方候选已转换为 ONNX；当前两服候选使用 PP-OCRv5_mobile_rec，完整配置在 models/ocr/ja.json 和 models/ocr/zh-Hant.json。它们仍标记 candidate，尚未通过准确率验收；不得将候选当成已验收。预标时每个框保存 `prediction`，`text` 保持 null、`reviewed` 保持 false；人工对照原图确认后才填正确文字、复核人和带时区时间。漏检的文字要人工新增框，不能仅验模型已检测到的框。预标只输出整帧检测+识别的原始耗时，含冷启动，不能用作裁块识别或检测独立耗时。

## 标注约定与门禁

一行 JSON 一个区域，坐标是归一化 1280×720 图片上的 `[x,y,width,height]`。模板由 `init` 或 `ocr_prelabel.py` 生成，包括：

```json
{"schema_version":1,"id":"ja-example-region-001","language":"ja","image":"images/000001.png","image_sha256":"实际PNG的SHA256","scene":"home","split":"test","kind":"numbers","number_form":"fraction","styles":["outlined","translucent"],"box":[100,20,85,24],"text":"12/20","prediction":"12/20","reviewed":true,"reviewer":"实际逐条复核人","reviewed_at":"2026-11-01T10:00:00-08:00","audited_by":"","audited_at":""}
```

示例中的 true 是描述完成核对后的记录，不能直接复制成实际审核证据。

- `language`：`ja` 或 `zh-Hant`。`kind`：`ui` 或 `numbers`。
- `scene` 枚举见 `scripts/ocr_corpus.py` 的 SCENES，覆盖上表全部界面。`styles` 用 `outlined` / `small` / `translucent` 标签，可多选。
- 数字的 `number_form`：`integer`、`thousands`、`fraction`、`multiplier`、`ranking`、`level`；UI 记录留空。
- `split`：`tune` 或 `test`。按采集会话/近似页面分组预先冻结，不把同一画面或连续动画近似帧分到两侧；工具可检查完全相同帧泄漏，近似帧仍需人工检查。
- 每种语言至少 **150 条 UI、100 条数字**作为独立 test；调参数据另存 tune。相同截图的重复区域不计数，几乎相同区域（IoU > .9）也被拒绝。同一字符在不同真实样式中可以出现，避免用相同数字重复凑数量。
- 每条必须记录逐条复核人和时间。Claude 抽查填不同的 `audited_by` 和 `audited_at`；尚未抽查的记录留空，不代签。
- UI 完全匹配只去掉首尾空格、统一全角 ASCII 与半角 ASCII/全角空格；不使用会折叠其他兼容字的 NFKC，不做简繁转换。数字按原串完全匹配，不删逗号、不改 `x`/`×`。

核对后合并并检查：

```powershell
python ./scripts/ocr_corpus.py merge --annotations JA_BATCH/reviewed.jsonl CHT_BATCH/reviewed.jsonl --output ./tests/ocr_corpus/annotations.jsonl
python ./scripts/ocr_corpus.py validate --annotations ./tests/ocr_corpus/annotations.jsonl
python -m unittest discover -s ./tests -p test_ocr_tools.py -v
```

merge 只重定位图片相对路径并合并，保留审核状态；拒绝同名 ID，不覆盖输出文件。validate 不通过时返回 2，并列出样本数不足、漏场景、缺样式、未复核、重复框、图片 hash 改变、越界和 tune/test 泄漏。它检查证据完整性，不能证明人工实际完成复核；最终由 Codex 的目视核对记录和 Claude 的审查补足。

两端 native 预标需要对照时，可分别使用 CPU/GPU 输出目录下的 OCR DLL，然后运行 `python ./scripts/ocr_compare.py --cpu CPU_PROPOSALS.jsonl --gpu GPU_PROPOSALS.jsonl --output NEW_DIFF.json`。比较按 image SHA256 与区域 IoU 匹配，分别列文字、框、顺序及未配对差异；它不是准确率测试，仍需要人工真值。GPU DLL 与 CUDA/cuDNN 依赖必须来自对应已 stage 的 GPU 输出目录，工具仅在自身进程调整 DLL 搜索路径。

100 条数字时 ≥99.5% 意味着必须 100/100 正确；这只是最小样本的点估计，最终报告必须同时给出分子/分母。每种语言建议进一步扩大数字样本，不把不足数据或预标结果当成通过。

## 已完成采集与复现

B1c最终冻结及新独立验收以 [OCR_B1C_ACCEPTANCE](OCR_B1C_ACCEPTANCE.md) 为准；新批次必须晚于 `freeze-b1c-final.json`，不覆盖旧语料。以下B1/B1b段落保留为历史记录。

B1b已将旧test限定为回归集，新增ROI/审核版本为 `tests/ocr_corpus/annotations-b1b-v4.jsonl`。新验收批次尚未采集；采前冻结和两服60文字/40数字计划见 [OCR_B1B_ACCEPTANCE_PLAN](OCR_B1B_ACCEPTANCE_PLAN.md)，最新结果见 [OCR_B1B](OCR_B1B.md)。以下文件名描述B1冻结版本，勿覆盖它们。

正式合并标注为 `tests/ocr_corpus/annotations.jsonl`，模型和数据冻结哈希在 `tests/ocr_corpus/freeze-final.json`。原始预标文件保持未审核；只有经过逐条目视核对的区域进入合并文件。标注中未代填 Claude 的抽查字段。完整实测、缺项和重跑命令见 `docs/OCR_HEALTHCHECK.md`。

旧主批次和补采批次各 600 张（两服合计 1200 张）；另保留日服启动、更新期间 145 张早期截图，不计入正式采集规模或验收。每次采集仍必须确认实际实例语言。
