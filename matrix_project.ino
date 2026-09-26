// ============================================================
//  MATRIX PROJECT — ESP32 + Dual P10 LED Matrix + DS3231 RTC
//  MFMCF Edition — DMD32 Library
//  Arduino IDE compatible — ESP32 Core 2.x
//  Modes: Default Clock | Countdown Timer | Scrolling Text
// ============================================================

#include <Wire.h>
#include <SPI.h>
#include <DMD32.h>
#include "fonts/SystemFont5x7.h"
#include "fonts/Arial_Black_16.h"
#include <RTClib.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "web_interface.h"

// ─────────────────────────────────────────
//  TIME DATA STRUCT — must be before any
//  function that uses it
// ─────────────────────────────────────────
struct TimeData {
  int h, m, s, dayOfWeek, day, month, year;
};

// ─────────────────────────────────────────
//  ACCESS POINT CONFIG
// ─────────────────────────────────────────
const char* AP_SSID     = "MATRIX-PROJECT";
const char* AP_PASSWORD = "HOLYGHOST";
IPAddress   AP_IP(192, 168, 4, 1);
IPAddress   AP_GW(192, 168, 4, 1);
IPAddress   AP_SN(255, 255, 255, 0);

#define SET_RTC_TIME false

// ─────────────────────────────────────────
//  P10 DISPLAY CONFIG
// ─────────────────────────────────────────
#define DISPLAYS_WIDE 2
#define DISPLAYS_HIGH 1
#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 16

DMD dmd(DISPLAYS_WIDE, DISPLAYS_HIGH);

// ─────────────────────────────────────────
//  HARDWARE TIMER
// ─────────────────────────────────────────
hw_timer_t* dmdTimer = NULL;

void IRAM_ATTR triggerScan() {
  dmd.scanDisplayBySPI();
}

// ─────────────────────────────────────────
//  HARDWARE OBJECTS
// ─────────────────────────────────────────
RTC_DS3231     rtc;
bool           rtcAvailable = false;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ─────────────────────────────────────────
//  DISPLAY MODES
// ─────────────────────────────────────────
enum DisplayMode {
  MODE_CLOCK,
  MODE_COUNTDOWN,
  MODE_TIMEUP,
  MODE_SCROLL
};

DisplayMode currentMode = MODE_CLOCK;

// ─────────────────────────────────────────
//  CLOCK DISPLAY STYLE
//  0 = Date + Time (small font)
//  1 = Time only (bold, full HH:MM:SS)
// ─────────────────────────────────────────
int  clockStyle       = 0;
bool use12HourFormat  = false; // no AM/PM suffix — just wraps the hour

// ─────────────────────────────────────────
//  COUNTDOWN STATE
// ─────────────────────────────────────────
long          countdownRemaining = 0;
unsigned long lastCountdownTick  = 0;
String        countdownLabel     = "COUNTDOWN";
long          timeUpDuration     = 10000;

int           labelMarqueeX    = DISPLAY_WIDTH - 1;
unsigned long lastLabelStep    = 0;
const long    LABEL_STEP_SPEED = 50;

// Heading (label) show/hide cycling.
// While hidden, the countdown switches to the big Arial_Black_16
// font (single-digit hour, e.g. "0:00:49") for long-distance visibility.
int           cdHeadingIntervalSec = 0;    // 0 = heading always shown
int           cdHeadingShowSec     = 3;    // how long heading shows each cycle
unsigned long cdCycleStart         = 0;
bool          cdHeadingVisible     = true;
unsigned long lastHeadingCheck     = 0;

unsigned long timeUpStart    = 0;
bool          blinkState     = false;
unsigned long lastBlink      = 0;
const long    BLINK_INTERVAL = 500;

