// ── State ──
let products = [];
let chatHistory = [];

// ── DOM ──
const $ = (s) => document.querySelector(s);
const $$ = (s) => document.querySelectorAll(s);

// ── Tabs ──
$$('.tab').forEach(btn => {
  btn.addEventListener('click', () => {
    $$('.tab').forEach(t => t.classList.remove('active'));
    $$('.tab-content').forEach(c => c.classList.remove('active'));
    btn.classList.add('active');
    $(`#${btn.dataset.tab}`).classList.add('active');
    if (btn.dataset.tab === 'reminders') loadReminders();
  });
});

// ── Load products ──
async function loadProducts() {
  try {
    const res = await fetch('/api/products');
    products = await res.json();
    renderProducts();
    loadSummary();
  } catch (e) {
    console.error(e);
  }
}

async function loadSummary() {
  try {
    const res = await fetch('/api/summary');
    const s = await res.json();
    $('#freshCount').textContent = `${s.fresh} fresh`;
    $('#warnCount').textContent = `${s.warning} expiring soon`;
    $('#dangerCount').textContent = `${s.danger} expired`;
  } catch {}
}

function renderProducts() {
  const q = ($('#search').value || '').toLowerCase();
  const list = products.filter(p =>
    p.name.toLowerCase().includes(q) ||
    (p.category || '').toLowerCase().includes(q)
  );

  const container = $('#productList');
  container.innerHTML = '';
  $('#emptyMsg').classList.toggle('hidden', list.length > 0);

  list.forEach(p => {
    const card = document.createElement('div');
    card.className = `card status-${p.expiryStatus}`;
    card.innerHTML = `
      <div class="card-icon">${p.icon || '🛒'}</div>
      <div class="card-body">
        <div class="card-name">${escapeHtml(p.name)}</div>
        <div class="card-meta">${escapeHtml(p.quantity)} · ${escapeHtml(p.category || '')}</div>
        <div class="card-expiry ${p.expiryStatus}">${escapeHtml(p.expiryLabel)}</div>
        <div class="card-reminder">${escapeHtml(p.reminderLabel || '')}</div>
      </div>
      <button class="btn-delete" data-id="${p.id}" title="Delete">🗑</button>
    `;
    container.appendChild(card);
  });

  $$('.btn-delete').forEach(btn => {
    btn.addEventListener('click', async () => {
      if (!confirm('Delete this product?')) return;
      await fetch(`/api/products/${btn.dataset.id}`, { method: 'DELETE' });
      loadProducts();
    });
  });
}

$('#search').addEventListener('input', renderProducts);

// ── Reminders ──
async function loadReminders() {
  try {
    const res = await fetch('/api/reminders');
    const data = await res.json();
    const container = $('#reminderSections');
    container.innerHTML = '';

    const sections = [
      { key: 'daily', title: '🔔 Daily reminders', items: data.daily || [] },
      { key: 'every_2_days', title: '📅 Every 2 days', items: data.every_2_days || [] },
      { key: 'weekly', title: '📆 Weekly reminders', items: data.weekly || [] }
    ];

    let total = 0;
    sections.forEach(sec => {
      total += sec.items.length;
      if (sec.items.length === 0) return;

      const title = document.createElement('div');
      title.className = 'section-title';
      title.innerHTML = `${sec.title} <span class="count">${sec.items.length}</span>`;
      container.appendChild(title);

      const list = document.createElement('div');
      list.className = 'reminder-list';
      sec.items.forEach(p => {
        const card = document.createElement('div');
        card.className = `card status-${p.expiryStatus}`;
        card.innerHTML = `
          <div class="card-icon">${p.icon || '🛒'}</div>
          <div class="card-body">
            <div class="card-name">${escapeHtml(p.name)}</div>
            <div class="card-meta">${escapeHtml(p.quantity)} · ${escapeHtml(p.category || '')}</div>
            <div class="card-expiry ${p.expiryStatus}">${escapeHtml(p.expiryLabel)}</div>
          </div>
        `;
        list.appendChild(card);
      });
      container.appendChild(list);
    });

    $('#reminderEmpty').classList.toggle('hidden', total > 0);
  } catch (e) {
    console.error(e);
  }
}

