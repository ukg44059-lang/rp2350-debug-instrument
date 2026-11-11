# RP2350B-Plus-W 资料索引

整理日期：2026-11-01（America/Denver）。板卡：Waveshare RP2350B-Plus-W，RP2350B / 16 MB NOR Flash。

## 建议阅读顺序

1. [开发板官方中文说明（离线网页，含本地图片）](02_board/Waveshare_Wiki_zh.html)：引脚、尺寸、MicroPython、C/C++、Arduino 上手说明。
2. [开发板原理图](02_board/RP2350B-Plus-W_Schematic.pdf)：本板实际接线的依据。
3. [RP2350 芯片完整手册](01_chip/RP2350_Datasheet.pdf)：1371 页，外设、寄存器、启动、存储器、电气参数。
4. [RP2350 硬件设计指南](01_chip/Hardware_Design_with_RP2350.pdf)：25 页，供电、晶振、USB、Flash/PSRAM、参考设计。

## 文件分类

| 目录 | 内容 |
| --- | --- |
| `01_chip` | RP2350 芯片手册、硬件设计指南、官方 RP2350B 最小系统 KiCad 参考设计 ZIP |
| `02_board` | 本板原理图、引脚图、尺寸图、原理图预览、官方中文 Wiki 离线网页和纯文本，以及网页图片 |
| `03_examples` | Waveshare 官方示例 ZIP、已解压的 C/Python 示例、专用 UF2 固件、Arduino RP2350B 板级支持包 |
| `04_reference` | Raspberry Pi Pico 2 W 手册、Pico 2 入门、C SDK、Python SDK 参考文档 |
| `05_blink_and_backup` | 本次 200 ms 点灯程序、连接与验证记录、原始完整 Flash 备份、恢复说明 |

`04_reference` 中的 Pico 文档用于 SDK 和通用开发参考。Pico 2 / Pico 2 W 的引脚、供电和外围接线与这块 Waveshare 板有区别，接线请以 `02_board` 为准。下载的 PDF 保留官方原文件；各文档的版本以正文为准，下载日期不代表发布日期。

## LED 与本次程序

以下为 2026-11-01 的历史 MicroPython 点灯记录。当前已改为 C++ 下载调试器 + 逻辑分析仪一体化固件 0.2.0，最新设备与接口状态见 [项目 README](../README.md)。本节旧固件和端口号仅供历史恢复参考。

- LED2：MCU **GPIO23**，高电平点亮；本次控制的就是它。
- LED1：无线模块 **WL_GPIO0**，厂商 MicroPython 示例用 `Pin("LED")` 控制。
- 本次程序：亮 **0.2 秒**、灭 **0.2 秒**，完整周期 **0.4 秒（2.5 Hz）**。
- 板内已保存 `main.py`，重启后自动执行。
- 当前串口：**COM38**，重新插拔后端口号可能变化。
- 当前固件：官方示例包内 `WAVESHARE_RP2350B_PLUS_W.uf2`，版本 `v1.29.0-preview.349.gaf38ee165f.dirty`，板级配置 `waveshare_rp2350b_plus_w`。这是厂商提供的预览构建，点灯已验证，其他外设尚未验证。
- `05_blink_and_backup/RPI_PICO2_W_MicroPython_v1.29.0.uf2` 是初次调试使用的通用参考固件，当前板内运行的是上述 Waveshare 专用固件。后续重新刷机优先使用专用固件。

## 官方来源

- [Waveshare 中文开发板 Wiki](https://www.waveshare.net/wiki/RP2350B-Plus-W)
- [Waveshare 英文开发板 Wiki](https://www.waveshare.com/wiki/RP2350B-Plus-W)
- [开发板原理图](https://files.waveshare.com/wiki/RP2350B-Plus-W/RP2350B-Plus-W.pdf)
- [开发板官方示例包](https://www.waveshare.net/w/upload/3/3e/RP2350B-Plus-W.zip)
- [Raspberry Pi RP2350 官方资料入口](https://pip.raspberrypi.com/categories/1214-rp2350)
- [Raspberry Pi Pico 2 W 官方资料入口](https://pip.raspberrypi.com/categories/1088-raspberry-pi-pico-2-w)
- [Pico SDK 官方仓库](https://github.com/raspberrypi/pico-sdk)
- [Pico Examples 官方仓库](https://github.com/raspberrypi/pico-examples)
- [Arduino-Pico 官方项目文档](https://arduino-pico.readthedocs.io/)
- [MicroPython RP2 官方文档](https://docs.micropython.org/en/latest/rp2/quickref.html)

`sources.json` 记录下载 URL、文件大小、SHA-256、PDF 页数、ZIP 校验结果。`checksums.sha256` 覆盖整理后的本地文件。所有 7 份 PDF 均已成功解析，所有下载的 ZIP 均已通过 CRC 检查。离线 Wiki 的图像已下载，外部视频和附件链接仍需联网。

## RT1064 目标板

已整理逐飞 RT1064 Lite 使用说明、原理图、NXP 数据/参考手册及引脚命名表，见 [RT1064资料索引](06_rt1064/README.md)。

