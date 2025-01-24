
#include "ESP_I2S.h"
#include "FS.h"
#include <OpenAI.h>
#include <WiFi.h>

const char* ssid = "your wifi here";
const char* password = "your wifi password here";
const char* api_key = "your openai api key here";
OpenAI openai(api_key);

const uint8_t I2S_SCK = 6;
const uint8_t I2S_WS = 7;
const uint8_t I2S_DIN = 15;

void setup() {
  // Initialize the serial port
  delay(500);
  Serial.begin(115200);
  delay(500);

  // Create an instance of the I2SClass
  I2SClass i2s;

  // Create variables to store the audio data
  uint8_t *wav_buffer;
  size_t wav_size;

  Serial.println("Initializing I2S bus...");

  // Set up the pins used for audio input
  i2s.setPins(I2S_SCK, I2S_WS, -1, I2S_DIN);

  // Initialize the I2S bus in standard mode
  if (!i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
    Serial.println("Failed to initialize I2S bus!");
    return;
  }

  Serial.println("I2S bus initialized.");

  // Init WiFi connection
  WiFi.begin(ssid, password);
  Serial.print(F("Connecting"));
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println();
  Serial.println(F("WiFi inicializado y conectado."));

  Serial.println("Recording 5 seconds of audio data...");

  // Record 5 seconds of audio data
  wav_buffer = i2s.recordWAV(5, &wav_size);

  // Usar el buffer con OpenAI para transcripción
  OpenAI_AudioTranscription transcription = openai.audioTranscription();
  transcription.setLanguage("es");
  String result = transcription.file(wav_buffer, wav_size, OPENAI_AUDIO_INPUT_FORMAT_WAV);

  if (result.length() > 0) {
      Serial.println(F("Transcripción completada:"));
      Serial.println(result);
  } else {
      Serial.println(F("Error durante la transcripción"));
  }

  // Liberar la memoria del buffer
  free(wav_buffer);

  Serial.println("Application complete.");
}

void loop() {}
