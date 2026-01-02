#include <WiFi.h>
#include <ArduinoGPTChat.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "Audio.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <WeatherIcons.h>
#include "LunaFace.h"
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Adafruit_NeoPixel.h>

// NeoPixel configuration
#define LED_PIN   48
#define NUM_LEDS  1

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Define I2S pins for audio output
#define I2S_DOUT 47
#define I2S_BCLK 21
#define I2S_LRC 45

// Define INMP441 microphone input pins
#define I2S_MIC_SERIAL_CLOCK 5
#define I2S_MIC_LEFT_RIGHT_CLOCK 4
#define I2S_MIC_SERIAL_DATA 6

// Define button pins
#define TALK_BUTTON 1
#define SWITCH_BUTTON 2  // New button for window switching

// Sample rate for recording
#define SAMPLE_RATE 8000

// I2S configuration for microphone
#define I2S_MODE I2S_MODE_STD
#define I2S_BIT_WIDTH I2S_DATA_BIT_WIDTH_16BIT
#define I2S_SLOT_MODE I2S_SLOT_MODE_MONO
#define I2S_SLOT_MASK I2S_STD_SLOT_LEFT

// WiFi settings
const char* ssid = "your-wifi-ssid";
const char* password = "your-wifi-password";

// --- TFT Pinout ---
#define TFT_CS 10
#define TFT_RST 9
#define TFT_DC 8
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_MISO -1

// --- Color Palette ---
#define BG_COLOR 0x0841
#define USER_BUBBLE 0x2E8B57
#define GPT_BUBBLE 0x4A4A4A
#define TEXT_WHITE 0xFFFF
#define STATUS_BG 0x1082
#define CARD_BG 0x1082
#define ACCENT_BLUE 0x3C9F
#define STATUS_ORANGE 0xFD60
#define STATUS_GREEN 0x07E0
#define TEXT_PRIMARY 0xFFFF
#define TEXT_SECONDARY 0x8410
#define ACCENT_WARM 0xFD60
#define TEMP_COLOR 0xFFDF

// --- Display instance ---
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// --- LunaFace instance ---
LunaFace lunaFace(&tft);

// Global audio variable declaration
Audio audio;


// ByteDance ASR API configuration
//const char* asr_api_key = "07fcb4a5-b7b2-45d8-864a-8cc0292380df";

// Initialize ArduinoGPTChat instance
const char* apiKey = "sk-KkEHJ5tO1iiYIqr1jOmrH6FV2uagIICwzL0PDWarGIoHe3Zm";
const char* apiBaseUrl = "https://api.chatanywhere.tech";
ArduinoGPTChat gptChat(apiKey, apiBaseUrl);

// System prompt configuration
const char* systemPrompt = "you're a joyful gen Z named Luna,don't use emojis, responses should not exceed 30 words. IMPORTANT: When the user asks to control LEDs, include a command tag in your response using this exact format: [LED:COLOR:STATE] where COLOR can be RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, PINK, ORANGE, CORAL, PEACH, PURPLE, VIOLET, INDIGO, LAVENDER, HOTPINK, ROSE, SKYBLUE, NAVY, TEAL, TURQUOISE, LIME, MINT, EMERALD, OLIVE, GOLD, AMBER, BRONZE, WHITE, WARMWHITE, COOLWHITE, CREAM, CRIMSON, MAROON, MIDNIGHT, NEONGREEN, NEONPINK, NEONBLUE and STATE is ON or OFF. Example: 'Sure [LED:PURPLE:ON]' or 'going amber for you [LED:AMBER:ON]'.";
// --- WeatherAPI Configuration ---
const char* weatherApiKey = "your-weatherapi-api-key";
const char* city = "Kampala";
String weatherUrl = "http://api.weatherapi.com/v1/current.json?key=" + String(weatherApiKey) + "&q=" + String(city) + "&aqi=yes";

// --- NTP Configuration ---
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3 * 3600;
const int daylightOffset_sec = 0;

// Window switching variables
enum WindowMode {
  FACE_WINDOW,
  CHAT_WINDOW,
  CLOCK_WINDOW
};

WindowMode currentWindow = FACE_WINDOW;  
bool switchButtonPressed = false;
bool lastSwitchButtonState = false;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// Chat variables
bool buttonPressed = false;
bool wasButtonPressed = false;



#define MAX_MESSAGES 4
struct ChatMessage {
  String text;
  bool isUser;
  unsigned long timestamp;
};

