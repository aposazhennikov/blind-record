# ESP32 WAV Player + Web Panel (PlatformIO)

Проект для **ESP32**, который:

* подключается к **Wi-Fi**
* запускает **WEB-панель управления** на порту **80**
* читает/сохраняет настройки в **`/settings.json`** на **SD-карте**
* воспроизводит **`/test.wav`** через **I2S (MAX98357A)**
* логирует все шаги в **Serial Monitor**

---

## 1) Что делает проект

После запуска ESP32 выполняет последовательность:

1. Подключается к Wi-Fi (`SSID / password` заданы в конфиге)
2. Печатает в Serial Monitor:

   * SSID
   * IP / Gateway / DNS
   * RSSI
   * проверку DNS `google.com`
   * проверку “доступен ли интернет” через TCP подключение к `google.com:80`
3. Запускает Web Server на порту 80
4. Инициализирует SD-карту
5. Загружает аудио-настройки из `/settings.json` (или создаёт файл при первом старте)
6. Инициализирует I2S по настройкам
7. Запускает проигрывание `/test.wav`

---

## 2) WEB UI (панель управления)

После подключения к Wi-Fi в Serial Monitor появится строка вида:

```
[WEB] Open: http://<IP_адрес_ESP32>/
```

Открой этот адрес в браузере и ты увидишь панель, где можно менять:

* **Volume** (громкость)
* **Sample Rate**
* **SD Read Buffer**
* **I2S DMA buf count**
* **I2S DMA buf len**

### Кнопки:

* **Apply settings (saved to SD)** — применяет настройки и сохраняет их в `/settings.json`
* **Restart audio** — перезапускает аудио с текущими настройками
* **Refresh status** — обновляет статус сети/памяти/наличия settings.json

---

## 3) Файлы проекта и структура

```
Blind-record/
├─ platformio.ini
├─ include/
│  ├─ app_config.h
│  ├─ settings.h
│  ├─ net_utils.h
│  ├─ i2s_audio.h
│  ├─ wav_reader.h
│  ├─ audio_player.h
│  ├─ web_panel.h
├─ src/
│  ├─ main.cpp
│  ├─ settings.cpp
│  ├─ net_utils.cpp
│  ├─ i2s_audio.cpp
│  ├─ wav_reader.cpp
│  ├─ audio_player.cpp
│  ├─ web_panel.cpp
```

---

## 4) Разница между .h и .cpp (простое объяснение)

### `.h` (Header / Заголовок)

Это **описание интерфейса** (что модуль умеет):

* объявления структур
* объявления глобальных переменных (`extern`)
* прототипы функций

Можно представить `.h` как “инструкцию”:

> «Вот какие функции можно вызывать и какие типы данных используются».

---

### `.cpp` (Source / Реализация)

Это **реальный код**, что именно выполняется внутри функций.

Можно представить `.cpp` как “движок”:

> «Вот как именно работает функция: шаги, логика, действия».

---

## 5) Разница между include/ и src/

### `src/`

Всё что здесь лежит — **компилируется** PlatformIO автоматически (`*.cpp`).

### `include/`

Здесь лежат заголовочные файлы (`*.h`), которые подключаются через:

```cpp
#include "имя_файла.h"
```

PlatformIO автоматически добавляет `include/` в пути поиска заголовков.

---

## 6) Модули проекта (кто за что отвечает)

### `app_config.h`

Конфигурация проекта:

* Wi-Fi SSID/password
* пины SD-карты
* пины I2S

---

### `net_utils.h / net_utils.cpp`

Сеть:

* подключение к Wi-Fi и логирование
* DNS проверка `google.com`
* TCP проверка доступа к `google.com:80`
* полезные функции `isWiFiOk()`, `ipToString()`

---

### `settings.h / settings.cpp`

Настройки аудио:

* структура `AudioSettings`
* чтение и запись `/settings.json`
* создание defaults при первом запуске

Файл настроек хранится на SD:

```
/settings.json
```

---

### `i2s_audio.h / i2s_audio.cpp`

Работа с I2S:

