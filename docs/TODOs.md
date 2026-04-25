- [x] 多标签
  - [x] 标签页支持
  - [x] 垂直标题栏支持
- [x] 快捷键设置
  - [ ] 优化一些切换标签页的快捷键，应该是 Alt+数字，按下 Alt 标签页应该有数字提示
- [ ] 个性化设置
  - [ ] 主题配色
  - [x] 字体
- [x] 屏幕拆分
- [x] 查找
- [x] shell standard keybindings
- [ ] 现代扩展协议
  - [ ] kitty image protocol
- [ ] 面向多 Agent 优化
  - [ ] pane 状态点: 接入 shell 集成或命令退出码通路，再区分运行中/成功/失败状态；当前 UI 已先移除状态点，避免误导
- [ ] SE
  - [x] 添加单元测试
  - [x] 添加日志系统
  - [x] 添加自动化构建和打包
  - [ ] CI
    - [ ] clang-static-analyzer
- [ ] 兼容性和性能测试
  - [ ] 兼容性: vttest + ucs-detect
  - [ ] 兼容性: esctest
    - [x] 启动入口: `--execute` / `--wait-for-child` / `--propagate-exit-code`
    - [x] 包装脚本: `tests/run_esctest.sh`
    - [x] 基础子集: `tests/run_esctest_basic_subset.sh`，当前固定 59 个本地实跑可通过的用例
    - [ ] 固定 headless 环境: 在 CI 镜像中补齐 `xvfb-run`、字体和稳定窗口尺寸
    - [ ] 上游兼容: `esctest` upstream 仍是 Python 2.7，当前靠本地 checkout 转换为 Python 3 兼容副本；后续要决定是维护补丁集还是锁定 fork
    - [ ] 退出码: `esctest` upstream 本身失败仍返回 `0`，后续需要决定是继续在外层解析日志，还是维护一个返回非零的 runner patch
    - [ ] 稳定性: `CUUTests.test_CUU_ExplicitParam` / `CUUTests.test_CUU_StopsAtTopLine` 单跑可过、混跑会漂，需定位状态污染来源
    - [ ] `DA/DSR`: 对齐设备属性画像，并补 `DECDSR DECXCPR` 等状态报告回包
    - [ ] 标题类 OSC: 明确 title query / winops 语义，决定是对齐 `xtermWinops` 还是单独维护本项目画像
    - [ ] 基础编辑类: 先筛出不依赖 checksum 的 `ED` / `EL` / `ICH` / `DCH` / `IL` / `DL` 子集，再逐步扩大
    - [ ] 基础 SGR: upstream 目前没有现成可直接纳入的 `SGR` 自动化子集，需要补充测试策略
    - [ ] 鼠标协议: 接入 Ghostty mouse encoder，把 press / move / release / wheel 在 tracking 模式下写回 PTY
    - [ ] 屏幕校验: 确认并补齐 `DECRQCRA` 等矩形校验和查询支持，再开放依赖 checksum 的用例
    - [ ] 扩展协议: 分批验证 OSC 8、kitty image、focus reporting 等高级用例
    - [ ] 用例分层: 先整理 smoke / core / extended 子集，避免一开始把不支持项全部跑红
  - [ ] 吞吐: vtebench + termbench
  - [ ] 交互体验: typometer



## ucs-detect current status

╔══════════════════════════════════════╗ ╔════════════════════════════════════════════╗
║        Terminal Capabilities         ║ ║         Terminal Capabilities (2)          ║
╠════════════════════╦═════════════════╣ ╠══════════════════════════════════════╦═════╣
║      Terminal Type ║ xterm-256color  ║ ║                Bracketed Paste MIME? ║ No  ║
║           Software ║ deepin-termina… ║ ║            Color Report (OSC 10/11)? ║ No  ║
║             Colors ║ 24-bit          ║ ║               Enable SGR Mouse Mode? ║ Yes ║
║       Size (cells) ║ 89 x 32         ║ ║ In-Band Window Resize Notifications? ║ Yes ║
║      Size (pixels) ║ 801 x 576       ║ ║                     iTerm2 Features? ║ No  ║
║ Cell Size (pixels) ║ 9 x 18          ║ ║                     Kitty Clipboard? ║ No  ║
║       Aspect Ratio ║ 4:3 (VGA)       ║ ║                      Kitty Keyboard? ║ Yes ║
║     Tab Stop Width ║ 8               ║ ║                 Kitty Notifications? ║ No  ║
║          Graphics? ║ Kitty, iTerm2   ║ ║                Kitty Pointer Shapes? ║ No  ║
║       Device Class ║ VT500           ║ ║                   Kitty Text Sizing? ║ No  ║
║    Ambiguous Width ║ narrow (1)      ║ ║                    OSC 52 Clipboard? ║ No  ║
║    Graphemes(2027) ║ Yes             ║ ║        Send FocusIn/FocusOut events? ║ Yes ║
║               WIDE ║ 100.0 %         ║ ║            Set bracketed paste mode? ║ Yes ║
║      Standalone RI ║ 100.0 %         ║ ║                 Synchronized Output? ║ Yes ║
║   Standalone Fitz. ║ 100.0 %         ║ ║                           XTGETTCAP? ║ No  ║
║           RI Flags ║ 100.0 %         ║ ╚══════════════════════════════════════╩═════╝
║                ZWJ ║ 100.0 %         ║                                               
║               VS16 ║ 100.0 %         ║                                               
║               VS15 ║ 95.6 %          ║                                               
║          Languages ║ 83.9 %          ║                                               
╚════════════════════╩═════════════════╝                                               

╔══════════════════════════════════════════════════╗
║       Language Support (99 of 118 passed)        ║
╠═════════════════════╦═══════╦══════════╦═════════╣
║       Language      ║ Total ║ Failures ║ Success ║
╠═════════════════════╬═══════╬══════════╬═════════╣
║       Bengali       ║  385  ║    96    ║  75.1 % ║
║       Bhojpuri      ║  313  ║    41    ║  86.9 % ║
║       Burmese       ║  268  ║    10    ║  96.3 % ║
║       Gujarati      ║  343  ║    50    ║  85.4 % ║
║        Hindi        ║  390  ║    82    ║  79.0 % ║
║ Javanese (Javanese) ║  530  ║    43    ║  91.9 % ║
║       Kannada       ║  287  ║    14    ║  95.1 % ║
║    Khmer, Central   ║  443  ║    8     ║  98.2 % ║
║         Khün        ║  396  ║    1     ║  99.7 % ║
║        Magahi       ║  314  ║    43    ║  86.3 % ║
║       Maithili      ║  357  ║    71    ║  80.1 % ║
║      Malayalam      ║  845  ║   246    ║  70.9 % ║
║       Marathi       ║  391  ║    86    ║  78.0 % ║
║         Mon         ║  332  ║    3     ║  99.1 % ║
║        Nepali       ║  352  ║    69    ║  80.4 % ║
║       Sanskrit      ║  493  ║   145    ║  70.6 % ║
║  Sanskrit (Grantha) ║  293  ║    39    ║  86.7 % ║
║   Tamang, Eastern   ║   70  ║    11    ║  84.3 % ║
║        Telugu       ║  384  ║    42    ║  89.1 % ║
╚═════════════════════╩═══════╩══════════╩═════════╝