ChatMessage chatHistory[MAX_MESSAGES];
int messageCount = 0;

String currentStatus = "Ready";
uint16_t statusColor = STATUS_GREEN;
unsigned long lastStatusUpdate = 0;

// Clock/Weather variables
unsigned long lastTimeUpdate = 0;
unsigned long lastWeatherUpdate = 0;
const unsigned long timeUpdateInterval = 1000;
const unsigned long weatherUpdateInterval = 1800000;

float temperature = 0;
float feelsLike = 0;
int humidity = 0;
int uvIndex = 0;
int aqi = 0;
float windSpeed = 0;
String weatherCondition = "";
bool isDay = true;

String prevTimeStr = "";
String prevSecStr = "";
String prevDateStr = "";
bool needsFullRedraw = true;

void addMessageToHistory(String text, bool isUser) {
  if (messageCount < MAX_MESSAGES) {
    chatHistory[messageCount].text = text;
    chatHistory[messageCount].isUser = isUser;
    chatHistory[messageCount].timestamp = millis();
    messageCount++;
  } else {
    // Shift messages up
    for (int i = 0; i < MAX_MESSAGES - 1; i++) {
      chatHistory[i] = chatHistory[i + 1];
    }
    chatHistory[MAX_MESSAGES - 1].text = text;
    chatHistory[MAX_MESSAGES - 1].isUser = isUser;
    chatHistory[MAX_MESSAGES - 1].timestamp = millis();
  }
}

void parseLEDCommand(String response) {
  // Look for [LED:COLOR:STATE] pattern
  int startIdx = response.indexOf("[LED:");
  if (startIdx == -1) return; // No command found
  
  int endIdx = response.indexOf("]", startIdx);
  if (endIdx == -1) return; // Malformed command
  
  // Extract the command: "LED:RED:ON"
  String command = response.substring(startIdx + 1, endIdx);
  
  // Split by colons
  int firstColon = command.indexOf(':');
  int secondColon = command.indexOf(':', firstColon + 1);
  
  if (firstColon == -1 || secondColon == -1) return;
  
  String colorStr = command.substring(firstColon + 1, secondColon);
  String stateStr = command.substring(secondColon + 1);
  
  colorStr.toUpperCase();
  stateStr.toUpperCase();
  
  Serial.println("LED Command detected:");
  Serial.println("Color: " + colorStr);
  Serial.println("State: " + stateStr);
  
  // Define colors - EXPANDED PALETTE
  uint32_t color = 0;
  
  // Primary colors
  if (colorStr == "RED") {
    color = strip.Color(255, 0, 0);
  } else if (colorStr == "GREEN") {
    color = strip.Color(0, 255, 0);
  } else if (colorStr == "BLUE") {
    color = strip.Color(0, 0, 255);
  } 
  // Secondary colors
  else if (colorStr == "YELLOW") {
    color = strip.Color(255, 255, 0);
  } else if (colorStr == "CYAN" || colorStr == "AQUA") {
    color = strip.Color(0, 255, 255);
  } else if (colorStr == "MAGENTA" || colorStr == "PINK") {
    color = strip.Color(255, 0, 255);
  }
  // Orange tones
  else if (colorStr == "ORANGE") {
    color = strip.Color(255, 165, 0);
  } else if (colorStr == "CORAL") {
    color = strip.Color(255, 127, 80);
  } else if (colorStr == "PEACH") {
    color = strip.Color(255, 218, 185);
  }
  // Purple tones
  else if (colorStr == "PURPLE") {
    color = strip.Color(128, 0, 128);
  } else if (colorStr == "VIOLET") {
    color = strip.Color(138, 43, 226);
  } else if (colorStr == "INDIGO") {
    color = strip.Color(75, 0, 130);
  } else if (colorStr == "LAVENDER") {
    color = strip.Color(230, 230, 250);
  }
  // Pink tones
  else if (colorStr == "HOTPINK") {
    color = strip.Color(255, 105, 180);
  } else if (colorStr == "ROSE") {
    color = strip.Color(255, 0, 127);
  }
  // Blue tones
  else if (colorStr == "SKYBLUE" || colorStr == "SKY") {
    color = strip.Color(135, 206, 235);
  } else if (colorStr == "NAVY") {
    color = strip.Color(0, 0, 128);
  } else if (colorStr == "TEAL") {
    color = strip.Color(0, 128, 128);
  } else if (colorStr == "TURQUOISE") {
    color = strip.Color(64, 224, 208);
  }
  // Green tones
  else if (colorStr == "LIME") {
    color = strip.Color(0, 255, 0);
  } else if (colorStr == "MINT") {
    color = strip.Color(189, 252, 201);
  } else if (colorStr == "EMERALD") {
    color = strip.Color(80, 200, 120);
  } else if (colorStr == "OLIVE") {
    color = strip.Color(128, 128, 0);
  }
  // Warm tones
  else if (colorStr == "GOLD") {
    color = strip.Color(255, 215, 0);
  } else if (colorStr == "AMBER") {
    color = strip.Color(255, 191, 0);
  } else if (colorStr == "BRONZE") {
    color = strip.Color(205, 127, 50);
  }
  // Neutral/White tones
  else if (colorStr == "WHITE") {
    color = strip.Color(255, 255, 255);
  } else if (colorStr == "WARM" || colorStr == "WARMWHITE") {
    color = strip.Color(255, 240, 200);
  } else if (colorStr == "COOL" || colorStr == "COOLWHITE") {
    color = strip.Color(200, 220, 255);
  } else if (colorStr == "CREAM") {
    color = strip.Color(255, 253, 208);
  }
  // Dark/dramatic colors
  else if (colorStr == "CRIMSON") {
    color = strip.Color(220, 20, 60);
  } else if (colorStr == "MAROON") {
    color = strip.Color(128, 0, 0);
  } else if (colorStr == "MIDNIGHT") {
    color = strip.Color(25, 25, 112);
  }
  // Fun/vibrant colors
  else if (colorStr == "NEON" || colorStr == "NEONGREEN") {
    color = strip.Color(57, 255, 20);
  } else if (colorStr == "NEONPINK") {
    color = strip.Color(255, 16, 240);
  } else if (colorStr == "NEONBLUE") {
    color = strip.Color(77, 77, 255);
  }
  else {
    Serial.println("Unknown color: " + colorStr);
    return;
  }
  
  // Set LED state
  if (stateStr == "ON") {
    strip.setPixelColor(0, color);
    strip.show();
    Serial.println("LED turned ON");
  } else if (stateStr == "OFF") {
    strip.setPixelColor(0, 0); // Turn off
    strip.show();
    Serial.println("LED turned OFF");
  }
}

