// ============================================================
//  MATRIX PROJECT — Web Interface (MFMCF Edition / P10)
//  Purple theme | Logo | Set Time | Named Countdown |
//  Configurable TIME UP | Scroll Speed | Font Selection |
//  Clock Style + 12/24hr | Heading Interval
//  Scroll direction: Right to Left
//  Live mirror driven entirely by ESP32 broadcasts
// ============================================================

const char INDEX_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>MFMCF Display</title>
<style>
  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

  :root {
    --bg:         #0a0008;
    --surface:    #110014;
    --border:     #2a0035;
    --accent:     #cc44ff;
    --accent-dim: #aa22dd;
    --warn:       #ff4444;
    --text:       #e8d0f0;
    --text-dim:   #7a5a88;
    --font-mono:  'Courier New', Courier, monospace;
    --font-ui:    system-ui, -apple-system, sans-serif;
  }

  body {
    background: var(--bg);
    color: var(--text);
    font-family: var(--font-ui);
    min-height: 100vh;
    padding: 0 0 60px 0;
  }

  header {
    background: var(--surface);
    border-bottom: 1px solid var(--border);
    padding: 14px 20px;
    position: sticky;
    top: 0;
    z-index: 10;
    display: flex;
    align-items: center;
    gap: 14px;
  }

  .header-text h1 {
    font-family: var(--font-mono);
    font-size: 0.85rem;
    letter-spacing: 0.18em;
    color: var(--accent);
    text-transform: uppercase;
  }

  .header-text p {
    font-size: 0.68rem;
    color: var(--text-dim);
    margin-top: 2px;
  }

  #status-bar {
    background: #0a000d;
    border-bottom: 1px solid var(--border);
    padding: 9px 20px;
    display: flex;
    align-items: center;
    gap: 10px;
  }

  #status-dot {
    width: 8px; height: 8px;
    border-radius: 50%;
    background: var(--text-dim);
    flex-shrink: 0;
    transition: background 0.3s;
  }
  #status-dot.connected { background: var(--accent); box-shadow: 0 0 6px var(--accent); }
  #status-dot.error     { background: var(--warn); }

  #status-text {
    color: var(--text-dim);
    font-family: var(--font-mono);
    font-size: 0.72rem;
  }

  #display-mirror {
    margin: 18px 16px 0;
    background: #0a000d;
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 14px;
  }

  #display-mirror .label {
    font-size: 0.62rem;
    letter-spacing: 0.15em;
    color: var(--text-dim);
    text-transform: uppercase;
    margin-bottom: 10px;
  }

  .lcd-screen {
    background: #0a0010;
    border: 1px solid #3a0050;
    border-radius: 6px;
    padding: 12px 16px;
    font-family: var(--font-mono);
    font-size: 1rem;
    line-height: 1.9;
    color: var(--accent);
    letter-spacing: 0.08em;
    min-height: 64px;
    box-shadow: inset 0 0 12px rgba(204,68,255,0.08);
    overflow: hidden;
    white-space: nowrap;
  }

  .lcd-row { overflow: hidden; }
  .lcd-row.big { font-size: 1.6rem; line-height: 2.2; }

  #mode-badge {
    display: inline-block;
    font-family: var(--font-mono);
    font-size: 0.62rem;
    letter-spacing: 0.15em;
    padding: 3px 10px;
    border-radius: 20px;
    margin-top: 10px;
    background: #1a001f;
    color: var(--text-dim);
    border: 1px solid var(--border);
    text-transform: uppercase;
  }
  #mode-badge.active { background: #1f0030; color: var(--accent); border-color: #6600aa; }
  #mode-badge.warn   { background: #1a0000; color: var(--warn);   border-color: #440000; }

  .section {
    margin: 16px 16px 0;
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 8px;
    overflow: hidden;
  }

  .section-header {
    padding: 12px 16px 10px;
    border-bottom: 1px solid var(--border);
    display: flex;
    align-items: baseline;
    gap: 10px;
    background: #0f0018;
  }

  .section-header h2 {
    font-family: var(--font-mono);
    font-size: 0.75rem;
    letter-spacing: 0.15em;
    color: var(--accent);
    text-transform: uppercase;
  }

  .section-header span { font-size: 0.68rem; color: var(--text-dim); }

  .section-body { padding: 16px; }

  label {
    display: block;
    font-size: 0.68rem;
    color: var(--text-dim);
    letter-spacing: 0.08em;
    margin-bottom: 5px;
    text-transform: uppercase;
  }

  input[type="text"],
  input[type="number"],
  input[type="time"],
  input[type="date"],
  input[type="range"],
  textarea,
  select {
    width: 100%;
    background: #0a000d;
    border: 1px solid var(--border);
    border-radius: 5px;
    color: var(--text);
    font-family: var(--font-mono);
    font-size: 0.95rem;
    padding: 10px 13px;
    outline: none;
    transition: border-color 0.2s;
    -webkit-appearance: none;
  }

  input:focus, textarea:focus, select:focus { border-color: var(--accent); }

  input#countdown-input {
    letter-spacing: 0.2em;
    font-size: 1.35rem;
    text-align: center;
  }

  input[type="range"] {
    padding: 6px 0;
    cursor: pointer;
    accent-color: var(--accent);
  }

  textarea { resize: vertical; min-height: 75px; line-height: 1.5; }

  select option { background: #110014; }

  .two-col { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }

  .field + .field { margin-top: 13px; }

  .hint { font-size: 0.65rem; color: var(--text-dim); margin-top: 4px; }

  .style-choice-row {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px;
  }

  .style-choice {
    background: #0a000d;
    border: 1px solid var(--border);
    border-radius: 6px;
    padding: 12px;
    text-align: center;
    cursor: pointer;
    font-family: var(--font-mono);
    font-size: 0.72rem;
    letter-spacing: 0.08em;
    color: var(--text-dim);
    text-transform: uppercase;
    transition: border-color 0.15s, color 0.15s, background 0.15s;
    user-select: none;
  }
  .style-choice:active { transform: scale(0.98); }
  .style-choice.selected {
    border-color: var(--accent);
    color: var(--accent);
    background: #1f0030;
  }
  .style-choice .sub {
    display: block;
    font-size: 0.6rem;
    color: var(--text-dim);
    margin-top: 4px;
    letter-spacing: 0.05em;
    text-transform: none;
  }
  .style-choice.selected .sub { color: var(--accent-dim); }

  .speed-row {
    display: flex;
    align-items: center;
    gap: 10px;
  }
  .speed-row input[type="range"] { flex: 1; }
  .speed-label {
    font-family: var(--font-mono);
    font-size: 0.8rem;
    color: var(--accent);
    min-width: 55px;
    text-align: right;
  }

  .btn-row {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px;
    margin-top: 14px;
  }

  button {
    padding: 11px;
    border-radius: 5px;
    border: none;
    font-family: var(--font-mono);
    font-size: 0.75rem;
    letter-spacing: 0.1em;
    text-transform: uppercase;
    cursor: pointer;
    transition: opacity 0.15s, transform 0.1s;
  }

  button:active   { transform: scale(0.97); }
  button:disabled { opacity: 0.3; cursor: not-allowed; }

  .btn-start { background: var(--accent); color: #000; font-weight: 700; }
  .btn-start:hover:not(:disabled) { background: var(--accent-dim); }

  .btn-cancel { background: transparent; color: var(--warn); border: 1px solid var(--warn); }
  .btn-cancel:hover:not(:disabled) { background: #1a0000; }

  .btn-full {
    width: 100%;
    margin-top: 14px;
    background: var(--accent);
    color: #000;
    font-weight: 700;
    padding: 11px;
    border-radius: 5px;
    border: none;
    font-family: var(--font-mono);
    font-size: 0.75rem;
    letter-spacing: 0.1em;
    text-transform: uppercase;
    cursor: pointer;
  }
  .btn-full:hover { background: var(--accent-dim); }
</style>
</head>
<body>

<!-- ── Header ── -->
<header>
  <div class="header-text">
    <h1>MFMCF Display</h1>
    <p>Mountain of Fire and Miracles Campus Fellowship</p>
  </div>
</header>

<!-- ── Status ── -->
<div id="status-bar">
  <div id="status-dot"></div>
  <span id="status-text">Connecting...</span>
</div>

<!-- ── Live Display Mirror — driven entirely by ESP32 broadcasts ── -->
<div id="display-mirror">
  <div class="label">Live Display</div>
  <div class="lcd-screen">
    <div class="lcd-row" id="lcd-row0">&nbsp;</div>
    <div class="lcd-row" id="lcd-row1">&nbsp;</div>
  </div>
  <div id="mode-badge">CLOCK</div>
</div>

<!-- ── DEFAULT CLOCK DISPLAY STYLE ── -->
<div class="section">
  <div class="section-header">
    <h2>Default Display Style</h2>
    <span>Used in clock mode</span>
  </div>
  <div class="section-body">
    <div class="style-choice-row">
      <div class="style-choice" id="style-choice-0" onclick="setClockStyle(0)">
        Date + Time
        <span class="sub">Small font, both rows</span>
      </div>
      <div class="style-choice" id="style-choice-1" onclick="setClockStyle(1)">
        Time Only
        <span class="sub">Bold HH:MM:SS</span>
      </div>
    </div>
    <div class="style-choice-row" style="margin-top:10px;">
      <div class="style-choice" id="format-choice-24" onclick="setTimeFormat(false)">
        24-Hour
        <span class="sub">e.g. 14:30:00</span>
      </div>
      <div class="style-choice" id="format-choice-12" onclick="setTimeFormat(true)">
        12-Hour
        <span class="sub">e.g. 02:30:00</span>
      </div>
    </div>
  </div>
</div>

<!-- ── SET TIME ── -->
<div class="section">
  <div class="section-header">
    <h2>Set Time &amp; Date</h2>
    <span>Writes to RTC</span>
  </div>
  <div class="section-body">
    <div class="two-col">
      <div class="field">
        <label for="set-date">Date</label>
        <input type="date" id="set-date">
      </div>
      <div class="field">
        <label for="set-time">Time</label>
        <input type="time" id="set-time" step="1">
      </div>
    </div>
    <button class="btn-full" onclick="setDateTime()">Sync to Display</button>
  </div>
</div>

<!-- ── COUNTDOWN ── -->
<div class="section">
  <div class="section-header">
    <h2>Countdown Timer</h2>
    <span>HH:MM:SS</span>
  </div>
  <div class="section-body">
    <div class="field">
      <label for="countdown-label">Label</label>
      <input type="text" id="countdown-label" placeholder="e.g. PRAYER TIME">
      <p class="hint">Scrolls automatically if too long for the display</p>
    </div>
    <div class="field">
      <label for="countdown-input">Set Time</label>
      <input type="text" id="countdown-input" placeholder="00:30:00"
             maxlength="8" inputmode="numeric">
      <p class="hint">Hours : Minutes : Seconds</p>
    </div>
    <div class="two-col">
      <div class="field">
        <label for="heading-interval">Show Heading Every</label>
        <input type="number" id="heading-interval" placeholder="0" min="0" max="3600" inputmode="numeric">
        <p class="hint">Seconds. 0 = always show</p>
      </div>
      <div class="field">
        <label for="heading-show">Show Heading For</label>
        <input type="number" id="heading-show" placeholder="3" min="1" max="60" inputmode="numeric">
        <p class="hint">Seconds, each time</p>
      </div>
    </div>
    <p class="hint">While the heading is hidden, the timer switches to a large bold display for long-distance visibility.</p>
    <div class="field">
      <label for="timeup-duration">TIME UP Blink Duration</label>
      <select id="timeup-duration">
        <option value="5">5 seconds</option>
        <option value="10" selected>10 seconds</option>
        <option value="15">15 seconds</option>
        <option value="20">20 seconds</option>
        <option value="30">30 seconds</option>
        <option value="60">1 minute</option>
      </select>
    </div>
    <div class="btn-row">
      <button class="btn-start"  id="btn-cd-start"  onclick="startCountdown()">Start</button>
      <button class="btn-cancel" id="btn-cd-cancel" onclick="cancelCountdown()" disabled>Cancel</button>
    </div>
  </div>
</div>

<!-- ── SCROLLING TEXT ── -->
<div class="section">
  <div class="section-header">
    <h2>Scrolling Text</h2>
    <span>Right to left</span>
  </div>
  <div class="section-body">
    <div class="field">
      <label for="scroll-text">Message</label>
      <textarea id="scroll-text" placeholder="Type your message here..."></textarea>
    </div>
    <div class="field">
      <label for="scroll-dur">Display Duration (seconds)</label>
      <input type="number" id="scroll-dur" placeholder="30"
             min="5" max="3600" inputmode="numeric">
      <p class="hint">Minimum 5 seconds — loops until time expires</p>
    </div>
    <div class="field">
      <label>Scroll Speed</label>
      <div class="speed-row">
        <span style="font-size:0.7rem;color:var(--text-dim)">Slow</span>
        <input type="range" id="scroll-speed" min="20" max="120"
               value="40" step="10" oninput="updateSpeedLabel(this.value)">
        <span style="font-size:0.7rem;color:var(--text-dim)">Fast</span>
        <span class="speed-label" id="speed-label">Normal</span>
      </div>
    </div>
    <div class="field">
      <label for="scroll-font">Font</label>
      <select id="scroll-font">
        <option value="0">Small — fits more text</option>
        <option value="1">Large Bold — visible from distance</option>
      </select>
    </div>
    <div class="btn-row">
      <button class="btn-start"  id="btn-sc-start"  onclick="startScroll()">Start</button>
      <button class="btn-cancel" id="btn-sc-cancel" onclick="cancelScroll()" disabled>Cancel</button>
    </div>
  </div>
</div>

<script>
// ── WebSocket ──────────────────────────────────────────────────
let ws, reconnectTimer;
let wsConnecting = false;

function connect() {
  if (wsConnecting) return;
  wsConnecting = true;
  ws = new WebSocket('ws://' + location.host + '/ws');
  ws.onopen = () => {
    wsConnecting = false;
    clearTimeout(reconnectTimer);
    setDot('connected');
    setStatusText('Connected');
  };
  ws.onclose = () => {
    wsConnecting = false;
    setDot('');
    setStatusText('Disconnected — retrying...');
    clearTimeout(reconnectTimer);
    reconnectTimer = setTimeout(connect, 1500);
  };
  ws.onerror = () => {
    wsConnecting = false;
    setDot('error');
    setStatusText('Connection error');
  };
  ws.onmessage = (e) => { try { handleStatus(JSON.parse(e.data)); } catch(err) {} };
}

// Reconnect-safe command sender.
function sendCmd(obj) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify(obj));
    return;
  }
  clearTimeout(reconnectTimer);
  connect();
  setTimeout(() => {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(obj));
    }
  }, 400);
}

