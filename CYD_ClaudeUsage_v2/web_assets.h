// ═══════════════════════════════════════════════════════
//  web_assets.h — HTML/CSS/JS served by the monitor
//    SETUP_HTML     first-time setup portal (192.168.4.1)
//    SETTINGS_HTML  settings page on your network
//                   (http://claude-monitor.local)
//  Placeholders %CSS%, %TZJS% and %WIFI_OPTIONS% are filled
//  in when a page is served.
// ═══════════════════════════════════════════════════════
#pragma once
#include <Arduino.h>

static const char WEB_CSS[] PROGMEM = R"rawliteral(
:root{--bg:#121110;--card:#1f1d1b;--line:#3a3632;--text:#f4f0e8;--dim:#ada79e;
      --accent:#e5825f;--ok:#66c792;--warn:#f4b740;--crit:#f4675f}
*{box-sizing:border-box;margin:0}
html{font-size:18px}
body{background:var(--bg);color:var(--text);font-family:system-ui,-apple-system,"Segoe UI",sans-serif;
     line-height:1.45;padding:20px 16px 40px}
main{max-width:520px;margin:0 auto}
header{display:flex;align-items:center;gap:12px;margin:4px 0 18px}
.dot{width:14px;height:14px;border-radius:50%;background:var(--accent);flex:none}
h1{font-size:1.6rem;font-weight:700;letter-spacing:-.01em}
h2{font-size:.85rem;text-transform:uppercase;letter-spacing:.08em;color:var(--dim);
   margin:26px 0 10px;font-weight:600}
h2:first-child{margin-top:0}
.card{background:var(--card);border:1px solid var(--line);border-radius:18px;padding:20px;margin-bottom:16px}
label{display:block;color:var(--dim);font-size:.95rem;margin:14px 0 6px}
select,input[type=text],input[type=password]{display:block;width:100%;margin-top:6px;padding:12px 14px;
     font-size:1.05rem;color:var(--text);background:var(--bg);border:1px solid var(--line);border-radius:12px;outline:0}
select:focus,input:focus{border-color:var(--accent)}
input.valid{border-color:var(--ok)} input.invalid{border-color:var(--crit)}
label.check{display:flex;align-items:center;gap:12px;color:var(--text);font-size:1.05rem;margin-top:18px}
label.check input{width:24px;height:24px;accent-color:var(--accent)}
.row{display:flex;gap:12px}.row>*{flex:1}
button{display:block;width:100%;margin-top:22px;padding:15px;font-size:1.1rem;font-weight:700;
       border:0;border-radius:14px;background:var(--accent);color:#1a1210;cursor:pointer}
button.ghost{background:transparent;color:var(--text);border:1px solid var(--line);margin-top:16px;font-weight:600}
button:disabled{opacity:.5;cursor:wait}
.hint{color:var(--dim);font-size:.9rem;margin-top:8px}
.foot{color:var(--dim);font-size:.9rem;text-align:center;margin-top:18px}
#status{text-align:center;margin-top:12px;min-height:1.4em;font-weight:600}
.ok{color:var(--ok)} .err{color:var(--crit)}
.stats{display:flex;flex-wrap:wrap;gap:14px}
.stat{flex:1 1 200px;background:var(--bg);border-radius:14px;padding:14px 16px}
.lbl{color:var(--dim);font-size:.95rem}
.big{font-size:2.6rem;font-weight:700;line-height:1.1;letter-spacing:-.02em}
.sub{color:var(--dim);font-size:.9rem;margin-top:4px;white-space:pre-line}
.meta{color:var(--dim);font-size:.9rem;margin-top:14px}
code{display:block;background:var(--bg);border:1px solid var(--line);border-radius:10px;padding:12px 14px;
     color:var(--accent);font-family:ui-monospace,Menlo,monospace;font-size:1rem;cursor:pointer;margin:8px 0}
.small{font-size:.85rem;color:var(--dim)}
)rawliteral";

// Time zones: [label, POSIX TZ string, IANA names used to auto-detect]
static const char WEB_TZJS[] PROGMEM = R"rawliteral(
const TZ=[
["Hawaii","HST10","Pacific/Honolulu"],
["Alaska","AKST9AKDT,M3.2.0,M11.1.0","America/Anchorage America/Juneau"],
["Pacific (US & Canada)","PST8PDT,M3.2.0,M11.1.0","America/Los_Angeles America/Vancouver America/Tijuana"],
["Arizona","MST7","America/Phoenix"],
["Mountain (US & Canada)","MST7MDT,M3.2.0,M11.1.0","America/Denver America/Edmonton America/Boise"],
["Central (US & Canada)","CST6CDT,M3.2.0,M11.1.0","America/Chicago America/Winnipeg"],
["Mexico City","CST6","America/Mexico_City America/Monterrey"],
["Eastern (US & Canada)","EST5EDT,M3.2.0,M11.1.0","America/New_York America/Toronto America/Detroit America/Indiana/Indianapolis"],
["Atlantic (Canada)","AST4ADT,M3.2.0,M11.1.0","America/Halifax"],
["Newfoundland","NST3:30NDT,M3.2.0,M11.1.0","America/St_Johns"],
["Brazil (Sao Paulo)","<-03>3","America/Sao_Paulo"],
["Argentina","<-03>3","America/Argentina/Buenos_Aires America/Buenos_Aires"],
["UTC","UTC0","UTC Etc/UTC"],
["UK, Ireland, Portugal","GMT0BST,M3.5.0/1,M10.5.0","Europe/London Europe/Dublin Europe/Lisbon"],
["Central Europe","CET-1CEST,M3.5.0,M10.5.0/3","Europe/Paris Europe/Berlin Europe/Madrid Europe/Rome Europe/Amsterdam Europe/Brussels Europe/Vienna Europe/Stockholm Europe/Oslo Europe/Copenhagen Europe/Warsaw Europe/Prague Europe/Zurich Europe/Budapest"],
["Eastern Europe","EET-2EEST,M3.5.0/3,M10.5.0/4","Europe/Athens Europe/Kiev Europe/Kyiv Europe/Helsinki Europe/Bucharest Europe/Sofia Europe/Riga Europe/Vilnius Europe/Tallinn"],
["Turkey","<+03>-3","Europe/Istanbul"],
["Moscow","MSK-3","Europe/Moscow"],
["Gulf (Dubai)","<+04>-4","Asia/Dubai"],
["India","IST-5:30","Asia/Kolkata Asia/Calcutta"],
["Bangkok, Jakarta","<+07>-7","Asia/Bangkok Asia/Jakarta Asia/Ho_Chi_Minh"],
["Singapore, Manila","<+08>-8","Asia/Singapore Asia/Manila Asia/Kuala_Lumpur"],
["China, Hong Kong, Taiwan","CST-8","Asia/Shanghai Asia/Hong_Kong Asia/Taipei"],
["Perth","AWST-8","Australia/Perth"],
["Japan","JST-9","Asia/Tokyo"],
["Korea","KST-9","Asia/Seoul"],
["Adelaide","ACST-9:30ACDT,M10.1.0,M4.1.0/3","Australia/Adelaide"],
["Darwin","ACST-9:30","Australia/Darwin"],
["Brisbane","AEST-10","Australia/Brisbane"],
["Sydney, Melbourne","AEST-10AEDT,M10.1.0,M4.1.0/3","Australia/Sydney Australia/Melbourne Australia/Hobart"],
["New Zealand","NZST-12NZDT,M9.5.0,M4.1.0/3","Pacific/Auckland"]];
function detectTz(){try{const z=Intl.DateTimeFormat().resolvedOptions().timeZone;
  for(const t of TZ) if(t[2].split(' ').includes(z)) return t[1];}catch(e){} return '';}
function fillTz(sel,current){sel.innerHTML='';
  if(current&&!TZ.some(t=>t[1]==current)) sel.add(new Option('Current ('+current+')',current));
  for(const t of TZ) sel.add(new Option(t[0],t[1]));
  sel.value=current||detectTz()||'UTC0';}
)rawliteral";

