alpha.3 修复 Chameleon Ultra GUI 的旧协议弹窗与 USB Lite 误识别，补充卡槽查询、
独立 HF/LF 启用、持久化昵称、MF1／EM410X 导入导出和现行 HF 寻卡响应。
学习和自动选卡算法保持原行为。管理格式为 2.0，私人学习协议保持 1。
不声称具备官方全部射频功能；能力表仅列实际实现的命令。
alpha.2 的卡片和学习记录无需迁移；昵称新增私有记录，原配置保留字节保存频率启用状态。

ChameleonLern 为基于官方 ChameleonUltra 历史代码的个人二次开发固件，实现周边蓝牙、读卡器和时间特征的学习与自动选卡。

本版本修正量产 Ultra 的 LF 输入、读卡器电源、按钮和 LED 引脚，并提供 **ultra-dfu-app.zip**：使用与现行官方引导程序匹配的共享密钥签名，hw_version=0、sd_req=0x100（S140 7.2.0）、application_version=1。仅更新应用，不替换 SoftDevice 或引导程序。HEX/BIN 仍为原始应用（基址 0x27000），普通 DFU 安装请选择 ZIP。

升级前备份卡片。官方格式及 alpha.1 数据不会自动导入；本版本使用独立 FDS 记录，安装后使用随版本提供的学习 CLI 重新导入。勿将 alpha.1 的原型引脚固件用于量产 Ultra。本版本保留官方 DFU 进入命令，方便再次升级或刷回官方应用。

CI 包含主机测试、ARM 编译、镜像范围与签名校验；打包格式另经 Nordic nrfutil 独立验证。**尚未完成实体设备安装、刷卡和功耗验收，属于实验预览版。** 默认 OFF，先按 docs/FLASHING.md 验收，确认观察模式后再启用 AUTO。Release 自动创建为草稿，不等于实机认证。

原作者和第三方许可见 LICENSE／NOTICE.md。本项目不全量同步官方更新，会按需修复适用的官方 bug。