// ── Status Handler — the mirror renders exactly what the ESP32
//    reports (line0/line1), it never guesses on its own anymore. ──
function handleStatus(data) {
  const badge  = document.getElementById('mode-badge');
  const btnCDS = document.getElementById('btn-cd-start');
  const btnCDC = document.getElementById('btn-cd-cancel');
  const btnSCS = document.getElementById('btn-sc-start');
  const btnSCC = document.getElementById('btn-sc-cancel');

  btnCDS.disabled = false; btnCDC.disabled = true;
  btnSCS.disabled = false; btnSCC.disabled = true;
  badge.className = '';

  if (typeof data.clockStyle !== 'undefined') updateStyleChoiceUI(data.clockStyle);
  if (typeof data.use12h !== 'undefined')      updateFormatChoiceUI(data.use12h);

  const row0 = document.getElementById('lcd-row0');
  const row1 = document.getElementById('lcd-row1');
  row0.classList.toggle('big', !!data.bigMode);
  row1.classList.toggle('big', !!data.bigMode);
  setLCDRow(0, data.line0);
  setLCDRow(1, data.line1);

  if (data.mode === 'clock') {
    badge.textContent = 'CLOCK';
  } else if (data.mode === 'countdown') {
    badge.textContent = 'COUNTDOWN';
    badge.className   = 'active';
    btnCDS.disabled   = true;
    btnCDC.disabled   = false;
    btnSCS.disabled   = true;
  } else if (data.mode === 'timeup') {
    badge.textContent = 'TIME UP';
    badge.className   = 'warn';
  } else if (data.mode === 'scroll') {
    badge.textContent = 'SCROLLING';
    badge.className   = 'active';
    btnSCS.disabled   = true;
    btnSCC.disabled   = false;
    btnCDS.disabled   = true;
  }
}

