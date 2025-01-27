# Speech2Text using ESP32

This is a system made for ESP32, to translate from speech to text using third-party services like [Deepgram](https://deepgram.com/). 
You can also use the OpenAI Whisper model using [this library](https://github.com/me-no-dev/OpenAI-ESP32). There are no big notable differences between both services. You can get a Deepgram API key [here](https://console.deepgram.com/signup?jump=keys).

The code uses the PSRAM of the esp32 to temporarily save the audio and send it. I do this to not to depend on a microSD. 
To do this you must have an ESP32 with a large amount of PSRAM (I'm using an ESP32 S3) and activate it by Tools -> PSRAM -> OPI PSRAM on Arduino IDE.
