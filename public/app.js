let products = [];
let chatHistory = [];
let pendingBase64 = null;

const $ = (s) => document.querySelector(s);
const $$ = (s) => document.querySelectorAll(s);

$$('.tab').forEach(btn => {
  btn.addEventListener('click', () => {
    $$('.tab').forEach(t => t.classList.remove('active'));
    $$('.tab-content').forEach(c => c.classList.remove('active'));
    btn.classList.add('active');
    $(`#${btn.dataset.tab}`).classList.add('active');
    if (btn.dataset.tab === 'reminders') loadReminders();
  });
});

async function loadProducts() {
  try {
    const res = await fetch('/api/products');
    products = await res.json();
    renderProducts();
    loadSummary();
  } catch (e) { console.error(e); }
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

async function loadReminders() {
  try {
    const res = await fetch('/api/reminders');
    const data = await res.json();
    const container = $('#reminderSections');
    container.innerHTML = '';
    const sections = [
      { title: '🔔 Daily reminders', items: data.daily || [] },
      { title: '📅 Every 2 days', items: data.every_2_days || [] },
      { title: '📆 Weekly reminders', items: data.weekly || [] }
    ];
    let total = 0;
    sections.forEach(sec => {
      total += sec.items.length;
      if (!sec.items.length) return;
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
          </div>`;
        list.appendChild(card);
      });
      container.appendChild(list);
    });
    $('#reminderEmpty').classList.toggle('hidden', total > 0);
  } catch (e) { console.error(e); }
}

/** Compress image to max 800px JPEG ~quality 0.7 — avoids "request too large" */
function compressImage(file, maxSide = 800, quality = 0.7) {
  return new Promise((resolve, reject) => {
    const img = new Image();
    const url = URL.createObjectURL(file);
    img.onload = () => {
      URL.revokeObjectURL(url);
      let w = img.width, h = img.height;
      if (w > maxSide || h > maxSide) {
        if (w > h) { h = Math.round(h * maxSide / w); w = maxSide; }
        else { w = Math.round(w * maxSide / h); h = maxSide; }
      }
      const canvas = document.createElement('canvas');
      canvas.width = w;
      canvas.height = h;
      canvas.getContext('2d').drawImage(img, 0, 0, w, h);
      resolve(canvas.toDataURL('image/jpeg', quality));
    };
    img.onerror = reject;
    img.src = url;
  });
}

/** Normalize AI date strings to YYYY-MM-DD for <input type="date"> */
function toISODate(s) {
  if (!s) return '';
  s = String(s).trim();
  if (/^\d{4}-\d{2}-\d{2}$/.test(s)) return s;
  // DD/MM/YYYY or DD-MM-YYYY
  let m = s.match(/^(\d{1,2})[\/\-.](\d{1,2})[\/\-.](\d{4})$/);
  if (m) {
    const d = m[1].padStart(2, '0'), mo = m[2].padStart(2, '0'), y = m[3];
    return `${y}-${mo}-${d}`;
  }
  // MM/DD/YYYY ambiguous — if first > 12 treat as DD/MM
  m = s.match(/^(\d{4})[\/\-.](\d{1,2})[\/\-.](\d{1,2})$/);
  if (m) return `${m[1]}-${m[2].padStart(2,'0')}-${m[3].padStart(2,'0')}`;
  return '';
}

$('#fPhoto').addEventListener('change', async (e) => {
  const file = e.target.files[0];
  pendingBase64 = null;
  $('#btnAnalyze').disabled = !file;
  $('#btnSave').disabled = true;
  $('#photoPreview').classList.add('hidden');
  $('#analyzeStatus').classList.add('hidden');
  $('#aiFields').classList.add('hidden');
  $('#aiDishes').classList.add('hidden');
  if (!file) return;

  try {
    $('#analyzeStatus').textContent = 'Compressing photo...';
    $('#analyzeStatus').classList.remove('hidden');
    pendingBase64 = await compressImage(file);
    $('#photoPreview').src = pendingBase64;
    $('#photoPreview').classList.remove('hidden');
    $('#analyzeStatus').classList.add('hidden');
  } catch (err) {
    $('#analyzeStatus').textContent = 'Could not read image';
    $('#analyzeStatus').classList.remove('hidden');
    $('#btnAnalyze').disabled = true;
  }
});

$('#btnAnalyze').addEventListener('click', async () => {
  if (!pendingBase64) return;
  const status = $('#analyzeStatus');
  const btn = $('#btnAnalyze');
  btn.disabled = true;
  status.textContent = 'Analyzing photo with AI...';
  status.classList.remove('hidden');

  try {
    const res = await fetch('/api/analyze-photo', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ image: pendingBase64 })
    });
    const data = await res.json();
    if (data.error) {
      status.textContent = 'Error: ' + data.error;
      btn.disabled = false;
      return;
    }

    $('#fName').value = data.name || 'Unknown food';
    $('#fCategory').value = data.category || 'Other';
    $('#fIcon').value = data.icon || '🛒';

    let exp = toISODate(data.suggestedExpiry);
    if (!exp) {
      const d = new Date();
      d.setDate(d.getDate() + 7);
      exp = d.toISOString().slice(0, 10);
    }
    $('#fExpiry').value = exp;

    status.textContent = data.notes ? `AI: ${data.notes}` : 'AI done — edit if needed, then Save.';
    $('#aiFields').classList.remove('hidden');
    $('#btnSave').disabled = false;

    if (data.dishes && data.dishes.length) {
      const box = $('#aiDishes');
      box.innerHTML = '<h4>Suggested dishes</h4><ul>' +
        data.dishes.map(d =>
          `<li>${d.icon || '🍽'} <strong>${escapeHtml(d.title || '')}</strong> — ${escapeHtml(d.why || '')}</li>`
        ).join('') + '</ul>';
      box.classList.remove('hidden');
    }
  } catch (e) {
    status.textContent = 'Network error: ' + e.message;
  } finally {
    btn.disabled = false;
  }
});

$('#btnAdd').addEventListener('click', () => {
  pendingBase64 = null;
  $('#fPhoto').value = '';
  $('#fName').value = '';
  $('#fCategory').value = '';
  $('#fQty').value = '1';
  $('#fExpiry').value = '';
  $('#fIcon').value = '🛒';
  $('#photoPreview').classList.add('hidden');
  $('#analyzeStatus').classList.add('hidden');
  $('#aiFields').classList.add('hidden');
  $('#aiDishes').classList.add('hidden');
  $('#btnAnalyze').disabled = true;
  $('#btnSave').disabled = true;
  $('#modal').classList.remove('hidden');
});
$('#btnCancel').addEventListener('click', () => $('#modal').classList.add('hidden'));

$('#btnSave').addEventListener('click', async () => {
  const body = {
    name: $('#fName').value.trim(),
    category: $('#fCategory').value.trim() || 'Other',
    quantity: $('#fQty').value.trim() || '1',
    expiryDate: toISODate($('#fExpiry').value) || $('#fExpiry').value,
    icon: $('#fIcon').value.trim() || '🛒'
  };
  if (!body.name || !body.expiryDate) {
    alert('Please analyze a photo first so AI can fill name and expiry.');
    return;
  }
  if (!/^\d{4}-\d{2}-\d{2}$/.test(body.expiryDate)) {
    alert('Invalid expiry date. Please pick a date in the date field.');
    return;
  }
  const res = await fetch('/api/products', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  });
  if (res.ok) {
    $('#modal').classList.add('hidden');
    loadProducts();
  } else {
    const err = await res.json().catch(() => ({}));
    alert(err.error || 'Save failed');
  }
});

function renderRecipeCards(data, list) {
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
}

$('#btnRecipes').addEventListener('click', async () => {
  const btn = $('#btnRecipes');
  const loading = $('#recipeLoading');
  const list = $('#recipeList');
  btn.disabled = true;
  loading.classList.remove('hidden');
  list.innerHTML = '';
  try {
    const res = await fetch('/api/recipes', { method: 'POST' });
    let data = await res.json();
    if (data && data.raw && typeof data.raw === 'string') {
      try {
        const start = data.raw.indexOf('[');
        const end = data.raw.lastIndexOf(']');
        if (start >= 0 && end > start) data = JSON.parse(data.raw.slice(start, end + 1));
      } catch (_) {}
    }
    if (data.error) list.innerHTML = `<p class="empty">Error: ${escapeHtml(data.error)}</p>`;
    else if (Array.isArray(data)) renderRecipeCards(data, list);
    else list.innerHTML = `<p class="empty">Could not parse recipes. Try again.</p>`;
  } catch (e) {
    list.innerHTML = `<p class="empty">Network error: ${e.message}</p>`;
  } finally {
    btn.disabled = false;
    loading.classList.add('hidden');
  }
});

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
  if (data.error) appendBubble('assistant', '❌ ' + data.error);
  else {
    appendBubble('assistant', data.reply);
    chatHistory.push({ role: 'assistant', content: data.reply });
  }
}
$('#btnSend').addEventListener('click', sendChat);
$('#chatInput').addEventListener('keydown', e => { if (e.key === 'Enter') sendChat(); });

function escapeHtml(s) {
  return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

loadProducts();
appendBubble('assistant', 'Hi! I am FreshMate AI. Ask me anything about your pantry 🥗');