void setup() {
  Serial.begin(115200);

  // Initialize NeoPixel
strip.begin();
strip.show(); // Initialize all pixels to 'off'
  delay(1000);

  // Initialize TFT display
  tft.init(240, 280);
  tft.setRotation(1);
  tft.fillScreen(BG_COLOR);

  // Initialize LunaFace
  lunaFace.begin();  

  // Initialize buttons
  pinMode(TALK_BUTTON, INPUT_PULLUP);
  pinMode(SWITCH_BUTTON, INPUT_PULLUP);

  // Show connecting screen
  drawConnectingScreen();

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    delay(500);
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    // Set I2S output pins
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(100);

    // Set system prompt
    gptChat.setSystemPrompt(systemPrompt);

    // Initialize recording
    gptChat.initializeRecording(I2S_MIC_SERIAL_CLOCK, I2S_MIC_LEFT_RIGHT_CLOCK, I2S_MIC_SERIAL_DATA,
                               SAMPLE_RATE, I2S_MODE, I2S_BIT_WIDTH, I2S_SLOT_MODE, I2S_SLOT_MASK);

    // Initialize time
    tft.fillScreen(BG_COLOR);
    tft.setFont(&FreeSans12pt7b);
    tft.setTextColor(TEXT_PRIMARY);
    tft.setCursor(50, 130);
    tft.print("Syncing time");
    
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo)) {
      delay(500);
    }
    
    Serial.println("Time synchronized!");
    
    // Fetch initial weather
    tft.fillScreen(BG_COLOR);
    tft.setCursor(20, 130);
    tft.print("Loading weather");
    
    fetchWeatherData();

    // Draw initial chat interface
    tft.fillScreen(BG_COLOR);
    drawChatInterface();
    updateStatus("Ready - Hold button to talk", STATUS_GREEN);
  } else {
    updateStatus("WiFi Failed", 0xF800);
  }
}

