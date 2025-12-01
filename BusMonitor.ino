/*
  ESP32 SEPTA Bus ETA + TTGO T-Display Countdown
  ----------------------------------------------

  Purpose:
    - Connect ESP32 to Wi-Fi
    - Query SEPTA's real-time API for a given route/direction/stop
    - Determine the earliest upcoming arrival (next bus)
    - Maintain global variables:
        bool   busDataAvailable
        time_t nextBusEtaEpoch
        int    nextBusMinutes
    - Display a large bright-green MM:SS countdown on a Lilygo TTGO T-Display
      (240x135 ST7789), in landscape mode.
    - If no next bus exists, show "--:--"

  Display behavior:
    - Landscape (240x135), black background.
    - Big green "MM:SS" in the center (text size 6).
    - Updated every second based on the difference between the current epoch
      time and nextBusEtaEpoch.
    - If busDataAvailable == false or nextBusEtaEpoch == 0, show "--:--".

  How to configure:
    1. Set your Wi-Fi SSID/password below.
    2. Set ROUTE_ID, DIRECTION_ID, STOP_ID to match your stop.
    3. Make sure the TFT_eSPI library is installed and configured
       for TTGO T-Display (135x240 ST7789).
       - In TFT_eSPI's User_Setup_Select.h, enable the TTGO_T_Display setup.
    4. Board: usually "ESP32 Dev Module" or the TTGO T-Display profile.

  Data for other code:
    - bool   busDataAvailable
    - time_t nextBusEtaEpoch
    - int    nextBusMinutes

    Example:
      if (busDataAvailable && nextBusMinutes >= 0) {
        // draw or use the ETA in your own UI
      }

  Serial:
    - Baud: 115200
    - Logs SEPTA queries and prints summary each time data is fetched.

*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <vector>
#include <algorithm>

// --- Display (TTGO T-Display: ST7789 240x135 via TFT_eSPI) ---
#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();  // Assumes TFT_eSPI is configured for TTGO T-Display

// WiFi credentials (change these to match your network)
const char* ssid = "WIFI_SSID"; // Your WiFi SSID
const char* password = "WIFI_PASSWORD"; // Your WiFi password


// ---------- SEPTA configuration ----------
//Original setup from Github repo:
//const String ROUTE_ID = "48"; // Route ID (48)
//const String DIRECTION_ID = "0"; // Direction ID (Southbound)
//const String STOP_ID = "10266"; // Stop ID (Market St & 15th St)

//3611 Fairmount setup
const String ROUTE_ID = "31"; // Route ID (31)
const String DIRECTION_ID = "0"; // Direction ID (Southbound)
const String STOP_ID = "21514"; // Stop ID (Fairmount & 37th)

// ---------- Time / NTP configuration ----------
const char* ntpServer          = "pool.ntp.org";
// Adjust these if you want a different time zone; these are for Eastern Time (US)
const long  gmtOffset_sec      = -18000; // UTC-5 hours (EST)
const int   daylightOffset_sec = 3600;   // +1h for DST when applicable

// ---------- Update interval ----------
unsigned long SEPTA_UPDATE_INTERVAL = 10000; // 10 seconds between API checks

// ---------- Global state for “what’s the next bus?” ----------

// True if at least one upcoming bus found at the stop
bool busDataAvailable = false;

// Earliest ETA (Unix epoch time in seconds). 0 means invalid.
time_t nextBusEtaEpoch = 0;

// Minutes until the next bus (rounded up). -1 means invalid.
int nextBusMinutes = -1;

// ---------- Function declarations ----------
void setupWiFi();
void fetchSeptaData();
time_t processSeptaData(DynamicJsonDocument& doc);
void updateDisplay();

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.println("=== ESP32 SEPTA Bus ETA + TTGO T-Display ===");

  // Init display
  tft.init();
  tft.setRotation(1); // Landscape: 240x135
  tft.fillScreen(TFT_BLACK);
  tft.setTextFont(1);    // Built-in small font, we will scale it up
  tft.setTextDatum(MC_DATUM); // Middle-center text datum for easy centering

  // Initial splash
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("SEPTA ETA", tft.width() / 2, tft.height() / 2);

  setupWiFi();

  // Configure NTP time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Give it a moment to sync
  delay(1000);

  Serial.println("Setup complete. Starting periodic SEPTA queries...");
}

// ---------- Main loop ----------
void loop() {
  static unsigned long lastFetch   = 0;
  static unsigned long lastDisplay = 0;
  unsigned long nowMillis = millis();

  // Fetch SEPTA data every SEPTA_UPDATE_INTERVAL ms
  if (nowMillis - lastFetch >= SEPTA_UPDATE_INTERVAL) {
    lastFetch = nowMillis;
    fetchSeptaData();

    // Serial summary after each fetch (globals already updated)
    Serial.println();
    Serial.println("=== Summary (for your code to use) ===");
    if (busDataAvailable && nextBusMinutes >= 0) {
      Serial.print("Next bus in ");
      Serial.print(nextBusMinutes);
      Serial.println(" minute(s).");
      Serial.print("Epoch ETA: ");
      Serial.println((long)nextBusEtaEpoch);
    } else {
      Serial.println("No upcoming buses found or data unavailable.");
    }
    Serial.println("======================================");
    Serial.println();
  }

  // Update the on-screen MM:SS countdown every 1 second
  if (nowMillis - lastDisplay >= 1000) {
    lastDisplay = nowMillis;
    updateDisplay();
  }

  delay(10); // keep loop responsive but calm
}

// ---------- WiFi helper ----------
void setupWiFi() {
  Serial.print("Connecting to WiFi SSID: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  const int maxAttempts = 40; // ~20 seconds

  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("WiFi connection failed.");
  }
}

// ---------- Display update ----------

void updateDisplay() {
  char buf[6]; // "MM:SS" or "--:--" + null

  if (busDataAvailable && nextBusEtaEpoch > 0) {
    time_t nowEpoch;
    time(&nowEpoch);

    long delta = (long)(nextBusEtaEpoch - nowEpoch);
    if (delta < 0) {
      delta = 0; // if we've crossed the ETA, clamp at zero until next refresh
    }

    int mins = delta / 60;
    int secs = delta % 60;

    // Clamp to 99:59 for sanity
    if (mins > 99) {
      mins = 99;
      secs = 59;
    }

    snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
  } else {
    // No valid bus: show "--:--"
    strcpy(buf, "--:--");
  }

  // Draw the big green countdown in the center
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(6);             // big, but still fits "MM:SS" on 240x135
  tft.setTextDatum(MC_DATUM);     // drawString position is center of text
  tft.drawString(buf, tft.width() / 2, tft.height() / 2);
}

// ---------- SEPTA fetch + parse ----------

void fetchSeptaData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected; attempting reconnect...");
    setupWiFi();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Still not connected. Skipping this fetch.");
      busDataAvailable = false;
      nextBusEtaEpoch = 0;
      nextBusMinutes = -1;
      return;
    }
  }

  HTTPClient http;
  String url = "https://www3.septa.org/api/v2/trips/?route_id=" + ROUTE_ID;

  Serial.println();
  Serial.print("Fetching SEPTA trips for route ");
  Serial.print(ROUTE_ID);
  Serial.print(" (direction ");
  Serial.print(DIRECTION_ID);
  Serial.println(")...");

  http.setTimeout(5000);
  http.begin(url);
  http.addHeader("User-Agent", "ESP32");
  http.addHeader("Accept", "application/json");

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();

    if (payload.length() > 0) {
      // Big enough to hold a bunch of trips
      DynamicJsonDocument doc(32768);
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        time_t result = processSeptaData(doc);
        (void)result; // not used further here
      } else {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        busDataAvailable = false;
        nextBusEtaEpoch = 0;
        nextBusMinutes = -1;
      }
    } else {
      Serial.println("Empty response received from trips endpoint.");
      busDataAvailable = false;
      nextBusEtaEpoch = 0;
      nextBusMinutes = -1;
    }
  } else {
    Serial.print("HTTP request failed with code: ");
    Serial.println(httpCode);
    if (httpCode < 0) {
      Serial.print("Error string: ");
      Serial.println(http.errorToString(httpCode));
    }
    busDataAvailable = false;
    nextBusEtaEpoch = 0;
    nextBusMinutes = -1;
  }

  http.end();
}

// Parse the /trips/ array and query /trip-update/ for each candidate trip
time_t processSeptaData(DynamicJsonDocument& doc) {
  time_t nowEpoch;
  time(&nowEpoch);

  struct tm timeinfo;
  localtime_r(&nowEpoch, &timeinfo);

  Serial.println();
  Serial.println("=== Processing SEPTA data ===");
  Serial.printf("Current local time: %02d:%02d:%02d\n",
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  std::vector<time_t> allETAs;

  // doc should be a JSON array
  JsonArray trips = doc.as<JsonArray>();

  for (JsonVariant trip : trips) {
    String tripId    = trip["trip_id"].as<String>();
    String direction = trip["direction_id"].as<String>();
    String status    = trip["status"].as<String>();

    // Only consider trips in the right direction and not canceled/no GPS
    if (direction == DIRECTION_ID &&
        status != "CANCELED" &&
        status != "NO GPS") {

      Serial.println();
      Serial.print("Checking trip ");
      Serial.println(tripId);

      HTTPClient http;
      String updateUrl = "https://www3.septa.org/api/v2/trip-update/?trip_id=" + tripId;

      http.setTimeout(5000);
      http.begin(updateUrl);
      http.addHeader("User-Agent", "ESP32");
      http.addHeader("Accept", "application/json");

      int httpCode = http.GET();

      if (httpCode == HTTP_CODE_OK) {
        String updatePayload = http.getString();

        DynamicJsonDocument updateDoc(updatePayload.length() + 512);
        DeserializationError error = deserializeJson(updateDoc, updatePayload);

        if (!error) {
          JsonArray stopTimes = updateDoc["stop_times"].as<JsonArray>();

          for (JsonVariant stop : stopTimes) {
            String stopId = stop["stop_id"].as<String>();
            bool departed = stop["departed"].as<bool>();
            time_t eta    = stop["eta"].as<time_t>();

            if (stopId == STOP_ID && !departed) {
              // Only consider future arrivals
              if (eta > nowEpoch) {
                int minutesUntil = (int)((eta - nowEpoch + 59) / 60); // round up

                localtime_r(&eta, &timeinfo);
                Serial.printf("  Found arrival at %02d:%02d:%02d (%d minute(s) from now)\n",
                              timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                              minutesUntil);

                allETAs.push_back(eta);
              }
            }
          }
        } else {
          Serial.print("  Error parsing trip-update JSON: ");
          Serial.println(error.c_str());
        }
      } else {
        Serial.print("  trip-update HTTP failed with code: ");
        Serial.println(httpCode);
      }

      http.end();
    }
  }

  if (!allETAs.empty()) {
    std::sort(allETAs.begin(), allETAs.end());

    Serial.println();
    Serial.println("Upcoming buses at this stop:");

    for (time_t eta : allETAs) {
      int minutesUntil = (int)((eta - nowEpoch + 59) / 60); // round up
      localtime_r(&eta, &timeinfo);

      Serial.printf("  %02d:%02d:%02d (%d minute(s) from now)\n",
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                    minutesUntil);
    }

    // Earliest ETA
    time_t soonest = allETAs.front();
    int soonestMinutes = (int)((soonest - nowEpoch + 59) / 60); // round up

    localtime_r(&soonest, &timeinfo);

    Serial.println();
    Serial.println("=== Results ===");
    Serial.print("Found ");
    Serial.print(allETAs.size());
    Serial.println(" upcoming bus(es).");

    Serial.print("Soonest arrival in ");
    Serial.print(soonestMinutes);
    Serial.print(" minute(s), at ");
    Serial.printf("%02d:%02d:%02d\n",
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    // Update global state for other code to use
    busDataAvailable = true;
    nextBusEtaEpoch  = soonest;
    nextBusMinutes   = soonestMinutes;

    return soonest;
  } else {
    Serial.println();
    Serial.println("No upcoming buses found for this route/direction/stop.");

    busDataAvailable = false;
    nextBusEtaEpoch  = 0;
    nextBusMinutes   = -1;

    return 0;
  }
}