* `i2sInitFromSettings()` — инициализация I2S из настроек (sampleRate, DMA)
* `i2sDeinit()` — деинициализация I2S

---

### `wav_reader.h / wav_reader.cpp`

Парсер WAV файла:

* читает заголовок WAV
* определяет:

  * PCM ли это
  * Mono или Stereo
  * sampleRate / bitsPerSample
  * где начинается аудио data chunk

---

### `audio_player.h / audio_player.cpp`

Проигрыватель:

* запускает задачу FreeRTOS на Core 1
* открывает `/test.wav`
* читает WAV header и проверяет формат
* воспроизводит файл через I2S
* поддерживает MONO и STEREO
* применяет громкость `volume`
* поддерживает:

  * `audioStart()`
  * `audioStop()`
  * `audioRestart()`

---

### `web_panel.h / web_panel.cpp`

WEB панель:

* поднимает WebServer на 80 порту
* отдаёт HTML UI
* принимает команды:

  * `/set?...` (изменение параметров + запись в settings.json)
  * `/restart` (перезапуск аудио)
  * `/status` (JSON статус)

---

### `main.cpp`

Главный файл (оркестратор):

* подключает Wi-Fi
* поднимает WebServer
* инициализирует SD
* загружает `/settings.json`
* запускает аудио

---

## 7) Кто от кого зависит (карта зависимостей)

### main.cpp использует:

* `net_utils` (Wi-Fi + интернет проверка)
* `web_panel` (Web UI)
* `settings` (settings.json)
* `audio_player` (воспроизведение)
* `app_config` (пины/SSID)

---

### web_panel.cpp использует:

* `settings` (менять и сохранять параметры)
* `net_utils` (проверка google)
* `SD` (наличие settings.json)
* callback `audioRestart()` через `RestartAudioFn`

---

### audio_player.cpp использует:

* `settings` (volume, buffers)
* `i2s_audio` (init I2S)
* `wav_reader` (формат WAV)
* `SD + i2s` (чтение/вывод данных)

---

### i2s_audio.cpp использует:

* `settings` (sampleRate, DMA параметры)
* `app_config` (пины)
* драйвер ESP32 I2S

---

### wav_reader.cpp

почти независим:

* просто читает WAV структуру из `File`

---

## 8) Настройки, влияющие на звук

### Скорость / “питч” (высота тона)

Влияет:

* `sampleRate` (I2S sample rate)
* и реальный sampleRate в WAV заголовке

Если они разные — звук будет **быстрее/медленнее** и тон выше/ниже.

---

### Громкость

Влияет:

* `volume` в settings.json
  (применяется как умножение к sample значениям)

---

### Качество звука / хрипы

Чаще всего влияет:

* слишком маленький `SD read buffer` (`inBufBytes`)
* недостаточный DMA буфер (`dmaBufCount`, `dmaBufLen`)
* плохое питание MAX98357A/динамика
* плохой контакт по I2S проводам (особенно BCLK/LRC)

Рекомендации:

* увеличить `inBufBytes` до 4096–8192
* увеличить `dmaBufCount` до 8–12
* увеличить `dmaBufLen` до 256–512

---

## 9) Serial Monitor

Рекомендуемая скорость:

```
115200
```

Именно на этой скорости проект пишет логи.

---

## 10) Требования к файлу /test.wav

Поддерживается:

* PCM
* 16-bit
* Mono или Stereo
* обычно 44100 Hz

Путь:

```
/test.wav
```

---

## 11) Быстрый чек-лист “если не играет”

✅ SD карта доступна?
✅ файл существует `/test.wav`?
✅ WAV формат: PCM 16-bit?
✅ проводка MAX98357A:

* DIN -> GPIO21
* BCLK -> GPIO26
* LRC -> GPIO25
* питание 3.3V / GND

---

## 12) Полезные идеи для улучшений

Если захочешь, можно добавить:

* загрузку текущих значений settings.json прямо в UI при открытии страницы
* кнопку “Stop audio”
* выбор WAV файла на SD через Web UI
* показ текущего статуса аудио PLAYING / STOPPED
* отображение format/channels/sampleRate из WAV

---

"""
