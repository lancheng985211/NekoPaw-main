#pragma once
#include <Arduino.h>
static const char kCalendarPage[] PROGMEM=R"HTML(<!doctype html>
<html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>NekoPaw 月历</title>
<style>
*{box-sizing:border-box}body{font:16px system-ui;margin:auto;max-width:680px;padding:16px;background:#edf4fa;color:#111}h1{font-size:28px}button{font:inherit;padding:11px;margin:4px 4px 4px 0;background:#fff;border:1px solid #abc;border-radius:8px}p{line-height:1.7}a{color:#367dc4}.card{background:#fff;padding:14px;border-radius:14px;margin:16px 0}.heading{display:flex;align-items:center;justify-content:space-between;border-bottom:2px solid #000;padding-bottom:8px;margin-bottom:10px}.heading h2{margin:0}.week,.grid{display:grid;grid-template-columns:repeat(7,minmax(0,1fr))}.week{text-align:center;font-size:13px;margin-bottom:8px}.grid{border-top:1px solid #000;border-left:1px solid #000}.cell{position:relative;min-height:60px;border-right:1px solid #000;border-bottom:1px solid #000;text-align:center;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:6px;min-width:0}.number{font:20px Arial}.label{font-size:11px;white-space:nowrap}.badge{position:absolute;top:2px;right:2px;font-size:10px;border:1px solid currentColor;line-height:12px}.today{background:#000;color:#fff}.muted{font-size:13px}#error{color:#9a1b1b}#details{font-weight:700;margin-bottom:0}#note{font-size:13px;margin-top:4px}
</style><h1>日历</h1><p>北京时间每天跨零点自动换日、换月。农历和节气已存入设备，断网也能显示；断电重启后需要校时。</p>
<div><button id="syncClock">校准设备时间</button></div>
<p><a href="/">课程管理</a> · <a href="/weather">长沙天气</a></p>
<p id="status" role="status">正在读取设备日期…</p><p id="error" role="alert"></p>
<section class="card" id="calendar" hidden><div class="heading"><h2 id="month"></h2><span>日历 05/08</span></div><div class="week"><span>日</span><span>一</span><span>二</span><span>三</span><span>四</span><span>五</span><span>六</span></div><div class="grid" id="grid"></div><p id="details"></p><p id="note"></p></section>
<p class="muted">已收录 2026—2030 年农历、全部二十四节气；休／班安排目前收录 2026 年。其他年份会提示未收录。日历的休／班不会自动修改学校课表。</p>
<p class="muted">数据来源：<a href="https://www.hko.gov.hk/sc/gts/time/conversion.htm" target="_blank" rel="noopener">香港天文台</a>、<a href="https://www.beijing.gov.cn/cs/gncs/zcwj/202603/t20260327_4568275.html" target="_blank" rel="noopener">国务院 2026 年放假通知</a>。</p>
<script>
const $=id=>document.getElementById(id);let clockAttempted=false;
async function write(path,body){const r=await fetch(path,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});if(!r.ok){const d=await r.json().catch(()=>({}));throw Error(d.error||'操作失败，HTTP '+r.status);}return r.json();}
async function syncClock(){await write('/api/clock',{epoch:Math.floor(Date.now()/1000)});}
function render(d){
 if(!d.clockValid){$('calendar').hidden=true;$('status').textContent='等待校准北京时间，请检查手机或电脑时间后点击校准。';return;}
 $('calendar').hidden=false;$('month').textContent=`${d.year}年 ${d.month}月`;
 const names=['首页','宠物','课表','天气','日历','Codex用量','外卖','备忘录'],index={home:0,pet:1,course:2,weather:3,calendar:4,codex:5,delivery:6,memo:7};
 $('status').textContent='北京时间已校准 · 屏幕当前显示：'+(names[index[d.page]]||'未知')+(!d.holidayKnown?' · 当年休班安排未收录':'')+(!d.supported?' · 农历及节气数据未收录':'');
 const grid=$('grid');grid.replaceChildren();
 for(let i=0;i<d.rows*7;i++){
  const cell=document.createElement('div');cell.className='cell';const item=d.days[i-d.firstWeekday];
  if(item){cell.dataset.day=item.day;if(item.day===d.day)cell.classList.add('today');
   const n=document.createElement('div');n.className='number';n.textContent=item.day;cell.append(n);
   const label=document.createElement('div');label.className='label';label.textContent=item.label||'\u00a0';cell.append(label);
   if(item.badge){const b=document.createElement('span');b.className='badge';b.textContent=item.badge;cell.append(b);}
  }grid.append(cell);
 }
 $('details').textContent=`${d.month}月${d.day}日 周${'日一二三四五六'[d.weekday]}`+(d.details?' · '+d.details:'');$('note').textContent=d.note||d.lunar||'';
}
async function load(){try{const r=await fetch('/api/calendar',{cache:'no-store'});if(!r.ok)throw Error(r.status===404?'日历接口不存在，请先上传新版固件':'读取失败，HTTP '+r.status);const d=await r.json();if(!d.clockValid&&!clockAttempted){clockAttempted=true;await syncClock();return load();}render(d);$('error').textContent='';}catch(e){$('error').textContent='日历读取失败：'+e.message;}}
$('syncClock').onclick=async()=>{try{await syncClock();await load();}catch(e){$('error').textContent=e.message;}};

load();setInterval(load,60000);
</script><script src="/static/screen-navigation.js" defer></script></html>)HTML";
