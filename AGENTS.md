# 项目工作路径

- 最新交付：RP2350已刷 **0.3.0 / 300 MHz**，新增边采样边USB传输与8路网页（C++17本地服务+原生Canvas）。`tools/start-logic-web.ps1` 启动 http://127.0.0.1:8765/ ，服务PID记录artifacts/streaming/server.pid。256KiB双DMA环、4096点块、最长2ms合并、8×16KiB WinUSB RAW_IO读取；自动无损RLE/RAW回退。25kHz源以10MS/s连续约10秒99,999,744点、网页41,840,640点，CRC/序号/flags通过；源已恢复1000step/s。RAW750kS/s通过，1MS/s溢出；约0.8MB/s有效流，不能承诺任意信号10MS/s持续。溢出明确flags8停止，完整LAS/JSON在artifacts/web-captures，网页仅保留30秒/最多1048576次跳变；VCD/CSV导出可视窗口。指南docs/logic-web.md，报告reports/逻辑分析仪网页与USB流采样_2026-11-02.md。早期短包版25kHz源溢出已保留失败证据，合并后通过。此前输入毛刺、触发首点、SWD NoACK、pyOCD与LA句柄冲突均不能称已解决；本轮只验证流传输，未重复原严格波形回归。旧定长CLI、DAP USB自测仍通过；旧0.2.2备份artifacts/streaming/ai_debugprobe-v0.2.2-before-stream.uf2。

- 历史：0.2.2/300 MHz严格波形测试18轮中7轮有毛刺/触发首点问题；详见reports/RP2350_300MHz_10MSps_实测报告_2026-11-02.md。先前E9灯未亮后改B9，用户已目视确认B9闪烁；烧录6.594秒，64KiB回读、B9电平及八路回归通过。当前 E9/B9/RT1064 高变化率信号源仍照常，速率已恢复1000 step/s。Windows pyOCD/libusb与独立LA同时开句柄曾有Windows=5；SWD密集读取NoACK仍未完全解决。

- 用户指定的唯一项目开发目录为 `G:\material\week_prj\ai_debuginstructment`。
- 所有项目开发、命令执行、下载、资料整理和文件修改都基于此目录。执行命令时显式指定此工作目录。
- 不再使用旧路径 `C:\Users\666\Documents\ChatGPT\ai_debuginstructment`，也不要在那里重新创建项目文件。
- 项目的源代码、工具、依赖、下载文件、固件备份、构建产物、缓存和临时文件均保存到本项目目录下。需要临时目录或缓存时，先在 G 盘项目内建立对应目录，并为当前进程配置相关环境变量；不要修改系统全局设置。
- `datasheet/README.md` 是开发板资料索引；现有 `main.py` 是此前 LED2 亮灭各 200 ms 的历史验证程序。
- 操作硬件前重新检测串口；串口号可能变化。完整原固件备份保存在 `datasheet/05_blink_and_backup/original-firmware.uf2`。

# 固件开发语言与选型

- 用户明确要求：后续 RP2350 固件仅使用 C 或 C++ 开发，不再新增 MicroPython 固件功能。
- 默认工程方案：应用层使用 C++17，通过 Raspberry Pi Pico SDK 调用 C 接口；SDK、厂商示例和底层驱动可保留 C。C 驱动以独立 `.c` 源文件编译，跨语言接口使用适当的 C linkage。
- C++17 是本项目建议的应用标准，不是对 Pico SDK 默认标准的描述。具体工程应显式配置语言标准。
- 默认关闭 C++ 异常和 RTTI；优先静态分配、固定容量缓冲区与明确所有权。中断和实时关键路径避免动态内存分配、阻塞操作及无界执行时间。
- 根据实际需要使用类、RAII、枚举和模板；性能、内存、栈及固件体积通过实际构建和测量判断，不假定 C 或 C++ 必然更快、更小。
- 使用本板匹配的 `waveshare_rp2350b_plus_w` 板级配置，核实 16 MB Flash、RP2350B GPIO 和无线模块接线。
- 开发优先使用 Pico SDK + CMake；工具链、SDK、构建目录及缓存保存在本项目 G 盘目录内。

# 项目目标与设计阶段

