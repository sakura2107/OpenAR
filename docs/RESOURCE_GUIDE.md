# 资源与 SDK

以 `resources/android-demo` 为可运行起点。manifest 定义 schema_version、名称/版本、参考尺寸、pipeline 和模板路径；`schemas` 是 CLI 导出的准确字段/必填/范围规则。相对路径（含符号链接解析）不得逃出资源包，重复键/非法 UTF-8 被拒绝。默认 JSON 上限 4 MiB、深度 64。

节点包含 recognition、action、有序 next/on_error、timeout_ms、max_visits。limits 控制总超时、轮询、转换/恢复预算；next 按优先级识别，无匹配时等待后重新截图。动作不证明完成，应用后继节点确认。点击不指定 target 时使用识别框中心；ROI 和坐标属于参考尺寸。

```json
{
  "entry": "Open",
  "limits": {"timeout_ms": 3000, "poll_ms": 50, "max_transitions": 10, "max_recoveries": 2},
  "nodes": {
    "Open": {
      "recognition": {"type": "Template", "template": "open", "threshold": 0.95},
      "action": {"type": "Click"},
      "next": ["Ready"], "on_error": ["Ready"], "timeout_ms": 1000
    },
    "Ready": {
      "recognition": {"type": "Template", "template": "ready", "threshold": 0.95},
      "action": {"type": "None"}, "timeout_ms": 1000
    }
  }
}
```

识别支持 Direct、Template、OCR、All、Any、Custom，以及 negate/ROI；OCR 文本匹配支持 exact/contains/regex。动作支持 None、Click、LongPress、Swipe、Key、Wait、StartApp、StopApp、Reconnect、Custom。以 Schema 为准，未知字段不静默忽略。

设备配置见 `examples/device.*.json`。ADB serial 支持 USB/TCP。MuMu 实例编号不是 ADB 端口，应提供正确 serial。rotation 是标准化截图的顺时针旋转，viewport 位于旋转后图片；不自动猜测黑边和布局。共享 OCR 模型目录从 `--models` 或 ResourceBundle::load 第二参数注入；字段见 manifest Schema。

后端：ncc-cpu / ssim-cpu / ncc-cuda / ssim-cuda / paddle-ocr-cpu / paddle-ocr-gpu。GPU 初始化失败不会静默降级。

## C++

见 `examples/sdk/main.cpp`、`include/ar/OpenAR.h` 和 `projects/vs/OpenARSDKExample/OpenARSDKExample.vcxproj` 的完整链接设置。链接 ARFrameworkLib.lib、OpenCV 导入库和所需 Windows 库，运行时部署 OpenCV/插件及依赖；宿主无需链接 CUDA/ORT。

顺序：检查插件加载 → 加载资源 → 创建控制器/会话 → Tasker::post_task → cancel/wait → 检查 RunReport。注册后冻结注册表。Extensions 用具名识别/动作映射接收只读帧、会话、截止时间、停止令牌和 JSON 参数；返回 Result，标准异常转为错误恢复。不能在回调外保存帧引用，也不能绕过会话使用旧截图输入。

分发包中的 `sdk/examples/OpenARSDKExample.vcxproj` 可独立用 MSBuild 构建（Release/x64），不引用源码目录；得到包根目录的 StandaloneSDKExample.exe，传入资源目录即可回放。

BA/明日方舟可各自维护资源包，例如“识别收取按钮 → 点击 → 确认已收取”，动态列表交给扩展；具体游戏素材、登录和业务流程不属于通用框架示例。
