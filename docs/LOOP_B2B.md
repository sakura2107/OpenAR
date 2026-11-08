# B2b：加载等待、导航确认与挂机唤醒

**离线与推演已达标，新的真机批次尚未运行。** CPU81/81、GPU83/83，其中循环专项37项；旧回归20/20、新故障6/6、繁中106/106隐藏UI帧通过。26段精确回放1744/1744帧一致，2000条事件通过Schema。B1d继续冻结，原24张模板未改，没有游戏输入或提交commit。

## §2 最终API与行为

```cpp
ctx.busy(any(see("common/loading", roi(900, 600, 350, 100)), black_condition));
Phase("进入页面")
    .goal(see("page/title", title_roi))
    .on("入口", see("home/entry", entry_roi),
        tap().expect(see("page/title", title_roi)).settle(2s))
    .unknown_after(8s);
```

- 阶段导航动作后默认settle2秒；只允许安全全局规则，阶段规则等待。goal/gone/expect/timeout终止等待。**busy时不以被遮挡条件的gone终止等待**，防止加载覆盖菜单后解除保护；goal/expect可以确认，截止时间继续计时。busy抑制全局和阶段的所有输入，目标仍可完成；下一Phase重新截图，busy未消失仍禁止动作。
- 同一规则每个Phase最多执行3次，按累计次数，不再看图像差异；第4次候选命中判卡住并恢复。`stuck_after(n)`保留名字，含义改为执行上限，n为1–3；规则可用`max_attempts(2)`收紧。副作用仍要求expect，超时结果未知且跨恢复不重点。
- 未知预算使用`unknown_after(8s)`，替换帧数参数。`$busy`、settle不累计未知时间，Phase/Task总时限继续；已知UI识别会重置未知段。纯背景安全谓词不算UI锚点。
- trace新增`loop_revision=2`、`busy`、`settle_until`、`settle_reason`、`settle_rule`、`rule_attempts`、`retry_count`、`unknown_ms/unknown_limit_ms`、`phase_deadline_ms`。新回放检查这些状态、动作类型/按键和坐标，错误类型字段会返回错误。旧B2精确回放由已归档的旧runner保留；新逻辑看旧图走独立的`Task.project`，不冒称决策一致。
- 唤醒使用通用的显式例外`tap_at(point, reason).after_unknown(3s).expect(hall(),2s).max_attempts(2)`；启动时拒绝缺延迟、缺确认、无次数上限、越界或注册为全局的固定点动作。BA规则仅放在回大厅/恢复Phase。

**do-while方式保留**：Phase内部每轮截图→先判目标→最多一个动作→重截。JSON pipeline/Tasker不改写，继续共用DeviceSession、Frame代次、IImageMatcher、OCR和TraceWriter。

## §3 故障测试和业务策略

| 服务器/故障 | 总动作 | 恢复 | 精确回放帧 | 结果 |
|---|---:|---:|---:|---|
| ja / overlay3s | 6 | 0 | 246/246 | 通过 |
| ja / black6s | 6 | 0 | 468/468 | 通过 |
| ja / idle | 1 | 0 | 44/44 | 通过 |
| zh-Hant / overlay3s | 6 | 0 | 246/246 | 通过 |
| zh-Hant / black6s | 6 | 0 | 468/468 | 通过 |
| zh-Hant / idle | 1 | 0 | 44/44 | 通过 |

overlay3s/black6s各巡视3页：每次进入和离开均只有1次动作，0次恢复；idle每服只唤醒1次。模拟器新增基于毫秒的旧页+加载覆盖、黑屏过渡和隐藏UI注入，没有引入真实设备控制器。框架另覆盖busy跨Phase、busy禁止全局动作、默认2秒重试、图像动画也受3次上限、未知预算不随帧率变、gone/expect原因和恶意回放字段等测试。

繁中旧失败的106张原图逐一验证：背景安全匹配、无大厅UI、无按钮/弹窗命中、非busy；106/106通过，零输入。这些是已查看的调试图，不能进入以后盲验。每张结果在[106帧检查](../out/b2b-deliverable/evidence/繁中106帧逐帧检查.jsonl)。

新增12个模板均来自本人的旧截图/登录记录，带源帧SHA和裁框。**帧59–87基本全黑，不能从59裁出不存在的图标**；因此近纯黑转场单独判busy，仅抑制输入和暂停未知预算，仍受总超时。日服loading取10/52帧，繁中取登录加载图；当前没有繁中“黑底+图标”实录，后续自然出现再补，不伪造素材。

BA导航入口使用settle8秒上限，覆盖实录中约5–6秒的慢加载和加载字样消隐；目标/expect一出现即结束，不能理解成固定Sleep8秒。框架默认仍2秒。首轮推演虽共9次，但学生入口3次，保留为未达标；延长上限本身没有解决busy遮挡被误判gone的问题，修正该状态处理后才得到下面7次结果。

