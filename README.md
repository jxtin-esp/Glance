# ESP32-S3 (N16R8) + 2.8" ILI9341 3D Platformer Screen

A retro 3D platformer dashboard featuring live WiFi weather fetching with big 8-bit procedural weather art, an interactive Python monthly grid calendar with automatic daily event rollover, real-world date countdowns, and a pseudo-3D scrolling ground & sky world.

## Hardware Configuration
* **Microcontroller:** ESP32-S3 N16R8 (16MB Flash, 8MB PSRAM)
* **Display:** 2.8" SPI TFT LCD (ILI9341), 320x240 Landscape (`tft.setRotation(1)`)
* **Touchscreen:** XPT2046 Resistive Touch (`TOUCH_CS: 8`)
* **Pins:**
  - MOSI: GPIO 11
  - MISO: GPIO 13
  - SCLK: GPIO 12
  - TFT CS: GPIO 10
  - TFT DC: GPIO 9
  - TFT RST: GPIO 14
  - Touch CS: GPIO 8

## Latest Features
1. **Big 8-Bit Procedural Weather Art Icons:**
   - The empty space beside the temperature readout is now filled with a large, animated retro weather illustration:
     - **Sunny:** Radiant glowing pixel sun with 8-way stepped rays.
     - **Partly Cloudy:** Bright retro sun peeking from behind a puffy cloud.
     - **Cloudy / Overcast:** Big dual-layered 3D retro clouds.
     - **Rainy:** Dark storm cloud with 4 animated cyan pixel rain streams.
     - **Thunderstorm:** Deep dark cloud with a thick jagged neon-yellow lightning bolt striking through!
     - **Snowy:** Winter cloud with falling cross-shaped pixel snowflakes.
2. **Interactive Monthly Grid Calendar:**
   - The companion app features a full monthly calendar grid with month navigation (`◀` / `▶`).
   - Click any date to add or edit up to 3 events for that specific day.
   - Dates with scheduled events are marked with an indicator dot (`•`).
   - All events persist locally in `calendar_events.json`.
3. **Automatic Daily Event Rollover on ESP32:**
   - You no longer need to adjust daily dates manually!
   - The ESP32 syncs with atomic NTP time over WiFi, checks the current real-world date, updates the date banner automatically (e.g. `SATURDAY, NOV 21`), and loads that day's events when that date arrives.
4. **Touch Calibration:**
   - First-boot interactive 4-corner calibration with flash memory persistence.
   - Re-calibratable anytime via the **🎯 Calibrate Touch** button in the Python companion.
5. **Zero-Flicker Double Buffering:**
   - Full 320x240 16-bit PSRAM canvas sprite running at 35–40 FPS.

## How to Flash & Run

### 1. Arduino IDE Upload
1. Open [esp32_pixel_dashboard.ino](file:///C:/Users/jatin/OneDrive/Documents/Arduino/esp32_pixel_dashboard/esp32_pixel_dashboard.ino).
2. Set board options under **Tools**:
   - **Board:** `ESP32S3 Dev Module`
   - **PSRAM:** `OPI PSRAM`
   - **Flash Size:** `16MB (128Mb)`
3. Click **Upload**.
4. If prompted on screen, touch the 4 corner arrows to calibrate touch.

### 2. Python Companion App
1. Double-click [run_companion.bat](file:///C:/Users/jatin/OneDrive/Documents/Arduino/esp32_pixel_dashboard/run_companion.bat) or run:
   ```bash
   python companion_app.py
   ```
2. Select your ESP32's COM port and click **Connect**.
3. **WiFi:** Connect the ESP32 to your WiFi network using the top bar.
4. **Weather:** Set your city and watch the live weather and big procedural icon update!
5. **Grid Calendar:** Click any date on the calendar, enter events, and click **💾 Save Events for this Date**. The schedule syncs to the ESP32 and will automatically display when that day comes.