// ── Modal add ──
$('#btnAdd').addEventListener('click', () => {
  const d = new Date();
  d.setDate(d.getDate() + 7);
  $('#fExpiry').value = d.toISOString().slice(0, 10);
  $('#modal').classList.remove('hidden');
});
$('#btnCancel').addEventListener('click', () => $('#modal').classList.add('hidden'));

$('#btnSave').addEventListener('click', async () => {
  const body = {
    name: $('#fName').value.trim(),
    category: $('#fCategory').value,
    quantity: $('#fQty').value.trim() || '1',
    expiryDate: $('#fExpiry').value,
    icon: $('#fIcon').value.trim() || '🛒'
  };
  if (!body.name || !body.expiryDate) {
    alert('Please enter name and expiry date');
    return;
  }
  const res = await fetch('/api/products', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  });
  if (res.ok) {
    $('#modal').classList.add('hidden');
    $('#fName').value = '';
    loadProducts();
  } else {
    const err = await res.json();
    alert(err.error || 'Error');
  }
});

// ── Recipes ──
$('#btnRecipes').addEventListener('click', async () => {
  const btn = $('#btnRecipes');
  const loading = $('#recipeLoading');
  const list = $('#recipeList');
  btn.disabled = true;
  loading.classList.remove('hidden');
  list.innerHTML = '';

  try {
    const res = await fetch('/api/recipes', { method: 'POST' });
    const data = await res.json();
    if (data.error) {
      list.innerHTML = `<p class="empty">Error: ${escapeHtml(data.error)}</p>`;
    } else if (Array.isArray(data)) {
      data.forEach(r => {
        const card = document.createElement('div');
        card.className = 'recipe-card';
        card.innerHTML = `
          <div class="recipe-title">${r.icon || '🍽'} ${escapeHtml(r.title || '')}</div>
          <div class="recipe-meta">
            <span>⏱ ${escapeHtml(r.time || '')}</span>
            <span>${escapeHtml(r.difficulty || '')}</span>
            ${(r.tags || []).map(t => `<span>${escapeHtml(t)}</span>`).join('')}
          </div>
          <div class="recipe-section">
            <h4>Ingredients</h4>
            <ul>${(r.ingredients || []).map(i => `<li>${escapeHtml(i)}</li>`).join('')}</ul>
          </div>
          <div class="recipe-section">
            <h4>Steps</h4>
            <ol>${(r.steps || []).map(s => `<li>${escapeHtml(s)}</li>`).join('')}</ol>
          </div>
          ${r.tip ? `<div class="recipe-section"><h4>Tip</h4><p>${escapeHtml(r.tip)}</p></div>` : ''}
        `;
        list.appendChild(card);
      });
    } else {
      list.innerHTML = `<pre>${escapeHtml(JSON.stringify(data, null, 2))}</pre>`;
    }
  } catch (e) {
    list.innerHTML = `<p class="empty">Network error: ${e.message}</p>`;
  } finally {
    btn.disabled = false;
    loading.classList.add('hidden');
  }
});

// ── Chat ──
function appendBubble(role, text) {
  const div = document.createElement('div');
  div.className = `bubble ${role}`;
  div.textContent = text;
  $('#chatMessages').appendChild(div);
  $('#chatMessages').scrollTop = $('#chatMessages').scrollHeight;
}

async function sendChat() {
  const input = $('#chatInput');
  const msg = input.value.trim();
  if (!msg) return;
  input.value = '';
  appendBubble('user', msg);
  chatHistory.push({ role: 'user', content: msg });

  const res = await fetch('/api/chat', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ message: msg, history: chatHistory.slice(0, -1) })
  });
  const data = await res.json();
  if (data.error) {
    appendBubble('assistant', '❌ ' + data.error);
  } else {
    appendBubble('assistant', data.reply);
    chatHistory.push({ role: 'assistant', content: data.reply });
  }
}

$('#btnSend').addEventListener('click', sendChat);
$('#chatInput').addEventListener('keydown', e => {
  if (e.key === 'Enter') sendChat();
});

// ── Utils ──
function escapeHtml(s) {
  return String(s)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

// ── Init ──
loadProducts();
appendBubble('assistant', 'Hi! I am FreshMate AI. Ask me anything about your pantry 🥗');
