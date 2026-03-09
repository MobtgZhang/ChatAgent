# AgentClaw — AI Agent 桌面客户端

<p align="center">
  <strong>🤖 原生桌面 AI Agent · Qt 6 · 多模态工具调用 · ReAct 编排</strong>
</p>

---

AgentClaw（原名 ChatAgent）是一款基于 **Qt 6 + QML** 构建的本地 AI 智能体应用。不仅支持常规对话，更内置 **ReAct 循环**、**多工具调用**、**记忆模块**，可完成文件操作、网页搜索、终端命令、窗口控制等物理级任务。支持 OpenAI 兼容 API（含阿里云通义、DeepSeek 等推理模型）。

---

## ✨ 核心特点

### 🤖 三位一体模式切换

| 模式 | 说明 |
|------|------|
| **Chat** | 纯对话模式，无工具调用，轻量快速 |
| **Agent** | ReAct 智能体模式，自动选择并执行工具，迭代完成任务 |
| **Planning** | 规划模式，先拆解复杂任务为步骤，再按序执行 |

顶部一键切换，适配不同场景：简单问答用 Chat，复杂多步骤任务用 Agent 或 Planning。

### 🔧 九大内置工具

Agent 模式下，模型可调用以下工具完成实际操作：

| 工具 | 功能 |
|------|------|
| `file` | 文件读写、列目录、创建文件夹（工作目录：`agent_files`） |
| `shell` | 执行安全的 Shell 命令（白名单限制） |
| `web_search` | 网络搜索，返回摘要与链接 |
| `keyboard` | 模拟键盘输入、按键、组合键（Linux 需 xdotool） |
| `ocr` | 图片 OCR 文字识别（需 tesseract） |
| `window` | 列出/激活窗口、获取当前活动窗口（Linux 需 wmctrl） |
| `clipboard` | 读写系统剪贴板 |
| `wait` | 等待指定秒数（用于 UI 操作间等待） |
| `image_match` | 在场景图中查找模板图像位置（需 Python + opencv-python） |

所有工具均遵循 OpenAI Function Calling 格式，便于扩展。

### 🧠 记忆模块

- **短期记忆**：滚动窗口式对话历史，控制上下文长度
- **长期记忆**：SQLite 持久化存储用户偏好、事实，跨会话复用
- **自动注入**：记忆内容自动构建为 Prompt 上下文，供模型参考

设置界面可管理长期记忆，支持增删改查。

### 🧩 ReAct 编排

- **意图识别 → 工具选择 → 执行 → 结果反思** 的循环
- 支持多轮工具调用（最大轮数可配置，默认 40）
- 工具执行结果反馈给 LLM，驱动下一轮决策

### 🖥️ 原生桌面体验

- **Qt 6 + QML**：无 Electron，启动快、内存占用低
- **Discord 风格深色主题**：`#2B2D31` 背景，`#5865F2` 强调色
- **跨平台**：Linux / Windows / macOS
- **响应式布局**：最小 800×560，可自由拉伸

---

## 📋 其他功能

- **思考过程可视化**：支持 DeepSeek / Qwen 等推理模型的 `reasoning_content` 实时展示
- **流式输出**：SSE 流式，边生成边展示；支持中断生成
- **Markdown + LaTeX 数学公式**：WebEngineView + marked + KaTeX 渲染
- **会话与文件夹管理**：多会话、拖拽入库、右键菜单
- **消息级编辑**：编辑用户消息、删除截断、重新生成
- **API 配置**：Key、自定义 URL、模型列表刷新、Temperature/Top-P/Top-K/Max Tokens 调节
- **系统提示词**：全局 System Prompt，支持自定义 Agent 人设
- **会话持久化**：本地 JSON 自动保存，启动恢复

---

## 🏗️ 技术栈

| 层级 | 技术 |
|------|------|
| UI | Qt Quick (QML) |
| 后端 | C++17, Qt Network, Qt Sql |
| Agent | ReAct 循环、ToolRegistry、MemoryModule |
| 渲染 | Qt WebEngineQuick（Markdown/LaTeX） |
| 构建 | CMake 3.16+ |
| 平台 | Linux / Windows / macOS |

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

- **Qt 6**：Core, Gui, Quick, QuickControls2, Network, Sql, WebEngineQuick, WebChannel
- **可选（Agent 工具）**：
  - Linux：`wmctrl`、`xdotool`（窗口/键盘工具）
  - 通用：`tesseract`（OCR）、`Python3 + opencv-python`（图像匹配）

---

## 📂 数据存储

- 会话与历史索引：`QStandardPaths::AppDataLocation` → `sessions/`、`history_index.json`
- 长期记忆：`agent_memory.db`（SQLite）
- 文件工具工作目录：`agent_files/`

---

## 📄 License

见项目中的 `LICENSE` 或关于窗口中的授权信息。

---

<p align="center">
  <i>AgentClaw · 物理级全能执行者 · 简洁、原生、可扩展的 AI Agent 客户端</i>
</p>
