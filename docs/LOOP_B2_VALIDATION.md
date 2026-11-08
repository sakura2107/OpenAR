# B2：离线验收完成，真机按用户要求延期

**通用循环框架、BA-Auto 导航骨架和离线测试已完成。OpenAR CPU 66/66、GPU 68/68；其中循环专项22项。两服 B1 原图状态图共20/20符合预期，每例逐帧决策回放一致。真机已运行，日服两项成功、繁中回大厅失败并停止，繁中巡视未运行；详见[真机报告](LOOP_B2_LIVE.md)。用户已选择“稍后再做真机测试”，本轮不运行真机。**

20例中16例导航成功/已在目标，4例是资源弹窗起始：按预期 blocked、零输入，不把它们写成“导航成功”。没有写日常任务、没有提交 commit，没有访问或参考 D:\BAAX。

## 最终 API 与草图的差异

```cpp
using namespace ar::loop;
Options options;
options.full_frames = true; // 录制精确回放所需的原尺寸图
Context ctx(session, template_root, "ja", options);
ctx.recover(business_recovery_phase);
ctx.events([](const std::string& json) { std::cout << json << '\n'; });

Task("导航演示")
    .done(completed("完成阶段"))
    .phase(Phase("完成阶段")
        .goal(see("page/title", roi(90, 0, 330, 55)))
        .on("打开页面", see("home/entry", roi(150, 665, 130, 48)), tap())
        .timeout(30s).max_frames(250))
    .timeout(90s).retries(2).run(ctx, stop_token);
```

- 模块按现有工程结构放在 `ar/include/loop` 和 `ar/src/loop`，不是单层 ar/loop；只含通用能力。
- `.on(condition, action)` 保留，也可显式给规则名。稳定名称用于 trace 和回放，按注册顺序取首个命中。`tap()` 使用识别框中心；复合条件要把待点击的正向视觉条件放第一位，禁止独立用 gone/纯布尔检查点充当点击位置。
- `Task.run` 负责完成判断、恢复和从头重跑；`Phase.run` 只执行一个阶段。新增 `completed(phase)` 任务内记录及显式 `.checkpoint()`。**默认仍重跑全部阶段**，避免恢复后跳过必要导航；只有业务确认“完成后无需重做”的阶段才标 checkpoint。防重复的副作用记录跨恢复保留，与导航检查点分开。
- 副作用仍使用 `tap().effect().expect(condition, timeout)`，缺 expect 在启动时拒绝。尝试记录在调用 perform 前写入；回报丢失先观察确认，确认超时 outcome_unknown。恢复后仍不重复该动作。未决确认不能被阶段目标为真绕过。
- 增加 `ctx.guard` 和带 ROI 的纯 `custom` 条件。目标仍先计算；显式禁止状态可在背景目标可见时阻止退出/输入，避免弹窗下面露出大厅锚点时误判。BA 弹窗判定代码全在 BA-Auto，主库不知道资源或大厅。
- 默认成功帧缩小保存，失败帧保留完整图和原图。精确回放需要 `full_frames=true`，本轮全部演示已启用；仅缩小图的 trace 会明确拒绝精确回放。`Task.replay(ctx, path, observer)` 不连设备，逐帧输出 recorded/selected/equal，同时比对点击目标；原录制不改写。

完整[API说明](LOOP.md)、[事件协议](LOOP_EVENTS.md)、[事件JSON Schema](loop-event.schema.json)。OCR走已有 recognize_crop_checked 并缓存同帧同ROI；原始拒识/不匹配文字同样录入。没有新增 OCR 参数或识别后文字替换。

## 要求的离线测试

| 要求 | 结果 |
|---|---|
| 目标已达成时零动作退出 | 通过（CPU / GPU） |
| 一帧只执行一个动作 | 通过（CPU / GPU） |
| 副作用确认超时不重复，进入恢复 | 通过（CPU / GPU） |
| 全局 .effect() 被拒绝 | 通过（CPU / GPU） |
| 同规则/不变画面卡住触发恢复 | 通过（CPU / GPU） |
| 恢复后从头运行，完成判断跳过已做部分 | 通过（CPU / GPU） |
| 取消在一帧内生效 | 通过（CPU / GPU） |
| 动画不停时稳定等待到上限继续 | 通过（CPU / GPU） |
| 无 ROI 启动即报错 | 通过（CPU / GPU） |
| 录制/回放决策一致 | 通过（CPU / GPU） |

此外通过：显式整屏理由、第二层“恢复后必须重新导航”回归、显式检查点跳过、确认窗口/防重记录回放、缩小图拒绝回放、纯ROI识别异常恢复、延迟数帧才变化、点击生效但回报丢失、点击没生效、永久加载、禁止弹窗覆盖目标、底层设备互斥、原生数字OCR与斜杠语义不混淆。

单次副作用防重范围是同一次 Task.run 及其恢复重试，不声称已实现进程崩溃后的自动续跑。跨进程重新启动仍需业务完成判断。

