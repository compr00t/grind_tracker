// Grind Tracker 2.0: An M5Paper application to track, display, and manage multiple grind sizes or values
// with touch interaction and persistent storage.

#include <M5Unified.h>
#include <SD.h>
#include <WiFi.h>
#include <WebServer.h>
#include <vector>
#include <algorithm>

// Constants
const int MAX_ENTRIES = 10;
const char* FILENAME = "/grind_values.txt";
const int SCREEN_WIDTH = 540;
const int SCREEN_HEIGHT = 960;

// UI Layout constants - optimized for small screen
const int LEFT_MARGIN = 15;
const int RIGHT_MARGIN = 15;
const int BATTERY_AREA_Y = 10;
const int BATTERY_AREA_H = 36;
const int BATTERY_TEXT_SIZE = 3;
const int START_Y = BATTERY_AREA_Y + BATTERY_AREA_H + 8;
const int ROW_HEIGHT = 85;
const int ROW_PADDING = 4;
const int NAME_X = LEFT_MARGIN;
const int VALUE_X = SCREEN_WIDTH - RIGHT_MARGIN - 80;
const int VALUE_WIDTH = 60;
const int NAME_WIDTH = VALUE_X - NAME_X - 20;
const int STATUS_MSG_Y = 900;
const int STATUS_MSG_H = 35;
const int STATUS_MSG_X = 0;
const int STATUS_MSG_W = SCREEN_WIDTH;
const int STATUS_TEXT_SIZE = 2;
const int NAME_TEXT_SIZE = 3;
const int VALUE_TEXT_SIZE = 3;
const int MODE_TEXT_SIZE = 2;

// Values
const int MIN_VALUE = 0;
const int MAX_VALUE = 99;
const unsigned long SAVE_DELAY_MS = 3000;
const unsigned long INTERACTION_TIMEOUT_MS = 300000; // 5 minutes

