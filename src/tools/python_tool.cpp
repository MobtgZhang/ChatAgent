#include "python_tool.h"

#include <QProcess>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QStandardPaths>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

// ── 平台路径适配 ──────────────────────────────────────────────────────────────
#ifdef Q_OS_WIN
static constexpr const char *kPythonBin = "python.exe";
static constexpr const char *kPipBin    = "pip.exe";
static constexpr const char *kScripts   = "Scripts";
#else
static constexpr const char *kPythonBin = "python3";
static constexpr const char *kPipBin    = "pip3";
static constexpr const char *kScripts   = "bin";
#endif

PythonTool::PythonTool(QObject *parent) : BaseTool(parent) {}

// ── 描述与 Schema ─────────────────────────────────────────────────────────────
QString PythonTool::description() const {
    return QStringLiteral(
        "轻量级 Python 执行器。支持在隔离虚拟环境中运行代码/脚本，安装 pip 包。\n"
        "action 可选值：\n"
        "  run_code    — 执行 Python 代码字符串（code 参数）\n"
        "  run_script  — 执行 .py 脚本文件（script_path 参数）\n"
        "  pip_install — 安装包（packages 参数，字符串数组）\n"
        "  pip_list    — 列出已安装包\n"
        "  create_venv — 创建/重建虚拟环境\n"
        "依赖：系统已安装 python3（用于创建 venv）。"
    );
}

QVariantMap PythonTool::parametersSchema() const {
    return QVariantMap{
        { "type", "object" },
        { "properties", QVariantMap{
            { "action", QVariantMap{
                { "type", "string" },
                { "enum", QVariantList{"run_code","run_script","pip_install","pip_list","create_venv"} },
                { "description", "要执行的操作" }
            }},
            { "code", QVariantMap{
                { "type", "string" },
                { "description", "Python 代码字符串（action=run_code 时使用）" }
            }},
            { "script_path", QVariantMap{
                { "type", "string" },
                { "description", "Python 脚本文件绝对路径（action=run_script 时使用）" }
            }},
            { "packages", QVariantMap{
                { "type", "array" },
                { "items", QVariantMap{{"type","string"}} },
                { "description", "要安装的 PyPI 包列表（action=pip_install 时使用）" }
            }},
            { "timeout", QVariantMap{
                { "type", "integer" },
                { "description", "超时秒数（默认 60，最大 300）" }
            }}
        }},
        { "required", QVariantList{"action"} }
    };
}

// ── 路径辅助 ─────────────────────────────────────────────────────────────────
QString PythonTool::venvRoot() const {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(base).filePath(QStringLiteral("agent_venv"));
}

QString PythonTool::venvPython() const {
    return QDir(venvRoot()).filePath(
        QStringLiteral("%1/%2").arg(QLatin1String(kScripts), QLatin1String(kPythonBin))
    );
}

QString PythonTool::venvPip() const {
    return QDir(venvRoot()).filePath(
        QStringLiteral("%1/%2").arg(QLatin1String(kScripts), QLatin1String(kPipBin))
    );
}

QString PythonTool::systemPython() const {
#ifdef Q_OS_WIN
    // Windows: 尝试 py launcher，再回退到 python
    for (const QString &cand : { QStringLiteral("py"), QStringLiteral("python") }) {
        QProcess p;
        p.start(cand, { QStringLiteral("--version") });
        if (p.waitForFinished(3000) && p.exitCode() == 0)
            return cand;
    }
    return QStringLiteral("python");
#else
    return QStringLiteral("python3");
#endif
}

// ── 确保 venv 存在 ────────────────────────────────────────────────────────────
bool PythonTool::ensureVenv(QString &errOut) {
    if (QFileInfo::exists(venvPython()))
        return true;

    QString result = doCreateVenv();
    QJsonDocument doc = QJsonDocument::fromJson(result.toUtf8());
    if (doc.object().value("success").toBool()) {
        return true;
    }
    errOut = doc.object().value("error").toString(result);
    return false;
}

