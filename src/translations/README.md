# 多语言翻译文件

新增语言只需添加对应的 JSON 文件，例如：
- `zh.json` - 简体中文
- `en.json` - 英语
- `fr.json` - 法语（示例）
- `ja.json` - 日语（示例）

## 格式

```json
{
  "key": "翻译文本",
  "newChat": "新对话",
  "settings": "设置"
}
```

- **key**：与代码中 `locale.tr("key", locale.lang)` 使用的键一致
- **value**：该语言下的显示文本

## 添加新语言步骤

1. 复制 `en.json` 为 `xx.json`（xx 为语言代码，如 fr、ja）
2. 翻译所有 value
3. 在 CMakeLists.txt 的 `qt_add_resources` 中添加新文件
4. 在 Settings 的 language 选项中加入新语言（若需要 UI 选择）
