# 来源与许可证说明

## 官方代码与个人修改

ChameleonLern 基于官方 [RfidResearchGroup/ChameleonUltra](https://github.com/RfidResearchGroup/ChameleonUltra) 的历史代码，由 [NUaris](https://github.com/NUaris) 进行个人二次修改和维护。

起始提交为 [`d866e6f9626a57c4099e29d4bcda11781f0433a9`](https://github.com/RfidResearchGroup/ChameleonUltra/commit/d866e6f9626a57c4099e29d4bcda11781f0433a9)（`project merge`）。2026-10-04 补充了本项目的 README、许可证及来源说明；该日期不表示已完成计划中的自动选卡功能。

项目的个人修改及官方代码的派生部分按 GNU GPLv3 发布。根目录 [`LICENSE`](LICENSE) 使用官方仓库公布的 GPLv3 正文；源码中原有的作者、版权与许可声明应继续保留。

## 原有 GPLv2-or-later 代码

以下文件或文件组含有 GNU GPL version 2 or any later version 的声明：

- `firmware/application/app/rfid/mf1_crapto1.*` 与 `parity.*`。
- `software/src/crapto1.*`、`crypto1.c`、`mfkey.*` 与 `parity.*`。

原有版权与贡献说明包括 bla、Merlok、Roel 和文件中列出的其他作者；具体归属以各文件头为准。本项目保留这些声明，不将原作者的贡献归为个人原创。GPLv2 正文见 [`LICENSES/GPL-2.0.txt`](LICENSES/GPL-2.0.txt)；文件头中的“或任何更新版本”选择权仍然保留。

## Nordic 与第三方组件

根目录的 GPLv3 正文不替代以下组件各自的许可条款：

| 范围 | 许可来源 |
| --- | --- |
| Nordic nRF5 SDK 与引导程序中的 Nordic 示例代码 | 源文件头及 [`nRF5_Nordic_license.txt`](firmware/bootloader/sdk/documentation/nRF5_Nordic_license.txt) |
| nrfx 驱动 | [应用 SDK 的 LICENSE](firmware/application/sdk/modules/nrfx/LICENSE) 和 [引导程序 SDK 的 LICENSE](firmware/bootloader/sdk/modules/nrfx/LICENSE) |
| S140 SoftDevice | 各 SDK 下 `components/softdevice/s140/` 内的许可协议；例如 [应用 SDK 的协议](firmware/application/sdk/components/softdevice/s140/hex/s140_nrf52_7.2.0_licence-agreement.txt) |
| Nordic SDK 所包含的其他第三方库 | 各组件目录中的许可证，以及 [SDK 许可索引](firmware/bootloader/sdk/documentation/licenses.txt) |

复制或分发这些组件时，应同时保留其已有的版权、许可正文和适用条件。

## 固件签名材料

本项目的开源许可证不构成签名证书。历史基线包含 DFU 公钥与签名私钥材料；它们用于说明原有构建关系，不代表 ChameleonLern 拥有独立、保密的签名凭据。新发布版本需要使用单独管理的私钥及匹配的引导程序公钥。
