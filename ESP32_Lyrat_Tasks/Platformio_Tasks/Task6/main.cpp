#include <Arduino.h>
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

// LyraT Mini specific pins [citation:6]
#define PJ_DET   19   // Headphone detection (LOW = plugged)
#define PA_CTRL  21   // Amplifier control
#define LED      22   // LED indicator

// I2C pins
#define I2C_SDA  18
#define I2C_SCL  23

// I2S pins for ES8311
#define I2S_MCLK 0
#define I2S_BCLK 5
#define I2S_WS   25
#define I2S_DOUT 26

// Audio configuration
AudioInfo info(16000, 1, 16);  // 16kHz, mono, 16-bit
SineWaveGenerator<int16_t> sineWave(16000);
GeneratedSoundStream<int16_t> sound(sineWave);
DriverPins lyrat_pins;
AudioBoard board(AudioDriverES8311, lyrat_pins);
AudioBoardStream out(board);
StreamCopy copier(out, sound);

void setup() {
    Serial.begin(115200);
    while (!Serial);
    
    // Enable logging
    AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
    
    Serial.println("\n========================================");
    Serial.println("HEADPHONE OUTPUT TEST - LyraT Mini");
    Serial.println("========================================");
    
    // Configure GPIOs
    pinMode(PJ_DET, INPUT_PULLUP);
    pinMode(PA_CTRL, OUTPUT);
    pinMode(LED, OUTPUT);
    
    // Check headphones
    bool headphones_plugged = (digitalRead(PJ_DET) == LOW);
    Serial.printf("PJ_DET = %d - %s\n", digitalRead(PJ_DET),
                  headphones_plugged ? "HEADPHONES PLUGGED ✓" : "NO HEADPHONES ✗");
    
    if (headphones_plugged) {
        digitalWrite(PA_CTRL, HIGH);  // ENABLE amplifier for headphones
        Serial.println("PA_CTRL = HIGH - Amplifier ENABLED");
    } else {
        digitalWrite(PA_CTRL, LOW);
        Serial.println("PA_CTRL = LOW - Amplifier DISABLED");
        Serial.println("Please plug in headphones and reset");
        while(1) delay(1000);
    }
    
    // Initialize I2C
    Wire.begin(I2C_SDA, I2C_SCL);
    
    // Setup pins for LyraT Mini
    lyrat_pins.addI2C(audio_driver::PinFunction::CODEC, I2C_SDA, I2C_SCL);
    lyrat_pins.addI2S(audio_driver::PinFunction::CODEC, 
                      I2S_MCLK, I2S_BCLK, I2S_WS, I2S_DOUT, -1);
    
    // Start ES8311
    Serial.println("\nInitializing ES8311...");
    auto config = out.defaultConfig(TX_MODE);
    config.copyFrom(info);
    
    if (!out.begin(config)) {
        Serial.println("❌ Failed to initialize ES8311!");
        while(1) delay(1000);
    }
    
    Serial.println("✅ ES8311 initialized successfully!");
    
    // Start 440Hz tone
    sineWave.begin(info, 440.0);
    Serial.println("🎵 Playing 440Hz test tone...");
    Serial.println("========================================\n");
}

void loop() {
    if (digitalRead(PJ_DET) == LOW) {
        copier.copy();  // Play audio
        
        // Blink LED
        static unsigned long lastBlink = 0;
        static bool ledState = false;
        if (millis() - lastBlink > 500) {
            ledState = !ledState;
            digitalWrite(LED, ledState);
            lastBlink = millis();
        }
    } else {
        digitalWrite(LED, LOW);
        digitalWrite(PA_CTRL, LOW);
        delay(100);
    }
    
    delay(10);
}