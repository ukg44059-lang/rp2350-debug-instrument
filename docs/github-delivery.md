# GitHub 工程交付

账号：`ukg44059-lang`；仓库：`rp2350-debug-instrument`；初始可见性为私有。首次整理日期：2026-11-10。

项目是一块 RP2350 同时实现 CMSIS-DAP SWD 下载调试器和八通道逻辑分析仪。当前固件版本 0.3.0，网页与流采样指南见 [logic-web.md](logic-web.md)。输入毛刺、SWD 压力稳定性等限制保留在报告中；上传 GitHub 不代表这些限制已解决。

## 仓库内容

包含 firmware、host、web、target_demo、target_test、configs、tests、tools 中的项目脚本、datasheet 官方资料及历史恢复文件、docs、reports，以及第三方依赖版本记录。官方 PDF、示例、picotool 等保留原作者权利与已有许可证；项目没有为这些内容重新授权。

`third_party/versions.json` 记录 Pico SDK、CMSIS-DAP、TinyUSB 的准确 revision；SDK 和交叉编译器本体不提交 Git。`.cache`、`.tmp`、build、artifacts 和 packages 保留在 G 盘本地。原始目标单片机 Flash 备份、全部原始采集和大量诊断日志仍保存在本地 artifacts，不自动上传。

新电脑从源码构建时，先按版本记录安装 SDK/CMSIS-DAP/TinyUSB，并准备项目原有 tools/arm-toolchain、tools/cmake、tools/ninja 和 tools/picotool。源码包不假定新电脑已安装这些工具。Windows 主机编译使用 MinGW；原脚本中的 G 盘 CLion 编译器路径需要按电脑环境调整。无需重新编译即可使用 Releases 中的 Windows 运行包。

## 打包

```powershell
.\tools\package-project.ps1
```

生成 `packages/rp2350-debug-instrument-v0.3.0-source.zip`（Git 管理的源码和资料）、`rp2350-debug-instrument-v0.3.0-windows.zip`（已编译 UF2/ELF、Windows USB 工具、网页和启动脚本、文档及截图）及 SHA256SUMS。压缩包不包含 Git 凭据、下载工具链或临时目录。

Windows 包解压到 G 盘目录，运行 `tools/start-logic-web.ps1` 后打开 `http://127.0.0.1:8765/`。首次启动需要 RP2350 已运行 0.3.0；更新固件的方法见主 README。包中工具默认识别当前项目探针序列号；换板须修改配置/源码。历史波形等原始数据若未随包提供，相应入口需要自行导入采集文件。
