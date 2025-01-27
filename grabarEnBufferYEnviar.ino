#include <Arduino.h>
#include <driver/i2s.h>
#include "esp_heap_caps.h"
#include "FS.h"
#include "SD.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define SAMPLE_RATE 16000
#define BITS_PER_SAMPLE 32 // More BITS_PER_SAMPLE -> More audio quality. {8, 16, 24, 32}
#define BUFFER_SIZE 1024 // Tamaño del buffer en bytes
#define RECORD_TIME 10 // Duración máxima en segundos
#define BUTTON_PIN 20

#define I2S_WS  7 
#define I2S_SD  15  // DIN
#define I2S_SCK 6

const uint8_t sck = 12;
const uint8_t miso = 11;
const uint8_t mosi = 10;
const uint8_t cs = 9;

#define TIMEOUT_DEEPGRAM 10
#define LANGUAGE "es-ES"
WiFiClientSecure client;
const char* ssid = "";
const char* password = "";
const char* deepgramApiKey = "";

// Configuración de I2S
const i2s_port_t I2S_PORT = I2S_NUM_0;
i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = (i2s_bits_per_sample_t)BITS_PER_SAMPLE,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = BUFFER_SIZE / 2,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
};

i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
};

// Variables de almacenamiento en PSRAM
uint8_t *audio_buffer;
size_t max_samples = (SAMPLE_RATE * RECORD_TIME * (BITS_PER_SAMPLE / 8));
size_t recorded_bytes = 0;

void setup() {
  delay(500);
  Serial.begin(115200);
  delay(500);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);

  // Configurar I2S
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_zero_dma_buffer(I2S_PORT);

  // Reservar PSRAM para almacenar audio
  audio_buffer = (uint8_t *)heap_caps_malloc(max_samples, MALLOC_CAP_SPIRAM);
  if (!audio_buffer) {
      Serial.println(F("Error: No se pudo reservar PSRAM."));
      while (1);
  }
  Serial.println(F("PSRAM reservada correctamente."));

  // Inicializar SD
  SPI.begin(sck, miso, mosi, cs);
  if (!SD.begin(cs)) {
      Serial.println(F("Error: No se pudo inicializar la SD."));
      while (1);
  }
  Serial.println(F("SD inicializada correctamente."));

  WiFi.begin(ssid, password);
  Serial.print(F("Conectando a Wifi "));
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(F(". Hecho."));
}

void loop() {
    if (digitalRead(BUTTON_PIN) == LOW) {
        Serial.println(F("Grabando..."));
        digitalWrite(LED_BUILTIN, HIGH);

        recorded_bytes = 0;
        size_t bytes_read;

        while (recorded_bytes < max_samples && digitalRead(BUTTON_PIN) == LOW) {
            i2s_read(I2S_PORT, audio_buffer + recorded_bytes, BUFFER_SIZE, &bytes_read, portMAX_DELAY);
            recorded_bytes += bytes_read;
        }

        Serial.println(F("Grabación finalizada."));
        digitalWrite(LED_BUILTIN, LOW);

        generate_wav_header();
        save_to_sd(); // For debug purposes only (not necessary)
        send_to_deepgram();
    }
}

void generate_wav_header() {
    struct WAVHeader {
        char riff[4] = {'R', 'I', 'F', 'F'};
        uint32_t file_size;
        char wave[4] = {'W', 'A', 'V', 'E'};
        char fmt[4] = {'f', 'm', 't', ' '};
        uint32_t fmt_size = 16;
        uint16_t audio_format = 1;
        uint16_t num_channels = 1;
        uint32_t sample_rate = SAMPLE_RATE;
        uint32_t byte_rate;
        uint16_t block_align;
        uint16_t bits_per_sample = BITS_PER_SAMPLE;
        char data[4] = {'d', 'a', 't', 'a'};
        uint32_t data_size;
    } header;

    header.file_size = recorded_bytes + sizeof(WAVHeader) - 8;
    header.byte_rate = SAMPLE_RATE * BITS_PER_SAMPLE / 8;
    header.block_align = BITS_PER_SAMPLE / 8;
    header.data_size = recorded_bytes;

    // Escribir encabezado WAV al inicio del buffer
    memmove(audio_buffer + sizeof(WAVHeader), audio_buffer, recorded_bytes);
    memcpy(audio_buffer, &header, sizeof(WAVHeader));
    recorded_bytes += sizeof(WAVHeader);

    Serial.println(F("Archivo WAV generado en memoria."));
}

