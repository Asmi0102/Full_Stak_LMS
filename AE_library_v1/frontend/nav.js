/**
 * Shared sidebar navigation helpers.
 * Call initNav('books') on each page to wire up links + active state.
 */
const NAV_PAGES = {
  dashboard:   'staff_dashboard.html',
  books:       'staff_books.html',
  members:     'staff_members.html',
  issue_return:'staff_issue_return.html',
  transactions:'staff_transactions.html',
  settings:    'staff_transactions.html#settings',
};

function initNav(activePage) {
  // Wire all nav links
  document.querySelectorAll('[data-nav]').forEach(el => {
    const key = el.dataset.nav;
    if (NAV_PAGES[key]) el.href = NAV_PAGES[key];
  });
  // Set active state
  document.querySelectorAll('[data-nav]').forEach(el => {
    if (el.dataset.nav === activePage) {
      el.classList.add('bg-white','text-blue-900','border-l-4','border-blue-900','shadow-sm');
      el.classList.remove('text-slate-600','hover:bg-slate-100');
    }
  });
  // Wire "New Transaction" button
  document.querySelectorAll('[data-action="new-transaction"]').forEach(btn => {
    btn.onclick = () => window.location.href = 'staff_issue_return.html';
  });
  // Show logged-in user name
  const user = sessionStorage.getItem('ae_user') || 'Librarian';
  document.querySelectorAll('[data-user-name]').forEach(el => el.textContent = user);
  // Logout
  document.querySelectorAll('[data-action="logout"]').forEach(btn => {
    btn.onclick = () => { sessionStorage.clear(); window.location.href='index.html'; };
  });
}
