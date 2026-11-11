# RP2350 通用下载调试器与八通道逻辑分析仪

工作目录：`G:\material\week_prj\ai_debuginstructment`。自研固件与主机工具使用 C++17，复用 Arm CMSIS-DAP 和 Pico SDK 的 C 实现。

GitHub：<https://github.com/ukg44059-lang/rp2350-debug-instrument>（公开工程）。[打包与新电脑使用说明](docs/github-delivery.md)。Windows运行包与源码/资料压缩包在 Releases 提供；依赖版本记录为 `third_party/versions.json`。

## 当前实机状态

- **当前为0.3.0 / 300 MHz**，新增8通道实时网页与采样/USB同时传输。运行 `.\tools\start-logic-web.ps1`，打开 <http://127.0.0.1:8765/>。可缩放、游标测时、边沿触发、保存历史及导出VCD/CSV/LAS。25kHz信号源10MS/s约10秒、约1亿点流传输通过；RAW750kS/s通过，1MS/s溢出，默认采用无损压缩。10MS/s连续能力取决于信号变化率；此前输入毛刺与SWD问题未因此判定解决。见[网页使用指南](docs/logic-web.md)和[实测报告](reports/逻辑分析仪网页与USB流采样_2026-11-02.md)。下列0.2.2及更早记录为历史。

- 历史：用户已目视确认 B9 LED 闪烁。0.2.2/300 MHz 严格波形回归18轮中7轮出现毛刺/触发首样点异常；完整结果和回退方法见[300 MHz / 10 MS/s报告](reports/RP2350_300MHz_10MSps_实测报告_2026-11-02.md)。
- 用户反馈E9灯未闪后，已改用逐飞板载LED脚B9（GPIO1_IO09），亮灭各200ms，保留八路测试信号。64KiB烧录回读、B9高低电平及八路采样回归均通过；烧录6.594秒。源码见target_demo/rt1064/blink.cpp，下文E9记录为此前版本。

- 当前接入目标为逐飞 RT1064 Lite（MIMXRT1064DVL6B）。已烧录 C++ E9 闪灯及八路信号测试程序，高低各 200 ms；八路输出连接 GP8–15，默认500 Hz逐路二分。21次真实采集、16项边沿触发及SWD只读并发通过，见 [RT1064逻辑分析仪报告](reports/RT1064_逻辑分析仪实测_2026-11-02.md)。此前单独闪灯程序及6.908秒计时见 [E9烧录报告](reports/RT1064_E9_烧录报告_2026-11-02.md)。原STM32记录保留在下面供历史参考。
- RP2350 当前一体化固件 `0.3.0`，接受 pyOCD 默认 150 次 WAIT 重试，保留 CMSIS-DAP SWD 和独立逻辑分析接口，采用本板 `waveshare_rp2350b_plus_w` 配置。
- Windows 自动绑定 CMSIS-DAP 和 AI Logic Analyzer 两个 WinUSB 接口；CDC 维护接口当前为 COM40（端口号可变），不提供目标 UART 桥接。
- 探针序列号：`0C36968AD31958CA`；当前实验室原型 VID:PID：`CAFE:4016`，旧 0.1.0 为 `CAFE:4015`。这些标识不是生产用 USB 标识分配。
- 接线：GP2→STM32 PA14、GP3→PA13、GND→GND；**未连接 NRST**。GP4 保持高阻。
- OpenOCD 已通过本探针识别 Cortex-M4 r0p1，并读出 CPUID `0x410fc241`、DBGMCU_IDCODE `0x10016413`、1024 KB Flash、UID 与现有向量表。
- 这些 ID 与 STM32F405/407/415/417 家族匹配，不能仅凭 ID 判断封装及完整料号。用户所提供板型为 STM32F407。
- 2026-11-02：底层 SWD 同步诊断曾在请求 50/100/500 kHz 时均读出 `0x2ba01477`。暂停目标核心后的双次分块读取完成 47 对（770048 字节），SHA-256 逐对一致；第 48 块读取中断，之后三档速度均收到无效应答 `ACK=7`。USB 状态显示挂起次数为 0，USB 自检仍通过，同固件重刷探针未恢复 SWD。供电/接线/信号与协议问题尚待区分，不能将首轮只读成功视为完整烧录验收通过。
- 随后用户两板断电并重新插接上电，SWD 恢复；补读后 64 对数据全部一致，已生成完整 1 MB 备份。又以请求 500 kHz 连续读取完整 1 MB，SHA-256 与备份完全相同。之后连续三次带调试日志的重连成功，GDB 也已连接、读取寄存器及向量表、退出恢复运行。早先的单次重连曾在完整打印所有数据后返回非零，后续三次均返回 0；保留日志继续观察，不认定历史断线根因已被唯一定位。
- 用户授权后，已通过 RP2350 将 `target_demo/pb2_blink.cpp` 烧入 STM32：PB2 亮灭各约 200 ms，OpenOCD 校验通过，调试结束后恢复运行。原始 1 MB Flash 备份保留。
- 带符号 ELF 的硬件断点、汇编/源码单步、函数返回、变量与寄存器查看已实测通过；JTAG、PIO 加速和 MCP 工具服务尚待实现。
- 同一 484 字节 demo 的计时复测：OpenOCD 完整流程 3.971 秒，擦写 2.913 秒，校验 0.390 秒；不含编译与备份。详见 [烧录与调试报告](reports/STM32F407_PB2_烧录与调试报告_2026-11-02.md)。
- 后续提速已完成：拆分扇区擦除与精确长度 BIN 写入，请求 1 MHz 时完整流程两次为 0.893 / 0.895 秒。默认 PB2 烧录脚本已更新，完整镜像回读散列匹配、PB2 运行检查通过。