void loop() {
  audio.loop();

  // Handle window switching button with debouncing
  bool switchReading = (digitalRead(SWITCH_BUTTON) == HIGH); // Active HIGH
  
  if (switchReading != lastSwitchButtonState) {
    lastDebounceTime = millis();
  }
  
 if ((millis() - lastDebounceTime) > debounceDelay) {
    if (switchReading != switchButtonPressed) {
      switchButtonPressed = switchReading;
      
      if (switchButtonPressed) {
        // Button just pressed - cycle through windows
        if (currentWindow == FACE_WINDOW) {
          currentWindow = CHAT_WINDOW;
          tft.fillScreen(BG_COLOR);
          drawChatInterface();
        } else if (currentWindow == CHAT_WINDOW) {
          currentWindow = CLOCK_WINDOW;
          needsFullRedraw = true;
          tft.fillScreen(BG_COLOR);
        } else {
          currentWindow = FACE_WINDOW;
          tft.fillScreen(BG_COLOR);
          lunaFace.showIdle();
        }
        delay(200);
      }
    }
  }
  
  lastSwitchButtonState = switchReading;

 // Execute window-specific code
  if (currentWindow == FACE_WINDOW) {
    runFaceWindow();
  } else if (currentWindow == CHAT_WINDOW) {
    runChatWindow();
  } else {
    runClockWindow();
  }

  delay(10);
}

void runChatWindow() {
  // Handle boot button for push-to-talk
  buttonPressed = (digitalRead(TALK_BUTTON) == HIGH);

  if (buttonPressed && !wasButtonPressed && !gptChat.isRecording()) {
    updateStatus("Recording...", STATUS_ORANGE);
    
    if (gptChat.startRecording()) {
      wasButtonPressed = true;
    }
  }
  else if (!buttonPressed && wasButtonPressed && gptChat.isRecording()) {
    updateStatus("Processing...", STATUS_ORANGE);
    
    String transcribedText = gptChat.stopRecordingAndProcess();

    if (transcribedText.length() > 0) {
      addMessage(transcribedText, true);
      
      updateStatus("Thinking...", ACCENT_BLUE);

      String response = gptChat.sendMessage(transcribedText);

      if (response != "") {
        // Parse for LED commands FIRST
        parseLEDCommand(response);

        addMessage(response, false);
        
        updateStatus("Speaking...", STATUS_ORANGE);

        if (response.length() > 0) {
          bool success = gptChat.textToSpeech(response);
          
          if (success) {
            updateStatus("Playing audio", STATUS_GREEN);
          } else {
            updateStatus("TTS Failed", 0xF800);
          }
        }
      } else {
        updateStatus("No response", 0xF800);
      }
    } else {
      updateStatus("No speech detected", 0xF800);
    }

    wasButtonPressed = false;
    
    delay(2000);
    updateStatus("Ready - Hold button", STATUS_GREEN);
  }
  else if (buttonPressed && gptChat.isRecording()) {
    gptChat.continueRecording();
  }
  else if (!buttonPressed && !gptChat.isRecording()) {
    wasButtonPressed = false;
  }

  if (millis() - lastStatusUpdate > 1000) {
    drawWiFiStatus();
    lastStatusUpdate = millis();
  }
}


