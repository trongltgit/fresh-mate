# FreshMate Web (C++)

Web-based version of FreshMate written in **C++17** (cpp-httplib + SQLite + libcurl).

Uses **Groq API** for AI features. Easy to deploy on **Render** (Docker).

---

## Features

| Feature | Description |
|---------|-------------|
| Pantry management | Add / delete / view products with expiry dates |
| Expiry status | Fresh / Expiring soon / Expired (color-coded) |
| **Smart reminders** | Automatic frequency based on days left |
| AI Recipes | Groq suggests 3 dishes prioritising items that expire soon |
| AI Chat | Chatbot that knows your current pantry |
| Storage | SQLite |

### Smart Reminder Rules

| Days until expiry | Reminder frequency |
|-------------------|--------------------|
| ≤ 2 days (or already expired) | **Daily** |
| 3 – 7 days | **Every 2 days** |
| 8 – 30 days | **Weekly** |
| > 30 days | No active reminder |

---

## Project structure

```
freshmate-web/
├── CMakeLists.txt
├── Dockerfile              ← for Render
├── Makefile
├── README.md
├── src/
│   ├── main.cpp            ← HTTP server + API routes
│   ├── Database.hpp/.cpp   ← SQLite CRUD
│   ├── ApiClient.hpp/.cpp  ← Groq API (chat + recipes + vision-ready)
│   └── ProductItem.hpp     ← includes reminderFrequency()
├── public/
│   ├── index.html
│   ├── style.css
│   └── app.js
└── third_party/
    └── httplib.h
```

---

## 1. Run locally

### Requirements

```bash
sudo apt update
sudo apt install -y g++ cmake libsqlite3-dev nlohmann-json3-dev libcurl4-openssl-dev
```

### Build & run

```bash
cd freshmate-web
make run
```

Open: **http://localhost:8080**

### Enable AI (required for Chat & Recipes)

```bash
export GROQ_API_KEY=gsk_xxxxxxxxxxxxxxxx
make run
```

Get a free key at: https://console.groq.com

---

## 2. Deploy to Render

1. Push the code to GitHub.

2. On [dashboard.render.com](https://dashboard.render.com) → **New +** → **Web Service**

3. Settings:

| Field | Value |
|-------|-------|
| Runtime | **Docker** |
| Branch | `main` |
| Instance | Free |

4. **Environment Variables**:

| Key | Value |
|-----|-------|
| `GROQ_API_KEY` | your Groq key (`gsk_...`) |
| `DB_PATH` | `/data/freshmate.db` |

5. (Recommended) Add a **Persistent Disk**:
   - Mount Path: `/data`
   - Size: 1 GB

6. Click **Create Web Service**. After build finishes you get a URL like:
   `https://freshmate-xxxx.onrender.com`

---

## 3. API endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/products` | List all products |
| POST | `/api/products` | Add product (JSON) |
| DELETE | `/api/products/:id` | Delete product |
| GET | `/api/summary` | Counts (fresh / warning / danger) |
| GET | `/api/reminders` | Items grouped by reminder frequency |
| POST | `/api/chat` | AI chat |
| POST | `/api/recipes` | Generate 3 recipes |
| GET | `/health` | Health check |

Example – add a product:

```bash
curl -X POST https://your-app.onrender.com/api/products \
  -H "Content-Type: application/json" \
  -d '{"name":"Milk","category":"Dairy & Eggs","quantity":"1 carton","expiryDate":"2026-09-25","icon":"🥛"}'
```

Example – get smart reminders:

```bash
curl https://your-app.onrender.com/api/reminders
```

---

## 4. Notes

- Free Render tier sleeps after ~15 min of inactivity (cold start on next request).
- Without a Persistent Disk the SQLite file is lost on restart.
- Never commit your `GROQ_API_KEY`. Use environment variables only.
- Model used: `llama-3.3-70b-versatile` (chat/recipes). Vision model is ready in code for future photo analysis.

---

## Next step: Android APK

After you test this web version, tell me and I will prepare the **C++ Qt/QML → Android APK** version with the same smart reminder logic.
