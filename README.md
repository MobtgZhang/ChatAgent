# ChatAgent — Community Edition

<p align="center">
  <strong>🖥️ 原生桌面 AI 对话客户端 · Qt 6 · 跨平台 · 轻量无 Electron</strong>
</p>

---

ChatAgent 是一款基于 **Qt 6 + QML** 构建的本地 AI 聊天应用，支持 OpenAI 兼容 API（含阿里云通义千问等）。采用 Discord 风格深色主题，专注简洁、高效、可扩展。

<p align="center">
  <img src="assets/screenshot.png" alt="ChatAgent 界面截图" width="800">
</p>

---

## ✨ Features

### 🧠 思考过程可视 —— 支持 DeepSeek / Qwen 等推理模型

- **实时展示 reasoning_content**：模型「想」的过程不再黑盒，逐字呈现思考链
- **一键开关**：顶部「思考：开/关」按钮，按需显示或隐藏，减少干扰
- **可折叠区块**：思考完成后可收起，支持 Markdown 渲染，排版清晰

### 💬 流式对话体验

- **SSE 流式输出**：边生成边展示，无需等待整段完成
- **中断生成**：随时点击「停止」结束当前回复
- **Enter 发送 · Shift+Enter 换行**：符合直觉的快捷键

### 📁 会话与文件夹管理

- **多会话**：新建、切换、重命名
- **文件夹组织**：创建文件夹，拖拽会话/文件夹入库，层级管理
- **右键菜单**：重命名、删除、新建子文件夹，操作集中
- **懒加载弹窗**：设置、关于等窗口按需创建，减轻内存

### ✏️ 消息级编辑与重试

- **编辑用户消息**：点击「编辑」修改后重发上下文
- **删除并截断**：删除用户消息时，顺带移除其后所有内容
- **重新生成**：从某条消息起重新请求，方便试错

### ⚙️ 完整 API 与参数调节

- **API 配置**：Key、自定义 URL（兼容任意 OpenAI 格式端点）
- **模型选择**：下拉框 +「从 API 刷新模型列表」，自动拉取可用模型
- **参数调节**：Temperature、Top-P、Top-K、Max Tokens，滑块 + 滚轮微调
- **系统提示词**：全局 System Prompt，对新对话生效

### 📝 Markdown 渲染

- **正文与思考内容**：均使用 `TextEdit.MarkdownText` 渲染
- **支持选中复制**：无 WebView，纯 QML，体验更轻

### 💾 会话持久化

- **自动保存**：每次回复完成或清空后写入本地 JSON
- **自动命名**：首条用户消息作为标题（当使用默认名时）
- **启动即恢复**：上次会话自动加载

### 🎨 界面与交互

- **Discord 风格**：深色背景 `#2B2D31`，强调色 `#5865F2`
- **响应式**：最小 800×560，可自由拉伸
- **字数提示**：输入框下方显示当前字数
- **对话框**：清空、重命名、新建文件夹等均用模态框确认

---

## 🏗️ 技术栈

| 层级 | 技术 |
|------|------|
| UI | Qt Quick (QML) |
| 后端 | C++17, Qt Network |
| 构建 | CMake 3.16+ |
| 平台 | Linux / Windows / macOS |

无 Electron、无 WebView 主聊天区，启动快，内存占用低。

---

## 🚀 构建与运行

```bash
mkdir build && cd build
cmake ..
make
./appAIChat
```

或使用项目根目录提供的 `run.sh` 一键构建运行。

---

## 📦 依赖

- Qt 6（Core, Gui, Quick, QuickControls2, Network）

---

## 📂 数据存储

- 会话与历史索引存储在 `QStandardPaths::AppDataLocation` 下
- Linux: `~/.local/share/<AppData>/sessions/` 及 `history_index.json`

---

## 📄 License

见项目中的 `LICENSE` 或关于窗口中的授权信息。

---

<p align="center">
  <i>ChatAgent Community Edition · 简洁、原生、可扩展的 AI 对话客户端</i>
</p>
