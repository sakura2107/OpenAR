# OpenAR

B3 后端：新增通用 [JobHost](docs/HOST.md)，提供设备队列、取消、事件游标与可恢复历史；CPU97/GPU99回归通过。BA stdio 接入、协议与便携包放在独立 BA-Auto 仓库。B2 已完成，见 [B2c真机验收](docs/LOOP_B2C_NATIVE.md)。

B2b 已增加导航 settle、busy、毫秒未知预算及有限恢复唤醒接口；CPU81/GPU83、37项循环专项、26例1744帧精确回放通过。真机新批次待用户在场同意，详见 [B2b报告](docs/LOOP_B2B.md)。

B2 新增通用 [C++ reaction loop API](docs/LOOP.md) 和 [JSONL事件协议](docs/LOOP_EVENTS.md)，与现有JSON pipeline共用底层；离线验收CPU66/GPU68及22项循环专项通过。业务导航脚本在独立仓库，主库不包含游戏流程。

B1d：按用户/Claude结论，B1有条件通过。[回归修复与冻结报告](docs/OCR_B1D.md)记录日服二级数字备用、繁中CTC字符屏蔽、CPU/GPU回归和新冻结 `freeze-b1d-final.json`。星级徽章仅在临时原型中测试，主库无游戏专用实现；用户脚本使用模板匹配。本轮与另一项编译并行，时延标为参考；未采图，B2等待用户交接。

B1c 历史记录：已完成最终冻结，入口为 [B1c 回报](docs/OCR_B1C.md) 和 [独立验收步骤](docs/OCR_B1C_ACCEPTANCE.md)。最终清单 `tests/ocr_corpus/freeze-b1c-final.json`；通用配置为 `ja-b1c-final.json` / `zh-Hant-b1c-final.json`。旧回归已达本轮目标；Claude 20条抽查及两处元数据/ROI修订后，已完成一次独立验收，结果未通过：日服数字35/40、繁中规范化文字56/60，且日服时延超标。两服封闭集均60/60。[独立验收报告](docs/OCR_B1C_INDEPENDENT_ACCEPTANCE.md)保留完整差距；未调参或重跑，B1尚未结束，B2等待用户通知。

B1b 最新进度见 [评分、数字修复与回归报告](docs/OCR_B1B.md)。两服数字旧回归集语义正确率均为100%；日文规范化文字与单次尾延迟仍有未通过项，新独立验收集尚未采集。B1b 配置为 `models/ocr/ja-b1b-v3.json` / `zh-Hant-b1b-v3.json`，修订语料为 `tests/ocr_corpus/annotations-b1b-v4.jsonl`。

B1 OCR 体检：只截图采集命令及两服手动路线见 [OCR_CAPTURE](docs/OCR_CAPTURE.md)；已核实的问题、模型/字典证据和未完成验收项见 [OCR_HEALTHCHECK](docs/OCR_HEALTHCHECK.md)。已完成两服真实截图回归，准确率和逐裁块时延仍未通过验收；验收通过前不编写游戏任务。

用 C++ 和计算机视觉开发通用安卓界面自动化。资源包描述状态、动作和后继状态，运行层处理截图、坐标变换、取消、超时、恢复和诊断。BA、明日方舟是应用示例；框架不依赖特定游戏。

保留 NCC/SSIM、PaddleOCR、ADB/MuMu 和 DLL 插件。CPU 为默认构建，GPU 独立可选。Windows x64，公共接口 C++20，CUDA 内核 C++17。

## 构建与验证

安装 VS2022 C++ 桌面工具、Windows SDK。复制 `deps.example.props` 为忽略的 `deps.local.props`，配置 ONNX Runtime CPU/GPU 路径。OpenCV 4.10 头文件/导入库/DLL 放在 `3rdparty/include`、`3rdparty/lib/opencv`、`3rdparty/bin`；新检出需自行准备二进制依赖。GPU 另需 CUDA 12.9、cuDNN 9 和 NVIDIA 驱动。

