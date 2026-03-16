#include "shell_tool.h"
#include <QProcess>
#include <QJsonObject>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QDir>
#include <QSysInfo>
#include <QRegularExpression>

ShellTool::ShellTool(QObject *parent) : BaseTool(parent) {
    // ===== 跨平台共有命令（相同子集） =====
    m_commonPrefixes = {
        // 文件/目录操作
        QStringLiteral("ls"),
        QStringLiteral("cd"),
        QStringLiteral("pwd"),
        QStringLiteral("mkdir"),
        QStringLiteral("rmdir"),
        QStringLiteral("cat"),
        QStringLiteral("more"),
        QStringLiteral("head"),
        QStringLiteral("tail"),

        // 文件搜索
        QStringLiteral("find"),
        QStringLiteral("which"),
        QStringLiteral("whereis"),

        // 文本处理
        QStringLiteral("grep"),
        QStringLiteral("sort"),
        QStringLiteral("uniq"),
        QStringLiteral("wc"),
        QStringLiteral("diff"),
        QStringLiteral("cmp"),

        // 系统信息
        QStringLiteral("whoami"),
        QStringLiteral("hostname"),
        QStringLiteral("date"),
        QStringLiteral("time"),
        QStringLiteral("echo"),
        QStringLiteral("env"),
        QStringLiteral("printenv"),

        // 网络基础
        QStringLiteral("ping"),
        QStringLiteral("netstat"),
        QStringLiteral("nslookup"),

        // 进程查看
        QStringLiteral("ps"),

        // 其他工具
        QStringLiteral("exit"),
        QStringLiteral("clear"),
        QStringLiteral("man"),
        QStringLiteral("help"),
        QStringLiteral("alias"),
        QStringLiteral("history"),

        // 压缩（通用格式）
        QStringLiteral("tar"),
        QStringLiteral("zip"),
        QStringLiteral("unzip"),
        QStringLiteral("gzip"),
        QStringLiteral("gunzip"),

        // 用户相关
        QStringLiteral("id"),
        QStringLiteral("groups"),

        // 文件属性
        QStringLiteral("stat"),
        QStringLiteral("file")
    };

    // ===== Windows 独有命令 =====
    m_windowsPrefixes = {
        // 文件/目录操作
        QStringLiteral("dir"),
        QStringLiteral("md"),
        QStringLiteral("rd"),
        QStringLiteral("type"),
        QStringLiteral("copy"),
        QStringLiteral("move"),
        QStringLiteral("del"),
        QStringLiteral("erase"),
        QStringLiteral("ren"),
        QStringLiteral("rename"),
        QStringLiteral("replace"),
        QStringLiteral("attrib"),
        QStringLiteral("xcopy"),
        QStringLiteral("robocopy"),

        // 文件内容
        QStringLiteral("fc"),
        QStringLiteral("comp"),
        QStringLiteral("findstr"),

        // 系统信息
        QStringLiteral("systeminfo"),
        QStringLiteral("ver"),
        QStringLiteral("path"),
        QStringLiteral("set"),
        QStringLiteral("chdir"),
        QStringLiteral("prompt"),
        QStringLiteral("title"),
        QStringLiteral("color"),
        QStringLiteral("mode"),
        QStringLiteral("driverquery"),

        // 网络
        QStringLiteral("ipconfig"),
        QStringLiteral("ipconfig /all"),
        QStringLiteral("tracert"),
        QStringLiteral("arp"),
        QStringLiteral("route"),
        QStringLiteral("nbtstat"),
        QStringLiteral("netsh"),
        QStringLiteral("hostname"),

        // 进程/任务
        QStringLiteral("tasklist"),
        QStringLiteral("taskkill"),
        QStringLiteral("sc"),
        QStringLiteral("net"),
        QStringLiteral("net1"),
        QStringLiteral("wmic"),
        QStringLiteral("qprocess"),
        QStringLiteral("query"),
        QStringLiteral("runas"),
        QStringLiteral("schtasks"),

        // 磁盘操作
        QStringLiteral("diskpart"),
        QStringLiteral("chkdsk"),
        QStringLiteral("format"),
        QStringLiteral("label"),
        QStringLiteral("vol"),
        QStringLiteral("fsutil"),
        QStringLiteral("convert"),
        QStringLiteral("defrag"),

        // 注册表
        QStringLiteral("reg"),
        QStringLiteral("regedit"),

        // 其他 Windows 工具
        QStringLiteral("cls"),
        QStringLiteral("start"),
        QStringLiteral("call"),
        QStringLiteral("goto"),
        QStringLiteral("if"),
        QStringLiteral("for"),
        QStringLiteral("setlocal"),
        QStringLiteral("endlocal"),
        QStringLiteral("pushd"),
        QStringLiteral("popd"),
        QStringLiteral("typeperf"),
        QStringLiteral("powershell"),
        QStringLiteral("pnputil"),
        QStringLiteral("bcdedit"),
        QStringLiteral("diskcopy"),
        QStringLiteral("format"),
        QStringLiteral("ftype"),
        QStringLiteral("getmac"),
        QStringLiteral("gpresult"),
        QStringLiteral("msiexec"),
        QStringLiteral("openfiles"),
        QStringLiteral("setx"),
        QStringLiteral("shutdown"),
        QStringLiteral("systeminfo"),
        QStringLiteral("takeown"),
        QStringLiteral("ver"),
        QStringLiteral("wevtutil"),
        QStringLiteral("winrm"),
        QStringLiteral("wscript")
    };

    // PowerShell 命令白名单（仅 Windows）
    m_powershellAllowedPrefixes = {
        // 文件操作
        QStringLiteral("Get-ChildItem"),
        QStringLiteral("Set-Location"),
        QStringLiteral("Get-Location"),
        QStringLiteral("New-Item"),
        QStringLiteral("Remove-Item"),
        QStringLiteral("Copy-Item"),
        QStringLiteral("Move-Item"),
        QStringLiteral("Rename-Item"),
        QStringLiteral("Get-Content"),
        QStringLiteral("Set-Content"),
        QStringLiteral("Add-Content"),
        QStringLiteral("Clear-Content"),
        QStringLiteral("Get-Item"),
        QStringLiteral("Set-Item"),
        QStringLiteral("Test-Path"),
        QStringLiteral("Split-Path"),
        QStringLiteral("Join-Path"),
        QStringLiteral("Resolve-Path"),

        // 进程/服务
        QStringLiteral("Get-Process"),
        QStringLiteral("Stop-Process"),
        QStringLiteral("Start-Process"),
        QStringLiteral("Wait-Process"),
        QStringLiteral("Get-Service"),
        QStringLiteral("Start-Service"),
        QStringLiteral("Stop-Service"),
        QStringLiteral("Restart-Service"),
        QStringLiteral("Get-ScheduledTask"),
        QStringLiteral("Start-ScheduledTask"),

        // 系统信息
        QStringLiteral("Get-ComputerInfo"),
        QStringLiteral("Get-Date"),
        QStringLiteral("Get-Host"),
        QStringLiteral("Get-Variable"),
        QStringLiteral("Get-EventLog"),
        QStringLiteral("Get-WinEvent"),
        QStringLiteral("Get-WmiObject"),
        QStringLiteral("Get-CimInstance"),
        QStringLiteral("Get-Counter"),
        QStringLiteral("Get-HotFix"),

        // 网络
        QStringLiteral("Get-NetIPAddress"),
        QStringLiteral("Get-NetAdapter"),
        QStringLiteral("Get-NetTCPConnection"),
        QStringLiteral("Get-NetUDPEndpoint"),
        QStringLiteral("Test-Connection"),
        QStringLiteral("Test-NetConnection"),
        QStringLiteral("Resolve-DnsName"),
        QStringLiteral("Get-DnsClientServerAddress"),
        QStringLiteral("Get-NetRoute"),
        QStringLiteral("Get-NetNeighbor"),

        // 用户/组
        QStringLiteral("Get-LocalUser"),
        QStringLiteral("Get-LocalGroup"),
        QStringLiteral("Get-LocalGroupMember"),
        QStringLiteral("Get-ADUser"),
        QStringLiteral("Get-ADGroup"),
        QStringLiteral("Get-ADComputer"),
        QStringLiteral("Get-ADObject"),
        QStringLiteral("Get-ADPrincipalGroupMembership"),

        // 输出命令
        QStringLiteral("Write-Host"),
        QStringLiteral("Write-Output"),
        QStringLiteral("Write-Warning"),
        QStringLiteral("Write-Error"),
        QStringLiteral("Write-Verbose"),
        QStringLiteral("Write-Debug"),
        QStringLiteral("Write-Information"),

        // 管道操作
        QStringLiteral("Select-Object"),
        QStringLiteral("Where-Object"),
        QStringLiteral("ForEach-Object"),
        QStringLiteral("Sort-Object"),
        QStringLiteral("Group-Object"),
        QStringLiteral("Measure-Object"),
        QStringLiteral("Compare-Object"),
        QStringLiteral("Format-Table"),
        QStringLiteral("Format-List"),
        QStringLiteral("Format-Wide"),
        QStringLiteral("Out-String"),
        QStringLiteral("Out-File"),
        QStringLiteral("Out-Null"),
        QStringLiteral("Out-GridView"),

        // 其他
        QStringLiteral("Get-Help"),
        QStringLiteral("Get-Command"),
        QStringLiteral("Get-Module"),
        QStringLiteral("Import-Module"),
        QStringLiteral("Get-PSDrive"),
        QStringLiteral("Get-PSProvider"),
        QStringLiteral("Test-ComputerSecureChannel"),
        QStringLiteral("Get-ItemProperty"),
        QStringLiteral("Set-ItemProperty"),
        QStringLiteral("Get-Acl"),
        QStringLiteral("Set-Acl"),
        QStringLiteral("Get-Process"),
        QStringLiteral("Stop-Computer"),
        QStringLiteral("Restart-Computer"),
        QStringLiteral("Get-Service"),
        QStringLiteral("Get-WinEvent"),
        QStringLiteral("Get-EventLog"),
        QStringLiteral("Get-WmiObject"),
        QStringLiteral("Get-CimInstance"),
        QStringLiteral("Invoke-Command"),
        QStringLiteral("Invoke-Expression"),
        QStringLiteral("Invoke-WebRequest"),
        QStringLiteral("Invoke-RestMethod"),
        QStringLiteral("ConvertTo-Json"),
        QStringLiteral("ConvertFrom-Json"),
        QStringLiteral("ConvertTo-Xml"),
        QStringLiteral("ConvertTo-Csv"),
        QStringLiteral("ConvertFrom-Csv")
    };

    // ===== Linux/macOS 独有命令 =====
    m_linuxPrefixes = {
        // 文件操作
        QStringLiteral("rm"),
        QStringLiteral("cp"),
        QStringLiteral("mv"),
        QStringLiteral("touch"),
        QStringLiteral("ln"),
        QStringLiteral("chmod"),
        QStringLiteral("chown"),
        QStringLiteral("chgrp"),
        QStringLiteral("umask"),
        QStringLiteral("split"),
        QStringLiteral("csplit"),

        // 文本处理
        QStringLiteral("egrep"),
        QStringLiteral("fgrep"),
        QStringLiteral("sed"),
        QStringLiteral("awk"),
        QStringLiteral("cut"),
        QStringLiteral("tr"),
        QStringLiteral("tee"),
        QStringLiteral("paste"),
        QStringLiteral("comm"),

        // 系统信息
        QStringLiteral("uname"),
        QStringLiteral("cal"),
        QStringLiteral("df"),
        QStringLiteral("du"),
        QStringLiteral("free"),
        QStringLiteral("top"),
        QStringLiteral("htop"),
        QStringLiteral("uptime"),
        QStringLiteral("lsof"),
        QStringLiteral("vmstat"),
        QStringLiteral("iostat"),
        QStringLiteral("sar"),
        QStringLiteral("nproc"),

        // 网络
        QStringLiteral("ifconfig"),
        QStringLiteral("ip"),
        QStringLiteral("traceroute"),
        QStringLiteral("tracepath"),
        QStringLiteral("ss"),
        QStringLiteral("dig"),
        QStringLiteral("host"),
        QStringLiteral("whois"),
        QStringLiteral("curl"),
        QStringLiteral("wget"),
        QStringLiteral("lftp"),
        QStringLiteral("nc"),
        QStringLiteral("netcat"),
        QStringLiteral("ssh"),
        QStringLiteral("scp"),
        QStringLiteral("sftp"),
        QStringLiteral("ftp"),
        QStringLiteral("telnet"),
        QStringLiteral("mail"),
        QStringLiteral("mutt"),

        // 进程控制
        QStringLiteral("killall"),
        QStringLiteral("pkill"),
        QStringLiteral("nice"),
        QStringLiteral("renice"),
        QStringLiteral("bg"),
        QStringLiteral("fg"),
        QStringLiteral("jobs"),
        QStringLiteral("nohup"),

        // 包管理（通用命令格式）
        QStringLiteral("apt"),
        QStringLiteral("apt-get"),
        QStringLiteral("apt-cache"),
        QStringLiteral("apt-search"),
        QStringLiteral("yum"),
        QStringLiteral("dnf"),
        QStringLiteral("pacman"),
        QStringLiteral("pacman -S"),
        QStringLiteral("pacman -Ss"),
        QStringLiteral("zypper"),
        QStringLiteral("dpkg"),
        QStringLiteral("rpm"),
        QStringLiteral("snap"),
        QStringLiteral("flatpak"),
        QStringLiteral("brew"),
        QStringLiteral("port"),
        QStringLiteral("pkgin"),

        // 磁盘操作
        QStringLiteral("mount"),
        QStringLiteral("umount"),
        QStringLiteral("fsck"),
        QStringLiteral("mkfs"),
        QStringLiteral("dd"),
        QStringLiteral("fdisk"),
        QStringLiteral("parted"),
        QStringLiteral("lsblk"),
        QStringLiteral("blkid"),
        QStringLiteral("sync"),

        // 用户管理
        QStringLiteral("adduser"),
        QStringLiteral("useradd"),
        QStringLiteral("usermod"),
        QStringLiteral("userdel"),
        QStringLiteral("addgroup"),
        QStringLiteral("groupadd"),
        QStringLiteral("groupmod"),
        QStringLiteral("groupdel"),
        QStringLiteral("passwd"),
        QStringLiteral("su"),
        QStringLiteral("sudo"),
        QStringLiteral("login"),
        QStringLiteral("logout"),
        QStringLiteral("last"),
        QStringLiteral("lastlog"),
        QStringLiteral("who"),
        QStringLiteral("w"),
        QStringLiteral("users"),

        // 系统控制
        QStringLiteral("shutdown"),
        QStringLiteral("reboot"),
        QStringLiteral("halt"),
        QStringLiteral("poweroff"),
        QStringLiteral("systemctl"),
        QStringLiteral("service"),
        QStringLiteral("journalctl"),
        QStringLiteral("dmesg"),
        QStringLiteral("strace"),
        QStringLiteral("ltrace"),
        QStringLiteral("ldd"),

        // 其他工具
        QStringLiteral("printf"),
        QStringLiteral("read"),
        QStringLiteral("test"),
        QStringLiteral("true"),
        QStringLiteral("false"),
        QStringLiteral("unalias"),
        QStringLiteral("source"),
        QStringLiteral("xargs"),
        QStringLiteral("watch"),
        QStringLiteral("crontab"),
        QStringLiteral("at"),
        QStringLiteral("batch"),
        QStringLiteral("nice"),
        QStringLiteral("ionice"),

        // 压缩/归档
        QStringLiteral("bzip2"),
        QStringLiteral("bunzip2"),
        QStringLiteral("xz"),
        QStringLiteral("unxz"),
        QStringLiteral("rar"),
        QStringLiteral("unrar"),
        QStringLiteral("7z"),
        QStringLiteral("7za"),

        // 性能监控
        QStringLiteral("vmstat"),
        QStringLiteral("mpstat"),
        QStringLiteral("iostat"),
        QStringLiteral("netstat"),
        QStringLiteral("sar"),
        QStringLiteral("nmon"),
        QStringLiteral("htop"),
        QStringLiteral("iotop"),
        QStringLiteral("atop"),
        QStringLiteral("glances"),

        // ===== macOS 独有命令 =====
        QStringLiteral("sw_vers"),
        QStringLiteral("system_profiler"),
        QStringLiteral("diskutil"),
        QStringLiteral("hdiutil"),
        QStringLiteral("codesign"),
        QStringLiteral("spctl"),
        QStringLiteral("xcode-select"),
        QStringLiteral("xcodebuild"),
        QStringLiteral("plistbuddy"),
        QStringLiteral("launchctl"),
        QStringLiteral("say"),
        QStringLiteral("open"),
        QStringLiteral("pbcopy"),
        QStringLiteral("pbpaste"),
        QStringLiteral("screencapture"),
        QStringLiteral("softwareupdate"),
        QStringLiteral("airport"),
        QStringLiteral("caffeinate"),
        QStringLiteral("mdutil"),
        QStringLiteral("mdfind"),
        QStringLiteral("mdls"),
        QStringLiteral("locate"),
        QStringLiteral("updatedb"),
        QStringLiteral("defaults"),
        QStringLiteral("dscl"),
        QStringLiteral("dscacheutil"),
        QStringLiteral("disk Arbitration"),
        QStringLiteral("imetutil"),
        QStringLiteral("security"),
        QStringLiteral("keychain"),
        QStringLiteral("codesign"),
        QStringLiteral("productbuild"),
        QStringLiteral("installer"),
        QStringLiteral("ditto"),
        QStringLiteral("SetFile"),
        QStringLiteral("GetFileInfo"),
        QStringLiteral("lsregister"),
        QStringLiteral("plutil"),
        QStringLiteral("osascript"),
        QStringLiteral("afplay"),
        QStringLiteral("say"),
        QStringLiteral("drutil"),
        QStringLiteral("cupsenable"),
        QStringLiteral("cupsdisable"),
        QStringLiteral("lpadmin"),
        QStringLiteral("lpstat"),
        QStringLiteral("cancel"),
        QStringLiteral("lp"),
        QStringLiteral("lpr"),
        QStringLiteral("launchd"),
        QStringLiteral("asr"),
        QStringLiteral("bless"),
        QStringLiteral("diskutil"),
        QStringLiteral("fdesetup"),
        QStringLiteral("profiles"),
        QStringLiteral("sysadminctl"),
        QStringLiteral("systemsetup"),
        QStringLiteral("networksetup"),
        QStringLiteral("scutil"),
        QStringLiteral("hostinfo"),
        QStringLiteral("gestoolutil")
    };

    // 根据运行时操作系统构建最终白名单
    m_allowedPrefixes = buildAllowedPrefixes();
}

