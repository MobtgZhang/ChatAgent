// 历史消息记录调试程序：验证 MessageModel 的 editHistory/branchTails 逻辑
#include "messagemodel.h"
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

static QVariantMap makeUserMsg(const QString &content) {
    QVariantMap m;
    m["role"] = "user";
    m["content"] = content;
    m["id"] = "u1";
    return m;
}

static QVariantMap makeAiMsg(const QString &content) {
    QVariantMap m;
    m["role"] = "ai";
    m["content"] = content;
    m["thinking"] = "";
    m["isThinking"] = false;
    m["id"] = "a1";
    return m;
}

static void runTests() {
    qDebug() << "=== 测试 1: 基本流程 (发送 -> AI 回复 -> 修改 -> 新 AI 回复) ===";
    MessageModel model;
    model.appendOne(makeUserMsg("hello"));
    model.appendOne(makeAiMsg("hi"));
    qDebug() << "初始: size =" << model.size();

    // 模拟 editAndRegenerate
    model.appendEditHistoryNode(0, "hello world", -1);
    qDebug() << "修改后 (截断): size =" << model.size() << "期望 1";

    model.appendOne(makeAiMsg("hi, world"));
    qDebug() << "新 AI 回复后: size =" << model.size() << "期望 2";

    model.persistCurrentBranchTails();

    // 切换到版本 1
    model.setUserMessageEditIndex(0, 0);
    qDebug() << "切换到版本 1 后: size =" << model.size();
    QVariantMap u = model.at(0);
    QVariantMap a = model.at(1);
    qDebug() << "  user content:" << u["content"].toString() << "期望 hello";
    qDebug() << "  ai content:" << a["content"].toString() << "(注: tail 替换暂禁，可能仍为新回复)";

    model.setUserMessageEditIndex(0, 1);
    qDebug() << "切换回版本 2 后 user:" << model.at(0)["content"].toString() << "期望 hello world";

    qDebug() << "\n=== 测试 2: 模拟流式 AI 更新后保存 ===";
    MessageModel model2;
    model2.appendOne(makeUserMsg("test"));
    model2.appendOne(makeAiMsg(""));  // 空 AI 占位
    // 多次 append 模拟流式
    for (int i = 0; i < 100; ++i)
        model2.updateLastAiMessageAppend("", "x", true);
    model2.updateLastAiMessageAppend("", "", false);  // 完成
    qDebug() << "流式更新后 content 长度:" << model2.at(1)["content"].toString().length();
    model2.persistCurrentBranchTails();

    qDebug() << "\n=== 测试 3: 编辑 + 流式 + 保存 ===";
    MessageModel model3;
    model3.appendOne(makeUserMsg("q1"));
    model3.appendOne(makeAiMsg("a1"));
    model3.appendEditHistoryNode(0, "q2", -1);
    model3.appendOne(makeAiMsg(""));
    for (int i = 0; i < 50; ++i)
        model3.updateLastAiMessageAppend("", "y", true);
    model3.updateLastAiMessageAppend("", "", false);
    qDebug() << "编辑+流式后 size:" << model3.size();
    model3.persistCurrentBranchTails();
    qDebug() << "branchTails 长度:" << model3.at(0)["branchTails"].toList().size();

    qDebug() << "\n=== 测试 4: 多次编辑分支导航 ===";
    MessageModel model4;
    model4.appendOne(makeUserMsg("v1"));
    model4.appendOne(makeAiMsg("r1"));
    model4.appendEditHistoryNode(0, "v2", -1);
    model4.appendOne(makeAiMsg("r2"));
    model4.appendEditHistoryNode(0, "v3", -1);
    model4.appendOne(makeAiMsg("r3"));
    qDebug() << "三个版本后 size:" << model4.size();
    // 切回第一个版本
    model4.setUserMessageEditIndex(0, 0);
    qDebug() << "切到版本1 content:" << model4.at(0)["content"].toString() << "期望 v1";
    // 切到第二个版本
    model4.setUserMessageEditIndex(0, 1);
    qDebug() << "切到版本2 content:" << model4.at(0)["content"].toString() << "期望 v2";

    qDebug() << "\n=== 所有测试完成 ===";
    QCoreApplication::quit();
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QTimer::singleShot(0, runTests);
    return app.exec();
}