// ── First-time setup portal ───────────────────────────
static const char SETUP_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Claude Monitor setup</title><style>%CSS%</style></head>
<body><main>
<header><span class="dot"></span><h1>Claude Monitor</h1></header>
<form id="f">
<div class="card">
  <h2>Step 1 &middot; Your Claude token</h2>
  <p class="small">On your computer, run this in a terminal (needs Claude Code):</p>
  <code id="cmd" onclick="copyCmd()">claude setup-token</code>
  <p class="small">It opens a browser to sign in, then prints a token starting with <b>sk-ant-oat01-</b>. Paste it here:</p>
  <label for="token">OAuth token
    <input id="token" name="token" type="password" required maxlength="512" autocomplete="off"
           placeholder="sk-ant-oat01-..." oninput="validateToken(this)"></label>
  <div class="hint" id="token-status"></div>
</div>
<div class="card">
  <h2>Step 2 &middot; Wi-Fi (2.4 GHz)</h2>
  <label>Network
    <select id="ssid-select" onchange="onNetworkSelect(this)">
      <option value="">Choose a network</option>
      %WIFI_OPTIONS%
      <option value="__manual__">Other (type it in)</option>
    </select></label>
  <button type="button" class="ghost" onclick="rescan()">Scan again</button>
  <div id="ssid-manual-field" style="display:none">
    <label for="ssid-manual">Network name<input id="ssid-manual" type="text" maxlength="32" autocomplete="off"></label>
  </div>
  <input type="hidden" id="ssid" name="ssid">
  <label for="wifipass">Password
    <input id="wifipass" name="wifipass" type="password" maxlength="64" autocomplete="off"
           placeholder="Leave blank for open networks"></label>
