#include "ProcessIcon.h"

#include <QHash>
#include <QSet>

namespace {

const QHash<QString, QString> &iconNames() {
    static const QHash<QString, QString> kIconNames = {
        {QStringLiteral("claude"), QStringLiteral("claude")},
        {QStringLiteral("claude-code"), QStringLiteral("claude")},
        {QStringLiteral("codex"), QStringLiteral("codex")},
        {QStringLiteral("copilot"), QStringLiteral("github-copilot")},
        {QStringLiteral("github-copilot"), QStringLiteral("github-copilot")},
        {QStringLiteral("gemini"), QStringLiteral("gemini")},
        {QStringLiteral("gemini-cli"), QStringLiteral("gemini")},
        {QStringLiteral("aider"), QStringLiteral("aider")},
        {QStringLiteral("opencode"), QStringLiteral("opencode")},
        {QStringLiteral("goose"), QStringLiteral("goose")},
        {QStringLiteral("qwen"), QStringLiteral("qwen")},
        {QStringLiteral("qwen-code"), QStringLiteral("qwen")},
        {QStringLiteral("kimi"), QStringLiteral("kimi")},
        {QStringLiteral("kimi-cli"), QStringLiteral("kimi")},
        {QStringLiteral("ssh"), QStringLiteral("shell")},
        {QStringLiteral("mosh"), QStringLiteral("shell")},
        {QStringLiteral("mosh-client"), QStringLiteral("shell")},
        {QStringLiteral("docker"), QStringLiteral("docker")},
        {QStringLiteral("podman"), QStringLiteral("podman")},
        {QStringLiteral("kubectl"), QStringLiteral("kubernetes")},
        {QStringLiteral("helm"), QStringLiteral("helm")},
        {QStringLiteral("nvim"), QStringLiteral("nvim")},
        {QStringLiteral("vim"), QStringLiteral("vim")},
        {QStringLiteral("nano"), QStringLiteral("nano")},
        {QStringLiteral("emacs"), QStringLiteral("emacs")},
        {QStringLiteral("htop"), QStringLiteral("htop")},
        {QStringLiteral("btop"), QStringLiteral("htop")},
        {QStringLiteral("top"), QStringLiteral("htop")},
    };
    return kIconNames;
}

// Interpreters whose first operand is the script that names the program.
bool isInterpreter(const QString &name) {
    static const QSet<QString> kInterpreters = {
        QStringLiteral("node"),
        QStringLiteral("nodejs"),
        QStringLiteral("bun"),
    };
    return kInterpreters.contains(name);
}

// Runners whose first operand is the package or program they launch.
bool isPackageRunner(const QString &name) {
    static const QSet<QString> kPackageRunners = {
        QStringLiteral("npx"),
        QStringLiteral("pnpx"),
        QStringLiteral("bunx"),
        QStringLiteral("uvx"),
    };
    return kPackageRunners.contains(name);
}

// Runners that launch a package only through a specific subcommand, so
// "npm exec codex" resolves but "npm install codex" does not.
bool isRunnerSubcommand(const QString &name, const QString &subcommand) {
    static const QHash<QString, QSet<QString>> kRunnerSubcommands = {
        {QStringLiteral("npm"), {QStringLiteral("exec"), QStringLiteral("x")}},
        {QStringLiteral("pnpm"), {QStringLiteral("dlx"), QStringLiteral("exec")}},
        {QStringLiteral("yarn"), {QStringLiteral("dlx")}},
        {QStringLiteral("pipx"), {QStringLiteral("run")}},
    };
    const auto it = kRunnerSubcommands.constFind(name);
    return it != kRunnerSubcommands.constEnd() && it->contains(subcommand);
}

// Reduces a path, package spec or module name to a bare program name:
// "/usr/bin/docker" -> "docker", "@openai/codex@latest" -> "codex",
// ".../gemini-cli/dist/index.js" -> "gemini-cli", "vim.basic" -> "vim".
QString programName(const QString &value) {
    static const QSet<QString> kGenericParts = {
        QStringLiteral("index"),    QStringLiteral("cli"),   QStringLiteral("main"),
        QStringLiteral("__main__"), QStringLiteral("dist"),  QStringLiteral("bin"),
        QStringLiteral("lib"),      QStringLiteral("build"), QStringLiteral("src"),
    };

    const QStringList parts = value.toLower().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (auto it = parts.crbegin(); it != parts.crend(); ++it) {
        QString part = *it;
        // A leading '@' marks an npm scope; a later one starts a version.
        const int versionStart = part.indexOf(QLatin1Char('@'), 1);
        if (versionStart > 0)
            part.truncate(versionStart);
        const int suffixStart = part.indexOf(QLatin1Char('.'), 1);
        if (suffixStart > 0)
            part.truncate(suffixStart);
        if (part.isEmpty() || part.startsWith(QLatin1Char('@')) || part.startsWith(QLatin1Char('.'))
            || kGenericParts.contains(part))
            continue;
        return part;
    }
    return {};
}

int operandIndex(const QStringList &arguments, int start) {
    for (int i = start; i < arguments.size(); ++i) {
        if (!arguments.at(i).startsWith(QLatin1Char('-')))
            return i;
    }
    return -1;
}

QString iconForArguments(const QStringList &arguments) {
    // Every step moves past at least one argument, so the loop terminates.
    int index = arguments.isEmpty() ? -1 : 0;
    while (index >= 0) {
        const QString name = programName(arguments.at(index));
        const QString icon = iconNames().value(name);
        if (!icon.isEmpty())
            return icon;

        if (name.startsWith(QStringLiteral("python"))) {
            int next = index + 1;
            while (next < arguments.size() && arguments.at(next).startsWith(QLatin1Char('-'))) {
                if (arguments.at(next) == QStringLiteral("-m"))
                    return iconNames().value(programName(arguments.value(next + 1)));
                ++next;
            }
            index = next < arguments.size() ? next : -1;
        } else if (isInterpreter(name) || isPackageRunner(name)) {
            index = operandIndex(arguments, index + 1);
        } else {
            const int subcommand = operandIndex(arguments, index + 1);
            if (subcommand < 0 || !isRunnerSubcommand(name, arguments.at(subcommand)))
                return {};
            index = operandIndex(arguments, subcommand + 1);
        }
    }
    return {};
}

} // namespace

QString processIconName(const QString &processName, const QStringList &arguments) {
    const QString icon = iconNames().value(programName(processName));
    if (!icon.isEmpty())
        return icon;

    // Programs that set their process title (e.g. npm's "npm exec codex")
    // overwrite argv with one space-separated string.
    if (arguments.size() == 1)
        return iconForArguments(arguments.first().split(QLatin1Char(' '), Qt::SkipEmptyParts));
    return iconForArguments(arguments);
}
