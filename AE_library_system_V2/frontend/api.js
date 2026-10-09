/**
 * AE Library — Staff API Client
 * portReady Promise ensures port detected before every fetch call.
 */
let API_BASE = 'http://localhost:8080';

const portReady = (async () => {
  for (const port of [8080, 8081, 8082, 8083]) {
    try {
      const res = await fetch(`http://localhost:${port}/settings`,
        { signal: AbortSignal.timeout(800) });
      if (res.ok) { API_BASE = `http://localhost:${port}`; return; }
    } catch(e) { /* try next */ }
  }
  console.warn('[API] Backend not reachable on 8080-8083.');
})();

async function apiFetch(path, options = {}) {
  await portReady;
  const res = await fetch(API_BASE + path, {
    headers: { 'Content-Type': 'application/json', ...(options.headers || {}) },
    ...options,
  });
  return res.json();
}

const BooksAPI = {
  getAll:       ()     => apiFetch('/books/all'),
  getAvailable: ()     => apiFetch('/books/available'),
  search:       (q)    => apiFetch(`/books/search?q=${encodeURIComponent(q)}`),
  add:          (body) => apiFetch('/books/add',          { method:'POST', body:JSON.stringify(body) }),
  remove:       (id)   => apiFetch(`/books/remove/${id}`, { method:'DELETE' }),
};

const MembersAPI = {
  getAll:  ()     => apiFetch('/members/all'),
  add:     (body) => apiFetch('/members/add',             { method:'POST', body:JSON.stringify(body) }),
  remove:  (id)   => apiFetch(`/members/remove/${id}`,    { method:'DELETE' }),
};

const TransactionsAPI = {
  getAll:    ()     => apiFetch('/transactions'),
  getActive: ()     => apiFetch('/transactions/active'),
  // staff_id is injected automatically from sessionStorage if not provided
  issue:     (body) => apiFetch('/issue',  { method:'POST', body:JSON.stringify({...body, staff_id: body.staff_id || parseInt(sessionStorage.getItem('ae_staff_id')||'0')}) }),
  return_:   (body) => apiFetch('/return', { method:'POST', body:JSON.stringify({...body, staff_id: body.staff_id || parseInt(sessionStorage.getItem('ae_staff_id')||'0')}) }),
};

const FinesAPI = {
  clear:          (id)   => apiFetch(`/fine/clear/${id}`,       { method:'POST' }),
  getSettings:    ()     => apiFetch('/settings'),
  setFineRate:    (rate) => apiFetch('/settings/fine-rate',      { method:'POST', body:JSON.stringify({fine_rate_per_day:rate}) }),
  setAllowedDays: (days) => apiFetch('/settings/allowed-days',   { method:'POST', body:JSON.stringify({allowed_days:days}) }),
};

const StaffProfileAPI = {
  update:         (body) => apiFetch('/staff/update-profile',      { method:'POST', body:JSON.stringify(body) }),
  clearFirstLogin:(id)   => apiFetch(`/staff/clear-first-login/${id}`, { method:'POST' }),
};

const StaffAPI = {
  getAll:  ()     => apiFetch('/staff/all'),
  add:     (body) => apiFetch('/staff/add',           { method:'POST', body:JSON.stringify(body) }),
  remove:  (id)   => apiFetch(`/staff/remove/${id}`,  { method:'DELETE' }),
  login:   (body) => apiFetch('/staff/login',         { method:'POST', body:JSON.stringify(body) }),
};

/* ── Utilities ─────────────────────────────────────────── */
function showToast(message, type = 'success') {
  let c = document.getElementById('_toast_wrap');
  if (!c) {
    c = document.createElement('div');
    c.id = '_toast_wrap';
    c.style.cssText = 'position:fixed;top:72px;right:20px;z-index:9999;display:flex;flex-direction:column;gap:8px;pointer-events:none';
    document.body.appendChild(c);
  }
  const col = {success:'#16a34a',error:'#dc2626',warn:'#d97706',info:'#1e40af'};
  const ico = {success:'check_circle',error:'error',warn:'warning',info:'info'};
  const el = document.createElement('div');
  el.style.cssText=`background:${col[type]||col.info};color:#fff;padding:12px 18px;border-radius:12px;font-size:14px;font-weight:600;display:flex;align-items:center;gap:10px;box-shadow:0 8px 24px rgba(0,0,0,.18);pointer-events:auto;font-family:Inter,sans-serif;max-width:340px`;
  el.innerHTML=`<span class="material-symbols-outlined" style="font-size:18px">${ico[type]||'info'}</span>${message}`;
  c.appendChild(el);
  setTimeout(()=>{el.style.cssText+=';opacity:0;transition:opacity .4s';setTimeout(()=>el.remove(),400);},3200);
}

function fmtDate(d) {
  if (!d) return '—';
  try { return new Date(d+'T00:00:00').toLocaleDateString('en-IN',{day:'2-digit',month:'short',year:'numeric'}); }
  catch { return d; }
}

function getInitials(name='') {
  return name.trim().split(' ').slice(0,2).map(w=>w[0]?.toUpperCase()||'').join('');
}

function escHtml(s) {
  return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}
