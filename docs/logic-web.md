# 八通道实时逻辑分析仪网页

当前固件 **0.3.0 / 300 MHz**，USB 与 CMSIS-DAP 接口继续保留。开发与数据目录均为 `G:\material\week_prj\ai_debuginstructment`。

## 启动

在项目目录运行：

```powershell
.\tools\start-logic-web.ps1
```

打开 <http://127.0.0.1:8765/>。脚本会在后台启动 C++17 本地服务，再打开浏览器；服务已运行时复用。`-NoBrowser` 仅启动服务，`-Port 8766` 改端口。不要直接双击 HTML，因为网页需要本地 USB 服务。前端无 CDN、无 npm 依赖，原生 WinUSB 服务无需 Python。

重新构建：

```powershell
.\tools\build-logic-web.ps1
.\tools\build.ps1 -ClockMHz 300
```

前者同时运行原生压缩和协议解析测试。正在运行的 `artifacts/logic_web.exe` 会锁住产物；重建服务前先停止该进程。固件更新方法仍见 `logic-analyzer-usage.md`，UF2 为 `artifacts/ai_debugprobe.uf2`。

## 操作

1. 确认右上角显示“RP2350 已连接”。CH0–CH7 对应 GP8–GP15，当前目标依次为 B2/B13/B12/B17/B19/B21/B23/B25。
2. 默认选择 **10 MS/s + 自动无损压缩**。选择连续或定长、立即或边沿触发、可选自动停止，然后“开始采集”。连续模式没有预设总样点上限。
3. 网页显示八路实时波形、实际 PIO 采样率、USB 流字节速率、累计样点、设备环形缓冲占用。每完成 4096 个八位采样点即入队；小传输块最多合并 2 ms，USB 传输与 PIO/DMA 采样重叠。
4. 滚轮或＋/－缩放，拖动平移，“跟随”返回最新数据，“适应全部”显示网页保留的历史。单击设置 A 游标，Shift＋单击设置 B，读取 Δt、1/Δt。每路可隐藏；悬停电平数字可看最近两次上升沿估算的频率。
5. “暂停显示”只暂停绘图，接收与磁盘记录继续；停止按钮等待完整结束帧。停止时不保留最后不足 4096 点的 DMA 尾块，尾部截断小于 4096 点，已发布块保持连续。
6. VCD、CSV 导出当前可视时间窗，时间从窗口起点归零；完整 LAS 下载保存该次全部接收帧。左侧可打开本机历史、导入 LAS，或导入 BIN 与可选原有 JSON。无 JSON 的 BIN 使用当前选择的采样率，并显示提示。

完整流记录在 `artifacts/web-captures/<毫秒时间戳>.las`，旁边 JSON 保存样点总数、传输量、flags、最大缓冲占用和错误。LAS 是本项目流帧容器，不是通用逻辑分析文件标准。浏览器最多保存最近 30 秒且不超过 1,048,576 次八位电平变化；更早数据仍在完整磁盘记录中。缩小波形时显示像素内高低电平范围，不删除窄脉冲，不做毛刺过滤。

## 带宽与当前实测