// ── Clock Style / Format Selectors ─────────────────────────────
function setClockStyle(style) {
  updateStyleChoiceUI(style); // optimistic — don't wait for round trip
  sendCmd({ cmd: 'set_clock_style', style: style });
}

function updateStyleChoiceUI(style) {
  document.getElementById('style-choice-0').classList.toggle('selected', style === 0);
  document.getElementById('style-choice-1').classList.toggle('selected', style === 1);
}

function setTimeFormat(is12h) {
  updateFormatChoiceUI(is12h);
  sendCmd({ cmd: 'set_time_format', format12h: is12h });
}

function updateFormatChoiceUI(is12h) {
  document.getElementById('format-choice-24').classList.toggle('selected', !is12h);
  document.getElementById('format-choice-12').classList.toggle('selected', is12h);
}

function setLCDRow(row, text) {
  const el = document.getElementById('lcd-row' + row);
  if (el) el.textContent = (text && text.length > 0) ? text : '\u00a0';
}

// ── Speed Label ────────────────────────────────────────────────
function updateSpeedLabel(val) {
  val = parseInt(val);
  let label = val <= 30 ? 'Fast' : val <= 50 ? 'Normal' : val <= 80 ? 'Medium' : 'Slow';
  document.getElementById('speed-label').textContent = label;
}

