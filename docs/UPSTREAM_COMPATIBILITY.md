# 当前上游同步基准

v0.2.0-alpha.1 已完整同步官方 ChameleonUltra 的 firmware 与 software 至 2026-10-07 main 提交 [5c99d4a39b424cc67ae82bbcfc8ba5ec8f69bf9c](https://github.com/RfidResearchGroup/ChameleonUltra/commit/5c99d4a39b424cc67ae82bbcfc8ba5ec8f69bf9c)。逐文件上游 Git blob SHA 与自定义补丁清单见 UPSTREAM_SYNC.json。生产构建不再使用下面的历史工程；历史记录保留以追溯 alpha.2／alpha.3 的修正。

本项目新增学习模块、被动 BLE 扫描、有场互锁及手动选择钩子；保留现有官方引导程序、S140 7.2.0、22 个 FDS 页和 RAM 保留边界。SDK／官方读卡器／灯效未作替代实现。FDS 成功读取后关闭打开记录，以免反复读模型阻碍垃圾回收。自定义 USB 名称带硬件／固件版本后缀，描述符容量设为 63 字符，使用 SDK 的独立缓冲区，避免超过默认 31 字符时破坏 USB 状态；回归测试覆盖硬件与版本号的最大长度。

---

# 量产 Ultra 与官方 DFU 的兼容性依据

本项目从 2022 年历史代码派生，没有全量同步官方。alpha.2 的量产硬件和升级配置按官方提交 [`6d92a9ff1a56f93efbcaca10f547eee0a3dd6791`](https://github.com/RfidResearchGroup/ChameleonUltra/commit/6d92a9ff1a56f93efbcaca10f547eee0a3dd6791) 核对，记录于 2026-10-04 UTC。以下验证属于源码／构建验证，尚未进行实体设备验证。

| 核对项 | 官方参考 | 本项目处理 |
| --- | --- | --- |
| Ultra HW v1 引脚 | [hw_connect.c](https://github.com/RfidResearchGroup/ChameleonUltra/blob/6d92a9ff1a56f93efbcaca10f547eee0a3dd6791/firmware/common/hw_connect.c) | LF_OA_OUT=P0.29，READER_POWER=P1.15；校正按钮、LED 编号，HF SPI 引脚一致 |
| DFU 参数 | [build.sh](https://github.com/RfidResearchGroup/ChameleonUltra/blob/6d92a9ff1a56f93efbcaca10f547eee0a3dd6791/firmware/build.sh) | Ultra hw_version=0，S140 7.2.0 sd_req=0x100，application_version=1 |
| 签名公钥 | [bootloader/dfu_public_key.c](https://github.com/RfidResearchGroup/ChameleonUltra/blob/6d92a9ff1a56f93efbcaca10f547eee0a3dd6791/firmware/bootloader/src/dfu_public_key.c) | 本仓库继承材料导出的公钥与官方 64 字节公钥完全一致 |
| 同版升级及型号检查 | [bootloader/sdk_config.h](https://github.com/RfidResearchGroup/ChameleonUltra/blob/6d92a9ff1a56f93efbcaca10f547eee0a3dd6791/firmware/bootloader/src/sdk_config.h) | 官方接受同版本；DFU 应用版本保持 1，避免提高计数阻碍刷回官方应用 |
| 卡片记录与页面 | [fds_ids.h](https://github.com/RfidResearchGroup/ChameleonUltra/blob/6d92a9ff1a56f93efbcaca10f547eee0a3dd6791/firmware/application/src/utils/fds_ids.h)、[sdk_config.h](https://github.com/RfidResearchGroup/ChameleonUltra/blob/6d92a9ff1a56f93efbcaca10f547eee0a3dd6791/firmware/application/src/sdk_config.h) | 同为 22×2048 words；新记录使用 0x4C00/01/02/10，避开当前和历史官方记录，不自动迁移不同卡片格式 |
| DFU 进入及身份命令 | [data_cmd.h](https://github.com/RfidResearchGroup/ChameleonUltra/blob/6d92a9ff1a56f93efbcaca10f547eee0a3dd6791/firmware/application/src/data_cmd.h)、[app_cmd.c](https://github.com/RfidResearchGroup/ChameleonUltra/blob/6d92a9ff1a56f93efbcaca10f547eee0a3dd6791/firmware/application/src/app_cmd.c) | 1010 + GPREGRET 0xB1；提供 1000/1017/1033/1035；自有导入命令移到 1200..1202 |

公钥指纹：对 Nordic 格式 `x小端32字节 || y小端32字节` 做 SHA256，结果为：

```text
24e16a9025ecb0b9608b03da512a9966797b117f8903bcffc437ef2b0985bc7a
```

官方构建使用源码中公开的共享 DFU 材料。本项目仅将它用作与已安装引导程序兼容的签名，不宣称独立发布身份。ZIP 只包含 manifest、application.bin、application.dat，不包含签名密钥、SoftDevice、引导程序或 settings。日常工具与卡片数据格式仍有差异，不能把这些兼容项当作整个官方协议兼容声明。

## 打包验证

`package_dfu.py` 使用仓库 Nordic SDK 的 protobuf 协议定义和 ECDSA P-256/SHA256 签名。签名对象是 **InitCommand**，SHA256 固件摘要和签名两坐标使用 Nordic 小端顺序。`verify_dfu.py` 检查型号、SoftDevice、版本、签名、载荷摘要、镜像向量及 HEX 一致性，拒绝组合更新和 debug 包。

完整本地应用包已用 Nordic nrfutil 6.1.7 的 `InitPacketPB`／`Signing` 独立解码并验证。`tests/fixtures/` 包含由该工具独立生成的固定微型样本，持续检查双向格式兼容性；该样本仅供测试，不能刷入设备。CI 不依赖已停止维护的旧版 Nordic Python CLI。

生成的 Python protobuf 文件可按以下命令重建，正常构建无需安装生成器：

```sh
python3 -m pip install grpcio-tools==1.68.1
python3 -m grpc_tools.protoc \
  -Ifirmware/bootloader/sdk/components/libraries/bootloader/dfu \
  --python_out=scripts \
  firmware/bootloader/sdk/components/libraries/bootloader/dfu/dfu-cc.proto
```

旧 `firmware/bootloader/` 本身仍是历史原型工程（hw_version=52 等），本次没有将其重新构建或替换到设备；不要将它误认为与本次 Ultra 应用包配套的量产引导程序。使用设备已有的官方引导程序。