// ── 通用进程执行 ──────────────────────────────────────────────────────────────
QString PythonTool::runProcess(const QString &program,
                               const QStringList &args,
                               int timeoutMs,
                               const QString &workDir)
{
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    if (!workDir.isEmpty())
        proc.setWorkingDirectory(workDir);

    proc.start(program, args);
    if (!proc.waitForStarted(5000)) {
        QJsonObject o;
        o["error"] = QStringLiteral("无法启动进程: %1").arg(program);
        o["exitCode"] = -1;
        return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    }

    bool finished = proc.waitForFinished(timeoutMs);
    if (!finished) {
        proc.kill();
        QJsonObject o;
        o["error"] = QStringLiteral("执行超时（%1ms）").arg(timeoutMs);
        o["exitCode"] = -2;
        return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    }

    QString output = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    int exitCode = proc.exitCode();

    QJsonObject o;
    o["stdout"]   = output;
    o["exitCode"] = exitCode;
    if (exitCode != 0)
        o["error"] = QStringLiteral("进程以非零退出码 %1 结束").arg(exitCode);
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

// ── 具体动作实现 ──────────────────────────────────────────────────────────────
QString PythonTool::doCreateVenv() {
    QString root = venvRoot();
    QDir().mkpath(QFileInfo(root).absolutePath());

    QString result = runProcess(
        systemPython(),
        { QStringLiteral("-m"), QStringLiteral("venv"), root },
        60000
    );

    QJsonDocument doc = QJsonDocument::fromJson(result.toUtf8());
    QJsonObject obj = doc.object();
    int code = obj.value("exitCode").toInt(-1);

    QJsonObject ret;
    if (code == 0) {
        ret["success"] = true;
        ret["venv_path"] = root;
        ret["message"] = QStringLiteral("虚拟环境已创建：%1").arg(root);
    } else {
        ret["success"] = false;
        ret["error"] = obj.value("stdout").toString(QStringLiteral("创建虚拟环境失败"));
        ret["venv_path"] = root;
    }
    return QString::fromUtf8(QJsonDocument(ret).toJson(QJsonDocument::Compact));
}

QString PythonTool::doRunCode(const QString &code, int timeoutSec) {
    // 写入临时文件避免 shell 转义问题
    QTemporaryFile tmp;
    tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/agent_py_XXXXXX.py"));
    if (!tmp.open()) {
        return QStringLiteral("{\"error\":\"无法创建临时脚本文件\"}");
    }
    tmp.write(code.toUtf8());
    tmp.flush();
    QString scriptPath = tmp.fileName();
    tmp.setAutoRemove(false);
    tmp.close();

    QString result = runProcess(
        venvPython(),
        { scriptPath },
        timeoutSec * 1000
    );

    QFile::remove(scriptPath);
    return result;
}

QString PythonTool::doRunScript(const QString &path, int timeoutSec) {
    if (!QFileInfo::exists(path)) {
        return QStringLiteral("{\"error\":\"脚本文件不存在: %1\"}").arg(path);
    }
    return runProcess(
        venvPython(),
        { path },
        timeoutSec * 1000,
        QFileInfo(path).absolutePath()
    );
}

QString PythonTool::doPipInstall(const QStringList &packages, int timeoutSec) {
    if (packages.isEmpty()) {
        return QStringLiteral("{\"error\":\"packages 列表为空\"}");
    }

    QStringList args{ QStringLiteral("-m"), QStringLiteral("pip"),
                      QStringLiteral("install"), QStringLiteral("--quiet") };
    args.append(packages);

    QString result = runProcess(
        venvPython(),
        args,
        timeoutSec * 1000
    );

    QJsonDocument doc = QJsonDocument::fromJson(result.toUtf8());
    QJsonObject obj = doc.object();
    int code = obj.value("exitCode").toInt(-1);

    QJsonObject ret;
    ret["packages"] = QJsonArray::fromStringList(packages);
    if (code == 0) {
        ret["success"] = true;
        ret["message"] = QStringLiteral("安装成功: %1").arg(packages.join(", "));
        QString out = obj.value("stdout").toString().trimmed();
        if (!out.isEmpty()) ret["stdout"] = out;
    } else {
        ret["success"] = false;
        ret["error"] = obj.value("stdout").toString(QStringLiteral("pip install 失败"));
    }
    return QString::fromUtf8(QJsonDocument(ret).toJson(QJsonDocument::Compact));
}

QString PythonTool::doPipList() {
    return runProcess(
        venvPython(),
        { QStringLiteral("-m"), QStringLiteral("pip"),
          QStringLiteral("list"), QStringLiteral("--format=columns") },
        30000
    );
}

// ── 入口 ──────────────────────────────────────────────────────────────────────
QString PythonTool::execute(const QVariantMap &args) {
    QString action = args.value("action").toString().trimmed();
    int timeout = qBound(5, args.value("timeout", 60).toInt(), 300);

    QString result;

    if (action == QLatin1String("create_venv")) {
        result = doCreateVenv();

    } else if (action == QLatin1String("pip_list")) {
        QString err;
        if (!ensureVenv(err)) {
            result = QStringLiteral("{\"error\":\"虚拟环境不可用: %1\"}").arg(err);
        } else {
            result = doPipList();
        }

    } else if (action == QLatin1String("pip_install")) {
        QStringList packages;
        QVariant pkgVar = args.value("packages");
        if (pkgVar.typeId() == QMetaType::QStringList) {
            packages = pkgVar.toStringList();
        } else if (pkgVar.typeId() == QMetaType::QVariantList) {
            for (const QVariant &v : pkgVar.toList())
                packages << v.toString();
        }
        if (packages.isEmpty()) {
            result = QStringLiteral("{\"error\":\"请提供 packages 参数（字符串数组）\"}");
        } else {
            QString err;
            if (!ensureVenv(err)) {
                result = QStringLiteral("{\"error\":\"虚拟环境不可用: %1\"}").arg(err);
            } else {
                result = doPipInstall(packages, timeout);
            }
        }

    } else if (action == QLatin1String("run_code")) {
        QString code = args.value("code").toString();
        if (code.trimmed().isEmpty()) {
            result = QStringLiteral("{\"error\":\"code 参数为空\"}");
        } else {
            QString err;
            if (!ensureVenv(err)) {
                result = QStringLiteral("{\"error\":\"虚拟环境不可用: %1\"}").arg(err);
            } else {
                result = doRunCode(code, timeout);
            }
        }

    } else if (action == QLatin1String("run_script")) {
        QString path = args.value("script_path").toString().trimmed();
        if (path.isEmpty()) {
            result = QStringLiteral("{\"error\":\"script_path 参数为空\"}");
        } else {
            QString err;
            if (!ensureVenv(err)) {
                result = QStringLiteral("{\"error\":\"虚拟环境不可用: %1\"}").arg(err);
            } else {
                result = doRunScript(path, timeout);
            }
        }

    } else {
        result = QStringLiteral(
            "{\"error\":\"未知 action: %1。可选: run_code, run_script, pip_install, pip_list, create_venv\"}"
        ).arg(action);
    }

    emit executionFinished(name(), result);
    return result;
}
