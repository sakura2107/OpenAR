# 第三方依赖

## B1b server 识别候选

另已核验并转换 `PP-OCRv5_server_rec`，固定 revision `b26c3587fda8da3c8ec0ce357214b4d661ff1558`。[官方固定版本模型卡](https://huggingface.co/PaddlePaddle/PP-OCRv5_server_rec/tree/b26c3587fda8da3c8ec0ce357214b4d661ff1558) 标记 Apache-2.0；存档模型卡位于 `docs/licenses/PaddleOCR/PP-OCRv5_server_rec.model-card.md`，许可证全文沿用同目录的 LICENSE。原始来源、字典、转换命令/工具版本及哈希在 `models/official/PP-OCRv5_server_rec.provenance.json`。该模型仅作本机实验，未选为默认模型、未制作分发包；本机抽样准确率和 CPU 时延见 [B1b 报告](OCR_B1B.md)。

OpenCV、ORT、RapidJSON、PaddleOCR 后处理、ADB、CUDA/cuDNN、MuMu SDK 属于各自项目/厂商。保留现有版权说明，分发者应从准确版本获取许可证/通知，确认模型/二进制再分发条件。

本轮未改变原项目授权，未复制 MaaFramework/BAAH 代码或游戏素材。MuMu SDK 从用户安装目录加载，不打包模拟器。GPU DLL 的分发遵守实际版本厂商条件。

离线示例图片由 examples/Demo.h 生成。真实设备截图不进入源码或默认分发包。

## B1 OCR 官方模型（2026-11-01）

以下五个模型已从 PaddlePaddle 官方仓库的固定 revision 下载并转换为 ONNX；模型卡均标记 Apache-2.0。当前只是本机测试候选，没有发布分发包。两服通用识别候选为 PP-OCRv5_mobile_rec，繁中 v3 保留作对照；数字通道使用日文 v3，低置信时使用英文 v4。

| 模型 | 固定 revision | 官方来源 |
|---|---|---|
| chinese_cht_PP-OCRv3_mobile_rec | `ab3009d849dea4d7296aabce47acff1ab124ebff` | [官方权重卡](https://huggingface.co/PaddlePaddle/chinese_cht_PP-OCRv3_mobile_rec/tree/ab3009d849dea4d7296aabce47acff1ab124ebff) |
| en_PP-OCRv4_mobile_rec | `f97b62fdc0eb71c689393a19a4b21baa1795c9ab` | [官方权重卡](https://huggingface.co/PaddlePaddle/en_PP-OCRv4_mobile_rec/tree/f97b62fdc0eb71c689393a19a4b21baa1795c9ab) |
| japan_PP-OCRv3_mobile_rec | `199d71fd22b19f6964075e6164a4cf8ca23d0178` | [官方权重卡](https://huggingface.co/PaddlePaddle/japan_PP-OCRv3_mobile_rec/tree/199d71fd22b19f6964075e6164a4cf8ca23d0178) |
| PP-OCRv5_mobile_det | `0d63e78e2b680928f6b1747d76a08db6e645efb7` | [官方权重卡](https://huggingface.co/PaddlePaddle/PP-OCRv5_mobile_det/tree/0d63e78e2b680928f6b1747d76a08db6e645efb7) |
| PP-OCRv5_mobile_rec | `682f20538d8c086cb2128e5cfac775e6c4904e85` | [官方权重卡](https://huggingface.co/PaddlePaddle/PP-OCRv5_mobile_rec/tree/682f20538d8c086cb2128e5cfac775e6c4904e85) |

许可证全文和五份固定版本模型卡保存在 `docs/licenses/PaddleOCR/`。每个模型的 `models/official/<name>.provenance.json` 保存来源、revision、源文件 SHA256、原始字典行序、导出工具版本、命令、实际 opset 及 ONNX SHA256；转换标记为 Paddle PIR → ONNX，保留权重。字典直接从同一权重的 inference.yml 的 PostProcess.character_dict 提取，未排序或去重。

Apache-2.0 的许可条件及分发义务以随附全文为准；闭源分发时保留适用许可证、版权与 NOTICE，并保留转换说明。离线转换依赖 PaddlePaddle 3.1.1、Paddle2ONNX 2.1.0、ONNX 1.17.0；OpenAR 运行时只依赖 native ONNX Runtime，无 Paddle/Python 依赖。

原有 det.sim.onnx、rec.sim.onnx、det_ja.onnx、rec_ja.onnx 缺少可追溯的下载/转换记录，继续只作旧行为对照；不因名称相似而将其许可证归于上表模型。简中字典与官方有 29 个类别差异，未静默覆盖。正式候选配置使用上述可追溯模型和其配套字典。

真实游戏截图只保存在本地测试语料，未上传或加入默认分发包。MuMu SDK 仍从用户安装目录加载。Python 辅助工具及转换环境未打包进 OpenAR。
# B1d CTC exclusion data

OpenCC dictionary data is used offline to generate recognition class masks, under Apache-2.0. Pinned revision: `fae2a0a911c30764e669a7d8b0591cd8e852934a`. Sources: [STCharacters](https://github.com/BYVoid/OpenCC/blob/fae2a0a911c30764e669a7d8b0591cd8e852934a/data/dictionary/STCharacters.txt) and [TWVariants](https://github.com/BYVoid/OpenCC/blob/fae2a0a911c30764e669a7d8b0591cd8e852934a/data/dictionary/TWVariants.txt). Original license and SHA256 provenance are retained in `models/ocr/opencc/`. OpenCC is not loaded at runtime, and recognized text is never converted after decoding.