void save_to_sd() {
    File file = SD.open("/audio.wav", FILE_WRITE);
    if (!file) {
        Serial.println(F("Error al abrir archivo en SD"));
        return;
    }

    file.write(audio_buffer, recorded_bytes);
    file.close();
    Serial.println(F("Archivo WAV guardado en SD."));
}

void send_to_deepgram() {
    if (!client.connected()) {
        Serial.print(F("Conectando con Deepgram... "));
        client.setInsecure();
        if (!client.connect("api.deepgram.com", 443)) {
            Serial.println(F("Error al conectar con Deepgram."));
            return;
        }
    }
    Serial.println(F("Conectado a Deepgram."));

    String optional_param = "?model=nova-2-general&language=" + String(LANGUAGE) + "&smart_format=true&numerals=true";
    client.println("POST /v1/listen" + optional_param + " HTTP/1.1");
    client.println("Host: api.deepgram.com");
    client.println("Authorization: Token " + String(deepgramApiKey));
    client.println("Content-Type: audio/wav");
    client.println("Transfer-Encoding: chunked");
    client.println();

    Serial.println(F("Cabecera enviada. Enviado audio..."));

    size_t chunkSize = 1024;
    size_t bytesSent = 0;
    while (bytesSent < recorded_bytes) {
        size_t remaining = recorded_bytes - bytesSent;
        size_t toSend = (remaining < chunkSize) ? remaining : chunkSize;
        client.print(String(toSend, HEX) + "\r\n");
        client.write(audio_buffer + bytesSent, toSend);
        client.print("\r\n");
        bytesSent += toSend;
    }
    client.print("0\r\n\r\n");
    Serial.println(F("Audio enviado a Deepgram en fragmentos."));

    // Esperar a que haya datos en la respuesta
    unsigned long timeoutStart = millis();
    while (!client.available() && millis() - timeoutStart < TIMEOUT_DEEPGRAM * 1000) {
        delay(100);
    }

    // Leer la respuesta si está disponible
    String response = "";
    while (client.available()) {
        char c = client.read();
        response += c;
    }

    String transcription = json_object( response, "\"transcript\":" );
    //String language      = json_object( response, "\"detected_language\":" );
    String wavduration   = json_object( response, "\"duration\":" );
    Serial.println("Respuesta de Deepgram: " + transcription);
    Serial.println("Duracion del audio: " + wavduration + " segundos");
    Serial.println("Peso del audio: " + String(recorded_bytes/1000) + "KB");

}

String json_object( String input, String element )
{ String content = "";
  int pos_start = input.indexOf(element);      
  if (pos_start > 0)                                      // if element found:
  {  pos_start += element.length();                       // pos_start points now to begin of element content     
     int pos_end = input.indexOf( ",\"", pos_start);      // pos_end points to ," (start of next element)  
     if (pos_end > pos_start)                             // memo: "garden".substring(from3,to4) is 1 char "d" ..
     { content = input.substring(pos_start,pos_end);      // .. thats why we use for 'to' the pos_end (on ," ):
     } content.trim();                                    // remove optional spaces between the json objects
     if (content.startsWith("\""))                        // String objects typically start & end with quotation marks "    
     { content=content.substring(1,content.length()-1);   // remove both existing quotation marks (if exist)
     }     
  }  
  return (content);
}
