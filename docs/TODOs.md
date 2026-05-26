- [x] 多标签
  - [x] 标签页支持
  - [x] 垂直标题栏支持
- [x] 快捷键设置
  - [ ] 优化一些切换标签页的快捷键，应该是 Alt+数字，按下 Alt 标签页应该有数字提示
- [ ] 个性化设置
  - [x] 主题配色
    - [x] 更多主题
    - [ ] 自定义主题
  - [x] 背景透明 & 背景模糊
  - [x] 字体
- [x] 屏幕拆分
- [x] 查找
- [x] shell standard keybindings
- [ ] 现代扩展协议
  - [ ] kitty image protocol
    - [x] inline/direct image storage
    - [x] inline PNG decode callback
    - [x] non-virtual placement rendering
    - [x] below-background / below-text / above-text z-layer rendering
    - [ ] Unicode placeholder / virtual placements (`U=1`, required by textual-image TGP)
    - [ ] file / temporary-file / shared-memory media
- [ ] Quake 模式
- [x] shell integration
- [x] 标签页状态信息展示
  - [x] 显示当前执行命令的图标
  - [x] 显示当前执行命令的状态
- [ ] 面向多 Agent 优化
  - [x] 支持 Agent 图标显示
  - [ ] ACP：获取 Agent 执行状态
- [ ] SE
  - [x] 添加单元测试
  - [x] 添加日志系统
  - [x] 添加自动化构建和打包
  - [ ] 添加玲珑打包
  - [x] CI
    - [x] clang-static-analyzer
- [ ] 兼容性和性能测试
  - [ ] 兼容性: vttest + ucs-detect
  - [ ] 兼容性: esctest
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
║    Ambiguous Width ║ narrow (1)      ║ ║                    OSC 52 Clipboard? ║ Yes ║
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
