#pragma once
// Local-UI web console.  Inline CSS/JS = one request, no external assets.
const char PAGE_HTML[] = R"=====(<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>4.2 吋墨水屏 · 本地 UI</title>
<style>
:root{--bg:#eef1f5;--card:#fff;--ink:#111827;--muted:#667085;--line:#d0d5dd;--accent:#0a6cff;--ok:#0a7a3d;--err:#c62828}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.5 system-ui,-apple-system,"Segoe UI",Roboto,"PingFang SC","Microsoft YaHei",sans-serif}
header{background:#101828;color:#fff;padding:14px 18px;display:flex;justify-content:space-between;align-items:center;gap:12px;position:sticky;top:0;z-index:5}
header h1{font-size:18px;margin:0;font-weight:600;letter-spacing:.2px}
#status{font-size:12px;color:#cbd5e1;text-align:right}
main{max-width:1040px;margin:0 auto;padding:16px;display:grid;grid-template-columns:minmax(0,1.1fr) minmax(0,.9fr);gap:16px}
.card{background:var(--card);border-radius:12px;padding:14px;box-shadow:0 1px 2px rgba(16,24,40,.08),0 1px 3px rgba(16,24,40,.06)}
.remote{grid-column:1/-1}
.remote-wrap{display:grid;grid-template-columns:minmax(0,400px) minmax(0,1fr);gap:20px;align-items:start}
.remote-screen #remoteScreen{width:100%;max-width:400px}
#remoteScreen{width:100%;height:auto;display:block;border:1px solid var(--line);border-radius:6px;background:#fff;image-rendering:pixelated}
#uiState{margin-top:8px;font-size:12px;color:var(--muted);min-height:18px}
.remote-side{display:flex;flex-direction:column;gap:14px}
.pad{display:grid;grid-template-columns:64px 64px 64px;grid-template-rows:44px 44px 44px 44px;gap:8px;
     grid-template-areas:". up ." "left ok right" ". down ." "back back back";justify-content:start}
.pad button{font-size:18px;padding:0;height:44px;border-radius:10px}
.pad .k-up{grid-area:up}.pad .k-left{grid-area:left}.pad .k-ok{grid-area:ok}
.pad .k-right{grid-area:right}.pad .k-down{grid-area:down}.pad .k-back{grid-area:back;font-size:14px}
.pad .k-ok{background:var(--accent);border-color:var(--accent);color:#fff;font-weight:700}
.apps{display:flex;flex-wrap:wrap;gap:8px}
.apps button{padding:7px 10px;font-size:13px}
.note-row{display:grid;grid-template-columns:1fr auto;gap:10px;margin-top:12px;align-items:start}
#noteText{width:100%;height:74px;padding:8px;border:1px solid var(--line);border-radius:8px;font:14px/1.5 inherit;resize:vertical}
.qr,.host{grid-column:1/-1}
.host input{padding:8px;border:1px solid var(--line);border-radius:8px;font:14px/1.5 inherit;min-width:220px}
.qr-grid{display:grid;grid-template-columns:1fr 1fr;gap:18px}
.qr-grid label{display:block;margin:8px 0 4px}
.qr-grid textarea,.qr-grid input{width:100%;padding:8px;border:1px solid var(--line);border-radius:8px;font:14px/1.5 inherit}
.qr-grid textarea{height:96px;resize:vertical}
canvas{width:100%;height:auto;display:block;border:1px solid var(--line);border-radius:6px;background:#fff;image-rendering:pixelated}
#drop{margin-top:10px;border:2px dashed #cbd5e1;border-radius:8px;padding:14px;text-align:center;color:var(--muted);cursor:pointer;transition:.15s}
#drop.over{border-color:var(--accent);background:#f0f6ff}
h2{font-size:15px;margin:0 0 10px}
.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin:10px 0}
label{font-size:13px;color:var(--muted);display:inline-flex;align-items:center;gap:6px}
select{padding:5px 6px;border:1px solid var(--line);border-radius:6px;font:inherit}
input[type=range]{width:130px}
button{padding:8px 12px;border:1px solid var(--line);background:#fff;border-radius:8px;font:inherit;cursor:pointer}
button:hover{border-color:#98a2b3}
button.primary{background:var(--accent);border-color:var(--accent);color:#fff}
button.primary:hover{background:#0057d8}
button:disabled{opacity:.45;cursor:default}
.hint{font-size:12px;color:var(--muted);margin:6px 0 0;line-height:1.5}
#log{height:150px;overflow:auto;background:#0b1020;color:#d7e0ff;border-radius:8px;padding:10px;font:12px/1.55 ui-monospace,Consolas,monospace}
#log div{white-space:pre-wrap;word-break:break-word}
#log .ok{color:#7ee2a8}#log .err{color:#ff8a8a}#log .warn{color:#ffd166}
@media(max-width:760px){main{grid-template-columns:1fr}.remote-wrap{grid-template-columns:1fr}.note-row,.qr-grid{grid-template-columns:1fr}#status{display:none}}
</style>
</head>
<body>
<header><h1>4.2″ 墨水屏 · 本地 UI</h1><div id="status">连接中…</div></header>
<main>

  <section class="card remote">
    <h2>本地 UI 遥控</h2>
    <div class="remote-wrap">
      <div class="remote-screen">
        <canvas id="remoteScreen" width="400" height="300"></canvas>
        <div id="uiState">连接中…</div>
      </div>
      <div class="remote-side">
        <div class="pad">
          <button class="k-up" data-key="up">▲</button>
          <button class="k-left" data-key="left">◀</button>
          <button class="k-ok" data-key="ok">OK</button>
          <button class="k-right" data-key="right">▶</button>
          <button class="k-down" data-key="down">▼</button>
          <button class="k-back" data-key="back">返回 / 主页</button>
        </div>
        <div class="apps">
          <button data-app="home">主页</button>
          <button data-app="clock">时钟</button>
          <button data-app="notes">记事</button>
          <button data-app="image">图片</button>
          <button data-app="qr">二维码</button>
          <button data-app="settings">设置</button>
        </div>
      </div>
    </div>
    <div class="note-row">
      <textarea id="noteText" maxlength="1200" placeholder="输入记事内容（支持中文），发送后由 ESP32 本地渲染并保存到 NVS。"></textarea>
      <button id="btnNote" class="primary">发送到记事</button>
    </div>
    <p class="hint">方向键控制本地菜单；“OK”进入选中的应用；“返回/主页”回主菜单。预览每 1.5 秒同步一次；时钟页由 ESP32 自己每分钟全刷更新（老 4.2 不适合 1Hz 局部刷新，秒跳会累积发灰）。</p>
  </section>

  <section class="card qr">
    <h2>二维码</h2>
    <div class="qr-grid">
      <div>
        <label>文字内容（支持中文）</label>
        <textarea id="qrText" maxlength="512" placeholder="输入要生成二维码的文字，例如：https://example.com 或一段备忘"></textarea>
        <div class="row"><button id="btnQrText" class="primary">生成文字二维码</button></div>
      </div>
      <div>
        <label>WiFi SSID（2.4G）</label>
        <input id="qrSsid" maxlength="32" placeholder="例如 MyRouter">
        <label>WiFi 密码</label>
        <input id="qrPass" type="password" maxlength="63" placeholder="8-63 位；留空表示开放网络">
        <div class="row"><button id="btnQrWifi" class="primary">生成 WiFi 二维码</button></div>
      </div>
    </div>
    <div class="row">
      <button id="btnQrClear">清空二维码</button>
      <span class="hint">生成后自动切到设备“二维码”页；使用 50Hz 高清全刷（约 4.1s）保证最大对比度。WiFi 二维码会把 SSID/密码显示在屏幕上。</span>
    </div>
  </section>

  <section class="card host">
    <h2>设备主机名</h2>
    <div class="row">
      <input id="hostName" maxlength="32" placeholder="epaper42">
      <button id="btnHost" class="primary">保存主机名</button>
      <a id="hostLink" class="hint" href="http://epaper42.local" target="_blank" rel="noopener">http://epaper42.local</a>
    </div>
    <p class="hint">1–32 位小写字母/数字/连字符，不能以 - 开头或结尾。保存后主菜单左上角和 mDNS 立即更新；DHCP 主机名在重启后完全生效。路由器支持本地 DNS 时也可直接用主机名访问。</p>
  </section>

  <section class="card">
    <canvas id="preview" width="400" height="300"></canvas>
    <div id="drop">拖入图片，或点击选择文件<input id="file" type="file" accept="image/*" hidden></div>
  </section>

  <section class="card">
    <h2>图片上传</h2>
    <div class="row">
      <label>缩放
        <select id="fit">
          <option value="cover">铺满裁切</option>
          <option value="contain">完整留白</option>
        </select>
      </label>
      <label><input type="checkbox" id="dither" checked> Floyd–Steinberg 抖动</label>
    </div>
    <div class="row"><label>阈值 <input type="range" id="thr" min="1" max="254" value="128"><span id="thrv">128</span></label></div>
    <div class="row">
      <label>刷新模式
        <select id="fullMode">
          <option value="fast">快速 0.6s（略灰）</option>
          <option value="std">标准 1.0s</option>
          <option value="hq">高清 4.1s</option>
        </select>
      </label>
      <button id="btnFull" class="primary">全屏刷新</button>
      <button id="btnRefresh">全刷清残影</button>
    </div>
    <p class="hint">上传后自动进入本地“图片”页；“返回/主页”回菜单。快速：0.6s，标准：1.0s，高清：4.1s。</p>
  </section>

  <section class="card" style="grid-column:1/-1">
    <h2>日志</h2>
    <div id="log"></div>
  </section>
</main>
<script>
const W=400,H=300,STRIDE=W/8;
const $=id=>document.getElementById(id);
const APP_CN={home:'主菜单',clock:'时钟',notes:'记事',image:'图片',qr:'二维码',settings:'设置'};

function log(msg,cls){
  const el=$('log');
  const t=new Date().toLocaleTimeString('zh-CN',{hour12:false});
  const d=document.createElement('div');
  if(cls) d.className=cls;
  d.textContent='['+t+'] '+msg;
  el.appendChild(d);
  el.scrollTop=el.scrollHeight;
  while(el.childNodes.length>200) el.removeChild(el.firstChild);
}
function setStatus(t){ $('status').textContent=t; }

async function api(path,body){
  const t0=performance.now();
  const opt={method:'POST',cache:'no-store'};
  if(body!==undefined && body!==null){
    opt.body=body;
    opt.headers={'Content-Type':'application/octet-stream'};
  }
  const r=await fetch(path,opt);
  const text=await r.text();
  let j;
  try{ j=JSON.parse(text); }
  catch(e){ throw new Error(text.slice(0,120)||('HTTP '+r.status)); }
  j.client_ms=Math.round(performance.now()-t0);
  if(!r.ok||!j.ok) throw new Error(j.err||('HTTP '+r.status));
  return j;
}

// ---------------- local UI remote ----------------
const remote=$('remoteScreen'), rctx=remote.getContext('2d');
let remoteBusy=false, remoteSeq=0;
function setRemoteBusy(b){
  remoteBusy=b;
  document.querySelectorAll('.pad button,.apps button,#btnQrText,#btnQrWifi,#btnQrClear,#btnHost').forEach(x=>{x.disabled=b;});
}
function drawRemote(buf){
  const out=rctx.createImageData(W,H);
  for(let y=0;y<H;y++)for(let x=0;x<W;x++){
    const on=(buf[y*STRIDE+(x>>3)]>>(7-(x&7)))&1;
    const p=(y*W+x)*4; const v=on?255:0;
    out.data[p]=out.data[p+1]=out.data[p+2]=v;
    out.data[p+3]=255;
  }
  rctx.putImageData(out,0,0);
}
async function refreshRemote(force){
  if(remoteBusy && !force) return;
  const seq=++remoteSeq;
  try{
    const r=await fetch('/api/screen?t='+Date.now(),{cache:'no-store'});
    if(!r.ok) throw new Error('HTTP '+r.status);
    const buf=new Uint8Array(await r.arrayBuffer());
    if(seq!==remoteSeq) return;
    if(buf.length!==15000) throw new Error('screen '+buf.length);
    drawRemote(buf);
  }catch(e){
    if(seq===remoteSeq) $('uiState').textContent='设备离线或重启中…';
    return;
  }
  try{
    const r=await fetch('/api/ui?t='+Date.now(),{cache:'no-store'});
    const j=await r.json();
    if(seq!==remoteSeq) return;
    $('uiState').textContent='当前：'+(APP_CN[j.app]||j.app)+' · 选中 '+(j.sel+1)+
      ' · 页码 '+(j.page+1)+'/'+j.pages+' · 上次 '+j.ms+'ms · '+j.ip;
  }catch(e){}
}
async function sendKey(k){
  if(remoteBusy) return;
  setRemoteBusy(true);
  try{
    const j=await api('/api/key?k='+encodeURIComponent(k));
    if(j.ms>0) log('按键 '+k+'：'+j.ms+'ms','ok');
    await refreshRemote(true);
  }catch(e){ log('遥控失败：'+e.message,'err'); }
  finally{ setRemoteBusy(false); }
}
async function sendApp(name){
  if(remoteBusy) return;
  setRemoteBusy(true);
  try{
    await api('/api/app?name='+encodeURIComponent(name));
    await refreshRemote(true);
  }catch(e){ log('切换失败：'+e.message,'err'); }
  finally{ setRemoteBusy(false); }
}
document.querySelectorAll('[data-key]').forEach(b=>b.addEventListener('click',()=>sendKey(b.dataset.key)));
document.querySelectorAll('[data-app]').forEach(b=>b.addEventListener('click',()=>sendApp(b.dataset.app)));
$('btnNote').addEventListener('click',async()=>{
  if(remoteBusy) return;
  setRemoteBusy(true);
  try{
    await api('/api/note',$('noteText').value);
    log('记事已发送到设备','ok');
    await refreshRemote(true);
  }catch(e){ log('发送失败：'+e.message,'err'); }
  finally{ setRemoteBusy(false); }
});
async function qrAction(path,body,label){
  if(remoteBusy) return;
  setRemoteBusy(true);
  try{
    const j=await api(path,body);
    log(label+'：'+j.ms+'ms，'+j.qrModules+' 模块','ok');
    await refreshRemote(true);
  }catch(e){ log(label+'失败：'+e.message,'err'); }
  finally{ setRemoteBusy(false); }
}
$('btnQrText').addEventListener('click',()=>{
  const t=$('qrText').value;
  if(!t.trim()){ log('请输入二维码文字','warn'); return; }
  qrAction('/api/qr?type=text',t,'生成文字二维码');
});
$('btnQrWifi').addEventListener('click',()=>{
  const s=$('qrSsid').value.trim(), p=$('qrPass').value;
  if(!s){ log('请输入 WiFi SSID','warn'); return; }
  qrAction('/api/qr?type=wifi&ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p),null,'生成 WiFi 二维码');
});
$('btnQrClear').addEventListener('click',()=>qrAction('/api/qr?type=clear',null,'清空二维码'));

function updateHostUi(h){
  $('hostName').value=h;
  $('hostLink').textContent='http://'+h+'.local';
  $('hostLink').href='http://'+h+'.local';
}
async function loadHost(){
  try{
    const r=await fetch('/api/host',{cache:'no-store'});
    const j=await r.json();
    if(j.host) updateHostUi(j.host);
  }catch(e){}
}
$('btnHost').addEventListener('click',async()=>{
  if(remoteBusy) return;
  const name=$('hostName').value.trim().toLowerCase();
  if(!/^[a-z0-9](?:[a-z0-9-]{0,30}[a-z0-9])?$/.test(name)){
    log('主机名格式不对：1–32 位 a-z/0-9/-，首尾不能是 -','warn'); return;
  }
  setRemoteBusy(true);
  try{
    const j=await api('/api/host?name='+encodeURIComponent(name));
    updateHostUi(j.host);
    log('主机名已保存：'+j.host+' · '+j.mdns,'ok');
    await refreshRemote(true);
  }catch(e){ log('主机名保存失败：'+e.message,'err'); }
  finally{ setRemoteBusy(false); }
});

async function loadNote(){
  try{
    const r=await fetch('/api/note',{cache:'no-store'});
    $('noteText').value=await r.text();
  }catch(e){}
}

// ---------------- image upload ----------------
const preview=$('preview'), pctx=preview.getContext('2d');
const work=document.createElement('canvas'); work.width=W; work.height=H;
const wctx=work.getContext('2d',{willReadFrequently:true});
let srcImg=null, srcPixels=null, fullBits=null, hasImage=false, busy=false;

function setBusy(b){
  busy=b;
  ['btnFull','btnRefresh'].forEach(id=>{$(id).disabled=b;});
}
function drawSource(){
  if(!srcImg) return;
  const fit=$('fit').value;
  const s=fit==='cover'?Math.max(W/srcImg.width,H/srcImg.height):Math.min(W/srcImg.width,H/srcImg.height);
  const dw=srcImg.width*s, dh=srcImg.height*s;
  wctx.fillStyle='#fff'; wctx.fillRect(0,0,W,H);
  wctx.imageSmoothingEnabled=true; wctx.imageSmoothingQuality='high';
  wctx.drawImage(srcImg,(W-dw)/2,(H-dh)/2,dw,dh);
  srcPixels=wctx.getImageData(0,0,W,H);
  hasImage=true;
  process();
}
function process(){
  if(!srcPixels) return;
  const data=srcPixels.data;
  const dither=$('dither').checked;
  const thr=+$('thr').value;
  const gray=new Float32Array(W*H);
  for(let i=0;i<W*H;i++){
    const p=i*4;
    gray[i]=0.299*data[p]+0.587*data[p+1]+0.114*data[p+2];
  }
  const bits=new Uint8Array(STRIDE*H);
  if(!dither){
    for(let y=0;y<H;y++)for(let x=0;x<W;x++){
      if(gray[y*W+x]>=thr) bits[y*STRIDE+(x>>3)]|=0x80>>(x&7);
    }
  }else{
    for(let y=0;y<H;y++){
      for(let x=0;x<W;x++){
        const i=y*W+x;
        const old=gray[i];
        const nv=old<thr?0:255;
        gray[i]=nv;
        if(nv) bits[y*STRIDE+(x>>3)]|=0x80>>(x&7);
        const err=old-nv;
        if(x+1<W) gray[i+1]+=err*7/16;
        if(y+1<H){
          if(x>0) gray[i+W-1]+=err*3/16;
          gray[i+W]+=err*5/16;
          if(x+1<W) gray[i+W+1]+=err*1/16;
        }
      }
    }
  }
  fullBits=bits;
  renderPreview(bits);
}
function renderPreview(bits){
  const out=pctx.createImageData(W,H);
  for(let y=0;y<H;y++)for(let x=0;x<W;x++){
    const on=(bits[y*STRIDE+(x>>3)]>>(7-(x&7)))&1;
    const p=(y*W+x)*4; const v=on?255:0;
    out.data[p]=out.data[p+1]=out.data[p+2]=v;
    out.data[p+3]=255;
  }
  pctx.putImageData(out,0,0);
}
function loadFile(file){
  if(!file||!file.type.startsWith('image/')){ log('请选择图片文件','err'); return; }
  const url=URL.createObjectURL(file);
  const img=new Image();
  img.onload=()=>{
    URL.revokeObjectURL(url);
    srcImg=img; drawSource();
    log('已载入 '+file.name+'（'+img.width+'×'+img.height+'）','ok');
  };
  img.onerror=()=>{ URL.revokeObjectURL(url); log('图片加载失败','err'); };
  img.src=url;
}
$('fit').addEventListener('change',drawSource);
$('dither').addEventListener('change',process);
$('thr').addEventListener('input',()=>{ $('thrv').textContent=$('thr').value; process(); });
const drop=$('drop');
drop.addEventListener('click',e=>{ if(e.target===$('file')) return; $('file').click(); });
['dragenter','dragover'].forEach(ev=>drop.addEventListener(ev,e=>{e.preventDefault();drop.classList.add('over');}));
['dragleave','drop'].forEach(ev=>drop.addEventListener(ev,e=>{e.preventDefault();drop.classList.remove('over');}));
drop.addEventListener('drop',e=>{ const f=e.dataTransfer.files[0]; if(f) loadFile(f); });
$('file').addEventListener('change',e=>{ const f=e.target.files[0]; if(f) loadFile(f); e.target.value=''; });

$('btnFull').addEventListener('click',async()=>{
  if(!fullBits){ log('先选择一张图片','warn'); return; }
  try{
    setBusy(true);
    const mode=$('fullMode').value;
    const j=await api('/api/full?mode='+mode,fullBits);
    log('全屏完成（'+mode+'）：设备 '+j.total+'ms（初始化 '+j.init+' / 数据 '+j.data+' / 刷屏 '+j.refresh+'），往返 '+j.client_ms+'ms','ok');
    await refreshRemote(true);
  }catch(e){ log('全屏失败：'+e.message,'err'); }
  finally{ setBusy(false); }
});
$('btnRefresh').addEventListener('click',async()=>{
  try{
    setBusy(true);
    const j=await api('/api/refresh');
    log('全刷清残影：设备 '+j.total+'ms，往返 '+j.client_ms+'ms','ok');
    await refreshRemote(true);
  }catch(e){ log('全刷失败：'+e.message,'err'); }
  finally{ setBusy(false); }
});

async function poll(){
  try{
    const r=await fetch('/api/status',{cache:'no-store'});
    const j=await r.json();
    setStatus('IP '+j.ip+' · 可用内存 '+(j.heap/1024).toFixed(0)+'KB · 面板 '+(j.powered?'已上电':'已断电')+' · '+j.mode);
  }catch(e){ setStatus('设备离线'); }
  setTimeout(poll,5000);
}

log('就绪：用方向键操作设备本地 UI，或拖入图片上传。');
refreshRemote(true);
loadNote();
loadHost();
setInterval(()=>refreshRemote(false),1500);
poll();
</script>
</body>
</html>
)=====";