## 逻辑分析仪第一阶段

已实现 GP8–GP15 的 8 通道 PIO/DMA 定长采集、256-KiB 缓冲、独立 WinUSB 协议及 C++17 `logic_cli`，可导出 BIN、JSON、VCD、边沿 CSV。立即采集、取消、触发等待超时已验证；边沿触发代码已完成，真实信号尚待接线验证。采集与 OpenOCD 只读/GDB 调试、上传与 DAP 自检均通过单次并发检查；并发烧录出现过整板重枚举，根因待查，不能标记长期稳定。

最新RT1064实测补充：10 kS/s、100 kS/s、1 MS/s、10 MS/s已采集已知八路信号；16项上升/下降沿触发通过。21份正式采集的CRC、样点数、flags、逐通道频率/半周期及计数序列检查通过。10 MS/s使用最高25-kHz方波验证采样路径，不代表10-MHz输入带宽。历史并发烧录问题未在本次测试中解决。

用户要求先完成工程，稍后才接 PB2→GP8，因此当前采集文件不作为已知外部波形的验证结果。接线后执行 `tools/capture-pb2.ps1`。

- [使用与协议说明](docs/logic-analyzer-usage.md)
- [第一阶段实机记录与限制](docs/logic-analyzer-bringup-2026-11-02.md)

## 构建

在项目根目录打开 PowerShell：

```powershell
.\tools\build.ps1
.\tools\build-host.ps1
.\tools\build-target-test.ps1
.\tools\build-logic-host.ps1
.\tools\build-logic-web.ps1
```

产物：`artifacts/ai_debugprobe.uf2`、`ai_debugprobe.elf`、`probe_cli.exe`、`logic_cli.exe`、`target-test/stm32f407-selftest.elf`。

交叉工具链、CMake、Ninja、SDK、OpenOCD 均在 G 盘项目内。原生 Windows C++ 编译器默认使用现有 G 盘 CLion 内的 MinGW，可通过 `tools/build-host.ps1 -Compiler ...` 指定。构建脚本将当前进程临时目录指向项目 `.tmp`。

源码锁定记录见 `third_party/versions.json`。SDK 为 2.2.0；CMSIS-DAP 为固定提交，TinyUSB 与 SDK 锁定版本一致。外部源码通过存档展开，保留各自许可证。不是完整 Git 子模块检出。

## 主机工具与只读验证

```powershell
.\artifacts\probe_cli.exe info
.\artifacts\probe_cli.exe selftest 0C36968AD31958CA
.\artifacts\probe_cli.exe status 0C36968AD31958CA
.\tools\inspect.ps1
.\tools\openocd\openocd.exe -s tools/openocd/scripts -s . -f configs/stm32f407-readonly.cfg
```

`probe_cli` 只枚举本项目 GUID 的设备；`selftest` 校验错误请求拒绝和 100 次 USB 序列号查询，不操作目标 Flash。只读 OpenOCD 配置读取核心/器件 ID、容量、UID 与前 64 字节 Flash，不主动暂停、复位或写入目标 Flash。调试连接本身仍包含必要的 DAP 控制寄存器访问。

