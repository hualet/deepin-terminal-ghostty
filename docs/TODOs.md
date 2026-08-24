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
    - [x] pin `libghostty-vt` and the complete public header tree to
      `e77b2309fca3a27db1123a4f904b7fb432ee7162`
    - [x] inline/direct image storage
    - [x] inline PNG decode callback
    - [x] decoded RGB, RGBA, grayscale, and grayscale-alpha image conversion
    - [x] non-virtual placement rendering
    - [x] below-background / below-text / above-text z-layer rendering
    - [x] graphics/image generation-based cache invalidation
    - [ ] Unicode placeholder / virtual placements (`U=1`, required by
      `textual-image` TGP)
      - [ ] Preferred dependency: add a Ghostty C API that returns decoded
        virtual render fragments, including image/placement ids, viewport cell
        rectangle, source rectangle, and z-layer. The current placement helper
        intentionally reports virtual placements as not viewport-visible.
      - [ ] Available fallback dependencies: consume
        `GHOSTTY_ROW_DATA_KITTY_VIRTUAL_PLACEHOLDER`, render-cell graphemes,
        foreground/underline colors, and Ghostty's pinned row/column diacritic
        table to decode U+10EEEE cells in `qtghostty`.
      - [ ] Resolve virtual placements and relative placements whose root is
        virtual, including continuation cells, omitted row/column diacritics,
        32-bit image ids, and optional placement ids.
      - [ ] Paint only the matching source fragment for every placeholder run,
        honor source cropping, scaling, offsets and z-order, and suppress the
        U+10EEEE glyph without suppressing unrelated text decorations.
      - [ ] Cover scrolling, scrollback, resize/reflow, clipping, deletion,
        retransmission, alternate-screen switching, and overlapping virtual
        placements.
    - [ ] Animated images
      - [x] Ghostty exposes current-frame pixels and changes image generation
        when its internal animation state advances.
      - [ ] Required upstream dependency: expose animation tick/next-deadline
        or wake scheduling through the C API. Reading the current frame alone
        cannot advance an animation from a Qt embedder.
      - [ ] Drive the API from a single-shot Qt timer, invalidate only affected
        image caches/regions, and stop scheduling when no visible animation is
        active.
      - [ ] Verify frame loading, composition modes, background frames, loop
        counts, frame gaps, replacement, deletion, and hidden/offscreen images.
    - [ ] Local transfer media
      - [ ] Temporary file first: pass a `GhosttyString` restricted to an
        application-controlled temporary directory, define symlink/path and
        lifecycle policy, and add outside-directory rejection tests.
      - [ ] Shared memory: gate by Linux/platform availability, retain
        Ghostty's size/range checks, define same-user namespace expectations,
        and verify cleanup on success, failure, deletion, and terminal reset.
      - [ ] Direct file: keep disabled unless an explicit product security
        policy permits terminal clients to read arbitrary local paths; prefer
        an opt-in allowlist or sandboxed broker if enabled.
    - [ ] Protocol conformance and regression matrix
      - [ ] Transmission: chunked direct payloads, RGB/RGBA/PNG, zlib,
        pending payload completion, image id/number lookup, quiet/query
        responses, malformed input, and storage/APC limits.
      - [ ] Placement: transmit-and-display, display existing image, source
        cropping, pixel offsets, cell sizing, cursor movement, pinned and
        relative placement roots, negative z-index layers, scroll/resize
        geometry, and partial viewport clipping.
      - [ ] Mutation: retransmit the same id, replace with the same dimensions,
        delete by id/number/placement/z/cursor scope, reset, and main/alternate
        screen lifetime.
      - [ ] Interoperability fixtures: `kitten icat` stream mode,
        `kitten icat --unicode-placeholder`, `textual-image` TGP, Kitty's
        graphics protocol test programs, and captured APC byte streams that run
        without depending on a live shell.
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
