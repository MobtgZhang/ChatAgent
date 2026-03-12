/**
 * 工具功能测试：逐个验证各 BaseTool 子类的基本可用性
 * 运行方式: ./test_tools
 *
 * 覆盖的工具：
 *   - FileTool   : write / read / list / mkdir / 路径越权拒绝
 *   - ShellTool  : 白名单内命令 / 白名单外命令拒绝
 *   - WaitTool   : 等待 1 秒并计时
 *   - PythonTool : run_code / pip_list（需系统已安装 python3）
 */

#include "tools/file_tool.h"
#include "tools/shell_tool.h"
#include "tools/wait_tool.h"
#include "tools/python_tool.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDateTime>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>

static int g_pass = 0, g_fail = 0;

static void check(bool cond, const QString &desc) {
    if (cond) {
        qDebug().noquote() << "  [PASS]" << desc;
        ++g_pass;
    } else {
        qDebug().noquote() << "  [FAIL]" << desc;
        ++g_fail;
    }
}

// ────────────────────────────────────────────────────────────────────────────
// FileTool
// ────────────────────────────────────────────────────────────────────────────
static void testFileTool() {
    qDebug().noquote() << "\n=== FileTool ===";
    FileTool tool;

    // 1. write
    QVariantMap wArgs;
    wArgs["action"]  = "write";
    wArgs["path"]    = "test_output.txt";
    wArgs["content"] = "hello from test_tools";
    QString wRes = tool.execute(wArgs);
    qDebug().noquote() << "  write:" << wRes;
    check(!wRes.contains("\"error\""), "写文件不报错");

    // 2. read
    QVariantMap rArgs;
    rArgs["action"] = "read";
    rArgs["path"]   = "test_output.txt";
    QString rRes = tool.execute(rArgs);
    qDebug().noquote() << "  read:" << rRes;
    check(rRes.contains("hello from test_tools"), "读文件内容匹配");

    // 3. list
    QVariantMap lArgs;
    lArgs["action"] = "list";
    lArgs["path"]   = ".";
    QString lRes = tool.execute(lArgs);
    check(!lRes.contains("\"error\"") && !lRes.isEmpty(), "列目录不报错");

    // 4. mkdir
    QVariantMap mkArgs;
    mkArgs["action"] = "mkdir";
    mkArgs["path"]   = "test_subdir";
    QString mkRes = tool.execute(mkArgs);
    check(!mkRes.contains("\"error\""), "创建子目录不报错");

    // 5. 路径越权应被拒绝
    QVariantMap badArgs;
    badArgs["action"] = "read";
    badArgs["path"]   = "../../../../etc/passwd";
    QString badRes = tool.execute(badArgs);
    check(badRes.contains("error") || badRes.contains("不合法"),
          "路径越权被正确拒绝");
}

// ────────────────────────────────────────────────────────────────────────────
// ShellTool
// ────────────────────────────────────────────────────────────────────────────
static void testShellTool() {
    qDebug().noquote() << "\n=== ShellTool ===";
    ShellTool tool;

    // 1. echo（白名单内命令）
    QVariantMap echoArgs;
    echoArgs["command"] = "echo shell_test_ok";
    QString echoRes = tool.execute(echoArgs);
    qDebug().noquote() << "  echo:" << echoRes;
    check(echoRes.contains("shell_test_ok"), "echo 命令输出正确");

    // 2. pwd（白名单内命令）
    QVariantMap pwdArgs;
    pwdArgs["command"] = "pwd";
    QString pwdRes = tool.execute(pwdArgs);
    check(!pwdRes.contains("\"error\"") && !pwdRes.isEmpty(), "pwd 返回路径");

    // 3. 白名单外命令被拒绝
    QVariantMap rmArgs;
    rmArgs["command"] = "rm -rf /tmp/nonexistent_ca_test";
    QString rmRes = tool.execute(rmArgs);
    qDebug().noquote() << "  rm (blocked):" << rmRes;
    check(rmRes.contains("不允许") || rmRes.contains("error") ||
          rmRes.contains("denied") || rmRes.contains("blocked"),
          "rm 命令被白名单拒绝");
}

// ────────────────────────────────────────────────────────────────────────────
// WaitTool
// ────────────────────────────────────────────────────────────────────────────
static void testWaitTool() {
    qDebug().noquote() << "\n=== WaitTool ===";
    WaitTool tool;

    QVariantMap args;
    args["seconds"] = 1;

    qint64 t0 = QDateTime::currentMSecsSinceEpoch();
    QString res = tool.execute(args);
    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - t0;

    qDebug().noquote() << "  result:" << res << "  elapsed:" << elapsed << "ms";
    check(!res.contains("\"error\""), "wait 执行无错误");
    check(elapsed >= 900, QString("等待时间 >= 0.9s (实际 %1ms)").arg(elapsed));
}

// ────────────────────────────────────────────────────────────────────────────
// PythonTool
// ────────────────────────────────────────────────────────────────────────────
static void testPythonTool() {
    qDebug().noquote() << "\n=== PythonTool ===";
    PythonTool tool;

    // 1. run_code：运行简单代码
    QVariantMap codeArgs;
    codeArgs["action"] = "run_code";
    codeArgs["code"]   = "print('python_tool_ok'); import sys; print(sys.version.split()[0])";
    QString codeRes = tool.execute(codeArgs);
    qDebug().noquote() << "  run_code:" << codeRes.left(300);

    bool hasOutput   = codeRes.contains("python_tool_ok");
    bool missingPy   = codeRes.contains("python3 not found") ||
                       codeRes.contains("not found") ||
                       codeRes.contains("No such file");
    check(hasOutput || missingPy, "run_code 有输出或报告缺少 python3");

    // 2. pip_list
    QVariantMap listArgs;
    listArgs["action"] = "pip_list";
    QString listRes = tool.execute(listArgs);
    qDebug().noquote() << "  pip_list (first 200):" << listRes.left(200);
    check(!listRes.isEmpty(), "pip_list 返回非空结果");

    // 3. run_code：数学计算验证
    QVariantMap mathArgs;
    mathArgs["action"] = "run_code";
    mathArgs["code"]   = "print(sum(range(1,101)))";
    QString mathRes = tool.execute(mathArgs);
    bool hasMathResult = mathRes.contains("5050");
    bool mathMissingPy = mathRes.contains("python3 not found") ||
                         mathRes.contains("not found");
    check(hasMathResult || mathMissingPy, "数学计算结果正确(5050)或报告缺少 python3");
}

// ────────────────────────────────────────────────────────────────────────────
static void runAllTests() {
    testFileTool();
    testShellTool();
    testWaitTool();
    testPythonTool();

    qDebug().noquote() << "\n============================================";
    qDebug().noquote() << QString("测试完成: %1 通过, %2 失败").arg(g_pass).arg(g_fail);
    if (g_fail > 0)
        qDebug().noquote() << "警告：有失败项，请检查对应工具实现或运行环境";

    QCoreApplication::exit(g_fail > 0 ? 1 : 0);
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("ChatAgent");    // 与主程序一致，确保 AppDataLocation 路径相同
    QTimer::singleShot(0, runAllTests);
    return app.exec();
}
