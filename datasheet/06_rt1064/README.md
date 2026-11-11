# RT1064参考资料

来源为逐飞官方仓库 https://gitee.com/seekfree/RT1064_Library ，本次检索日期2026-11-02。

- IMXRT1064_Datasheet.pdf：NXP数据手册。
- IMXRT1064_Reference_Manual.pdf：NXP参考手册。
- SeekFree_RT1064_Lite_Manual_1.2.pdf：逐飞Lite使用说明。
- SeekFree_RT1064_Lite_Schematic_V1.1.pdf：Lite V1.1原理图，用户照片为V1.0；修订说明主要为供电二极管/跳线修改，不能把V1.1所有板级细节当作用户V1.0的实物确认。
- SeekFree_RT1064_Pin_Mapping.xlsx：逐飞命名表（包含E9=GPIO_EMC_09，注明E组SDRAM占用）。

芯片封装球位E9=NVCC_GPIO与板级E9=GPIO_EMC_09是不同命名。此次按用户确认的信号焊盘及逐飞表配置GPIO4_IO09，禁用SDRAM使用。
