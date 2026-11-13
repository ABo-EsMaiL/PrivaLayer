const $=s=>document.querySelector(s), $$=s=>[...document.querySelectorAll(s)], send=m=>{try{window.chrome?.webview?.postMessage(m)}catch(_){}};
const icons={grid:'<rect x="3" y="3" width="7" height="7" rx="1.4"/><rect x="14" y="3" width="7" height="7" rx="1.4"/><rect x="14" y="14" width="7" height="7" rx="1.4"/><rect x="3" y="14" width="7" height="7" rx="1.4"/>',note:'<path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><path d="M14 2v6h6M8 13h8M8 17h6"/>',globe:'<circle cx="12" cy="12" r="9"/><path d="M3 12h18M12 3a15 15 0 0 1 0 18M12 3a15 15 0 0 0 0 18"/>',spark:'<path d="m12 3 1.9 5.8L20 11l-6.1 2.2L12 19l-2-5.8L4 11l6-2.2L12 3z"/><path d="m19 14 1.1 2.3L22 17l-1.9.8L19 20l-.9-2.2L16 17l2.1-.7L19 14z"/>',layers:'<path d="m12 3 9 5-9 5-9-5 9-5z"/><path d="m3 12 9 5 9-5M3 16l9 5 9-5"/>',sliders:'<path d="M4 21v-7M4 10V3M12 21v-9M12 8V3M20 21v-5M20 12V3M2 14h4M10 8h4M18 16h4"/>',panel:'<rect x="3" y="4" width="18" height="16" rx="2"/><path d="M8 4v16"/>',lock:'<rect x="4" y="10" width="16" height="11" rx="2"/><path d="M8 10V7a4 4 0 1 1 8 0v3"/>',scan:'<path d="M7 3H5a2 2 0 0 0-2 2v2M17 3h2a2 2 0 0 1 2 2v2M21 17v2a2 2 0 0 1-2 2h-2M3 17v2a2 2 0 0 0 2 2h2M3 12h18"/>',minus:'<path d="M5 12h14"/>',x:'<path d="m18 6-12 12M6 6l12 12"/>','arrow-up-right':'<path d="M7 17 17 7M7 7h10v10"/>','arrow-right':'<path d="M5 12h14M13 6l6 6-6 6"/>',plus:'<path d="M12 5v14M5 12h14"/>',command:'<path d="M9 7V5a2 2 0 1 0-2 2h10a2 2 0 1 0-2-2v14a2 2 0 1 0 2-2H7a2 2 0 1 0 2 2V7z"/>',search:'<circle cx="10.8" cy="10.8" r="6.8"/><path d="m16 16 4.5 4.5"/>',trash:'<path d="M3 6h18M8 6V4h8v2M19 6l-1 14H6L5 6M10 11v5M14 11v5"/>',copy:'<rect x="8" y="8" width="13" height="13" rx="2"/><path d="M16 8V5a2 2 0 0 0-2-2H5a2 2 0 0 0-2 2v9a2 2 0 0 0 2 2h3"/>',sun:'<circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4m11.4 11.4 1.4 1.4M2 12h2m16 0h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/>',mouse:'<rect x="6" y="2" width="12" height="20" rx="6"/><path d="M12 2v6"/>',shield:'<path d="M12 22s8-4 8-11V5l-8-3-8 3v6c0 7 8 11 8 11z"/>','shield-check':'<path d="M12 22s8-4 8-11V5l-8-3-8 3v6c0 7 8 11 8 11z"/><path d="m9 12 2 2 4-4"/>',info:'<circle cx="12" cy="12" r="10"/><path d="M12 16v-4M12 8h.01"/>',alert:'<path d="M10.3 3.9 2 18a2 2 0 0 0 1.7 3h16.6a2 2 0 0 0 1.7-3L13.7 3.9a2 2 0 0 0-3.4 0z"/><path d="M12 9v4M12 17h.01"/>',keyboard:'<rect x="2" y="5" width="20" height="14" rx="2"/><path d="M6 9h.01M10 9h.01M14 9h.01M18 9h.01M6 13h.01M10 13h.01M14 13h.01M18 13h.01M8 16h8"/>',image:'<rect x="3" y="3" width="18" height="18" rx="2"/><circle cx="8.5" cy="8.5" r="1.5"/><path d="m21 15-5-5L5 21"/>',check:'<path d="m5 12 4 4L19 6"/>',
back:'<path d="m15 18-6-6 6-6"/>',forward:'<path d="m9 18 6-6-6-6"/>',
refresh:'<path d="M20 7v5h-5"/><path d="M4.9 9a8 8 0 0 1 13.3-2L20 12M4 17v-5h5"/><path d="M19.1 15a8 8 0 0 1-13.3 2L4 12"/>',
home:'<path d="m3 10 9-7 9 7"/><path d="M5 9v12h14V9M9 21v-7h6v7"/>',
close:'<path d="m18 6-12 12M6 6l12 12"/>'};
$$('[data-icon]').forEach(e=>{const p=icons[e.dataset.icon];if(p)e.innerHTML='<svg viewBox="0 0 24 24" aria-hidden="true">'+p+'</svg>';});
const shell=$('#appShell');let page='home',collapsed=localStorage.getItem('pl-collapsed')==='1';
// Compact Pomodoro timer. It runs independently of page navigation.
const FOCUS_SECONDS=25*60;
let focusRemaining=FOCUS_SECONDS, focusInterval=null, focusRunning=false;
function renderFocus(){
 const mins=Math.floor(focusRemaining/60),secs=focusRemaining%60;
 $('#focusTime').textContent=String(mins).padStart(2,'0')+':'+String(secs).padStart(2,'0');
 const progress=(FOCUS_SECONDS-focusRemaining)/FOCUS_SECONDS;
 $('#focusRing').style.setProperty('--progress',String(progress));
 $('#focusStatus').textContent=focusRunning?'FOCUS IN PROGRESS':focusRemaining===0?'SESSION COMPLETE':'READY TO FOCUS';
 $('#focusStart').innerHTML=(focusRunning?'<svg viewBox="0 0 24 24"><path d="M8 5h3v14H8zM15 5h3v14h-3z"/></svg> Pause focus':'<svg viewBox="0 0 24 24"><path d="m8 5 12 7-12 7z"/></svg> '+(focusRemaining===FOCUS_SECONDS?'Start focus':'Resume focus'));
}
function stopFocus(){clearInterval(focusInterval);focusInterval=null;focusRunning=false;}
$('#focusStart').addEventListener('click',()=>{
 if(focusRunning){stopFocus();renderFocus();return;}
 if(focusRemaining===0)focusRemaining=FOCUS_SECONDS;
 focusRunning=true;renderFocus();
 focusInterval=setInterval(()=>{
  if(focusRemaining>0){focusRemaining--;renderFocus();}
  if(focusRemaining===0){stopFocus();renderFocus();toast('Focus session complete — take a short break');$('#focusTip').textContent='You did the work. Take a breath before the next sprint.';}
 },1000);
});
$('#focusReset').addEventListener('click',()=>{stopFocus();focusRemaining=FOCUS_SECONDS;$('#focusTip').textContent='Silence the noise. Pick one task. Begin.';renderFocus();});
renderFocus();
function toast(s){$('#toastText').textContent=s;$('#toast').classList.add('show');clearTimeout(toast.t);toast.t=setTimeout(()=>$('#toast').classList.remove('show'),2300);}
function setCollapsed(v){collapsed=v;shell.classList.toggle('sidebar-collapsed',v);localStorage.setItem('pl-collapsed',v?'1':'0');send('sidebar-collapsed|'+(v?'1':'0'));}
function navigate(p){if(!$('#page-'+p))return;page=p;$$('.page').forEach(e=>e.classList.toggle('visible',e.id==='page-'+p));$$('.nav-item[data-page]').forEach(e=>e.classList.toggle('active',e.dataset.page===p));const names={home:'Overview',notes:'Notes',browser:'Browser',assistant:'AI assistant',workspace:'Workspace',settings:'Preferences'};$('#pageCrumb').textContent=names[p];send(p==='browser'?'browser-show':'browser-hide');}
setCollapsed(collapsed);$$('[data-page]').forEach(e=>e.addEventListener('click',()=>navigate(e.dataset.page)));$$('[data-go]').forEach(e=>e.addEventListener('click',()=>navigate(e.dataset.go)));$('#sidebarCollapse').addEventListener('click',()=>setCollapsed(!collapsed));$('#hideWindow').addEventListener('click',()=>send('window-hide'));$('#minimizeWindow').addEventListener('click',()=>send('window-minimize'));$('#quickCapture').addEventListener('click',()=>send('capture-screen'));
$('#quickCaptureHome').addEventListener('click',()=>send('capture-screen'));
$('#startCapture').addEventListener('click',()=>send('capture-screen'));
$('#importCapture').addEventListener('click',()=>send('capture-folder'));
$('#openCaptures').addEventListener('click',()=>send('capture-folder'));
function openStudio(){navigate('browser');send('browser-aistudio');}
$('#goStudio').addEventListener('click',openStudio);
$('#openStudio').addEventListener('click',openStudio);

