# DE BRUYNE 17 墨水屏桌面

基于 NekoPaw 的 ESP32 八页墨水屏项目。硬件为 ESP32 WROOM-32、GDEY042T81（4.2英寸、400×300黑白）和两个翻页按键。

## 当前功能

| 页面 | 内容 |
|---|---|
| 首页 | 六张插画每60秒轮换、北京时间、Wi-Fi状态 |
| 宠物 | 萨摩耶动画、改名、摸摸、喂食 |
| 课表 | 每天五行、课程周数、上课时间和地点、临时调课 |
| 天气 | 长沙天气、未来8小时和未来三天、每整点更新、离线缓存 |
| 日历 | 月历、农历、二十四节气及已收录节假日 |
| Codex | 电脑同步额度与Token统计，数量以万为单位保留一位小数 |
| 外卖 | 页面和接收接口保留；荣耀YOYO胶囊自动读取尚未解决 |
| 备忘录 | 最多8项、表情、可选目标时间和倒计时 |

GPIO32下一页，GPIO33上一页，首尾循环。管理网页也能切换。所有页面显示北京时间；校时后持续通电可离线走时，断电后需重新校时。

## 使用

1. VS Code安装PlatformIO，打开`examples/BasicDisplay42_WROOM32`。
2. 把`src/wifi_config.example.h`复制成`src/wifi_config.h`，填写Wi-Fi名称和密码。
3. 选择`esp32dev`执行Build，再接入ESP32执行Upload。普通更新不要Erase Flash，设备内原有课表等数据需要保留。
4. 从串口Monitor（115200）获取实际IP，手机与ESP32连同一Wi-Fi/热点，访问`http://设备IP:81/`。
5. 也可以安装`phone-manager/NekoPaw-Manager-1.0.0.apk`，填设备实际IP；App内刷新后可见八页切换按钮。

硬件引脚：SCLK18、MOSI23、CS5、DC17、RST16、BUSY4；按键32/33。沿用已验证接线，接口供电与屏幕转接板按实际硬件确认。

## 文件说明

- `examples/BasicDisplay42_WROOM32/src/`：八页固件、网页、数据模型与位图。
- `src/`：NekoPaw桥接库，保留HTTP接口兼容性。
- `phone-manager/`：已完成的Android管家源码和安装包。
- `examples/BasicDisplay42_WROOM32/tools/`：Codex同步与日历数据生成工具。
- `examples/BasicDisplay42_WROOM32/*.md`：各功能操作说明。

本版本仅包含八页固件、手机管家与同步工具，不包含旧示例、历史备份、构建缓存和外卖实验助手。Codex同步先将tools/codex-sync.example.json复制为codex-sync.json并填写设备IP，然后运行python tools/codex_usage_sync.py --once。后台启动脚本需要pythonw.exe可用，也可传入Python的完整路径。请避免重复运行同步进程。

## 来源与限制

基于[TruElysia02/NekoPaw](https://github.com/TruElysia02/NekoPaw)，保留原MIT许可证及作者署名。图片素材与代码授权范围见`NOTICE.md`。

电脑编译和模拟网页检查不代表所有硬件场景已实测；残影、按键、断电恢复需在实际设备上检查。外卖页不代表已经能够读取荣耀胶囊。
