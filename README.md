# OpenAR

**基于 C++ 与计算机视觉的通用安卓界面自动化运行时**

OpenAR 是一个面向安卓模拟器（ADB / MuMu）的自动化框架。它负责截图、识别、点击、超时、取消、恢复和诊断。业务脚本只要描述三件事：“当前在哪个界面、做什么动作、动作之后应该看到什么”。框架本身不依赖任何特定游戏，蔚蓝档案、明日方舟等只是接入示例。具体游戏的任务流程放在各自独立的仓库里。

> **English summary.** OpenAR is a C++20 runtime for automating Android UIs on emulators (ADB / MuMu). It captures frames, recognises them with template matching (NCC / SSIM, CPU or CUDA) and PaddleOCR (ONNX Runtime, CPU or GPU), performs input, and confirms every transition on a fresh frame. Deadlines, cooperative cancellation, bounded recovery and full traces with deterministic replay are built in. Recognition and device backends are DLL plugins behind a C ABI. Game-specific logic lives in separate resource packs and repositories; the core stays game-agnostic.

## 特性

- **可确认的状态转换**：每一步都是“识别 → 动作 → 重新截图确认”。点击之后即使返回错误，也会重新识别当前画面，不会盲目重复可能已经生效的操作。
- **两种编写方式**
  - **JSON 资源包**（pipeline）：声明状态、动作和后继状态，加载时做 Schema、引用、ROI、正则和预算校验。
  - **C++ 反应循环 API**（`ar/include/loop`）：用 `Task` / `Phase` / `see(...)` 写 do-while 风格的流程，带恢复锚点和 JSONL 事件流。
- **识别后端**
  - 模板匹配：NCC、SSIM，各有 CPU 版和 CUDA 版。
  - OCR：PaddleOCR（PP-OCRv3/v4/v5）ONNX 推理，支持按服务器配置的 profile（日服 `ja`、繁中 `zh-Hant`），包括字典规范化、OpenCC 简繁转换、CTC 字符屏蔽、数字专用模型和裁块直识别。
- **设备层**：ADB 和 MuMu 原生 SDK 控制器。按设备 ID 互斥；Frame 带会话和代次，旧画面不能被拿去点击；参考分辨率、旋转和 viewport 不匹配时直接拒绝，不会猜坐标。
- **可靠性**：所有操作共享一个截止时间并支持协作式取消；子进程用 Job Object 统一收回；错误分类保留后端和原生错误码；插件加载时检查完整导出，创建失败不会留下不可用的对象。
- **JobHost 任务宿主**（`ar/include/host`）：设备队列、并发、取消、幂等提交、重启后历史可恢复、带游标的事件流，供 GUI 或外部进程接入。
- **可观测与回放**：Trace 记录事件和证据帧，可以离线精确回放识别决策，用来复现问题和做回归测试。
- **插件化**：算法和设备后端都是 DLL，通过 C ABI（v1 兼容，v2 显式传入缓冲区长度并由插件自己释放结果句柄）加载，C++ 类型不跨 DLL 边界。

## 架构

```text
CLI / SDK / JobHost
  ├─ ResourceBundle：manifest + pipeline + 模板 + OCR 配置（不可变、共享）
  ├─ Tasker / TaskRunner / loop::Context：识别 → 动作 → 确认
  │    ├─ 取消、总/节点截止时间、转换/恢复预算
  │    ├─ Extensions：自定义识别与动作
  │    └─ Trace / Replay：事件、证据帧、确定性回放
  └─ DeviceSession：设备互斥、帧代次、坐标变换
       └─ IController → ADB / MuMu 插件
识别服务 → NCC / SSIM / PaddleOCR 插件（CPU / GPU）
```

详见 [架构与可靠性边界](docs/ARCHITECTURE.md)、[资源与 SDK 指南](docs/RESOURCE_GUIDE.md)、[反应循环 API](docs/LOOP.md)、[JSONL 事件协议](docs/LOOP_EVENTS.md)、[JobHost](docs/HOST.md)。

## 目录

| 路径 | 内容 |
|---|---|
| `include/ar/OpenAR.h`、`ar/include` | 公共 SDK 头文件 |
| `ar/src/runtime` | 会话、资源、任务、回放、Trace |
| `ar/src/loop` | C++ 反应循环与模拟器 |
| `ar/src/host` | JobHost 任务宿主 |
| `plugins/` | `matcher`（NCC/SSIM，CPU/CUDA）、`ocr`（Paddle CPU/GPU）、`control`（ADB/MuMu）、`device`（MuMu） |
| `apps/cli` | `OpenAR.exe` 命令行宿主 |
| `examples/` | SDK 接入示例、设备配置模板 |
| `resources/android-demo` | 离线演示资源包（测试图片由代码生成） |
| `models/ocr` | OCR profile 配置与 OpenCC 词表 |
| `schemas/` | 资源包与事件的 JSON Schema |
| `tests/`、`benchmarks/` | 单元/集成/故障注入/回放测试，性能基准；`tests/ocr_corpus` 为 OCR 标注语料 |
| `scripts/` | 构建、检查、打包、OCR 采集/标注/验收/基准工具 |

