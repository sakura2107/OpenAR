# OCR 字典编码兼容

Paddle OCR 的字典类别顺序必须与模型一致，插件向调用方输出 UTF-8。部分 Windows 本地模型包包含 GBK 字典，原加载器直接输出原字节，导致调用方得到无效 UTF-8，影响字符判断和数字识别。

`plugins/ocr/paddle_common/Dictionary.cpp` 现在以二进制加载，接受 UTF-8（可带 BOM）和 CRLF；整份文件不是有效 UTF-8 时，使用 Windows GB18030 转为 UTF-8，并记录兼容转换日志。GBK 是 GB18030 的兼容子集。不按行猜测编码，以免同一文件中的字节被混合解码。

字典项按原始行顺序保留，包括单空格。空行和 NUL 被拒绝，避免静默跳过造成类别索引偏移；全部解析成功后才替换旧字典，重载不会追加，失败保留上一次有效内容。

2026-10-06：CPU Release 构建与 `OpenARTests` 共 30 项通过，新增 UTF-8 BOM / CRLF、GBK、重复加载及无效输入回归测试。CPU SDK 已重新打包，并部署到 BAAX 实机验证竞技场排名 OCR。GPU 插件共用此字典实现，本轮未构建 GPU 配置。