// ── Commands ───────────────────────────────────────────────────
function setDateTime() {
  const d = document.getElementById('set-date').value;
  const t = document.getElementById('set-time').value;
  if (!d || !t) { alert('Please set both date and time.'); return; }
  const dp = d.split('-'), tp = t.split(':');
  sendCmd({ cmd:'set_time', year:parseInt(dp[0]), month:parseInt(dp[1]),
            day:parseInt(dp[2]), hour:parseInt(tp[0]), min:parseInt(tp[1]),
            sec: tp[2] ? parseInt(tp[2]) : 0 });
  alert('Time synced to display.');
}

function startCountdown() {
  const raw   = document.getElementById('countdown-input').value.trim();
  const match = raw.match(/^(\d{1,2}):(\d{2}):(\d{2})$/);
  if (!match) { alert('Enter time as HH:MM:SS  e.g.  00:30:00'); return; }
  const h = parseInt(match[1]), m = parseInt(match[2]), s = parseInt(match[3]);
  if (h===0 && m===0 && s===0) { alert('Time cannot be zero.'); return; }
  const fmt   = String(h).padStart(2,'0')+':'+String(m).padStart(2,'0')+':'+String(s).padStart(2,'0');
  const label = document.getElementById('countdown-label').value.trim().toUpperCase() || 'COUNTDOWN';
  const tuSec = parseInt(document.getElementById('timeup-duration').value);

  const headingInterval = parseInt(document.getElementById('heading-interval').value) || 0;
  const headingShow     = parseInt(document.getElementById('heading-show').value) || 3;

  document.getElementById('btn-cd-start').disabled  = true;
  document.getElementById('btn-cd-cancel').disabled = false;

  sendCmd({ cmd:'countdown_start', time:fmt, label:label, timeup_sec:tuSec,
            heading_interval: headingInterval, heading_show: headingShow });
}

