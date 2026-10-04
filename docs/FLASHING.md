# Ultra 安装、构建产物与实体设备验收

从 `v0.1.0-alpha.2` 起，`ultra-dfu-app.zip` 是为**量产 Chameleon Ultra HW v1、官方引导程序、S140 7.2.0** 制作的签名应用 DFU 包。可以保留设备已有的官方引导程序与 SoftDevice，通过 DFU 安装。Lite、历史原型机或自行更换了签名公钥的引导程序不适用。DFU 包生成与签名通过独立工具验证，实机安装、射频与功耗仍未验证。

**不要将 alpha.1 的旧原型引脚固件刷入量产 Ultra；选择 alpha.2 或后续版本的 ZIP。** 本版校正 LF 输入、读卡器电源、按钮和 LED 引脚，兼容性依据见 [UPSTREAM_COMPATIBILITY.md](UPSTREAM_COMPATIBILITY.md)。

## 从官方固件安装

1. 先用当前官方工具备份自己的卡片原始 dump、EM410X ID、昵称和配置，并保留可刷回的官方 Ultra 应用 DFU ZIP。
2. 下载本版本的 `ultra-dfu-app.zip`，核对同一构建 manifest 中的 SHA256。CI／Release 附件里的 HEX/BIN 不能直接交给 DFU 升级器。不要解压后再选择 BIN。
3. 通过当前工具的进入 DFU 功能，或设备官方说明中的按键／复位方法进入引导程序。也可用本项目 CLI 向正在运行的官方应用发送 1010：

   ```sh
   python3 -m pip install -r software/script/requirements.txt
   python3 software/script/chameleon_learning.py --port /dev/ttyACM0 enter-dfu --official
   ```

   当前官方固件通常立即重启而不返回 ACK；CLI 只报告请求已发送，需要实际看到 DFU USB 设备／串口才能继续。Windows 将应用端口换成 `COM3` 等；DFU 后端口可能改变。升级时移开读卡器，保持 USB 供电。
4. 使用支持**本地 Nordic Secure DFU ZIP** 的升级工具选取 `ultra-dfu-app.zip` 并完成传输。官方默认“升级到最新版”使用官方发布源，不能自动找到本项目；是否有本地 ZIP 入口取决于所用工具版本。Nordic 新版 nrfutil 的命令示例（先按 Nordic 官方说明安装工具）：

   ```sh
   nrfutil install nrf5sdk-tools
   nrfutil nrf5sdk-tools dfu usb-serial --package ultra-dfu-app.zip --port /dev/ttyACM1
   ```

   已安装旧版 nrfutil 的用户可使用 `nrfutil dfu usb-serial --package ultra-dfu-app.zip --port /dev/ttyACM1`。这里的端口是**进入 DFU 后**的端口。
5. 重启后连接应用 USB，使用本版本 CLI 查看 `status`。默认学习模式 OFF；重新导入备份卡片，核对 HF/LF 基础行为，再按下面顺序验收，先 OBSERVE 后 AUTO。

本项目使用独立 FDS 记录，避免按旧格式覆盖官方记录。**官方卡片和 alpha.1 数据不会自动显示在本项目里**；需要重新导入与学习。已有官方记录与本项目记录共用有限 Flash 空间，存储不足会报错；不要通过擦除全芯片解决。全芯片擦除会同时移除 SoftDevice、引导程序、settings 和用户数据。

## 后续升级或刷回官方

在本项目应用运行时执行：

```sh
python3 software/script/chameleon_learning.py --port /dev/ttyACM0 enter-dfu
```

成功响应后约 200 ms 重启进入现有官方引导程序；有场或保存失败会拒绝。进入 DFU 后可安装后续本项目 Ultra ZIP 或匹配设备的官方应用 ZIP。DFU application_version 保持官方值 1，避免人为提高计数阻碍返回官方固件。原始官方记录未被本项目主动改写，但仍应保存独立备份。

升级失败时先记录升级工具报错：型号／SoftDevice／签名不匹配应检查包与设备，不要换包反复盲试；若进入 DFU 后应用未正常启动，按照设备官方恢复说明重新进入引导程序，刷回已备份的官方应用 ZIP。本项目保留引导程序，但恢复操作仍需实体设备验证。

## 原始构建产物

HEX/BIN 是 nRF52840、S140 7.2.0 的**原始应用镜像**，入口 `0x27000`；BIN 用 SWD 写入时基址必须是 `0x27000`，不能写到地址 0。ELF/MAP 用于调试。应用链接范围至 `0xc7000`，FDS 保留 22×8192 字节，引导程序在 `0xf3000`，均不包含在应用 ZIP 的载荷中。ZIP 的 init packet 包含共享官方兼容密钥签名，原始 HEX/BIN 本身没有签名。

CI 自动编译、测试、签名、验证并生成草稿 Release，不执行设备烧录。正式公开发布前仍需下面的硬件验收。

## 验收顺序

1. 先验证原有 HF／LF 模拟、读卡模式、USB、BLE 连接及按钮切槽；空槽不得广播上一槽身份。
2. 导入两张有效卡，重启后核对卡槽和数据。尝试在有场时切卡、导入、保存：切卡应排队，修改／保存应报告忙；离场后处理切卡。
3. 开启 observe，同步时间；分别在两处实际环境观察扫描和评分，在多次使用中选择正确卡并提交反馈。无信号、只有通用寻卡、两卡候选相同或支持不足时应保持当前卡。
4. 高频刷卡离场后 20 秒内提交 reader 反馈；再次靠近确认建议，检查不同认证块／不同读卡器前缀的区分能力。不能依据设备端认证 ACK 判断门禁已放行。
5. 启用 auto，在场内验证卡槽不变；离场后验证 350 ms 等待、30 秒手动保护、10 秒自动切换间隔和失败回退。
6. 重启校时并验证模型恢复，清除指定槽／全部历史后重启确认；导入中断后保持读卡模式并完整重试。
7. 测量 observe／auto 的功耗、连续扫描与 BLE NUS 通信并行时的丢包、HF／LF 响应时序；关闭模式验证深睡和场唤醒。分别测试 MF1 Mini／1K／2K／4K、实际读卡器及双频同时有场的情况。

以上为待执行的硬件验收，不代表已经通过。CI 的主机替身测试与镜像编译不能测量射频时序、供电、Flash 实际寿命或续航。