QStringList ShellTool::buildAllowedPrefixes() {
    QStringList result = m_commonPrefixes;

#if defined(Q_OS_WIN)
    result.append(m_windowsPrefixes);
#else
    result.append(m_linuxPrefixes);
#endif

    return result;
}

QString ShellTool::description() const {
    return QStringLiteral("执行安全的 Shell 命令（白名单限制）。适用于列出目录、查看文件、获取系统信息等。");
}

QVariantMap ShellTool::parametersSchema() const {
    return QVariantMap{
        { "type", "object" },
        { "properties", QVariantMap{
            { "command", QVariantMap{
                { "type", "string" },
                { "description", "要执行的 Shell 命令（仅白名单允许）" }
            }}
        }},
        { "required", QVariantList{"command"} }
    };
}

void ShellTool::setAllowedPrefixes(const QStringList &prefixes) {
    m_allowedPrefixes = prefixes;
}

QStringList ShellTool::allowedPrefixes() const {
    return m_allowedPrefixes;
}

void ShellTool::setPowerShellAllowedPrefixes(const QStringList &prefixes) {
    m_powershellAllowedPrefixes = prefixes;
}

QStringList ShellTool::powershellAllowedPrefixes() const {
    return m_powershellAllowedPrefixes;
}

bool ShellTool::isAllowed(const QString &cmd) const {
    QString c = cmd.trimmed();
    if (c.isEmpty()) return false;

    // 移除前导的特殊符号
    if (c.startsWith(QChar('@')) || c.startsWith(QChar('!'))) {
        c = c.mid(1).trimmed();
    }

    if (c.isEmpty()) return false;

    // 分割命令获取第一个词
    QStringList parts = c.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.isEmpty()) return false;

    QString first = parts.value(0).toLower();

    // 运行时检测操作系统
