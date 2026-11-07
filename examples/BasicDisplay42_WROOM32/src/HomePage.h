#pragma once
#include <Arduino.h>

static const char kHomePage[] PROGMEM = R"HTML(<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>图片轮播首页</title>
<style>*{box-sizing:border-box}body{margin:0;background:#eaf3fc;color:#243d56;font:16px/1.7 system-ui,"Microsoft YaHei",sans-serif}main{max-width:650px;margin:auto;padding:20px 16px}h1{font-size:26px;margin:0 0 12px}.card{background:#fff;border-radius:17px;padding:20px;margin:16px 0}p{margin:10px 0}.muted{color:#647b91;font-size:14px}a{color:#367dc4}</style></head><body><main>
<h1>图片轮播首页</h1><section class="card"><h2>DE BRUYNE 17</h2>
<p>这是独立的第 1 页，宠物保留在第 2 页。</p>
<p>首页包含之前确认的六张插画。停留在首页时每 60 秒换一张，显示北京时间、日期和 Wi-Fi 状态。</p>
<p>切到其他页面后停止换图；回首页时保留上张插画，再重新计时 60 秒。</p>
<p class="muted">上方点“1 · 首页”才会切换墨水屏。打开本网页、读取状态不会自动切屏。</p></section>
<section class="card"><h2>断网时也能看</h2><p>插画存放在设备里，不依赖手机或电脑持续在线。首页会提示 WiFi 未连接。</p>
<p class="muted">校时后断网仍继续走时；断电重启后需要重新校时，可在<a href="/weather">天气管理页</a>点“校准设备时间”。</p></section>
</main><script defer src="/static/screen-navigation.js"></script></body></html>)HTML";
