#include "web_panel.h"

#include "audio_player.h"
#include "audio_progress.h"
#include "auto_tuner.h"
#include "equalizer.h"
#include "net_utils.h"
#include "ntp_time.h"
#include "sd_browser.h"
#include "sd_upload.h"
#include "settings.h"
#include "web_log.h"

#include <SD.h>
#include <WebServer.h>
#include <WiFi.h>

static WebServer      server(80);
static RestartAudioFn g_restartCb = nullptr;

// Forward declarations for handlers.
static void handleRoot();
static void handleStatus();
static void handleProgress();
static void handleSet();
static void handleRestart();
static void handleFiles();
static void handlePlay();
static void handleDelete();
static void handleRename();
static void handleLogs();
static void handleEq();

static String htmlPage()
{
  String ip = WiFi.localIP().toString();

  // Current values from g_settings.
  int volPercent = (int)(g_settings.volume * 100.0f + 0.5f);
  int gainDb     = (int)(g_settings.softGain + 0.5f);
  int sr         = g_settings.sampleRate;
  int inbuf      = g_settings.inBufBytes;
  int dmac       = g_settings.dmaBufCount;
  int dmal       = g_settings.dmaBufLen;

  String page = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <title>ESP32 Аудио Панель</title>
  <style>
    *{box-sizing:border-box}
    body{font-family:'Segoe UI',Tahoma,sans-serif;background:linear-gradient(135deg,#0a0f1a 0%,#1a1f35 100%);color:#e7e9ee;margin:0;padding:16px;min-height:100vh}
    .container{max-width:1100px;margin:0 auto}
    .card{background:linear-gradient(180deg,#141b2d 0%,#0f1525 100%);border:1px solid #2a3557;border-radius:16px;padding:20px;margin-bottom:16px;box-shadow:0 8px 32px rgba(0,0,0,.4)}
    h1{font-size:20px;margin:0 0 4px;color:#7eb8ff}
    h2{font-size:16px;margin:0 0 12px;color:#8ec8ff}
    .subtitle{font-size:13px;opacity:.7;margin-bottom:12px}
    .tabs{display:flex;gap:8px;margin-bottom:16px;flex-wrap:wrap}
    .tab{padding:10px 16px;border-radius:10px;border:1px solid #2a3557;background:#10162a;color:#e7e9ee;cursor:pointer;font-size:13px;transition:all .2s}
    .tab:hover,.tab.active{background:#1a2440;border-color:#4a6090}
    .tab.active{color:#7eb8ff;font-weight:600}
    .panel{display:none}
    .panel.active{display:block}
    .row{display:flex;gap:14px;flex-wrap:wrap;margin-bottom:14px}
    .col{flex:1;min-width:240px}
    .box{background:#10162a;border:1px solid #24304d;border-radius:12px;padding:14px;margin-bottom:12px}
    .box-title{font-size:13px;font-weight:600;margin-bottom:10px;color:#8ec8ff}
    label{display:block;font-size:12px;margin-bottom:4px;opacity:.8}
    input[type=range]{width:100%;accent-color:#5a9cff;margin:6px 0}
    input[type=number],input[type=text],select{width:100%;padding:8px 10px;border-radius:8px;border:1px solid #2a3557;background:#0b1020;color:#e7e9ee;font-size:13px}
    input:focus,select:focus{outline:none;border-color:#5a9cff}
    .hint{font-size:11px;opacity:.55;margin-top:6px;line-height:1.4}
    .btns{display:flex;gap:10px;flex-wrap:wrap;margin:12px 0}
    button{padding:10px 16px;border-radius:10px;border:1px solid #3a4a70;background:linear-gradient(135deg,#1a2440,#253560);color:#e7e9ee;cursor:pointer;font-size:13px;font-weight:500;transition:all .15s}
    button:hover{background:linear-gradient(135deg,#253560,#3a5080);border-color:#5a7aaa}
    button:active{transform:scale(.98)}
    .btn-primary{background:linear-gradient(135deg,#2a5090,#3a70c0);border-color:#4a80d0}
    .btn-danger{background:linear-gradient(135deg,#802020,#a03030);border-color:#c04040}
    .btn-success{background:linear-gradient(135deg,#206030,#308040);border-color:#40a050}
    .status-bar{background:#0b1020;border:1px solid #24304d;border-radius:10px;padding:10px 14px;font-size:12px;line-height:1.8}
    .status-ok{color:#6fcf97}
    .status-fail{color:#eb5757}
    .status-warn{color:#f2c94c}
    .progress-bar{height:8px;background:#1a2440;border-radius:4px;overflow:hidden;margin:8px 0}
    .progress-fill{height:100%;background:linear-gradient(90deg,#3a70c0,#5a9cff);transition:width .3s}
    .file-list{max-height:300px;overflow-y:auto;border:1px solid #24304d;border-radius:8px}
    .file-item{display:flex;align-items:center;padding:8px 12px;border-bottom:1px solid #1a2440;cursor:pointer;transition:background .15s}
    .file-item:hover{background:#1a2440}
    .file-item.selected{background:#253560;border-color:#4a80d0}
    .file-item .icon{margin-right:10px;font-size:16px}
    .file-item .name{flex:1;font-size:13px}
    .file-item .size{font-size:11px;opacity:.6}
    .log-box{background:#050810;border:1px solid #1a2440;border-radius:8px;padding:10px;height:250px;overflow-y:auto;font-family:'Consolas',monospace;font-size:11px;line-height:1.5}
    .log-line{margin:2px 0;word-break:break-all}
    .eq-slider{display:flex;flex-direction:column;align-items:center;padding:8px}
    .eq-slider input{writing-mode:vertical-lr;direction:rtl;height:100px;width:24px}
    .eq-slider .freq{font-size:10px;opacity:.7;margin-top:6px}
    .eq-slider .val{font-size:12px;font-weight:600;margin-bottom:4px}
    .eq-row{display:flex;justify-content:space-around;background:#10162a;border-radius:10px;padding:10px}
    .now-playing{background:linear-gradient(135deg,#1a2a50,#253a70);border:1px solid #3a5a90;border-radius:12px;padding:14px;margin-bottom:14px}
    .now-playing .title{font-size:14px;font-weight:600;margin-bottom:8px}
    .now-playing .time{font-size:24px;font-weight:bold;color:#7eb8ff}
    .now-playing .info{font-size:11px;opacity:.7;margin-top:6px}
    .upload-zone{border:2px dashed #3a4a70;border-radius:12px;padding:30px;text-align:center;cursor:pointer;transition:all .2s}
    .upload-zone:hover{border-color:#5a9cff;background:#10162a}
    .upload-zone.dragover{border-color:#6fcf97;background:#102030}
    .checkbox-row{display:flex;align-items:center;gap:8px;margin:8px 0}
    .checkbox-row input{width:auto}
  </style>
</head>
<body>
<div class="container">
  <div class="card">
    <h1>🎵 ESP32 Аудио Панель</h1>
    <div class="subtitle">WAV/MP3 Player + Web Panel | IP: <b>)HTML";
  page += ip;
  page += R"HTML(</b></div>

    <div class="tabs">
      <div class="tab active" onclick="showPanel('player')">🎵 Плеер</div>
      <div class="tab" onclick="showPanel('files')">📂 Файлы</div>
      <div class="tab" onclick="showPanel('eq')">🎛 Эквалайзер</div>
      <div class="tab" onclick="showPanel('settings')">⚙️ Настройки</div>
      <div class="tab" onclick="showPanel('logs')">📟 Логи</div>
    </div>
  </div>

  <!-- PLAYER PANEL -->
  <div id="panel-player" class="panel active">
    <div class="now-playing">
      <div class="title" id="np-file">Загрузка...</div>
      <div class="progress-bar"><div class="progress-fill" id="np-progress" style="width:0%"></div></div>
      <div style="display:flex;justify-content:space-between;align-items:center">
        <div class="time"><span id="np-time">00:00</span> / <span id="np-total">00:00</span></div>
        <div id="np-percent">0%</div>
      </div>
      <div class="info" id="np-info">-</div>
    </div>

    <div class="card">
      <div class="row">
        <div class="col">
          <div class="box">
            <div class="box-title">🔊 Громкость (0-200%)</div>
            <input id="vol" type="range" min="0" max="200" value=")HTML";
  page += String(volPercent);
  page += R"HTML(" oninput="volVal.innerText=this.value+'%'"/>
            <div style="text-align:center;font-size:18px;font-weight:bold"><span id="volVal">)HTML";
  page += String(volPercent);
  page += R"HTML(%</span></div>
            <div class="hint">100% = оригинал, >100% = усиление (может искажать)</div>
          </div>
        </div>
        <div class="col">
          <div class="box">
            <div class="box-title">📢 Доп. усиление (0-12 dB)</div>
            <input id="gain" type="range" min="0" max="12" value=")HTML";
  page += String(gainDb);
  page += R"HTML(" oninput="gainVal.innerText='+'+this.value+' dB'"/>
            <div style="text-align:center;font-size:18px;font-weight:bold"><span id="gainVal">+)HTML";
  page += String(gainDb);
  page += R"HTML( dB</span></div>
            <div class="hint">+6 dB = x2 громкость, +12 dB = x4 громкость</div>
          </div>
        </div>
      </div>

      <div class="checkbox-row">
        <input type="checkbox" id="soft-limiter" checked>
        <label for="soft-limiter">🛡️ Soft Limiter (защита от искажений при высокой громкости)</label>
      </div>

      <div class="btns">
        <button class="btn-success" onclick="playAudio()">▶️ Воспроизвести</button>
        <button class="btn-danger" onclick="stopAudio()">⏹ Стоп</button>
        <button onclick="restartAudio()">🔄 Перезапуск</button>
        <button onclick="applyVolume()">💾 Применить громкость</button>
      </div>
    </div>

    <div class="status-bar" id="status">Загрузка...</div>
  </div>

  <!-- FILES PANEL -->
  <div id="panel-files" class="panel">
    <div class="card">
      <h2>📂 Файлы на SD карте</h2>
      <div class="btns">
        <button onclick="refreshFiles()">🔄 Обновить</button>
        <button onclick="goUp()">⬆️ Вверх</button>
        <input type="text" id="current-path" value="/" style="flex:1" readonly>
      </div>
      <div class="file-list" id="file-list"></div>

      <div class="btns" style="margin-top:14px">
        <button class="btn-success" onclick="playSelected()">▶️ Воспроизвести</button>
        <button class="btn-danger" onclick="deleteSelected()">🗑 Удалить</button>
        <button onclick="renameSelected()">✏️ Переименовать</button>
      </div>
    </div>

    <div class="card">
      <h2>📤 Загрузка файлов</h2>
      <div class="upload-zone" id="upload-zone" onclick="document.getElementById('upload-input').click()">
        <div style="font-size:32px;margin-bottom:10px">📁</div>
        <div>Нажмите или перетащите файл сюда</div>
        <div class="hint">Поддерживаются: .wav, .mp3</div>
      </div>
      <input type="file" id="upload-input" accept=".wav,.mp3" style="display:none" onchange="uploadFile(this.files[0])">
      <div id="upload-status" class="hint" style="margin-top:10px"></div>
    </div>
  </div>

  <!-- EQ PANEL -->
  <div id="panel-eq" class="panel">
    <div class="card">
      <h2>🎛 5-полосный эквалайзер</h2>
      <div class="hint" style="background:#3d2c00;border:1px solid #f0a000;padding:10px;border-radius:8px;margin-bottom:15px">
        ⚠️ <b>Важно:</b> Эквалайзер работает только с WAV файлами!<br>
        Для MP3 используется обход — конвертируйте в WAV для полного контроля звука.
      </div>
      <div class="checkbox-row">
        <input type="checkbox" id="eq-enabled" onchange="toggleEq()">
        <label for="eq-enabled">Включить эквалайзер</label>
      </div>

      <div class="eq-row" id="eq-sliders">
        <div class="eq-slider">
          <div class="val" id="eq-val-0">0</div>
          <input type="range" id="eq-0" min="-12" max="12" value="0" oninput="updateEqVal(0)">
          <div class="freq">60 Hz<br>Sub-bass</div>
        </div>
        <div class="eq-slider">
          <div class="val" id="eq-val-1">0</div>
          <input type="range" id="eq-1" min="-12" max="12" value="0" oninput="updateEqVal(1)">
          <div class="freq">250 Hz<br>Bass</div>
        </div>
        <div class="eq-slider">
          <div class="val" id="eq-val-2">0</div>
          <input type="range" id="eq-2" min="-12" max="12" value="0" oninput="updateEqVal(2)">
          <div class="freq">1 kHz<br>Mid</div>
        </div>
        <div class="eq-slider">
          <div class="val" id="eq-val-3">0</div>
          <input type="range" id="eq-3" min="-12" max="12" value="0" oninput="updateEqVal(3)">
          <div class="freq">4 kHz<br>Presence</div>
        </div>
        <div class="eq-slider">
          <div class="val" id="eq-val-4">0</div>
          <input type="range" id="eq-4" min="-12" max="12" value="0" oninput="updateEqVal(4)">
          <div class="freq">12 kHz<br>Brilliance</div>
        </div>
      </div>

      <div class="btns">
        <button class="btn-primary" onclick="applyEq()">💾 Применить EQ</button>
        <button onclick="resetEq()">🔄 Сбросить</button>
      </div>

      <div class="hint" style="margin-top:12px">
        <b>Подсказка:</b> Диапазон каждой полосы от -12 до +12 дБ.<br>
        • <b>Sub-bass (60 Hz)</b> — глубокий бас, ощущается телом<br>
        • <b>Bass (250 Hz)</b> — основной бас, «мясо» звука<br>
        • <b>Mid (1 kHz)</b> — голоса, основные инструменты<br>
        • <b>Presence (4 kHz)</b> — чёткость, разборчивость<br>
        • <b>Brilliance (12 kHz)</b> — воздух, яркость
      </div>
    </div>
  </div>

  <!-- SETTINGS PANEL -->
  <div id="panel-settings" class="panel">
    <div class="card">
      <h2>⚙️ Настройки аудио</h2>

      <div class="row">
        <div class="col">
          <div class="box">
            <div class="box-title">Частота дискретизации</div>
            <input id="sr" type="number" value=")HTML";
  page += String(sr);
  page += R"HTML(" min="8000" max="48000">
            <div class="hint">Стандарт: 44100 (CD) или 48000 (видео)</div>
          </div>
        </div>
        <div class="col">
          <div class="box">
            <div class="box-title">Буфер чтения SD (байт)</div>
            <input id="inbuf" type="number" value=")HTML";
  page += String(inbuf);
  page += R"HTML(" min="512" max="8192">
            <div class="hint">Больше = меньше хрипов. Рекомендуется 4096-8192</div>
          </div>
        </div>
      </div>

      <div class="row">
        <div class="col">
          <div class="box">
            <div class="box-title">DMA буферы</div>
            <label>Количество (4-16)</label>
            <input id="dmac" type="number" value=")HTML";
  page += String(dmac);
  page += R"HTML(" min="4" max="16">
            <label style="margin-top:8px">Размер (128-1024)</label>
            <input id="dmal" type="number" value=")HTML";
  page += String(dmal);
  page += R"HTML(" min="128" max="1024">
            <div class="hint">Больше = плавнее звук, но больше задержка</div>
          </div>
        </div>
        <div class="col">
          <div class="box">
            <div class="box-title">Дополнительно</div>
            <div class="checkbox-row">
              <input type="checkbox" id="auto-tune">
              <label for="auto-tune">Авто-тюнинг буферов</label>
            </div>
            <div class="checkbox-row">
              <input type="checkbox" id="resampling">
              <label for="resampling">Ресемплинг</label>
            </div>
            <div class="hint">Авто-тюнинг увеличивает буферы при хрипах.<br>Ресемплинг конвертирует частоту WAV под настройки.</div>
          </div>
        </div>
      </div>

      <div class="row">
        <div class="col">
          <div class="box">
            <div class="box-title">🕐 Часовой пояс</div>
            <select id="timezone">
              <option value="0">UTC+0 (Лондон)</option>
              <option value="3600">UTC+1 (Берлин)</option>
              <option value="7200">UTC+2 (Киев)</option>
              <option value="10800" selected>UTC+3 (Москва)</option>
              <option value="14400">UTC+4 (Самара)</option>
              <option value="18000">UTC+5 (Екатеринбург)</option>
              <option value="21600">UTC+6 (Омск)</option>
              <option value="25200">UTC+7 (Красноярск)</option>
              <option value="28800">UTC+8 (Иркутск)</option>
              <option value="32400">UTC+9 (Якутск)</option>
              <option value="36000">UTC+10 (Владивосток)</option>
              <option value="39600">UTC+11 (Магадан)</option>
              <option value="43200">UTC+12 (Камчатка)</option>
              <option value="-18000">UTC-5 (Нью-Йорк)</option>
              <option value="-28800">UTC-8 (Лос-Анджелес)</option>
            </select>
            <div class="hint">Часовой пояс для отображения времени в логах</div>
          </div>
        </div>
      </div>

      <div class="btns">
        <button class="btn-primary" onclick="applySettings()">💾 Сохранить настройки</button>
        <button onclick="restartAudio()">🔄 Перезапустить аудио</button>
      </div>
    </div>
  </div>

  <!-- LOGS PANEL -->
  <div id="panel-logs" class="panel">
    <div class="card">
      <h2>📟 Serial Log</h2>
      <div class="btns">
        <button onclick="refreshLogs()">🔄 Обновить</button>
        <button onclick="clearLogs()">🗑 Очистить</button>
        <button onclick="copyLogs()">📋 Копировать</button>
        <div class="checkbox-row" style="margin-left:auto">
          <input type="checkbox" id="auto-refresh" checked>
          <label for="auto-refresh">Авто-обновление</label>
        </div>
      </div>
      <div class="log-box" id="log-box"></div>
      <div id="copy-status" class="hint" style="margin-top:8px;color:#6fcf97;display:none">✅ Скопировано в буфер обмена!</div>
    </div>
  </div>
</div>

<script>
let currentPath = '/';
let selectedFile = null;
let logAutoRefresh = true;
let slidersInitialized = false;  // Only update sliders on first load.

function showPanel(name) {
  document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  document.getElementById('panel-' + name).classList.add('active');
  event.target.classList.add('active');

  if (name === 'files') refreshFiles();
  if (name === 'logs') refreshLogs();
}

async function refreshStatus() {
  try {
    const r = await fetch('/status');
    const j = await r.json();

    let html = '';
    html += `<span style="margin-right:16px">📡 WiFi: <b class="${j.wifi==='OK'?'status-ok':'status-fail'}">${j.wifi}</b></span>`;
    html += `<span style="margin-right:16px">🎵 Аудио: <b class="${j.audio==='PLAYING'?'status-ok':'status-warn'}">${j.audio}</b></span>`;
    html += `<span style="margin-right:16px">💾 Heap: <b>${j.heap}</b></span>`;
    html += `<span>📄 Settings: <b class="${j.settings==='OK'?'status-ok':'status-fail'}">${j.settings}</b></span>`;
    document.getElementById('status').innerHTML = html;

    // Only update sliders/checkboxes on first load to avoid overwriting user input.
    if (!slidersInitialized) {
      // Update checkboxes.
      document.getElementById('auto-tune').checked = j.autoTune === 'ON';
      document.getElementById('resampling').checked = j.resampling === 'ON';
      document.getElementById('eq-enabled').checked = j.eqEnabled === 'ON';
      document.getElementById('soft-limiter').checked = j.softLimiter === 'ON';

      // Update volume and gain sliders.
      if (j.volume !== undefined) {
        document.getElementById('vol').value = j.volume;
        document.getElementById('volVal').innerText = j.volume + '%';
      }
      if (j.softGain !== undefined) {
        document.getElementById('gain').value = j.softGain;
        document.getElementById('gainVal').innerText = '+' + j.softGain + ' dB';
      }

      // Update timezone selector.
      if (j.timezoneOffset !== undefined) {
        document.getElementById('timezone').value = j.timezoneOffset;
      }

      slidersInitialized = true;
    }
  } catch(e) {
    document.getElementById('status').innerText = '❌ Ошибка: ' + e;
  }
}

async function refreshProgress() {
  try {
    const r = await fetch('/progress');
    const j = await r.json();

    document.getElementById('np-file').innerText = j.fileName || 'Нет файла';
    document.getElementById('np-time').innerText = j.playedTime || '00:00';
    document.getElementById('np-total').innerText = j.totalTime || '00:00';
    document.getElementById('np-percent').innerText = j.percent + '%';
    document.getElementById('np-progress').style.width = j.percent + '%';
    document.getElementById('np-info').innerText =
      `${j.sampleRate} Hz | ${j.channels} ch | ${j.bitsPerSample} bit`;
  } catch(e) {}
}

async function applyVolume() {
  const vol = document.getElementById('vol').value;
  const gain = document.getElementById('gain').value;
  const limiter = document.getElementById('soft-limiter').checked ? 1 : 0;
  await fetch(`/set?vol=${vol}&gain=${gain}&limiter=${limiter}`);
  // Don't reset slidersInitialized - keep user values.
}

async function applySettings() {
  const vol = document.getElementById('vol').value;
  const gain = document.getElementById('gain').value;
  const limiter = document.getElementById('soft-limiter').checked ? 1 : 0;
  const sr = document.getElementById('sr').value;
  const inbuf = document.getElementById('inbuf').value;
  const dmac = document.getElementById('dmac').value;
  const dmal = document.getElementById('dmal').value;
  const autoTune = document.getElementById('auto-tune').checked ? 1 : 0;
  const resampling = document.getElementById('resampling').checked ? 1 : 0;
  const tz = document.getElementById('timezone').value;

  await fetch(`/set?vol=${vol}&gain=${gain}&limiter=${limiter}&sr=${sr}&inbuf=${inbuf}&dmac=${dmac}&dmal=${dmal}&autoTune=${autoTune}&resampling=${resampling}&tz=${tz}`);
  alert('Настройки сохранены!');
  // Don't reset slidersInitialized - keep user values.
}

async function playAudio() {
  await fetch('/play');
  refreshProgress();
}

async function stopAudio() {
  await fetch('/restart?stop=1');
  refreshProgress();
}

async function restartAudio() {
  await fetch('/restart');
  refreshProgress();
}

// FILES
async function refreshFiles() {
  try {
    const r = await fetch('/files?path=' + encodeURIComponent(currentPath));
    const files = await r.json();

    document.getElementById('current-path').value = currentPath;

    let html = '';
    files.forEach(f => {
      const lowerName = f.name.toLowerCase();
      const isAudio = lowerName.endsWith('.wav') || lowerName.endsWith('.mp3');
      const icon = f.isDir ? '📁' : (isAudio ? '🎵' : '📄');
      const size = f.isDir ? '' : formatSize(f.size);
      html += `<div class="file-item" onclick="selectFile('${f.name}', ${f.isDir})" ondblclick="openFile('${f.name}', ${f.isDir})">
        <span class="icon">${icon}</span>
        <span class="name">${f.name}</span>
        <span class="size">${size}</span>
      </div>`;
    });
    document.getElementById('file-list').innerHTML = html || '<div style="padding:20px;text-align:center;opacity:.5">Пусто</div>';
    selectedFile = null;
  } catch(e) {
    document.getElementById('file-list').innerHTML = '<div style="padding:20px;color:#eb5757">Ошибка загрузки</div>';
  }
}

function formatSize(bytes) {
  if (bytes < 1024) return bytes + ' B';
  if (bytes < 1024*1024) return (bytes/1024).toFixed(1) + ' KB';
  return (bytes/1024/1024).toFixed(1) + ' MB';
}

function selectFile(name, isDir) {
  document.querySelectorAll('.file-item').forEach(el => el.classList.remove('selected'));
  event.currentTarget.classList.add('selected');
  selectedFile = { name, isDir, path: currentPath + (currentPath.endsWith('/') ? '' : '/') + name };
}

function openFile(name, isDir) {
  if (isDir) {
    currentPath = currentPath + (currentPath.endsWith('/') ? '' : '/') + name;
    refreshFiles();
  } else {
    const lowerName = name.toLowerCase();
    if (lowerName.endsWith('.wav') || lowerName.endsWith('.mp3')) {
      playSelected();
    }
  }
}

function goUp() {
  if (currentPath === '/') return;
  const parts = currentPath.split('/').filter(p => p);
  parts.pop();
  currentPath = '/' + parts.join('/');
  if (!currentPath.endsWith('/') && currentPath !== '/') currentPath += '/';
  if (currentPath === '') currentPath = '/';
  refreshFiles();
}

async function playSelected() {
  if (!selectedFile || selectedFile.isDir) {
    alert('Выберите аудио файл (WAV или MP3)');
    return;
  }
  const lowerName = selectedFile.name.toLowerCase();
  if (!lowerName.endsWith('.wav') && !lowerName.endsWith('.mp3')) {
    alert('Поддерживаются только WAV и MP3 файлы');
    return;
  }
  await fetch('/play?file=' + encodeURIComponent(selectedFile.path));

  // Switch to player tab and refresh.
  document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  document.getElementById('panel-player').classList.add('active');
  document.querySelector('.tab').classList.add('active');

  // Update progress immediately.
  setTimeout(refreshProgress, 500);
}

async function deleteSelected() {
  if (!selectedFile) {
    alert('Выберите файл');
    return;
  }
  if (!confirm('Удалить ' + selectedFile.name + '?')) return;

  const r = await fetch('/delete?path=' + encodeURIComponent(selectedFile.path));
  const txt = await r.text();
  alert(txt);
  refreshFiles();
}

async function renameSelected() {
  if (!selectedFile) {
    alert('Выберите файл');
    return;
  }
  const newName = prompt('Новое имя:', selectedFile.name);
  if (!newName || newName === selectedFile.name) return;

  const newPath = currentPath + (currentPath.endsWith('/') ? '' : '/') + newName;
  const r = await fetch('/rename?old=' + encodeURIComponent(selectedFile.path) + '&new=' + encodeURIComponent(newPath));
  const txt = await r.text();
  alert(txt);
  refreshFiles();
}

// UPLOAD
const uploadZone = document.getElementById('upload-zone');
uploadZone.addEventListener('dragover', e => { e.preventDefault(); uploadZone.classList.add('dragover'); });
uploadZone.addEventListener('dragleave', e => { uploadZone.classList.remove('dragover'); });
uploadZone.addEventListener('drop', e => {
  e.preventDefault();
  uploadZone.classList.remove('dragover');
  if (e.dataTransfer.files.length) uploadFile(e.dataTransfer.files[0]);
});

async function uploadFile(file) {
  if (!file) return;

  const statusEl = document.getElementById('upload-status');
  const totalSize = file.size;
  const startTime = Date.now();

  statusEl.innerHTML = `
    <div style="margin:10px 0">
      <div>📤 <b>${file.name}</b> (${formatSize(totalSize)})</div>
      <div class="progress-bar" style="margin:8px 0"><div class="progress-fill" id="upload-progress" style="width:0%"></div></div>
      <div id="upload-info">Начало загрузки...</div>
    </div>
  `;

  const formData = new FormData();
  formData.append('file', file);

  try {
    const xhr = new XMLHttpRequest();

    xhr.upload.addEventListener('progress', (e) => {
      if (e.lengthComputable) {
        const percent = Math.round((e.loaded / e.total) * 100);
        const elapsed = (Date.now() - startTime) / 1000;
        const speed = e.loaded / 1024 / elapsed;
        const remaining = (e.total - e.loaded) / 1024 / speed;

        document.getElementById('upload-progress').style.width = percent + '%';
        document.getElementById('upload-info').innerHTML =
          `${formatSize(e.loaded)} / ${formatSize(e.total)} (${percent}%) | ` +
          `${speed.toFixed(1)} KB/s | Осталось: ${Math.ceil(remaining)} сек`;
      }
    });

    xhr.onload = () => {
      if (xhr.status === 200) {
        const j = JSON.parse(xhr.responseText);
        statusEl.innerHTML = `<div style="color:#6fcf97">${j.status}</div>`;
        refreshFiles();
      } else {
        statusEl.innerHTML = `<div style="color:#eb5757">❌ Ошибка: ${xhr.status}</div>`;
      }
    };

    xhr.onerror = () => {
      statusEl.innerHTML = `<div style="color:#eb5757">❌ Ошибка сети</div>`;
    };

    xhr.open('POST', '/upload');
    xhr.send(formData);
  } catch(e) {
    statusEl.innerHTML = `<div style="color:#eb5757">❌ Ошибка: ${e}</div>`;
  }
}

// EQ
function updateEqVal(idx) {
  const val = document.getElementById('eq-' + idx).value;
  document.getElementById('eq-val-' + idx).innerText = val > 0 ? '+' + val : val;
}

function toggleEq() {
  const enabled = document.getElementById('eq-enabled').checked;
  fetch('/eq?enabled=' + (enabled ? 1 : 0));
}

async function applyEq() {
  const enabled = document.getElementById('eq-enabled').checked ? 1 : 0;
  const b0 = document.getElementById('eq-0').value;
  const b1 = document.getElementById('eq-1').value;
  const b2 = document.getElementById('eq-2').value;
  const b3 = document.getElementById('eq-3').value;
  const b4 = document.getElementById('eq-4').value;

  await fetch(`/eq?enabled=${enabled}&b0=${b0}&b1=${b1}&b2=${b2}&b3=${b3}&b4=${b4}`);
  alert('EQ сохранён!');
}

function resetEq() {
  for (let i = 0; i < 5; i++) {
    document.getElementById('eq-' + i).value = 0;
    document.getElementById('eq-val-' + i).innerText = '0';
  }
}

// LOGS
async function refreshLogs() {
  try {
    const r = await fetch('/logs');
    const logs = await r.json();

    const box = document.getElementById('log-box');
    box.innerHTML = logs.map(l => `<div class="log-line">${l}</div>`).join('');
    box.scrollTop = box.scrollHeight;
  } catch(e) {}
}

function clearLogs() {
  fetch('/logs?clear=1');
  document.getElementById('log-box').innerHTML = '';
}

async function copyLogs() {
  const box = document.getElementById('log-box');
  const lines = Array.from(box.querySelectorAll('.log-line')).map(el => el.innerText);
  const text = lines.join('\n');

  try {
    await navigator.clipboard.writeText(text);
    const status = document.getElementById('copy-status');
    status.style.display = 'block';
    setTimeout(() => { status.style.display = 'none'; }, 2000);
  } catch(e) {
    // Fallback for older browsers.
    const textarea = document.createElement('textarea');
    textarea.value = text;
    textarea.style.position = 'fixed';
    textarea.style.opacity = '0';
    document.body.appendChild(textarea);
    textarea.select();
    document.execCommand('copy');
    document.body.removeChild(textarea);

    const status = document.getElementById('copy-status');
    status.style.display = 'block';
    setTimeout(() => { status.style.display = 'none'; }, 2000);
  }
}

// Auto-refresh
setInterval(() => {
  refreshStatus();
  refreshProgress();
  if (document.getElementById('auto-refresh').checked &&
      document.getElementById('panel-logs').classList.contains('active')) {
    refreshLogs();
  }
}, 2000);

// Init
refreshStatus();
refreshProgress();
</script>
</body>
</html>
)HTML";
  return page;
}

static void handleRoot()
{
  server.send(200, "text/html", htmlPage());
}

static void handleStatus()
{
  bool googleOk     = checkGoogleTCP();
  bool audioRunning = audioIsRunning();

  int volPercent = (int)(g_settings.volume * 100.0f + 0.5f);
  int gainDb     = (int)(g_settings.softGain + 0.5f);

  String json = "{";
  json += "\"wifi\":\"" + String(isWiFiOk() ? "OK" : "DOWN") + "\",";
  json += "\"ip\":\"" + ipToString() + "\",";
  json += "\"google\":\"" + String(googleOk ? "OK" : "FAIL") + "\",";
  json += "\"audio\":\"" + String(audioRunning ? "PLAYING" : "STOPPED") + "\",";
  json += "\"heap\":\"" + String(ESP.getFreeHeap()) + "\",";
  json += "\"settings\":\"" + String(SD.exists("/settings.json") ? "OK" : "MISSING") + "\",";
  json += "\"autoTune\":\"" + String(g_settings.autoTuneEnabled ? "ON" : "OFF") + "\",";
  json += "\"resampling\":\"" + String(g_settings.resamplingEnabled ? "ON" : "OFF") + "\",";
  json += "\"eqEnabled\":\"" + String(g_settings.eqEnabled ? "ON" : "OFF") + "\",";
  json += "\"softLimiter\":\"" + String(g_settings.softLimiterEnabled ? "ON" : "OFF") + "\",";
  json += "\"volume\":" + String(volPercent) + ",";
  json += "\"softGain\":" + String(gainDb) + ",";
  json += "\"timezoneOffset\":" + String(g_settings.timezoneOffset);
  json += "}";

  server.send(200, "application/json", json);
}

static void handleProgress()
{
  server.send(200, "application/json", progressGetJson());
}

static void handleSet()
{
  if (server.hasArg("vol")) {
    int v = server.arg("vol").toInt();
    if (v < 0)
      v = 0;
    if (v > 200)
      v = 200;
    g_settings.volume = (float)v / 100.0f;
    WebLog.print("[WEB] volume=");
    WebLog.println(g_settings.volume, 3);
  }

  if (server.hasArg("gain")) {
    int g = server.arg("gain").toInt();
    if (g < 0)
      g = 0;
    if (g > 12)
      g = 12;
    g_settings.softGain = (float)g;
    WebLog.print("[WEB] softGain=");
    WebLog.print(g_settings.softGain);
    WebLog.println(" dB");
  }

  if (server.hasArg("limiter")) {
    g_settings.softLimiterEnabled = server.arg("limiter").toInt() == 1;
    WebLog.print("[WEB] softLimiterEnabled=");
    WebLog.println(g_settings.softLimiterEnabled);
  }

  if (server.hasArg("sr")) {
    g_settings.sampleRate = server.arg("sr").toInt();
    WebLog.print("[WEB] sampleRate=");
    WebLog.println(g_settings.sampleRate);
  }

  if (server.hasArg("inbuf")) {
    g_settings.inBufBytes = server.arg("inbuf").toInt();
    WebLog.print("[WEB] inBufBytes=");
    WebLog.println(g_settings.inBufBytes);
  }

  if (server.hasArg("dmac")) {
    g_settings.dmaBufCount = server.arg("dmac").toInt();
    WebLog.print("[WEB] dmaBufCount=");
    WebLog.println(g_settings.dmaBufCount);
  }

  if (server.hasArg("dmal")) {
    g_settings.dmaBufLen = server.arg("dmal").toInt();
    WebLog.print("[WEB] dmaBufLen=");
    WebLog.println(g_settings.dmaBufLen);
  }

  if (server.hasArg("autoTune")) {
    g_settings.autoTuneEnabled = server.arg("autoTune").toInt() == 1;
    WebLog.print("[WEB] autoTuneEnabled=");
    WebLog.println(g_settings.autoTuneEnabled);
  }

  if (server.hasArg("resampling")) {
    g_settings.resamplingEnabled = server.arg("resampling").toInt() == 1;
    WebLog.print("[WEB] resamplingEnabled=");
    WebLog.println(g_settings.resamplingEnabled);
  }

  if (server.hasArg("tz")) {
    g_settings.timezoneOffset = server.arg("tz").toInt();
    WebLog.print("[WEB] timezoneOffset=");
    WebLog.println(g_settings.timezoneOffset);
    // Update NTP timezone.
    ntpSetTimezoneOffset(g_settings.timezoneOffset);
  }

  bool ok = settingsSaveToSD();
  server.send(200, "text/plain", ok ? "OK" : "FAIL");
}

static void handleRestart()
{
  WebLog.println("[WEB] Restart audio requested");

  if (server.hasArg("stop") && server.arg("stop") == "1") {
    audioStop();
    server.send(200, "text/plain", "Stopped");
    return;
  }

  if (g_restartCb)
    g_restartCb();
  server.send(200, "text/plain", "Restarted");
}

static void handleFiles()
{
  String path = server.hasArg("path") ? server.arg("path") : "/";
  String json = sdListDir(path);
  server.send(200, "application/json", json);
}

static void handlePlay()
{
  String file = server.hasArg("file") ? server.arg("file") : g_settings.currentFile;

  if (file.length() == 0) {
    server.send(400, "text/plain", "No file specified");
    return;
  }

  if (!sdFileExists(file)) {
    server.send(404, "text/plain", "File not found");
    return;
  }

  if (!audioIsSupportedFormat(file)) {
    server.send(400, "text/plain", "Unsupported format. Use WAV or MP3.");
    return;
  }

  audioStartFile(file);
  server.send(200, "text/plain", "Playing: " + file);
}

static void handleDelete()
{
  if (!server.hasArg("path")) {
    server.send(400, "text/plain", "No path");
    return;
  }

  String path = server.arg("path");
  bool   ok   = sdDeleteFile(path);
  server.send(200, "text/plain", ok ? "Удалено" : "Ошибка удаления");
}

static void handleRename()
{
  if (!server.hasArg("old") || !server.hasArg("new")) {
    server.send(400, "text/plain", "Missing params");
    return;
  }

  String oldPath = server.arg("old");
  String newPath = server.arg("new");
  bool   ok      = sdRenameFile(oldPath, newPath);
  server.send(200, "text/plain", ok ? "Переименовано" : "Ошибка переименования");
}

static void handleLogs()
{
  if (server.hasArg("clear") && server.arg("clear") == "1") {
    webLogClear();
    server.send(200, "application/json", "[]");
    return;
  }

  server.send(200, "application/json", webLogPeekJson());
}

static void handleEq()
{
  if (server.hasArg("enabled")) {
    g_settings.eqEnabled = server.arg("enabled").toInt() == 1;
    g_eqSettings.enabled = g_settings.eqEnabled;
  }

  if (server.hasArg("b0")) {
    g_settings.eq.band60Hz = server.arg("b0").toFloat();
    g_eqSettings.band60Hz  = g_settings.eq.band60Hz;
  }
  if (server.hasArg("b1")) {
    g_settings.eq.band250Hz = server.arg("b1").toFloat();
    g_eqSettings.band250Hz  = g_settings.eq.band250Hz;
  }
  if (server.hasArg("b2")) {
    g_settings.eq.band1kHz = server.arg("b2").toFloat();
    g_eqSettings.band1kHz  = g_settings.eq.band1kHz;
  }
  if (server.hasArg("b3")) {
    g_settings.eq.band4kHz = server.arg("b3").toFloat();
    g_eqSettings.band4kHz  = g_settings.eq.band4kHz;
  }
  if (server.hasArg("b4")) {
    g_settings.eq.band12kHz = server.arg("b4").toFloat();
    g_eqSettings.band12kHz  = g_settings.eq.band12kHz;
  }

  // Update coefficients for current sample rate.
  eqUpdateCoefficients(g_settings.sampleRate);

  settingsSaveToSD();
  server.send(200, "text/plain", "EQ updated");
}

void webPanelBegin(RestartAudioFn restartCb)
{
  g_restartCb = restartCb;

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/progress", handleProgress);
  server.on("/set", handleSet);
  server.on("/restart", handleRestart);
  server.on("/files", handleFiles);
  server.on("/play", handlePlay);
  server.on("/delete", handleDelete);
  server.on("/rename", handleRename);
  server.on("/logs", handleLogs);
  server.on("/eq", handleEq);

  // Initialize upload handlers.
  sdUploadBegin(server);

  server.begin();

  WebLog.println("[WEB] ✅ Web server started on port 80");
  WebLog.print("[WEB] Open: http://");
  WebLog.print(WiFi.localIP());
  WebLog.println("/");
}

void webPanelHandleClient()
{
  server.handleClient();
}