</div>
<div class="card">
  <h2>Step 3 &middot; Preferences</h2>
  <label>Time zone <select id="tz" name="tz"></select></label>
  <div class="row">
    <label>Check usage every
      <select name="poll_sec">
        <option value="60">1 minute</option><option value="120" selected>2 minutes</option>
        <option value="300">5 minutes</option></select></label>
    <label>Brightness
      <select name="brightness">
        <option value="1">Dim</option><option value="2" selected>Normal</option>
        <option value="3">Bright</option></select></label>
  </div>
  <p class="hint">You can change these later at http://claude-monitor.local</p>
  <button type="submit" id="btn">Save &amp; restart monitor</button>
  <p id="status"></p>
</div>
</form>
</main>
<script>%TZJS%
fillTz(document.getElementById('tz'),'');
function copyCmd(){navigator.clipboard&&navigator.clipboard.writeText('claude setup-token').then(()=>{
  const e=document.getElementById('cmd');e.style.color='var(--ok)';setTimeout(()=>e.style.color='',1500);});}
function validateToken(i){const v=i.value.trim(),st=document.getElementById('token-status');
  if(!v){i.className='';st.textContent='';return;}
  if(v.startsWith('sk-ant-')&&v.length>20){i.className='valid';st.className='hint ok';st.textContent='Token looks right';}
  else{i.className='invalid';st.className='hint err';st.textContent='Tokens start with sk-ant-oat01-';}}
function onNetworkSelect(s){document.getElementById('ssid-manual-field').style.display=s.value==='__manual__'?'block':'none';}
function rescan(){const st=document.getElementById('status');st.className='';st.textContent='Scanning...';
  fetch('/scan').then(r=>r.text()).then(h=>{const s=document.getElementById('ssid-select'),p=s.value;
    s.innerHTML='<option value="">Choose a network</option>'+h+'<option value="__manual__">Other (type it in)</option>';
    s.value=p;st.textContent='';}).catch(()=>{st.className='err';st.textContent='Scan failed';});}
document.getElementById('f').addEventListener('submit',async e=>{
  e.preventDefault();const b=document.getElementById('btn'),st=document.getElementById('status');
  const tok=document.getElementById('token').value.trim(),sel=document.getElementById('ssid-select');
  const ssid=sel.value==='__manual__'?document.getElementById('ssid-manual').value.trim():sel.value;
  document.getElementById('ssid').value=ssid;
  if(!ssid){st.className='err';st.textContent='Please choose a Wi-Fi network.';return;}
  if(!tok.startsWith('sk-ant-')){st.className='err';st.textContent='That token doesn\'t look right. Run claude setup-token and copy all of it.';return;}
  b.disabled=true;st.className='';st.textContent='Saving...';
  try{const body=new URLSearchParams(new FormData(e.target));body.set('token',tok);
    const r=await fetch('/provision',{method:'POST',body});
    if(r.ok){st.className='ok';st.textContent='Saved! The monitor is restarting and joining your Wi-Fi.';}
    else{st.className='err';st.textContent='Error: '+await r.text();b.disabled=false;}}
  catch(x){st.className='ok';st.textContent='Saved! The monitor is restarting.';}});
</script></body></html>)rawliteral";

// ── Settings page on your network ─────────────────────
static const char SETTINGS_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Claude Monitor</title><style>%CSS%</style></head>
<body><main>
<header><span class="dot"></span><h1>Claude Monitor</h1></header>
<section class="card">
  <div class="stats">
    <div class="stat"><div class="lbl">5-hour</div><div class="big" id="h5">--</div><div class="sub" id="h5r"></div></div>
    <div class="stat"><div class="lbl">Weekly</div><div class="big" id="d7">--</div><div class="sub" id="d7r"></div></div>
  </div>
  <div class="meta" id="meta">Loading...</div>
  <button type="button" class="ghost" id="rf" onclick="refreshNow()">Refresh now</button>