首轮故障检查把三个不同页面的“主页”边都叫`home-icon`，计数器将总数3误判为单边重复；已按来源页面命名并重跑，生产决策/模板未为它改动。原首轮结果保留在evidence，不改失败记录。

## §4 旧339帧的推演

| Phase | 新动作数 | 源帧号 | 动作 |
|---|---:|---|---|
| 进入-lessons | 1 | 6 | 点击[220, 688] |
| 离开-lessons | 1 | 56 | 返回键4 |
| 进入-students | 2 | 113、217 | 点击[327, 688]；点击[327, 688] |
| 离开-students | 1 | 234 | 点击[1238, 22] |
| 进入-shop | 1 | 280 | 点击[771, 688] |
| 离开-shop | 1 | 309 | 点击[1238, 22] |

**合计7次（旧38次），0次恢复，各阶段1–2次；59–87共29帧全部`$busy`。** 输出[完整逐帧推演](../out/b2b-deliverable/evidence/339帧新逻辑推演.jsonl)。推演使用同一决策状态机和旧捕获时间，包含任务开始的完成判断，旧Phase/恢复决策不会控制新Phase。

这是固定旧图序列的假设推演：若真实点击发生变化，后续真实画面也会变化。旧录像保留了原恢复后再次进入课程表的画面，所以学生阶段在源113、217各有一次候选点击；不能把7次直接当成真机结果。模拟状态图补充验证了实际点击改变状态时各只点1次。

## 唤醒点证据与适用范围

选择`(900,90)`，位于当前两服的泳池/窗帘背景，低于资源栏、远离顶部主页/展开按钮，不落在角色上。区域`[820,75,240,70]`由原生NCC核对背景，再检查无已知UI、无确认/购买/资源动作模板及无中央弹窗；未知已满3秒且非busy才允许尝试。expect大厅2秒，每次Phase最多2次。

- [日服正常大厅·源6帧](../out/b2b-deliverable/ja-normal-f6.png)、[日服正常大厅·源339帧](../out/b2b-deliverable/ja-normal-f339.png)：红圈只标注证据图，不改变原始帧。
- [繁中正常大厅·B1原图](../out/b2b-deliverable/zh-normal-B1.png)、[繁中挂机隐藏·源106帧](../out/b2b-deliverable/zh-hidden-f106.png)：同一点均为窗帘空白背景。全部106帧的背景NCC原尺度最低约0.982。
- **缺少日服隐藏UI的真实旧帧。** 日服故障测试使用“移除UI锚点、保留原背景”的合成图，已明确标注，不能声称是真实隐藏UI证据。新真机批次前先只读核对日服实际隐藏画面；点位不为空白就不输入。该点只对目前两服记录的主题/1280×720成立，换主题或布局要重核对。

流程参考：[BAAS to_main_page](https://github.com/pur1fying/blue_archive_auto_script/blob/223000d10f3dff73f980cba04ec73fe9755fc75a/core/Baas_thread.py)及co_detect的连续识别失败后试探点击；本地BAAH的`modules/AllTask/Task.py`/`DATA/assets/MatchAssets.py`的MagicPoint和成本弹窗判断。没有找到专门以“挂机隐藏UI”命名的独立分支。这里只参考恢复流程，独立C++实现有限、带确认的唤醒，没有复制其代码或图片。

## §5 新真机批次（等待新在场同意）

日服回大厅→日服巡视→繁中回大厅→繁中巡视。繁中事先故意保持挂机隐藏UI；先只读确认两服画面与中性点。脚本对每例检查成功、巡视每个进入/离开≤2次、巡视0恢复、唤醒≤2次、无副作用动作；任何失败即停。随后离线精确回放。只导航和返回，不领取、不购买、不扫荡、不开战、不改设置。

本轮未连接或输入游戏；用户B2b第5节明确要求再次征得在场同意。现有B2首轮结果继续保留，B2b没有拿离线结果替代真机验收。

## 交付

[API](LOOP.md) · [事件协议](LOOP_EVENTS.md) · [Schema](loop-event.schema.json) · [离线验收与逐Phase统计](../out/b2b-deliverable/evidence/B2b离线验收汇总.json) · [源码与精确差异](../out/b2b-deliverable/B2b源码与差异.zip) · [故障回放样例](../out/b2b-deliverable/B2b故障回放样例.zip) · [哈希清单](../out/b2b-deliverable/SHA256SUMS.json)。

全部26例原始trace在`D:\BA-Auto\out\offline-b2b-counted`、`D:\BA-Auto\out\offline-b2b-regression`，逐帧重算序列在`D:\BA-Auto\out\b2b-verified-replays`。OpenAR改动仅Loop/Simulator、专项测试、Schema和文档；BA模板、busy与唤醒规则在业务仓库。MuMu80ms与stdout隔离保留，不重写。没有访问/参考禁止目录，没有提交commit，B1d模型/参数/字典/评分及原24张导航模板保持不变。
