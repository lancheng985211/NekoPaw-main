# 第五页：手机外卖通知

新增DeliveryManager.h、DeliveryPage.h，修改main.cpp、CourseManager.h及四个原管理页面入口。循环顺序课表→天气→日历→Codex用量→外卖，GPIO32下一页，GPIO33上一页。旧四项数据与程序分区不改。

## 使用顺序

1. PlatformIO esp32dev Upload此示例。串口会输出Delivery page: http://实际IP:81/delivery。
2. 项目根目录phone-bridge/NekoPaw-Delivery.apk安装到荣耀手机，按phone-bridge/README先测试读取通知。
3. 确认有实际外卖文字后，填设备地址、测试连接、开启自动同步。
4. 手机和ESP32在同一可互访网络；打开/delivery点击屏幕显示外卖。后台通知到达只更新缓存，不抢占屏幕。

## 接口与存储

- GET /api/delivery：service=nekopaw-delivery-v1、最新状态、明确ETA、手机通知时间、设备收到时间、当前页、时钟状态、较早通知标记。
- POST /api/delivery：schema=1、source=phone-notification、eventId=64位小写hex、capturedAt=Unix秒、state枚举、eta为空或HH:MM、test=false。设备从state派生显示文案，不接受通知原文、地址、电话等内容。
- 完全重复eventId直接确认不写NVS；更旧时间返回409保留新状态；时间超前超过5分钟返回422，格式错误400，保存失败507。手机收到409后不重复发送旧事件。
- 独立Preferences命名空间nekodelivery，键latest。先验证、保存成功再替换内存。重启加载最后通知；未校时标较早/待校时。
- 四小时前的状态只标“较早通知”，不推断订单取消、完成或新进度。无ETA则标未提供。数据是最近一条通知，不是完整订单轨迹或实时定位。

原六个文件安装前备份到delivery-backup-时间戳。恢复可复制六个备份回src再Build/Upload，无需删除新文件或EraseFlash。上传、通知监听、设备断电恢复和实屏显示需要用户设备验证。