void runFaceWindow() {
  // Update Luna's automatic animations (blinking, tilting)
  lunaFace.update();
  
  // Handle talk button for push-to-talk
  buttonPressed = (digitalRead(TALK_BUTTON) == HIGH);

  if (buttonPressed && !wasButtonPressed && !gptChat.isRecording()) {
    // Start recording - show listening face
    lunaFace.showListening();
    
    if (gptChat.startRecording()) {
      wasButtonPressed = true;
    }
  }
  else if (!buttonPressed && wasButtonPressed && gptChat.isRecording()) {
    // Button released - show thinking face with spinner immediately
    tft.fillScreen(lunaFace._pink);
    lunaFace.drawEyes(lunaFace.CENTER_X, false);
    lunaFace.drawBlush(lunaFace.CENTER_X);
    lunaFace.drawCircleMouth(lunaFace.CENTER_X, 15);
    
    // Start spinner immediately
    int16_t centerX = 280 - 50;
    int16_t centerY = 40;
    float angleStep = 360.0 / 6;
    int spinnerFrame = 0;
    
    // Keep spinner running for at least 20 frames before processing
    for (int i = 0; i < 20; i++) {
      tft.fillCircle(centerX, centerY, 28, lunaFace._pink);
      float rotationAngle = spinnerFrame * 6.0;
      for (int j = 0; j < 6; j++) {
        float angle = (rotationAngle + (j * angleStep)) * 3.14159 / 180.0;
        int dotX = centerX + 20 * cos(angle);
        int dotY = centerY + 20 * sin(angle);
        tft.fillCircle(dotX, dotY, 6, lunaFace._dotColor);
      }
      spinnerFrame++;
      delay(50);
    }
    
    // Process transcription (spinner keeps running via periodic updates)
    String transcribedText = "";
    unsigned long lastSpinnerUpdate = millis();
    
    // We'll update spinner every 50ms while waiting
    transcribedText = gptChat.stopRecordingAndProcess();
    
    if (transcribedText.length() > 0) {
      addMessageToHistory(transcribedText, true);
      Serial.print("You said: ");
      Serial.println(transcribedText);
      
      // Continue spinner while getting GPT response
      String response = "";
      unsigned long responseStart = millis();
      
      // Animate spinner while waiting for response
      while (response == "" && millis() - responseStart < 30000) {
        // Draw spinner
        tft.fillCircle(centerX, centerY, 28, lunaFace._pink);
        float rotationAngle = spinnerFrame * 6.0;
        for (int j = 0; j < 6; j++) {
          float angle = (rotationAngle + (j * angleStep)) * 3.14159 / 180.0;
          int dotX = centerX + 20 * cos(angle);
          int dotY = centerY + 20 * sin(angle);
          tft.fillCircle(dotX, dotY, 6, lunaFace._dotColor);
        }
        spinnerFrame++;
        
        // Try to get response
        response = gptChat.sendMessage(transcribedText);
        delay(50);
      }
      
      if (response != "") {
        // Parse for LED commands
        parseLEDCommand(response);
        addMessageToHistory(response, false);
        Serial.print("Luna says: ");
        Serial.println(response);
        
        // Continue spinner while generating TTS
        bool ttsSuccess = false;
        unsigned long ttsStart = millis();
        
        while (!ttsSuccess && millis() - ttsStart < 20000) {
          // Draw spinner
          tft.fillCircle(centerX, centerY, 28, lunaFace._pink);
          float rotationAngle = spinnerFrame * 6.0;
          for (int j = 0; j < 6; j++) {
            float angle = (rotationAngle + (j * angleStep)) * 3.14159 / 180.0;
            int dotX = centerX + 20 * cos(angle);
            int dotY = centerY + 20 * sin(angle);
            tft.fillCircle(dotX, dotY, 6, lunaFace._dotColor);
          }
          spinnerFrame++;
          
          ttsSuccess = gptChat.textToSpeech(response);
          delay(50);
        }
        
        // Clear spinner
        tft.fillCircle(centerX, centerY, 28, lunaFace._pink);
        
        // Now show talking animation WHILE audio is playing
        if (ttsSuccess) {
          tft.fillScreen(lunaFace._pink);
          lunaFace.drawEyes(lunaFace.CENTER_X, false);
          lunaFace.drawBlush(lunaFace.CENTER_X);
          
          // Animate mouth while audio is playing
          while (audio.isRunning()) {
            lunaFace.clearMouthArea(lunaFace.CENTER_X);
            int width = 36;
            int height = random(8, 28);
            lunaFace.drawOvalMouth(lunaFace.CENTER_X, width, height);
            audio.loop();
            delay(100);
          }
          
          // Return to smile after speaking
          lunaFace.clearMouthArea(lunaFace.CENTER_X);
          lunaFace.drawSmileMouth(lunaFace.CENTER_X);
        }
      }
    }
    
    wasButtonPressed = false;
    
    // Return to idle face
    delay(500);
    lunaFace.showIdle();
  }
  else if (buttonPressed && gptChat.isRecording()) {
    // Continue recording
    gptChat.continueRecording();
  }
  else if (!buttonPressed && !gptChat.isRecording()) {
    wasButtonPressed = false;
  }
}





void runClockWindow() {
  if (millis() - lastTimeUpdate >= timeUpdateInterval) {
    lastTimeUpdate = millis();
    displayTime();
    drawWiFiStatus();
  }
  
  if (millis() - lastWeatherUpdate >= weatherUpdateInterval) {
    lastWeatherUpdate = millis();
    fetchWeatherData();
    needsFullRedraw = true;
  }
}

// ============================================
// CHAT WINDOW FUNCTIONS
// ============================================

void drawConnectingScreen() {
  tft.fillScreen(BG_COLOR);
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextColor(TEXT_WHITE);
  tft.setCursor(40, 110);
  tft.print("AI Assistant");
  
  tft.setFont(&FreeSans9pt7b);
  tft.setCursor(60, 140);
  tft.print("Connecting...");
}

