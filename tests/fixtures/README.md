`nordic-ultra.dat` 和 `nordic-ultra.bin` 是 Nordic nrfutil 6.1.7 独立生成的微型应用测试样本，参数为 Ultra hw_version=0、sd_req=0x100、application_version=1、CRC boot validation，使用上游共享兼容密钥签名。

12 字节 BIN 仅含测试向量表和虚拟指令，**不能刷入设备**。固定样本用于防止本项目打包器与校验器同时犯同一种格式错误，不是可发布固件。