// Data structures
struct TouchRegion {
    int x, y, w, h;
    bool contains(int px, int py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

struct GrindSetting {
    String name;
    int value;
};

// Global state
std::vector<GrindSetting> settings;
bool needsSave = false;
unsigned long lastChangeTime = 0;
unsigned long lastTouchTime = 0;
bool statusMessageActive = false;
unsigned long messageStartTime = 0;
uint32_t messageDuration = 0;
bool displayNeedsUpdate = false;
int savedSelectedIndex = 0;

// Navigation state
enum class NavigationMode {
    NAVIGATE,  // Navigating through entries
    EDIT_VALUE // Editing the selected entry's value
};
NavigationMode navMode = NavigationMode::NAVIGATE;
int selectedIndex = 0;  // Currently selected entry index

// WiFi and Web Server
const char* WIFI_SSID = "GrindTracker";
WebServer server(80);
bool wifiInitialized = false;


uint16_t getBackgroundColor() {
    return M5.Display.isEPD() ? TFT_WHITE : M5.Display.getBaseColor();
}

uint16_t getForegroundColor() {
    return M5.Display.isEPD() ? TFT_BLACK : ~M5.Display.getBaseColor();
}

// Function declarations
void drawUI();
void drawBatteryStatus();
void readValuesFromSD();
void saveValuesToSD();
void displayStatusMessage(const String& msg, uint32_t duration);
void clearStatusMessageArea();
void goToSleep();
void addNewSetting();
void deleteSetting(size_t index);
void initWiFi();
void handleRoot();
void handleAPI();
void handleAddCoffee();
void handleUpdateValue();
void handleDeleteCoffee();
void handleSync();

void setup() {
    // Initialize M5Unified
    M5.begin();
    
    // Configure e-paper display
    if (M5.Display.isEPD()) {
        M5.Display.setEpdMode(epd_mode_t::epd_fastest);
        // Ensure portrait mode (height > width)
        if (M5.Display.width() > M5.Display.height()) {
            M5.Display.setRotation(M5.Display.getRotation() ^ 1);
        }
    }
    
    // Initialize SD card
    if (!SD.begin(GPIO_NUM_4, SPI, 25000000)) {
        // SD card failed - show error but continue
        M5.Display.startWrite();
        M5.Display.fillScreen(getBackgroundColor());
        M5.Display.setTextSize(3);
        M5.Display.setColor(getForegroundColor());
        M5.Display.setTextDatum(textdatum_t::middle_center);
        M5.Display.drawString("SD Card Error", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 20);
        M5.Display.drawString("Continuing...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 20);
        M5.Display.endWrite();
        M5.Display.display();
        delay(2000);
    }
    
    // Load settings from SD
    readValuesFromSD();
    
    // Create default settings if empty
    if (settings.empty()) {
        settings.push_back({"Espresso", 15});
        saveValuesToSD();
        needsSave = false;
    }
    
    // Restore selection if we have saved one and it's valid
    if (savedSelectedIndex >= 0 && savedSelectedIndex < (int)settings.size()) {
        selectedIndex = savedSelectedIndex;
    } else {
        selectedIndex = 0;  // Default to first entry
    }
    navMode = NavigationMode::NAVIGATE;  // Start in navigate mode
    
    // Initialize WiFi and Web Server
    initWiFi();
    
    // Initial display
    M5.Display.startWrite();
    drawUI();
    M5.Display.endWrite();
    M5.Display.display();
    
    lastTouchTime = millis();
}

void loop() {
    M5.update();
    
    // Handle web server requests
    if (wifiInitialized) {
        server.handleClient();
    }
    
    // Check status message timeout and clear it
    if (statusMessageActive && millis() - messageStartTime >= messageDuration) {
        clearStatusMessageArea();
        statusMessageActive = false;  // Ensure it's marked as inactive
    }
    
    // Handle joystick/button input
    bool valueWasUpdated = false;
    bool displayNeedsUpdate = false;
    
    // BtnA = Up (navigate up or increase value)
    if (M5.BtnA.wasClicked()) {
        lastTouchTime = millis();
        if (navMode == NavigationMode::NAVIGATE) {
            // Navigate up through entries
            if (selectedIndex > 0) {
                selectedIndex--;
                displayNeedsUpdate = true;
            }
        } else if (navMode == NavigationMode::EDIT_VALUE) {
            // Increase value
            if (selectedIndex < settings.size() && settings[selectedIndex].value < MAX_VALUE) {
                settings[selectedIndex].value++;
                valueWasUpdated = true;
                displayNeedsUpdate = true;
            }
        }
    }
    
    // BtnC = Down (navigate down or decrease value)
    if (M5.BtnC.wasClicked()) {
        lastTouchTime = millis();
        if (navMode == NavigationMode::NAVIGATE) {
            // Navigate down through entries
            if (selectedIndex < settings.size() - 1) {
                selectedIndex++;
                displayNeedsUpdate = true;
            }
        } else if (navMode == NavigationMode::EDIT_VALUE) {
            // Decrease value
            if (selectedIndex < settings.size() && settings[selectedIndex].value > MIN_VALUE) {
                settings[selectedIndex].value--;
                valueWasUpdated = true;
                displayNeedsUpdate = true;
            }
        }
    }
    
    // BtnB = Select (enter edit mode, exit edit mode, or perform action)
    if (M5.BtnB.wasClicked()) {
        lastTouchTime = millis();
        if (navMode == NavigationMode::NAVIGATE) {
            // Enter edit mode for selected entry
            if (selectedIndex < settings.size()) {
                navMode = NavigationMode::EDIT_VALUE;
                displayNeedsUpdate = true;
            }
        } else if (navMode == NavigationMode::EDIT_VALUE) {
            // Exit edit mode
            navMode = NavigationMode::NAVIGATE;
            displayNeedsUpdate = true;
        }
    }
    
    // Long press BtnA to delete entry (when in navigate mode)
    if (navMode == NavigationMode::NAVIGATE && M5.BtnA.wasHold() && selectedIndex < settings.size()) {
        lastTouchTime = millis();
        deleteSetting(selectedIndex);
        displayNeedsUpdate = true;
    }
    
    // Update display if needed
    if (displayNeedsUpdate || valueWasUpdated) {
        M5.Display.startWrite();
        drawUI();
        M5.Display.endWrite();
        M5.Display.display();
        if (valueWasUpdated) {
            needsSave = true;
            lastChangeTime = millis();
        }
    }
    
    // Save if needed
    if (needsSave && (millis() - lastChangeTime >= SAVE_DELAY_MS)) {
        saveValuesToSD();
        needsSave = false;
    }
    
    // Sleep if no activity
    if (millis() - lastTouchTime >= INTERACTION_TIMEOUT_MS) {
        savedSelectedIndex = selectedIndex;
        selectedIndex = -1;
        navMode = NavigationMode::NAVIGATE;
        displayNeedsUpdate = true;
        
        if (displayNeedsUpdate) {
            M5.Display.startWrite();
            drawUI();
            M5.Display.endWrite();
            M5.Display.display();
            displayNeedsUpdate = false;
        }
        
        delay(500);
        goToSleep();
    }
    
    delay(50);
}

void drawBatteryStatus() {
    const float batVolMin = 3.3;
    const float batVolMax = 4.2;
    float voltage = M5.Power.getBatteryVoltage() / 1000.0;
    int percentage = int(((voltage - batVolMin) / (batVolMax - batVolMin)) * 100.0);
    percentage = std::max(0, std::min(100, percentage));
    
    String batText = String(percentage) + "%";
    String titleText = "Grind Tracker";
    
    // Header bar with black background
    M5.Display.setColor(getForegroundColor());  // Black
    M5.Display.fillRect(0, 0, SCREEN_WIDTH, BATTERY_AREA_Y + BATTERY_AREA_H);
    
    M5.Display.setColor(getBackgroundColor());
    M5.Display.setTextColor(getBackgroundColor());
    M5.Display.setTextSize(BATTERY_TEXT_SIZE);
    
    M5.Display.setTextDatum(textdatum_t::top_left);
    int text_y = BATTERY_AREA_Y + (BATTERY_AREA_H - M5.Display.fontHeight(BATTERY_TEXT_SIZE)) / 2;
    M5.Display.drawString(titleText, LEFT_MARGIN, text_y);
    
    M5.Display.setTextDatum(textdatum_t::top_right);
    M5.Display.setColor(getBackgroundColor());
    M5.Display.setTextColor(getBackgroundColor());
    M5.Display.drawString(batText, SCREEN_WIDTH - RIGHT_MARGIN, text_y);
}

void drawUI() {
    M5.Display.fillScreen(getBackgroundColor());
    drawBatteryStatus();
    
    int availableHeight = STATUS_MSG_Y - START_Y - 20;
    int maxVisibleRows = availableHeight / ROW_HEIGHT;
    
    for (size_t i = 0; i < settings.size(); i++) {
        int current_y = START_Y + i * ROW_HEIGHT;
        bool isSelected = (selectedIndex >= 0 && i == (size_t)selectedIndex);
        bool isEditing = (isSelected && navMode == NavigationMode::EDIT_VALUE);
        
        if (isSelected) {
            M5.Display.setColor(getForegroundColor());
            M5.Display.fillRect(0, current_y - ROW_PADDING, SCREEN_WIDTH, ROW_HEIGHT + ROW_PADDING * 2);
        } else {
            M5.Display.setColor(getForegroundColor());
            M5.Display.drawFastHLine(LEFT_MARGIN, current_y + ROW_HEIGHT - 1, SCREEN_WIDTH - LEFT_MARGIN - RIGHT_MARGIN);
        }
        
        if (isSelected) {
            M5.Display.setColor(getBackgroundColor());
            M5.Display.setTextColor(getBackgroundColor());
        } else {
            M5.Display.setColor(getForegroundColor());
            M5.Display.setTextColor(getForegroundColor());
        }
        
        M5.Display.setTextSize(NAME_TEXT_SIZE);
        M5.Display.setTextDatum(textdatum_t::top_left);
        int name_y = current_y + (ROW_HEIGHT - M5.Display.fontHeight(NAME_TEXT_SIZE)) / 2;
        
        String nameText = settings[i].name;
        int maxNameWidth = NAME_WIDTH;
        M5.Display.setTextSize(NAME_TEXT_SIZE);
        if (M5.Display.textWidth(nameText) > maxNameWidth) {
            while (M5.Display.textWidth(nameText + "...") > maxNameWidth && nameText.length() > 0) {
                nameText = nameText.substring(0, nameText.length() - 1);
            }
            nameText += "...";
        }
        M5.Display.drawString(nameText, NAME_X, name_y);
        
        int value_y = current_y + (ROW_HEIGHT - M5.Display.fontHeight(VALUE_TEXT_SIZE)) / 2;
        
        if (isSelected) {
            M5.Display.setColor(getBackgroundColor());
            M5.Display.setTextColor(getBackgroundColor());
        } else {
            M5.Display.setColor(getForegroundColor());
            M5.Display.setTextColor(getForegroundColor());
        }
        
        M5.Display.setTextSize(VALUE_TEXT_SIZE);
        M5.Display.setTextDatum(textdatum_t::middle_right);
        String valueStr = String(settings[i].value);
        if (isEditing) {
            valueStr = "[" + valueStr + "]";
        }
        M5.Display.drawString(valueStr, VALUE_X, value_y + M5.Display.fontHeight(VALUE_TEXT_SIZE) / 2);
    }
}

void displayStatusMessage(const String& msg, uint32_t duration) {
    statusMessageActive = true;
    messageDuration = duration;
    messageStartTime = millis();
    
    M5.Display.startWrite();
    // Clear status area with white background
    M5.Display.setColor(getBackgroundColor());
    M5.Display.fillRect(STATUS_MSG_X, STATUS_MSG_Y, STATUS_MSG_W, STATUS_MSG_H);
    // Draw message in black text
    M5.Display.setTextSize(STATUS_TEXT_SIZE);
    M5.Display.setColor(getForegroundColor());
    M5.Display.setTextDatum(textdatum_t::middle_center);
    M5.Display.drawString(msg, STATUS_MSG_X + STATUS_MSG_W / 2, STATUS_MSG_Y + STATUS_MSG_H / 2);
    M5.Display.endWrite();
    M5.Display.display(STATUS_MSG_X, STATUS_MSG_Y, STATUS_MSG_W, STATUS_MSG_H);
}

void clearStatusMessageArea() {
    if (!statusMessageActive) return;
    
    M5.Display.startWrite();
    M5.Display.setColor(getBackgroundColor());
    M5.Display.fillRect(STATUS_MSG_X, STATUS_MSG_Y, STATUS_MSG_W, STATUS_MSG_H);
    M5.Display.endWrite();
    M5.Display.display(STATUS_MSG_X, STATUS_MSG_Y, STATUS_MSG_W, STATUS_MSG_H);
    statusMessageActive = false;
}

void readValuesFromSD() {
    settings.clear();
    
    if (!SD.begin(GPIO_NUM_4, SPI, 25000000)) {
        return;  // SD not available
    }
    
    File file = SD.open(FILENAME, FILE_READ);
    if (file) {
        while (file.available() && settings.size() < MAX_ENTRIES) {
            String line = file.readStringUntil('\n');
            line.trim();
            if (line.length() > 0) {
                int commaIndex = line.indexOf(',');
                if (commaIndex != -1) {
                    String name = line.substring(0, commaIndex);
                    String valueStr = line.substring(commaIndex + 1);
                    int value = valueStr.toInt();
                    settings.push_back({name, value});
                }
            }
        }
        file.close();
    }
}

void saveValuesToSD() {
    if (!SD.begin(GPIO_NUM_4, SPI, 25000000)) {
        displayStatusMessage("Save Error: SD Missing", 4000);
        return;
    }
    
    File file = SD.open(FILENAME, FILE_WRITE);
    if (file) {
        bool writeError = false;
        for (const auto& setting : settings) {
            size_t nameBytesWritten = file.print(setting.name + ",");
            size_t valueBytesWritten = file.println(setting.value);
            
            if (nameBytesWritten == 0 || valueBytesWritten == 0) {
                writeError = true;
            }
        }
        file.close();
        
        if (writeError) {
            displayStatusMessage("Save Error: Write Failed", 4000);
        } else {
            displayStatusMessage("Saved OK", 2000);
        }
    } else {
        displayStatusMessage("Save Error: Can't Open File", 4000);
    }
}

void goToSleep() {
    M5.Display.powerSaveOn();
    M5.Display.sleep();
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_36, 0);
    M5.Power.powerOff();
}

void addNewSetting() {
    if (settings.size() >= MAX_ENTRIES) {
        displayStatusMessage("Max entries reached", 2000);
        return;
    }
    
    String newName = "Setting " + String(settings.size() + 1);
    settings.push_back({newName, 0});
    displayStatusMessage("Added: " + newName, 2000);
}

void deleteSetting(size_t index) {
    if (index >= settings.size()) return;
    
    String deletedName = settings[index].name;
    settings.erase(settings.begin() + index);
    displayStatusMessage("Deleted: " + deletedName, 2000);
}

void initWiFi() {
    WiFi.mode(WIFI_AP);
    bool apStarted = WiFi.softAP(WIFI_SSID, NULL);
    
    if (apStarted) {
        delay(100);
        IPAddress IP = WiFi.softAPIP();
        
        server.on("/", handleRoot);
        server.on("/api/data", HTTP_GET, handleAPI);
        server.on("/api/add", HTTP_POST, handleAddCoffee);
        server.on("/api/update", HTTP_POST, handleUpdateValue);
        server.on("/api/delete", HTTP_POST, handleDeleteCoffee);
        server.on("/api/sync", HTTP_POST, handleSync);
        
        server.begin();
        wifiInitialized = true;
    } else {
        wifiInitialized = false;
    }
}

void handleRoot() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Grind Tracker</title>";
    html += "<link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/css/bootstrap.min.css' rel='stylesheet'>";
    html += "<style>";
    html += "body { padding: 10px; background: #f5f5f5; }";
    html += ".container { max-width: 100%; }";
    html += ".entry { margin-bottom: 8px; }";
    html += ".sync-btn-container { margin-top: 20px; padding-top: 15px; border-top: 2px solid #dee2e6; }";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<div class='card shadow-sm'>";
    html += "<div class='card-body'>";
    html += "<div class='d-flex gap-2 align-items-center mb-3'>";
    html += "<input type='text' class='form-control' id='newName' placeholder='Coffee name' maxlength='30'>";
    html += "<input type='number' class='form-control' id='newValue' placeholder='Grind' min='0' max='99' value='25' style='max-width: 80px;'>";
    html += "<button class='btn btn-sm btn-outline-primary' onclick='addCoffee()' title='Add Coffee'>+</button>";
    html += "</div>";
    html += "<div id='entries'></div>";
    html += "<div class='sync-btn-container'>";
    html += "<button class='btn btn-success w-100' onclick='syncToDevice()' title='Sync to Device'>Sync to Device</button>";
    html += "</div>";
    html += "</div></div>";
    html += "<script>";
    html += "function loadData() {";
    html += "  fetch('/api/data').then(r => r.json()).then(data => {";
    html += "    let html = '';";
    html += "    data.forEach((entry, idx) => {";
    html += "      html += '<div class=\"entry card\"><div class=\"card-body\">';";
    html += "      html += '<div class=\"d-flex justify-content-between align-items-center\">';";
      html += "      html += '<span class=\"fw-bold\">' + entry.name + '</span>';";
    html += "      html += '<div class=\"d-flex gap-2 align-items-center\">';";
    html += "      html += '<input type=\"number\" class=\"form-control\" id=\"val' + idx + '\" value=\"' + entry.value + '\" min=\"0\" max=\"99\" style=\"width: 70px;\" onchange=\"updateValue(' + idx + ')\">';";
    html += "      html += '<button class=\"btn btn-sm btn-outline-danger\" onclick=\"deleteCoffee(' + idx + ')\" title=\"Delete\">X</button>';";
    html += "      html += '</div></div></div></div>';";
    html += "    });";
    html += "    document.getElementById('entries').innerHTML = html;";
    html += "  });";
    html += "}";
    html += "function addCoffee() {";
    html += "  let name = document.getElementById('newName').value.trim();";
    html += "  const value = parseInt(document.getElementById('newValue').value);";
    html += "  if (!name) { alert('Please enter a coffee name'); return; }";
    html += "  fetch('/api/add', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({name: name, value: value}) })";
    html += "    .then(() => { document.getElementById('newName').value = ''; document.getElementById('newValue').value = '25'; loadData(); });";
    html += "}";
    html += "function updateValue(idx) {";
    html += "  const value = parseInt(document.getElementById('val' + idx).value);";
    html += "  fetch('/api/update', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({index: idx, value: value}) });";
    html += "}";
    html += "function deleteCoffee(idx) {";
    html += "  if (confirm('Delete this coffee?')) {";
    html += "    fetch('/api/delete', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({index: idx}) })";
    html += "      .then(() => loadData());";
    html += "  }";
    html += "}";
    html += "function syncToDevice() {";
    html += "  const entries = [];";
    html += "  document.querySelectorAll('.entry').forEach((entry, idx) => {";
    html += "    const nameEl = entry.querySelector('span.fw-bold');";
    html += "    const valueEl = entry.querySelector('input[type=\"number\"]');";
    html += "    if (nameEl && valueEl) {";
    html += "      entries.push({";
    html += "        index: idx,";
    html += "        name: nameEl.textContent,";
    html += "        value: parseInt(valueEl.value)";
    html += "      });";
    html += "    }";
    html += "  });";
    html += "  fetch('/api/sync', {";
    html += "    method: 'POST',";
    html += "    headers: {'Content-Type': 'application/json'},";
    html += "    body: JSON.stringify({entries: entries})";
    html += "  });";
    html += "}";
    html += "loadData();";
    html += "</script></div></body></html>";
    server.send(200, "text/html", html);
}

void handleAPI() {
    String json = "[";
    for (size_t i = 0; i < settings.size(); i++) {
        if (i > 0) json += ",";
        json += "{\"name\":\"" + settings[i].name + "\",\"value\":" + String(settings[i].value) + "}";
    }
    json += "]";
    server.send(200, "application/json", json);
}

void handleAddCoffee() {
    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        int nameStart = body.indexOf("\"name\":\"") + 8;
        int nameEnd = body.indexOf("\"", nameStart);
        int valueStart = body.indexOf("\"value\":") + 8;
        int valueEnd = body.indexOf("}", valueStart);
        
        if (nameStart > 7 && nameEnd > nameStart && valueStart > 7) {
            String name = body.substring(nameStart, nameEnd);
            int value = body.substring(valueStart, valueEnd).toInt();
            
            if (settings.size() < MAX_ENTRIES && name.length() > 0) {
                settings.push_back({name, value});
                saveValuesToSD();
                needsSave = false;
                server.send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                server.send(400, "application/json", "{\"status\":\"error\",\"msg\":\"Max entries or invalid name\"}");
            }
        } else {
            server.send(400, "application/json", "{\"status\":\"error\",\"msg\":\"Invalid data\"}");
        }
    } else {
        server.send(400, "application/json", "{\"status\":\"error\",\"msg\":\"No data\"}");
    }
}

