# STM32F407 首轮接线：SWD

日期：2026-11-01（America/Denver）。这是本项目选定的原型 GPIO 分配，后续固件必须与之保持一致；不是 RP2350 板卡出厂默认探针功能。更新：已刷入本项目 C++ 探针固件，通过三线 SWD 完成首轮目标只读识别；持续读取稳定性仍在验证。

## SWD 接线

先断开两块板子的供电和其他下载器，再接线。

| RP2350B-Plus-W | 40 针位置 | STM32F407 端 | 作用 |
| --- | --- | --- | --- |
| GP2 | 4 | PA14 / SWCLK / TCK | 探针输出时钟 |
| GP3 | 5 | PA13 / SWDIO / TMS | 双向 SWD 数据 |
| GP4 | 6 | NRST / nRESET | 系统复位，固件采用拉低/高阻释放；建议连接 |
| GND | 3（或其他 GND） | GND | 共地 |

RP2350 40 针位置以厂商引脚图为准：元件正面朝向自己、USB-C 在上时，左列从上往下为位置 1（GP0）、2（GP1）、3（GND）、4（GP2）、5（GP3）、6（GP4）。GP2 不等于排针第 2 脚。

STM32 开发板型号尚未确定，因此只指定芯片端口和信号名，不猜测其 SWD/JTAG 连接器针序。以开发板丝印、原理图或其引脚表找到 PA13、PA14 和 NRST；不要按另一块板子的排针位置照接。SWDIO 不是串口 TX/RX。

## 供电与准备

- RP2350 使用自己的 USB-C 接电脑；STM32 使用开发板规定的 USB 或电源输入独立供电。
- 两板之间先只连接上述四根线，不连接 3V3、5V、VBUS 或 VSYS。目标连接器上的 VTref/3V3 先留空，本原型尚无 VTref 检测前端。
- 确认目标 SWD 逻辑电平为 3.3 V。开发板允许 5 V 电源输入不代表调试信号是 5 V。
- BOOT0 保持正常从 Flash 启动的低电平配置，无需切换串口 ISP 启动。
- 外接 ST-Link、J-Link、DAP 或 XDS110 从目标调试信号上断开；若板载 ST-Link 仍连接目标，按具体板卡手册断开对应跳线，不能只假定拔 USB 就完成隔离。
- 线尽量短，原型建议约 10 cm；首次连接从低调试时钟起步。
- 两板接好后同时供电。避免一板断电、另一板持续驱动信号；结束时先停调试并断电再改线。
- 用户实接仅 GP2、GP3、GND，NRST 未接。固件按无 NRST 配置，禁止硬件 connect-under-reset；必要时使用目标板的手动复位按钮。当前烧录功能尚未通过完整验收。

## 后续 JTAG 预留

此表定义后续固件方向；第一轮 SWD 验证不需要连接这些额外信号。

| RP2350 | 40 针位置 | STM32F407 | 功能 |
| --- | --- | --- | --- |
| GP2 | 4 | PA14 / JTCK | TCK，与 SWCLK 复用 |
| GP3 | 5 | PA13 / JTMS | TMS，与 SWDIO 复用 |
| GP5 | 7 | PA15 / JTDI | TDI，探针到目标 |
| GP6 | 9 | PB3 / JTDO | TDO，目标到探针 |
| GP7 | 10 | PB4 / NJTRST | 可选 JTAG 复位 |
| GP4 | 6 | NRST | 系统复位 |
| GND | 3 或其他 GND | GND | 共地 |

PB4/NJTRST 与 NRST 不可短接。SWD 与 JTAG 为互斥模式。PA15/PB3/PB4 的板载电路和当前程序复用情况需要在启用 JTAG 前核实。尚未验证 JTAG 传输引擎，不能把预留接线当作可用功能。

## 依据

- [ST STM32F405/407 数据手册](https://www.st.com/resource/en/datasheet/stm32f407vg.pdf)：STM32 调试引脚定义。
- [Waveshare 开发板原理图](https://files.waveshare.com/wiki/RP2350B-Plus-W/RP2350B-Plus-W.pdf)：GP2 至 GP7 的排针引出；这些引脚未接板载无线模块或 LED。
- 本地 `datasheet/02_board/Pinout.jpg`：RP2350 板卡排针位置。

用户已确定目标系列 STM32F407；完整料号、开发板型号及目标板接口针序仍待核实。
