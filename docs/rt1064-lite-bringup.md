# RT1064 Lite 核心板接入记录

日期：2026-11-02（America/Denver）。板上照片与逐飞科技官方产品资料页表明目标为 RT1064 Lite V1.0，芯片丝印为 NXP MIMXRT1064DVL6B。新目标接入采用 SWD。

## 用户提供接线

| RP2350B-Plus-W | 目标板信号/网络 | 用途 |
| --- | --- | --- |
| GP0 | RX | 目标 UART RX |
| GP1 | TX | 目标 UART TX |
| GP2 | CLK | SWCLK |
| GP3 | DIO | SWDIO |
| GP4 | RST | 目标复位 |
| GP8 | B2 | 逻辑分析 CH0 |
| GP9 | B13 | 逻辑分析 CH1 |
| GP10 | B12 | 逻辑分析 CH2 |
| GP11 | B17 | 逻辑分析 CH3 |
| GP12 | B19 | 逻辑分析 CH4 |
| GP13 | B21 | 逻辑分析 CH5 |
| GP14 | B23 | 逻辑分析 CH6 |
| GP15 | B25 | 逻辑分析 CH7 |
| GND | GND | SWD / 逻辑分析共地（用户接线描述未列出，需确保共地） |

GP0/GP1 在当前 RP2350 固件中不是 UART 桥接；只能视作接线预留。逻辑分析器固件把 GP8–GP15 配成无上下拉输入，不会主动驱动这些目标信号。SWD 的 GP4 在 CMSIS-DAP 复位适配中尚未实现物理 RST 驱动；本次身份检查没有使用硬件复位。

## SWD 只读检测

2026-11-02 通过 RP2350 CMSIS-DAP v2 / OpenOCD 通用 Cortex-M 配置只读连接成功：

- SWD DPIDR：`0x0bd11477`
- OpenOCD：Cortex-M7 r1p1，目标检查成功
- CPUID：`0x411fc271`，Cortex-M7 r1p1
- APIDR：`0x000000c7`
- 检测时 PC：`0x08004fc0`，MSP：`0x2006fde0`
- 目标在只读检查后恢复运行；没有执行 Flash erase/write、解锁或选项字节操作。

证据日志：[OpenOCD 只读检查](../artifacts/captures/rt1064-readonly.log)。CPUID/DPIDR 证明调试链路和 Cortex-M7 能访问；目标型号还依据照片芯片丝印与官方产品资料判断。

## LED 网络名须谨慎区分

用户确认 LED 接在板上标为 E9 的信号焊盘，不是芯片封装球位 E9。逐飞公开的 [RT1064 Lite 核心板资料页](https://www.seekfree.com.cn/%E4%BA%A7%E5%93%81%E8%B5%84%E6%96%99/%E6%A0%B8%E5%BF%83%E6%9D%BF/%E6%81%A9%E6%99%BA%E6%B5%A6%E7%B3%BB%E5%88%97/rt1064-lite%E6%A0%B8%E5%BF%83%E6%9D%BF/)及随附开源库用于核对：

- 核心板 GPIO demo 把板载 LED 定义为 B9，运行 `gpio_init(B9, GPO, 0, ...)` 并翻转 B9。
- SDK `board.h` 把板载用户 LED 定义为 GPIO1、pin 9，即 GPIO1_IO09。
- 逐飞核心板丝印与芯片引脚对应表将板级 E9 映射为 `GPIO_EMC_09`，并注明 E 组被 SDRAM 占用且未引出；NXP IOMUX 表中 `GPIO_EMC_09` 的 ALT5 功能为 `GPIO4_IO09`，芯片球位为 C2。
- NXP MIMXRT1064DVL6B LFBGA196 封装的球位 E9 则是 `NVCC_GPIO` 电源。板级 E9、GPIO_EMC_09 与芯片封装球位 E9 是不同标记，不能混为一谈。

用户确认所接为板级 E9 信号焊盘后，按逐飞命名表配置 `GPIO_EMC_09` / `GPIO4_IO09`。该 pad 的 ALT0 是 SEMC_ADDR00；此次 demo 无 SDRAM DCD、无 SDRAM 数据/向量/栈，避免 SDRAM 占用。实机读到原 mux=0，demo 后 mux=0x15（ALT5+SION）、GPIO4 GDIR bit9=1，PSR 与 DR 高低交替一致；程序正常从 0x70002xxx 运行。板级具体焊接点仍依据用户描述，未通过照片目视确认外接 LED。

## Flash 下载限制

现有 RP2350 探针可在 SWD 上访问 Cortex-M7，但项目绑定的 OpenOCD 脚本中没有 MIMXRT1064 Flash bank/编程算法。RT1064 集成 FlexSPI2 QSPI Flash，启动配置和 boot header 与 STM32F4 完全不同；原 STM32F407 的 ELF、链接地址及 `flash erase_sector` 配置不能用于本目标。

NXP [AN12290](https://www.nxp.com/docs/en/application-note/AN12290.pdf) 说明 RT1064 使用 FlexSPI2 连接片内 4-MB QSPI Flash，启动镜像/链接地址体系不同于 RT1060。NXP [AN12183](https://www.nxp.com/docs/en/nxp/application-notes/AN12183.pdf) 提到 MCUXpresso 的 RT1064 FlexSPI NOR flash driver。两份资料证明需要专用目标算法，但不代表当前 OpenOCD 构建已集成该算法。

2026-11-02 再次运行只读检查，探针序列号 `0C36968AD31958CA` 仍可读到 DPIDR `0x0bd11477`、CPUID `0x411fc271` 和 APIDR `0x000000c7`；核心检查后已恢复运行。本次未写入目标。

下载器路径复核：pyOCD 0.46.1 包含 MIMXRT1064 算法（0x70000000、4 MiB、64-KiB 擦除块、256-byte 页）。旧探针拒绝其默认 wait_retry=150；现已将包校验上限从100改为150，边界/100000包模糊测试通过，v0.2.1 已构建、刷入并校验。原 v0.2.0 UF2 回退副本仍保留。

最终已完成 Flash 双读备份、专用算法初始化、启动块擦写、系统复位后独立完整回读校验及运行检查。E9 demo 高低各200 ms，目前保持运行；最终全流程6.908秒。详见 [烧录报告](../reports/RT1064_E9_烧录报告_2026-11-02.md)。
