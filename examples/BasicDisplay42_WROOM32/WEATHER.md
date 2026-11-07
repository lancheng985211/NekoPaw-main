# 长沙天气：每小时更新、逐小时预报

北京时间每个整点（00:00至23:00）自动获取一次天气。首次联网无缓存或旧缓存缺逐小时字段时立即获取；错过整点断网重连只补最新一次，不回放历史请求。失败每10分钟重试，新整点或重连可提前重试；未校时不发请求。请求完成可能晚于整点，更新时间记录成功获取时刻，不保证整点零秒完成。

长沙坐标28.19874/112.97087；Open-Meteo current/daily接口增加hourly temperature_2m、weather_code、precipitation_probability，forecast_hours=48、forecast_days=7，timezone=Asia/Shanghai。缓存7天日预报、48小时预报；屏幕显示未来三天和严格晚于当前时刻的接下来8个整点。08:00或08:35时显示09:00～16:00，跨零点正确显示下一天，整点会自动推进小时格。断网沿用具体时间戳对应数据，超出缓存显示暂无，不重标旧小时。旧版日缓存可读，获取失败保留旧缓存，不清空其他数据。

天气页采用已确认的右侧天气/今日高低温组合、8个小时格、3天预报，保留天气03/06页码及Open-Meteo署名。天气页顶部显示长沙与北京时间，分钟走时局刷页眉。字体与预览有设备差异，屏幕实际显示为准。

管理页仍 http://设备IP:81/weather，共享6页导航保留。GET/api/weather新增service=weather-hourly-v3、updateSchedule=hourly、now及nextHours（8条，missing明确表示暂无）；hourly为48条原缓存，current.time仍保留接口原始北京时间。NVS仍nekoweather/cache，只有成功校验的新数据才保存；其他功能、页码设置、Wi-Fi配置、分区均不改。

HTTPS仍验证根证书；后台任务只取数据，主线程画屏。固件以本工程esp32dev进行USB Upload，保持已有单应用分区，不要Erase Flash。手机/电脑与设备同一网络，IP以串口为准。

数据源文档：https://open-meteo.com/en/docs 。实机仍需验证整点、断网重连、跨小时显示和NVS恢复。