// ─────────────────────────────────────────
//  SCROLL TEXT STATE
//  Manual frame-by-frame scroll (not DMD32's
//  drawMarquee/stepMarquee — those had a
//  width-related bug that blanked long text).
// ─────────────────────────────────────────
String        scrollText        = "";
long          scrollDuration    = 0;
long          scrollRemaining   = 0;
unsigned long lastScrollTick    = 0;
int           scrollFont        = 0;
long          scrollSpeed       = 30;
unsigned long lastScrollStep    = 0;
int           scrollX           = 0;
int           scrollTextWidthPx = 0;
char          scrollBuf[1024];

// ─────────────────────────────────────────
//  CLOCK TRACKER
// ─────────────────────────────────────────
unsigned long lastClockUpdate = 0;
unsigned long softClockBase   = 0;

// ─────────────────────────────────────────
//  LIVE MIRROR BROADCAST
//  Pushes the ESP32's actual computed display
//  content to the web page periodically, so the
//  page renders real hardware state instead of
//  approximating with its own independent timer.
// ─────────────────────────────────────────
unsigned long lastLiveBroadcast = 0;
const unsigned long LIVE_BROADCAST_INTERVAL = 700; // ms

// ─────────────────────────────────────────
//  DIAGNOSTICS — free heap log
//  Helps spot a slow memory leak if the AP
//  ever destabilizes again after running a while.
// ─────────────────────────────────────────
unsigned long lastHeapLog = 0;

// ─────────────────────────────────────────
//  LOOKUP TABLES
// ─────────────────────────────────────────
const char* DAYS[]   = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
const char* MONTHS[] = {"JAN","FEB","MAR","APR","MAY","JUN",
                         "JUL","AUG","SEP","OCT","NOV","DEC"};

// ─────────────────────────────────────────
//  WEBSOCKET MULTI-FRAME REASSEMBLY BUFFER
// ─────────────────────────────────────────
String wsAssemblyBuffer = "";
const size_t WS_MAX_MESSAGE_LEN = 4000; // safety cap

// ─────────────────────────────────────────
//  HELPERS
// ─────────────────────────────────────────
String pad2(int n) {
  return (n < 10 ? "0" : "") + String(n);
}

String secsToHMS(long secs) {
  int h = secs / 3600;
  int m = (secs % 3600) / 60;
  int s = secs % 60;
  return pad2(h) + ":" + pad2(m) + ":" + pad2(s);
}

void toCharArr(const String& s, char* buf, int maxLen) {
  s.toCharArray(buf, maxLen);
}

int strPixelWidth(const String& s) {
  return s.length() * 6;
}

// Draw string centered on display at row y using current font
void drawCentered(const String& text, int y) {
  char buf[128];
  toCharArr(text, buf, 128);
  int w = strPixelWidth(text);
  int x = (DISPLAY_WIDTH - w) / 2;
  if (x < 0) x = 0;
  dmd.drawString(x, y, buf, text.length(), GRAPHICS_NORMAL);
}

// Bold via VERTICAL double-draw (y+1, OR) — safe on this hardware.
// (Horizontal x-shift bold corrupted the leftmost digit previously —
// this panel packs columns into bytes, so an x-shift can cross a byte
// boundary mid-character; a y-shift never does.)
void drawBoldAt(const String& text, int x, int y) {
  char buf[128];
  toCharArr(text, buf, 128);
  dmd.drawString(x, y,     buf, text.length(), GRAPHICS_NORMAL);
  dmd.drawString(x, y + 1, buf, text.length(), GRAPHICS_OR);
}

void drawBoldCentered(const String& text, int y) {
  int w = strPixelWidth(text);
  int x = (DISPLAY_WIDTH - w) / 2;
  if (x < 0) x = 0;
  drawBoldAt(text, x, y);
}

