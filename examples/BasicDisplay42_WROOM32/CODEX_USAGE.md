# Codex 用量：电脑自动同步

本功能显示 Codex 账户额度，包含短窗口和每周窗口的剩余百分比、进度条、北京时间重置时间及最后同步时间。不是对当前聊天剩余上下文的估算。

## 使用

1. 在本示例的 esp32dev 环境 Upload。Monitor 应打印 `Codex usage page: http://实际IP:81/codex`。
2. 电脑和 ESP32 连接同一可互访的热点；电脑还需要能连接 Codex 服务。
3. 打开 `http://实际IP:81/codex`，点击“屏幕显示用量”。如果设备 IP 改变，修改本示例 `tools/codex-sync.json` 的 device 地址。
4. 电脑后台同步每5分钟一次，Windows当前用户登录后自动启动。首次等待一次查询和保存；手机网页只读取设备缓存，不需要手机登录Codex。

按键1下一页、按键2上一页，循环课表→天气→日历→Codex用量。同步新数据不会抢占当前页面。旧电脑 page_switcher.py 应停用。

## 同步与数据

- tools/codex_usage_sync.py 使用本机 Codex `app-server` 的 `initialize`/`initialized` 和只读 `account/rateLimits/read`。复用现有登录，不读取或复制auth.json，不创建对话，不调用模型，也不消耗重置券或购买额度。
- 优先读取 rateLimitsByLimitId.codex，兼容旧 rateLimits；usedPercent 表示已用，剩余为100减已用并限制0～100。窗口时长按接口返回显示，不把所有账户都写死为5小时。缺失额度显示未知，不当成0或100。
- 发给ESP32的字段只有额度窗口、方案、读取时间。没有账号ID、邮箱、登录令牌、充值信息。ESP32字段白名单验证后存独立 nekocodex/usage NVS。
- 电脑每5分钟查询并POST /api/codex；连续失败下一次重试，旧数据保留。电脑关机或网络断开不会更新；15分钟未同步标记缓存。重置时间到了但没新数据时，该窗口显示“--／等待同步”，不会自己算成100%。
- NTP和原有网页时钟保持共用；首次电脑同步发现设备未校时，会用电脑时间调用 /api/clock。请保证电脑系统时间正确。断电恢复读取旧用量，未校时先标缓存；校时后按实际年龄判断。
- 目前只显示 codex 额度桶，其他模型专属桶、API账单、充值积分和重置券不在本页。

官方协议：https://learn.chatgpt.com/docs/app-server （account/rateLimits/read）。

## 查看状态与停用

- GET /api/codex：缓存、stale、clockValid、每窗expired和remainingPercent。
- 日志：tools/codex-sync.log（仅通用成功／错误，不记录原始账户回复）。
- 手动检查读取：`python tools/codex_usage_sync.py --read-only`。
- 手动同步一次：`python tools/codex_usage_sync.py --once`。若后台已运行，单实例锁会让第二份退出。
- 停用：把tools/codex-sync.json中的enabled改为false，运行中的脚本会在当前网络请求结束后停止；后续登录也不查询。再设true后，运行tools/start-codex-sync.vbs或下次登录恢复。
- Windows自启动入口是当前用户Startup中的“NekoPaw Codex Usage.vbs”，它只启动同名工具。电脑必须实际登录Windows，睡眠期间不执行；无需保持Codex应用窗口打开，但本机Codex需保持登录。

## 修改范围

新增src/CodexUsageManager.h、CodexUsagePage.h及tools下同步工具；修改main.cpp、CourseManager.h、CoursePage.h、WeatherPage.h、CalendarPage.h。原五个文件备份在codex-usage-backup-时间戳。没有改Wi-Fi密码、天气时点、课表内容、日历数据和分区。

代码编译和网页模拟不能代替ESP32写入缓存、断电恢复、实屏及实际开机自启动测试。本次不会自动Upload或Erase Flash。
