# 架构与可靠性边界

宿主加载插件和资源，为每台设备创建 DeviceSession。ResourceBundle 在加载时验证 JSON Schema、引用、路径、ROI、正则和预算，再以 `shared_ptr<const ResourceBundle>` 分发。Tasker 将资源、扩展和会话快照交给异步任务，宿主销毁 Tasker 不使任务引用悬空。

```text
CLI / SDK
  ├─ ResourceBundle：manifest + pipeline + 模板 + OCR 配置
  ├─ Tasker / TaskRunner：识别 → 动作 → 下一次截图确认
  │    ├─ 取消、总/节点截止时间、访问/转换/恢复预算
  │    ├─ Extensions：自定义识别和动作
  │    └─ Trace / Replay：事件、证据和确定性回放
  └─ DeviceSession：设备互斥、画面归属/代次、坐标转换
       └─ IController → ADB / MuMu
识别接口 → NCC / SSIM / PaddleOCR（CPU / GPU）
```

输入和非零等待使旧画面失效。Frame 携带会话实例、设备 ID 和代次；另一会话即使使用相同 ID 也不能借用截图点击。同一设备 ID 的会话共享任务锁，跨进程不提供锁，宿主应避免多进程同时控制同一设备。

候选识别共享同一帧，OCR 按帧/ROI 缓存单次推理。动作执行一次；错误后沿 `on_error` 重新截图识别，不自动重复可能已经生效的点击。恢复仍消耗总预算。GPU 并发许可限制同时进入的任务，不同设备可并行。

子进程执行使用独立二进制 stdout/stderr、受限继承句柄、Job Object 和统一截止时间，PNG 不混入诊断输出。错误保留类别、操作、后端和原生码。MuMu 取消长按时释放按下状态，SDK 失败使连接缓存失效。

DLL 使用绝对路径和限定搜索目录。加载器校验描述符、版本、族和对应导出；创建失败不留下不可用对象。v2 图片显式传入可读字节数，OCR 结果句柄由同一 DLL 释放。v1 导出仍可使用，新任务默认要求控制器声明取消上下文能力。保留命名导出兼容旧加载方式，本轮未改为函数表。

MaaFramework 的 Controller / Resource / Tasker 分层及 Pipeline、BAAH 的前置判断/动作/完成确认提供了设计参考。未复制其代码/资源，也不承诺兼容其 JSON/C API。参考：[集成接口](https://github.com/MaaXYZ/MaaFramework/blob/main/docs/en_us/2.2-IntegratedInterfaceOverview.md)、[Pipeline 协议](https://github.com/MaaXYZ/MaaFramework/blob/main/docs/en_us/3.1-PipelineProtocol.md)。

## 边界

取消是协作式。ADB 子进程可终止；单次 MuMu SDK、OpenCV、ONNX 调用不能执行中硬中断，返回后检查截止时间。插件访问违规/永久阻塞不受 C++ 异常保护，强隔离需另设后端进程。自定义扩展应调用 `ExtensionContext::check()`、`remaining_ms()`。

Trace 使用时间/进程/计数标识的独立目录，写 JSONL 事件、识别框/分数、动作参数和错误，失败保存原图及带框标准化图片。保留策略由宿主管理，本轮不自动删除历史目录。复合识别的每个子 ROI 尚未单独可视化。

代理扩展区分自管/外部内核，恢复设备原代理，只清理自己的 ADB reverse，自管进程用 Job 回收。HTTPS 默认校验证书，显式不安全开关仅影响对应订阅请求。同步 WinHTTP 在当前 I/O 返回后响应取消，外部内核组选择会影响共享内核。