// ─────────────────────────────────────────
//  GET CURRENT TIME
// ─────────────────────────────────────────
TimeData getCurrentTime() {
  TimeData t;
  if (rtcAvailable) {
    DateTime now = rtc.now();
    t.h         = now.hour();
    t.m         = now.minute();
    t.s         = now.second();
    t.dayOfWeek = now.dayOfTheWeek();
    t.day       = now.day();
    t.month     = now.month();
    t.year      = now.year();
  } else {
    unsigned long elapsed = (millis() - softClockBase) / 1000;
    t.h         = (elapsed / 3600) % 24;
    t.m         = (elapsed % 3600) / 60;
    t.s         = elapsed % 60;
    t.dayOfWeek = 0;
    t.day       = 1;
    t.month     = 1;
    t.year      = 2026;
  }
  return t;
}

// HH:MM:SS, 24hr; or wrapped-hour HH:MM:SS in 12hr mode (no AM/PM)
String formatClockTime(const TimeData& t) {
  int h = t.h;
  if (use12HourFormat) {
    h = t.h % 12;
    if (h == 0) h = 12;
  }
  return pad2(h) + ":" + pad2(t.m) + ":" + pad2(t.s);
}

// Rough per-character width for Arial_Black_16 — a flat 10px/char
// estimate overestimated total width (colons are much narrower than
// digits), which pushed the centering math negative and clamped the
// text flush to the left edge.
int bigFontCharWidth(char c) {
  return (c == ':') ? 4 : 10;
}

int bigFontStringWidth(const String& s) {
  int total = 0;
  for (unsigned int i = 0; i < s.length(); i++) {
    total += bigFontCharWidth(s[i]);
  }
  return total;
}

// The big countdown format is ALWAYS exactly 7 characters in the
// same digit/colon pattern — "H:MM:SS" — regardless of the actual
// digits shown (e.g. "0:00:00" and "9:59:59" are the same width).
// So the x-position is computed ONCE from that fixed reference
// instead of every frame — this also stops any side-to-side jitter
// as digits change. If it's still not perfectly centered on your
// panel, tell me which side has more space and I'll adjust the
// digit/colon width estimates above (bigFontCharWidth).
const int BIG_COUNTDOWN_WIDTH_PX = 5 * 10 + 2 * 4; // 5 digits + 2 colons = 58px
const int BIG_COUNTDOWN_X = (DISPLAY_WIDTH - BIG_COUNTDOWN_WIDTH_PX) / 2;

// Big-font countdown format — always shows a single-digit hour
// (even when it's 0), so it fits the panel width at Arial_Black_16
// size while staying consistent, e.g. "0:10:00" or "2:05:30"
String formatBigCountdown(long secs) {
  int h = secs / 3600;
  int m = (secs % 3600) / 60;
  int s = secs % 60;
  if (h > 9) h = 9; // clamp — this font can't fit a 2-digit hour
  return String(h) + ":" + pad2(m) + ":" + pad2(s);
}

// ─────────────────────────────────────────
//  COUNTDOWN HEADING CYCLE
// ─────────────────────────────────────────
bool computeShowHeading() {
  if (cdHeadingIntervalSec <= 0) return true; // always show
  unsigned long cycleLen = (unsigned long)(cdHeadingIntervalSec + cdHeadingShowSec) * 1000UL;
  if (cycleLen == 0) return true;
  unsigned long elapsed = (millis() - cdCycleStart) % cycleLen;
  return elapsed < (unsigned long)cdHeadingShowSec * 1000UL;
}

