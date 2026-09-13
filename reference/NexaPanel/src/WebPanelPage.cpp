#include "WebPanelPage.h"

const char WEB_PANEL_PAGE[] PROGMEM =
    R"HTML(<!doctype html><html lang="pl"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="color-scheme" content="dark">
<title>Panel kuchenny</title>
<style>
:root{--bg:#09111a;--card:#12202d;--card2:#172837;--line:#294052;--text:#edf5f9;--muted:#94a8b7;--cyan:#24d3e5;--green:#43d39a;--red:#ff6874;--yellow:#f3ca57}
*{box-sizing:border-box}body{margin:0;background:radial-gradient(circle at 15% -10%,#14384b 0,transparent 35%),var(--bg);color:var(--text);font:14px/1.45 system-ui,-apple-system,Segoe UI,sans-serif}
.shell{width:min(1120px,calc(100% - 28px));margin:auto;padding-bottom:45px}.top{display:flex;justify-content:space-between;align-items:center;gap:18px;padding:27px 2px 18px;border-bottom:1px solid var(--line);margin-bottom:17px}
h1{font-size:clamp(25px,4vw,36px);margin:0;line-height:1.1}.sub,.hint{color:var(--muted)}.device{display:flex;align-items:center;gap:9px;white-space:nowrap;color:var(--muted)}
.dot{width:11px;height:11px;border-radius:50%;background:var(--yellow);box-shadow:0 0 12px currentColor}.dot.ok{background:var(--green)}.dot.err{background:var(--red)}
.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:16px}.card{min-width:0;background:linear-gradient(145deg,var(--card2),var(--card));border:1px solid var(--line);border-radius:14px;padding:19px;box-shadow:0 14px 34px #0006}.wide{grid-column:1/-1}
.card h2{display:flex;justify-content:space-between;min-width:0;color:var(--cyan);font-size:15px;text-transform:uppercase;letter-spacing:.1em;margin:0 0 16px}.card h2 .hint{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.status-grid{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:10px}
.metric{background:#0c1721;border:1px solid #203548;border-radius:10px;padding:11px;min-width:0}.metric span{display:block;color:var(--muted);font-size:12px;margin-bottom:5px}.metric strong{display:block;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.badge{display:inline-flex!important;width:max-content;padding:3px 8px;border-radius:999px;background:#263644;color:var(--muted);font-size:11px!important}.badge.ok{background:#123c31;color:var(--green)}.badge.err{background:#451f29;color:var(--red)}.badge.warn{background:#46391b;color:var(--yellow)}
form{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px 14px}.full{grid-column:1/-1}label{display:grid;gap:6px;color:var(--muted);font-size:12px}input{width:100%;background:#09141e;border:1px solid #304b60;border-radius:8px;color:var(--text);padding:10px;font:inherit;outline:none}
input:focus{border-color:var(--cyan);box-shadow:0 0 0 3px #24d3e522}input:disabled{opacity:.4;cursor:not-allowed}.check{display:flex;align-items:center;gap:9px;padding-top:7px}.check input{width:17px;height:17px;accent-color:var(--cyan)}
.secret{display:flex;gap:7px}.secret button{padding:8px 10px}.actions{display:flex;align-items:center;gap:10px;grid-column:1/-1;margin-top:4px;flex-wrap:wrap}
button,.button{border:1px solid #31566a;border-radius:8px;background:#163649;color:var(--text);padding:10px 15px;font:600 13px inherit;cursor:pointer;text-decoration:none;text-align:center}button:hover,.button:hover{border-color:var(--cyan);background:#19465c}button:disabled{opacity:.5;cursor:wait}.danger{background:#421f29;border-color:#71313e}
.button.disabled{opacity:.5;pointer-events:none}.updatebox{width:100%;display:grid;gap:9px}.updatebox .actions{margin:0}.updatebox progress{width:100%;height:18px;accent-color:var(--cyan)}.progressline{display:flex;align-items:center;gap:10px}.progressline progress{flex:1}.progressline span{min-width:42px;text-align:right;color:var(--muted)}
.msg{min-height:20px;color:var(--muted);font-size:12px}.msg.ok{color:var(--green)}.msg.err{color:var(--red)}.msg.warn{color:var(--yellow)}
.row{display:flex;justify-content:space-between;align-items:center;gap:12px;padding:12px 0;border-top:1px solid var(--line)}.row:first-of-type{border-top:0}.row p{margin:3px 0 0;color:var(--muted);font-size:12px}
.modal{display:none;position:fixed;inset:0;place-items:center;background:#000b;padding:18px;z-index:5}.modal.open{display:grid}.dialog{width:min(430px,100%);background:var(--card2);border:1px solid var(--line);border-radius:14px;padding:22px}.dialog h3{margin-top:0}.dialog .actions{justify-content:flex-end}
@media(max-width:760px){.grid{grid-template-columns:1fr}.wide{grid-column:auto}.status-grid{grid-template-columns:repeat(2,minmax(0,1fr))}.top{align-items:flex-start;flex-direction:column}form{grid-template-columns:1fr}.full,.actions{grid-column:auto}}
@media(max-width:430px){.shell{width:calc(100% - 18px)}.card{padding:15px}.status-grid{grid-template-columns:1fr}.row{align-items:stretch;flex-direction:column}.button,button{width:100%}}
</style></head><body><main class="shell">
<header class="top"><div><h1>Panel kuchenny</h1><div class="sub">Konfiguracja i diagnostyka urz&#261;dzenia</div></div><div class="device"><i id="onlineDot" class="dot"></i><span id="onlineText">Po&#322;&#261;czenie...</span><span id="headVersion"></span></div></header>
<div id="globalMsg" class="msg"></div><section class="grid">
<article class="card wide"><h2>Status <span id="statusAge" class="hint"></span></h2><div class="status-grid">
<div class="metric"><span>Firmware</span><strong id="sFirmware">--</strong></div><div class="metric"><span>Build</span><strong id="sBuild">--</strong></div>
<div class="metric"><span>Czas pracy</span><strong id="sUptime">--</strong></div><div class="metric"><span>Czas lokalny</span><strong id="sTime">--</strong></div>
<div class="metric"><span>Wi-Fi</span><strong id="sWifi" class="badge">--</strong></div><div class="metric"><span>IP / RSSI</span><strong id="sIp">--</strong></div>
<div class="metric"><span>MQTT</span><strong id="sMqtt" class="badge">--</strong></div><div class="metric"><span>yoRadio</span><strong id="sRadio" class="badge">--</strong></div>
<div class="metric"><span>NTP</span><strong id="sNtp" class="badge">--</strong></div><div class="metric"><span>Wolny heap</span><strong id="sHeap">--</strong></div>
<div class="metric"><span>Wolny PSRAM</span><strong id="sPsram">--</strong></div><div class="metric"><span>Konfiguracja</span><strong id="sConfig">--</strong></div>
</div></article>
)HTML"
    R"HTML(
<article class="card"><h2>Wi-Fi</h2><form id="wifiForm">
<label class="full">SSID<input id="wifiSsid" maxlength="32" required></label>
<label class="full">Has&#322;o<span class="secret"><input id="wifiPassword" type="password" maxlength="63" autocomplete="new-password" placeholder="Puste = bez zmiany"><button type="button" data-show="wifiPassword">Poka&#380;</button></span><span id="wifiPasswordState" class="hint"></span></label>
<label class="check full"><input id="wifiDhcp" type="checkbox"> DHCP</label>
<label>Statyczny IP<input id="wifiStaticIp" data-static placeholder="192.168.1.50"></label><label>Brama<input id="wifiGateway" data-static placeholder="192.168.1.1"></label>
<label>Maska<input id="wifiSubnet" data-static placeholder="255.255.255.0"></label><label>DNS 1<input id="wifiDns1" data-static></label><label>DNS 2<input id="wifiDns2" data-static></label>
<div class="actions"><button type="submit">Zapisz Wi-Fi</button><span id="wifiMsg" class="msg"></span></div></form></article>

<article class="card"><h2>MQTT / Home Assistant</h2><form id="mqttForm">
<label>Host<input id="mqttHost" maxlength="64" required></label><label>Port<input id="mqttPort" type="number" required></label>
<label>User<input id="mqttUser" maxlength="32"></label><label>Has&#322;o<span class="secret"><input id="mqttPassword" type="password" maxlength="64" autocomplete="new-password" placeholder="Puste = bez zmiany"><button type="button" data-show="mqttPassword">Poka&#380;</button></span><span id="mqttPasswordState" class="hint"></span></label>
<label>Client ID<input id="mqttClientId" maxlength="64" required></label><label>Base topic<input id="mqttBaseTopic" maxlength="64" required></label>
<label>Discovery prefix<input id="discoveryPrefix" maxlength="64" required></label><label class="check"><input id="discoveryEnabled" type="checkbox"> HA Discovery</label>
<div class="actions"><button type="submit">Zapisz MQTT</button><span id="mqttMsg" class="msg"></span></div></form></article>

<article class="card"><h2>yoRadio</h2><form id="radioForm">
<label>Host / IP<input id="yoRadioHost" maxlength="64" required></label><label>Port<input id="yoRadioPort" type="number" required></label>
<label class="full">WebSocket path<input id="yoRadioPath" maxlength="64" required placeholder="/ws"></label>
<div class="actions"><button type="submit">Zapisz yoRadio</button><span id="radioMsg" class="msg"></span></div></form></article>

<article class="card"><h2>Interfejs</h2><form id="uiForm">
<label>Nazwa urz&#261;dzenia<input id="deviceName" maxlength="48" required></label><label>Hostname<input id="hostname" maxlength="32" required></label>
<label>Powr&#243;t do START<input id="uiTimeoutSeconds" type="number" min="5" max="300" step="1" required><span class="hint">sekundy, zakres 5-300 s</span></label>
<div class="actions"><button type="submit">Zapisz interfejs</button><span id="uiMsg" class="msg"></span></div></form></article>

<article class="card"><h2>Aktualizacje</h2>
<div class="row"><div class="updatebox"><div><strong>Firmware ESP</strong><p>Wgraj skompilowany plik .bin do nieaktywnego slotu OTA.</p></div>
<input id="firmwareFile" type="file" accept=".bin,application/octet-stream">
<div class="actions"><button id="firmwareButton" type="button">Aktualizuj firmware</button></div>
<div class="progressline"><progress id="firmwareProgress" value="0" max="100"></progress><span id="firmwarePercent">0%</span></div>
<div id="firmwareMsg" class="msg">Wybierz plik firmware .bin.</div></div></div>
<div class="row"><div class="updatebox"><div><strong>Nextion LCD</strong><p>Wgraj skompilowany plik .tft do wy&#347;wietlacza.</p></div>
<input id="nextionFile" type="file" accept=".tft,application/octet-stream">
<div class="actions"><button id="nextionButton" type="button">Aktualizuj Nextion</button></div>
<div class="progressline"><progress id="nextionProgress" value="0" max="100"></progress><span id="nextionPercent">0%</span></div>
<div id="nextionMsg" class="msg">Wybierz plik Nextiona .tft.</div></div></div></article>

<article class="card"><h2>System</h2>
<div class="row"><div class="updatebox"><div><strong>Konfiguracja</strong><p>Eksport i import ustawie&#324; bez hase&#322;.</p></div>
<div class="actions"><a id="exportConfig" class="button" href="/api/config/export" download>Eksportuj konfiguracj&#281;</a></div>
<input id="importFile" type="file" accept=".json,application/json">
<div class="actions"><button id="importButton" type="button">Importuj konfiguracj&#281;</button></div>
<div id="importMsg" class="msg"></div></div></div>
<div class="row"><div><strong>Restart urz&#261;dzenia</strong><p>Konfiguracja NVS pozostanie zachowana.</p></div><button id="restartButton" type="button">Restart</button></div>
<div class="row"><div><strong>Ustawienia domy&#347;lne</strong><p>Usuwa tylko konfiguracj&#281; panelu z NVS.</p></div><button id="resetButton" class="danger" type="button">Przywr&#243;&#263;</button></div><div id="systemMsg" class="msg"></div></article>
</section></main>
<div id="resetModal" class="modal" role="dialog" aria-modal="true" aria-labelledby="resetTitle"><div class="dialog"><h3 id="resetTitle">Przywr&#243;ci&#263; ustawienia?</h3><p>Konfiguracja runtime zostanie usuni&#281;ta. Restart wykonasz osobno.</p><div class="actions"><button id="cancelReset" type="button">Anuluj</button><button id="confirmReset" class="danger" type="button">Przywr&#243;&#263;</button></div></div></div>
<div id="firmwareModal" class="modal" role="dialog" aria-modal="true" aria-labelledby="firmwareModalTitle"><div class="dialog"><h3 id="firmwareModalTitle">Aktualizowa&#263; firmware?</h3><p>Aktualizacja firmware zrestartuje urz&#261;dzenie. Nie od&#322;&#261;czaj zasilania.</p><div class="actions"><button id="cancelFirmware" type="button">Anuluj</button><button id="confirmFirmware" type="button">Aktualizuj</button></div></div></div>
<div id="nextionModal" class="modal" role="dialog" aria-modal="true" aria-labelledby="nextionModalTitle"><div class="dialog"><h3 id="nextionModalTitle">Aktualizowa&#263; Nextiona?</h3><p>Aktualizacja Nextiona rozpocznie programowanie wy&#347;wietlacza. Nie od&#322;&#261;czaj zasilania ani przewod&#243;w UART.</p><div class="actions"><button id="cancelNextion" type="button">Anuluj</button><button id="confirmNextion" type="button">Aktualizuj</button></div></div></div>
<script>
const byId=id=>document.getElementById(id);
const setValue=(id,value)=>{byId(id).value=value??''};
const setMessage=(id,text,type='')=>{const n=byId(id);n.textContent=text;n.className='msg '+type};
const setBadge=(id,ok,good='ONLINE',bad='OFFLINE')=>{const n=byId(id);n.textContent=ok?good:bad;n.className='badge '+(ok?'ok':'err')};
async function request(url,options={}){const response=await fetch(url,{cache:'no-store',...options});const text=await response.text();let data={};try{data=text?JSON.parse(text):{}}catch(_){data={error:text||('HTTP '+response.status)}}if(!response.ok)throw new Error(data.error||('HTTP '+response.status));return data}
)HTML"
    R"HTML(
function toggleStaticFields(){const disabled=byId('wifiDhcp').checked;document.querySelectorAll('[data-static]').forEach(n=>n.disabled=disabled)}
function fillConfig(c){
 ['deviceName','hostname','wifiSsid','wifiStaticIp','wifiGateway','wifiSubnet','wifiDns1','wifiDns2','mqttHost','mqttUser','mqttClientId','mqttBaseTopic','discoveryPrefix','yoRadioHost','yoRadioPath'].forEach(id=>setValue(id,c[id]));
 setValue('mqttPort',c.mqttPort);setValue('yoRadioPort',c.yoRadioPort);setValue('uiTimeoutSeconds',Math.round((c.uiTimeoutMs||10000)/1000));
 byId('wifiDhcp').checked=!!c.wifiDhcp;byId('discoveryEnabled').checked=!!c.discoveryEnabled;byId('wifiPassword').value='';byId('mqttPassword').value='';
 byId('wifiPasswordState').textContent=c.wifiPasswordSet?'Has\u0142o zapisane':'Brak zapisanego has\u0142a';
 byId('mqttPasswordState').textContent=c.mqttPasswordSet?'Has\u0142o zapisane':'Brak zapisanego has\u0142a';toggleStaticFields()
}
function formatUptime(seconds){seconds=Math.max(0,Number(seconds)||0);const d=Math.floor(seconds/86400);seconds%=86400;const h=Math.floor(seconds/3600);seconds%=3600;const m=Math.floor(seconds/60),s=seconds%60;return(d?d+'d ':'')+(h?h+'h ':'')+(m?m+'m ':'')+s+'s'}
function formatBytes(bytes){bytes=Number(bytes)||0;return bytes>=1048576?(bytes/1048576).toFixed(1)+' MB':(bytes/1024).toFixed(1)+' kB'}
function fillStatus(s){
 byId('sFirmware').textContent=s.firmwareVersion||'--';byId('headVersion').textContent=s.firmwareVersion?'v'+s.firmwareVersion:'';
 byId('sBuild').textContent=s.build||'--';byId('sUptime').textContent=formatUptime(s.uptime);byId('sTime').textContent=s.currentTime||'--';
 setBadge('sWifi',!!s.wifiConnected);setBadge('sMqtt',!!s.mqttConnected);setBadge('sRadio',!!s.yoRadioConnected);setBadge('sNtp',!!s.ntpValid,'OK','ERROR');
 byId('sIp').textContent=(s.ip||'--')+(s.wifiConnected?' / '+s.rssi+' dBm':'');byId('sHeap').textContent=formatBytes(s.freeHeap);byId('sPsram').textContent=formatBytes(s.freePsram);
 byId('sConfig').textContent=(s.configSource||'--')+' / schema '+(s.configSchema??'--');byId('onlineDot').className='dot ok';byId('onlineText').textContent='ONLINE';byId('statusAge').textContent='od\u015bwie\u017cono teraz'
}
let statusLoading=false,updateActive=false;
async function loadStatus(){if(statusLoading||updateActive)return;statusLoading=true;try{fillStatus(await request('/api/status'))}catch(error){byId('onlineDot').className='dot err';byId('onlineText').textContent='OFFLINE';byId('statusAge').textContent='brak po\u0142\u0105czenia'}finally{statusLoading=false}}
async function loadConfig(){try{fillConfig(await request('/api/config'))}catch(error){setMessage('globalMsg','Nie uda\u0142o si\u0119 pobra\u0107 konfiguracji: '+error.message,'err')}}
async function saveSection(form,messageId,payload){
 const button=form.querySelector('button[type="submit"]');button.disabled=true;setMessage(messageId,'Zapisywanie...');
 try{await request('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});if(payload.wifiPassword){byId('wifiPassword').value='';byId('wifiPasswordState').textContent='Has\u0142o zapisane'}if(payload.mqttPassword){byId('mqttPassword').value='';byId('mqttPasswordState').textContent='Has\u0142o zapisane'}setMessage(messageId,'Zapisano. Zmiany wymagaj\u0105 restartu.','ok')}
 catch(error){setMessage(messageId,error.message,'err')}finally{button.disabled=false}
}
byId('wifiDhcp').addEventListener('change',toggleStaticFields);
document.querySelectorAll('[data-show]').forEach(button=>button.addEventListener('click',()=>{const input=byId(button.dataset.show),show=input.type==='password';input.type=show?'text':'password';button.textContent=show?'Ukryj':'Poka\u017c'}));
)HTML"
    R"HTML(
byId('wifiForm').addEventListener('submit',event=>{event.preventDefault();const p={wifiSsid:byId('wifiSsid').value,wifiDhcp:byId('wifiDhcp').checked,wifiStaticIp:byId('wifiStaticIp').value,wifiGateway:byId('wifiGateway').value,wifiSubnet:byId('wifiSubnet').value,wifiDns1:byId('wifiDns1').value,wifiDns2:byId('wifiDns2').value};if(byId('wifiPassword').value)p.wifiPassword=byId('wifiPassword').value;saveSection(event.currentTarget,'wifiMsg',p)});
byId('mqttForm').addEventListener('submit',event=>{event.preventDefault();const p={mqttHost:byId('mqttHost').value,mqttPort:Number(byId('mqttPort').value),mqttUser:byId('mqttUser').value,mqttClientId:byId('mqttClientId').value,mqttBaseTopic:byId('mqttBaseTopic').value,discoveryPrefix:byId('discoveryPrefix').value,discoveryEnabled:byId('discoveryEnabled').checked};if(byId('mqttPassword').value)p.mqttPassword=byId('mqttPassword').value;saveSection(event.currentTarget,'mqttMsg',p)});
byId('radioForm').addEventListener('submit',event=>{event.preventDefault();saveSection(event.currentTarget,'radioMsg',{yoRadioHost:byId('yoRadioHost').value,yoRadioPort:Number(byId('yoRadioPort').value),yoRadioPath:byId('yoRadioPath').value})});
byId('uiForm').addEventListener('submit',event=>{event.preventDefault();saveSection(event.currentTarget,'uiMsg',{deviceName:byId('deviceName').value,hostname:byId('hostname').value,uiTimeoutMs:Math.round(Number(byId('uiTimeoutSeconds').value)*1000)})});
function setUpdateBusy(busy){
 updateActive=busy;
 document.querySelectorAll('button,input').forEach(n=>n.disabled=busy);
 byId('exportConfig').classList.toggle('disabled',busy);
 if(!busy)toggleStaticFields()
}
function uploadError(xhr){try{const data=JSON.parse(xhr.responseText);return data.error||('HTTP '+xhr.status)}catch(_){return xhr.responseText||('HTTP '+xhr.status)}}
byId('firmwareButton').addEventListener('click',()=>{
 const file=byId('firmwareFile').files[0];
 if(!file){setMessage('firmwareMsg','Najpierw wybierz plik .bin.','err');return}
 if(!file.name.toLowerCase().endsWith('.bin')){setMessage('firmwareMsg','Dozwolony jest wy\u0142\u0105cznie plik .bin.','err');return}
 byId('firmwareModal').classList.add('open')
});
byId('cancelFirmware').addEventListener('click',()=>byId('firmwareModal').classList.remove('open'));
byId('confirmFirmware').addEventListener('click',()=>{
 const file=byId('firmwareFile').files[0];
 byId('firmwareModal').classList.remove('open');setUpdateBusy(true);
 byId('firmwareProgress').value=0;byId('firmwarePercent').textContent='0%';setMessage('firmwareMsg','Przygotowanie...','warn');
 const xhr=new XMLHttpRequest();
 xhr.open('POST','/api/firmware/upload?name='+encodeURIComponent(file.name),true);
 xhr.setRequestHeader('Content-Type','application/octet-stream');
 xhr.upload.onprogress=event=>{if(event.lengthComputable){const percent=Math.round(event.loaded*100/event.total);byId('firmwareProgress').value=percent;byId('firmwarePercent').textContent=percent+'%';setMessage('firmwareMsg',percent>=100?'Plik odebrany przez ESP. Trwa finalizacja...':'Wysy\u0142anie firmware: '+percent+'%','warn')}};
 xhr.onload=()=>{if(xhr.status===200){byId('firmwareProgress').value=100;byId('firmwarePercent').textContent='100%';setMessage('firmwareMsg','Firmware zapisany. Urz\u0105dzenie uruchamia si\u0119 ponownie.','ok')}else setMessage('firmwareMsg',uploadError(xhr),'err');setUpdateBusy(false)};
 xhr.onerror=()=>{setMessage('firmwareMsg','B\u0142\u0105d po\u0142\u0105czenia podczas aktualizacji.','err');setUpdateBusy(false)};
 xhr.onabort=()=>{setMessage('firmwareMsg','Wysy\u0142anie firmware przerwane.','err');setUpdateBusy(false)};
 xhr.send(file)
});
byId('firmwareModal').addEventListener('click',event=>{if(event.target===event.currentTarget)event.currentTarget.classList.remove('open')});
byId('nextionButton').addEventListener('click',()=>{
 const file=byId('nextionFile').files[0];
 if(!file){setMessage('nextionMsg','Najpierw wybierz plik .tft.','err');return}
 if(!file.name.toLowerCase().endsWith('.tft')){setMessage('nextionMsg','Dozwolony jest wy\u0142\u0105cznie plik .tft.','err');return}
 byId('nextionModal').classList.add('open')
});
byId('cancelNextion').addEventListener('click',()=>byId('nextionModal').classList.remove('open'));
byId('confirmNextion').addEventListener('click',()=>{
 const file=byId('nextionFile').files[0];
 byId('nextionModal').classList.remove('open');setUpdateBusy(true);
 byId('nextionProgress').value=0;byId('nextionPercent').textContent='0%';setMessage('nextionMsg','Przygotowanie wy\u015bwietlacza...','warn');
 const xhr=new XMLHttpRequest();
 xhr.open('POST','/nextion/upload?name='+encodeURIComponent(file.name)+'&size='+file.size,true);
 xhr.setRequestHeader('Content-Type','application/octet-stream');
 xhr.upload.onprogress=event=>{if(event.lengthComputable){const transport=Math.round(event.loaded*100/event.total),shown=Math.min(transport,99);byId('nextionProgress').value=shown;byId('nextionPercent').textContent=shown+'%';setMessage('nextionMsg',transport>=100?'Plik odebrany przez ESP. Trwa programowanie Nextiona...':'Wysy\u0142anie pliku do ESP: '+transport+'%','warn')}};
 xhr.onload=()=>{if(xhr.status===200){byId('nextionProgress').value=100;byId('nextionPercent').textContent='100%';setMessage('nextionMsg','Aktualizacja Nextiona zako\u0144czona. Urz\u0105dzenie uruchamia si\u0119 ponownie.','ok')}else setMessage('nextionMsg',uploadError(xhr),'err');setUpdateBusy(false)};
 xhr.onerror=()=>{setMessage('nextionMsg','B\u0142\u0105d po\u0142\u0105czenia podczas aktualizacji Nextiona.','err');setUpdateBusy(false)};
 xhr.onabort=()=>{setMessage('nextionMsg','Wysy\u0142anie pliku TFT przerwane.','err');setUpdateBusy(false)};
 xhr.send(file)
});
byId('nextionModal').addEventListener('click',event=>{if(event.target===event.currentTarget)event.currentTarget.classList.remove('open')});
byId('importButton').addEventListener('click',async()=>{
 const file=byId('importFile').files[0],button=byId('importButton');
 if(!file){setMessage('importMsg','Najpierw wybierz plik .json.','err');return}
 if(!file.name.toLowerCase().endsWith('.json')){setMessage('importMsg','Dozwolony jest wy\u0142\u0105cznie plik .json.','err');return}
 button.disabled=true;setMessage('importMsg','Importowanie...','warn');
 try{const body=await file.text();await request('/api/config/import',{method:'POST',headers:{'Content-Type':'application/json'},body});setMessage('importMsg','Konfiguracja zaimportowana. Wymagany restart.','ok');await loadConfig()}
 catch(error){setMessage('importMsg',error.message,'err')}finally{button.disabled=false}
});
byId('restartButton').addEventListener('click',async()=>{const b=byId('restartButton');b.disabled=true;setMessage('systemMsg','Wysy\u0142anie polecenia restartu...','warn');try{await request('/api/restart',{method:'POST'});setMessage('systemMsg','Urz\u0105dzenie uruchamia si\u0119 ponownie.','ok')}catch(error){setMessage('systemMsg',error.message,'err');b.disabled=false}});
byId('resetButton').addEventListener('click',()=>byId('resetModal').classList.add('open'));
byId('cancelReset').addEventListener('click',()=>byId('resetModal').classList.remove('open'));
byId('confirmReset').addEventListener('click',async()=>{const b=byId('confirmReset');b.disabled=true;try{await request('/api/config/reset',{method:'POST'});byId('resetModal').classList.remove('open');setMessage('systemMsg','Ustawienia przywr\u00f3cone. Wymagany restart.','ok');await loadConfig()}catch(error){byId('resetModal').classList.remove('open');setMessage('systemMsg',error.message,'err')}finally{b.disabled=false}});
byId('resetModal').addEventListener('click',event=>{if(event.target===event.currentTarget)event.currentTarget.classList.remove('open')});
loadConfig();loadStatus();setInterval(loadStatus,5000);
</script></body></html>)HTML"
    ;

const size_t WEB_PANEL_PAGE_LENGTH = sizeof(WEB_PANEL_PAGE) - 1U;
