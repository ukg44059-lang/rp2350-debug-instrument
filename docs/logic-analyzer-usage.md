# 一体化逻辑分析仪使用与协议

版本：固件 0.3.0，保留定长协议1，新增流协议1，当前主频300 MHz。实时网页运行 `tools/start-logic-web.ps1`，打开 <http://127.0.0.1:8765/>，见[网页与流采样指南](logic-web.md)。此前0.2.2的300 MHz回归中，18轮传输检查全部通过但7轮严格波形检查失败（毛刺/一次触发首样点异常），本次尚未重复同等输入稳定性验收。详见[超频报告](../reports/RP2350_300MHz_10MSps_实测报告_2026-11-02.md)。精密触发延迟和输入带宽尚未标定。

## 300 MHz 与 10 MS/s

```powershell
# 默认构建300 MHz；回退构建使用 -ClockMHz 150
.\tools\build.ps1 -ClockMHz 300
# 立即采集8路，10 MS/s，262144点，输出BIN/JSON/VCD/CSV
.\tools\capture-10m.ps1
# CH0上升沿触发
.\tools\capture-10m.ps1 -Trigger 1 -Channel 0
# 严格回归：失败会返回错误，原始波形与日志保留
.\tools\test-overclock.ps1
```

10 MS/s意味着采样间隔100 ns，旧CLI满缓冲记录26.2144 ms、上传约0.4秒。0.3.0新增持续USB流：10MS/s无损压缩流已采集25kHz源约10秒；原始流750kS/s通过，1MS/s会溢出，不能保证任意信号10MS/s持续。300 MHz下PIO使用整数分频15，每点两条指令；150 MHz原本也支持10 MS/s。采样率由命令指定，提高主频不会自动改变采样命令。实测已知输入最高25 kHz，尚未验收10 MHz输入带宽。

固件在启动时设定核心电压1.20 V，从SRAM调整QMI分频使Flash时钟不超过75 MHz，再提高clk_sys；USB保持48 MHz，DAP的CPU_CLOCK同步更新。FC0对晶振参考检查系统时钟，不替代外部校准。原150 MHz固件备份位于 `artifacts/overclock-300/ai_debugprobe-v0.2.1-150MHz.uf2`。

## 当前RT1064实机测试

GP8–15依次连接RT1064的B2、B13、B12、B17、B19、B21、B23、B25。信号源为C++八位计数器，默认每毫秒递增，CH0–7输出500、250、125、62.5、31.25、15.625、7.8125、3.90625 Hz方波。用户反馈E9灯未闪后，当前已改为B9板载LED每200 ms翻转；E9不再配置为GPIO输出。

```powershell
# 构建并烧录信号源（会替换目标当前程序，已有用户授权）
.\tools\build-rt1064-demo.ps1 -SignalTest
.\tools\program-rt1064-demo.ps1 -SignalTest
# 默认速度、全部16种触发测试
.\tools\set-rt1064-signal-rate.ps1 -StepHz 1000
.\tools\test-rt1064-logic.ps1 -Mode baseline
.\tools\test-rt1064-logic.ps1 -Mode triggers
# 较快信号：CH0=25 kHz，依次二分
.\tools\set-rt1064-signal-rate.ps1 -StepHz 50000
.\tools\test-rt1064-logic.ps1 -Mode fast
# 恢复默认，检查采集与SWD只读并行
.\tools\test-rt1064-concurrent.ps1
```

速率参数只改目标RAM，重启恢复1 kstep/s。C++检查器`logic_signal_check.exe`验证样点数、flags、各通道频率/半周期与计数序列；USB上传CRC由`logic_cli`检查。八线多位进位时允许最多单样点、且只在旧/新电平变化位上的混合值，额外记录`mixed_bus_samples`，不修改原始数据；逐通道边沿检查仍用原始数据。报告见[RT1064实测](../reports/RT1064_逻辑分析仪实测_2026-11-02.md)。

## 当前实现

- 同一块 RP2350 同时提供 CMSIS-DAP v2、独立逻辑分析 WinUSB、CDC 维护接口。
- GP8–GP15 对应 CH0–CH7；全部为无上下拉输入，未连接的通道可能漂浮，不能据其边沿统计判断真实信号。
- 8 通道，每点 1 字节；262144 字节缓冲；样点数必须是 4 的倍数。
- 支持请求 10 kS/s–10 MS/s，报告系统时钟和实际 PIO 分频值。标称时钟尚未用外部仪器校准。
- 定长立即采集；PIO上升沿/下降沿触发已对RT1064八个通道分别通过真实信号测试。当前只有触发后数据，不含预触发缓冲。
- 可取消，有触发等待超时、DMA 错误与 RX FIFO 停顿标志。错误采集不允许按成功数据上传。
- 电脑端 C++17 CLI，导出 BIN、JSON、VCD 和边沿 CSV，上传块 CRC-32 检查。
- 0.3.0新增网页GUI与无损压缩连续USB流，详见[流协议及网页说明](logic-web.md)。模拟电压测量、协议解码、预触发、AI/MCP服务尚未实现。

## 构建

在 `G:\material\week_prj\ai_debuginstructment` 的 PowerShell 中：

```powershell
.\tools\build.ps1
.\tools\build-host.ps1
.\tools\build-logic-host.ps1
```

产物为 `artifacts/ai_debugprobe.uf2`、`probe_cli.exe` 和 `logic_cli.exe`。原下载器 0.1.0 固件保存在 `artifacts/probe-v0.1.0/`；如回退该固件，OpenOCD 的 `configs/probe-rp2350.cfg` PID 也应回到 `4015`。