// ─────────────────────────────────────────
//  LIVE MIRROR CONTENT
//  Computes exactly what's currently meant to
//  be on the panel, so the web page can render
//  the same thing instead of guessing.
// ─────────────────────────────────────────
void getLiveLines(String &line0, String &line1, bool &bigMode) {
  bigMode = false;
  line0 = "";
  line1 = "";

  if (currentMode == MODE_CLOCK) {
    TimeData t = getCurrentTime();
    String timeStr = formatClockTime(t);
    if (clockStyle == 1) {
      line0 = timeStr;
    } else {
      line0 = String(DAYS[t.dayOfWeek]) + " " + pad2(t.day) + " " + String(MONTHS[t.month - 1]);
      line1 = timeStr;
    }
  } else if (currentMode == MODE_COUNTDOWN) {
    if (cdHeadingVisible) {
      line0 = countdownLabel;
      line1 = secsToHMS(countdownRemaining);
    } else {
      line0   = formatBigCountdown(countdownRemaining);
      bigMode = true;
    }
  } else if (currentMode == MODE_TIMEUP) {
    line0 = blinkState ? "** TIME UP **" : "";
    line1 = line0;
  } else if (currentMode == MODE_SCROLL) {
    // Exact scroll-position sync isn't practical over the small
    // periodic broadcast — this just shows the full message.
    line0 = scrollText;
  }
}

// ─────────────────────────────────────────
//  SEND STATUS / LIVE MIRROR TO PHONE
//  Called both on state-changing events and
//  periodically (see LIVE_BROADCAST_INTERVAL).
// ─────────────────────────────────────────
void sendStatus() {
  JsonDocument doc;
  doc["clockStyle"] = clockStyle;
  doc["use12h"]     = use12HourFormat;

  String line0, line1;
  bool bigMode;
  getLiveLines(line0, line1, bigMode);
  doc["line0"]   = line0;
  doc["line1"]   = line1;
  doc["bigMode"] = bigMode;

  if (currentMode == MODE_CLOCK) {
    doc["mode"] = "clock";
  } else if (currentMode == MODE_COUNTDOWN) {
    doc["mode"]           = "countdown";
    doc["showingHeading"] = cdHeadingVisible;
  } else if (currentMode == MODE_TIMEUP) {
    doc["mode"] = "timeup";
  } else if (currentMode == MODE_SCROLL) {
    doc["mode"] = "scroll";
  }
  String out;
  serializeJson(doc, out);
  ws.textAll(out);
}

// ─────────────────────────────────────────
//  DISPLAY FUNCTIONS
// ─────────────────────────────────────────

void displayClock() {
  TimeData t = getCurrentTime();
  dmd.clearScreen(true);
  dmd.selectFont(SystemFont5x7);

  String timeStr = formatClockTime(t);

  if (clockStyle == 1) {
    // ── Time Only — full HH:MM:SS, bold, vertically centered ──
    drawBoldCentered(timeStr, 4);
  } else {
    // ── Date + Time — small font both rows, time row bold ──
    String dateStr = String(DAYS[t.dayOfWeek]) + " " +
                     pad2(t.day) + " " +
                     String(MONTHS[t.month - 1]);
    drawCentered(dateStr, 0);
    drawCentered(timeStr, 8); // plain, not bold — paired with date text above
  }
}

void displayCountdown() {
  dmd.clearScreen(true);

  if (!cdHeadingVisible) {
    // ── Big mode — Arial_Black_16 fills the full panel height ──
    dmd.selectFont(Arial_Black_16);
    String bigTime = formatBigCountdown(countdownRemaining);
    int x = BIG_COUNTDOWN_X;
    if (x < 0) x = 0;
    char buf[16];
    toCharArr(bigTime, buf, 16);
    dmd.drawString(x, 0, buf, bigTime.length(), GRAPHICS_NORMAL);
    return;
  }

  // ── Heading visible — small plain time (not bold — paired with
  //    the label above) + label row ──
  dmd.selectFont(SystemFont5x7);
  String timeRow = secsToHMS(countdownRemaining);
  char timeBuf[12];
  toCharArr(timeRow, timeBuf, 12);
  int timePxW = strPixelWidth(timeRow);
  int timeX   = (DISPLAY_WIDTH - timePxW) / 2;
  if (timeX < 0) timeX = 0;
  dmd.drawString(timeX, 8, timeBuf, timeRow.length(), GRAPHICS_NORMAL);

  char labelBuf[128];
  toCharArr(countdownLabel, labelBuf, 128);
  int labelW = strPixelWidth(countdownLabel);
  if (labelW <= DISPLAY_WIDTH) {
    int x = (DISPLAY_WIDTH - labelW) / 2;
    dmd.drawString(x, 0, labelBuf, countdownLabel.length(), GRAPHICS_NORMAL);
  } else {
    dmd.drawString(labelMarqueeX, 0, labelBuf,
                   countdownLabel.length(), GRAPHICS_NORMAL);
  }
}

