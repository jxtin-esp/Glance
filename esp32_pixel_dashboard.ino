#include <Arduino.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <Preferences.h>

// ==============================================================================
// DEFAULT WIFI CREDENTIALS (Can also be set dynamically via the Python program!)
// ==============================================================================
#define DEFAULT_WIFI_SSID "YOUR_WIFI_SSID"
#define DEFAULT_WIFI_PASS "YOUR_WIFI_PASSWORD"

// --- DISPLAY & HARDWARE INSTANCES ---
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);
Preferences prefs;

// Calibration data array for ILI9341 rotation 1 (Landscape 320x240)
uint16_t touchCalData[5] = { 275, 3620, 264, 3532, 1 };
volatile bool triggerCalibration = false;

// --- TOUCH & SWIPE STATE ---
#define SWIPE_THRESHOLD 35
int16_t touchStartX = -1;
int16_t touchStartY = -1;
int16_t lastTouchX = -1;
int16_t lastTouchY = -1;
bool isTouching = false;
uint32_t touchStartTime = 0;

// --- SCREEN STATE ---
uint8_t currentScreen = 0; // 0 = Weather, 1 = Calendar, 2 = Countdown
const uint8_t TOTAL_SCREENS = 3;

// --- CUSTOMIZABLE DATA STRUCTS ---
struct ScreenColors {
  uint16_t text;
  uint16_t accent;
  uint16_t cardBg;
};

// Screen 0: Weather
struct WeatherData {
  char city[32];
  char temp[16];
  char condition[24];
  char subText[28];
  uint8_t fontMode; // 0 = Retro Pixel (GLCD), 1 = Clean Modern (Font 2/4)
  ScreenColors colors;
} weather = {
  "Tokyo", "--C", "CONNECTING...", "SYNCING WIFI", 0,
  { 0xFFFF, 0xFDE0, 0x18C6 } // White, Gold accent, Dark Indigo Card
};

// Screen 1: Calendar
struct ScheduledDay {
  char date[12]; // "YYYY-MM-DD"
  char e1[32];
  char e2[32];
  char e3[32];
  bool active;
};
const int MAX_SCHEDULED_DAYS = 24;
ScheduledDay scheduleStore[MAX_SCHEDULED_DAYS];

struct CalendarData {
  char dateHeader[28];
  char event1[32];
  char event2[32];
  char event3[32];
  uint8_t fontMode;
  ScreenColors colors;
} calendarData = {
  "SATURDAY, NOV 21",
  "10:00 AM - Dev Sprint",
  "02:30 PM - Hardware Lab",
  "07:00 PM - Retro Arcade",
  0,
  { 0xFFFF, 0x07E0, 0x1123 } // White, Retro Green, Dark Slate Card
};

// Screen 2: Countdown
struct CountdownData {
  char title[28];
  time_t targetEpoch;       // Real-world target epoch
  uint32_t remainingSecs;   // Live ticking seconds
  uint8_t fontMode;
  ScreenColors colors;
} countdown = {
  "CYBERPUNK 2077",
  0,
  86400 * 5 + 3600 * 14 + 1820,
  0,
  { 0xFFFF, 0xF81F, 0x2086 } // White, Neon Magenta, Deep Violet Card
};

// --- WIFI & WEATHER BACKGROUND FETCH STATE ---
String wifiSSID = DEFAULT_WIFI_SSID;
String wifiPASS = DEFAULT_WIFI_PASS;
volatile bool needWeatherFetch = true;
volatile bool isFetchingWeather = false;
uint32_t lastWeatherFetchMillis = 0;
const uint32_t WEATHER_REFRESH_INTERVAL = 900000; // Auto-refresh every 15 minutes
bool ntpSynced = false;

// --- 3D PLATFORMER & SKY PARALLAX ANIMATION STATE ---
struct PixelCloud {
  float x;
  int y;
  float speed;
  uint8_t width;
};
PixelCloud clouds[3] = {
  { 20.0f,  16, 0.22f, 44 },
  { 160.0f, 34, 0.35f, 56 },
  { 260.0f, 22, 0.18f, 36 }
};

struct PixelStar {
  int x;
  int y;
  uint8_t brightness;
};
const int NUM_STARS = 20;
PixelStar stars[NUM_STARS];

float groundScrollZ = 0.0f;
uint8_t heroAnimFrame = 0;

// --- FORWARD DECLARATIONS ---
void runTouchCalibration();
void updateCalendarFromNTP();
void loadTouchCalibration();
void saveWiFiCredentials(const String& ssid, const String& pass);
void loadWiFiCredentials();
void saveWeatherConfig();
void loadWeatherConfig();
void saveCountdownConfig();
void loadCountdownConfig();
void saveCalendarConfig();
void loadCalendarConfig();
void saveCurrentScreen();
void loadCurrentScreen();

