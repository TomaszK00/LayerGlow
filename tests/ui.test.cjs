const fs=require('node:fs');
const path=require('node:path');
const vm=require('node:vm');
const crypto=require('node:crypto');
const assert=require('node:assert/strict');
const {test}=require('node:test');
const {JSDOM,VirtualConsole}=require('jsdom');
const root=path.resolve(__dirname,'..');
const source=fs.readFileSync(path.join(root,'PlatformIO/src/main.cpp'),'utf8');
const html=source.split('R"HTML(')[1].split(')HTML";')[0];
const scripts=[...html.matchAll(/<script(?:\s[^>]*)?>([\s\S]*?)<\/script>/g)].map(x=>x[1]);
const script=scripts[1];
const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));
function fixture(){
 const rows={};for(const id of ['progress','heating','paused','complete','error','standby'])rows[id]={color:'#00c8f0',colours:Array(12).fill('#ff9e35'),colourCount:1,brightness:65,pulse:false,enabled:true,period:1400,style:0};
 return {version:'2.46',bootId:'abc123',rows,ssid:'',ip:'192.168.1.9',port:7125,ledCount:24,printerType:'moonraker',state:'standby',connected:false,printerEnabled:true,ap:true,sim:false,simState:'standby',simProgress:0,progress:0,recentColours:[]};
}
function panel(status=fixture()){
 const errors=[],calls=[];const vc=new VirtualConsole();vc.on('jsdomError',e=>errors.push(e.message));
 const dom=new JSDOM(html,{url:'http://layerglow.local/',runScripts:'dangerously',virtualConsole:vc,beforeParse(w){
  w.matchMedia=()=>({matches:false,addEventListener(){},removeEventListener(){}});
  w.ResizeObserver=class{observe(){}};w.AbortSignal.timeout=()=>new w.AbortController().signal;
  w.fetch=async(url,options={})=>{
   calls.push({url:String(url),options});let body={};
   if(String(url).startsWith('/api/status'))body=JSON.parse(JSON.stringify(status));
   else if(url==='/api/colours/recent'){
    const colour=new URLSearchParams(options.body.toString()).get('colour');status.recentColours=[colour,...status.recentColours.filter(x=>x!==colour)].slice(0,8);body={recentColours:status.recentColours};
   }else if(url==='/api/simulation'){
    const values=new URLSearchParams(options.body.toString());status.sim=values.get('enabled')==='1';status.simState=values.get('status');body={ok:true};
   }
   return {ok:true,json:async()=>body,text:async()=>JSON.stringify(body)};
  };
 }});
 return {dom,errors,calls,status};
}
test('embedded scripts parse; all four languages cover the UI and FAQ',()=>{
 scripts.forEach(code=>new vm.Script(code));
 const T=vm.runInNewContext(script.slice(0,script.indexOf('const FAQ_CONTENT='))+'\nT');
 const start=script.indexOf('const FAQ_CONTENT=');
 const FAQ=vm.runInNewContext(script.slice(start,script.indexOf(';\n',start)+1)+'\nFAQ_CONTENT');
 const keys=[...html.matchAll(/\bdata-(?:i|aria-i)="([A-Za-z_][\w]*)"/g)].map(x=>x[1]);
 for(const lang of ['en','pl','es','de']){
  for(const key of keys)assert.ok(T[lang][key],lang+': '+key);
  assert.deepEqual(Object.keys(T[lang]).sort(),Object.keys(T.en).sort());
  assert.deepEqual(FAQ[lang].sections.map(x=>x[1].length),FAQ.en.sections.map(x=>x[1].length));
 }
});
test('LED adjustments are exclusive and preserve their selected values',async()=>{
 const p=panel();try{
  await pause(30);const d=p.dom.window.document;
  const pulse=d.getElementById('pp_progress').closest('details');
  const brightness=d.getElementById('b_progress').closest('details');
  const other=d.getElementById('pp_heating').closest('details');
  pulse.open=true;await pause(10);brightness.open=true;await pause(10);
  assert.equal(pulse.open,false);assert.equal(brightness.open,true);
  other.open=true;await pause(10);
  assert.equal(brightness.open,false);assert.equal(other.open,true);
  assert.equal(d.getElementById('pp_progress').value,'1400');
  assert.equal(d.getElementById('b_progress').value,'65');assert.deepEqual(p.errors,[]);
 }finally{p.dom.window.close();}
});
test('live printer data is labelled in every language and visually distinct from simulation',async()=>{
 const p=panel();try{
  await pause(30);const w=p.dom.window,d=w.document,select=d.getElementById('testStatus');
  for(const language of ['en','pl','es','de']){
   w.eval('setLang('+JSON.stringify(language)+')');
   assert.match(select.options[0].textContent,/^● /);
  }
  assert.equal(select.classList.contains('live-reading'),true);
  select.value='printing';select.dispatchEvent(new w.Event('change',{bubbles:true}));await pause(150);
  assert.equal(select.classList.contains('live-reading'),false);
  select.value='printer';select.dispatchEvent(new w.Event('change',{bubbles:true}));await pause(150);
  assert.equal(select.classList.contains('live-reading'),true);assert.deepEqual(p.errors,[]);
 }finally{p.dom.window.close();}
});
test('OTA needs evidence of restart and the requested version',()=>{
 const fn=vm.runInNewContext(script.slice(script.indexOf('function firmwareRestarted('),script.indexOf('async function waitForRestart('))+'\nfirmwareRestarted');
 const before={version:'2.46',bootId:'old',uptimeSeconds:100};
 assert.equal(fn(before,{...before,uptimeSeconds:110}),false);
 assert.equal(fn(before,{...before,bootId:'new'}),true);
 assert.equal(fn(before,{...before,bootId:'new'},'2.47'),false);
 assert.equal(fn(before,{version:'2.47',bootId:'new'},'2.47'),true);
 assert.equal(fn({version:'2.45',uptimeSeconds:100},{version:'2.45',uptimeSeconds:2}),true);
});
test('SHA-256 fallback matches Node for the actual published binary',()=>{
 const sha=vm.runInNewContext(script.slice(script.indexOf('function sha256('),script.indexOf('const versionParts='))+'\nsha256');
 const manifest=JSON.parse(fs.readFileSync(path.join(root,'Software/update.json')));
 const bytes=fs.readFileSync(path.join(root,'Software',manifest.file));
 assert.equal(bytes.length,manifest.size);
 assert.equal(sha(bytes),crypto.createHash('sha256').update(bytes).digest('hex'));
 assert.equal(sha(bytes),manifest.sha256);
});
test('native colour picker remembers custom value across swatches and reload',async()=>{
 const p=panel();try{
  await pause(30);assert.deepEqual(p.errors,[]);const w=p.dom.window,d=w.document;
  assert.equal(d.querySelectorAll('.led-row').length,6);
  const colour=d.getElementById('c_progress');assert.equal(colour.type,'color');
  assert.equal(colour.getAttribute('list'),'layerglow-colour-presets');
  colour.value='#123456';colour.dispatchEvent(new w.Event('change',{bubbles:true}));await pause(30);
  d.getElementById('c_heating').value='#ff0000';d.getElementById('c_heating').dispatchEvent(new w.Event('change',{bubbles:true}));await pause(30);
  assert.equal(d.getElementById('savedCustomColour').value,'#123456');
  assert.equal(p.calls.filter(x=>x.url==='/api/colours/recent').length,1);
  const reload=panel(p.status);try{await pause(30);assert.deepEqual(reload.errors,[]);assert.equal(reload.dom.window.document.getElementById('savedCustomColour').value,'#123456');}finally{reload.dom.window.close();}
 }finally{p.dom.window.close();}
});
test('palette limits, live/simulation selection and stale filename',async()=>{
 const p=panel();try{
  await pause(30);const w=p.dom.window,d=w.document;
  for(let n=1;n<12;n++)d.getElementById('add_progress').click();
  assert.equal(d.getElementById('n_progress').value,'12');assert.equal(d.getElementById('add_progress').disabled,true);
  for(let n=12;n>1;n--)d.getElementById('remove_progress').click();
  assert.equal(d.getElementById('n_progress').value,'1');assert.equal(d.getElementById('remove_progress').disabled,true);
  const select=d.getElementById('testStatus');select.value='printing';select.dispatchEvent(new w.Event('change',{bubbles:true}));await pause(30);assert.equal(p.status.sim,true);
  select.value='printer';select.dispatchEvent(new w.Event('change',{bubbles:true}));await pause(30);assert.equal(p.status.sim,false);
  p.status.filename='stale.gcode';await w.eval('refresh()');assert.equal(d.getElementById('file').style.display,'none');
  assert.deepEqual(p.errors,[]);
 }finally{p.dom.window.close();}
});
