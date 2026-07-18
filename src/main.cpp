#include <Arduino.h>
#include <BleGamepad.h>

// --- Pin Definitions ---
// All pins support internal pull-ups. Buttons connect pin to GND.
#define PIN_FORWARD   4
#define PIN_BACKWARDS 5
#define PIN_BUTTON1   2
#define PIN_BUTTON2   3
#define PIN_BUTTON3   6

// --- Configuration ---
#define DEBOUNCE_MS        50
#define PAIRING_HOLD_MS    15000  // 15 seconds hold for pairing mode
#define RECONNECT_DELAY_MS 5000   // Wait before restarting BLE after disconnect
#define MULTI_PRESS_MS     400    // Max time between presses for multi-press detection
#define LONG_PRESS_MS      2000   // Long press threshold for Button 3
#define BUTTON3_MULTIPRESS 0          // 1 = multi/long press detection, 0 = simple button

// --- Gamepad Button Mappings ---
// Change these to remap buttons to different gamepad buttons (1-indexed)
#define GAMEPAD_BTN_BUTTON1       BUTTON_1   // A
#define GAMEPAD_BTN_BUTTON2       BUTTON_2   // B
#define GAMEPAD_BTN_BUTTON3_1X    BUTTON_3   // X  (single press)
#define GAMEPAD_BTN_BUTTON3_2X    BUTTON_4   // Y  (double press)
#define GAMEPAD_BTN_BUTTON3_3X    BUTTON_5   // LB (triple press)
#define GAMEPAD_BTN_BUTTON3_LONG  BUTTON_6   // RB (long press)

// Hat switch directions for Forward/Backwards
#define HAT_FORWARD   HAT_RIGHT
#define HAT_BACKWARDS HAT_LEFT

// --- BLE Gamepad ---
BleGamepad bleGamepad("Rally Remote", "DIY Rally Controller", 100);

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
  bleGamepad.end();
  delay(500);
  BleGamepadConfiguration config;
  config.setButtonCount(6);
  config.setHatSwitchCount(1);
  config.setAutoReport(false);
  bleGamepad.begin(&config);
  Serial.println("Pairing mode active. Device is discoverable.");
}

