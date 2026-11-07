#pragma once
#include <Arduino.h>
static const char kCodexUsagePage[] PROGMEM=R"HTML(<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Codex 用量</title>
<style>*{box-sizing:border-box}body{font:16px system-ui;max-width:680px;margin:auto;padding:18px;background:#edf4fa;color:#111}button{font:inherit;padding:11px;margin:4px 4px 4px 0;background:white;border:1px solid #abc;border-radius:8px}a{color:#367dc4}p{line-height:1.7}.card{position:relative;background:#fff;padding:18px;margin:18px 0;border-radius:14px}.label{font-size:16px;padding-right:80px;min-height:38px;margin:0}.percent{position:absolute;top:18px;right:18px;font:bold 30px Arial}.bar{height:12px;border:1px solid black;padding:2px;margin:14px 0}.fill{height:6px;background:black;width:0}.reset{font-size:14px;margin:0}#error{color:#9a1b1b}.muted{font-size:14px}</style>
<h1>Codex 用量</h1><p>电脑开机后，每5分钟自动同步账户额度。电脑关机或断网时保留最后一次数据；15分钟未同步会标为缓存。</p>
<div><button id="syncClock">校准设备时间</button></div><p><a href="/">课程管理</a> · <a href="/weather">天气管理</a> · <a href="/calendar">日历管理</a></p>
<p id="status" role="status">正在读取…</p><p id="error" role="alert"></p>
<section class="card"><h2 class="label" id="primary-label">当前额度剩余</h2><span class="percent" id="primary-percent">--</span><div class="bar"><div class="fill" id="primary-fill"></div></div><p class="reset" id="primary-reset"></p></section>
<section class="card"><h2 class="label" id="secondary-label">其他额度剩余</h2><span class="percent" id="secondary-percent">--</span><div class="bar"><div class="fill" id="secondary-fill"></div></div><p class="reset" id="secondary-reset"></p></section>
<section class="card"><h2 class="label">Token 消耗 <span class="muted">今日</span></h2><div style="display:flex;gap:24px;align-items:center;flex-wrap:wrap"><div><div class="muted">今日总消耗</div><strong id="token-total" style="font-size:30px">--</strong></div><div><div class="muted">输入</div><strong id="token-input">--</strong></div><div><div class="muted">输出</div><strong id="token-output">--</strong></div></div><p id="token-history" class="muted">近 7 天：暂无 Token 统计数据</p><p id="token-source" class="reset">当前电脑同步接口只提供额度百分比，Token 数量尚未接入。</p></section>
<p id="stamp"></p><p class="muted">已到重置时间时显示“等待同步”，不会推算为100%。如果一直没有数据，请确认电脑同步脚本已启动，电脑和设备能通过同一热点互相访问。</p>
<script>
const $=id=>document.getElementById(id);let clockAttempted=false;
const fmt=t=>t?new Date(t*1000).toLocaleString('zh-CN',{timeZone:'Asia/Shanghai',hour12:false}):'未知';
async function write(path,body){const r=await fetch(path,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});if(!r.ok){const d=await r.json().catch(()=>({}));throw Error(d.error||'操作失败，HTTP '+r.status);}return r.json();}
async function syncClock(){await write('/api/clock',{epoch:Math.floor(Date.now()/1000)});}
function title(w,index){const n=w?.windowMinutes;return n===10080?'每周额度剩余':n&&n%60===0?(n/60)+' 小时额度剩余':n?n+' 分钟额度剩余':index===0?'当前额度剩余':'其他额度剩余';}
function render(d){
 const names={home:'首页',pet:'宠物',course:'课表',weather:'天气',calendar:'日历',codex:'Codex用量',delivery:'外卖',memo:'备忘录'};
 const t=d.tokens?.today;const count=n=>Number.isFinite(n)?(n/10000).toFixed(1)+' 万':'--';
 $('token-total').textContent=count(t?.totalTokens);$('token-input').textContent=count(t?.inputTokens);$('token-output').textContent=count(t?.outputTokens);$('token-source').textContent=!t?'等待读取 Codex 本地 Token 记录':d.tokens.complete===false?'统计不完整：近期有压缩日志未计入':'Token 数据由电脑端 Codex 会话日志汇总。';
 const days=Array.isArray(d.tokens?.history)?d.tokens.history:[];if(days.length)$('token-history').textContent='近 7 天：'+days.slice(-7).map(x=>x.date+' '+count(x.totalTokens)).join(' · ');
 $('status').textContent=(!d.hasData?'等待电脑首次同步':!d.clockValid?'缓存 · 设备待校时':d.stale?'缓存 · 等待电脑同步':'已同步')+' · 屏幕显示：'+(names[d.page]||'未知');
 ['primary','secondary'].forEach((key,i)=>{const w=d[key]||{},known=w.remainingPercent!=null&&!w.expired;
  $(key+'-label').textContent=title(w,i);$(key+'-percent').textContent=known?Math.round(w.remainingPercent)+'%':'--';
  $(key+'-fill').style.width=(known?Math.max(0,Math.min(100,w.remainingPercent)):0)+'%';
  $(key+'-reset').textContent=w.expired?'已到重置时间，等待同步':'重置：'+fmt(w.resetsAt)+'（北京时间）';
 });$('stamp').textContent='最后成功同步：'+(d.hasData?fmt(d.fetchedAt):'尚无');
}
async function load(){try{const r=await fetch('/api/codex',{cache:'no-store'});if(!r.ok)throw Error(r.status===404?'请先 Upload 新版固件，当前设备还没有用量页':'读取失败，HTTP '+r.status);const d=await r.json();if(!d.clockValid&&!clockAttempted){clockAttempted=true;await syncClock();return load();}render(d);$('error').textContent='';}catch(e){$('error').textContent=e.message;}}
$('syncClock').onclick=async()=>{try{await syncClock();await load();}catch(e){$('error').textContent=e.message;}};
load();setInterval(load,30000);
</script><script src="/static/screen-navigation.js" defer></script></html>)HTML";
