# `esctest` 接入说明

## 目标

用真实产品二进制 `deepin-terminal-ghostty` 启动一条 `esctest` 命令，并把退出码带回调用方，便于本地或 CI 做兼容性回归。

## 前提

1. 已构建应用：

```bash
cmake -B build -DBUILD_TESTING=ON
cmake --build build
```

2. 本地有一份 `esctest` checkout。

3. 无图形环境时，系统需要有 `xvfb-run`。

## 推荐用法

如果你的 `esctest` checkout 里存在常见入口文件，脚本会自动探测：

```bash
tests/run_esctest.sh --esctest-dir /path/to/esctest -- --tests cursor
```

如果自动探测失败，直接显式传入要在启动 tab 中执行的命令：

```bash
tests/run_esctest.sh \
  --esctest-command "python3 /path/to/esctest/esctest.py --tests cursor"
```

脚本默认行为：

1. 启动 `build/deepin-terminal-ghostty`
2. 通过 `--execute` 把 `esctest` 命令放进首个 tab
3. 使用 `--propagate-exit-code` 把 `esctest` 退出码返回给外层 shell
4. 在没有 `DISPLAY` 时自动包一层 `xvfb-run -a`

如果要强制指定启动会话目录：

```bash
tests/run_esctest.sh \
  --esctest-dir /path/to/esctest \
  --working-directory /path/to/esctest \
  -- --tests cursor
```

## 诊断

先看脚本最终会执行什么：

```bash
tests/run_esctest.sh \
  --esctest-command "python3 /path/to/esctest/esctest.py --tests cursor" \
  --print-command
```

先做一条最小回归，确认退出码通路正常：

```bash
QT_QPA_PLATFORM=offscreen ./build/deepin-terminal-ghostty \
  --execute "exit 23" \
  --propagate-exit-code
```

预期进程退出码是 `23`。

## 已自动化的 smoke tests

当前仓库里已经补了这几条可稳定运行的自动化覆盖：

1. `test_startup_options`：命令行参数解析
2. `test_pty_session`：指定命令、工作目录、子进程退出码
3. `test_main_window`：启动会话退出信号与 `wait-for-child` 关闭窗口链路
4. `StartupCommandIntegration`：真实二进制执行指定命令并传播退出码、工作目录
5. `EsctestWrapperSmoke`：`tests/run_esctest.sh` 自动探测入口并透传退出码

## 当前范围

这套接入目前只解决了两件事：

1. 应用可以在首个 tab 里执行指定命令
2. 启动会话的退出码可以传播回外层进程

`esctest` 里依赖鼠标报告、矩形校验和或更高级扩展协议的用例，还需要补能力后再逐步放开。详见 [TODOs.md](/home/hualet/projects/hualet/deepin-terminal-ghostty/docs/TODOs.md)。
