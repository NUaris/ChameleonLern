# ChameleonLern

[![Firmware CI](https://github.com/NUaris/ChameleonLern/actions/workflows/ci.yml/badge.svg)](https://github.com/NUaris/ChameleonLern/actions/workflows/ci.yml)

ChameleonLern 是 Chameleon Ultra 的个人自定义固件，保留官方卡片模拟、读卡、密钥恢复、卡槽管理、蓝牙及灯效功能，并增加设备端场景学习与自动选卡。

## 已根据最新官方固件同步更新

**v0.2.0-alpha.1 已于 2026-10-07 完整同步官方 [ChameleonUltra](https://github.com/RfidResearchGroup/ChameleonUltra) 的 firmware 与 software 源码，基准提交为 [`5c99d4a`](https://github.com/RfidResearchGroup/ChameleonUltra/commit/5c99d4a39b424cc67ae82bbcfc8ba5ec8f69bf9c)。** 此提交包含官方最新的 4K 卡块处理修复。生产构建使用官方 Makefile、Nordic SDK、硬件定义和读卡实现；学习模块以扩展方式接入，不再使用旧历史固件加基础 GUI 兼容层的方案。

本次同步包括官方 HF/LF 读卡、Classic 卡型识别、扇区密钥检查、static nested / nested / hardnested 数据采集、卡片模拟、USB／充电灯效、按钮配置、NTAG 与其他现行官方卡型支持。密钥恢复计算仍由 GUI／CLI 执行，结果取决于卡型、已知密钥、卡片防护与射频通信；不保证每张卡均可恢复。Classic 1K 是 **16 个扇区，编号 0–15**；Mini 是 5 个扇区，不通过改界面扇区数量掩盖卡型识别问题。

自定义 USB 名称的描述符缓存已扩大到 63 字符，修复名称带版本后缀时超过 SDK 默认容量、导致串口无法枚举的问题。CLI 创建／导入卡片会同步设置卡型并启用对应天线。

2026-10-07 在 Ultra HW v1 上实测：USB 串口正常；测试卡识别为 Classic 1K／16 个扇区；使用已有备份密钥完成全部 32 次 A/B 认证，并核对每个扇区的一个数据块。GUI 默认字典检查与 static nested 恢复已验证，其中最后扇区的 B 密钥候选搜索未跑完；已有密钥认证不等同于全部密钥恢复成功。两张 1K 模拟卡已从旧版备份恢复并读回核对。A/B 与外部读卡器的实际刷卡结果、充电呼吸灯仍需实物验收。

同步来源和逐文件哈希见 [UPSTREAM_SYNC.json](docs/UPSTREAM_SYNC.json)，其中列出为学习扩展、保留存储边界及 FDS 记录关闭修复而修改的文件。本项目保留个人维护方向；本次为明确版本同步，后续更新仍需合并、编译与实机验证，不代表自动追随官方每次提交。

## A/B 手动选择与自动学习

- **按 A/B 选择卡片：** 固定模拟选中的卡。此次选择不会被自动推荐覆盖，也不会在等待超过 30 秒后失效。刷卡过程中锁定卡槽；离场后将这次明确选择及周边蓝牙、读卡器交互、时间特征用于学习。场中按键请求会等离场后安全切换，用于下一次刷卡。
- **没有手动选择：** AUTO 模式在触场前根据学习结果预选卡。缺少样本、匹配分数不足或候选冲突时保留当前卡；正在进行的刷卡不会换卡。
- **不会把自动推荐当作正确答案反复训练。** 同场景通常需重复明确选择至少两次；支持清除或纠正错误标签。

全新模型默认 AUTO；已有保存的 OFF／OBSERVE／AUTO 设置仍会保留。可用 CLI 开启 `mode auto` 或关闭 `mode off`。型号相同的读卡器可能具有相似交互，蓝牙广播也可能变化；评分是特征相似度，不是开门成功率。只有时间、通用寻卡指令或未知场景不会强行选卡。

读卡器交互只能在开始刷卡后采集。首次接近未知读卡器时，设备无法凭尚未发生的交互识别它；本版采用 BLE 触场前预选和离场后学习，不在一次认证中途换卡。

## 安装与迁移

目标是量产 **Chameleon Ultra HW v1 / nRF52840 / S140 7.2.0**；签名 `ultra-dfu-app.zip` 仅更新应用，保留现有官方引导程序、SoftDevice 和 FDS 区。Lite 和旧原型机不适用。见 [FLASHING.md](docs/FLASHING.md)。

v0.2.0 起卡槽与卡片数据采用现行官方格式，支持官方 GUI 管理。**从 alpha.2／alpha.3 升级时须先导出私有格式中的卡片数据，再按当前协议导入；升级不自动把私有卡片记录解释成官方记录。** `0x4C10` 学习模型及 CRC 格式保持不变，已有训练记录可继续读取。升级和恢复前请保留自己的独立备份；卡片 dump、ID 和密钥不要提交到公共仓库。

## 学习管理 CLI

```sh
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install -r software/script/requirements-learning.txt
python3 software/script/chameleon_learning.py --port /dev/ttyACM0 status
python3 software/script/chameleon_learning.py --port /dev/ttyACM0 mode auto
python3 software/script/chameleon_learning.py --port /dev/ttyACM0 sync-time --timezone 480
python3 software/script/chameleon_learning.py --port /dev/ttyACM0 slots
python3 software/script/chameleon_learning.py --port /dev/ttyACM0 samples
python3 software/script/chameleon_learning.py --port /dev/ttyACM0 save
```

卡槽编号为 **1..8**；macOS 使用实际 `/dev/cu.usbmodem...`，Windows 使用 `COM...`。CLI 的卡片创建／导入采用现行官方命令和 16 位类型 ID；支持 `import-mf1 1 my-card.bin`、`import-em410x 2 0102030405`、`init-card 1 1k`。MF1 原始 dump 长度为 320／1024／2048／4096 字节；长 UID、ATS 和其他模拟参数可通过官方 GUI 配置。

调试或明确反馈可使用 `mode observe`、`select 1`、`train 1 --source environment`、`train 1 --source reader`、`predict`、`forget 1` 和 `configure`。`reader` 反馈应在交互结束 20 秒内提供。具体数据格式见 [PROTOCOL.md](docs/PROTOCOL.md)，学习规则见 [LEARNING.md](docs/LEARNING.md)。

## 构建与验证

```sh
sudo apt-get install --no-install-recommends gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi libnewlib-dev make
python3 -m pip install -r scripts/requirements-dfu.txt
python3 scripts/build_firmware.py
python3 scripts/verify_firmware.py build/firmware/chameleon-learning.hex
python3 scripts/package_dfu.py
python3 scripts/verify_dfu.py build/firmware/ultra-dfu-app.zip --hex build/firmware/chameleon-learning.hex
SANITIZE=1 scripts/test.sh
```

可用 `--cc /path/to/arm-none-eabi-gcc` 指定工具链。输出包括签名 ZIP、HEX、BIN、ELF、MAP、编译日志，以及记录官方基准、自定义源码提交和 SHA256 的 manifest。只将签名 ZIP 交给 DFU 工具；HEX／BIN 为未签名原始应用。

主机测试覆盖融合评分、持久化校验、按键锁卡、完成交互后学习、自动选择、HF/LF 重叠场保护、场中排队切换、OFF 模式、模型存储适配、CLI 边界、DFU 签名及上游未改文件完整性。它们不能替代实体卡和读卡器验收。官方 SDK 在较新 GCC 上的警告保留于日志；生产源码编译错误仍会阻止构建。

生产代码位于 `firmware/application/src/`，学习扩展位于其 `selection/` 子目录。`firmware/common/`、`firmware/nrf52_sdk/` 和官方 host 工具同步自上述提交。原历史 Keil 工程和重复 SDK 已移除，避免误编译旧实现。

默认扫描每 8 秒运行 1.2 秒；AUTO／OBSERVE 保持 System ON，会增加功耗。运行时间不会作为跨重启 RTC 保存，重启后需重新同步时间。实际刷卡兼容性、呼吸灯、续航与射频稳定性须按 [验收清单](docs/FLASHING.md) 在自己的硬件上确认。

## 来源与许可证

本项目最初来自官方历史提交 `d866e6f`，现已迁移至上述最新官方基准。官方代码、设计及第三方组件贡献属于原团队与各自作者；派生修改按 GNU GPLv3 发布，见 [LICENSE](LICENSE) 和 [NOTICE.md](NOTICE.md)。历史版本可通过 Git 标签查看。

DFU 使用上游公开的共享兼容签名材料，仅为兼容现有官方引导程序，不代表独立、保密的发布身份。普通 Ultra 用户无需更换引导程序。