void drawChatInterface() {
  tft.fillScreen(BG_COLOR);
  
  drawRoundRect(5, 5, 270, 30, 8, STATUS_BG);
  
  tft.setFont(&FreeSansBold9pt7b);
  tft.setTextColor(TEXT_WHITE);
  tft.setCursor(15, 23);
  tft.print("ChatGPT Assistant");
  
  drawWiFiStatus();
  drawStatusBar();
  drawChatMessages();
}

void drawStatusBar() {
  int statusY = 205;
  
  tft.fillRect(0, statusY, 280, 35, BG_COLOR);
  drawRoundRect(5, statusY, 270, 28, 8, STATUS_BG);
  
  tft.fillCircle(15, statusY + 14, 5, statusColor);
  
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(TEXT_WHITE);
  tft.setCursor(28, statusY + 18);
  tft.print(currentStatus);
}

void drawChatMessages() {
  int chatAreaTop = 40;
  int chatAreaBottom = 200;
  
  tft.fillRect(5, chatAreaTop, 270, chatAreaBottom - chatAreaTop, BG_COLOR);
  
  int yPos = chatAreaBottom - 5;
  
  for (int i = messageCount - 1; i >= 0 && yPos > chatAreaTop; i--) {
    ChatMessage msg = chatHistory[i];
    
    int messageHeight = calculateMessageHeight(msg.text);
    
    if (yPos - messageHeight < chatAreaTop) break;
    
    yPos -= messageHeight;
    
    if (msg.isUser) {
      drawMessageBubble(msg.text, yPos, true);
    } else {
      drawMessageBubble(msg.text, yPos, false);
    }
    
    yPos -= 8;
  }
}

int calculateMessageHeight(String text) {
  int lines = (text.length() / 30) + 1;
  return lines * 12 + 10;
}

void drawMessageBubble(String text, int yPos, bool isUser) {
  uint16_t bubbleColor = isUser ? USER_BUBBLE : GPT_BUBBLE;
  int bubbleWidth = 180;
  int xPos = isUser ? 90 : 10;
  
  int bubbleHeight = calculateMessageHeight(text) - 2;
  
  drawRoundRect(xPos, yPos, bubbleWidth, bubbleHeight, 8, bubbleColor);
  
  tft.setFont();
  tft.setTextSize(1);
  tft.setTextColor(TEXT_WHITE);
  
  int textX = xPos + 6;
  int textY = yPos + 6;
  int maxWidth = bubbleWidth - 12;
  
  int currentX = textX;
  int currentY = textY;
  String word = "";
  
  for (int i = 0; i < text.length(); i++) {
    char c = text.charAt(i);
    
    if (c == ' ' || i == text.length() - 1) {
      if (i == text.length() - 1 && c != ' ') {
        word += c;
      }
      
      int wordWidth = word.length() * 6;
      
      if (currentX + wordWidth > textX + maxWidth) {
        currentX = textX;
        currentY += 9;
      }
      
      tft.setCursor(currentX, currentY);
      tft.print(word);
      currentX += wordWidth + 6;
      word = "";
    } else {
      word += c;
    }
  }
}

void addMessage(String text, bool isUser) {
  // Use the shared helper function
  addMessageToHistory(text, isUser);
  
  // Only redraw if we're in chat window
  if (currentWindow == CHAT_WINDOW) {
    drawChatMessages();
    drawStatusBar();
  }
}

void updateStatus(String status, uint16_t color) {
  currentStatus = status;
  statusColor = color;
  drawStatusBar();
}

// ============================================
// CLOCK/WEATHER WINDOW FUNCTIONS
// ============================================

void fetchWeatherData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(weatherUrl);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
      String payload = http.getString();
      DynamicJsonDocument doc(2048);
      DeserializationError error = deserializeJson(doc, payload);
      
      if (!error) {
        temperature = doc["current"]["temp_c"];
        feelsLike = doc["current"]["feelslike_c"];
        humidity = doc["current"]["humidity"];
        uvIndex = doc["current"]["uv"];
        windSpeed = doc["current"]["wind_kph"];
        weatherCondition = doc["current"]["condition"]["text"].as<String>();
        isDay = doc["current"]["is_day"].as<int>() == 1;
        
        if (doc["current"]["air_quality"]["us-epa-index"]) {
          aqi = doc["current"]["air_quality"]["us-epa-index"];
        }
        
        Serial.println("Weather data updated successfully!");
      }
    }
    http.end();
  }
  lastWeatherUpdate = millis();
}

