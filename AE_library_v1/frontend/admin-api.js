/**
 * AE Library — Admin API Client
 * portReady Promise ensures port detected before every fetch call.
 */
let API = 'http://localhost:8080';

const portReady = (async () => {
  for (const port of [8080, 8081, 8082, 8083]) {
    try {
      const res = await fetch(`http://localhost:${port}/settings`,
        { signal: AbortSignal.timeout(800) });
      if (res.ok) { API = `http://localhost:${port}`; return; }
    } catch(e) { /* try next */ }
  }
  console.warn('[API] Backend not reachable on 8080-8083.');
})();

async function apiFetch(path, opts = {}) {
  await portReady;
  const res = await fetch(API + path, {
    headers: { 'Content-Type': 'application/json', ...(opts.headers||{}) },
    ...opts,
  });
  return res.json();
}

const BooksAPI   = { getAll:()=>apiFetch('/books/all'), getAvailable:()=>apiFetch('/books/available'), search:(q)=>apiFetch(`/books/search?q=${encodeURIComponent(q)}`), add:(b)=>apiFetch('/books/add',{method:'POST',body:JSON.stringify(b)}), remove:(id)=>apiFetch(`/books/remove/${id}`,{method:'DELETE'}) };
const MembersAPI = { getAll:()=>apiFetch('/members/all'), add:(b)=>apiFetch('/members/add',{method:'POST',body:JSON.stringify(b)}), remove:(id)=>apiFetch(`/members/remove/${id}`,{method:'DELETE'}) };
const TxnAPI     = { getAll:()=>apiFetch('/transactions'), getActive:()=>apiFetch('/transactions/active'), issue:(b)=>apiFetch('/issue',{method:'POST',body:JSON.stringify(b)}), return_:(b)=>apiFetch('/return',{method:'POST',body:JSON.stringify(b)}) };
const SettingsAPI= { get:()=>apiFetch('/settings'), setFineRate:(r)=>apiFetch('/settings/fine-rate',{method:'POST',body:JSON.stringify({fine_rate_per_day:r})}), setAllowedDays:(d)=>apiFetch('/settings/allowed-days',{method:'POST',body:JSON.stringify({allowed_days:d})}), clearFine:(id)=>apiFetch(`/fine/clear/${id}`,{method:'POST'}) };
const StaffAPI   = { getAll:()=>apiFetch('/staff/all'), add:(b)=>apiFetch('/staff/add',{method:'POST',body:JSON.stringify(b)}), remove:(id)=>apiFetch(`/staff/remove/${id}`,{method:'DELETE'}) };

/* ── Utilities ─────────────────────────────────────────── */
function fmtDate(d) {
  if (!d) return '—';
  try { return new Date(d+'T00:00:00').toLocaleDateString('en-IN',{day:'2-digit',month:'short',year:'numeric'}); }
  catch { return d; }
}
function getInitials(name='') {
  return name.trim().split(' ').slice(0,2).map(w=>w[0]?.toUpperCase()||'').join('');
}
function esc(s) {
  return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}
function toast(msg, type='success') {
  let w = document.getElementById('_toast_w');
  if (!w) {
    w = document.createElement('div');
    w.id = '_toast_w';
    w.style.cssText = 'position:fixed;top:72px;right:20px;z-index:9999;display:flex;flex-direction:column;gap:8px;pointer-events:none';
    document.body.appendChild(w);
  }
  const c={success:'#16a34a',error:'#dc2626',warn:'#d97706',info:'#1e40af'};
  const ic={success:'check_circle',error:'error',warn:'warning',info:'info'};
  const el=document.createElement('div');
  el.style.cssText=`background:${c[type]||c.info};color:#fff;padding:12px 18px;border-radius:12px;font-size:14px;font-weight:600;display:flex;align-items:center;gap:10px;box-shadow:0 8px 24px rgba(0,0,0,.18);pointer-events:auto;font-family:Manrope,sans-serif;max-width:340px`;
  el.innerHTML=`<span class="material-symbols-outlined" style="font-size:18px">${ic[type]||'info'}</span>${msg}`;
  w.appendChild(el);
  setTimeout(()=>{el.style.cssText+=';opacity:0;transition:opacity .4s';setTimeout(()=>el.remove(),400);},3200);
}
function wireNav(active) {
  const map = {
    books:'admin_books.html', members:'admin_members.html',
    transactions:'admin_transactions.html', staff:'admin_staff.html',
    settings:'admin_settings.html', dashboard:'admin_dashboard.html',
  };
  document.querySelectorAll('aside nav a, [data-nav]').forEach(a => {
    const txt = a.textContent.trim().toLowerCase();
    let key = null;
    if (txt.includes('book'))        key='books';
    else if (txt.includes('member')) key='members';
    else if (txt.includes('transaction')) key='transactions';
    else if (txt.includes('staff'))  key='staff';
    else if (txt.includes('setting'))key='settings';
    else if (txt.includes('dashboard'))key='dashboard';
    if (key && map[key]) a.href = map[key];
  });
  // Logout
  document.querySelectorAll('button, a').forEach(el => {
    const t=el.textContent.trim().toLowerCase();
    if (t.includes('sign out')||t.includes('logout')||t.includes('log out')) {
      el.onclick=(e)=>{e.preventDefault();sessionStorage.clear();window.location.href='index.html';};
    }
  });
  // Show admin user name
  const user = sessionStorage.getItem('ae_admin_user')||'Admin';
  document.querySelectorAll('[data-user-name]').forEach(el=>el.textContent=user);
}
function confirm_dlg(title, body) {
  return new Promise(resolve=>{
    let m=document.getElementById('_confirm_m');
    if (!m) {
      m=document.createElement('div');
      m.id='_confirm_m';
      m.style.cssText='display:none;position:fixed;inset:0;background:rgba(0,0,0,.45);z-index:10000;align-items:center;justify-content:center;padding:16px';
      m.innerHTML=`<div style="background:#fff;border-radius:20px;padding:36px;max-width:400px;width:100%;box-shadow:0 24px 60px rgba(0,0,0,.2);font-family:Manrope,sans-serif"><h3 id="_cm_t" style="font-size:18px;font-weight:700;color:#002045;margin:0 0 8px"></h3><p id="_cm_b" style="font-size:14px;color:#505f76;margin:0 0 28px;line-height:1.5"></p><div style="display:flex;gap:12px"><button id="_cm_no" style="flex:1;padding:12px;border:1px solid #c4c6cf;border-radius:12px;background:#fff;font-size:14px;font-weight:600;cursor:pointer">Cancel</button><button id="_cm_ok" style="flex:1;padding:12px;border:none;border-radius:12px;background:#002045;color:#fff;font-size:14px;font-weight:600;cursor:pointer">Confirm</button></div></div>`;
      document.body.appendChild(m);
    }
    document.getElementById('_cm_t').textContent=title;
    document.getElementById('_cm_b').textContent=body;
    m.style.display='flex';
    const close=(v)=>{m.style.display='none';resolve(v);};
    document.getElementById('_cm_no').onclick=()=>close(false);
    document.getElementById('_cm_ok').onclick=()=>close(true);
  });
}