void handleUpdateValue() {
    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        int indexStart = body.indexOf("\"index\":") + 8;
        int indexEnd = body.indexOf(",", indexStart);
        int valueStart = body.indexOf("\"value\":") + 8;
        int valueEnd = body.indexOf("}", valueStart);
        
        if (indexStart > 7 && valueStart > 7) {
            int index = body.substring(indexStart, indexEnd).toInt();
            int value = body.substring(valueStart, valueEnd).toInt();
            
            if (index >= 0 && index < (int)settings.size() && value >= MIN_VALUE && value <= MAX_VALUE) {
                settings[index].value = value;
                saveValuesToSD();
                needsSave = false;
                server.send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                server.send(400, "application/json", "{\"status\":\"error\",\"msg\":\"Invalid index or value\"}");
            }
        } else {
            server.send(400, "application/json", "{\"status\":\"error\",\"msg\":\"Invalid data\"}");
        }
    } else {
        server.send(400, "application/json", "{\"status\":\"error\",\"msg\":\"No data\"}");
    }
}

void handleDeleteCoffee() {
    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        int indexStart = body.indexOf("\"index\":") + 8;
        int indexEnd = body.indexOf("}", indexStart);
        
        if (indexStart > 7) {
            int index = body.substring(indexStart, indexEnd).toInt();
            
            if (index >= 0 && index < (int)settings.size()) {
                settings.erase(settings.begin() + index);
                if (selectedIndex >= (int)settings.size()) {
                    selectedIndex = settings.size() > 0 ? settings.size() - 1 : 0;
                }
                saveValuesToSD();
                needsSave = false;
                server.send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                server.send(400, "application/json", "{\"status\":\"error\",\"msg\":\"Invalid index\"}");
            }
        } else {
            server.send(400, "application/json", "{\"status\":\"error\",\"msg\":\"Invalid data\"}");
        }
    } else {
        server.send(400, "application/json", "{\"status\":\"error\",\"msg\":\"No data\"}");
    }
}

