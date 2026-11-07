#pragma once
#include <Arduino.h>
static const char kWeatherPage[] PROGMEM=R"HTML(<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>长沙天气</title><style>
*{box-sizing:border-box}body{font:16px/1.7 system-ui;max-width:680px;margin:auto;padding:18px;background:#edf4fa}section{background:#fff;padding:18px;margin:16px 0;border-radius:16px}button{font:inherit;padding:11px;border:1px solid #abc;border-radius:8px;background:white}a{color:#367dc4}h1{margin:12px 0}h2{font-size:21px}#status,#error{white-space:pre-wrap}#error{color:#963232}.hours{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:8px}.hour{min-width:0;padding:8px 2px;text-align:center;border:1px solid #cad9e6;border-radius:8px;font-size:12px}.hour strong{display:block;font-size:20px}.hour small{display:block}#forecast{padding-left:20px}.muted{font-size:13px;color:#526679}@media(min-width:620px){.hours{grid-template-columns:repeat(8,minmax(0,1fr))}}
</style></head><body><h1>长沙天气</h1><p>北京时间每个整点更新一次。断网保留缓存，重连后补更新一次。</p><button id="syncClock">校准设备时间</button><section><h2 id="current">读取中…</h2><p id="status"></p><p id="error" role="alert"></p><h2>未来8小时</h2><p class="muted">每小时一格，按当前北京时间显示接下来的8个整点；缺失的时段显示暂无。</p><div class="hours" id="hours"></div><h2>未来三天</h2><ul id="forecast"></ul></section><small>天气数据：<a href="https://open-meteo.com/" target="_blank" rel="noopener">Open-Meteo</a>。温度为天气模型数据；更新时间表示设备成功获取时间。</small>
<script>
const $=id=>document.getElementById(id);let clockAttempted=false,busy=false;
const fmt=t=>t?new Date(t*1000).toLocaleString('zh-CN',{timeZone:'Asia/Shanghai',hour12:false}):'尚无';
const description=c=>c===0?'晴':c<=2&&c>=1?'多云':c===3?'阴':c===45||c===48?'雾':c>=95?'雷雨':c>=71&&c<=77||c===85||c===86?'雪':c>=51&&c<=67||c>=80&&c<=82?'雨':'未知';
async function syncClock(){const r=await fetch('/api/clock',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({epoch:Math.floor(Date.now()/1000)})});if(!r.ok)throw Error('校时失败，请检查手机/电脑时间');}
function render(d){
 $('current').textContent=d.current?'长沙 '+Math.round(d.current.temperature)+'°C':'暂未获取天气';
 $('status').textContent=(d.online?'Wi-Fi 已连接':'离线')+' · '+(d.clockValid?'北京时间已校准':'等待校时')+'\n上次获取：'+fmt(d.fetchedAt)+'\n下次整点更新：'+fmt(d.nextUpdate)+(d.stale?'\n正在显示缓存或等待更新':'');
 $('error').textContent=d.error||'';$('hours').replaceChildren();
 const hours=Array.isArray(d.nextHours)?d.nextHours:[];
 for(const h of hours){
  const cell=document.createElement('div');cell.className='hour';cell.dataset.hour=h.time;
  const time=document.createElement('small');time.textContent=new Date(h.time*1000).toLocaleString('zh-CN',{timeZone:'Asia/Shanghai',month:'2-digit',day:'2-digit',hour:'2-digit',minute:'2-digit',hour12:false});
  const condition=document.createElement('div');condition.textContent=h.missing?'暂无':description(h.code);
  const temp=document.createElement('strong');temp.textContent=h.missing?'--':Math.round(h.temperature)+'°';
  const rain=document.createElement('small');rain.textContent=h.missing||h.rain==null?'降雨未知':'降雨 '+Math.round(h.rain)+'%';cell.append(time,condition,temp,rain);$('hours').append(cell);
 }
 if(!hours.length)$('hours').textContent=d.clockValid?'等待首次获取逐小时预报':'请先校准北京时间';
 $('forecast').replaceChildren();
 const now=Number(d.now),base=Number.isFinite(now)?now:Date.now()/1000;
 const today=new Date(base*1000+28800000).toISOString().slice(0,10);
 for(const x of (d.daily||[]).filter(x=>x.date>today).slice(0,3)){
  const row=document.createElement('li');row.textContent=x.date+' '+description(x.code)+' '+Math.round(x.min)+'～'+Math.round(x.max)+'°C 降雨 '+(x.rain==null?'未知':Math.round(x.rain)+'%');$('forecast').append(row);
 }
 if(!$('forecast').children.length)$('forecast').textContent='暂无未来日期预报';
}
async function load(){if(busy)return;busy=true;try{const r=await fetch('/api/weather',{cache:'no-store'});if(!r.ok)throw Error('读取失败，HTTP '+r.status);const d=await r.json();if(!d.clockValid&&!clockAttempted){clockAttempted=true;await syncClock();}render(d);}catch(e){$('error').textContent='无法连接设备：'+e.message;}finally{busy=false;}}
$('syncClock').onclick=async()=>{try{await syncClock();await load();}catch(e){$('error').textContent=e.message;}};load();setInterval(load,30000);
</script><script src="/static/screen-navigation.js" defer></script></body></html>)HTML";