function cancelCountdown() {
  document.getElementById('btn-cd-start').disabled  = false;
  document.getElementById('btn-cd-cancel').disabled = true;
  sendCmd({ cmd:'countdown_cancel' });
}

function startScroll() {
  const text  = document.getElementById('scroll-text').value.trim();
  const dur   = parseInt(document.getElementById('scroll-dur').value);
  const speed = parseInt(document.getElementById('scroll-speed').value);
  const font  = parseInt(document.getElementById('scroll-font').value);
  if (!text)           { alert('Enter a message.'); return; }
  if (!dur || dur < 5) { alert('Duration must be at least 5 seconds.'); return; }

  document.getElementById('btn-sc-start').disabled  = true;
  document.getElementById('btn-sc-cancel').disabled = false;

  sendCmd({ cmd:'scroll_start', text:text, duration:dur, speed:speed, font:font });
}

function cancelScroll() {
  document.getElementById('btn-sc-start').disabled  = false;
  document.getElementById('btn-sc-cancel').disabled = true;
  sendCmd({ cmd:'scroll_cancel' });
}

// ── Auto-format countdown input ────────────────────────────────
document.getElementById('countdown-input').addEventListener('input', function() {
  let v = this.value.replace(/[^0-9]/g,'');
  if (v.length > 6) v = v.substring(0,6);
  let out = v.substring(0, Math.min(2,v.length));
  if (v.length > 2) out += ':' + v.substring(2, Math.min(4,v.length));
  if (v.length > 4) out += ':' + v.substring(4,6);
  this.value = out;
});

// ── Prefill date/time ──────────────────────────────────────────
(function() {
  const now = new Date(), p = n => String(n).padStart(2,'0');
  document.getElementById('set-date').value =
    now.getFullYear()+'-'+p(now.getMonth()+1)+'-'+p(now.getDate());
  document.getElementById('set-time').value =
    p(now.getHours())+':'+p(now.getMinutes())+':'+p(now.getSeconds());
})();

function setDot(cls)        { document.getElementById('status-dot').className   = cls; }
function setStatusText(msg) { document.getElementById('status-text').textContent = msg; }

updateStyleChoiceUI(0);
updateFormatChoiceUI(false);
connect();
</script>
</body>
</html>
)rawhtml";