当前一体化实验标识为 VID:PID `CAFE:4016`，序列号 `0C36968AD31958CA`；它是实验室原型标识，不是量产分配。更新 PID 避免 Windows 缓存旧接口布局。CMSIS-DAP 接口 0 沿用原 GUID；分析接口 3 使用 `{F6B88B84-3221-4B89-A912-730BD7F28AF1}`。Windows 已自动绑定两个 WinUSB 接口；当前维护串口为 COM40，端口号可能变化。

## 连接与采集

保留原 GP2→PA14、GP3→PA13、GND→GND 接线。已知测试波形增加一根 **STM32 PB2 → RP2350 GP8（CH0）**。本版本只用于已确认的 3.3-V 数字输入；无需接两板的 3.3-V 电源输出。

```powershell
.\artifacts\logic_cli.exe info
.\artifacts\logic_cli.exe selftest 0C36968AD31958CA

# 100 kS/s，262144 点，约 2.621 秒
.\artifacts\logic_cli.exe capture 0C36968AD31958CA 100000 262144 artifacts/captures/example

# CH0 上升沿触发，等待触发最多 5 秒；参数末尾分别为模式、通道、超时
.\artifacts\logic_cli.exe capture 0C36968AD31958CA 100000 262144 artifacts/captures/rising 1 0 5000

# 针对现有 PB2 200-ms 闪灯程序的一键检查，接线完成后使用
.\tools\capture-pb2.ps1
.\tools\capture-pb2.ps1 -Trigger rising
.\tools\capture-pb2.ps1 -Trigger falling
```

`capture-pb2.ps1` 不改写目标程序；它检查 CH0 至少 4 个边沿、完整边沿间隔处于 180–220 ms。触发模式额外检查首样点电平。此为功能检查，不是精密时钟校准或样点级触发延迟测量。

需要联合调试时，可分开武装和上传：

```powershell
.\artifacts\logic_cli.exe arm 0C36968AD31958CA 10000 262144
# 采集约 26.214 秒，其间可用独立终端运行现有 OpenOCD/GDB 工具
.\artifacts\logic_cli.exe status 0C36968AD31958CA
# 等 state=3、flags=0 后上传
.\artifacts\logic_cli.exe fetch 0C36968AD31958CA artifacts/captures/debug-session
# 或取消尚未完成的采集
.\artifacts\logic_cli.exe cancel 0C36968AD31958CA
```

同一逻辑接口每次只运行一个主机客户端；CMSIS-DAP 的 OpenOCD 可以独立打开其接口。当前并发烧录尚存在整板重枚举的待查记录，不能把只读/GDB 并发成功推定为全部并发场景已稳定。

## 文件格式

| 文件 | 内容 |
| --- | --- |
| `.bin` | 按时间先后排列的原始样点，一点一字节，bit0=GP8、bit7=GP15 |
| `.json` | 序列号、会话、采样率、时钟/分频、深度、触发位置、完整性、上传时间与通道边沿统计 |
| `.vcd` | 8 通道波形，1-ns 时间刻度；时间戳由实际分频率换算并四舍五入，不表示 1-ns 测量精度 |
| `.edges.csv` | 电平字节变化时的样点索引、秒数和完整 8-bit 电平；不是每样点一行 |

主机先收齐所有块并校验，再输出文件。未连接通道的漂浮变化会同时进入原始数据和统计；后续解码只能选择实际连接的信号。

## USB 协议 1

所有字段为 little-endian uint32，完整定义见 `firmware/logic_protocol.hpp`。每个请求 64 字节，每个响应头 64 字节。每条命令必须先完整读取响应和声明的负载，再发下一条命令。

请求字序：`magic, operation, session, rate, samples, trigger, channel, timeout_ms, reserved[8]`。Magic 为 `0x31414c41`。触发为 0=立即、1=上升沿、2=下降沿，通道为 0–7。

响应字序：`magic, operation, error, state, session, requested_rate, system_clock, divider256, samples, flags, trigger_index, payload_length, payload_crc, offset, capabilities, protocol`。

- 操作：1 Info、2 Arm、3 Status、4 Fetch、5 Cancel。
- 状态：0 Idle、1 Armed、2 Capturing、3 Complete、4 Cancelled、5 Failed。
- 错误：0 OK、1 Invalid、2 Busy、3 NotReady、4 Stale。
- 标志：bit0 RX FIFO 停顿、bit1 DMA 错误、bit2 超时；正常为 0。
- Arm 必须指定非零 session，采集期间不允许重新武装。接收成功只表示命令已排队，随后 Status 获取最新硬件状态。
- Fetch 将请求 `rate/samples` 复用为字节偏移/长度，单块 1–4096 字节，仅允许读取相同 session 的 Complete 数据。响应头后紧随 `payload_length` 个原始字节，CRC 为标准 IEEE CRC-32。
- 实际采样率：`system_clock × 128 / divider256`。每样点执行 `IN PINS,8` 和 `JMP X--` 两条 PIO 指令；四点自动打包成一个 32-bit FIFO / DMA 字。
- 固件使用有限 PIO 循环，完成后不继续采样，避免把采集终止后的 FIFO 堵塞误判为中途丢样。
- 当前触发位置索引定义为采集数据的第 0 点；它是首个触发后样点，不承诺零检测延迟或采集前历史。

固件在 Core 0 上每轮最多发送一个分析 USB 包，并处理独立 DAP 请求。Core 1 管理采样；所有 DMA/PIO 操作不在 USB 中断中执行，跨核短临界区只管理命令和状态。

## 当前验收与待办

实机结果见 [第一阶段开发记录](logic-analyzer-bringup-2026-11-02.md)。下一步在接线后验证 PB2、定时器已知频率和各通道接线，然后实现并验收预触发环形缓冲、协议解码与 AI 会话封装。
