# 逻辑分析仪第一阶段开发记录

日期：2026-11-02（America/Denver）。工程目录：`G:\material\week_prj\ai_debuginstructment`。

## 交付状态

固件 0.2.0 已编译并刷入现有 RP2350，Windows 同时自动识别 CMSIS-DAP v2 与 AI Logic Analyzer 两个 WinUSB 接口。现有下载调试器保留。用户要求先完成工程、之后再连接 PB2→GP8；本轮不把浮空输入的采集当成已知波形验收。

新增工程：

- `firmware/logic_analyzer.cpp`：Core 1 控制、PIO1 有限定长循环、DMA、256-KiB 缓冲、触发/超时/取消。
- `firmware/logic_protocol.hpp`：64-byte 请求与响应头、状态与错误、CRC 和采集限制。
- `host/logic_cli.cpp`：独立 WinUSB 客户端，采集/武装/状态/上传/取消、自检、BIN/JSON/VCD/CSV 导出。
- `tools/build-logic-host.ps1`：C++17 原生构建及协议回归测试。
- `tools/capture-pb2.ps1`：接线后执行的已知波形检查，尚未运行。
- [使用与协议说明](logic-analyzer-usage.md)。

交付 UF2 SHA-256：`3C08FAF7B0BFC27C65AB2199C42F6FDB2EB6D2548473FDEF8F651FC4E9540D51`。原 0.1.0 UF2 与 ELF 已备份到 `artifacts/probe-v0.1.0/`，原 UF2 散列仍为 `B2228F3F596BD5463A54E5C2E69AC5BA310BFF8A5CDE652EB9F0D81F62787C8B`。

## 实机与软件检查

| 检查 | 结果 | 限制 |
| --- | --- | --- |
| 固件 / 两个 C++ 主机工具构建 | 通过，最终固件构建无编译警告 | 未作所有配置组合验证 |
| CMSIS-DAP USB 自检 | 错误请求拒绝，连续 100 次序列号读取通过 | 不等于所有目标兼容 |
| 分析协议自检 | 非法采集参数与过期会话拒绝 | 不等于真实触发验收 |
| CRC 标准向量与参数边界 | 原生测试通过 | 另有每块上传 CRC 实机校验 |
| 100-kS/s 定长采集 | 262144 点、flags=0、全部上传块校验通过 | 输入未接已知波形 |
| 最终固件 1-MS/s 定长采集 | 262144 点、flags=0；上传 611.085 ms | 实际频率为系统时钟/分频计算值，未校准 |
| 最终固件 10-MS/s 定长采集 | 262144 点、flags=0；上传 625.518 ms | 未验证外部高速边沿精度 |
| 取消 | 长采集后取消，状态变为 Cancelled | 未作断电等全部异常场景 |
| 触发等待超时 | CH0 等待上升沿、100 ms 超时，返回 Failed/TimedOut | CH0 没有已知外部波形输入 |
| 采集 + OpenOCD 只读访问 | 同时识别 F407、读出 UID/向量；采集完成上传，flags=0 | 单次回归，非长期稳定性证明 |
| 采集 + GDB 源码调试 | 硬件断点、stepi、next、finish、变量与寄存器读取通过；采集完成，flags=0 | 目标暂停会影响其自身波形，本轮没有接线观察 |
| 上传 + CMSIS-DAP USB 自检 | 上传完成、CRC 一致，同时 100 次序列号查询通过 | 不等同于上传期间持续高速调试吞吐测试 |

GDB 并发时实际观察到 `toggle_count: 638 → 639`、`led_level: 0 → 1`，硬件断点位于 `0x08000130`。已删除断点并恢复目标运行。26.2144 秒的采集在调试前武装，调试结束时仍处于 Capturing，随后进入 Complete，262144 字节全部上传校验通过。

最后修正了“命令已排队时，状态查询不应返回 Busy”的竞争窗口，重新刷写后再次通过 DAP 自检、分析接口自检及完整 10-MS/s 采集上传：262144 字节、flags=0、上传 573.868 ms。见 [交付版采集元数据](../artifacts/captures/delivery-10m.json)。上述并发记录来自这一状态查询修正前的相同采样实现，不将其描述为最终版本完整并发回归。

交付固件 `size`：text 36608、data 0、bss 268308 字节。该数值包含 256-KiB 缓冲和静态对象，不是栈水位或全部运行期内存余量实测。

## 证据文件

- [最终 1-MS/s 元数据](../artifacts/captures/final-static-1m.json) / [原始样点](../artifacts/captures/final-static-1m.bin)
- [最终 10-MS/s 元数据](../artifacts/captures/final-static-10m.json) / [原始样点](../artifacts/captures/final-static-10m.bin)
- [并发只读 OpenOCD 日志](../artifacts/captures/concurrent-readonly-final.log) / [采集结果](../artifacts/captures/concurrent-readonly-final.json)
- [并发 GDB 日志](../artifacts/captures/concurrent-gdb.log) / [采集结果](../artifacts/captures/concurrent-gdb.json)
- [上传与 DAP 并发结果](../artifacts/captures/upload-with-dap.json) / [上传日志](../artifacts/captures/upload-with-dap.log)
- [取消 / 超时检查记录](../artifacts/captures/state-check.log)
- [最终固件构建日志](../artifacts/logic-build.log)

VCD/边沿 CSV 与对应 BIN、JSON 使用同一个文件名前缀。未连接的通道存在漂浮变化，其边沿统计不能用作被测电路结论。

## 修复与尚待解决的问题

首次实现把 PIO FIFO 全部合并为 RX，导致启动计数无法通过 TX FIFO 送入，采集超时。已改为保留 TX/RX 独立 FIFO；后续有限长度采集通过。Core 1 控制轮询加入短暂让出间隔，避免无意义地持续争用跨核锁。

并发验证中两次观察到会话丢失/整板重新枚举，其中一次发生在 F407 写入、校验、软件复位均记录成功之后。后续探针启动时间重新计数、采集 session 回到 0；该次读取到的 watchdog reason 为 0。不能据此唯一判定为看门狗、供电、外部复位、USB 或固件问题。后续只读并发、GDB 并发、上传并发已通过，但**并发烧录不能标记为稳定验收通过**。失败场景日志保留在 `artifacts/captures/concurrent-program.log`，当次没有获得有效采样结果。

当前 CLI 会拒绝会话号改变、错误状态、CRC 不符或不完整数据，避免把上述异常返回为成功波形。新增维护状态显示启动时间及 watchdog reason，供后续定位。

## 下一步

1. 用户接 PB2→GP8 后，用现有闪灯程序验证通道映射、样点时间间隔与边沿触发。
2. 使用 F407 定时器及已知 UART/SPI/I²C 数据验证更快波形和逐字节内容。
3. 定位整板重枚举问题，完成并发烧录、调试、上传的重复回归。
4. 再实现环形预触发、协议解码、波形前端和 AI/MCP 会话封装。

第一阶段已提供可构建、可刷写、可采集、可导出的基础工程；真实信号、采样绝对精度、触发精度及完整并发稳定性仍待上述验收。
