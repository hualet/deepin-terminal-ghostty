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

建议直接放在项目目录里：

```bash
git clone --depth=1 https://gitlab.freedesktop.org/terminal-wg/esctest.git third_party/esctest
```

3. 无图形环境时，系统需要有 `xvfb-run`。

## 推荐用法

如果你的 `esctest` checkout 里存在常见入口文件，脚本会自动探测。现在也支持 upstream 的
`esctest/esctest.py` 布局：

```bash
tests/run_esctest.sh --esctest-dir /path/to/esctest -- --expected-terminal=xterm
```

如果自动探测失败，直接显式传入要在启动 tab 中执行的命令：

```bash
tests/run_esctest.sh \
  --esctest-command "python3 /path/to/esctest/esctest.py --tests cursor"
```

上游仓库当前仍是 Python 2.7 代码。`tests/run_esctest_basic_subset.sh` 会在首次运行时对本地
checkout 做一次 `python3 -m lib2to3 -w` 转换，并在 checkout 下写入一个
`.codex-python3-ready` 标记文件。

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

## 当前可跑子集

当前仓库里新增了一个“只跑已验证可稳定通过的基础子集”的脚本：

```bash
tests/run_esctest_basic_subset.sh
```

这条脚本目前会：

1. 自动准备本地 `third_party/esctest` checkout 的 Python 3 兼容副本
2. 用真实产品二进制启动 `esctest`
3. 只选择当前已验证能稳定通过的 59 个基础用例
4. 把 `esctest` 日志写到 `build/test-logs/esctest-basic-subset.log`
5. 额外解析日志，在出现失败或 known bug 时返回非零退出码

当前纳入子集的主要范围：

1. 光标移动基础用例：`CHA`、`CNL`、`CPL`、`CUB`、`CUD`、`CUF`、`CUP`、`CUU`
2. 定位类用例：`HPA`、`HPR`、`HVP`、`VPA`、`VPR`
3. 滚动区基础用例：`DECSTBM` 的 cursor-to-origin，以及 `IND` / `LF` / `NEL` / `RI` / `VT`
   的 basic 用例

这批用例是按本地实跑结果筛出来的，不是按协议名静态猜的。未纳入的项见下方“当前限制”。
其中 `CUUTests.test_CUU_ExplicitParam` 和 `CUUTests.test_CUU_StopsAtTopLine` 单独跑可过，但混跑时会漂，
当前也先排除在稳定子集之外。

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

## 当前限制

这轮实跑后，下面几类还不能稳定纳入自动化子集：

1. 编辑类大多数用例：很多断言最终落到 `DECRQCRA`/checksum，当前会超时
2. 滚动区的复杂场景：同样大量依赖 checksum
3. `DA/DSR`：
   - `DA` 当前返回 `CSI ? 62 ; 1 ; 6 ; 22 c`，和 `esctest` 的 `xterm` / `iTerm2` 画像都不完全匹配
   - `DECDSR DECXCPR` 当前没有回包，直接超时
4. 标题类 `OSC`：
   - `SMTitle` 当前只能观测到部分行为
   - `esctest` 对 title query 走的是 `xtermWinops` 路径，当前不能作为稳定通过项
5. 基础 `SGR`：upstream `esctest` 当前没有独立的、可直接纳入的 `SGR` 自动化子集

`esctest` 里依赖鼠标报告、矩形校验和或更高级扩展协议的用例，还需要补能力后再逐步放开。详见 [TODOs.md](/home/hualet/projects/hualet/deepin-terminal-ghostty/docs/TODOs.md)。