void setup() {
  Serial.begin(115200);
  delay(1000);  // Wait for serial monitor to connect

  Serial.println("================================");
  Serial.println("  Rally Controller v2.0");
  Serial.println("  BLE HID Gamepad Device");
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

  Serial.println("Starting BLE Gamepad...");
  BleGamepadConfiguration config;
  config.setButtonCount(6);
  config.setHatSwitchCount(1);
  config.setAutoReport(false);
  bleGamepad.begin(&config);
  Serial.println("BLE Gamepad started. Waiting for connection...");
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
  bool isConnected = bleGamepad.isConnected();

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
      bleGamepad.end();
      delay(500);
      BleGamepadConfiguration config;
      config.setButtonCount(6);
      config.setHatSwitchCount(1);
      config.setAutoReport(false);
      bleGamepad.begin(&config);
      Serial.println("[BLE] BLE restarted. Waiting for connection...");
      reconnecting = true;
      disconnectTime = 0;
    }
  }

  wasConnected = isConnected;

  // --- Handle button presses (only when connected) ---
  if (isConnected) {
    bool reportNeeded = false;

    // Forward button -> Hat switch right
    static bool fwdHeld = false;
    if (buttons[BTN_IDX_FORWARD].pressed && !fwdHeld) {
      Serial.println("[BTN] Forward pressed");
      bleGamepad.setHat1(HAT_FORWARD);
      fwdHeld = true;
      reportNeeded = true;
    } else if (!buttons[BTN_IDX_FORWARD].pressed && fwdHeld) {
      Serial.println("[BTN] Forward released");
      // Only release hat if backwards isn't held
      if (!buttons[BTN_IDX_BACKWARDS].pressed) {
        bleGamepad.setHat1(HAT_CENTERED);
      }
      fwdHeld = false;
      reportNeeded = true;
    }

    // Backwards button -> Hat switch left
    static bool bwdHeld = false;
    if (buttons[BTN_IDX_BACKWARDS].pressed && !bwdHeld) {
      Serial.println("[BTN] Backwards pressed");
      bleGamepad.setHat1(HAT_BACKWARDS);
      bwdHeld = true;
      reportNeeded = true;
    } else if (!buttons[BTN_IDX_BACKWARDS].pressed && bwdHeld) {
      Serial.println("[BTN] Backwards released");
      // Only release hat if forward isn't held
      if (!buttons[BTN_IDX_FORWARD].pressed) {
        bleGamepad.setHat1(HAT_CENTERED);
      }
      bwdHeld = false;
      reportNeeded = true;
    }

    // Button 1
    static bool btn1Held = false;
    if (buttons[BTN_IDX_BUTTON1].pressed && !btn1Held) {
      Serial.println("[BTN] Button 1 pressed");
      bleGamepad.press(GAMEPAD_BTN_BUTTON1);
      btn1Held = true;
      reportNeeded = true;
    } else if (!buttons[BTN_IDX_BUTTON1].pressed && btn1Held) {
      Serial.println("[BTN] Button 1 released");
      bleGamepad.release(GAMEPAD_BTN_BUTTON1);
      btn1Held = false;
      reportNeeded = true;
    }

    // Button 2
    static bool btn2Held = false;
    if (buttons[BTN_IDX_BUTTON2].pressed && !btn2Held) {
      Serial.println("[BTN] Button 2 pressed");
      bleGamepad.press(GAMEPAD_BTN_BUTTON2);
      btn2Held = true;
      reportNeeded = true;
    } else if (!buttons[BTN_IDX_BUTTON2].pressed && btn2Held) {
      Serial.println("[BTN] Button 2 released");
      bleGamepad.release(GAMEPAD_BTN_BUTTON2);
      btn2Held = false;
      reportNeeded = true;
    }

#if BUTTON3_MULTIPRESS
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

    // Long press detection
    if (buttons[BTN_IDX_BUTTON3].pressed && btn3WasPressed && !btn3LongPressSent && !pairingTriggered) {
      unsigned long holdTime = millis() - buttons[BTN_IDX_BUTTON3].pressStartTime;
      if (holdTime >= LONG_PRESS_MS) {
        Serial.println("[BTN] Button 3 long press -> Gamepad Button 6");
        bleGamepad.press(GAMEPAD_BTN_BUTTON3_LONG);
        bleGamepad.sendReport();
        delay(50);
        bleGamepad.release(GAMEPAD_BTN_BUTTON3_LONG);
        bleGamepad.sendReport();
        btn3LongPressSent = true;
        btn3TapCount = 0;  // Cancel multi-tap
      }
    }

    // Fire multi-tap action after window expires (only if no long press)
    if (btn3TapCount > 0 && !btn3WasPressed && !btn3LongPressSent &&
        (millis() - btn3LastTapTime) >= MULTI_PRESS_MS) {
      uint8_t btn;
      switch (btn3TapCount) {
        case 1:  btn = GAMEPAD_BTN_BUTTON3_1X; break;
        case 2:  btn = GAMEPAD_BTN_BUTTON3_2X; break;
        default: btn = GAMEPAD_BTN_BUTTON3_3X; break;
      }
      Serial.printf("[BTN] Button 3 %dx -> Gamepad Button %d\n", btn3TapCount, btn);
      bleGamepad.press(btn);
      bleGamepad.sendReport();
      delay(50);
      bleGamepad.release(btn);
      reportNeeded = true;
      btn3TapCount = 0;
    }
#else
    // Button 3 - simple press
    static bool btn3Held = false;
    if (buttons[BTN_IDX_BUTTON3].pressed && !btn3Held && !pairingTriggered) {
      Serial.println("[BTN] Button 3 pressed");
      bleGamepad.press(GAMEPAD_BTN_BUTTON3_1X);
      btn3Held = true;
      reportNeeded = true;
    } else if (!buttons[BTN_IDX_BUTTON3].pressed && btn3Held) {
      Serial.println("[BTN] Button 3 released");
      bleGamepad.release(GAMEPAD_BTN_BUTTON3_1X);
      btn3Held = false;
      reportNeeded = true;
    }
#endif

    if (reportNeeded) {
      bleGamepad.sendReport();
    }
  } else {
    // Release all buttons if we lose connection
    bleGamepad.resetButtons();
  }

  // Yield more time to BLE stack when not connected
  if (isConnected) {
    delay(10);
  } else {
    delay(100);
  }
}