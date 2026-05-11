#include <Arduino.h>
#include <BleKeyboard.h>

// --- Pin Definitions ---
// All pins support internal pull-ups. Buttons connect pin to GND.
#define PIN_FORWARD   25
#define PIN_BACKWARDS 26
#define PIN_BUTTON1   32
#define PIN_BUTTON2   33
#define PIN_BUTTON3   27

// --- Configuration ---
#define DEBOUNCE_MS        50
#define PAIRING_HOLD_MS    15000  // 15 seconds hold for pairing mode
#define RECONNECT_DELAY_MS 5000   // Wait before restarting BLE after disconnect
#define MULTI_PRESS_MS     400    // Max time between presses for multi-press detection
#define LONG_PRESS_MS      2000   // Long press threshold for Button 3

// --- Key Mappings ---
// Change these to remap buttons to different keys
#define KEY_MAP_FORWARD       KEY_RIGHT_ARROW
#define KEY_MAP_BACKWARDS     KEY_LEFT_ARROW
#define KEY_MAP_BUTTON1       KEY_RETURN
#define KEY_MAP_BUTTON2       KEY_ESC
#define KEY_MAP_BUTTON3_1X    KEY_F5       // Single press
#define KEY_MAP_BUTTON3_2X    KEY_F6       // Double press
#define KEY_MAP_BUTTON3_3X    KEY_F7       // Triple press
#define KEY_MAP_BUTTON3_LONG  KEY_MEDIA_WWW_HOME  // Android Home (AC Home consumer key)

// --- BLE Keyboard ---
BleKeyboard bleKeyboard("Rally Remote", "DIY Rally Controller", 100);

// --- Button State ---
struct Button {
  uint8_t pin;
  bool lastState;
  bool currentState;
  unsigned long lastDebounceTime;
  unsigned long pressStartTime;
  bool pressed;
};

Button buttons[] = {
  {PIN_FORWARD,   HIGH, HIGH, 0, 0, false},
  {PIN_BACKWARDS, HIGH, HIGH, 0, 0, false},
  {PIN_BUTTON1,   HIGH, HIGH, 0, 0, false},
  {PIN_BUTTON2,   HIGH, HIGH, 0, 0, false},
  {PIN_BUTTON3,   HIGH, HIGH, 0, 0, false},
};

#define NUM_BUTTONS (sizeof(buttons) / sizeof(buttons[0]))
#define BTN_IDX_FORWARD   0
#define BTN_IDX_BACKWARDS 1
#define BTN_IDX_BUTTON1   2
#define BTN_IDX_BUTTON2   3
#define BTN_IDX_BUTTON3   4

bool pairingTriggered = false;

// --- Functions ---
void updateButton(Button &btn) {
  bool reading = digitalRead(btn.pin);

  if (reading != btn.lastState) {
    btn.lastDebounceTime = millis();
  }

  if ((millis() - btn.lastDebounceTime) > DEBOUNCE_MS) {
    if (reading != btn.currentState) {
      btn.currentState = reading;
      // Button is active LOW (pressed = connected to GND)
      btn.pressed = (btn.currentState == LOW);

      if (btn.pressed) {
        btn.pressStartTime = millis();
      }
    }
  }

  btn.lastState = reading;
}

void enterPairingMode() {
  Serial.println("Entering pairing mode...");
  // Stop the current BLE keyboard and restart to allow new pairings
  bleKeyboard.end();
  delay(500);
  bleKeyboard.begin();
  Serial.println("Pairing mode active. Device is discoverable.");
}

void pressKey(uint8_t key) {
  if (bleKeyboard.isConnected()) {
    bleKeyboard.press(key);
  }
}

void releaseKey(uint8_t key) {
  if (bleKeyboard.isConnected()) {
    bleKeyboard.release(key);
  }
}

void sendMediaKey(const MediaKeyReport key) {
  if (bleKeyboard.isConnected()) {
    bleKeyboard.write(key);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);  // Wait for serial monitor to connect

  Serial.println("================================");
  Serial.println("  Rally Controller v1.0");
  Serial.println("  BLE HID Keyboard Device");
  Serial.println("================================");
  Serial.println();
  Serial.println("Pin configuration (all INPUT_PULLUP, buttons to GND):");
  Serial.printf("  Forward:   GPIO %d\n", PIN_FORWARD);
  Serial.printf("  Backwards: GPIO %d\n", PIN_BACKWARDS);
  Serial.printf("  Button 1:  GPIO %d\n", PIN_BUTTON1);
  Serial.printf("  Button 2:  GPIO %d\n", PIN_BUTTON2);
  Serial.printf("  Button 3:  GPIO %d\n", PIN_BUTTON3);
  Serial.println();

  // Configure button pins - all use internal pull-ups
  pinMode(PIN_FORWARD,   INPUT_PULLUP);
  pinMode(PIN_BACKWARDS, INPUT_PULLUP);
  pinMode(PIN_BUTTON1,   INPUT_PULLUP);
  pinMode(PIN_BUTTON2,   INPUT_PULLUP);
  pinMode(PIN_BUTTON3,   INPUT_PULLUP);

  Serial.println("Starting BLE Keyboard...");
  bleKeyboard.begin();
  Serial.println("BLE Keyboard started. Waiting for connection...");
  Serial.printf("Hold Button 3 for %d seconds to enter pairing mode.\n", PAIRING_HOLD_MS / 1000);
  Serial.println();
}

