"""Sync Codex account quotas and aggregate token counters from local rollout metadata.

Uses the existing local Codex sign-in. Never reads/copies auth.json or starts a turn.
"""
from pathlib import Path
from datetime import datetime, timedelta, timezone
import argparse, io, json, logging, math, os, queue, shutil, subprocess, sys, threading, time
import urllib.request, urllib.error

ROOT=Path(__file__).resolve().parent
CREATE_NO_WINDOW=getattr(subprocess,'CREATE_NO_WINDOW',0)

def find_codex():
    candidates=list((Path(os.environ.get('LOCALAPPDATA',''))/'OpenAI/Codex/bin').glob('*/codex.exe'))
    if candidates:return str(max(candidates,key=lambda p:p.stat().st_mtime))
    found=shutil.which('codex.exe') or shutil.which('codex')
    if not found:raise RuntimeError('找不到 Codex，请先安装并登录 Codex 桌面应用。')
    return found

class CodexRPC:
    def __init__(self,executable=None,timeout=40):
        self.executable=executable or find_codex();self.timeout=timeout;self.process=None;self.messages=queue.Queue();self.counter=0
    def __enter__(self):
        self.process=subprocess.Popen([self.executable,'app-server','--listen','stdio://'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,text=True,encoding='utf-8',creationflags=CREATE_NO_WINDOW)
        threading.Thread(target=self._read,daemon=True).start()
        try:
            self.request('initialize',{'clientInfo':{'name':'nekopaw_usage_sync','title':'NekoPaw Usage Sync','version':'1.0'}})
            self.send({'method':'initialized','params':{}})
            return self
        except Exception:
            self.close();raise
    def _read(self):
        try:
            for line in self.process.stdout:
                try:self.messages.put(json.loads(line))
                except json.JSONDecodeError:pass
        finally:self.messages.put(None)
    def send(self,message):
        self.process.stdin.write(json.dumps(message,separators=(',',':'))+'\n');self.process.stdin.flush()
    def request(self,method,params=None):
        self.counter+=1;req_id=self.counter;message={'id':req_id,'method':method}
        if params is not None:message['params']=params
        self.send(message);deadline=time.monotonic()+self.timeout
        while True:
            try:reply=self.messages.get(timeout=max(0,deadline-time.monotonic()))
            except queue.Empty:raise RuntimeError('Codex 用量查询超时，请检查电脑网络和登录状态。') from None
            if reply is None:raise RuntimeError('Codex 用量服务已退出，请重新登录后重试。')
            if 'method' in reply and 'id' in reply:
                # This integration cannot satisfy interactive auth/approval/tool requests.
                self.send({'id':reply['id'],'error':{'code':-32601,'message':'Usage-only client'}})
            elif reply.get('id')==req_id:
                if 'error' in reply:raise RuntimeError('Codex 未返回用量，请检查桌面应用登录状态。')
                return reply.get('result',{})
    def close(self):
        if self.process:
            if self.process.stdin:
                try:self.process.stdin.close()
                except OSError:pass
            try:self.process.wait(timeout=4)
            except subprocess.TimeoutExpired:self.process.terminate();self.process.wait(timeout=4)
            if self.process.stdout:self.process.stdout.close()
            self.process=None
    def __exit__(self,*args):self.close()

def normalize_window(value):
    if value is None:return None
    if not isinstance(value,dict):raise ValueError('窗口格式无效')
    used=value.get('usedPercent');minutes=value.get('windowDurationMins');reset=value.get('resetsAt')
    if used is not None and (isinstance(used,bool) or not isinstance(used,(int,float)) or not math.isfinite(used) or not 0<=used<=10000):raise ValueError('用量百分比无效')
    if minutes is not None and (type(minutes) is not int or not 1<=minutes<=525600):raise ValueError('窗口时长无效')
    if reset is not None and (type(reset) is not int or not 1704067200<=reset<=4102444799):raise ValueError('重置时间无效')
    return {'usedPercent':used,'windowMinutes':minutes,'resetsAt':reset}

def normalize(result,now=None):
    buckets=result.get('rateLimitsByLimitId')
    bucket=buckets.get('codex') if isinstance(buckets,dict) else None
    if not bucket:
        bucket=result.get('rateLimits')
        if bucket and bucket.get('limitId') not in (None,'codex'):bucket=None
    if not bucket:raise ValueError('当前账户没有返回 Codex 额度。')
    windows=[normalize_window(bucket.get(k)) for k in ('primary','secondary')]
    if not any(w and w['usedPercent'] is not None for w in windows):raise ValueError('当前账户未提供可显示的额度。')
    plan=bucket.get('planType')
    return {'schema':1,'fetchedAt':int(time.time()) if now is None else now,'limitId':'codex',
            'plan':plan if plan in ('free','plus','pro','team','business','enterprise','edu') else '',
            'primary':windows[0],'secondary':windows[1]}

CODEX_HOME=Path(os.environ.get('CODEX_HOME',str(Path.home()/'.codex')))
BEIJING=timezone(timedelta(hours=8))
_TOKEN_FILE_CACHE={}

def collect_token_usage(now=None):
    """Aggregate recent rollout token counters; transcript records are not JSON-decoded or retained."""
    now_dt=datetime.now(BEIJING) if now is None else now.astimezone(BEIJING)
    today=now_dt.date(); first_day=today-timedelta(days=6)
    cutoff=datetime.combine(first_day,datetime.min.time(),tzinfo=BEIJING)-timedelta(days=1)
    try:
        import zstandard as zstd
    except ImportError:
        zstd=None
    compressed_omitted=False;active_keys=set()
    roots=(CODEX_HOME/'sessions',CODEX_HOME/'archived_sessions')
    for root in roots:
        if not root.is_dir():continue
        for path in root.rglob('rollout-*.jsonl*'):
            key=str(path);compressed=path.name.endswith('.jsonl.zst')
            try:st=path.stat()
            except OSError:continue
            if st.st_mtime<cutoff.timestamp():continue
            if compressed and zstd is None:
                compressed_omitted=True;continue
            active_keys.add(key);cached=_TOKEN_FILE_CACHE.get(key)
            if cached and cached[0]==st.st_mtime_ns and cached[1]==st.st_size:continue
            file_events=[];thread_id=None
            try:
                if compressed:
                    source=path.open('rb');reader=zstd.ZstdDecompressor().stream_reader(source)
                    stream=io.TextIOWrapper(reader,encoding='utf-8')
                else:stream=path.open('r',encoding='utf-8')
                with stream:
                    for line in stream:
                        # Parse only session metadata and token counters; skip chat/message rows.
                        if 'session_meta' not in line and 'token_count' not in line:continue
                        try:row=json.loads(line)
                        except (json.JSONDecodeError,UnicodeDecodeError):continue
                        if row.get('type')=='session_meta':
                            meta=row.get('payload') or {};thread_id=meta.get('id') or meta.get('session_id') or thread_id;continue
                        payload=row.get('payload') or {}
                        if row.get('type')!='event_msg' or payload.get('type')!='token_count':continue
                        usage=(payload.get('info') or {}).get('total_token_usage') or {}
                        values=(usage.get('total_tokens'),usage.get('input_tokens'),usage.get('output_tokens'))
                        if not all(type(v) is int and 0<=v<=9000000000000000 for v in values):continue
                        stamp=row.get('timestamp')
                        if not isinstance(stamp,str):continue
                        try:event_time=datetime.fromisoformat(stamp.replace('Z','+00:00')).astimezone(BEIJING)
                        except (ValueError,OverflowError):continue
                        file_events.append((thread_id or path.stem,event_time,*values))
            except (OSError,EOFError,ValueError):
                continue
            _TOKEN_FILE_CACHE[key]=(st.st_mtime_ns,st.st_size,file_events)
    for key in list(_TOKEN_FILE_CACHE):
        if key not in active_keys:_TOKEN_FILE_CACHE.pop(key,None)
    events={}
    for _,_,file_events in _TOKEN_FILE_CACHE.values():
        for tid,event_time,total,input_tokens,output_tokens in file_events:
            events.setdefault(tid,[]).append((event_time,total,input_tokens,output_tokens))
    daily={first_day+timedelta(days=i):[0,0,0] for i in range(7)};event_count=0
    for records in events.values():
        records.sort(key=lambda item:item[0]);previous=(0,0,0);seen=set()
        for event_time,total,input_tokens,output_tokens in records:
            current=(total,input_tokens,output_tokens)
            if current in seen:continue
            seen.add(current)
            if any(current[i]<previous[i] for i in range(3)):
                previous=current;continue
            delta=tuple(current[i]-previous[i] for i in range(3));previous=current;day=event_time.date()
            if day not in daily or not any(delta):continue
            for i,value in enumerate(delta):daily[day][i]+=value
            event_count+=1
    if not event_count:return None,not compressed_omitted
    history=[{'date':day.strftime('%m/%d'),'totalTokens':values[0]} for day,values in daily.items()]
    today_values=daily[today]
    return {'today':{'totalTokens':today_values[0],'inputTokens':today_values[1],'outputTokens':today_values[2]},
            'history':history,'complete':not compressed_omitted},not compressed_omitted

def post_snapshot(device,snapshot):
    # Skip system HTTP proxies for the local device. Do not send credentials.
    opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
    request=urllib.request.Request(device.rstrip('/')+'/api/codex',data=json.dumps(snapshot).encode('utf-8'),
            headers={'Content-Type':'application/json'},method='POST')
    try:
        with opener.open(device.rstrip('/')+'/api/codex',timeout=12) as response:status=json.load(response)
        if not status.get('clockValid'):
            clock=urllib.request.Request(device.rstrip('/')+'/api/clock',data=json.dumps({'epoch':snapshot['fetchedAt']}).encode(),headers={'Content-Type':'application/json'},method='POST')
            with opener.open(clock,timeout=12) as response:json.load(response)
        with opener.open(request,timeout=12) as response:
            if response.status!=200:raise RuntimeError('设备未确认保存用量。')
            reply=json.load(response)
            if not reply.get('ok'):raise RuntimeError('设备未确认保存用量。')
    except urllib.error.HTTPError as exc:
        if exc.code==404:raise RuntimeError('请先 Upload 新版固件，设备还没有用量接口。') from None
        raise RuntimeError(f'设备保存失败，HTTP {exc.code}。') from None
    except urllib.error.URLError:raise RuntimeError('连不上 ESP32，请检查同一热点及设备 IP。') from None

def instance_lock(path):
    handle=path.open('a+b')
    if path.stat().st_size==0:handle.write(b'0');handle.flush()
    handle.seek(0)
    try:
        if os.name=='nt':
            import msvcrt
            msvcrt.locking(handle.fileno(),msvcrt.LK_NBLCK,1)
        else:
            import fcntl
            fcntl.flock(handle,fcntl.LOCK_EX|fcntl.LOCK_NB)
    except OSError:
        handle.close();return None
    return handle

def main():
    parser=argparse.ArgumentParser(description='同步 Codex 额度和本地会话 Token 汇总到墨水屏，不发起 AI 对话。')
    parser.add_argument('--config',default=str(ROOT/'codex-sync.json'));parser.add_argument('--read-only',action='store_true')
    parser.add_argument('--once',action='store_true');args=parser.parse_args()
    if args.read_only:
        with CodexRPC() as rpc:snapshot=normalize(rpc.request('account/rateLimits/read'))
        print(json.dumps(snapshot,ensure_ascii=False));return
    config_path=Path(args.config);config=json.loads(config_path.read_text(encoding='utf-8-sig'))
    device=config['device'].rstrip('/')
    from urllib.parse import urlparse
    parsed=urlparse(device)
    if parsed.scheme!='http' or not parsed.hostname or parsed.username or parsed.password or parsed.path not in ('','/') or parsed.query or parsed.fragment:raise ValueError('device 必须是设备的本地 http://IP:81 地址')
    interval=config.get('intervalSeconds',300)
    if type(interval) is not int or not 60<=interval<=3600:raise ValueError('同步间隔须为60～3600秒')
    logging.basicConfig(filename=str(ROOT/'codex-sync.log'),encoding='utf-8',level=logging.INFO,format='%(asctime)s %(levelname)s %(message)s')
    lock=instance_lock(ROOT/'codex-sync.lock')
    if lock is None:logging.info('已有同步脚本运行，本次退出。');return
    rpc=None
    try:
        while True:
            config=json.loads(config_path.read_text(encoding='utf-8-sig'))
            if config.get('enabled',True) is False:logging.info('自动同步已停用。');return
            device=config['device'].rstrip('/')
            parsed=urlparse(device)
            if parsed.scheme!='http' or not parsed.hostname or parsed.username or parsed.password or parsed.path not in ('','/') or parsed.query or parsed.fragment:raise ValueError('设备地址无效')
            try:
                if rpc is None:rpc=CodexRPC().__enter__()
                snapshot=normalize(rpc.request('account/rateLimits/read'))
                tokens,complete=collect_token_usage()
                if tokens is not None:snapshot['tokens']=tokens
                if not complete:logging.warning('部分近期 Codex 会话日志为压缩格式，未安装 zstandard 时这部分 Token 不计入。')
                post_snapshot(device,snapshot)
                logging.info('Codex 用量已同步。')
                if sys.stdout:print('Codex 用量已同步。',flush=True)
            except Exception as exc:
                # Messages are deliberately generic: never log raw account/auth replies.
                message=str(exc) if isinstance(exc,(RuntimeError,ValueError)) else type(exc).__name__
                logging.warning('%s',message)
                if sys.stdout:print(message,flush=True)
                if rpc:rpc.close();rpc=None
            if args.once:break
            deadline=time.monotonic()+interval
            while time.monotonic()<deadline:
                time.sleep(min(5,max(0,deadline-time.monotonic())))
                try:
                    if json.loads(config_path.read_text(encoding='utf-8-sig')).get('enabled',True) is False:return
                except (OSError,ValueError):pass # Editor may briefly replace the file while saving.
    finally:
        if rpc:rpc.close()
        lock.close()

if __name__=='__main__':main()
