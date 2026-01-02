// LunaFace.h - Cute AI Assistant Face Library
// Designed for ESP32-S3 + ST7789 240x280 Display

#ifndef LUNAFACE_H
#define LUNAFACE_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// --- Animation States ---
enum FaceState {
  FACE_IDLE,
  FACE_LISTENING,
  FACE_THINKING,
  FACE_TALKING
};

// --- Luna Face Class ---
class LunaFace {
public:
  Adafruit_ST7789* _tft;
  
  // Display dimensions
  static const int16_t DISPLAY_WIDTH = 280;
  static const int16_t DISPLAY_HEIGHT = 240;
  static const int16_t CENTER_X = DISPLAY_WIDTH / 2;
  static const int16_t CENTER_Y = DISPLAY_HEIGHT / 2;
  
  // Face parameters
  static const int16_t EYE_Y = CENTER_Y - 20;
  static const int16_t EYE_RADIUS = 32;
  static const int16_t EYE_OFFSET_X = 60;
  static const int16_t MOUTH_Y = EYE_Y + 55;
  static const int16_t BLUSH_Y = EYE_Y + 50;
  
  // Colors
  uint16_t _pink;
  uint16_t _blush;
  uint16_t _micColor;
  uint16_t _dotColor;
  
  // State tracking
  FaceState _currentState;
  unsigned long _lastBlink;
  unsigned long _lastTilt;
  unsigned long _nextBlinkDelay;
  unsigned long _nextTiltDelay;
  
  // Animation parameters
  static const int DOT_COUNT = 6;
  static const int DOT_RADIUS = 6;
  static const int ORBIT_RADIUS = 20;
  
  // Microphone bitmap (64x64)
  static const int BITMAP_WIDTH = 64;
  static const int BITMAP_HEIGHT = 64;
  static const unsigned char _micBitmap[] PROGMEM;
  
  // Internal drawing methods
  void drawEyes(int16_t cx, bool closed = false);
  void drawBlush(int16_t cx);
  void drawSmileMouth(int16_t cx);
  void drawOvalMouth(int16_t cx, int width, int height);
  void drawCircleMouth(int16_t cx, int radius);
  void clearMouthArea(int16_t cx);
  
public:
  LunaFace(Adafruit_ST7789* tft);
  
  // Initialization
  void begin();
  
  // Main face states
  void showIdle();
  void showListening();
  void showThinking();
  void showTalking(int duration = 3000);
  
  // Animations (called in loop)
  void update();  // Call this in loop() for automatic animations
  void blink();
  void tilt();
  
  // Utility
  void clearScreen();
  FaceState getState() { return _currentState; }
};

#endif // LUNAFACE_H