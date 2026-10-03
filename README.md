# ChameleonLern

ChameleonLern 是由 [NUaris](https://github.com/NUaris) 维护的个人固件二次开发项目，基于官方 [ChameleonUltra](https://github.com/RfidResearchGroup/ChameleonUltra) 的历史代码进行个人修改和扩展。

项目目标是结合周边蓝牙环境、读卡器交互特征、时间和个人使用习惯，学习不同场景对应的卡片，并自动选择合适的模拟卡槽。

**目前仓库处于基础固件阶段，混合特征学习与自动选卡尚未实现。** 下文分别列出已有代码和后续计划，不将计划功能视为已可用功能。

## 官方来源与维护方式

本项目的起始代码来自官方 ChameleonUltra 仓库的历史提交：

- 官方仓库：[RfidResearchGroup/ChameleonUltra](https://github.com/RfidResearchGroup/ChameleonUltra)。
- 起始提交：[`d866e6f9626a57c4099e29d4bcda11781f0433a9`](https://github.com/RfidResearchGroup/ChameleonUltra/commit/d866e6f9626a57c4099e29d4bcda11781f0433a9)，提交说明为 `project merge`。
- 个人二次开发仓库：[NUaris/ChameleonLern](https://github.com/NUaris/ChameleonLern)。

原始代码、设计和第三方组件的贡献属于官方团队及各自作者，本仓库保留其版权和许可证说明。ChameleonLern 由个人维护，项目名称用于区分这份二次开发版本。

后续维护遵循以下原则：

1. 按本项目的混合学习与自动选卡目标推进开发，不跟随官方发布节奏，也不自动同步官方分支或全量合并官方新功能。
2. 持续参考官方代码、问题反馈和修复方案，保持对上游改进的关注。
3. 对官方发现或修复、且适用于本项目的 bug，在本项目中修复或按需移植官方补丁，并验证与本项目改动的兼容性。
4. 移植修复时，在提交说明中记录对应的官方提交或 issue，以及本项目所需的调整和验证结果。

## 当前代码状态

| 模块 | 已有实现 | 尚需补齐 |
| --- | --- | --- |
| 卡槽管理 | 8 个卡槽；每槽可配置高频与低频卡；按钮和 USB 命令切槽 | 自动选择、安全切换和异常回退 |
| 高频模拟 | ISO14443-A、Mifare Classic 系列处理逻辑 | 稳定性验证；NTAG 当前仅有类型定义与空处理回调 |
| 低频模拟 | EM410X 模拟逻辑 | 实机兼容性与稳定性验证 |
| 读卡与通信 | 高频、低频读卡；USB 数据帧；Python CLI | 真实卡片数据导入模拟卡槽的完整流程 |
| 蓝牙 | 外设广播、连接和 NUS 服务框架 | 周边广播扫描、RSSI 与环境特征采集；通信处理完善 |
| 读卡器特征 | 交互状态机、部分 Mifare 认证日志 | 特征提取、场景识别和与卡槽的关联 |
| 时间 | 运行期间的计时与休眠控制 | 时间同步、日期／星期／时段特征与休眠后的时间保持 |
| 混合学习 | 尚未接入 | 样本记录、融合评分、置信度、手动纠正与持久化 |

这些状态来自源码检查，不代表已通过完整固件编译或硬件验收。

## 仓库结构

```text
firmware/
  application/
    app/                 应用、卡槽、通信和 RFID 代码
    project/             Keil 应用工程
    sdk/                 随仓库提供的 Nordic SDK 组件
  bootloader/
    app/                 DFU 引导程序和公钥
    sdk/                 引导程序使用的 Nordic SDK
    Makefile             引导程序 GCC 构建入口
software/
  script/                Python 交互式 CLI 与设备通信
  src/                   配套 C 工具及 CMake 工程
resource/                从历史基线继承的 DFU 签名相关材料
LICENSE                  GNU GPLv3 正文
LICENSES/                原有 GPLv2 代码对应的许可正文
NOTICE.md                来源、版权与第三方许可说明
```

## 开发与构建

当前硬件目标为 **nRF52840**，使用 **S140 SoftDevice**。实际引脚配置以 [`rfid_main.h`](firmware/application/app/rfid_main.h) 为准；构建配置中的 `BOARD_PCA10056` 不表示可以直接用于任意开发板。

### 应用固件

使用 Keil µVision 打开 [`firmware/application/project/nfctag.uvprojx`](firmware/application/project/nfctag.uvprojx)，选择 `nrf52` 目标。

工程记录的工具链为 ARM Compiler 5.06 update 7，设备包为 `NordicSemiconductor.nRF_DeviceFamilyPack` 8.40.3。应用 SDK 配置位于 [`sdk_config.h`](firmware/application/sdk/config/sdk_config.h)。当前应用没有提供 Makefile、CMake 或 PlatformIO 构建入口，后续需补齐可复现的构建流程。

### 引导程序

引导程序包含 GNU Make 构建入口；仓库内 SDK 的发布说明标记为 nRF5 SDK 17.1.0。工具链路径配置见 [`Makefile.posix`](firmware/bootloader/sdk/components/toolchain/gcc/Makefile.posix)，默认记录的 GCC 版本为 9.3.1。

准备 ARM GNU 工具链后，在仓库根目录执行：

```sh
make -C firmware/bootloader
```

该入口只构建引导程序。烧录前需核对应用、SoftDevice、引导程序及 DFU 公钥的配套关系。

### Python CLI

建议使用 Python 3.10 或更新版本，并在虚拟环境中安装脚本实际使用的依赖：

```sh
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install pyserial colorama
python3 software/script/chameleon_cli_main.py
```

Windows 下使用 `.venv\Scripts\activate` 激活虚拟环境。CLI 启动后，可按实际设备端口输入：

```text
hw connect -p /dev/ttyACM0
hw mode get
```

Windows 端口通常为 `COM` 加编号。命令参数以 CLI 内的帮助信息为准。

### 配套 C 工具

准备 C 编译器和 CMake 后，在仓库根目录执行：

```sh
cmake -S software/src -B software/src/out
cmake --build software/src/out
```

现有 CMake 配置将可执行文件输出到 `software/bin/`。这部分构建的是上位机辅助工具，不包含应用固件。

## 后续开发顺序

- 修复编译声明冲突、Flash 记录关闭、缺失卡槽数据处理、认证日志边界和蓝牙接收路径问题，建立可靠的基础固件。
- 接入蓝牙环境扫描、时间同步和读卡器交互特征采集，明确采样窗口、缓存与功耗策略。
- 将手动选择的卡槽与当时的环境特征关联，记录学习样本，并支持纠正和清除历史。
- 实现可解释的融合评分与置信度判断；不确定时保留手动选择，并在一次刷卡交互中锁定卡槽。
- 补齐构建检查、持久化验证和实机验收，再发布可用的自动选卡版本。

现有 System OFF 深度休眠会停止蓝牙扫描与运行计时。自动选卡需要同时设计唤醒、扫描周期、特征缓存和时间保持方式。

## 许可证与签名材料

本项目的个人修改及基于官方代码形成的派生部分按 **GNU GPLv3** 发布，完整条款见 [`LICENSE`](LICENSE)。原有 GPLv2-or-later 源文件的声明仍然保留；Nordic SDK、SoftDevice 和其他第三方组件适用各自许可证，具体范围见 [`NOTICE.md`](NOTICE.md)。

开源许可证与 DFU 签名密钥是两种不同的材料。`resource/` 中的历史签名材料来自原始基线，不能作为新项目独有的可信签名凭据；正式发布应另外准备私有签名密钥和与之匹配的引导程序公钥，私钥不应提交到仓库。