`status` 通过维护请求读取探针 GPIO 输入电平、SWD 输出使能、USB 挂起计数、包计数和最近应答，不是目标电压测量。`inspect.ps1` 先执行三档 SWD 同步/DPIDR 诊断，再运行目标只读配置；任一检查失败即返回错误。

当前实际 SWD 频率未用仪器测量；配置值是请求频率。GPIO 基准实现支持请求 50 kHz 至 1 MHz，首次验证使用 100 kHz。目标脚本可能覆盖速度，因此配置必须在 `source target/stm32f4x.cfg` **之后**重新设置 `adapter speed`。

## RP2350 更新

```powershell
.\artifacts\probe_cli.exe bootloader 0C36968AD31958CA
# 等到 BOOTSEL 设备出现后：
.\tools\picotool\picotool\picotool.exe load -v -x artifacts/ai_debugprobe.uf2
```

也可用 BOOT+RESET 进入 BOOTSEL。维护串口设置为 1200 baud 同样会触发 BOOTSEL；普通串口工具不要无意设置为 1200 baud。

## STM32 PB2 闪灯与源码调试：已验证

`target_demo/pb2_blink.cpp` 为 484 字节 C++17 裸机镜像，使用 HSI 标称 16 MHz 与 SysTick，让 PB2 每 200 ms 翻转一次。链接地址为 `0x08000000`，擦除第 0 扇区（16 KB），替换原启动程序。用户已授权该 demo 烧录并保留运行。

```powershell
.\tools\program-pb2-demo.ps1
.\tools\check-gdb.ps1 -GdbScript configs/stm32f407-pb2-debug.gdb -ResultLog artifacts/pb2-demo/gdb-debug.log
```

烧录脚本先核对原始备份散列，再构建、检查 SWD、执行 OpenOCD 下载校验及复位。GDB 检查使用与目标一致的 ELF：`led_toggle()` 的硬件断点命中，`stepi`、`next`、`finish` 成功，`toggle_count` 从 81 增至 82，`led_level` 从 1 变为 0。退出恢复运行后，GPIOB ODR 与 IDR 均再次出现高低交替。未用仪器测量精确周期；普通变量查看按暂停调试验证。

完整原程序备份由 64 对 16-KB 块逐对比较，并与第三次完整读取比对一致，位于 `artifacts/backups/stm32f407-original-30001d-32334709-32313836.bin`，SHA-256 为 `2BE4A842C57AFF5F86A434BC02DB66D1B2DC3BD7BFB6D940C92F13F1359D0BDB`。早期的 `incomplete-first-attempt.bin` 只是局部数据，不能用于完整恢复。备份期间的临时 halt 和历史断线记录见 `docs/bringup-2026-11-02.md`。

`tools/measure-pb2-program.ps1` 可再次下载同一镜像并生成 `artifacts/pb2-demo/timing.json`。本次测量和完整日志见 [报告](reports/STM32F407_PB2_烧录与调试报告_2026-11-02.md)。

默认 `tools/program-pb2-demo.ps1` 已使用 `configs/stm32f407-program-pb2-fast.cfg`，将第 0 扇区擦除与 484 字节 BIN 写入分开，避免原流程传输 16 KB 的填充值，默认请求 1 MHz；可传 `-SpeedKhz 100` 或 `500`。计时脚本 `tools/benchmark-pb2-fast.ps1 -SpeedKhz 1000` 已实测完整流程约 0.89 秒，保留校验与复位。原 `tools/measure-pb2-program.ps1` 保留用于旧流程基线比较。历史 SWD 无应答和一次 500-kHz 进程非零退出仍保留在报告中，长期稳定性尚待验证。

早期 `target_test/main.cpp` 为未下载的 204 字节 RAM 心跳测试镜像；`tools/program-selftest.ps1` 未执行。`tools/restore-sector0.ps1` 可用于后续恢复原启动扇区，本轮未恢复，当前保持 PB2 demo。

## 设计与资料

- [架构提案](docs/architecture.md)
- [同一块 RP2350 同时运行下载器与逻辑分析仪的规划](docs/logic-analyzer-architecture.md)
- [STM32F407 接线](docs/wiring-stm32f407.md)
- [官方资料索引](datasheet/README.md)

历史 `main.py` 与 MicroPython 固件保留作前期记录，不再用于新增固件功能。原始 RP2350 完整固件备份仍保存在 `datasheet/05_blink_and_backup`。项目测试日志与目标固件备份默认不进入 Git。
