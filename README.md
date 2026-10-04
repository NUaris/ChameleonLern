# ChameleonLern

[![Firmware CI](https://github.com/NUaris/ChameleonLern/actions/workflows/ci.yml/badge.svg)](https://github.com/NUaris/ChameleonLern/actions/workflows/ci.yml)

ChameleonLern 是基于官方 [ChameleonUltra](https://github.com/RfidResearchGroup/ChameleonUltra) 历史代码进行个人二次修改的固件项目。目标是结合周边蓝牙环境、读卡器交互特征、时间和个人选择，学习场景与卡槽的关系，自动选择将要使用的卡。

**已实现第一版设备端混合学习与自动选卡，并提供量产 Chameleon Ultra 的签名 DFU 应用包。** 默认关闭自动选卡，建议先使用观察模式收集反馈。评分表示特征相似程度，不能当作准确率或开门成功率。

运行官方固件的 Ultra 可以通过现有官方引导程序安装 `ultra-dfu-app.zip`，无需更换引导程序。先备份卡片，再使用支持本地 ZIP 的 DFU 工具；官方默认升级源仍只提供官方固件。安装步骤见 [FLASHING.md](docs/FLASHING.md)。**不要将 `v0.1.0-alpha.1` 的旧原型引脚应用刷入量产 Ultra；使用 `v0.1.0-alpha.2` 或后续版本。** 本项目卡片格式与现行官方不同，需要重新导入，日常管理使用本项目 CLI。

## 官方来源与维护方式

起始代码来自官方 ChameleonUltra 的历史提交 [`d866e6f9626a57c4099e29d4bcda11781f0433a9`](https://github.com/RfidResearchGroup/ChameleonUltra/commit/d866e6f9626a57c4099e29d4bcda11781f0433a9)（`project merge`），官方代码、设计及第三方组件的贡献属于原团队与各自作者。

本项目由个人维护，以自己的混合学习与自动选卡目标推进，不跟随官方发布节奏，不自动同步官方分支或全量合并官方功能。后续会持续参考官方的实现和问题反馈；**官方发现或修复、且适用于本项目的 bug，这里会修复或按需移植补丁**，并验证与个人改动的兼容性。移植时记录官方提交或 issue 及本项目所需的调整。本项目会借鉴上游修复，维护方向也会保留个人修改。

## 已有功能

| 功能 | 当前实现 |
| --- | --- |
| 卡片与卡槽 | 8 个卡槽；每槽可同时配置 MIFARE Classic 高频和 EM410X 低频卡；按钮／命令切卡；原始卡片数据导入 |
| 蓝牙环境 | 被动扫描周边广播；缓存最多 6 个环境标识及平滑 RSSI；20 秒过期 |
| 读卡器特征 | 高频 REQA／WUPA、级联流程、认证类型与块号、RATS、粗粒度时序；不把认证密钥或 nonce 写入学习模型 |
| 时间 | USB／蓝牙命令同步 UTC 与时区，融合本地时段、工作日／周末；重启后需重新同步 |
| 学习 | 按钮／手动切卡产生环境标签；明确反馈可关联最近 20 秒内的读卡交互；32 个有限样本，合并相近反馈，替换最旧样本 |
| 决策 | BLE／读卡器／时间加权匹配；最低评分、候选差距、重复反馈门槛；冲突或不足时保持当前卡 |
| 切换保护 | 有场时锁定卡槽；手动请求排队；离场等待 350 ms；手动选择保护 30 秒；自动切卡间隔 10 秒；加载失败回退 |
| 持久化 | 版本化模型与 CRC32；Flash 完成事件检查、失败重试、记录关闭；模型及配置可显式保存／清除 |
| 管理与构建 | USB CDC／BLE NUS 同一命令协议；Python 管理工具；GCC 构建、主机测试、GitHub Actions 与标签草稿发布 |

模式：`off` 关闭采样和自动选卡；`observe` 采样、学习并给出建议；`auto` 在满足所有门槛且设备处于模拟模式时自动切卡。默认权重为蓝牙 60、读卡器 30、时间 10，最低分 70、领先差距 12、相近反馈至少 2 次。缺失的特征不参与评分，时间本身不能触发选卡。

读卡器交互通常要在开始刷卡后才能采集。本版不会在一次交互中换卡：读卡器特征可用于离场后的推荐和下一次靠近时的选择，首次靠近主要依赖已扫描到的蓝牙环境。只有通用寻卡指令时不将读卡器视为可靠身份。低频读卡器暂未实现独立身份特征，主要依靠周边蓝牙和时间。

蓝牙环境不是定位服务。公共／静态地址、较稳定的设备名、128 位 UUID、iBeacon／Eddystone UID 可提供环境线索；仅有随机隐私地址或通用厂商／服务编号的广播会被排除。相同广播、遮挡、设备更换和 RSSI 波动仍可能导致不确定或误判，建议通过观察模式确认各场景可区分后启用自动模式。

## 使用示例

需要 Python 3.10+，连接设备 USB CDC 后执行（Windows 将端口换成 `COM3` 等）：

```sh
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install -r software/script/requirements.txt
python3 software/script/chameleon_learning.py --port /dev/ttyACM0 status
```

以下命令的卡槽编号为 **1..8**；协议内部使用 **0..7**。

```sh
# 将自己已有的原始卡片数据写入卡槽；会覆盖该槽对应频率的卡片数据。
python3 software/script/chameleon_learning.py --port /dev/ttyACM0 import-mf1 1 my-card.bin
python3 software/script/chameleon_learning.py --port /dev/ttyACM0 import-em410x 2 0102030405

# 开启观察模式，同步北京时间，然后在实际场景中选择正确的卡。
python3 software/script/chameleon_learning.py --port /dev/ttyACM0 mode observe
python3 software/script/chameleon_learning.py --port /dev/ttyACM0 sync-time --timezone 480
python3 software/script/chameleon_learning.py --port /dev/ttyACM0 select 1

# 在各场景多次使用并反馈正确卡槽；reader 反馈应在离场后 20 秒内提交。
python3 software/script/chameleon_learning.py --port /dev/ttyACM0 train 1 --source environment
python3 software/script/chameleon_learning.py --port /dev/ttyACM0 train 1 --source reader
python3 software/script/chameleon_learning.py --port /dev/ttyACM0 predict
python3 software/script/chameleon_learning.py --port /dev/ttyACM0 samples

# 确认建议稳定后开启自动模式。
python3 software/script/chameleon_learning.py --port /dev/ttyACM0 mode auto
python3 software/script/chameleon_learning.py --port /dev/ttyACM0 save

# 查看卡槽、调整门槛、清除错误学习或关闭自动模式。
python3 software/script/chameleon_learning.py --port /dev/ttyACM0 slots
python3 software/script/chameleon_learning.py --port /dev/ttyACM0 configure --min-score 80 --margin 15
python3 software/script/chameleon_learning.py --port /dev/ttyACM0 forget 1
python3 software/script/chameleon_learning.py --port /dev/ttyACM0 mode off
```

`init-card 1 1k` 可创建测试用默认卡；支持 `mini/1k/2k/4k/em410x`。MIFARE 导入接受 320／1024／2048／4096 字节的完整原始 dump，目前使用 4 字节 UID 的 block 0 格式；不包含独立导入 7／10 字节 UID、防冲撞参数或 ATS 的管理命令。CLI 在读卡模式下分块导入，完成保存后恢复原模式；失败时保留读卡模式，重试完整导入后再使用卡片。

手动选择立即建立保护期，实际切换可能等待离场。`status` 显示待切槽、建议、分数、原因、可用槽和存储状态。按钮学习在观察／自动模式下启用；明确反馈不会把自动推荐结果反复当作训练标签。纠正错误标签时可先 `forget` 原卡槽的历史，再重新学习。

## 构建、测试与 CI/CD

目标硬件为 **量产 Chameleon Ultra HW v1，nRF52840 + S140 7.2.0**。引脚见 [`board_chameleon_ultra.h`](firmware/application/app/board_chameleon_ultra.h)，已按现行官方实现校正 LF 输入、读卡器电源、按钮和 LED 顺序。DFU hardware version 为设备类型 **0**，与板卡修订号 1 不同；本包不支持 Lite 或旧原型机。`BOARD_PCA10056` 编译宏用于 SDK 编译配置。

在 Debian／Ubuntu 安装 ARM GCC、binutils 和 newlib 后构建：

```sh
sudo apt-get install --no-install-recommends gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi libnewlib-dev
python3 scripts/build_firmware.py
python3 scripts/verify_firmware.py build/firmware/chameleon-learning.hex
python3 -m pip install -r scripts/requirements-dfu.txt
python3 scripts/package_dfu.py
python3 scripts/verify_dfu.py build/firmware/ultra-dfu-app.zip --hex build/firmware/chameleon-learning.hex
SANITIZE=1 scripts/test.sh
```

可用 `--cc /path/to/arm-none-eabi-gcc` 指定工具链。输出在 `build/firmware/`：签名 DFU ZIP、HEX、BIN、ELF、MAP、编译告警和包含 SHA256 的 manifest。HEX/BIN 为未签名原始应用，签名存在 ZIP 的 Nordic init packet 中。构建脚本从 Keil 工程读取源文件与 include 配置，替换 GCC 专用启动／错误处理实现，保留持久化结构所需的短枚举布局。

主机测试覆盖融合评分、冲突、反馈、样本容量、模型损坏、时间回绕、广播解析、通信分片／边界、Flash 错误、切卡互锁、空卡槽清理和失败回退；使用 AddressSanitizer 与 UndefinedBehaviorSanitizer。它们不能代替 RF 硬件测试。

## 目录与实现说明

```text
firmware/application/app/selection/  可测试的融合核心与硬件适配
firmware/application/app/            RFID、通信、存储、模式及运行循环
firmware/application/project/        Keil 工程与 GCC 链接脚本
firmware/bootloader/                 原引导程序与 SDK
software/script/chameleon_learning.py 学习、时间、卡片导入管理
scripts/                            构建、镜像校验及测试入口
tests/                              主机测试与硬件接口替身
docs/                               协议、验收与发布说明
```

设计和数据格式见 [学习模块说明](docs/LEARNING.md)，外部应用可通过 [命令协议](docs/PROTOCOL.md) 对接 USB 或 BLE NUS。原交互式 CLI 及 `software/src/` 辅助工具仍保留。

观察／自动模式使用 System ON 空闲等待以保留扫描与运行时钟，会增加功耗；扫描默认每 8 秒运行 1.2 秒。关闭模式可以进入原有 System OFF 深睡，重启／深睡唤醒后的时间失效，必须重新同步。当前没有外置 RTC、手机后台自动校时或实机续航数据。NTAG 仍只有历史类型定义，没有完整模拟实现。

## 许可证与签名材料

个人修改及基于官方代码形成的派生部分按 **GNU GPLv3** 发布，条款见 [LICENSE](LICENSE)。原 GPLv2-or-later 声明保留，Nordic SDK、SoftDevice 等第三方组件适用各自许可证，见 [NOTICE.md](NOTICE.md)。

开源许可证与 DFU 密钥不同。`resource/` 中的历史签名材料与现行官方公钥一致，用于兼容设备已有的官方引导程序；官方也在源码中公开此共享材料。它不代表本项目拥有独立、保密的发布身份。若将来需要独立签名身份，才需要自行管理私钥和匹配引导程序；普通 Ultra 用户安装本项目应用包无需这样做。来源与核对依据见 [兼容性记录](docs/UPSTREAM_COMPATIBILITY.md)。