#if defined(Q_OS_WIN)
    // Windows: 检测是否使用 PowerShell
    bool isPowerShell = first.contains(QChar('-'))
                     || c.startsWith("powershell", Qt::CaseInsensitive)
                     || c.startsWith("pwsh", Qt::CaseInsensitive);

    if (isPowerShell) {
        // 检查 PowerShell 命令白名单
        for (const QString &p : m_powershellAllowedPrefixes) {
            if (c.startsWith(p, Qt::CaseInsensitive)) {
                return true;
            }
        }
        // 允许直接调用 powershell.exe
        if (first == "powershell" || first == "pwsh") {
            if (parts.size() > 1) {
                QString second = parts.value(1).toLower();
                for (const QString &p : m_powershellAllowedPrefixes) {
                    if (second == p.toLower()) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    // 检查 Windows cmd.exe 白名单
    for (const QString &p : m_allowedPrefixes) {
        if (first == p.toLower()) {
            return true;
        }
        // 允许带扩展名的命令（如 dir.exe）
        if (first.startsWith(p.toLower() + QChar('.'))) {
            return true;
        }
    }
#else
    // Linux/macOS: 检查 Shell 白名单
    for (const QString &p : m_allowedPrefixes) {
        if (first == p.toLower()) {
            return true;
        }
        // 允许带路径的命令（如 /bin/ls）
        if (first.contains(p.toLower())) {
            QStringList pathParts = first.split(QChar('/'), Qt::SkipEmptyParts);
            if (!pathParts.isEmpty() && pathParts.last().toLower() == p.toLower()) {
                return true;
            }
        }
    }
#endif

    return false;
}

QString ShellTool::runCommand(const QString &cmd) {
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);

    QString shell;
    QStringList args;

#if defined(Q_OS_WIN)
    // Windows: 检测是否使用 PowerShell
    QString cmdLower = cmd.trimmed().toLower();
    bool usePowerShell = cmdLower.startsWith("powershell")
                      || cmdLower.startsWith("pwsh")
                      || cmd.contains(QChar('-'), Qt::CaseInsensitive);

    if (usePowerShell) {
        // 使用 PowerShell 执行
        shell = QStringLiteral("powershell.exe");
        args << QStringLiteral("-NoProfile")
             << QStringLiteral("-NonInteractive")
             << QStringLiteral("-Command")
             << cmd;
    } else {
        // Windows cmd.exe
        shell = QStringLiteral("cmd.exe");
        args << QStringLiteral("/C") << cmd;
    }
#else
    // Linux / macOS
    shell = QStringLiteral("sh");
    args << QStringLiteral("-c") << cmd;
#endif

    proc.setProcessEnvironment(QProcessEnvironment::systemEnvironment());

    proc.start(shell, args, QIODevice::ReadOnly);
    if (!proc.waitForFinished(15000)) {
        proc.kill();
        QJsonObject o;
        o["error"] = QStringLiteral("命令超时或启动失败");
        return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    }

    const QString stdoutStr = QString::fromUtf8(proc.readAllStandardOutput());
    const QString stderrStr = QString::fromUtf8(proc.readAllStandardError());
    const int code = proc.exitCode();

    QJsonObject o;
    o["stdout"] = stdoutStr;
    o["stderr"] = stderrStr;
    o["exitCode"] = code;
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

QString ShellTool::execute(const QVariantMap &args) {
    QString cmd = args.value("command").toString().trimmed();
    if (cmd.isEmpty()) {
        QString r = QStringLiteral("{\"error\":\"命令为空\"}");
        emit executionFinished(name(), r);
        return r;
    }

    if (!isAllowed(cmd)) {
        QString r = QStringLiteral("{\"error\":\"命令不在白名单中，拒绝执行: %1\"}").arg(cmd);
        emit executionFinished(name(), r);
        return r;
    }

    QString result = runCommand(cmd);
    emit executionFinished(name(), result);
    return result;
}