```powershell
./scripts/build.ps1 -Profile cpu -Tests
./scripts/check.ps1 -Profile cpu
./scripts/check.ps1 -Profile gpu
```

`build_release.bat`、`ARFrameWork.sln` 和 `OpenAR.cpu.sln` 默认 CPU；`OpenAR.gpu.sln` 为 GPU。输出 `out/<profile>/Release`，完整日志 `out/build-<profile>-Release.log`。`ARFrameWork.legacy.sln` 保留历史布局，新构建以新解决方案为准。

## 离线示例

```powershell
$cli = './out/cpu/Release/OpenAR.exe'
& $cli doctor
& $cli validate --resource ./resources/android-demo
& $cli replay --resource ./resources/android-demo --replay ./resources/android-demo/replay.json --trace ./out/traces
& $cli replay --resource ./resources/android-demo --replay ./resources/android-demo/replay-uncertain.json --trace ./out/traces
```

示例验证“识别按钮 → 点击 → 确认目标页”，及点击返回错误但已生效时重新识别的恢复路径。测试图片由代码生成。`make-demo --output DIR` 可重新生成，`schemas --output DIR` 导出校验规则。

## 真实设备

复制 `examples/device.adb.json` 或 `examples/device.mumu.json`，填写实际 serial、MuMu 安装目录和实例编号。`device_id` 唯一标识物理设备，建议使用 serial；同一 ID 的任务在进程内互斥。

```powershell
& $cli run --resource YOUR_RESOURCE --device ./device.local.json --models ./models --trace ./out/traces
```

`run` 执行资源声明的操作，Ctrl+C 请求取消。按键使用 Android KEYCODE。MuMu 原生 SDK 负责截图/触摸，按键、滑动和应用启停走 ADB。参考分辨率、旋转和 viewport 必须正确，长宽比失配时拒绝猜测坐标。

## 开发与分发

- `ar/include`、`include/ar/OpenAR.h`：公共 SDK。
- `ar/src/runtime`：会话、不可变资源、任务、回放、诊断。
- `plugins`：算法/控制/设备后端，v1 兼容及 v2 ABI。
- `apps/cli`、`examples/sdk`：宿主与接入示例。
- `tests`、`benchmarks`：故障注入、集成、持续运行、性能对比。
- `OpenARProxy.lib`：可选代理扩展。

PowerShell 7：`./scripts/package.ps1 -Profile cpu -IncludeModels`，产出 `dist/OpenAR-cpu`，含 CLI、DLL、SDK、资源、文档和 SHA256 清单。GPU 用 `-Profile gpu`，代理库用 `-IncludeProxy`。目标机需 MSVC x64 运行库，GPU 另需驱动。第三方及模型许可见 `docs/THIRD_PARTY.md`。

[架构](docs/ARCHITECTURE.md) · [资源与 SDK](docs/RESOURCE_GUIDE.md) · [迁移](docs/MIGRATION.md) · [验收记录](docs/VALIDATION.md)


## B1 OCR 体检与多语言候选

[OCR 实测及未通过项](docs/OCR_HEALTHCHECK.md) · [只截图采集说明](docs/OCR_CAPTURE.md)。真实语料入口是 `tests/ocr_corpus/annotations.jsonl`。候选配置 `models/ocr/ja.json`、`models/ocr/zh-Hant.json` 使用可追溯的官方 ONNX 和字典，支持配置颜色、归一化、补边、输入尺寸、阈值、线程和数字模型。两份配置状态均为 candidate，未通过 B1 总体验收；本轮不新增游戏任务脚本。

SDK 可设置 `ar::vision::OcrParams::profile` 后调用 `create_ocr`；已知文本或数字裁块使用 `recognize_crop_checked(image, roi, numbers)` 跳过检测。旧 ABI 不支持 profile/crop 时明确返回错误，不忽略配置。完整格式和复现实测见体检报告。