## 构建

环境：Windows x64、VS2022（C++ 桌面开发）、Windows SDK。公共接口 C++20，CUDA 内核 C++17。

1. 复制 `deps.example.props` 为 `deps.local.props`（已忽略），填写 ONNX Runtime CPU/GPU 路径。
2. 把 OpenCV 4.10 放到 `3rdparty/include`、`3rdparty/lib/opencv`、`3rdparty/bin`。二进制依赖不随仓库分发。
3. GPU 构建另需 CUDA 12.9、cuDNN 9 和 NVIDIA 驱动。
4. OCR 官方 ONNX 模型不在仓库中，用 `scripts/ocr_prepare_models.py` 下载到 `models/official/`（来源与校验信息见同目录的 `*.provenance.json`）。

```powershell
./scripts/build.ps1 -Profile cpu -Tests
./scripts/check.ps1 -Profile cpu
./scripts/check.ps1 -Profile gpu   # 可选
```

`OpenAR.cpu.sln` 是默认的 CPU 构建，`OpenAR.gpu.sln` 是 GPU 构建，输出在 `out/<profile>/Release`。

## 快速体验（无需设备）

```powershell
$cli = './out/cpu/Release/OpenAR.exe'
& $cli doctor
& $cli validate --resource ./resources/android-demo
& $cli replay --resource ./resources/android-demo --replay ./resources/android-demo/replay.json --trace ./out/traces
& $cli replay --resource ./resources/android-demo --replay ./resources/android-demo/replay-uncertain.json --trace ./out/traces
```

演示流程是“识别按钮 → 点击 → 确认进入目标页”，另外还演示了点击返回错误但实际已生效时的恢复路径。`make-demo --output DIR` 可以重新生成演示资源，`schemas --output DIR` 导出校验规则。

## 连接真实设备

复制 `examples/device.adb.json` 或 `examples/device.mumu.json` 为 `device.local.json`，填写 serial、MuMu 安装目录和实例编号：

```powershell
& $cli run --resource YOUR_RESOURCE --device ./device.local.json --models ./models --trace ./out/traces
```

按 Ctrl+C 请求取消。MuMu 原生 SDK 负责截图和触摸；按键、滑动、应用启停走 ADB。

## 打包

```powershell
./scripts/package.ps1 -Profile cpu -IncludeModels   # 输出 dist/OpenAR-cpu，含 SHA256 清单
./scripts/package.ps1 -Profile gpu
```

目标机需要 MSVC x64 运行库，GPU 版另需驱动。

## 开发进度

| 阶段 | 内容 | 状态 |
|---|---|---|
| 重构 | 从旧框架重构为通用运行时：错误语义、插件 ABI v2、截止时间/取消、Trace 回放、CPU/GPU 分离构建 | 完成 · [计划](REFACTOR_PLAN.md) · [验收](docs/VALIDATION.md) |
| B1 OCR | 两服真实截图语料、标注与冻结清单；profile 化 OCR；数字修复、CTC 屏蔽；独立验收 | 有条件通过 · [B1d 报告](docs/OCR_B1D.md) · [体检](docs/OCR_HEALTHCHECK.md) |
| B2 循环 | C++ 反应循环 API、JSONL 事件、导航 settle/busy、有限恢复；离线回放与真机验收 | 完成 · [B2c 真机验收](docs/LOOP_B2C_NATIVE.md) |
| B3 宿主 | JobHost：设备队列、取消、幂等、事件游标、可恢复历史 | 完成 · [HOST](docs/HOST.md) |

## 许可与致谢

本项目以 [MIT](LICENSE) 许可发布。第三方库与模型（OpenCV、ONNX Runtime、PaddleOCR 模型、OpenCC 词表等）的许可见 [docs/THIRD_PARTY.md](docs/THIRD_PARTY.md) 和 `docs/licenses/`。

设计上参考了 [MaaFramework](https://github.com/MaaXYZ/MaaFramework) 的 Controller / Resource / Tasker 分层与 Pipeline 思路，以及 BAAH 的“前置判断 → 动作 → 完成确认”模式。本项目没有复制这两者的代码或资源，也不承诺兼容它们的 JSON / C API。

本项目由 [sakura2107](https://github.com/sakura2107) 与 GPT 协作开发。
