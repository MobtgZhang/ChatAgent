#include "memory_tool.h"
#include "memory_module.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

MemoryTool::MemoryTool(MemoryModule *memory, QObject *parent)
    : BaseTool(parent), m_memory(memory) {}

QString MemoryTool::description() const {
    return QStringLiteral(
        "自进化记忆工具。任务成功后保存 SOP 以便下次直接调用；遇到错误时记录教训避免重复。"
        "action: save_sop(保存学到的流程)、recall_sops(列出/搜索 SOP)、read_sop(读取完整 SOP)、add_lesson(记录教训)。"
        "执行前先 recall_sops 检查是否有现成 SOP；任务成功后用 save_sop 保存可复用流程。");
}

QVariantMap MemoryTool::parametersSchema() const {
    return QVariantMap{
        { "type", "object" },
        { "properties", QVariantMap{
            { "action", QVariantMap{
                { "type", "string" },
                { "enum", QVariantList{"save_sop", "recall_sops", "read_sop", "add_lesson"} },
                { "description", "save_sop=保存SOP; recall_sops=列出SOP; read_sop=读取SOP内容; add_lesson=记录教训" }
            }},
            { "name", QVariantMap{
                { "type", "string" },
                { "description", "SOP 名称（save_sop/read_sop 必需），如 send_email_sop、wechat_read_sop" }
            }},
            { "content", QVariantMap{
                { "type", "string" },
                { "description", "save_sop 时的 SOP 内容：关键步骤、前置条件、易踩坑点，精简扼要" }
            }},
            { "summary", QVariantMap{
                { "type", "string" },
                { "description", "SOP 简短摘要（可选），用于 recall 时展示" }
            }},
            { "keyword", QVariantMap{
                { "type", "string" },
                { "description", "recall_sops 时的搜索关键词（可选）" }
            }},
            { "event_summary", QVariantMap{
                { "type", "string" },
                { "description", "add_lesson 时的事件简述，如「web_search 返回空」" }
            }},
            { "lesson", QVariantMap{
                { "type", "string" },
                { "description", "add_lesson 时提炼的教训，如「百度搜索经常失败，应优先用 duckduckgo」" }
            }},
            { "confidence", QVariantMap{
                { "type", "number" },
                { "description", "add_lesson 时教训可信度 0~1（默认 0.8）" }
            }}
        }},
        { "required", QVariantList{"action"} }
    };
}

QString MemoryTool::execute(const QVariantMap &args) {
    if (!m_memory) {
        return QStringLiteral("{\"error\":\"记忆模块未初始化\"}");
    }

    QString action = args.value("action").toString();

    if (action == "save_sop") {
        QString name = args.value("name").toString().trimmed();
        QString content = args.value("content").toString();
        QString summary = args.value("summary").toString();
        if (name.isEmpty()) {
            return QStringLiteral("{\"error\":\"save_sop 需要 name 参数\"}");
        }
        if (content.isEmpty()) {
            return QStringLiteral("{\"error\":\"save_sop 需要 content 参数\"}");
        }
        bool ok = m_memory->addSop(name, content, summary);
        QJsonObject o;
        o["success"] = ok;
        o["message"] = ok ? QStringLiteral("SOP 已保存，下次可直接 recall 后执行") : QStringLiteral("保存失败");
        return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    }

    if (action == "recall_sops") {
        QString keyword = args.value("keyword").toString();
        QVariantList list = m_memory->listSops(keyword);
        QJsonArray arr;
        for (const QVariant &v : list) {
            QVariantMap m = v.toMap();
            QJsonObject j;
            j["name"] = m["name"].toString();
            j["summary"] = m["summary"].toString();
            j["usage_count"] = m["usage_count"].toInt();
            arr.append(j);
        }
        QJsonObject o;
        o["sops"] = arr;
        o["count"] = arr.size();
        return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    }

    if (action == "read_sop") {
        QString name = args.value("name").toString().trimmed();
        if (name.isEmpty()) {
            return QStringLiteral("{\"error\":\"read_sop 需要 name 参数\"}");
        }
        QString content = m_memory->getSop(name);
        if (content.isEmpty()) {
            QJsonObject err;
            err["error"] = QStringLiteral("未找到 SOP: %1").arg(name);
            return QString::fromUtf8(QJsonDocument(err).toJson(QJsonDocument::Compact));
        }
        m_memory->incrementSopUsage(name);
        QJsonObject o;
        o["name"] = name;
        o["content"] = content;
        return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    }

    if (action == "add_lesson") {
        QString eventSummary = args.value("event_summary").toString().trimmed();
        QString lessonText = args.value("lesson").toString().trimmed();
        double confidence = args.value("confidence", 0.8).toDouble();
        if (eventSummary.isEmpty()) {
            return QStringLiteral("{\"error\":\"add_lesson 需要 event_summary 参数\"}");
        }
        if (lessonText.isEmpty()) {
            return QStringLiteral("{\"error\":\"add_lesson 需要 lesson 参数\"}");
        }
        bool ok = m_memory->addLesson(eventSummary, lessonText, confidence);
        QJsonObject o;
        o["success"] = ok;
        o["message"] = ok ? QStringLiteral("教训已记录") : QStringLiteral("记录失败");
        return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    }

    return QStringLiteral("{\"error\":\"未知 action: %1\"}").arg(action);
}
