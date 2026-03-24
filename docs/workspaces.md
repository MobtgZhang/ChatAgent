# 工作区（Plan / Agent / Debug / Ask）

ChatAgent 主界面提供与 Cursor 相近的四种工作区，用于在同一对话线程中切换「推理方式」与「可用能力」，而非打开四个独立应用。

## Ask

- **用途**：问答、解释、改写；尽量不触发本机副作用。
- **实现**：走普通聊天补全链路，**不向模型注册工具**。
- **联网**：仍可使用输入区旁的「联网」开关（RAG）。
- **模型**：`Settings.modelNameAsk`；若为空则使用全局 `modelName`。

## Plan

- **用途**：拆解任务、输出可执行的 Markdown 计划（清单、风险、依赖）。
- **实现**：`AgentCore` 处于 `planning` 模式，系统提示中附加 Plan 专用段落。
- **工具**：`file`、`web_search`、`memory`、`wait`（弱执行、偏信息采集）。
- **模型**：`modelNamePlan`。

## Agent

- **用途**：完整 ReAct 闭环，自动调用全部已注册工具完成任务。
- **实现**：`AgentCore` 默认模式；任务成功且曾使用工具时可能自动提炼技能并写入 `SkillManager` / SOP。
- **模型**：`modelNameAgent`。

## Debug

- **用途**：根因分析、复现路径、根据日志与命令输出缩小问题范围。
- **实现**：`AgentCore` 在 `agent` 模式下运行，但 `agentUiMode=debug`，系统提示附加调试导向说明；**不触发**自动技能保存。
- **工具**：默认与 Plan 相同；在 **设置 → Agent → Debug：允许 Shell 与 Python 工具** 开启后，增加 `shell` 与 `python`。
- **模型**：`modelNameDebug`。

## 会话与持久化

- 每个会话文件（JSON）包含 `chatMode` 字段，切换会话时会恢复该会话上次使用的工作区。
- 顶部模型快捷按钮修改的是 **当前工作区** 对应的模型字段，并写入 `settings.json`。

## 相关源码

| 文件 | 职责 |
|------|------|
| [src/mainview.cpp](../src/mainview.cpp) | `normalizeChatMode`、`applyLlmForCurrentChatMode`、`configureAgentForChatMode`、发送路由 |
| [src/agent_core.cpp](../src/agent_core.cpp) | Plan/Debug 系统提示、`setAllowedToolNames`、自动保存技能门控 |
| [src/settings.h](../src/settings.h) | 工作区模型与温度覆盖、`debugAllowShell` |
| [src/tool_registry.cpp](../src/tool_registry.cpp) | `toolsSchemaAllowNames` |
| [src/qml/Main.qml](../src/qml/Main.qml) | 四段式切换与模型弹窗 |
| [src/qml/Settings.qml](../src/qml/Settings.qml) | 工作区模型下拉框与 Debug Shell 开关 |
