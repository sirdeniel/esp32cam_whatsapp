This project is the result of a lot of research done by myself and
from a lot of work found on the internet, especially Github and Stackoverflow

# Introduction
An embedded device like ESP32 is capable to connect to web services
thanks to WiFi. Although, there are many projects around IoT applications
no one has ever (as far as I know) make communication via WhatsApp on its own.

## Simple diagram
![esp32-whtsapp-simple](https://i.ibb.co/X286hP5/jpeg-optimizer-whatsapp-esp32-simple.jpg)

## Descriptive diagram
![esp32-whatsapp](https://i.ibb.co/Th4hk40/jpeg-optimizer-whatsapp-esp32.jpg)

## Video demo
[![ESP32CAM WhatsApp Communication](http://img.youtube.com/vi/-EnRYvCKHzE/0.jpg)](http://www.youtube.com/watch?v=-EnRYvCKHzE)

# ¿What does it do?
An ESP32CAM can answer you via WhatsApp and send you a photo if you request it.

# Web API's used
- Twilio (for WhatsApp)
- Imgbb (for uploading a photo)
- ngrok (expose ESP32 CAM to the internet)

# Credits to authors who made this possible (No specific order)
- [zenmanenergy](https://github.com/zenmanenergy) - URL Encoder for ESP8266
- [furrysalamander](https://github.com/furrysalamander) - ESP UUID4 generator
- [longpth](https://github.com/longpth) - camera_wrap.h and camera_wrap.cpp


