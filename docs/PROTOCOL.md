# 学习与卡片管理命令

USB CDC 与 BLE NUS 使用相同帧：`11 EF | cmd:u16 | status:u16 | length:u16 | header_LRC:u8 | payload | payload_LRC:u8`。多字节字段为大端；两段 LRC 分别使前 9 字节之和、payload 加末尾 LRC 之和在模 256 下等于 0。payload 最大 512 字节。支持分片与连续帧，每通道最多排队 3 帧；建议一次请求等待一次响应，超时后查询状态，不自动重试修改命令。

新命令成功状态 `0x68`；参数错误 `0x60`、模式错误 `0x66`、无此命令 `0x67`、有场／忙 `0x6A`、存储错误 `0x6B`、没有可用上下文 `0x6C`。历史模式命令 1001/1002 使用成功状态 0。

## 命令表

所有卡槽为 0..7，`255` 表示无槽／全部（取决于命令）。

| cmd | 请求 payload | 成功响应 |
| --- | --- | --- |
| 1000 | 空 | major:u8,minor:u8，当前 0,1；状态 0x68 |
| 1001 | mode:u8，0=模拟，1=读卡 | 空，状态 0；有场时拒绝切模式 |
| 1002 | 空 | mode:u8，状态 0 |
| 1003 | slot:u8 | 空；排队手动切卡，随后检查 1100 的 current/pending |
| 1005 | slot:u8,type:u8 | 初始化指定槽对应频率；type 1=EM410X，2=Mini，3=1K，4=2K，5=4K |
| 1200 | EM410X ID 5 字节 | 空；只在读卡模式更新当前槽并保存 |
| 1201 | first_block:u8,count:u8,data[count×16] | 空；只在读卡模式更新当前 MF1 的 RAM；count 1..31，最后执行 1107 |
| 1202 | 空 | current:u8 + 8×[enabled:u8,hf_type:u8,lf_type:u8] |
| 1010 | 空 | 保存后返回 0x68，约 200 ms 后重启进入现有官方 DFU；有场／保存失败拒绝 |
| 1017 | 空 | 项目版本标识 ASCII 字符串 |
| 1033 | 空 | device_model:u8，Ultra=0 |
| 1035 | 空 | 已实现命令编号列表，每项 u16 大端 |
| 1100 | 空 | 30 字节状态，见下表 |
| 1101 | 12 字节配置 | 空；更新 RAM，需保存 |
| 1102 | utc_seconds:u32,timezone_minutes:i16 | 空；范围 UTC 2020..2100，时区 -720..840 分钟 |
| 1103 | slot:u8,source:u8 | 空；source 0=当前环境，1=最近 20 秒内 HF 会话 |
| 1104 | slot:u8，255=全部 | 空；清除对应样本并保存；有场拒绝 |
| 1105 | 空 | slot,score,margin,reason,evidence,observations 各 u8 + 8 个槽评分 |
| 1106 | 空 | 0..32 个样本元数据，每项 13 字节 |
| 1107 | 空 | 空；保存模型、当前卡片 RAM 和卡槽配置；有场拒绝 |
| 1108 | 空 | 12 字节配置 |

卡片导入流程：1002 记下原模式 → 1001 进入读卡模式 → 1005 建立槽数据 → 1003 排队选槽 → 轮询 1100 确认当前槽已切换 → 1200／分批 1201 → 1107 保存 → 1001 恢复原模式。失败时保持读卡模式，重试完整导入。初始化会覆盖目标频率已有数据，HF/LF 的另一种卡可保留。

alpha.2 将私人导入／查询命令从 alpha.1 的 1006/1007/1008 移到 1200/1201/1202，避免官方工具的启用卡槽／昵称命令误写卡片。需要使用同版本 CLI。身份查询与 1010 用于升级工具基本识别，并不表示完整兼容现行官方卡片管理协议。1010 获得成功响应后，固件拒绝后续命令和按钮／自动切卡，处理通信发送并重启；若在保存期间重新有场，重新检查后报告忙。当前官方固件的同编号命令立即重启、通常没有响应，CLI 的 `enter-dfu --official` 仅发送请求，不声称已确认进入 DFU。

## 配置（1101/1108）

| 偏移 | 类型 | 字段／有效范围 |
| --- | --- | --- |
| 0 | u8 | mode：0 OFF、1 OBSERVE、2 AUTO |
| 1 | u8 | min_score：40..100，默认 70 |
| 2 | u8 | margin：5..50，默认 12 |
| 3 | u8 | min_observations：1..20，默认 2 |
| 4..6 | 3×u8 | BLE／reader／time 权重，合计 100；BLE+reader 必须非零，默认 60/30/10 |
| 7 | u8 | reserved，必须 0 |
| 8 | u16 | scan_period_ms：最多 60000，必须至少 window+500，默认 8000 |
| 10 | u16 | scan_window_ms：200..5000，默认 1200 |

## 状态（1100）

| 偏移 | 类型 | 字段 |
| --- | --- | --- |
| 0 | u8 | 协议版本 1 |
| 1..8 | 8×u8 | mode,current,recommendation,score,margin,reason,evidence,sample_count |
| 9..15 | 7×u8 | time_valid,hf_field,lf_field,pending_slot,dirty,scan_active,save_error |
| 16 | u32 | 当前 UTC；time_valid=0 时无效 |
| 20 | u32 | 本次运行自动切换成功数 |
| 24 | u32 | 来不及处理而丢弃的 HF 会话数 |
| 28 | u8 | 有记录且启用的卡槽 bitmask（最终加载仍会校验记录） |
| 29 | u8 | 推荐样本的重复反馈数 |

`reason`：0=OFF，1=无匹配样本，2=无上下文，3=分数不足，4=候选冲突，5=反馈不足，6=手动保护期，7=有场锁定，8=切换冷却，9=可切换，10=保持当前槽，11=目标加载失败。

`evidence` bit 0=有效蓝牙线索，bit 1=有效读卡器线索，bit 2=时间参与评分。reason 为 READY 在 OBSERVE 模式下只表示建议达标；AUTO 模式还受场、模式和保护期约束。dirty/save_error 应在成功保存后清除。

## 样本元数据（1106）

每项依次为 `slot:u8,observations:u8,beacon_count:u8,reader_count:u8,field:u8,time_valid:u8,weekday:u8,minute:u16,order:u32`。星期从周一 0 开始，minute 为本地当日分钟。没有提供广播原文、密钥、卡片 dump 或模型原始标识导出命令。
