# Open RealtimeAPI Embedded SDK

This fork is specifically customized for Freenove Media Kit and can be directly used on it.

## Platform/Device Support

* [Freenove ESP32-S3-WROOM](https://www.amazon.com/gp/product/B0BMQ8F7FN)

## Installation & Usage

### ESP32-S3 & Usage

1. Install [IDF SDK](https://github.com/espressif/esp-idf) according to the [tutorial](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/get-started/index.html#get-started-how-to-get-esp-idf).

2. Clone code and submodules.

   `git clone --recurse-submodules https://github.com/Freenove/openai-realtime-embedded`

3. Set target platform (if necessary).

   `idf.py set-target esp32s3`

4. Build.

   `idf.py build`

5. Flash to the device.

   `idf.py flash`

6. Open monitor (optional).

   `idf.py monitor`

7. Connect your mobile phone or computer to a router named "OpenAi", which has no password.

8. After successful connection, use the browser to access "192.168.4.1".

9. Set your Wifi SSID, Password, and openai api key in browser. 

   If you don't know OpenAI API key, you need to [register and purchase](https://platform.openai.com/) it. Currently, OpenAI does not provide free services.

10. Done! Now you can have a conversation with OpenAI !

### Linux & Usage
1. Install [IDF SDK](https://github.com/espressif/esp-idf) according to the [tutorial](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/get-started/index.html#get-started-how-to-get-esp-idf).

2. Clone code and submodules.

   `git clone --recurse-submodules https://github.com/Freenove/openai-realtime-embedded`

3. Generate `privateConfig.json` file (File isolation is to protect your password and key).

   `cp privateConfig.common.json privateConfig.json`

4. Set your Wifi SSID, Password, and openai api key in file `privateConfig.json`. If you don't know OpenAI API key, you need to [register and purchase](https://platform.openai.com/) it. Currently, OpenAI does not provide free services.

   ```
     "wifi_ssid": "xxxxx",
     "wifi_password": "xxxxx",
     "openai_api_key": "xxxxx"
   ```

5. Set target platform (if necessary).

   `idf.py set-target esp32s3`

6. Build.

   `idf.py build`

7. Flash to the device.

   `idf.py flash`

8. Open monitor (optional).

   `idf.py monitor`

9. Done! Now you can have a conversation with OpenAI !