String getAQIDescription(int aqi) {
  switch(aqi) {
    case 1: return "Good";
    case 2: return "Fair";
    case 3: return "Moderate";
    case 4: return "Poor";
    case 5: return "V.Poor";
    case 6: return "Severe";
    default: return "N/A";
  }
}

uint16_t getAQIColor(int aqi) {
  switch(aqi) {
    case 1: return 0x07E0;
    case 2: return 0xBFE0;
    case 3: return 0xFFE0;
    case 4: return 0xFD60;
    case 5: return 0xF800;
    case 6: return 0x9800;
    default: return TEXT_SECONDARY;
  }
}

void drawFullScreen() {
  tft.fillScreen(BG_COLOR);
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;
  
  char timeStr[10];
  char dateStr[20];
  char secStr[5];
  
  strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
  strftime(dateStr, sizeof(dateStr), "%a, %b %d", &timeinfo);
  strftime(secStr, sizeof(secStr), ":%S", &timeinfo);
  
  prevTimeStr = String(timeStr);
  prevDateStr = String(dateStr);
  prevSecStr = String(secStr);
  
  tft.setFont(&FreeSansBold24pt7b);
  tft.setTextColor(TEXT_PRIMARY);
  tft.setCursor(20, 45);
  tft.print(timeStr);
  
  tft.setFont(&FreeSansBold18pt7b);
  tft.setCursor(145, 45);
  tft.print(secStr);
  
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextColor(ACCENT_BLUE);
  tft.setCursor(10, 70);
  tft.print(city);
  
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(0xCE59);
  tft.setCursor(140, 70);
  tft.print(dateStr);
  
  drawWiFiStatus();
  
  tft.drawFastHLine(10, 78, 260, CARD_BG);
  
  int cardY = 85;
  drawRoundRect(10, cardY, 260, 95, 10, CARD_BG);
  
  bool isDaytime = isDay;
  const unsigned char* currentIcon = WeatherIcons::getIcon(weatherCondition, isDaytime);
  int iconX = 20;
  int iconY = cardY + 5;
  tft.drawBitmap(iconX, iconY, currentIcon, 
                 WEATHER_ICON_WIDTH, WEATHER_ICON_HEIGHT, ACCENT_WARM);
  
  tft.setFont(&FreeSansBold9pt7b);
  tft.setTextColor(0xFFDF);
  
  String displayCondition = weatherCondition;
  if (displayCondition.length() > 14) {
    displayCondition = displayCondition.substring(0, 14);
  }
  
  tft.setCursor(iconX, iconY + WEATHER_ICON_HEIGHT + 15);
  tft.print(displayCondition);
  
  tft.setFont(&FreeSans18pt7b);
  tft.setTextColor(TEMP_COLOR);
  tft.setCursor(150, cardY + 45);
  tft.print(String((int)temperature));
  
  tft.setFont(&FreeSans9pt7b);
  tft.setCursor(200, cardY + 35);
  tft.print("o");
  tft.setCursor(215, cardY + 42);
  tft.print("C");
  
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(ACCENT_BLUE);
  tft.setCursor(155, cardY + 73);
  tft.print("Feels ");
  tft.setTextColor(0xCE59);
  tft.print(String((int)feelsLike));
  tft.setCursor(230, cardY + 63);
  tft.print("o");
  tft.setCursor(235, cardY + 73);
  tft.print(" C");
  
  int bottomY = 187;
  int cardWidth = 62;
  int cardSpacing = 8;
  
  tft.setFont();
  
  drawRoundRect(10, bottomY, cardWidth, 55, 8, CARD_BG);
  tft.setTextSize(1);
  tft.setTextColor(ACCENT_BLUE);
  tft.setCursor(14, bottomY + 8);
  tft.print("HUMIDITY");
  tft.setTextSize(2);
  tft.setTextColor(TEXT_PRIMARY);
  tft.setCursor(20, bottomY + 28);
  tft.print(humidity);
  tft.setTextSize(1);
  tft.print("%");
  
  drawRoundRect(10 + cardWidth + cardSpacing, bottomY, cardWidth, 55, 8, CARD_BG);
  tft.setTextSize(1);
  tft.setTextColor(ACCENT_BLUE);
  tft.setCursor(84, bottomY + 8);
  tft.print("UV INDEX");
  tft.setTextSize(2);
  tft.setTextColor(TEXT_PRIMARY);
  tft.setCursor(96, bottomY + 28);
  tft.print(uvIndex);
  
  drawRoundRect(10 + (cardWidth + cardSpacing) * 2, bottomY, cardWidth, 55, 8, CARD_BG);
  tft.setTextSize(1);
  tft.setTextColor(ACCENT_BLUE);
  tft.setCursor(156, bottomY + 8);
  tft.print("WIND");
  tft.setTextSize(2);
  tft.setTextColor(TEXT_PRIMARY);
  tft.setCursor(156, bottomY + 24);
  tft.print(String((int)windSpeed));
  tft.setTextSize(1);
  tft.setCursor(156, bottomY + 40);
  tft.print("km/h");
  
  drawRoundRect(10 + (cardWidth + cardSpacing) * 3, bottomY, cardWidth, 55, 8, CARD_BG);
  tft.setTextSize(1);
  tft.setTextColor(ACCENT_BLUE);
  tft.setCursor(232, bottomY + 8);
  tft.print("AQI");
  tft.setTextSize(1);
  tft.setTextColor(getAQIColor(aqi));
  tft.setCursor(223, bottomY + 28);
  tft.print(getAQIDescription(aqi));
  
  Serial.println("=== Full Screen Draw ===");
}

void displayTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;
  
  char timeStr[10];
  char secStr[5];
  char dateStr[20];
  
  strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
  strftime(secStr, sizeof(secStr), ":%S", &timeinfo);
  strftime(dateStr, sizeof(dateStr), "%a, %b %d", &timeinfo);
  
  String currentTimeStr = String(timeStr);
  String currentSecStr = String(secStr);
  String currentDateStr = String(dateStr);
  
  if (needsFullRedraw) {
    drawFullScreen();
    needsFullRedraw = false;
    return;
  }
  
  if (currentSecStr != prevSecStr) {
    int16_t x1, y1;
    uint16_t w, h;
    
    tft.setFont(&FreeSans18pt7b);
    tft.getTextBounds(prevSecStr.c_str(), 145, 45, &x1, &y1, &w, &h);
    
    tft.fillRect(x1 - 2, y1 - 2, w + 4, h + 4, BG_COLOR);
    
    tft.setTextColor(TEXT_PRIMARY);
    tft.setCursor(145, 45);
    tft.print(currentSecStr);
    
    prevSecStr = currentSecStr;
  }
  
  if (currentTimeStr != prevTimeStr) {
    int16_t x1, y1;
    uint16_t w, h;
    
    tft.setFont(&FreeSansBold24pt7b);
    tft.getTextBounds(prevTimeStr.c_str(), 20, 45, &x1, &y1, &w, &h);
    
    tft.fillRect(x1 - 2, y1 - 2, w + 4, h + 4, BG_COLOR);
    
    tft.setTextColor(TEXT_PRIMARY);
    tft.setCursor(20, 45);
    tft.print(currentTimeStr);
    
    prevTimeStr = currentTimeStr;
  }
  
  if (currentDateStr != prevDateStr) {
    int16_t x1, y1;
    uint16_t w, h;
    
    tft.setFont(&FreeSans9pt7b);
    tft.getTextBounds(prevDateStr.c_str(), 140, 70, &x1, &y1, &w, &h);
    
    tft.fillRect(x1 - 2, y1 - 2, w + 4, h + 4, BG_COLOR);
    
    tft.setTextColor(0xCE59);
    tft.setCursor(140, 70);
    tft.print(currentDateStr);
    
    prevDateStr = currentDateStr;
  }
}

// ============================================
// SHARED UTILITY FUNCTIONS
// ============================================

void drawRoundRect(int x, int y, int w, int h, int r, uint16_t color) {
  tft.fillRect(x + r, y, w - 2 * r, h, color);
  tft.fillRect(x, y + r, w, h - 2 * r, color);
  tft.fillCircle(x + r, y + r, r, color);
  tft.fillCircle(x + w - r - 1, y + r, r, color);
  tft.fillCircle(x + r, y + h - r - 1, r, color);
  tft.fillCircle(x + w - r - 1, y + h - r - 1, r, color);
}

void drawWiFiStatus() {
  int dotX = 255;
  int dotY = 18;
  int dotRadius = 4;
  
  uint16_t dotColor;
  if (WiFi.status() == WL_CONNECTED) {
    dotColor = STATUS_GREEN;
  } else {
    dotColor = 0xF800;
  }
  
  tft.fillCircle(dotX, dotY, dotRadius, dotColor);
}