void displayTimeUp(bool show) {
  dmd.clearScreen(true);
  if (show) {
    dmd.selectFont(SystemFont5x7);
    const char* msg = "** TIME UP **";
    int len = strlen(msg);
    int w   = len * 6;
    int x   = (DISPLAY_WIDTH - w) / 2;
    dmd.drawString(x, 0, msg, len, GRAPHICS_NORMAL);
    dmd.drawString(x, 8, msg, len, GRAPHICS_NORMAL);
  }
}

// ─────────────────────────────────────────
//  SCROLL — manual, right-to-left
//  Small font is vertically centered instead
//  of sitting flush against the top row.
// ─────────────────────────────────────────
void startScrollDisplay() {
  dmd.clearScreen(true);
  dmd.selectFont(scrollFont == 1 ? Arial_Black_16 : SystemFont5x7);
  scrollTextWidthPx = scrollFont == 1
                      ? (int)scrollText.length() * 10
                      : strPixelWidth(scrollText);
  scrollX        = DISPLAY_WIDTH;   // start fully off the RIGHT edge
  lastScrollStep = millis();
}

void stepScrollDisplay() {
  dmd.clearScreen(true);
  dmd.selectFont(scrollFont == 1 ? Arial_Black_16 : SystemFont5x7);
  toCharArr(scrollText, scrollBuf, sizeof(scrollBuf));
  int drawLen = scrollText.length();
  if (drawLen > (int)sizeof(scrollBuf) - 1) drawLen = sizeof(scrollBuf) - 1;

  // Arial_Black_16 (16px tall) fills the full panel — y=0.
  // SystemFont5x7 (7px tall) is centered vertically instead of
  // sitting at the top — (16-7)/2 = 4.
  int y = (scrollFont == 1) ? 0 : 4;
  dmd.drawString(scrollX, y, scrollBuf, drawLen, GRAPHICS_NORMAL);

  scrollX--; // move LEFT each step — right-to-left scroll
  if (scrollX < -scrollTextWidthPx) {
    scrollX = DISPLAY_WIDTH; // loop back to the right edge
  }
}

