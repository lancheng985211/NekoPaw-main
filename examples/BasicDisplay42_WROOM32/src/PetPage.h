#pragma once
#include <Arduino.h>

static const char kPetPage[] PROGMEM=R"HTML(<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>我的萨摩耶</title>
<style>
:root{--blue:#4d94de;--ink:#243d56;--muted:#647b91}*{box-sizing:border-box}body{margin:0;background:#eaf3fc;color:var(--ink);font:16px/1.6 system-ui,"Microsoft YaHei",sans-serif}main{max-width:650px;margin:auto;padding:20px 16px 36px}h1{font-size:26px;margin:3px 0 9px}h2{font-size:20px;margin:0}p{margin:10px 0}.intro,.muted,small{color:var(--muted)}.intro,.muted{font-size:14px}.card{background:white;border-radius:17px;padding:20px;margin:16px 0;box-shadow:0 3px 15px #acc3d51c}.name{font-size:30px;line-height:1.3;margin:0 0 7px;overflow-wrap:anywhere}.action{font-size:18px;font-weight:600;margin:5px 0}.stats{display:flex;gap:24px;flex-wrap:wrap;border-top:1px solid #edf1f6;margin-top:17px;padding-top:13px;font-size:14px}.stats b{font-size:19px;margin:0 5px}.status{font-size:14px;white-space:pre-wrap;overflow-wrap:anywhere}.notice{color:#946022}.error{color:#a83434}.success{color:#26754d}.actions{display:flex;gap:8px;flex-wrap:wrap;margin-top:14px}button,input{font:inherit;border:1px solid #cedce9;border-radius:10px;padding:11px;background:white;color:var(--ink);max-width:100%}button{cursor:pointer}button:disabled{opacity:.45;cursor:wait}button:focus-visible,input:focus-visible{outline:2px solid var(--blue);outline-offset:2px}.primary{background:var(--blue);border-color:var(--blue);color:white}.secondary{background:#edf5fc}.small{font-size:14px;padding:8px 12px}label{display:block;margin-top:14px}input{display:block;width:100%;margin:7px 0 4px}.rename{display:flex;gap:9px;align-items:end;flex-wrap:wrap}.rename label{flex:1;min-width:160px}.rename button{margin-bottom:4px;flex-shrink:0}a{color:#367dc4;text-underline-offset:3px}.links{font-size:14px;line-height:2;overflow-wrap:anywhere}.links a{display:inline-block;margin-right:9px}.error-row{display:flex;gap:9px;align-items:center;flex-wrap:wrap}#readError{flex:1;min-width:0}#feedback{min-height:24px;white-space:pre-wrap;overflow-wrap:anywhere}[hidden]{display:none!important}
</style></head><body><main>
<h1>我的萨摩耶</h1>
<p class="intro">宠物是第2页，图片轮播首页是第1页，上方可切换课表、天气和其他功能。小狗会自己溜达、蹦跳和歇息。</p>
<div class="actions"><button id="retry" class="small">重新读取小狗状态</button></div>
<p id="connection" class="status" role="status">正在连接设备…</p>
<div class="error-row"><p id="readError" class="error status" role="alert"></p></div>
<section class="card" aria-label="小狗状态">
 <h2 id="petNameHeading" class="name">我的小狗</h2><p id="actionText" class="action">正在读取…</p>
 <p id="screenStatus" class="muted"></p><p id="warning" class="status notice" hidden></p>
 <div class="stats"><span>摸摸<b id="patCount">0</b>次</span><span>喂食<b id="foodCount">0</b>次</span></div>
 <div class="actions"><button id="togglePause" data-command="pause" disabled>暂停活动</button><button class="primary" data-command="pat" disabled>摸摸小狗</button></div>
 <div class="actions"><button data-command="jump" disabled>跳一跳</button><button data-command="roll" disabled>打个滚</button><button data-command="bark" disabled>汪汪叫</button><button data-command="eat" disabled>喂狗粮</button></div>
 <p class="muted">汪汪叫用画面气泡呈现。切到其他屏幕页后，小狗会暂停画面活动。</p>
</section>
<section class="card"><h2>给小狗取名字</h2>
 <form id="nameForm" novalidate><div class="rename"><label for="petName">小狗名字<input id="petName" name="name" maxlength="8" autocomplete="off" placeholder="例如：雪球" disabled></label><button id="saveName" class="primary" type="submit" disabled>保存名字</button></div></form>
 <small>1～8 个中文或英文字符，最多 24 字节。</small>
</section>
<p id="feedback" role="status"></p>
<p id="refreshInfo" class="muted">动作间用局刷，约每30秒趁休息清一次残影。清屏时仍会闪一下。</p>
<p class="muted">手机与 ESP32 连接同一 Wi-Fi。自动活动无需手机一直打开，也无需反复按键。</p>
</main><script>
'use strict';
(() => {
 const $=id=>document.getElementById(id),nameInput=$('petName');
 const pageNames={home:'首页',course:'课表',weather:'天气',calendar:'日历',codex:'Codex 用量',delivery:'外卖',memo:'备忘录',pet:'宠物'};
 const actionNames={idle:'停下来看看',rest:'歇一会儿',walk:'自己溜达中',jump:'开心蹦跳',roll:'打个滚',bark:'汪汪叫',eat:'吃狗粮'};
 let model=null,nameDirty=false,writing=false,refreshPromise=null,pollTimer=null,writeVersion=0,disposed=false;
 const count=value=>Number.isFinite(Number(value))?Math.max(0,Math.floor(Number(value))):0;
 function feedback(message,error=false){$('feedback').textContent=message;$('feedback').className=error?'error':'success';}
 function availability(){
  document.querySelectorAll('button[data-command],#saveName').forEach(b=>b.disabled=writing||!model);
  nameInput.disabled=writing||!model;$('retry').disabled=writing||!!refreshPromise;
 }
 function clearPoll(){if(pollTimer!==null){clearTimeout(pollTimer);pollTimer=null;}}
 function schedulePoll(){
  clearPoll();if(disposed||writing||refreshPromise||document.hidden)return;
  pollTimer=setTimeout(()=>{pollTimer=null;refresh();},3000);
 }
 async function api(path,body){
  const controller=new AbortController(),timeout=setTimeout(()=>controller.abort(),8000);
  try{
   const response=await fetch(path,{cache:'no-store',signal:controller.signal,...(body===undefined?{}:{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})});
   if(response.status===404)throw Error('请先上传宠物版固件，再重新读取。');
   let value;try{value=await response.json();}catch{throw Error('设备返回异常，请重试。');}
   if(!response.ok)throw Error(value.error||'请求失败，HTTP '+response.status);
   return value;
  }catch(error){
   if(error.name==='AbortError')throw Error('连接设备超时，请检查 Wi-Fi 后重试。');
   if(error instanceof TypeError)throw Error('未连接到设备，请检查 Wi-Fi 后重试。');
   throw error;
  }finally{clearTimeout(timeout);}
 }
 function render(data){
  model=data;
  const name=typeof data.name==='string'?data.name:'我的小狗';
  $('petNameHeading').textContent=name;
  if(!nameDirty&&document.activeElement!==nameInput)nameInput.value=name;
  $('actionText').textContent=data.paused?'已暂停活动':typeof data.actionText==='string'&&data.actionText?data.actionText:(actionNames[data.action]||'自动陪伴');
  $('screenStatus').textContent='当前屏幕：'+(pageNames[data.page]||'其他页面')+(data.visible?(data.paused?' · 点击“继续活动”恢复':' · 小狗正在自动活动'):' · 切到宠物页后开始活动');
  $('patCount').textContent=String(count(data.patCount));$('foodCount').textContent=String(count(data.foodCount));
  $('togglePause').dataset.command=data.paused?'resume':'pause';$('togglePause').textContent=data.paused?'继续活动':'暂停活动';
  const warnings=[];
  if(typeof data.warning==='string'&&data.warning)warnings.push(data.warning);
  if(data.storageReady===false&&!warnings.length)warnings.push('设备暂时无法保存名字与计数，请稍后重试。');
  if(data.clockValid===false)warnings.push('北京时间待校准。');
  $('warning').textContent=warnings.join('\n');$('warning').hidden=!warnings.length;
  const interval=Number(data.targetIntervalMs);
  const timing=(Number.isFinite(interval)&&interval>0?'目标动作间隔约 '+(interval/1000).toFixed(interval%1000?1:0)+' 秒。':'');
  $('refreshInfo').textContent=timing+(data.refreshPolicy==='rest-cleanup-v2'?'动作间用局刷；约每30秒趁休息清残影，持续活动时最迟60秒清一次。清屏仍会闪一下，残影需要实屏观察。':'墨水屏会周期清屏，真实动作速度以实屏表现为准。');
  $('connection').textContent='已连接设备';$('readError').textContent='';
 }
 function refresh(){
  if(disposed)return Promise.resolve();
  if(refreshPromise)return refreshPromise;
  clearPoll();const version=writeVersion;
  refreshPromise=(async()=>{
   try{const data=await api('/api/pet');if(data.service!=='nekopaw-pet-v1')throw Error('宠物服务版本不匹配，请先上传宠物版固件。');if(version===writeVersion)render(data);}
   catch(error){if(version===writeVersion){$('connection').textContent=model?'设备暂时未连接，保留上次状态':'尚未连接设备';$('readError').textContent=error.message;}}
   finally{refreshPromise=null;availability();schedulePoll();}
  })();
  availability();return refreshPromise;
 }
 async function send(path,body,message){
  if(writing)return;writing=true;writeVersion+=1;clearPoll();availability();feedback('正在发送…');
  try{
   const pending=refreshPromise;if(pending)await pending;
   await api(path,body);
   if(body.command==='rename'){nameDirty=false;nameInput.value=body.name;}
   feedback(message);
   await refresh();
  }catch(error){feedback(error.message,true);}
  finally{writing=false;availability();schedulePoll();}
 }
 function validateName(value){
  if(/[\u0000-\u001f\u007f]/u.test(value))throw Error('名字不能包含控制符。');
  const name=value.trim(),length=Array.from(name).length;
  if(length<1||length>8)throw Error('名字须为 1～8 个中文或英文字符。');
  if(!/^[\x20-\x7e\u3400-\u4dbf\u4e00-\u9fff\uf900-\ufaff]+$/u.test(name))throw Error('名字只能使用中文或可见英文字符，不能含控制符或表情。');
  if(new TextEncoder().encode(name).length>24)throw Error('名字最多 24 个 UTF-8 字节。');
  return name;
 }
 nameInput.addEventListener('input',()=>{nameDirty=true;if(!writing)feedback('名字已修改，点击“保存名字”后生效。');});
 $('nameForm').addEventListener('submit',event=>{
  event.preventDefault();if(writing)return;
  try{const name=validateName(nameInput.value);send('/api/pet',{command:'rename',name},'名字已保存到设备。');}catch(error){feedback(error.message,true);}
 });
 document.querySelectorAll('[data-command]').forEach(button=>button.addEventListener('click',()=>send('/api/pet',{command:button.dataset.command},button.dataset.command==='pause'?'已暂停小狗活动。':button.dataset.command==='resume'?'已恢复自动活动。':'已发送互动，小狗会在宠物页回应。')));
 $('retry').addEventListener('click',()=>refresh());
 document.addEventListener('visibilitychange',()=>{if(document.hidden)clearPoll();else refresh();});
 window.addEventListener('pagehide',()=>{disposed=true;clearPoll();});
 refresh();
})();
</script><script src="/static/screen-navigation.js" defer></script></body></html>)HTML";