void handleSync() {
    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        
        // Parse JSON: {"entries":[{"index":0,"name":"Coffee","value":25},...]}
        int entriesStart = body.indexOf("\"entries\":[");
        if (entriesStart > 0) {
            entriesStart = body.indexOf("[", entriesStart) + 1;
            int entriesEnd = body.lastIndexOf("]");
            
            if (entriesStart > 0 && entriesEnd > entriesStart) {
                String entriesStr = body.substring(entriesStart, entriesEnd);
                
                // Clear current settings
                settings.clear();
                
                // Parse each entry
                int pos = 0;
                while (pos < entriesStr.length() && settings.size() < MAX_ENTRIES) {
                    int entryStart = entriesStr.indexOf("{", pos);
                    if (entryStart == -1) break;
                    int entryEnd = entriesStr.indexOf("}", entryStart);
                    if (entryEnd == -1) break;
                    
                    String entry = entriesStr.substring(entryStart, entryEnd + 1);
                    
                    // Extract name
                    int nameStart = entry.indexOf("\"name\":\"") + 8;
                    int nameEnd = entry.indexOf("\"", nameStart);
                    String name = "";
                    if (nameStart > 7 && nameEnd > nameStart) {
                        name = entry.substring(nameStart, nameEnd);
                    }
                    
                    // Extract value
                    int valueStart = entry.indexOf("\"value\":") + 8;
                    int valueEnd = entry.indexOf(",", valueStart);
                    if (valueEnd == -1) valueEnd = entry.indexOf("}", valueStart);
                    int value = 0;
                    if (valueStart > 7) {
                        value = entry.substring(valueStart, valueEnd).toInt();
                    }
                    
                    if (name.length() > 0) {
                        settings.push_back({name, value});
                    }
                    
                    pos = entryEnd + 1;
                }
                
                // Save to SD and update display
                saveValuesToSD();
                needsSave = false;
                
                // Update display with full refresh for e-paper
                if (M5.Display.isEPD()) {
                    M5.Display.setEpdMode(epd_mode_t::epd_quality);
                }
                M5.Display.startWrite();
                drawUI();
                M5.Display.endWrite();
                M5.Display.display();
                if (M5.Display.isEPD()) {
                    M5.Display.setEpdMode(epd_mode_t::epd_fastest);
                }
                
                server.send(200, "application/json", "{\"status\":\"ok\"}");
                return;
            }
        }
    }
    server.send(400, "application/json", "{\"status\":\"error\",\"msg\":\"Invalid data\"}");
}

