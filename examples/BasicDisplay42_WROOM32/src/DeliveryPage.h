#pragma once
#include <Arduino.h>
static const char kDeliveryPage[] PROGMEM=R"HTML(<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>外卖通知</title>
<style>*{box-sizing:border-box}body{font:16px system-ui;max-width:680px;margin:auto;padding:18px;background:#edf4fa;color:#111}button{font:inherit;padding:11px;margin:4px 4px 4px 0;background:white;border:1px solid #abc;border-radius:8px}a{color:#367dc4}p{line-height:1.7}.card{background:#fff;border-radius:14px;padding:22px;margin:20px 0}h2{font-size:28px}#error{color:#9a1b1b}.muted{font-size:14px}</style>
<h1>美团外卖通知</h1><p>手机助手读取选定来源的外卖通知后，经同一热点发给设备。此页显示最近一条已收到的状态。</p>
<div></div><p><a href="/">课程管理</a> · <a href="/weather">天气</a> · <a href="/calendar">日历</a> · <a href="/codex">Codex用量</a></p>
<p id="error" role="alert"></p><section class="card"><p id="status">正在读取…</p><h2 id="state">等待手机通知</h2><p id="eta"></p><p id="captured"></p><p id="received"></p></section>
<p class="muted">只显示手机实际发来的通知，不推测骑手位置、距离或订单后续进度。通知未给出预计时间时显示未提供；多订单时当前只保留最近通知。四小时前的通知会标为较早通知。手机断网或后台受限时保留设备缓存。</p>
<script>
const $=id=>document.getElementById(id),fmt=t=>t?new Date(t*1000).toLocaleString('zh-CN',{timeZone:'Asia/Shanghai',hour12:false}):'待校时';
async function load(){try{const r=await fetch('/api/delivery',{cache:'no-store'});if(!r.ok)throw Error(r.status===404?'请先Upload外卖版固件':'读取失败，HTTP '+r.status);const d=await r.json();const names={home:'首页',pet:'宠物',course:'课表',weather:'天气',calendar:'日历',codex:'Codex用量',delivery:'外卖',memo:'备忘录'};$('status').textContent=(d.hasData?(d.older?'较早通知／待校时':'已收到手机通知'):'等待手机首次发送')+' · 屏幕显示：'+(names[d.page]||'未知');$('state').textContent=d.statusText||'等待手机通知';$('eta').textContent=!d.hasData?'请在手机助手中授权、检查通知文字并开启同步':d.state==='delivered'||d.state==='cancelled'?'本次通知已结束':d.eta?'预计送达：'+d.eta:'通知未提供预计送达时间';$('captured').textContent=d.hasData?'手机通知时间：'+fmt(d.capturedAt):'';$('received').textContent=d.hasData?'设备收到时间：'+fmt(d.receivedAt):'';$('error').textContent='';}catch(e){$('error').textContent=e.message;}}
load();setInterval(load,15000);
</script><script src="/static/screen-navigation.js" defer></script></html>)HTML";
