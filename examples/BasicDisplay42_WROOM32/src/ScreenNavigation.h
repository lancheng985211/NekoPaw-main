#pragma once
#include <Arduino.h>

// Shared by the phone management pages. Loading it only reads device state;
// changing the e-ink page always requires an explicit button click.
static const char kScreenNavigationJs[] PROGMEM = R"JS('use strict';
(() => {
 function mount(){
  if(document.getElementById('screen-navigation'))return;
  const pages=[
   {name:'home',label:'首页',href:'/home'},
   {name:'pet',label:'宠物',href:'/pet'},
   {name:'course',label:'课表',href:'/'},
   {name:'weather',label:'天气',href:'/weather'},
   {name:'calendar',label:'日历',href:'/calendar'},
   {name:'codex',label:'Codex 用量',href:'/codex'},
   {name:'delivery',label:'外卖',href:'/delivery'},
   {name:'memo',label:'备忘录',href:'/memo'}
  ];
  const make=(tag,text,css)=>{const e=document.createElement(tag);if(text!==undefined)e.textContent=text;if(css)e.className=css;return e;};
  const style=make('style');style.textContent=`
   #screen-navigation{box-sizing:border-box;max-width:650px;margin:0 auto 22px;padding:17px;background:#fff;border:1px solid #d6e3ef;border-radius:17px;color:#243d56;font:15px/1.6 system-ui,"Microsoft YaHei",sans-serif}
   #screen-navigation *{box-sizing:border-box}
   #screen-navigation h2{margin:0;font-size:19px;line-height:1.5}
   #screen-navigation p{margin:7px 0;font-size:13px;line-height:1.6;overflow-wrap:anywhere}
   #screen-navigation .screen-nav-current{font-size:15px;margin:9px 0 12px;font-weight:600}
   #screen-navigation .screen-nav-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px 8px}
   #screen-navigation .screen-nav-tile{min-width:0;text-align:center}
   #screen-navigation button{box-sizing:border-box;display:block;width:100%;min-width:0;min-height:46px;padding:8px 4px;margin:0;border:1px solid #c6d7e6;border-radius:9px;background:#edf5fc;color:#243d56;font:14px/1.4 system-ui,"Microsoft YaHei",sans-serif;cursor:pointer;overflow-wrap:anywhere}
   #screen-navigation button[aria-pressed="true"]{background:#4d94de;color:#fff;border-color:#4d94de}
   #screen-navigation button:disabled{opacity:.5;cursor:wait}
   #screen-navigation button:focus-visible,#screen-navigation a:focus-visible{outline:2px solid #367dc4;outline-offset:3px}
   #screen-navigation a{display:inline-block;color:#367dc4;font-size:13px;padding:4px 5px;text-underline-offset:3px}
   #screen-navigation .screen-nav-help{color:#647b91;margin-top:12px}
   #screen-navigation .screen-nav-error{color:#a83434;white-space:pre-wrap}
   #screen-navigation .screen-nav-feedback{color:#26754d;white-space:pre-wrap}
   #screen-navigation [hidden]{display:none!important}
   @media(min-width:560px){#screen-navigation .screen-nav-grid{grid-template-columns:repeat(4,minmax(0,1fr))}}
  `;
  const card=make('section');card.id='screen-navigation';card.setAttribute('aria-label','屏幕页面');
  const title=make('h2','屏幕页面');
  const current=make('p','正在读取屏幕状态…','screen-nav-current');current.id='screen-nav-current';
  const grid=make('div',undefined,'screen-nav-grid');
  const buttons=[];
  for(const [index,page] of pages.entries()){
   const tile=make('div',undefined,'screen-nav-tile');
   const button=make('button',(index+1)+' · '+page.label);button.type='button';button.dataset.screenPage=page.name;button.setAttribute('aria-pressed','false');button.setAttribute('aria-label','屏幕显示'+page.label);button.addEventListener('click',()=>selectPage(page));
   const link=make('a','管理'+page.label);link.href=page.href;link.dataset.screenManage=page.name;
   tile.append(button,link);grid.append(tile);buttons.push(button);
  }
  const help=make('p','点击上面的按钮切换墨水屏；“管理”只打开手机设置页。','screen-nav-help');
  const readError=make('p',undefined,'screen-nav-error');readError.id='screen-nav-error';readError.setAttribute('role','alert');readError.hidden=true;
  const feedback=make('p',undefined,'screen-nav-feedback');feedback.id='screen-nav-feedback';feedback.setAttribute('role','status');feedback.hidden=true;
  card.append(title,current,grid,help,readError,feedback);
  document.head.append(style);(document.querySelector('main')||document.body).prepend(card);

  let model=null,writing=false,pending=null,timer=null,disposed=false,generation=0;
  function controls(){buttons.forEach(b=>{b.disabled=writing;b.setAttribute('aria-pressed',String(!!model&&b.dataset.screenPage===model.page));});}
  function clearPoll(){if(timer!==null){clearTimeout(timer);timer=null;}}
  function schedule(){clearPoll();if(!disposed&&!document.hidden&&!writing&&!pending)timer=setTimeout(()=>{timer=null;refresh();},5000);}
  function say(message,error=false){feedback.textContent=message;feedback.hidden=!message;feedback.className=error?'screen-nav-error':'screen-nav-feedback';}
  async function api(body){
   const controller=new AbortController(),timeout=setTimeout(()=>controller.abort(),8000);
   try{
    const response=await fetch('/api/page',{cache:'no-store',signal:controller.signal,...(body===undefined?{}:{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})});
    if(response.status===404)throw Error('请先上传整合版固件，再刷新网页。');
    let value;try{value=await response.json();}catch{throw Error('设备页面状态响应异常，请稍后重试。');}
    if(!response.ok)throw Error(value.error||'切换失败，HTTP '+response.status);
    return value;
   }catch(error){
    if(error.name==='AbortError')throw Error('连接超时，请检查手机与设备是否连接同一 Wi-Fi。');
    if(error instanceof TypeError)throw Error('暂时连不上设备，将自动重试。请检查 Wi-Fi。');
    throw error;
   }finally{clearTimeout(timeout);}
  }
  function refresh(){
   if(disposed||document.hidden)return Promise.resolve(false);
   if(pending)return pending;
   clearPoll();const version=generation;
   pending=(async()=>{
    try{
     const data=await api();
     const name=data.page,index=pages.findIndex(p=>p.name===name);
     if(data.service!=='nekopaw-pages-v2'||data.pageCount!==pages.length||index<0)throw Error('页面版本不匹配，请上传整合版固件后刷新。');
     if(version!==generation)return false;
     model={page:name};current.textContent='当前屏幕：'+(index+1)+' / '+pages.length+' · '+pages[index].label;
     readError.textContent='';readError.hidden=true;controls();return true;
    }catch(error){
     if(version===generation){readError.textContent=error.message;readError.hidden=false;if(!model)current.textContent='尚未连接设备';}
     return false;
    }finally{pending=null;controls();schedule();}
   })();
   return pending;
  }
  async function selectPage(page){
   if(writing||disposed)return;
   writing=true;generation+=1;clearPoll();controls();say('正在切换到'+page.label+'…');
   try{
    const activeRead=pending;if(activeRead)await activeRead;
    await api({page:page.name});
    const confirmed=await refresh();
    if(confirmed&&model.page===page.name)say('屏幕已切换到'+page.label+'。');
    else if(confirmed)say('设备当前显示其他页面，状态已更新。');
    else say('切页指令已发送，等待重新读取屏幕状态。');
   }catch(error){say(error.message,true);}
   finally{writing=false;controls();schedule();}
  }
  document.addEventListener('visibilitychange',()=>{if(document.hidden)clearPoll();else refresh();});
  window.addEventListener('pagehide',()=>{disposed=true;clearPoll();});
  refresh();
 }
 if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',mount,{once:true});else mount();
})();
)JS";