// ─────────────────────────────────────────
//  WEBSOCKET COMMAND HANDLER
// ─────────────────────────────────────────
void processWsCommand(const String& msg) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, msg);
  if (err) {
    Serial.print("JSON parse failed: ");
    Serial.println(err.c_str());
    return;
  }

  String cmd = doc["cmd"].as<String>();

  // ── Set Clock Style ──────────────────
  if (cmd == "set_clock_style") {
    clockStyle = doc["style"].as<int>();
    Serial.print("Clock style set to: "); Serial.println(clockStyle);
    if (currentMode == MODE_CLOCK) {
      dmd.clearScreen(true);
      lastClockUpdate = 0;
    }
    sendStatus();
  }

  // ── Set 12h/24h Format ───────────────
  else if (cmd == "set_time_format") {
    use12HourFormat = doc["format12h"].as<bool>();
    Serial.print("Time format: "); Serial.println(use12HourFormat ? "12h" : "24h");
    if (currentMode == MODE_CLOCK) {
      dmd.clearScreen(true);
      lastClockUpdate = 0;
    }
    sendStatus();
  }

  // ── Set Date & Time ──────────────────
  else if (cmd == "set_time") {
    int yr = doc["year"].as<int>();
    int mo = doc["month"].as<int>();
    int dy = doc["day"].as<int>();
    int hr = doc["hour"].as<int>();
    int mn = doc["min"].as<int>();
    int sc = doc["sec"].as<int>();
    if (rtcAvailable) {
      rtc.adjust(DateTime(yr, mo, dy, hr, mn, sc));
      Serial.println("RTC updated.");
    } else {
      softClockBase = millis() -
        ((long)hr * 3600 + (long)mn * 60 + sc) * 1000;
      Serial.println("Software clock updated.");
    }
    sendStatus();
  }

  // ── Start Countdown ──────────────────
  else if (cmd == "countdown_start") {
    String hms = doc["time"].as<String>();
    int h = hms.substring(0, 2).toInt();
    int m = hms.substring(3, 5).toInt();
    int s = hms.substring(6, 8).toInt();
    countdownRemaining = (long)h * 3600 + (long)m * 60 + s;
    lastCountdownTick  = millis();
    labelMarqueeX      = DISPLAY_WIDTH - 1;
    lastLabelStep      = millis();
    String lbl = doc["label"].as<String>();
    countdownLabel = (lbl.length() > 0) ? lbl : "COUNTDOWN";
    countdownLabel.toUpperCase();
    int tuSec      = doc["timeup_sec"].as<int>();
    timeUpDuration = (tuSec > 0) ? (long)tuSec * 1000 : 10000;

    cdHeadingIntervalSec = doc["heading_interval"].as<int>();
    cdHeadingShowSec     = doc["heading_show"].as<int>();
    if (cdHeadingShowSec <= 0) cdHeadingShowSec = 3;
    cdCycleStart     = millis();
    cdHeadingVisible = true;
    lastHeadingCheck = millis();

    currentMode = MODE_COUNTDOWN;
    dmd.clearScreen(true);
    sendStatus();
  }

  // ── Cancel Countdown ─────────────────
  else if (cmd == "countdown_cancel") {
    currentMode = MODE_CLOCK;
    dmd.clearScreen(true);
    lastClockUpdate = 0;
    sendStatus();
  }

  // ── Start Scroll Text ────────────────
  else if (cmd == "scroll_start") {
    scrollText      = doc["text"].as<String>();
    scrollDuration  = doc["duration"].as<long>();
    scrollRemaining = scrollDuration;
    scrollSpeed     = doc["speed"].as<long>();
    scrollFont      = doc["font"].as<int>();
    if (scrollSpeed <= 0) scrollSpeed = 30;
    lastScrollTick  = millis();
    currentMode     = MODE_SCROLL;
    startScrollDisplay();
    sendStatus();
  }

  // ── Cancel Scroll ────────────────────
  else if (cmd == "scroll_cancel") {
    currentMode = MODE_CLOCK;
    dmd.clearScreen(true);
    lastClockUpdate = 0;
    sendStatus();
  }
}

// ─────────────────────────────────────────
//  WEBSOCKET FRAME HANDLER
//  Reassembles multi-frame text messages.
// ─────────────────────────────────────────
void handleWebSocketMessage(void* arg, uint8_t* data, size_t len) {
  AwsFrameInfo* info = (AwsFrameInfo*)arg;

  if (info->opcode != WS_TEXT) return;

  if (info->index == 0) {
    wsAssemblyBuffer = "";
  }

  for (size_t i = 0; i < len; i++) {
    wsAssemblyBuffer += (char)data[i];
  }

  if (wsAssemblyBuffer.length() > WS_MAX_MESSAGE_LEN) {
    Serial.println("WS message exceeded safety cap — discarding.");
    wsAssemblyBuffer = "";
    return;
  }

  if (!info->final) return;

  String completeMsg = wsAssemblyBuffer;
  wsAssemblyBuffer = "";
  processWsCommand(completeMsg);
}

