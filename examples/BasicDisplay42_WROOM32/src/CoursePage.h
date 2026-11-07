#pragma once
#include <Arduino.h>
static const char kCoursePage[] PROGMEM = R"HTML(<!doctype html>
<html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>NekoPaw 课程管理</title><style>
:root{--blue:#4d94de;--ink:#243d56}*{box-sizing:border-box}body{margin:0;background:#eaf3fc;color:var(--ink);font:16px system-ui}main{max-width:650px;margin:auto;padding:16px 16px 36px}.bar{display:flex;align-items:center;justify-content:space-between;gap:10px;padding:12px 0 22px}h1{font-size:23px;margin:0}h2{font-size:19px;margin:0 0 15px}p{line-height:1.6}button,input,select{font:inherit;border:1px solid #cedce9;border-radius:10px;padding:11px;background:white;color:var(--ink);max-width:100%}button{cursor:pointer}button:disabled{opacity:.5;cursor:wait}input,select{width:100%}.primary{background:var(--blue);color:white;border-color:var(--blue)}.link{border:0;background:transparent;color:#367dc4;padding:8px}.card{border:0;background:white;border-radius:18px;padding:20px;margin:14px 0;box-shadow:0 3px 15px #acc3d51c}label{display:block;margin:14px 0}label input,label select{margin-top:7px}.muted,small{color:#647b91}.row{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:18px 0;border-bottom:1px solid #edf1f6}.row button{border:0;text-align:right;color:#337ab8;max-width:75%}.times{display:flex;gap:12px}.times label{flex:1;min-width:0}.actions{display:flex;gap:8px;flex-wrap:wrap;margin-top:16px}.wide{display:block;width:100%;margin:18px 0;padding:16px}.courseName{font-size:20px;font-weight:600}.meetingSummary{white-space:pre-line;line-height:1.7;color:#647b91;font-size:14px}.weekgrid{display:grid;grid-template-columns:repeat(7,1fr);gap:8px;margin:28px 0}.weekgrid button{padding:0;aspect-ratio:1;border-radius:50%;min-width:0}.weekgrid button[aria-pressed=true]{background:var(--blue);color:white;border-color:var(--blue)}dialog{position:fixed;inset:auto 0 0;margin:0 auto;border:0;border-radius:22px 22px 0 0;padding:20px;width:100%;max-width:650px;max-height:88vh;overflow:auto;box-shadow:0 -8px 40px #10283a33}dialog::backdrop{background:#10283a80}.sheetbar{display:flex;align-items:center;justify-content:space-between;gap:6px}.sheetbar h2{font-size:18px;margin:0}.shortcuts{display:flex;justify-content:space-around;gap:8px;padding:10px 0}#status{white-space:pre-wrap;line-height:1.6;padding:8px}#todayRows{padding-left:22px;line-height:1.9}.danger{color:#b54242} [hidden]{display:none!important}
</style><main>
<section id="home">
 <header class="bar"><h1>我的课表</h1><button id="addCourse" class="primary">＋添加课程</button></header>
 <div class="card"><h2 id="todayTitle">今日课表</h2><ol id="todayRows"></ol><div class="actions"><button id="openTemp">临时调课</button><button id="openTerm">学期设置</button></div></div>
 <p class="muted">课表是第2页。按课程录入上课时间，设备自动排出当天的五行课表；其他功能可从上方切换或管理。</p><div id="courses"></div><p id="empty" class="card" hidden>还没有课程，点击右上角“添加课程”开始。</p>
</section>
<section id="editor" hidden>
 <header class="bar"><button id="back" class="link">‹ 返回</button><h1 id="editorTitle">添加课程</h1><button id="saveCourse" class="primary">保存</button></header>
 <div class="card"><label>课程名称<input id="courseName" maxlength="16" placeholder="例如：高等数学"></label><label>任课老师（选填）<input id="teacher" maxlength="12" placeholder="老师姓名"></label></div>
 <div id="meetings"></div><button id="addMeeting" class="wide">＋添加其他上课时间</button><button id="editorTerm" class="link">设置学期开始日期与总周数</button>
</section>
<section id="temporary" hidden>
 <header class="bar"><button id="tempBack" class="link">‹ 返回</button><h1>临时调课</h1><button id="saveTemp" class="primary">保存</button></header>
 <label>生效日期<input id="tempDate" type="date" min="2024-01-01" max="2099-12-31"></label><p class="muted">仅覆盖这一天。临时课程优先，不受原课程周数限制。</p>
 <div id="tempRows"></div><button id="restoreTemp" class="wide" hidden>取消当天调整，恢复课程安排</button>
</section>
<p id="status" role="status">正在连接设备…</p><small>手机与设备连接同一 Wi-Fi。新增和修改需点保存；同一天最多五节课。首次使用请设置学期第一周的周一。</small>
</main>
<dialog id="weeksDialog"><div class="sheetbar"><button id="cancelWeeks" class="link">取消</button><h2>选择课程周数</h2><button id="confirmWeeks" class="link">确定</button></div><div id="weekGrid" class="weekgrid"></div><p id="weeksError" class="danger" role="alert"></p><div class="shortcuts"><button data-preset="odd">单周</button><button data-preset="even">双周</button><button data-preset="all">全选</button><button data-preset="none">清空</button></div></dialog>
<dialog id="timeDialog"><div class="sheetbar"><button id="cancelTime" class="link">取消</button><h2>选择上课时间</h2><button id="confirmTime" class="link">确定</button></div><label>星期<select id="timeDay"><option value="0">周一</option><option value="1">周二</option><option value="2">周三</option><option value="3">周四</option><option value="4">周五</option><option value="5">周六</option><option value="6">周日</option></select></label><div class="times"><label>开始时间<input id="timeStart" type="time"></label><label>结束时间<input id="timeEnd" type="time"></label></div><p class="muted">填写实际钟点，系统按开始时间自动排序。</p><p id="timeError" class="danger" role="alert"></p></dialog>
<dialog id="termDialog"><h2>学期设置</h2><label>第一周的周一日期<input id="termStart" type="date" min="2024-01-01" max="2099-12-31"></label><label>总周数<input id="termWeeks" type="number" min="1" max="30" value="20"></label><p class="muted">默认20周，最多30周。已设置学期后，常规课程只在学期内显示。</p><p id="termError" class="danger" role="alert"></p><div class="actions"><button id="cancelTerm">取消</button><button id="saveTerm" class="primary">保存学期设置</button></div></dialog>
<script>
'use strict';
const $=id=>document.getElementById(id), days=['周一','周二','周三','周四','周五','周六','周日'];
let data,revision,draft,dirty=false,tempDirty=false,busy=false,readOnly=false,selectedIndex=0,chosenWeeks=new Set(),tempDate='';
const clone=v=>JSON.parse(JSON.stringify(v));
const dateToday=()=>new Date(Date.now()+8*3600e3).toISOString().slice(0,10);
const dateNumber=date=>Math.floor(Date.parse(date+'T12:00:00Z')/86400000);
const weekday=date=>(new Date(date+'T12:00:00Z').getUTCDay()+6)%7;
const weekOf=(model,date)=>model.termStart?Math.floor((dateNumber(date)-dateNumber(model.termStart))/7)+1:0;
const weekList=type=>type==='none'?[]:Array.from({length:data.termWeeks},(_,i)=>i+1).filter(n=>type==='all'||n%2===(type==='odd'?1:0));
const onWeek=(m,w)=>!m.weeks||m.weeks.includes(w);
const timeValid=s=>/^([01]\d|2[0-3]):[0-5]\d$/.test(s);
function say(s){$('status').textContent=s;}
function visible(id){for(const name of ['home','editor','temporary'])$(name).hidden=name!==id;window.scrollTo(0,0);}
async function api(path,body){const r=await fetch(path,{cache:'no-store',...(body===undefined?{}:{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})});let v;try{v=await r.json();}catch{throw Error('设备响应异常，请检查 Wi-Fi');}if(!r.ok)throw Error(v.error||'请求失败');return v;}
function weeksLabel(weeks){
 if(!weeks)return '每周';
 const equal=type=>JSON.stringify(weeks)===JSON.stringify(weekList(type));
 if(equal('odd'))return '单周';if(equal('even'))return '双周';
 const parts=[];for(let i=0;i<weeks.length;i++){let first=weeks[i],last=first;while(i+1<weeks.length&&weeks[i+1]===last+1)last=weeks[++i];parts.push(first===last?first:`${first}–${last}`);}return parts.join('、')+'周';
}
function resolve(model,date){
 const override=model.overrides.find(x=>x.date===date);
 if(override)return override.lessons.filter(Boolean).map(clone).sort((a,b)=>a.start.localeCompare(b.start));
 const w=weekOf(model,date),d=weekday(date);if(model.termStart&&(w<1||w>model.termWeeks))return [];
 return model.courses.flatMap(c=>c.meetings.filter(m=>m.weekday===d&&onWeek(m,w)).map(m=>({name:c.name,start:m.start,end:m.end,room:m.room}))).sort((a,b)=>a.start.localeCompare(b.start));
}
function validate(model){
 if(model.termStart&&weekday(model.termStart)!==0)throw Error('第一周开始日期必须是周一');
 if(!Number.isInteger(model.termWeeks)||model.termWeeks<1||model.termWeeks>30)throw Error('总周数须是1至30的整数');
 if(model.courses.length>40)throw Error('最多保存40门课');
 let total=0;
 for(const c of model.courses){
  if(!c.name.trim())throw Error('请填写课程名称');
  if(!c.meetings.length)throw Error('每门课至少需要一个上课时间');
  for(const m of c.meetings){
   total++;
   if(!Number.isInteger(m.weekday)||m.weekday<0||m.weekday>6||!timeValid(m.start)||!timeValid(m.end)||m.start>=m.end)throw Error(`${c.name}：请选择有效的星期和起止时间`);
   if(m.weeks&&(!m.weeks.length||m.weeks.some((w,i)=>!Number.isInteger(w)||w<1||w>30||(i&&w<=m.weeks[i-1]))))throw Error('请至少选择一个有效周数');
   if(m.weeks&&!model.termStart)throw Error('请先通过“学期设置”填写第一周的周一日期');
  }
 }
 if(total>70)throw Error('上课时间合计最多70个');
 for(let w=1;w<=model.termWeeks;w++)for(let d=0;d<7;d++){
  const rows=model.courses.flatMap(c=>c.meetings.filter(m=>m.weekday===d&&onWeek(m,w)).map(m=>({...m,name:c.name})));
  if(rows.length>5)throw Error(`第${w}周${days[d]}超过五节课`);
  for(let i=0;i<rows.length;i++)for(let j=0;j<i;j++)if(rows[i].start<rows[j].end&&rows[j].start<rows[i].end)throw Error(`第${w}周${days[d]}：${rows[j].name}与${rows[i].name}时间重叠`);
 }
 for(const o of model.overrides){
  const rows=o.lessons.filter(Boolean);
  for(const r of rows)if(!r.name.trim()||!timeValid(r.start)||!timeValid(r.end)||r.start>=r.end)throw Error('请检查临时课程的名称和起止时间');
  for(let i=0;i<rows.length;i++)for(let j=0;j<i;j++)if(rows[i].start<rows[j].end&&rows[j].start<rows[i].end)throw Error(`${o.date} 的临时课程时间重叠`);
 }
}
function setBusy(v){busy=v;document.querySelectorAll('button,input,select').forEach(x=>{if(!x.closest('#screen-navigation'))x.disabled=v;});}
async function commit(next){
 if(readOnly)throw Error('原课表读取异常，已禁止覆盖');validate(next);
 await api('/api/clock',{epoch:Math.floor(Date.now()/1000)});
 const result=await api('/api/schedule',{revision,data:next});data=result.data;revision=result.revision;
 renderHome();say('已保存到设备。当天课表有变化时，屏幕稍后刷新。');
}
async function act(action,errorTarget){
 if(busy)return;setBusy(true);say('正在保存…');
 try{await action();}catch(e){say(e.message+'；如有版本冲突，请刷新网页后重新修改。');if(errorTarget)$(errorTarget).textContent=e.message;}finally{setBusy(false);}
}
function renderHome(){
 const date=dateToday(),w=weekOf(data,date),rows=resolve(data,date);
 $('todayTitle').textContent=`${date} ${days[weekday(date)]} `+(data.termStart?(w>=1&&w<=data.termWeeks?`第${w}周`:'非教学周'):'今日课表');
 $('todayRows').replaceChildren();
 if(!rows.length){const li=document.createElement('li');li.textContent='今日无课';$('todayRows').append(li);}
 for(const row of rows){const li=document.createElement('li');li.textContent=`${row.start}–${row.end} ${row.name} ${row.room}`;$('todayRows').append(li);}
 $('courses').replaceChildren();$('empty').hidden=!!data.courses.length;
 for(const c of data.courses){
  const card=document.createElement('article');card.className='card';card.innerHTML='<div class="courseName"></div><p class="muted"></p><div class="meetingSummary"></div><div class="actions"><button class="edit">编辑课程</button><button class="delete danger">删除课程</button></div>';
  card.querySelector('.courseName').textContent=c.name;card.querySelector('p').textContent=c.teacher?`任课老师：${c.teacher}`:'';
  card.querySelector('.meetingSummary').textContent=c.meetings.map(m=>`${days[m.weekday]} ${m.start}–${m.end} · ${weeksLabel(m.weeks)}${m.room?' · '+m.room:''}`).join('\n');
  card.querySelector('.edit').onclick=()=>editCourse(c);
  card.querySelector('.delete').onclick=()=>{if(confirm(`删除“${c.name}”及它的所有上课时间？`))act(async()=>{const next=clone(data);next.courses=next.courses.filter(x=>x.id!==c.id);await commit(next);});};
  $('courses').append(card);
 }
}
function editCourse(c){
 draft=c?clone(c):{id:'c'+Date.now().toString(36)+Math.random().toString(36).slice(2,7),name:'',teacher:'',meetings:[{weekday:0,start:'',end:'',room:''}]};
 dirty=false;$('courseName').value=draft.name;$('teacher').value=draft.teacher;
 $('editorTitle').textContent=c?'编辑课程':'添加课程';renderMeetings();visible('editor');say('选择上课时间、周数和教室，最后点右上角保存。');
}
function renderMeetings(){
 $('meetings').replaceChildren();draft.meetings.forEach((m,i)=>{
  const card=document.createElement('section');card.className='card meeting';
  card.innerHTML=`<h2>上课时间 ${i+1}</h2><div class="row"><span>课程周数</span><button type="button" class="weeks"></button></div><div class="row"><span>上课时间</span><button type="button" class="when"></button></div><label>上课教室<input class="room" maxlength="12" placeholder="请输入上课地点"></label><button class="remove link danger" type="button">移除此上课时间</button>`;
  card.querySelector('.weeks').textContent=weeksLabel(m.weeks)+' ›';card.querySelector('.weeks').onclick=()=>openWeeks(i);
  card.querySelector('.when').textContent=m.start&&m.end?`${days[m.weekday]} ${m.start}–${m.end} ›`:'请选择星期与起止时间 ›';card.querySelector('.when').onclick=()=>openTime(i);
  card.querySelector('.room').value=m.room;card.querySelector('.room').oninput=e=>{m.room=e.target.value;dirty=true;};
  card.querySelector('.remove').onclick=()=>{if(draft.meetings.length===1){say('一门课至少需要一个上课时间。');return;}draft.meetings.splice(i,1);dirty=true;renderMeetings();};
  $('meetings').append(card);
 });
}
function paintWeeks(){
 $('weeksError').textContent='';
 $('weekGrid').replaceChildren();for(let n=1;n<=data.termWeeks;n++){
  const button=document.createElement('button');button.type='button';button.textContent=n;button.dataset.week=n;button.setAttribute('aria-pressed',chosenWeeks.has(n));
  button.onclick=()=>{if(chosenWeeks.has(n))chosenWeeks.delete(n);else chosenWeeks.add(n);paintWeeks();};$('weekGrid').append(button);
 }
}
function openWeeks(i){selectedIndex=i;chosenWeeks=new Set(draft.meetings[i].weeks||weekList('all'));$('weeksError').textContent='';paintWeeks();$('weeksDialog').showModal();}
function openTime(i){selectedIndex=i;const m=draft.meetings[i];$('timeDay').value=m.weekday;$('timeStart').value=m.start;$('timeEnd').value=m.end;$('timeError').textContent='';$('timeDialog').showModal();}
function openTerm(){ $('termStart').value=data.termStart;$('termWeeks').value=data.termWeeks;$('termError').textContent='';$('termDialog').showModal(); }
function fillTemporary(date){
 const override=data.overrides.find(x=>x.date===date),rows=override?clone(override.lessons):resolve(data,date);
 while(rows.length<5)rows.push(null);$('tempRows').replaceChildren();
 rows.forEach((r,i)=>{const card=document.createElement('section');card.className='card tempLesson';card.innerHTML=`<h2>第${i+1}节</h2><label>课程名称<input name="name" maxlength="16" placeholder="留空表示无课"></label><div class="times"><label>开始<input name="start" type="time"></label><label>结束<input name="end" type="time"></label></div><label>教室<input name="room" maxlength="12"></label><button type="button" class="cancelLesson">取消这节课</button>`;
  for(const key of ['name','start','end','room'])card.querySelector(`[name=${key}]`).value=r?.[key]||'';
  card.querySelectorAll('input').forEach(x=>x.oninput=()=>tempDirty=true);
  card.querySelector('button').onclick=()=>{card.querySelectorAll('input').forEach(x=>x.value='');tempDirty=true;};$('tempRows').append(card);
 });$('restoreTemp').hidden=!override;tempDate=date;tempDirty=false;
}
$('addCourse').onclick=()=>editCourse();$('courseName').oninput=$('teacher').oninput=()=>dirty=true;
$('back').onclick=()=>{if(!dirty||confirm('放弃未保存的课程修改？'))visible('home');};
$('addMeeting').onclick=()=>{draft.meetings.push({weekday:0,start:'',end:'',room:''});dirty=true;renderMeetings();};
$('saveCourse').onclick=()=>act(async()=>{const c=clone(draft);c.name=$('courseName').value.trim();c.teacher=$('teacher').value.trim();c.meetings.forEach(m=>m.room=m.room.trim());const next=clone(data),i=next.courses.findIndex(x=>x.id===c.id);if(i<0)next.courses.push(c);else next.courses[i]=c;await commit(next);dirty=false;visible('home');});
$('cancelWeeks').onclick=()=>$('weeksDialog').close();
$('confirmWeeks').onclick=()=>{const weeks=[...chosenWeeks].filter(n=>n<=data.termWeeks).sort((a,b)=>a-b);if(!weeks.length){$('weeksError').textContent='至少选择一个上课周数。';return;}const m=draft.meetings[selectedIndex];if(weeks.length===data.termWeeks)delete m.weeks;else m.weeks=weeks;dirty=true;$('weeksDialog').close();renderMeetings();};
document.querySelectorAll('[data-preset]').forEach(button=>button.onclick=()=>{chosenWeeks=new Set(weekList(button.dataset.preset));paintWeeks();});
$('cancelTime').onclick=()=>$('timeDialog').close();
$('confirmTime').onclick=()=>{const start=$('timeStart').value,end=$('timeEnd').value;if(!timeValid(start)||!timeValid(end)||start>=end){$('timeError').textContent='请填写有效时间，结束须晚于开始。';return;}Object.assign(draft.meetings[selectedIndex],{weekday:+$('timeDay').value,start,end});dirty=true;$('timeDialog').close();renderMeetings();};
$('openTerm').onclick=$('editorTerm').onclick=openTerm;$('cancelTerm').onclick=()=>$('termDialog').close();
$('saveTerm').onclick=()=>act(async()=>{const next=clone(data);next.termStart=$('termStart').value;next.termWeeks=Number($('termWeeks').value);await commit(next);$('termDialog').close();if(!$('editor').hidden)renderMeetings();},'termError');
$('openTemp').onclick=()=>{$('tempDate').value=dateToday();fillTemporary($('tempDate').value);visible('temporary');};
$('tempBack').onclick=()=>{if(!tempDirty||confirm('放弃未保存的临时修改？'))visible('home');};
$('tempDate').onchange=()=>{const date=$('tempDate').value;if(!date||tempDirty&&!confirm('放弃当前日期未保存的修改？')){$('tempDate').value=tempDate;return;}fillTemporary(date);};
$('saveTemp').onclick=()=>act(async()=>{const next=clone(data),date=$('tempDate').value;if(!date)throw Error('请选择日期');const rows=[...$('tempRows').children].map(card=>{const row=Object.fromEntries(['name','start','end','room'].map(key=>[key,card.querySelector(`[name=${key}]`).value.trim()]));return row.name?row:null;}).filter(Boolean).sort((a,b)=>a.start.localeCompare(b.start));while(rows.length<5)rows.push(null);next.overrides=next.overrides.filter(x=>x.date!==date);if(next.overrides.length>=10)throw Error('临时调整最多十个日期，请取消过期调整后再添加');next.overrides.push({date,lessons:rows});await commit(next);fillTemporary(date);});
$('restoreTemp').onclick=()=>{if(confirm('取消当天调整，恢复课程安排？'))act(async()=>{const date=$('tempDate').value,next=clone(data);next.overrides=next.overrides.filter(x=>x.date!==date);await commit(next);fillTemporary(date);});};
(async()=>{try{const result=await api('/api/schedule');if(!Array.isArray(result.data?.courses)||result.data.schema!==3)throw Error('请先上传新版固件，再刷新这个网页');data=result.data;revision=result.revision;readOnly=!!result.warning;renderHome();await api('/api/clock',{epoch:Math.floor(Date.now()/1000)});say(result.warning||'已连接。点击“添加课程”开始录入。');}catch(e){say(e.message);document.querySelectorAll('button').forEach(x=>{if(!x.closest('#screen-navigation'))x.disabled=true;});}})();
</script><script src="/static/screen-navigation.js" defer></script></html>)HTML";

