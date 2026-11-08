# 迁移

原始源码基线提交 `71ca295`。旧 build/ARFrameWork/Release 产物未覆盖，新输出为 out/cpu、out/gpu。个人配置 deps.local.props/device.local.json 被忽略。

1. 切换新解决方案/构建脚本，保留现有模型和算法。
2. 旧工厂接口仍可使用，检查空结果；模型缺失现在创建失败。
3. v1 DLL 可经适配器使用，新 DLL 保留旧导出及 v2；v2 OCR 单次推理、结果句柄和变长 UTF-8。
4. 逐步用 DeviceSession、ResourceBundle、Tasker 替换宿主循环，业务移入资源包/扩展。
5. 默认拒绝不支持取消上下文的旧控制器；临时兼容需显式设置 SessionConfig::allow_legacy_controller，并接受取消/截止时间限制。
6. 使用 Android KEYCODE。MuMu 原生按键枚举不再混用；应用启动解析 launcher component，不使用随机 monkey 输入。
7. 每台设备提供唯一 ID，显式设置旋转/viewport，动作和等待后重新截图。

子进程非零退出、超时、输出上限、模型/插件错误和坐标失配会明确失败。应修复配置或通过恢复路径确认页面，不要无限重试。

回退在独立目录检出基线，先保存当前修改。REFACTOR_PLAN.md 是历史静态审阅，代码行号已变化。
