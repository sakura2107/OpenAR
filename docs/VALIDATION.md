# 验证记录

2026-10-06，本机 Windows x64、VS2022/MSVC14.44、OpenCV4.10、ORT1.26、CUDA12.9、cuDNN9、RTX4070SUPER。仅代表本机环境，远端 CI 尚未运行。

可手动触发的 CPU CI 使用 self-hosted/windows/x64/openar runner；需设置 OPENAR_ONNX_CPU_ROOT 和 checkout 外的 OPENAR_DEPENDENCY_ROOT（准备好的3rdparty目录布局）。

## 已执行

- CPU Release 构建/28 项回归、GPU Release 构建/30 项回归，资源校验及正常/不确定点击回放通过。
- 子进程空闲管道超时、取消、流分离、退出码、输出上限、Unicode、进程树；HTTP 完整性和长度上限。
- 插件 ABI/导出/创建失败、非法结果、v1 OCR 数量增长、v2 单次推理/长 UTF-8/配置/释放、模型缺失。
- ROI/步长/缓存、坐标转换、连接耗时计入预算、帧失效/隔离、恢复/循环预算、取消/冲突/异步生命周期、双设备100次启停。
- 原始 v1 matcher DLL 经新宿主加载，NCC 位置保持基线。
- MuMu 实例0：ADB 和原生 MuMu 均完成实际 Android 设置导航（启停、模板点击、Back、返回确认）。
- 原生 MuMu 连接/截图/释放100次，进程句柄前后均317。
- 最终版本复测同样通过，最新一次句柄前后均319；不同运行的基线可能不同。
- MuMu 实例0/1并行，各100次任务和截图成功，归属独立。
- 单进程60秒持续离线回放，8,472次，句柄/私有内存采样未触发增长阈值；本地 out/soak/26584/samples.csv。
- CPU 分发包复制到源码目录外的中文路径，仅保留 Windows 系统 PATH，插件、模板匹配、OCR28区域正常；包内 SDK 示例独立编译/回放成功。
- GPU 分发包仅保留系统 PATH，CPU/CUDA 模板和 CPU/GPU OCR 均通过；依赖由包内 DLL 提供，GPU OCR 为28区域。

日志、真实截图位于忽略的 out，不进入源码。真实截图分享前检查设备信息。

## 性能

预热后每后端5次，模板中心(1285,546)，分数与原基线相近；OCR 3.jpg 都为28区域。小样本仅用于发现回归。

| 后端 | 中位数ms | P95ms |
|---|---:|---:|
| NCC CPU（GPU构建） | 5.74 | — |
| NCC CUDA | 7.15 | 7.41 |
| SSIM CPU | 27.41 | — |
| SSIM CUDA | 14.59 | 15.64 |
| OCR CPU | 1397.42 | — |
| OCR GPU | 99.20 | 112.91 |

小模板 NCC 此处 CPU 更快，不能笼统认为 GPU 更快。原 v1 OCR 重复推理，原样本 CPU 约2815ms、GPU约197ms；v2 消除重复工作。计时条件存在少量差异，目标机应重复运行 OpenARBenchmark。

## 尚待验收

8小时持续运行未执行。USB 实机、其他 Android 版本完整导航、真实代理订阅/内核、硬阻塞SDK进程隔离尚未验收。

```powershell
# 单进程：真实墙钟秒数，采样句柄/内存
./out/cpu/Release/OpenARTests.exe --soak 28800 .
# 多次 CLI 启动，可通过 -Resource/-Device 指定真实任务
./scripts/soak.ps1 -Profile cpu -Seconds 28800
```

这些测试不能替代目标应用业务正确性。长期 Trace 由宿主制定磁盘保留策略。