| 两服真实截图状态图 | 回大厅 | 巡视 | 决策回放 |
|---|---:|---:|---|
| 日服 | 7/7符合预期 | 3/3符合预期 | 每帧一致，零设备调用 |
| 繁中 | 7/7符合预期 | 3/3符合预期 | 每帧一致，零设备调用 |

回大厅覆盖大厅、课程表、学生、商店、课程总览、公告和资源弹窗；巡视覆盖大厅/公告起始及资源弹窗拒绝。随机公告用固定种子，便于复现。两种资源弹窗是 B1 既有截图，不是真机主动打开的。

首轮18/20的两项失败来自故障注入先用公告遮住了“资源弹窗起始”画面，关闭公告后总输入不再是零；修正为首帧直接显示资源弹窗后均零输入。原失败记录[保留说明](../out/b2-deliverable/evidence/初轮两项故障注入修正.json)，没有为它改模板阈值或原图。

协议校验：392 条事件通过 Draft2020-12 Schema；核验 228 张逐帧图片和 228 条重算决策。另实际执行 CLI 离线探针，stdout 全部为可解析 JSONL；无用户在场确认参数的 live 启动在创建控制器前被拒绝，未连接设备。

## 单帧耗时与 trace

模拟设备使用 B1 日服原图状态图，代表帧为进入课程表的动作帧：

| 截图 wall ms | 等稳定（逻辑ms） | 识别 wall ms | 动作 wall ms | 另计PNG wall ms |
|---:|---:|---:|---:|---:|
| 2.425 | 84 | 15.024 | 0.005 | 22.251 |

ManualClock 的等待是逻辑时间，不让主机实际睡眠；其余四项为实测 wall time，不能把逻辑等待当成主机耗时相加。逐帧记录见[单帧证据](../out/b2-deliverable/evidence/模拟设备单帧耗时.json)。真机耗时已补充至[真机报告](LOOP_B2_LIVE.md)。

- 代码生成状态图测试 trace：`D:\OpenAR-main\out\cpu\Release\test-artifacts\36284\loop`。
- 两服 B1 原图状态图的全部20例：`D:\BA-Auto\out\offline-b2-recorded-replay`；各例路径见[验收清单](../out/b2-deliverable/evidence/offline-summary.jsonl)。
- [可直接回放的日服样例](../out/b2-deliverable/离线真实截图回放样例.zip)，含完整帧与events；[20例重算序列](../out/b2-deliverable/evidence/replay-decisions/)中逐帧对照均相等。

这些素材全部已见，只是 B2 集成回归，不能计作新的 OCR 独立验收集。

## 真机测试结果（已获用户在场授权并执行）

日服回大厅/巡视成功；繁中回大厅因挂机自动隐藏 UI 无锚点而零输入失败，巡视按首失败停止规则未运行。三段451帧决策回放一致。完整过程、修复、耗时、登录记录和差距见[真机报告](LOOP_B2_LIVE.md)。本报告其余部分保留离线验收基线。

## 代码范围与仓库

OpenAR 新增 Loop/Simulator 头与实现、22项专项测试、API/事件说明和Schema；现有 TraceWriter 只新增逐帧保存方法，测试主入口和两个VS项目添加编译接入。原 Session/capture/perform/Frame代次、JSON Resource/Tasker/next/on_error 均未改写。真机调试另增加 MuMu 通用 click 的80ms可取消手势修复；详见补充报告。B1d 47项资产已先归档；当前冻结清单唯一差异为扩展后的 tests/main.cpp，模型、字典、profile、OCR插件和算法源码均保持原哈希。

```text
D:\BA-Auto
  build.ps1 / BA-Auto.vcxproj / README.md
  src/main.cpp
  tasks/Navigation.h / Navigation.cpp
  recover/ReturnHall.cpp
  templates/ja/        templates/zh-Hant/
  templates/provenance.json
  tests/frames/ja/     tests/frames/zh-Hant/
  out/                本地构建和trace，不提交
```

24张模板、14张原图均来自用户 B1 语料，[每张模板来源](../out/b2-deliverable/evidence/模板来源.json)含完整源路径、SHA和裁框。只参考[用户旧PCR分阶段循环](https://github.com/sakura2107/OpenAR/blob/34a5f0dbdf72e72fc4f83c35d5f873e17ab2cb43/PCRDemo/src/ReceiveGifts.cpp)、[BAAS先回主界面再进入课程](https://github.com/pur1fying/blue_archive_auto_script/blob/223000d10f3dff73f980cba04ec73fe9755fc75a/module/lesson.py)和本地 BAAH 的 Page 页面连接划分，没有复制 BAAS/BAAH 的代码或图片。

[BA-Auto骨架与离线素材](../out/b2-deliverable/BA-Auto骨架与B1离线素材.zip) · [OpenAR新增模块源码](../out/b2-deliverable/OpenAR-B2新增模块源码.zip) · [已有文件接入补丁](../out/b2-deliverable/B2已有文件接入改动.patch) · [验收汇总](../out/b2-deliverable/evidence/B2离线验收汇总.json) · [SHA256清单](../out/b2-deliverable/SHA256SUMS.json)