// ─────────────────────────────────────────
//  WEBSOCKET EVENT HANDLER
// ─────────────────────────────────────────
void onWsEvent(AsyncWebSocket* server,
               AsyncWebSocketClient* client,
               AwsEventType type, void* arg,
               uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    sendStatus();
  } else if (type == WS_EVT_DATA) {
    handleWebSocketMessage(arg, data, len);
  } else if (type == WS_EVT_DISCONNECT) {
    wsAssemblyBuffer = "";
  }
}

// ─────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("MATRIX PROJECT booting...");

  Wire.begin(32, 33);

  uint8_t cpuClock = ESP.getCpuFreqMHz();
  dmdTimer = timerBegin(0, cpuClock, true);
  timerAttachInterrupt(dmdTimer, &triggerScan, true);
  timerAlarmWrite(dmdTimer, 300, true);
  timerAlarmEnable(dmdTimer);
  Serial.println("Timer OK.");

  dmd.clearScreen(true);
  dmd.selectFont(SystemFont5x7);
  dmd.drawString(17, 0, "MFMCF",   5, GRAPHICS_NORMAL);
  dmd.drawString(11, 8, "DISPLAY", 7, GRAPHICS_NORMAL);
  delay(2000);
  dmd.clearScreen(true);

  Serial.println("Initialising RTC on GPIO 32/33...");
  if (!rtc.begin()) {
    Serial.println("WARNING: DS3231 not found. Using software clock.");
    rtcAvailable  = false;
    softClockBase = millis();
    dmd.drawString(14, 0, "NO RTC", 6, GRAPHICS_NORMAL);
    dmd.drawString(14, 8, "SW CLK", 6, GRAPHICS_NORMAL);
    delay(2000);
    dmd.clearScreen(true);
  } else {
    rtcAvailable = true;
    Serial.println("RTC found.");
    #if SET_RTC_TIME
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      Serial.println("RTC time set.");
    #endif
    if (rtc.lostPower()) {
      Serial.println("RTC lost power — set time via web interface.");
      dmd.drawString(8, 0, "SET TIME", 8, GRAPHICS_NORMAL);
      dmd.drawString(8, 8, "VIA APP!", 8, GRAPHICS_NORMAL);
      delay(2000);
      dmd.clearScreen(true);
    }
  }

  timerAlarmDisable(dmdTimer);
  delay(500);

  Serial.println("Starting Access Point...");
  WiFi.mode(WIFI_AP);
  // Disable modem sleep — on ESP32, AP-mode power-saving can make
  // the beacon go irregular/invisible after running a while, which
  // looks exactly like "the hotspot disappeared".
  WiFi.setSleep(false);
  delay(200);
  WiFi.softAPConfig(AP_IP, AP_GW, AP_SN);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  delay(500);

  Serial.println("Access Point started.");
  Serial.print("SSID:     "); Serial.println(AP_SSID);
  Serial.print("Password: "); Serial.println(AP_PASSWORD);
  Serial.print("IP:       "); Serial.println(WiFi.softAPIP());

  timerAlarmEnable(dmdTimer);
  delay(200);

  dmd.clearScreen(true);
  dmd.selectFont(SystemFont5x7);
  dmd.drawString(8, 0, "STARTING", 8, GRAPHICS_NORMAL);
  dmd.drawString(8, 8, "HOTSPOT.", 8, GRAPHICS_NORMAL);
  delay(1000);

  // Boot scroll — bold Arial_Black_16, right-to-left
  timerAlarmDisable(dmdTimer);
  delay(100);
  dmd.clearScreen(true);
  dmd.selectFont(Arial_Black_16);
  timerAlarmEnable(dmdTimer);

  const char* bootMsg =
    "  MOUNTAIN OF FIRE AND MIRACLES CAMPUS FELLOWSHIP  ";
  int bootLen = strlen(bootMsg);
  dmd.drawMarquee(bootMsg, bootLen, DISPLAY_WIDTH - 1, 0);

  unsigned long bootTimer = millis();
  bool bootDone = false;
  while (!bootDone) {
    if (millis() - bootTimer >= 50) {
      bootTimer = millis();
      bootDone  = dmd.stepMarquee(-1, 0);
    }
  }

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", INDEX_HTML);
  });

  server.begin();
  Serial.println("Web server started. Open 192.168.4.1 in browser.");

  dmd.clearScreen(true);
  currentMode     = MODE_CLOCK;
  lastClockUpdate = 0;
  Serial.println("Boot complete.");
}