void loop() {
  // Update all button states
  for (size_t i = 0; i < NUM_BUTTONS; i++) {
    updateButton(buttons[i]);
  }

  // --- Check Button 3 for pairing mode (10s hold) ---
  if (buttons[BTN_IDX_BUTTON3].pressed) {
    unsigned long holdTime = millis() - buttons[BTN_IDX_BUTTON3].pressStartTime;
    if (holdTime >= PAIRING_HOLD_MS && !pairingTriggered) {
      pairingTriggered = true;
      enterPairingMode();
    }
  } else {
    pairingTriggered = false;
  }

  // --- Connection status tracking and reconnection ---
  static bool wasConnected = false;
  static unsigned long disconnectTime = 0;
  static bool reconnecting = false;
  bool isConnected = bleKeyboard.isConnected();

  if (isConnected && !wasConnected) {
    Serial.println("[BLE] Device connected!");
    reconnecting = false;
  } else if (!isConnected && wasConnected) {
    Serial.println("[BLE] Device disconnected. Will restart BLE in 5s...");
    disconnectTime = millis();
    reconnecting = false;
  }

  // Restart BLE after disconnect to ensure it re-advertises properly
  if (!isConnected && !reconnecting && wasConnected == false && disconnectTime > 0) {
    if (millis() - disconnectTime >= RECONNECT_DELAY_MS) {
      Serial.println("[BLE] Restarting BLE to re-advertise...");
      bleKeyboard.end();
      delay(500);
      bleKeyboard.begin();
      Serial.println("[BLE] BLE restarted. Waiting for connection...");
      reconnecting = true;
      disconnectTime = 0;
    }
  }

  wasConnected = isConnected;

  // --- Handle key presses (only when connected) ---
  if (isConnected) {
    // Forward button
    static bool fwdHeld = false;
    if (buttons[BTN_IDX_FORWARD].pressed && !fwdHeld) {
      Serial.println("[BTN] Forward pressed");
      pressKey(KEY_MAP_FORWARD);
      fwdHeld = true;
    } else if (!buttons[BTN_IDX_FORWARD].pressed && fwdHeld) {
      Serial.println("[BTN] Forward released");
      releaseKey(KEY_MAP_FORWARD);
      fwdHeld = false;
    }

    // Backwards button
    static bool bwdHeld = false;
    if (buttons[BTN_IDX_BACKWARDS].pressed && !bwdHeld) {
      Serial.println("[BTN] Backwards pressed");
      pressKey(KEY_MAP_BACKWARDS);
      bwdHeld = true;
    } else if (!buttons[BTN_IDX_BACKWARDS].pressed && bwdHeld) {
      Serial.println("[BTN] Backwards released");
      releaseKey(KEY_MAP_BACKWARDS);
      bwdHeld = false;
    }

    // Button 1
    static bool btn1Held = false;
    if (buttons[BTN_IDX_BUTTON1].pressed && !btn1Held) {
      Serial.println("[BTN] Button 1 pressed");
      pressKey(KEY_MAP_BUTTON1);
      btn1Held = true;
    } else if (!buttons[BTN_IDX_BUTTON1].pressed && btn1Held) {
      Serial.println("[BTN] Button 1 released");
      releaseKey(KEY_MAP_BUTTON1);
      btn1Held = false;
    }

    // Button 2
    static bool btn2Held = false;
    if (buttons[BTN_IDX_BUTTON2].pressed && !btn2Held) {
      Serial.println("[BTN] Button 2 pressed");
      pressKey(KEY_MAP_BUTTON2);
      btn2Held = true;
    } else if (!buttons[BTN_IDX_BUTTON2].pressed && btn2Held) {
      Serial.println("[BTN] Button 2 released");
      releaseKey(KEY_MAP_BUTTON2);
      btn2Held = false;
    }

    // Button 3 - multi-press (only if pairing not triggered)
    static uint8_t btn3TapCount = 0;
    static unsigned long btn3LastTapTime = 0;
    static bool btn3WasPressed = false;
    static bool btn3LongPressSent = false;

    if (buttons[BTN_IDX_BUTTON3].pressed && !btn3WasPressed && !pairingTriggered) {
      btn3WasPressed = true;
      btn3LongPressSent = false;
      btn3TapCount++;
      btn3LastTapTime = millis();
      Serial.printf("[BTN] Button 3 tap %d\n", btn3TapCount);
    } else if (!buttons[BTN_IDX_BUTTON3].pressed && btn3WasPressed) {
      btn3WasPressed = false;
    }

    // Long press detection (3 seconds)
    if (buttons[BTN_IDX_BUTTON3].pressed && btn3WasPressed && !btn3LongPressSent && !pairingTriggered) {
      unsigned long holdTime = millis() - buttons[BTN_IDX_BUTTON3].pressStartTime;
      if (holdTime >= LONG_PRESS_MS) {
        Serial.println("[BTN] Button 3 long press -> Android Home");
        sendMediaKey(KEY_MAP_BUTTON3_LONG);
        btn3LongPressSent = true;
        btn3TapCount = 0;  // Cancel multi-tap
      }
    }

    // Fire multi-tap action after window expires (only if no long press)
    if (btn3TapCount > 0 && !btn3WasPressed && !btn3LongPressSent &&
        (millis() - btn3LastTapTime) >= MULTI_PRESS_MS) {
      uint8_t key;
      switch (btn3TapCount) {
        case 1:  key = KEY_MAP_BUTTON3_1X; break;
        case 2:  key = KEY_MAP_BUTTON3_2X; break;
        default: key = KEY_MAP_BUTTON3_3X; break;
      }
      Serial.printf("[BTN] Button 3 %dx -> key 0x%02X\n", btn3TapCount, key);
      bleKeyboard.write(key);
      btn3TapCount = 0;
    }
  } else {
    // Release all keys if we lose connection
    bleKeyboard.releaseAll();
  }

  // Yield more time to BLE stack when not connected
  if (isConnected) {
    delay(10);
  } else {
    delay(100);
  }
}