// --- COLOR HELPER: Convert "#RRGGBB" to RGB565 ---
uint16_t hexTo565(const char* hex) {
  if (!hex || strlen(hex) < 6) return 0xFFFF;
  if (hex[0] == '#') hex++;
  uint32_t val = strtoul(hex, NULL, 16);
  uint8_t r = (val >> 16) & 0xFF;
  uint8_t g = (val >> 8) & 0xFF;
  uint8_t b = val & 0xFF;
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// --- INITIALIZE BACKGROUND ELEMENTS ---
void initPlatformerEnvironment() {
  for (int i = 0; i < NUM_STARS; i++) {
    stars[i].x = random(4, 316);
    stars[i].y = random(4, 75);
    stars[i].brightness = random(0, 2);
  }
}

// --- RENDER 3D PLATFORMER SKY & SCROLLING GROUND ---
void drawPlatformer3DWorld() {
  // 1. SKY DUSK GRADIENT (y = 0 to 158)
  canvas.fillRect(0, 0,   320, 38, tft.color565(9, 10, 38));    // Deep Midnight
  canvas.fillRect(0, 38,  320, 38, tft.color565(34, 18, 69));   // Twilight Purple
  canvas.fillRect(0, 76,  320, 42, tft.color565(105, 24, 67));  // Sunset Magenta
  canvas.fillRect(0, 118, 320, 40, tft.color565(196, 89, 67));  // Warm Twilight Peach

  // 2. TWINKLING PIXEL STARS
  for (int i = 0; i < NUM_STARS; i++) {
    uint16_t sc = (stars[i].brightness == 0) ? 0x9CD3 : 0xFFFF;
    canvas.drawPixel(stars[i].x, stars[i].y, sc);
    if ((millis() + i * 200) % 1800 < 150) {
      canvas.drawPixel(stars[i].x + 1, stars[i].y, 0xFFFF);
      canvas.drawPixel(stars[i].x - 1, stars[i].y, 0xFFFF);
      canvas.drawPixel(stars[i].x, stars[i].y + 1, 0xFFFF);
      canvas.drawPixel(stars[i].x, stars[i].y - 1, 0xFFFF);
    }
  }

  // 3. RETRO SUNSET SUN WITH HALO (x = 255, y = 135)
  canvas.fillCircle(255, 135, 22, tft.color565(255, 120, 70));
  canvas.fillCircle(255, 135, 17, tft.color565(255, 190, 60));
  canvas.fillCircle(255, 135, 12, tft.color565(255, 255, 170));

  // 4. PARALLAX DRIFTING PIXEL CLOUDS
  for (int i = 0; i < 3; i++) {
    clouds[i].x -= clouds[i].speed;
    if (clouds[i].x + clouds[i].width < 0) {
      clouds[i].x = 320 + random(10, 50);
      clouds[i].y = random(10, 42);
    }
    int cx = (int)clouds[i].x;
    int cy = clouds[i].y;
    int cw = clouds[i].width;
    canvas.fillRoundRect(cx, cy + 6, cw, 12, 4, tft.color565(180, 130, 180));
    canvas.fillRoundRect(cx + 8, cy, cw - 16, 12, 4, tft.color565(230, 195, 230));
  }

  // 5. DISTANT MOUNTAIN SILHOUETTE (y = 145 to 158)
  uint16_t mountainCol = tft.color565(25, 14, 45);
  for (int x = 0; x < 320; x += 32) {
    int peakY = 145 + (x % 3 == 0 ? 0 : 5);
    canvas.fillTriangle(x - 16, 158, x + 16, peakY, x + 48, 158, mountainCol);
  }

  // 6. PSEUDO-3D PERSPECTIVE SCROLLING GROUND (y = 158 to 240)
  const int horizonY = 158;
  const int groundH = 240 - horizonY; // 82 pixels
  groundScrollZ += 0.035f;
  if (groundScrollZ >= 1.0f) groundScrollZ -= 1.0f;

  const int NUM_DEPTH_SLICES = 9;
  int prevSliceY = horizonY;

  for (int s = 0; s < NUM_DEPTH_SLICES; s++) {
    float depth = (s + groundScrollZ) / (float)NUM_DEPTH_SLICES;
    int currY = horizonY + (int)(depth * depth * groundH);
    if (currY > 240) currY = 240;

    int sliceH = currY - prevSliceY;
    if (sliceH > 0) {
      uint16_t stripeColor = ((s % 2) == 0) ? tft.color565(24, 125, 48) : tft.color565(14, 82, 32);
      canvas.fillRect(0, prevSliceY, 320, sliceH, stripeColor);
      canvas.drawFastHLine(0, currY - 1, 320, tft.color565(40, 175, 75));
    }
    prevSliceY = currY;
  }

  // Radiating 3D Perspective Lines
  const int cx = 160;
  for (int xOffset = -220; xOffset <= 220; xOffset += 28) {
    int topX = cx + (int)(xOffset * 0.12f);
    int bottomX = cx + (int)(xOffset * 1.55f);
    canvas.drawLine(topX, horizonY, bottomX, 240, tft.color565(35, 150, 65));
  }

  // 7. PLATFORMER FLOATING BLOCKS (Bobbing on sine wave)
  float bob = sin(millis() / 250.0f) * 2.5f;
  int blockY = 186 + (int)bob;

  // Retro [?] Coin Block at x = 30
  canvas.fillRect(30, blockY, 16, 16, tft.color565(240, 180, 20));
  canvas.drawRect(30, blockY, 16, 16, 0x0000);
  canvas.drawPixel(32, blockY + 2, 0xFFFF);
  canvas.drawPixel(43, blockY + 2, 0x0000);
  canvas.drawPixel(32, blockY + 13, 0x0000);
  canvas.drawPixel(43, blockY + 13, 0x0000);
  canvas.setTextColor(0x0000, tft.color565(240, 180, 20));
  canvas.setTextFont(1);
  canvas.setTextSize(1);
  canvas.drawString("?", 35, blockY + 4);

  // Retro Brick Block at x = 48
  canvas.fillRect(48, blockY, 16, 16, tft.color565(175, 65, 45));
  canvas.drawRect(48, blockY, 16, 16, 0x0000);
  canvas.drawFastHLine(48, blockY + 8, 16, 0x0000);
  canvas.drawFastVLine(56, blockY, 8, 0x0000);
  canvas.drawFastVLine(52, blockY + 8, 8, 0x0000);
  canvas.drawFastVLine(60, blockY + 8, 8, 0x0000);

  // 8. ANIMATED PIXEL PLATFORMER RUNNING HERO (x = 268, y = 184)
  if ((millis() / 140) % 2 == 0) heroAnimFrame = 0;
  else heroAnimFrame = 1;

  int heroX = 268;
  int heroY = 184;
  canvas.fillRect(heroX + 3, heroY, 9, 3, tft.color565(230, 40, 40));     // Cap
  canvas.fillRect(heroX + 4, heroY + 3, 7, 4, tft.color565(255, 210, 160)); // Face
  canvas.drawPixel(heroX + 9, heroY + 4, 0x0000);                         // Eye
  canvas.fillRect(heroX + 3, heroY + 7, 8, 6, tft.color565(30, 80, 210));   // Overalls

  if (heroAnimFrame == 0) {
    canvas.fillRect(heroX + 1, heroY + 13, 4, 3, tft.color565(120, 60, 20));
    canvas.fillRect(heroX + 8, heroY + 11, 4, 3, tft.color565(120, 60, 20));
  } else {
    canvas.fillRect(heroX + 8, heroY + 13, 4, 3, tft.color565(120, 60, 20));
    canvas.fillRect(heroX + 2, heroY + 11, 4, 3, tft.color565(120, 60, 20));
  }
}

// --- RETRO PIXEL CARD HELPER ---
void drawPixelCard(int x, int y, int w, int h, uint16_t cardBg, uint16_t borderCol) {
  canvas.fillRoundRect(x, y, w, h, 6, cardBg);
  canvas.drawRoundRect(x, y, w, h, 6, borderCol);
  canvas.drawPixel(x + 1, y + 1, borderCol);
  canvas.drawPixel(x + w - 2, y + 1, borderCol);
  canvas.drawPixel(x + 1, y + h - 2, borderCol);
  canvas.drawPixel(x + w - 2, y + h - 2, borderCol);
}

// --- BIG RETRO 8-BIT WEATHER ART ICONS (CENTERED BESIDE TEMPS AT cx, cy) ---
void drawBigWeatherIcon(int cx, int cy, const char* cond, uint16_t accentColor) {
  String c = String(cond);
  c.toUpperCase();

  // 1. SUNNY / CLEAR
  if ((c.indexOf("SUN") >= 0 || c.indexOf("CLEAR") >= 0) && c.indexOf("PART") < 0) {
    // Large Radiant Pixel Sun
    canvas.fillCircle(cx, cy, 15, 0xFC00); // Warm Amber Outer
    canvas.fillCircle(cx, cy, 12, 0xFFE0); // Bright Gold Core
    canvas.fillRect(cx - 5, cy - 5, 4, 4, 0xFFFF); // Highlight

    // Stepped Cardinal Rays
    canvas.fillRect(cx - 3, cy - 28, 6, 8, 0xFFE0); // Top
    canvas.fillRect(cx - 3, cy + 20, 6, 8, 0xFFE0); // Bottom
    canvas.fillRect(cx - 28, cy - 3, 8, 6, 0xFFE0); // Left
    canvas.fillRect(cx + 20, cy - 3, 8, 6, 0xFFE0); // Right

    // Diagonal Stepped Rays
    canvas.fillRect(cx - 20, cy - 20, 5, 5, 0xFFE0);
    canvas.fillRect(cx + 15, cy - 20, 5, 5, 0xFFE0);
    canvas.fillRect(cx - 20, cy + 15, 5, 5, 0xFFE0);
    canvas.fillRect(cx + 15, cy + 15, 5, 5, 0xFFE0);
  }
  // 2. PARTLY CLOUDY (Sun peeking from behind fluffy cloud)
  else if (c.indexOf("PART") >= 0 || c.indexOf("SCATT") >= 0 || c.indexOf("FEW") >= 0 || c.indexOf("INTERVAL") >= 0) {
    // Sun in upper right
    int sx = cx + 12;
    int sy = cy - 12;
    canvas.fillCircle(sx, sy, 13, 0xFC00);
    canvas.fillCircle(sx, sy, 10, 0xFFE0);
    // Sun rays on exposed top & right
    canvas.fillRect(sx - 2, sy - 21, 4, 6, 0xFFE0);
    canvas.fillRect(sx + 15, sy - 2, 6, 4, 0xFFE0);
    canvas.fillRect(sx + 10, sy - 16, 5, 5, 0xFFE0);

    // Front Fluffy Cloud
    int cldX = cx - 14;
    int cldY = cy + 6;
    canvas.fillRoundRect(cldX - 22, cldY - 4, 46, 22, 6, 0x8C71); // Shading Base
    canvas.fillRoundRect(cldX - 22, cldY - 8, 46, 20, 6, 0xD69A); // Mid Cloud
    canvas.fillRoundRect(cldX - 10, cldY - 18, 26, 20, 6, 0xFFFF); // Fluffy Top
    canvas.fillRoundRect(cldX - 18, cldY - 12, 16, 14, 5, 0xFFFF);
  }
  // 3. THUNDERSTORM (Dark Cloud + Bold Jagged Lightning Bolt)
  else if (c.indexOf("STORM") >= 0 || c.indexOf("THUNDER") >= 0 || c.indexOf("LIGHTNING") >= 0) {
    // Dark Storm Cloud
    canvas.fillRoundRect(cx - 28, cy - 14, 56, 24, 7, 0x39E7); // Dark indigo base
    canvas.fillRoundRect(cx - 16, cy - 24, 32, 20, 6, 0x5AEB); // Dark slate top
    canvas.fillRoundRect(cx + 4,  cy - 20, 18, 16, 5, 0x6B4D);

    // Thick Jagged Neon Lightning Bolt (Layered for 8-bit punch)
    int lx = cx;
    int ly = cy - 8;
    // Segment 1 (Down-Left)
    canvas.fillTriangle(lx + 2, ly, lx - 10, ly + 14, lx - 4, ly + 14, 0xFFE0);
    canvas.fillTriangle(lx + 2, ly, lx + 5, ly + 1, lx - 4, ly + 14, 0xFFE0);
    // Shelf
    canvas.fillRect(lx - 12, ly + 12, 12, 4, 0xFFE0);
    // Segment 2 (Down-Left to Point)
    canvas.fillTriangle(lx - 4, ly + 14, lx - 16, ly + 32, lx - 8, ly + 18, 0xFFE0);
    canvas.fillTriangle(lx + 1, ly + 14, lx - 16, ly + 32, lx - 4, ly + 14, 0xFFE0);
    // Hot center core
    canvas.drawLine(lx + 1, ly + 2, lx - 7, ly + 14, 0xFFFF);
    canvas.drawLine(lx - 6, ly + 14, lx - 14, ly + 30, 0xFFFF);
  }
  // 4. RAINY / SHOWER
  else if (c.indexOf("RAIN") >= 0 || c.indexOf("SHOWER") >= 0 || c.indexOf("DRIZZLE") >= 0) {
    // Slate Rain Cloud
    canvas.fillRoundRect(cx - 26, cy - 18, 52, 22, 6, 0x632C);
    canvas.fillRoundRect(cx - 14, cy - 26, 28, 18, 5, 0x8410);

    // 4 Animated angled bright cyan pixel rain streams
    int rPhase = (millis() / 120) % 4;
    for (int i = 0; i < 4; i++) {
      int rx = cx - 20 + i * 13 + rPhase;
      int ry = cy + 8 + ((i * 3 + rPhase * 2) % 6);
      canvas.drawLine(rx, ry, rx - 3, ry + 10, 0x07FF);
      canvas.drawLine(rx + 1, ry, rx - 2, ry + 10, 0x07FF);
      canvas.drawPixel(rx - 3, ry + 11, 0xFFFF); // Splash dot
    }
  }
  // 5. SNOWY / ICE
  else if (c.indexOf("SNOW") >= 0 || c.indexOf("BLIZZARD") >= 0 || c.indexOf("ICE") >= 0) {
    // Soft Blue-Gray Cloud
    canvas.fillRoundRect(cx - 26, cy - 18, 52, 22, 6, 0x8CD3);
    canvas.fillRoundRect(cx - 14, cy - 26, 28, 18, 5, 0xBDF7);

    // 3 Large 8-Bit Pixel Snowflakes
    const int snowOffsets[3][2] = { {-14, 10}, {2, 14}, {18, 9} };
    for (int i = 0; i < 3; i++) {
      int fx = cx + snowOffsets[i][0];
      int fy = cy + snowOffsets[i][1];
      canvas.drawFastHLine(fx - 3, fy, 7, 0xFFFF);
      canvas.drawFastVLine(fx, fy - 3, 7, 0xFFFF);
      canvas.drawPixel(fx - 2, fy - 2, 0xFFFF);
      canvas.drawPixel(fx + 2, fy - 2, 0xFFFF);
      canvas.drawPixel(fx - 2, fy + 2, 0xFFFF);
      canvas.drawPixel(fx + 2, fy + 2, 0xFFFF);
    }
  }
  // 6. DEFAULT CLOUDY / OVERCAST / FOG / MIST
  else {
    // Big Overlapping 8-bit Double Cloud
    // Back Cloud
    canvas.fillRoundRect(cx - 8, cy - 22, 38, 22, 6, 0x73AE);
    canvas.fillRoundRect(cx + 4, cy - 28, 20, 16, 5, 0x8C71);
    // Front Cloud
    canvas.fillRoundRect(cx - 28, cy - 10, 52, 24, 7, 0xBDD7);
    canvas.fillRoundRect(cx - 16, cy - 20, 28, 18, 5, 0xFFFF);
    canvas.fillRoundRect(cx - 24, cy + 4, 44, 8, 3, 0x94B2); // Shading line
  }
}

// --- FLAWLESS TEMPERATURE DRAWING (NO CHECKERED GLYPH BUG) ---
void drawCleanTemperature(int x, int y, const char* tempStr, uint16_t textColor, uint16_t cardBg, uint8_t fontMode) {
  String num = "";
  char unit = 'C';
  bool hasUnit = false;

  for (size_t i = 0; i < strlen(tempStr); i++) {
    char c = tempStr[i];
    if ((c >= '0' && c <= '9') || c == '-' || c == '+') {
      if (c != '+') num += c;
    } else if (c == 'C' || c == 'c') {
      unit = 'C';
      hasUnit = true;
    } else if (c == 'F' || c == 'f') {
      unit = 'F';
      hasUnit = true;
    }
  }

  if (num.length() == 0) num = "--";

  canvas.setTextDatum(TL_DATUM);
  canvas.setTextColor(textColor, cardBg);

  if (fontMode == 0) {
    // Retro 8-bit GLCD (size 4)
    canvas.setTextFont(1);
    canvas.setTextSize(4);
    canvas.drawString(num, x, y);

    int numWidth = canvas.textWidth(num);
    int degX = x + numWidth + 4;
    int degY = y + 3;

    // Draw razor-sharp hollow pixel degree ring (6x6)
    canvas.drawRect(degX, degY, 6, 6, textColor);
    canvas.drawPixel(degX, degY, cardBg);
    canvas.drawPixel(degX + 5, degY, cardBg);
    canvas.drawPixel(degX, degY + 5, cardBg);
    canvas.drawPixel(degX + 5, degY + 5, cardBg);

    // Draw Unit letter ('C' or 'F')
    if (hasUnit) {
      char uStr[2] = { unit, '\0' };
      canvas.drawString(uStr, degX + 11, y);
    }
  } else {
    // Clean Modern Font 4
    canvas.setTextFont(4);
    canvas.setTextSize(1);
    canvas.drawString(num, x, y);

    int numWidth = canvas.textWidth(num);
    int degX = x + numWidth + 3;
    int degY = y + 4;

    canvas.drawCircle(degX + 2, degY + 2, 2, textColor);

    if (hasUnit) {
      char uStr[2] = { unit, '\0' };
      canvas.drawString(uStr, degX + 9, y);
    }
  }
}

// --- RENDER SCREEN 0: LIVE WEATHER (WITH BIG PIXEL ART ICON BESIDE TEMPS) ---
void renderWeatherScreen() {
  drawPlatformer3DWorld();

  // Top Location Header Card (centered vertically at y = 26..58)
  drawPixelCard(10, 26, 300, 32, weather.colors.cardBg, weather.colors.accent);
  canvas.setTextDatum(ML_DATUM);

  if (weather.fontMode == 0) {
    canvas.setTextFont(1);
    canvas.setTextSize(2);
  } else {
    canvas.setTextFont(2);
    canvas.setTextSize(1);
  }
  canvas.setTextColor(weather.colors.text, weather.colors.cardBg);
  canvas.drawString(weather.city, 20, 42);

  // WiFi Status Badge
  canvas.setTextDatum(MR_DATUM);
  canvas.setTextFont(1);
  canvas.setTextSize(1);
  if (isFetchingWeather) {
    canvas.setTextColor(0xFFE0, weather.colors.cardBg);
    canvas.drawString("[FETCHING]", 298, 42);
  } else if (WiFi.status() == WL_CONNECTED) {
    canvas.setTextColor(0x07E0, weather.colors.cardBg);
    canvas.drawString("[WIFI: OK]", 298, 42);
  } else {
    canvas.setTextColor(0xF800, weather.colors.cardBg);
    canvas.drawString("[WIFI: ...]", 298, 42);
  }

  // Main Weather Card (centered vertically at y = 64..180)
  drawPixelCard(10, 64, 300, 116, weather.colors.cardBg, weather.colors.accent);

  // LEFT SIDE: Condition, Big Temperature Readout, Humidity
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextFont(1);
  canvas.setTextSize(2);
  canvas.setTextColor(weather.colors.accent, weather.colors.cardBg);
  canvas.drawString(weather.condition, 20, 74);

  drawCleanTemperature(20, 98, weather.temp, weather.colors.text, weather.colors.cardBg, weather.fontMode);

  canvas.setTextFont(1);
  canvas.setTextSize(1);
  canvas.setTextColor(0xBDF7, weather.colors.cardBg);
  canvas.drawString(weather.subText, 20, 150);

  // RIGHT SIDE: Big Procedural Weather Icon (Fills empty space beside current temps!)
  drawBigWeatherIcon(244, 120, weather.condition, weather.colors.accent);
}

// --- RENDER SCREEN 1: CALENDAR & EVENTS (AUTOMATIC TODAY'S EVENTS) ---
void renderCalendarScreen() {
  drawPlatformer3DWorld();

  // Date Header Card (y = 26..58)
  drawPixelCard(10, 26, 300, 32, calendarData.colors.cardBg, calendarData.colors.accent);
  canvas.setTextDatum(MC_DATUM);
  canvas.setTextFont(1);
  canvas.setTextSize(2);
  canvas.setTextColor(calendarData.colors.accent, calendarData.colors.cardBg);
  canvas.drawString(calendarData.dateHeader, 160, 42);

  // 3 Event Rows (y = 64, 104, 144)
  const char* events[3] = { calendarData.event1, calendarData.event2, calendarData.event3 };
  for (int i = 0; i < 3; i++) {
    int y = 64 + i * 40;
    drawPixelCard(10, y, 300, 34, calendarData.colors.cardBg, calendarData.colors.accent);

    // Retro bullet dot
    canvas.fillRect(20, y + 13, 7, 7, calendarData.colors.accent);

    // Event text
    canvas.setTextDatum(ML_DATUM);
    if (calendarData.fontMode == 0) {
      canvas.setTextFont(1);
      canvas.setTextSize(1);
    } else {
      canvas.setTextFont(2);
      canvas.setTextSize(1);
    }
    canvas.setTextColor(calendarData.colors.text, calendarData.colors.cardBg);
    canvas.drawString(events[i], 36, y + 17);
  }
}

// --- RENDER SCREEN 2: COUNTDOWN TIMER (VERTICALLY CENTERED) ---
void renderCountdownScreen() {
  drawPlatformer3DWorld();

  // Title Card (y = 26..58)
  drawPixelCard(10, 26, 300, 32, countdown.colors.cardBg, countdown.colors.accent);
  canvas.setTextDatum(MC_DATUM);
  canvas.setTextFont(1);
  canvas.setTextSize(2);
  canvas.setTextColor(countdown.colors.accent, countdown.colors.cardBg);
  canvas.drawString(countdown.title, 160, 42);

  // Main Countdown Container (y = 64..180)
  drawPixelCard(10, 64, 300, 116, countdown.colors.cardBg, countdown.colors.accent);

  uint32_t s = countdown.remainingSecs;
  uint16_t days = s / 86400;
  uint8_t hours = (s % 86400) / 3600;
  uint8_t mins = (s % 3600) / 60;
  uint8_t secs = s % 60;

  // 4 Retro Countdown Boxes (DAYS, HRS, MIN, SEC)
  char numBuf[10];
  const char* labels[4] = { "DAYS", "HRS", "MIN", "SEC" };
  uint16_t values[4] = { days, hours, mins, secs };

  for (int i = 0; i < 4; i++) {
    int bx = 20 + i * 69;
    int by = 76;
    canvas.fillRoundRect(bx, by, 63, 68, 4, tft.color565(16, 14, 25));
    canvas.drawRoundRect(bx, by, 63, 68, 4, countdown.colors.accent);

    // Digits
    canvas.setTextDatum(MC_DATUM);
    if (countdown.fontMode == 0) {
      canvas.setTextFont(1);
      canvas.setTextSize(3);
    } else {
      canvas.setTextFont(4);
      canvas.setTextSize(1);
    }
    canvas.setTextColor(countdown.colors.text, tft.color565(16, 14, 25));
    snprintf(numBuf, sizeof(numBuf), "%02d", values[i]);
    canvas.drawString(numBuf, bx + 31, by + 26);

    // Label
    canvas.setTextFont(1);
    canvas.setTextSize(1);
    canvas.setTextColor(countdown.colors.accent, tft.color565(16, 14, 25));
    canvas.drawString(labels[i], bx + 31, by + 53);
  }

  // Live seconds progress ticker
  int progressW = map(secs, 0, 59, 8, 276);
  canvas.fillRect(22, 158, progressW, 3, countdown.colors.accent);
}

// --- RENDER NAVIGATION OVERLAY ---
void renderNavigationFooter() {
  canvas.setTextFont(1);
  canvas.setTextSize(2);
  canvas.setTextDatum(MC_DATUM);

  // Left/Right Arrow touch zones
  canvas.setTextColor(0xFFFF, 0x0000);
  canvas.drawString("<", 16, 222);
  canvas.drawString(">", 304, 222);

  // Pagination Dots
  for (int i = 0; i < TOTAL_SCREENS; i++) {
    int dx = 144 + i * 16;
    if (i == currentScreen) {
      canvas.fillCircle(dx, 222, 4, 0xFFFF);
    } else {
      canvas.drawCircle(dx, 222, 3, 0xCE79);
    }
  }
}

// --- AUTOMATIC TODAY'S EVENTS CHECK FROM REAL-WORLD NTP TIME ---
void updateCalendarFromNTP() {
  time_t nowSecs;
  time(&nowSecs);
  if (nowSecs < 1000000000) return; // Wait until NTP has valid timestamp

  struct tm* t = localtime(&nowSecs);
  char todayKey[12];
  snprintf(todayKey, sizeof(todayKey), "%04d-%02d-%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);

  // Format today's banner e.g. "SATURDAY, NOV 21"
  char bannerBuf[32];
  strftime(bannerBuf, sizeof(bannerBuf), "%A, %b %d", t);
  for (size_t i = 0; i < strlen(bannerBuf); i++) bannerBuf[i] = toupper(bannerBuf[i]);
  strlcpy(calendarData.dateHeader, bannerBuf, sizeof(calendarData.dateHeader));

  // Check if scheduleStore has entries for today
  for (int i = 0; i < MAX_SCHEDULED_DAYS; i++) {
    if (scheduleStore[i].active && strcmp(scheduleStore[i].date, todayKey) == 0) {
      if (strlen(scheduleStore[i].e1) > 0) strlcpy(calendarData.event1, scheduleStore[i].e1, sizeof(calendarData.event1));
      else strlcpy(calendarData.event1, "No events scheduled", sizeof(calendarData.event1));

      strlcpy(calendarData.event2, scheduleStore[i].e2, sizeof(calendarData.event2));
      strlcpy(calendarData.event3, scheduleStore[i].e3, sizeof(calendarData.event3));
      return;
    }
  }
}

// --- INTERACTIVE TOUCH CALIBRATION ROUTINE ---
void runTouchCalibration() {
  Serial.println(F("[Touch] Starting interactive screen calibration..."));

  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.drawString("TOUCH CALIBRATION", 160, 85);

  tft.setTextFont(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Touch the arrows at the 4 corners", 160, 115);
  tft.drawString("as they appear one by one.", 160, 132);

  delay(600);

  // Invoke TFT_eSPI built-in corner calibration
  tft.calibrateTouch(touchCalData, TFT_MAGENTA, TFT_BLACK, 15);

  // Save calibrated parameters to Non-Volatile Flash (Preferences)
  prefs.begin("dashboard", false);
  prefs.putBytes("touchCal", touchCalData, sizeof(touchCalData));
  prefs.end();

  tft.setTouch(touchCalData);

  // Display completion banner
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextFont(2);
  tft.drawString("CALIBRATION COMPLETE!", 160, 105);
  tft.setTextFont(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Touch configuration saved to flash.", 160, 130);

  Serial.printf("[Touch] Calibration Saved: [%d, %d, %d, %d, %d]\n",
    touchCalData[0], touchCalData[1], touchCalData[2], touchCalData[3], touchCalData[4]);

  delay(1400);
}

// --- NON-VOLATILE FLASH (NVS PREFERENCES) PERSISTENCE HELPERS ---
void loadTouchCalibration() {
  prefs.begin("dashboard", false);
  size_t calLen = prefs.getBytesLength("touchCal");
  if (calLen == sizeof(touchCalData)) {
    prefs.getBytes("touchCal", touchCalData, sizeof(touchCalData));
    prefs.end();
    tft.setTouch(touchCalData);
    Serial.printf("[Touch] Loaded calibration from flash: [%d, %d, %d, %d, %d]\n",
      touchCalData[0], touchCalData[1], touchCalData[2], touchCalData[3], touchCalData[4]);
  } else {
    prefs.end();
    Serial.println(F("[Touch] First boot detected! Launching interactive calibration..."));
    runTouchCalibration();
  }
}

void saveWiFiCredentials(const String& ssid, const String& pass) {
  prefs.begin("dashboard", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
  Serial.printf("[NVS] Saved WiFi SSID: %s\n", ssid.c_str());
}

void loadWiFiCredentials() {
  prefs.begin("dashboard", true);
  String savedSSID = prefs.getString("ssid", "");
  String savedPASS = prefs.getString("pass", "");
  prefs.end();
  if (savedSSID.length() > 0) {
    wifiSSID = savedSSID;
    wifiPASS = savedPASS;
    Serial.printf("[NVS] Loaded WiFi SSID: %s\n", wifiSSID.c_str());
  }
}

void saveWeatherConfig() {
  prefs.begin("dashboard", false);
  prefs.putString("w_city", weather.city);
  prefs.putUChar("w_font", weather.fontMode);
  prefs.putUShort("w_ctxt", weather.colors.text);
  prefs.putUShort("w_cacc", weather.colors.accent);
  prefs.putUShort("w_cbg", weather.colors.cardBg);
  prefs.putString("w_temp", weather.temp);
  prefs.putString("w_cond", weather.condition);
  prefs.putString("w_sub", weather.subText);
  prefs.end();
  Serial.printf("[NVS] Saved Weather Config: %s\n", weather.city);
}

void loadWeatherConfig() {
  prefs.begin("dashboard", true);
  String savedCity = prefs.getString("w_city", "");
  if (savedCity.length() > 0) {
    strlcpy(weather.city, savedCity.c_str(), sizeof(weather.city));
    weather.fontMode = prefs.getUChar("w_font", weather.fontMode);
    weather.colors.text = prefs.getUShort("w_ctxt", weather.colors.text);
    weather.colors.accent = prefs.getUShort("w_cacc", weather.colors.accent);
    weather.colors.cardBg = prefs.getUShort("w_cbg", weather.colors.cardBg);

    String savedTemp = prefs.getString("w_temp", "");
    String savedCond = prefs.getString("w_cond", "");
    String savedSub = prefs.getString("w_sub", "");
    if (savedTemp.length() > 0) strlcpy(weather.temp, savedTemp.c_str(), sizeof(weather.temp));
    if (savedCond.length() > 0) strlcpy(weather.condition, savedCond.c_str(), sizeof(weather.condition));
    if (savedSub.length() > 0) strlcpy(weather.subText, savedSub.c_str(), sizeof(weather.subText));
    Serial.printf("[NVS] Loaded Weather: %s (%s, %s)\n", weather.city, weather.temp, weather.condition);
  }
  prefs.end();
}

void saveCountdownConfig() {
  prefs.begin("dashboard", false);
  prefs.putString("cd_title", countdown.title);
  prefs.putBytes("cd_epoch", &countdown.targetEpoch, sizeof(countdown.targetEpoch));
  prefs.putUInt("cd_secs", countdown.remainingSecs);
  prefs.putUChar("cd_font", countdown.fontMode);
  prefs.putUShort("cd_ctxt", countdown.colors.text);
  prefs.putUShort("cd_cacc", countdown.colors.accent);
  prefs.putUShort("cd_cbg", countdown.colors.cardBg);
  prefs.end();
  Serial.printf("[NVS] Saved Countdown: '%s'\n", countdown.title);
}

void loadCountdownConfig() {
  prefs.begin("dashboard", true);
  String savedTitle = prefs.getString("cd_title", "");
  if (savedTitle.length() > 0) {
    strlcpy(countdown.title, savedTitle.c_str(), sizeof(countdown.title));
    if (prefs.getBytesLength("cd_epoch") == sizeof(countdown.targetEpoch)) {
      prefs.getBytes("cd_epoch", &countdown.targetEpoch, sizeof(countdown.targetEpoch));
    }
    countdown.remainingSecs = prefs.getUInt("cd_secs", countdown.remainingSecs);
    countdown.fontMode = prefs.getUChar("cd_font", countdown.fontMode);
    countdown.colors.text = prefs.getUShort("cd_ctxt", countdown.colors.text);
    countdown.colors.accent = prefs.getUShort("cd_cacc", countdown.colors.accent);
    countdown.colors.cardBg = prefs.getUShort("cd_cbg", countdown.colors.cardBg);
    Serial.printf("[NVS] Loaded Countdown: '%s'\n", countdown.title);
  }
  prefs.end();
}

void saveCalendarConfig() {
  prefs.begin("dashboard", false);
  prefs.putString("c_hdr", calendarData.dateHeader);
  prefs.putString("c_e1", calendarData.event1);
  prefs.putString("c_e2", calendarData.event2);
  prefs.putString("c_e3", calendarData.event3);
  prefs.putUChar("c_font", calendarData.fontMode);
  prefs.putUShort("c_ctxt", calendarData.colors.text);
  prefs.putUShort("c_cacc", calendarData.colors.accent);
  prefs.putUShort("c_cbg", calendarData.colors.cardBg);
  prefs.putBytes("c_sched", scheduleStore, sizeof(scheduleStore));
  prefs.end();
  Serial.println(F("[NVS] Saved Calendar Config & Store."));
}

void loadCalendarConfig() {
  prefs.begin("dashboard", true);
  String savedHdr = prefs.getString("c_hdr", "");
  if (savedHdr.length() > 0) {
    strlcpy(calendarData.dateHeader, savedHdr.c_str(), sizeof(calendarData.dateHeader));
    String e1 = prefs.getString("c_e1", "");
    String e2 = prefs.getString("c_e2", "");
    String e3 = prefs.getString("c_e3", "");
    if (e1.length() > 0) strlcpy(calendarData.event1, e1.c_str(), sizeof(calendarData.event1));
    if (e2.length() > 0) strlcpy(calendarData.event2, e2.c_str(), sizeof(calendarData.event2));
    if (e3.length() > 0) strlcpy(calendarData.event3, e3.c_str(), sizeof(calendarData.event3));
    calendarData.fontMode = prefs.getUChar("c_font", calendarData.fontMode);
    calendarData.colors.text = prefs.getUShort("c_ctxt", calendarData.colors.text);
    calendarData.colors.accent = prefs.getUShort("c_cacc", calendarData.colors.accent);
    calendarData.colors.cardBg = prefs.getUShort("c_cbg", calendarData.colors.cardBg);
  }
  if (prefs.getBytesLength("c_sched") == sizeof(scheduleStore)) {
    prefs.getBytes("c_sched", scheduleStore, sizeof(scheduleStore));
    Serial.println(F("[NVS] Loaded Calendar Store."));
  }
  prefs.end();
}

void saveCurrentScreen() {
  prefs.begin("dashboard", false);
  prefs.putUChar("cur_scr", currentScreen);
  prefs.end();
}

void loadCurrentScreen() {
  prefs.begin("dashboard", true);
  currentScreen = prefs.getUChar("cur_scr", 0) % TOTAL_SCREENS;
  prefs.end();
}

// --- HTTP LIVE WEATHER FETCH (wttr.in format=%t|%C|%h) ---
void executeWeatherFetch() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("[Weather] WiFi not connected. Skipping fetch."));
    return;
  }

  isFetchingWeather = true;
  Serial.printf("[Weather] Fetching live weather for: %s\n", weather.city);

  String encodedCity = "";
  for (size_t i = 0; i < strlen(weather.city); i++) {
    char ch = weather.city[i];
    if (ch == ' ') encodedCity += "%20";
    else encodedCity += ch;
  }

  HTTPClient http;
  String url = "http://wttr.in/" + encodedCity + "?format=%t|%C|%h";
  http.begin(url);
  http.setTimeout(5000);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    payload.trim();
    Serial.printf("[Weather] Response: %s\n", payload.c_str());

    int p1 = payload.indexOf('|');
    int p2 = payload.indexOf('|', p1 + 1);

    if (p1 > 0 && p2 > p1) {
      String t = payload.substring(0, p1);
      String c = payload.substring(p1 + 1, p2);
      String h = payload.substring(p2 + 1);

      t.trim();
      c.trim();
      h.trim();

      if (t.startsWith("+")) t = t.substring(1);

      strlcpy(weather.temp, t.c_str(), sizeof(weather.temp));
      strlcpy(weather.condition, c.c_str(), sizeof(weather.condition));
      snprintf(weather.subText, sizeof(weather.subText), "HUMIDITY %s", h.c_str());
      Serial.println(F("[Weather] Updated successfully!"));
      saveWeatherConfig();
    }
  } else {
    Serial.printf("[Weather] HTTP failed with code: %d\n", httpCode);
  }
  http.end();
  isFetchingWeather = false;
  lastWeatherFetchMillis = millis();
}

// --- FREERTOS BACKGROUND TASK PINNED TO CORE 0 ---
void backgroundNetworkTask(void* pvParameters) {
  WiFi.mode(WIFI_STA);
  bool hasCreds = (wifiSSID.length() > 0 && wifiSSID != DEFAULT_WIFI_SSID);
  if (hasCreds) {
    WiFi.begin(wifiSSID.c_str(), wifiPASS.c_str());
    Serial.printf("[Network] Connecting to WiFi SSID: %s\n", wifiSSID.c_str());
  } else {
    Serial.println(F("[Network] No WiFi credentials configured yet. Waiting for configuration..."));
  }

  int wifiRetries = 0;
  while (WiFi.status() != WL_CONNECTED && wifiRetries < 25 && hasCreds) {
    vTaskDelay(pdMS_TO_TICKS(500));
    wifiRetries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[Network] WiFi Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    ntpSynced = true;
    updateCalendarFromNTP();
    executeWeatherFetch();
  } else if (hasCreds) {
    Serial.println(F("[Network] WiFi connection timed out. Check credentials."));
  }

  for (;;) {
    if (needWeatherFetch && WiFi.status() == WL_CONNECTED) {
      needWeatherFetch = false;
      executeWeatherFetch();
    } else if (millis() - lastWeatherFetchMillis > WEATHER_REFRESH_INTERVAL && WiFi.status() == WL_CONNECTED) {
      executeWeatherFetch();
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// --- PARSE JSON OVER SERIAL FROM PYTHON ---
void parseSerialCommand() {
  if (!Serial.available()) return;

  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, line);
  if (err) {
    Serial.print(F("JSON Error: "));
    Serial.println(err.c_str());
    return;
  }

  const char* type = doc["type"] | "";

  // 1. Trigger Screen Touch Calibration
  if (strcmp(type, "calibrate_touch") == 0) {
    triggerCalibration = true;
    Serial.println(F("OK: Touch Calibration Triggered."));
  }
  // 2. WiFi Credentials Configuration
  else if (strcmp(type, "wifi") == 0) {
    if (doc["ssid"].is<const char*>() && doc["pass"].is<const char*>()) {
      wifiSSID = doc["ssid"].as<String>();
      wifiPASS = doc["pass"].as<String>();
      saveWiFiCredentials(wifiSSID, wifiPASS);
      Serial.println(F("OK: WiFi Credentials Saved. Reconnecting..."));
      WiFi.disconnect();
      WiFi.begin(wifiSSID.c_str(), wifiPASS.c_str());
      needWeatherFetch = true;
    }
  }
  // 3. Weather City & Appearance Update
  else if (strcmp(type, "weather_city") == 0) {
    if (doc["city"].is<const char*>()) {
      strlcpy(weather.city, doc["city"], sizeof(weather.city));
      needWeatherFetch = true;
    }
    if (doc["font"].is<uint8_t>())     weather.fontMode = doc["font"].as<uint8_t>();
    if (doc["txt"].is<const char*>())  weather.colors.text = hexTo565(doc["txt"]);
    if (doc["acc"].is<const char*>())  weather.colors.accent = hexTo565(doc["acc"]);
    if (doc["card"].is<const char*>()) weather.colors.cardBg = hexTo565(doc["card"]);
    saveWeatherConfig();
    Serial.println(F("OK: Weather City Updated & Saved to Flash. Background Fetch Triggered."));
  }
  // 4. Bulk Calendar Schedule Sync from Python Grid
  else if (strcmp(type, "calendar_sync") == 0) {
    if (doc["events"].is<JsonArray>()) {
      JsonArray arr = doc["events"].as<JsonArray>();
      int idx = 0;
      for (JsonObject obj : arr) {
        if (idx >= MAX_SCHEDULED_DAYS) break;
        if (obj["date"].is<const char*>()) {
          strlcpy(scheduleStore[idx].date, obj["date"], sizeof(scheduleStore[idx].date));
          strlcpy(scheduleStore[idx].e1, obj["e1"] | "", sizeof(scheduleStore[idx].e1));
          strlcpy(scheduleStore[idx].e2, obj["e2"] | "", sizeof(scheduleStore[idx].e2));
          strlcpy(scheduleStore[idx].e3, obj["e3"] | "", sizeof(scheduleStore[idx].e3));
          scheduleStore[idx].active = true;
          idx++;
        }
      }
      for (int i = idx; i < MAX_SCHEDULED_DAYS; i++) {
        scheduleStore[i].active = false;
      }
    }
    if (doc["font"].is<uint8_t>())     calendarData.fontMode = doc["font"].as<uint8_t>();
    if (doc["txt"].is<const char*>())  calendarData.colors.text = hexTo565(doc["txt"]);
    if (doc["acc"].is<const char*>())  calendarData.colors.accent = hexTo565(doc["acc"]);
    if (doc["card"].is<const char*>()) calendarData.colors.cardBg = hexTo565(doc["card"]);

    // Update today's events from store right away
    updateCalendarFromNTP();
    saveCalendarConfig();
    Serial.println(F("OK: Calendar Store Synchronized & Saved to Flash."));
  }
  // 5. Direct Calendar Single-Day Display Update
  else if (strcmp(type, "calendar") == 0) {
    if (doc["header"].is<const char*>()) strlcpy(calendarData.dateHeader, doc["header"], sizeof(calendarData.dateHeader));
    if (doc["e1"].is<const char*>())     strlcpy(calendarData.event1, doc["e1"], sizeof(calendarData.event1));
    if (doc["e2"].is<const char*>())     strlcpy(calendarData.event2, doc["e2"], sizeof(calendarData.event2));
    if (doc["e3"].is<const char*>())     strlcpy(calendarData.event3, doc["e3"], sizeof(calendarData.event3));
    if (doc["font"].is<uint8_t>())       calendarData.fontMode = doc["font"].as<uint8_t>();
    if (doc["txt"].is<const char*>())    calendarData.colors.text = hexTo565(doc["txt"]);
    if (doc["acc"].is<const char*>())    calendarData.colors.accent = hexTo565(doc["acc"]);
    if (doc["card"].is<const char*>())   calendarData.colors.cardBg = hexTo565(doc["card"]);
    saveCalendarConfig();
    Serial.println(F("OK: Live Calendar Updated & Saved to Flash."));
  }
  // 6. Countdown Update (with Target Epoch)
  else if (strcmp(type, "countdown") == 0) {
    if (doc["title"].is<const char*>())        strlcpy(countdown.title, doc["title"], sizeof(countdown.title));
    if (doc["target_epoch"].is<time_t>())      countdown.targetEpoch = doc["target_epoch"].as<time_t>();
    if (doc["seconds"].is<uint32_t>())         countdown.remainingSecs = doc["seconds"].as<uint32_t>();
    if (doc["font"].is<uint8_t>())             countdown.fontMode = doc["font"].as<uint8_t>();
    if (doc["txt"].is<const char*>())          countdown.colors.text = hexTo565(doc["txt"]);
    if (doc["acc"].is<const char*>())          countdown.colors.accent = hexTo565(doc["acc"]);
    if (doc["card"].is<const char*>())         countdown.colors.cardBg = hexTo565(doc["card"]);
    saveCountdownConfig();
    Serial.println(F("OK: Countdown Updated & Saved to Flash."));
  }
  // 7. Force Screen Switch
  else if (strcmp(type, "switch_screen") == 0) {
    if (doc["screen"].is<uint8_t>()) {
      currentScreen = (doc["screen"].as<uint8_t>()) % TOTAL_SCREENS;
      saveCurrentScreen();
      Serial.print(F("OK: Screen Switched to "));
      Serial.println(currentScreen);
    }
  }
}

// --- TOUCH & SWIPE DETECTION ---
void handleTouchGestures() {
  uint16_t tx = 0, ty = 0;
  bool pressed = tft.getTouch(&tx, &ty, 400);

  if (pressed) {
    if (!isTouching) {
      isTouching = true;
      touchStartX = tx;
      touchStartY = ty;
      lastTouchX = tx;
      lastTouchY = ty;
      touchStartTime = millis();
    } else {
      lastTouchX = tx;
      lastTouchY = ty;
    }
  } else {
    if (isTouching) {
      isTouching = false;
      int16_t dx = lastTouchX - touchStartX;
      int16_t dy = lastTouchY - touchStartY;
      uint32_t duration = millis() - touchStartTime;

      if (abs(dx) > SWIPE_THRESHOLD && abs(dy) < 70 && duration < 800) {
        if (dx < 0) {
          currentScreen = (currentScreen + 1) % TOTAL_SCREENS;
        } else {
          currentScreen = (currentScreen == 0) ? (TOTAL_SCREENS - 1) : (currentScreen - 1);
        }
        saveCurrentScreen();
      }
      else if (touchStartY > 190) {
        if (touchStartX < 60) {
          currentScreen = (currentScreen == 0) ? (TOTAL_SCREENS - 1) : (currentScreen - 1);
          saveCurrentScreen();
        } else if (touchStartX > 260) {
          currentScreen = (currentScreen + 1) % TOTAL_SCREENS;
          saveCurrentScreen();
        }
      }
    }
  }
}

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  delay(200);

  // Initialize TFT with existing verified User_Setup
  tft.init();
  tft.setRotation(1); // 320x240 Landscape

  // Initialize schedule store with empty state
  for (int i = 0; i < MAX_SCHEDULED_DAYS; i++) {
    scheduleStore[i].active = false;
  }

  // Load touch calibration from NVS Flash
  loadTouchCalibration();

  // Load saved configurations from NVS Flash
  loadWiFiCredentials();
  loadWeatherConfig();
  loadCalendarConfig();
  loadCountdownConfig();
  loadCurrentScreen();

  // Allocate 320x240 16-bit double buffer in 8MB PSRAM (~153.6 KB)
  canvas.setAttribute(PSRAM_ENABLE, true);
  canvas.setColorDepth(16);
  void* spritePtr = canvas.createSprite(320, 240);

  if (!spritePtr) {
    Serial.println(F("Warning: PSRAM Sprite failed! Check PSRAM setting: OPI PSRAM"));
  } else {
    Serial.println(F("320x240 Zero-Flicker PSRAM Sprite Allocated Successfully."));
  }

  initPlatformerEnvironment();

  // Launch background network task on Core 0
  xTaskCreatePinnedToCore(
    backgroundNetworkTask,
    "net_task",
    8192,
    NULL,
    1,
    NULL,
    0
  );

  Serial.println(F("ESP32 3D Platformer Dashboard Online."));
}

// --- MAIN LOOP (RUNS ON CORE 1 AT 35-40 FPS) ---
void loop() {
  if (triggerCalibration) {
    triggerCalibration = false;
    runTouchCalibration();
  }

  static uint32_t lastSecTick = 0;
  static uint32_t lastCalendarCheck = 0;

  // Real-time second countdown update
  if (millis() - lastSecTick >= 1000) {
    lastSecTick = millis();

    time_t nowSecs;
    time(&nowSecs);
    if (countdown.targetEpoch > 0 && nowSecs > 1000000000) {
      if (countdown.targetEpoch > nowSecs) {
        countdown.remainingSecs = (uint32_t)(countdown.targetEpoch - nowSecs);
      } else {
        countdown.remainingSecs = 0;
      }
    } else if (countdown.remainingSecs > 0) {
      countdown.remainingSecs--;
    }
  }

  // Check NTP calendar day rollover every 30 seconds
  if (millis() - lastCalendarCheck >= 30000) {
    lastCalendarCheck = millis();
    updateCalendarFromNTP();
  }

  // Handle incoming Serial JSON and Touch Gestures
  parseSerialCommand();
  handleTouchGestures();

  // Render the current screen with 3D platformer world
  switch (currentScreen) {
    case 0: renderWeatherScreen(); break;
    case 1: renderCalendarScreen(); break;
    case 2: renderCountdownScreen(); break;
  }

  // Draw bottom navigation overlay
  renderNavigationFooter();

  // Push full 320x240 frame buffer to ILI9341 in a single burst (Zero Flicker!)
  canvas.pushSprite(0, 0);

  delay(25);
}