// ─────────────────────────────────────────
//  MAIN LOOP
// ─────────────────────────────────────────
void loop() {
  ws.cleanupClients();
  unsigned long now = millis();

  // ── Live mirror broadcast — keeps the web page in sync with
  //    what's actually on the panel, regardless of mode ──
  if (now - lastLiveBroadcast >= LIVE_BROADCAST_INTERVAL) {
    lastLiveBroadcast = now;
    sendStatus();
  }

  // ── Heap diagnostic — watch for a slow leak over time ──
  if (now - lastHeapLog >= 30000) {
    lastHeapLog = now;
    Serial.print("Free heap: "); Serial.println(ESP.getFreeHeap());
  }

  // ── CLOCK ────────────────────────────
  if (currentMode == MODE_CLOCK) {
    if (now - lastClockUpdate >= 1000) {
      lastClockUpdate = now;
      displayClock();
    }
  }

  // ── COUNTDOWN ────────────────────────
  else if (currentMode == MODE_COUNTDOWN) {
    if (now - lastCountdownTick >= 1000) {
      lastCountdownTick = now;
      countdownRemaining--;
      displayCountdown();

      if (countdownRemaining <= 0) {
        currentMode = MODE_TIMEUP;
        timeUpStart = millis();
        blinkState  = true;
        lastBlink   = millis();
        dmd.clearScreen(true);
        displayTimeUp(true);
        sendStatus();
      }
    }

    // Heading show/hide cycle — checked more often than once/sec
    if (now - lastHeadingCheck >= 200) {
      lastHeadingCheck = now;
      bool shouldShow = computeShowHeading();
      if (shouldShow != cdHeadingVisible) {
        cdHeadingVisible = shouldShow;
        displayCountdown();
      }
    }

    // Scroll long label independently (only while heading is visible)
    if (cdHeadingVisible) {
      int labelW = strPixelWidth(countdownLabel);
      if (labelW > DISPLAY_WIDTH) {
        if (now - lastLabelStep >= LABEL_STEP_SPEED) {
          lastLabelStep = now;
          labelMarqueeX--;
          if (labelMarqueeX < -(int)labelW) {
            labelMarqueeX = DISPLAY_WIDTH;
          }
          displayCountdown();
        }
      }
    }
  }

  // ── TIME UP ──────────────────────────
  else if (currentMode == MODE_TIMEUP) {
    if (now - lastBlink >= BLINK_INTERVAL) {
      lastBlink  = now;
      blinkState = !blinkState;
      displayTimeUp(blinkState);
    }
    if (now - timeUpStart >= (unsigned long)timeUpDuration) {
      currentMode = MODE_CLOCK;
      dmd.clearScreen(true);
      lastClockUpdate = 0;
      sendStatus();
    }
  }

  // ── SCROLL TEXT (manual, RIGHT TO LEFT) ──
  else if (currentMode == MODE_SCROLL) {
    if (now - lastScrollStep >= (unsigned long)scrollSpeed) {
      lastScrollStep = now;
      stepScrollDisplay();
    }

    if (now - lastScrollTick >= 1000) {
      lastScrollTick = now;
      scrollRemaining--;
      if (scrollRemaining <= 0) {
        currentMode = MODE_CLOCK;
        dmd.clearScreen(true);
        lastClockUpdate = 0;
        sendStatus();
      }
    }
  }
}