RP2350 原生 USB 为 Full Speed 12 Mb/s，时钟超频不改变链路速率。8 通道每时刻合为 1 字节，因此 10 MS/s 原始数据为 **10 MB/s / 80 Mb/s**，无法经原生 USB 持续原样传输。[Raspberry Pi 官方规格](https://www.raspberrypi.com/products/rp2350/)、[RP2350 数据手册](https://pip-assets.raspberrypi.com/categories/1214-rp2350/documents/RP-008373-DS-2-rp2350-datasheet.pdf?disposition=inline)。

本机当前原始流约 0.8 MB/s，750 kS/s 连续测试通过，1 MS/s 会填满缓冲。默认压缩对重复八位电平编码，不改变样点值、间隔或数量；无法压缩的块自动回退原始字节。**10 MS/s 持续时间取决于输入变化率**，不保证任意八路信号都能一直传输。

RT1064 CH0=25 kHz、其余逐路二分时，10 MS/s 实测收到 99,999,744 点、约 10 秒、3,664,516 流字节，flags=0，最大环形占用 4096 点。详见 [实测报告](../reports/逻辑分析仪网页与USB流采样_2026-11-02.md)。CRC/序号检查通过只说明传输和分块连续；此前存在的输入毛刺、触发首点异常、密集 SWD No ACK 不能据此宣布解决。

## 程序结构

- `firmware/logic_analyzer.cpp`：Core 1 管理 PIO 和双 DMA；Core 0 压缩、分块发送，继续处理 DAP。
- `firmware/stream_codec.hpp`：无分配的每块无损游程编码，收益不足即回退 RAW。
- `host/logic_usb.hpp`：独立 WinUSB 接口，8 个 16 KiB OVERLAPPED 读取，RAW_IO 绕过软件串行队列。[Microsoft 的队列与 RAW_IO 说明](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/winusb-functions-for-pipe-policy-modification)。
- `host/stream_parser.hpp`：连续偏移、帧序号、会话、编码长度、CRC-32 检查。
- `host/logic_web.cpp`：仅监听本机的 HTTP 服务，流式写盘并转发到浏览器；下载大文件分块读取。
- `web/`：原生 ES Module、Canvas 波形、有限容量电平历史，无第三方前端包。

`POST /api/stream` 参数：`rate`=10000–10000000，`samples`=0 或 4096 的倍数（单次最多 128 Mi 点），`encoding`=0 原始/1 自动无损，`trigger`=0 立即/1 上升/2 下降，`channel`=0–7，`timeout`=100–30000 ms，`seconds`=0 不限/1–3600 自动停止。`POST /api/stop` 请求完整停止；GET `/api/state`、`/api/devices`、`/api/records` 与 `/api/record?id=...` 提供状态与记录。服务绑定 127.0.0.1，限制 Host/Origin，不允许跨站控制设备。

## 协议扩展

保留原协议 1 的 Info/Arm/Status/Fetch/Cancel 和老 CLI；新增 StreamStart=6、StreamStop=7。Start 的 64 字节 Request 中 `reserved[0]` 指定编码，普通 Reply 后 USB IN 变为主动连续流，Stop 不另回普通 Reply，而是在已有数据后发 End。能力位新增 0x08 流、0x10 RLE，Info 合计 0x1f。

每帧 64 字节小端头，16 个 uint32 依次为：magic=0x32534c41、流协议=1、kind（1 Data / 2 End）、session、sequence、flags、clk_sys、PIO divider256、samples、first_sample低/高32位、payload_length、payload_crc32、ring_used、encoding、requested_rate。实际采样率为 `clk_sys * 128 / divider256`。

Data 固定含 4096 点。RAW payload=4096 字节；RLE 每个 uint32 的低 8 位为值、高 24 位为重复次数，计数之和必须为 4096。End 的 samples=0、payload=4 字节零，以独立短 USB 包使排队读及时完成。USB 最大包仍 **64 字节**；4160 字节只是 TinyUSB 单次传输缓冲。

设备环形缓冲 256 KiB / 64 块。跨核只有短临界区，块在复制到发送暂存区后释放。DMA 使用 4 KiB 地址环限制出界；双完成未及时服务或 RXSTALL 置 flags1，DMA 错误置2，等待触发超时置4，生产追上消费置8。异常立即停止采样、排空已完成块、发送带标志的 End，网页标红并保留数据；没有静默丢样或插值补点。USB 拥塞不能通过停顿 PIO 后继续而伪装等间隔采样。

## 验证工具

```powershell
node tests/web_stream_test.mjs
node tools/benchmark-stream.mjs
# 已安装 Playwright 时，可通过 NODE_PATH 指向现有模块目录；使用现有 Edge。
node tools/test-logic-web.cjs
```

浏览器测试使用真实 USB 设备与已保存的真实历史波形；截图、下载与结果写 `artifacts/streaming/`。它会执行采样、停止、原始流溢出与恢复，但不擦写 RT1064。测试脚本把 TEMP/TMP 指向项目 `.tmp` 后运行，以避免项目临时文件落在 C 盘。
