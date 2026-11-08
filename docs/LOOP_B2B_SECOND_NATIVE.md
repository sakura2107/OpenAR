# B2b 第二批补测与任务级预算修正 · 2026-11-02

**B2b实机仍未通过。** 用户手动关闭商店登入提示后，两服重新以真实挂机隐藏UI起步。第二批日服两项通过；繁中回大厅没有唤醒UI，触发原实现重复恢复，总共尝试4次，超过本轮≤2次要求；已停止，繁中巡视未启动。

## 第二批结果

| 用例 | 结果 | 帧 | 动作 | 恢复 | 唤醒 |
|---|---|---:|---:|---:|---:|
| ja/home | success | 35 | 1 | 0 | 1 |
| ja/patrol | success | 344 | 10 | 0 | 0 |
| zh-Hant/home | failed | 218 | 4 | 1 | 4 |
| zh-Hant/patrol | 首失败后未启动 | — | — | — | — |

日服巡视各Phase动作：课程表进入2/离开1、学生进入2/离开2、商店进入1/离开2，共10次、0恢复，符合每阶段≤2标准。繁中在源35、74帧于“回大厅”尝试唤醒，源139、178帧又在“恢复到大厅”尝试；源218帧达到单Phase上限后结束。四次都是(900,90)，SDK回报accepted，预期大厅锚点未出现。**这次画面已经没有商店提示，不能把未唤醒归因于那条通知。**

[第二批原始结果/动作](native-second-2026-11-02/native-audit.json)、[繁中末帧218](native-second-2026-11-02/繁中未唤醒-218.png)、[开始前日服隐藏实录](live-preflight-second/ja-hidden.png)、[开始前繁中隐藏实录](live-preflight-second/zh-Hant-hidden.png)。只读Android状态显示诊断时焦点在BA，当前没有按住的触点；这些信息不足以确定点击注入、点位响应或游戏状态哪个环节异常。

第二批597/597帧按录制版本精确回放一致，633条事件通过Schema。首批仍保留3通过/1商店通知阻断及625/625帧回放结果。两批都没有花/领资源、扫荡、开战、改设置或执行账户授权，失败均未被补测覆盖。

## 已离线修正的次数漏洞

原`.max_attempts(2)`只限制单Phase；Task默认恢复再进入另一个Phase时计数归零，造成2+2。现增加通用任务级共享预算：

```cpp
tap_at({900, 90}, "neutral recovery point")
    .after_unknown(3s).expect(hall(), 2s).settle(2s)
    .max_attempts(2).task_budget("hall-wake", 2);
```

同一Task.run内，所有Phase/恢复共享`hall-wake`预算；perform前计数，回报丢失也计入。用完选择`$budget.exhausted/hall-wake`，以blocked终止任务，不再次进入恢复。普通导航每Phase3次、默认恢复策略和副作用一次确认机制保留；副作用的结果未知优先于预算状态。下一次Task.run才重置预算。通用计数器放OpenAR，BA键名和唤醒规则仍在用户仓库。

trace新增`task_attempts`共享计数，回放会比较；启用预算后不能拿缺少该状态的旧trace冒充同版本回放。两批真实trace使用各自录制时的旧runner完成回放，旧runner/源码均留档。

## 本地验证

- CPU83/83、GPU85/85，循环专项39项。新增跨Phase共享预算测试，以及“唤醒不响应、恢复不能重新给两次”的测试，含精确回放；副作用与共享预算组合仍保持outcome_unknown且不重复。
- 32/32离线用例符合预期：20旧回归、8故障、4已知提示/资源伪装反例。其中两服各一个“不响应唤醒”用例均在总尝试2次后blocked、0恢复；它们不是导航成功。其余故障加载与正常唤醒通过。
- 2125/2125离线帧按当前版本精确回放一致，2427条事件通过Schema；繁中106/106旧隐藏UI图片继续通过只读检查。
- 当前业务修订后339帧推演为8次动作、0恢复，各Phase≤2；帧59–87仍全部busy。历史初版7次结果保留，当前交付源码的指标使用8次。

| 当前推演Phase | 动作数 |
|---|---:|
| 进入-lessons | 1 |
| 离开-lessons | 1 |
| 进入-students | 2 |
| 离开-students | 2 |
| 进入-shop | 1 |
| 离开-shop | 1 |

[本地验证汇总](task-budget-offline/summary.json)、[当前逐帧推演](task-budget-offline/339帧当前推演.jsonl)、[框架检查](task-budget-offline/framework-tests.json)。

**任务级预算修正仅做了离线验证，没有再次向游戏试点。** 它限制重复尝试，尚未解决或解释繁中实机没有响应唤醒的问题。空白点的几何核对不等于证明点击一定能唤醒。下一步应先做有界输入/点位响应对照诊断，再安排新完整批次；目前不能进入B4或宣称B2b通过。

## 资料与版本

- 首批`D:\BA-Auto\out\live-b2b\1793613427869`；第二批`D:\BA-Auto\out\live-b2b\1793616306127`。
- [第二批完整trace](B2b真机第二批trace-2026-11-02.zip)；[第一批及提示诊断](B2b实机测试记录-2026-11-02.md)。提示自动关闭仍未在实机成功，用户手动关闭事实已记录。
- [当前完整源码](B2b任务级预算修订源码.zip)、[API](LOOP.md)、[事件协议](LOOP_EVENTS.md)、[Schema](loop-event.schema.json)、[当前文件哈希](SHA256SUMS.json)。两批原生回放使用录制版本；第二批旧runner在`D:\BA-Auto\out\b2b-tested-store-v2-before-wake-cap`。
- C++ do-while逐帧反应方式保持；B1d模型/参数/字典/评分和原导航模板冻结，没有提交commit，也没有访问或参考禁止目录。
