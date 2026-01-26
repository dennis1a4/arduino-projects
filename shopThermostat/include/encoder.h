#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>
#include "config.h"

// ============================================================================
// ROTARY ENCODER HANDLER
// ============================================================================

class EncoderHandler {
public:
    enum Event {
        EVENT_NONE,
        EVENT_ROTATE_CW,
        EVENT_ROTATE_CCW,
        EVENT_BUTTON_SHORT,
        EVENT_BUTTON_LONG,
        EVENT_BUTTON_VERY_LONG
    };

private:
    volatile int _encoderPos;
    volatile int _lastEncoderPos;
    volatile uint8_t _lastState;

    bool _buttonPressed;
    unsigned long _buttonPressTime;
    bool _longPressHandled;
    bool _veryLongPressHandled;

    // For edge detection
    int _lastPos;

public:
    EncoderHandler()
        : _encoderPos(0),
          _lastEncoderPos(0),
          _lastState(0),
          _buttonPressed(false),
          _buttonPressTime(0),
          _longPressHandled(false),
          _veryLongPressHandled(false),
          _lastPos(0) {}

    void begin() {
        pinMode(PIN_ENCODER_A, INPUT_PULLUP);
        pinMode(PIN_ENCODER_B, INPUT_PULLUP);
        pinMode(PIN_ENCODER_BTN, INPUT_PULLUP);

        // Read initial state
        _lastState = (digitalRead(PIN_ENCODER_A) << 1) | digitalRead(PIN_ENCODER_B);
        _lastPos = _encoderPos;

        Serial.println(F("Encoder initialized"));
    }

    // Call this from loop() frequently
    void update() {
        // Read encoder state
        uint8_t state = (digitalRead(PIN_ENCODER_A) << 1) | digitalRead(PIN_ENCODER_B);

        // Detect rotation using state table
        if (state != _lastState) {
            // State transition table for quadrature decoding
            // CW: 00 -> 01 -> 11 -> 10 -> 00
            // CCW: 00 -> 10 -> 11 -> 01 -> 00
            switch (_lastState) {
                case 0b00:
                    if (state == 0b01) _encoderPos++;
                    else if (state == 0b10) _encoderPos--;
                    break;
                case 0b01:
                    if (state == 0b11) _encoderPos++;
                    else if (state == 0b00) _encoderPos--;
                    break;
                case 0b11:
                    if (state == 0b10) _encoderPos++;
                    else if (state == 0b01) _encoderPos--;
                    break;
                case 0b10:
                    if (state == 0b00) _encoderPos++;
                    else if (state == 0b11) _encoderPos--;
                    break;
            }
            _lastState = state;
        }

        // Handle button
        bool buttonNow = (digitalRead(PIN_ENCODER_BTN) == LOW);

        if (buttonNow && !_buttonPressed) {
            // Button just pressed
            _buttonPressed = true;
            _buttonPressTime = millis();
            _longPressHandled = false;
            _veryLongPressHandled = false;
        } else if (!buttonNow && _buttonPressed) {
            // Button released
            _buttonPressed = false;
        }
    }

    Event getEvent() {
        // Check for rotation first
        int delta = _encoderPos - _lastPos;
        if (delta >= 4) {  // Use 4 steps per detent for typical encoders
            _lastPos = _encoderPos;
            return EVENT_ROTATE_CW;
        } else if (delta <= -4) {
            _lastPos = _encoderPos;
            return EVENT_ROTATE_CCW;
        }

        // Check button state
        if (_buttonPressed) {
            unsigned long pressDuration = millis() - _buttonPressTime;

            // Very long press (for AP mode)
            if (pressDuration > BUTTON_VERY_LONG_PRESS_MS && !_veryLongPressHandled) {
                _veryLongPressHandled = true;
                return EVENT_BUTTON_VERY_LONG;
            }

            // Long press (for menu)
            if (pressDuration > BUTTON_LONG_PRESS_MS && !_longPressHandled) {
                _longPressHandled = true;
                return EVENT_BUTTON_LONG;
            }
        } else {
            // Button was released
            if (_buttonPressTime > 0 && !_longPressHandled) {
                unsigned long pressDuration = millis() - _buttonPressTime;

                // Short press (only if didn't trigger long press)
                if (pressDuration > 50 && pressDuration < BUTTON_LONG_PRESS_MS) {
                    _buttonPressTime = 0;
                    return EVENT_BUTTON_SHORT;
                }
            }

            _buttonPressTime = 0;
        }

        return EVENT_NONE;
    }

    bool isButtonPressed() const {
        return _buttonPressed;
    }

    unsigned long getButtonPressDuration() const {
        if (_buttonPressed && _buttonPressTime > 0) {
            return millis() - _buttonPressTime;
        }
        return 0;
    }

    int getPosition() const {
        return _encoderPos / 4;  // Return detent positions
    }

    void resetPosition() {
        _encoderPos = 0;
        _lastPos = 0;
    }

    // For interrupt-based usage (optional, more responsive)
    void IRAM_ATTR handleInterrupt() {
        uint8_t state = (digitalRead(PIN_ENCODER_A) << 1) | digitalRead(PIN_ENCODER_B);

        if (state != _lastState) {
            switch (_lastState) {
                case 0b00:
                    if (state == 0b01) _encoderPos++;
                    else if (state == 0b10) _encoderPos--;
                    break;
                case 0b01:
                    if (state == 0b11) _encoderPos++;
                    else if (state == 0b00) _encoderPos--;
                    break;
                case 0b11:
                    if (state == 0b10) _encoderPos++;
                    else if (state == 0b01) _encoderPos--;
                    break;
                case 0b10:
                    if (state == 0b00) _encoderPos++;
                    else if (state == 0b11) _encoderPos--;
                    break;
            }
            _lastState = state;
        }
    }
};

#endif // ENCODER_H