$('#copyPrompt').addEventListener('click',()=>{const v=$('#promptInput').value.trim();if(!v){toast('Write a prompt first');return;}send('copy-text|'+v.replace(/\|/g,' '));toast('Prompt copied to clipboard');});
$$('[data-settings-tab]').forEach(t=>t.addEventListener('click',()=>{$$('.settings-tab').forEach(x=>x.classList.toggle('active',x===t));$$('.settings-panel').forEach(x=>x.classList.toggle('active',x.id==='settings-'+t.dataset.settingsTab));}));
function setSwitch(el,v){el.classList.toggle('on',v);el.setAttribute('aria-checked',String(v));}
$('#compactSwitch').addEventListener('click',()=>{setCollapsed(!collapsed);setSwitch($('#compactSwitch'),collapsed);});$('#topmostSwitch').addEventListener('click',()=>{const v=!$('#topmostSwitch').classList.contains('on');setSwitch($('#topmostSwitch'),v);send('topmost|'+(v?'1':'0'));});$('#passSwitch').addEventListener('click',()=>{const v=!$('#passSwitch').classList.contains('on');setSwitch($('#passSwitch'),v);send('passthrough|'+(v?'1':'0'));});$('#opacityRange').addEventListener('input',e=>{$('#opacityValue').value=e.target.value+'%';send('opacity|'+e.target.value);});
$$('[data-theme]').forEach(b=>b.addEventListener('click',()=>{document.documentElement.dataset.theme=b.dataset.theme;localStorage.setItem('pl-theme',b.dataset.theme);$$('[data-theme]').forEach(x=>x.classList.toggle('selected',x===b));}));const theme=localStorage.getItem('pl-theme')||'midnight';document.documentElement.dataset.theme=theme;$$('[data-theme]').forEach(b=>b.classList.toggle('selected',b.dataset.theme===theme));
function norm(v){return v.trim().toUpperCase().replace(/\s+/g,'').replace(/CONTROL/g,'CTRL').replace(/WINDOWS/g,'WIN');}
function valid(v){const a=norm(v).split('+');if(a.length<2)return false;const mods=a.slice(0,-1),key=a.at(-1),allowed=['CTRL','ALT','SHIFT','WIN'];return mods.every(m=>allowed.includes(m))&&new Set(mods).size===mods.length&&(/^[A-Z0-9]$/.test(key)||/^F([1-9]|1[0-2])$/.test(key)||['SPACE','TAB','ENTER','ESC'].includes(key));}
['toggleHotkey','interactiveHotkey','captureHotkey'].forEach(id=>$('#'+id).addEventListener('keydown',e=>{if(['Backspace','Delete','Tab','Enter'].includes(e.key))return;e.preventDefault();const a=[];if(e.ctrlKey)a.push('CTRL');if(e.altKey)a.push('ALT');if(e.shiftKey)a.push('SHIFT');if(e.metaKey)a.push('WIN');let k=e.key.toUpperCase();if(k===' ')k='SPACE';if(k==='ESCAPE')k='ESC';if(['CONTROL','ALT','SHIFT','META'].includes(k))return;if(k.length===1||/^F([1-9]|1[0-2])$/.test(k)||['SPACE','TAB','ENTER','ESC'].includes(k))a.push(k);$('#'+id).value=a.join('+');}));
$('#saveHotkeys').addEventListener('click',()=>{const a=norm($('#toggleHotkey').value),b=norm($('#interactiveHotkey').value),c=norm($('#captureHotkey').value);if(!valid(a)||!valid(b)||!valid(c)||new Set([a,b,c]).size!==3){$('#shortcutFeedback').textContent='Enter three different valid shortcuts.';return;}$('#shortcutFeedback').textContent='Applying shortcuts…';send('hotkeys|'+a+'|'+b+'|'+c);});
let notes=[],active=null,timer=null;try{notes=JSON.parse(localStorage.getItem('pl-notes')||'[]');if(!Array.isArray(notes))notes=[];}catch(_){notes=[];}
function saveNotes(){localStorage.setItem('pl-notes',JSON.stringify(notes));updateCounts();renderNotes();}
function updateCounts(){const n=String(notes.length).padStart(2,'0');$('#noteCount').textContent=n;$('#listCount').textContent=n;$('#notesHeadingCount').textContent=notes.length;$('#homeNotesCount').textContent=notes.length+' '+(notes.length===1?'note':'notes')+' saved';}
function date(v){return new Date(v||Date.now()).toLocaleDateString(undefined,{month:'short',day:'numeric',year:'numeric'});}
function renderNotes(){const q=$('#noteSearch').value.toLowerCase(),list=$('#notesList');list.innerHTML='';const filtered=notes.filter(n=>(n.title+' '+n.body).toLowerCase().includes(q)).sort((a,b)=>b.updated-a.updated);if(!filtered.length){const e=document.createElement('div');e.className='notes-empty';e.innerHTML=notes.length?'No notes match your search.':'Nothing here yet.<br>Create a note to get started.';list.append(e);return;}filtered.forEach(n=>{const b=document.createElement('button');b.className='note-item'+(n.id===active?' active':'');const title=document.createElement('b'),body=document.createElement('p'),stamp=document.createElement('small');title.textContent=n.title||'Untitled note';body.textContent=(n.body||'No additional text').slice(0,160);stamp.textContent=date(n.updated);b.append(title,body,stamp);b.addEventListener('click',()=>openNote(n.id));list.append(b);});}
function openNote(id){active=id;const n=notes.find(x=>x.id===id);if(!n)return;$('#editorContent').classList.remove('hidden');$('.empty-editor').classList.add('hidden');$('#noteTitle').value=n.title;$('#noteBody').value=n.body;$('#noteDate').textContent='LAST EDITED '+date(n.updated).toUpperCase();$('#saveState').textContent='ALL CHANGES SAVED';wordCount();renderNotes();}
function newNote(){const n={id:crypto.randomUUID?crypto.randomUUID():String(Date.now()),title:'',body:'',updated:Date.now()};notes.unshift(n);saveNotes();openNote(n.id);$('#noteTitle').focus();}$('#heroNewNote').addEventListener('click',()=>{navigate('notes');newNote();});
function saveActive(){if(!active)return;const n=notes.find(x=>x.id===active);if(!n)return;n.title=$('#noteTitle').value;n.body=$('#noteBody').value;n.updated=Date.now();$('#saveState').textContent='SAVING…';clearTimeout(timer);timer=setTimeout(()=>{saveNotes();$('#saveState').textContent='ALL CHANGES SAVED';$('#noteDate').textContent='LAST EDITED '+date(n.updated).toUpperCase();},260);}
function wordCount(){const t=$('#noteBody').value.trim();$('#wordCount').textContent=(t?t.split(/\s+/).length:0)+' WORDS';}
$('#createNote').addEventListener('click',newNote);$('#emptyCreate').addEventListener('click',newNote);$('#noteSearch').addEventListener('input',renderNotes);$('#noteTitle').addEventListener('input',saveActive);$('#noteBody').addEventListener('input',()=>{wordCount();saveActive();});$('#deleteNote').addEventListener('click',()=>{if(!active)return;notes=notes.filter(n=>n.id!==active);active=null;saveNotes();$('#editorContent').classList.add('hidden');$('.empty-editor').classList.remove('hidden');toast('Note deleted');});updateCounts();renderNotes();
const browserTabs=new Map();
let activeBrowserTab=0;
function renderBrowserTabs(){
 const host=$('#browserTabs');if(!host)return;host.innerHTML='';
 browserTabs.forEach((tab,id)=>{
  const item=document.createElement('div');item.className='browser-tab'+(id===activeBrowserTab?' active':'');item.title=tab.title||tab.url||'New tab';
  const pick=document.createElement('button');pick.className='tab-select';pick.type='button';pick.textContent=tab.title||'New tab';pick.addEventListener('click',()=>send('browser-switch|'+id));
  const close=document.createElement('button');close.className='tab-close';close.type='button';close.setAttribute('aria-label','Close tab');close.innerHTML=icons.close?'<svg viewBox="0 0 24 24">'+icons.close+'</svg>':'×';close.addEventListener('click',e=>{e.stopPropagation();send('browser-close|'+id);});
  item.append(pick,close);host.append(item);
 });
 if(browserTabs.size===0){const empty=document.createElement('span');empty.className='tab-empty';empty.textContent='No tabs open';host.append(empty);}
}
function activeBrowser(){return browserTabs.get(activeBrowserTab)||null;}
function newBrowserTab(url='https://www.google.com/'){navigate('browser');send('browser-new-tab|'+url);}
function navigateAddress(){
 const tab=activeBrowser();if(!tab){newBrowserTab($('#browserAddress').value||'https://www.google.com/');return;}
 const url=$('#browserAddress').value.trim();if(!url)return;
 send('browser-url|'+activeBrowserTab+'|'+url.replace(/\|/g,'%7C'));
 $('#browserAddress').blur();
}
$('#addressForm').addEventListener('submit',e=>{e.preventDefault();navigateAddress();});
$('#browserBack').addEventListener('click',()=>send('browser-back|'+activeBrowserTab));
$('#browserForward').addEventListener('click',()=>send('browser-forward|'+activeBrowserTab));
$('#browserReload').addEventListener('click',()=>send('browser-reload|'+activeBrowserTab));
$('#browserHome').addEventListener('click',()=>{const tab=activeBrowser();if(tab)send('browser-url|'+activeBrowserTab+'|https://www.google.com/');});
$('#newBrowserTab').addEventListener('click',()=>newBrowserTab());
$('#addBrowserTab').addEventListener('click',()=>newBrowserTab());
window.chrome?.webview?.addEventListener('message',e=>{
 const message=String(e.data||''),a=message.split('|');
 if(a[0]==='browser-tab-pending'){const id=Number(a[1]);browserTabs.set(id,{id,title:a.slice(2).join('|')||'Loading…',url:''});activeBrowserTab=id;renderBrowserTabs();}
 else if(a[0]==='browser-tab-state'){const id=Number(a[1]);const old=browserTabs.get(id)||{id};old.title=a[2]||'New tab';old.url=a.slice(3).join('|')||'';browserTabs.set(id,old);if(id===activeBrowserTab)$('#browserAddress').value=old.url;renderBrowserTabs();}
 else if(a[0]==='browser-tab-active'){activeBrowserTab=Number(a[1]);const tab=activeBrowser();if(tab)$('#browserAddress').value=tab.url||'';renderBrowserTabs();}
 else if(a[0]==='browser-tab-closed'){browserTabs.delete(Number(a[1]));renderBrowserTabs();}
 else if(a[0]==='browser-tab-error'){const tab=browserTabs.get(Number(a[1]));if(tab)tab.title='Could not open';renderBrowserTabs();toast('Could not create browser tab');}
});
window.chrome?.webview?.addEventListener('message',e=>{const a=String(e.data||'').split('|');switch(a[0]){case'hotkeys-result':if(a[1]==='ok'){$('#shortcutFeedback').textContent='Shortcuts saved and active.';toast('Keyboard shortcuts updated');}else{$('#toggleHotkey').value=a[2]||'CTRL+ALT+SPACE';$('#interactiveHotkey').value=a[3]||'CTRL+ALT+I';$('#captureHotkey').value=a[4]||'CTRL+ALT+P';$('#shortcutFeedback').textContent='A shortcut is in use. Defaults restored.';toast('Shortcut unavailable');}break;case'native-settings':$('#opacityRange').value=a[1]||96;$('#opacityValue').value=(a[1]||96)+'%';setSwitch($('#topmostSwitch'),a[2]==='1');setSwitch($('#passSwitch'),a[3]==='1');$('#toggleHotkey').value=a[4]||'CTRL+ALT+SPACE';$('#interactiveHotkey').value=a[5]||'CTRL+ALT+I';$('#captureHotkey').value=a[6]||'CTRL+ALT+P';break;case'legacy-note':{const legacy=a.slice(1).join('|');if(notes.length===0&&legacy.trim()){notes=[{id:'legacy-'+Date.now(),title:'Imported note',body:legacy,updated:Date.now()}];saveNotes();toast('Your previous note was imported');}break;}case'native-passthrough':setSwitch($('#passSwitch'),a[1]==='1');break;case'capture-saved':$('#lastCapture').classList.remove('hidden');$('#captureName').textContent='Saved '+(a[1]||'capture');toast('Screenshot saved and copied — paste with Ctrl+V');break;case'capture-error':toast(a.slice(1).join('|')||'Could not import image');break;case'capture-started':toast('Capturing the desktop…');break;case'navigate-page':navigate(a[1]||'browser');break;}});