</section>
<form id="f" class="card">
  <h2>Clock</h2>
  <label>Time zone <select id="tz" name="tz"></select></label>
  <p class="hint" id="tzhint"></p>
  <label>Clock style
    <select name="clock24"><option value="0">12-hour (4:30 PM)</option><option value="1">24-hour (16:30)</option></select></label>
  <h2>Screen</h2>
  <label>Brightness
    <select name="brightness"><option value="0">Off</option><option value="1">Dim</option>
      <option value="2">Normal</option><option value="3">Bright</option></select></label>
  <label class="check"><input type="checkbox" name="night_on" value="1"> Dim the screen at night</label>
  <div class="row">
    <label>From <select name="night_from" class="hours"></select></label>
    <label>Until <select name="night_to" class="hours"></select></label>
  </div>
  <label>At night the screen is
    <select name="night_lvl"><option value="1">Dim</option><option value="0">Off (tap it to wake)</option></select></label>
  <label>Show this page first
    <select name="start_page"><option value="0">Overview</option><option value="1">5-hour</option>
      <option value="2">Weekly</option><option value="3">Device</option></select></label>
  <label>Cycle pages when idle
    <select name="rotate_sec"><option value="0">No</option><option value="10">Every 10 seconds</option>
      <option value="20">Every 20 seconds</option><option value="30">Every 30 seconds</option>
      <option value="60">Every minute</option></select></label>
  <h2>Updates</h2>
  <label>Check usage every
    <select name="poll_sec"><option value="30">30 seconds</option><option value="60">1 minute</option>
      <option value="120">2 minutes</option><option value="300">5 minutes</option>
      <option value="600">10 minutes</option></select></label>
  <button type="submit" id="save">Save</button>
  <p id="status"></p>
</form>
<p class="foot">To change Wi-Fi or the Claude token, hold the BOOT button on the monitor for 5 seconds.</p>
</main>
<script>%TZJS%
const f=document.getElementById('f'),$=id=>document.getElementById(id);
let clock24=false;
function hourLabel(h){return clock24?String(h).padStart(2,'0')+':00':((h%12)||12)+(h<12?' AM':' PM');}
function fillHours(){for(const s of document.querySelectorAll('.hours')){const v=s.value;s.innerHTML='';
  for(let h=0;h<24;h++)s.add(new Option(hourLabel(h),h));if(v!=='')s.value=v;}}
function dur(s){if(s<60)return'<1m';const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);
  return d?d+'d '+h+'h':h?h+'h '+String(m).padStart(2,'0')+'m':m+'m';}
function when(t){const d=new Date(t*1000),o={hour:'numeric',minute:'2-digit',hour12:!clock24};
  if(d.toDateString()!==new Date().toDateString())o.weekday='short';return d.toLocaleString([],o);}
function resetText(r,now){return r?'Resets in '+dur(Math.max(0,r-now))+'\n'+when(r):'';}
function showState(s){
  const now=s.now||Math.floor(Date.now()/1000);
  if(s.hasData){$('h5').textContent=Math.round(s.h5)+'%';$('d7').textContent=Math.round(s.d7)+'%';
    $('h5r').textContent=resetText(s.h5r,now);$('d7r').textContent=resetText(s.d7r,now);}
  const net=['Up to date','Loading','Offline','Token rejected','Update failed'][s.net]||'';
  $('meta').textContent=net+(s.last?' · updated '+dur(Math.max(0,now-s.last))+' ago':'')+
    (s.err?' · '+s.err:'')+' · Wi-Fi '+s.rssi+' dBm · v'+s.v;}
async function load(first){
  try{const s=await (await fetch('/api/state')).json();showState(s);
    if(first){clock24=!!s.clock24;fillHours();fillTz($('tz'),s.tz);
      $('tzhint').textContent=s.tz?'':(detectTz()?'Picked from your browser. Press Save to use it.':'Choose your time zone, then press Save.');
      for(const k of['clock24','brightness','night_from','night_to','night_lvl','start_page','rotate_sec','poll_sec'])
        if(f.elements[k])f.elements[k].value=String(+s[k]);
      f.elements.night_on.checked=!!s.night_on;}}
  catch(e){$('meta').textContent='Can\'t reach the monitor.';}}
f.elements.clock24.addEventListener('change',e=>{clock24=e.target.value==='1';fillHours();});
f.addEventListener('submit',async e=>{e.preventDefault();const st=$('status');st.className='';st.textContent='Saving...';
  const body=new URLSearchParams(new FormData(f));if(!f.elements.night_on.checked)body.set('night_on','0');
  try{const r=await fetch('/api/settings',{method:'POST',body});
    if(r.ok){st.className='ok';st.textContent='Saved';$('tzhint').textContent='';load(false);}
    else{st.className='err';st.textContent=await r.text();}}
  catch(x){st.className='err';st.textContent='Couldn\'t save. Is the monitor on?';}});
async function refreshNow(){const b=$('rf');b.disabled=true;b.textContent='Refreshing...';
  try{await fetch('/api/refresh',{method:'POST'});}catch(e){}
  setTimeout(()=>{load(false);b.disabled=false;b.textContent='Refresh now';},6000);}
load(true);setInterval(()=>load(false),30000);
</script></body></html>)rawliteral";