- 用户确认制作支持 SWD 和 JTAG 的通用下载调试器，从 STM32 开始，并由 AI 调用电脑端工具。目标不限于复刻当前连接的 CMSIS-DAP 或 XDS110。
- 架构提案见 `docs/architecture.md`：推荐 CMSIS-DAP v2 + OpenOCD + GDB/MI + C++ 工具服务。具体 STM32 型号和接线尚待确认，不能将提案当成已实现功能。
- 用户已于 2026-11-01 确认开始开发，允许实现探针固件并改刷 RP2350。先只读识别 STM32，未明确授权覆盖目标程序前不擦写其 Flash。
- 首块目标为 STM32F407，接线约定见 `docs/wiring-stm32f407.md`。原型分配为 GP2=SWCLK/TCK、GP3=SWDIO/TMS、GP4=nRESET，JTAG 预留 GP5=TDI、GP6=TDO、GP7=nTRST；后续固件须与用户按此连接的硬件一致。
- 用户实际仅接 GP2、GP3、GND，未接 NRST。首版按无外接复位配置，不使用 connect-under-reset；必要时请求用户手动复位。GP4 保持高阻。
- 用户于 2026-11-02 明确授权：通过 RP2350 探针给 STM32F407 烧录 PB2 LED 闪烁 C++ demo。允许覆盖目标启动扇区，完成后保留 demo 运行；原始 1 MB Flash 备份继续保留。不再就该 demo 的烧录重复请求许可。
- 用户于 2026-11-02 确认：同一块 RP2350 同时运行下载调试器和逻辑分析仪，不能按两块板分工或仅切换固件来设计。当前处于规划阶段，见 `docs/logic-analyzer-architecture.md`；首版建议保留 GP2–GP7 的调试分配，以 GP8–GP15 作为 8 路分析输入，PIO + DMA + SRAM 采集，USB 独立接口并保留 CMSIS-DAP。
- 用户随后授权按规划顺序开发。0.2.0 已实现并刷入单板双功能基础工程，当前 VID:PID 为 CAFE:4016，逻辑接口为 MI_03；见 `docs/logic-analyzer-usage.md` 与 `docs/logic-analyzer-bringup-2026-11-02.md`。JTAG、预触发、解码和 AI 服务未实现；并发烧录重枚举仍待排查。
- 用户明确说 PB2→GP8 采样线稍后才接，先完成工程，不要重复催促接线。可继续软件工作，真实波形/触发验收留待接线后；不得把未连接输入的噪声当作已知波形。
- 当前接入逐飞 RT1064 Lite V1.0 / MIMXRT1064DVL6B，用户已明确授权覆盖当前程序，并确认外接 LED 接板级 E9 信号焊盘。E9 按逐飞映射为 GPIO_EMC_09 / GPIO4_IO09，与 SDRAM 地址线复用。已烧录不使用 SDRAM 的 C++17 demo，焊盘 PSR/DR 高低交替通过，最终 6.908 秒；见 reports/RT1064_E9_烧录报告_2026-11-02.md。不要重复询问此次授权，也不要把 E9 当作芯片封装球位。
- RP2350 当前已刷 0.2.1，修复 pyOCD DAP_TRANSFER_CONFIGURE 等待重试150被拒的问题。RT1064 使用 pyOCD 0.46.1 专用 Flash 算法，OpenOCD 只读配置不能烧此芯片。4-MiB 原 Flash 双读一致备份在 artifacts/rt1064/backup/。烧录必须先系统复位/重新初始化 FlexSPI 后回读校验，避免缓存旧数据；失败不自动恢复运行。GP4仍无物理复位驱动；GP0/1 UART桥接仍未实现。
- 用户随后授权RT1064在已接的八路LA线上输出测试信号。当前已刷rt1064-signals.bin（C++17，保留E9闪灯），GP8–15依次对应B2/B13/B12/B17/B19/B21/B23/B25，全属GPIO1的2/13/12/17/19/21/23/25位。默认1 kstep/s二进制计数，CH0=500Hz逐路二分；可用tools/set-rt1064-signal-rate.ps1改RAM到50kstep/s。最后已恢复默认。21次真实采集/1966080样点，16项触发、最高10MS/s采集25kHz信号、SWD只读并发通过；详见reports/RT1064_逻辑分析仪实测_2026-11-02.md。不要再称八路外部信号完全未验收，也不要把10MS/s采样率说成10MHz输入带宽。多线进位边缘观察到单样点混合值，检查器记录而不删除原始数据。
