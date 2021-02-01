/*
 * This is BasicOTA example taken from ESP32 library.
 * READ HowTo.md for 100% working on ESP32 CAM!
 * IMPOARTANT! At first flash thru cable,
 * setup passowrd (setPasswordHash recommended)
 * or VSCode can bypass it even if you update password OTA!
*/
// #define ARDUINO_OTA
// #define MDNS_ON
#define STATUS_LED_PLEASE
#define CAMERA_ON
#define WEBSERVER
#define SERIAL_DEBUG

#ifdef STATUS_LED_PLEASE
// status led
unsigned long previousMillis = 0;  // will store last time LED was updated
bool ledState = HIGH;  // ledState used to set the LED
const int interval = 1000;  // interval at which to blink (milliseconds)
#endif

#include <WiFi.h>
#include <WiFiClientSecure.h>

#ifdef WEBSERVER
#include "webserver_wrap.h"
#endif

// install these from Arduino Library Manager
#include <ArduinoJson.h> // ArduinoJson external library
#include <StreamUtils.h> // StreamUtils external library
#include "uuid.h"

#include "credentials.h"  // for sharing scripts
#include "tiny_url_coding.h"

#ifdef CAMERA_ON
#include "camera_wrap.h"
#endif

WiFiClientSecure clientTCP;
ReadLoggingStream loggingStream(clientTCP, Serial);
String imageUrl;

void setup() {
  #ifdef STATUS_LED_PLEASE
    pinMode(33, OUTPUT);
    digitalWrite(33, HIGH); // high means led is off at boot
  #endif

  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(my_ssid, my_pass);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  #ifdef WEBSERVER
  initSPIFFS();
  initServer();
  #endif

  #ifdef MDNS_ON
  if(!MDNS.begin("esp32")) {
      Serial.println("Error starting mDNS");
      //return;
  }
  #endif

  #ifdef CAMERA_ON
  initCamera();
  #endif

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

/**
 *  @brief Takes a photo and uploads it to imgbb
 *  
 *  @param apikey The apikey for imagebb.com
 *  
 *  @param fromSpiffs Bool to send photo from memory or right now (incomplete)
 *  
 *  @return Returns URL of image upload if success
 */
String uploadPhoto(const char *apikey, bool fromSpiffs){

  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();
  if(!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
    return "Camera capture failed";
  }

  if (clientTCP.connect("api.imgbb.com", 443)) {
    
    String head = String("--c2lyRGVuaWVs\r\n")+
                  String("Content-Disposition: form-data; name=\"image\"; filename=\"")+StringUUIDGen()+".jpg\"\r\n"+
                  String("Content-Type: image/jpeg\r\n\r\n");

    String tail = "\r\n--c2lyRGVuaWVs--\r\n";

    size_t imageLen = fb->len;
    uint16_t extraLen = head.length() + tail.length();
    size_t totalLen = imageLen + extraLen;

    
    #ifdef SERIAL_DEBUG
    Serial.printf("imageLen:%d - extraLen: %d - totalLen: %d\n", imageLen, extraLen, totalLen);
    #endif
  
    clientTCP.println("POST /1/upload?expiration=600&key="+String(apikey)+" HTTP/1.1");
    clientTCP.println("Host: api.imgbb.com");
    clientTCP.println("User-Agent: Arduino/1.0");
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=c2lyRGVuaWVs"); // charset=UTF-8 made the trick
    clientTCP.println();
    clientTCP.print(head);

    uint8_t *fbBuf = fb->buf;

    #ifdef SERIAL_DEBUG
    Serial.printf("fbLen: %d\n", imageLen);
    #endif
    for (size_t n=0;n<imageLen;n=n+1024) {
      if (n+1024<imageLen) {
        Serial.print(".");
        clientTCP.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (imageLen%1024>0) {
        Serial.println(".");
        size_t remainder = imageLen%1024;
        clientTCP.write(fbBuf, remainder);
      }
    }

    clientTCP.print(tail);
    esp_camera_fb_return(fb); // clear buffer from camera
    imageUrl = getImageUrlFromImgBB();
    clientTCP.stop(); // disconnect from imagebb.com
    return imageUrl;
  }
  else {
    Serial.println("Connected to api.imgbb.com failed.");
    return String("");
  }
}

/**
 *  @brief Get image url from response on imgbb json
 *  
 *  @return Returns URL of image upload if success
 */
String getImageUrlFromImgBB() {
  // TODO: this should work asyncronously, think on dual core
  Serial.println(" Getting response from imgbb --------------");
  char status[32] = {0};
  clientTCP.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status + 9, "200 OK") != 0) {
    Serial.print("Unexpected response: ");
    Serial.println(status); return "Status not 200";
  }
  char endOfHeaders[] = "\r\n\r\n";
  if (!clientTCP.find(endOfHeaders)) {
    Serial.println("Invalid response"); return "Invalid json response";
  }
  const char preJson[] = "\r\n"; // needed because there are 3 numbers before imgbb json response
  if (!clientTCP.find(preJson)) {
    // Serial.println(F("Invalid preJson, try just \\n"));
    return "Maybe JSON starts different now";
  }

  // get capacity at arduinojson.org/assistant
  const size_t capacityBB = JSON_OBJECT_SIZE(3) + 3*JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(12) + 660;
  DynamicJsonDocument doc(capacityBB);
  DeserializationError error = deserializeJson(doc, loggingStream);
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str()); return "Deserialize failed";
  }
  if(doc["success"].as<char*>() == "false"){ // do not use .as<bool*> or it throws "multiple wifi.h sources" error
    Serial.println("Json says it failed"); return "Image upload failed";
  }

  JsonObject data = doc["data"];
  imageUrl = String(data["url"].as<char*>());
  Serial.println(imageUrl);
  Serial.println("-- Out of response from imgbb --------------");
  return imageUrl;
}

/**
 *
 *  @brief Sends a Whatsapp message to a specific number. Separating this uses 4 bytes more
 *  
 *  @param to_number [String] The number in international format like "+1xxxxxxx"
 *  @param from_number [String] Get he number in international format like "+1xxxxxxx"
 *                      from Twilio account
 *  @param message_body [String] Message on the picture sent
 *  @param imageUrl [String] Url of the picture (not encoded)
 *  
 *  @return Returns a bool of success
 */
bool send_wsapp_message(
  const String& to_number,
  const String& from_number,
  const String& message_body,
  String& image_url)
{
  // URL encode our message body & picture URL to escape special chars
  // such as '&' and '='
  String encoded_body = encodeUrl(message_body);
  // Use WiFiClientSecure class to create TLS 1.2 connection
  // Connect to Twilio's REST API
  if (!clientTCP.connect("api.twilio.com", 443)) {
    #ifdef SERIAL_DEBUG
    Serial.println("Connection to Twilio failed");
    #endif
    return false;
  }
  Serial.println("Success on connection to api.twilio.com");
  // To send a whatsapp message, user must send "join sandbox-name"
  // to Twilio number
  String post_data = "From=whatsapp:" + encodeUrl(from_number) +
                     "&To=whatsapp:" + encodeUrl(to_number) +
                     "&Body=" + encoded_body;
  if (image_url.length() > 0) {
    String encoded_image = encodeUrl(image_url);
    post_data += "&MediaUrl=" + encoded_image;
  }
  // Construct headers and post body manually
  String http_request = "POST /2010-04-01/Accounts/" +
                        String(account_sid) + "/Messages.json HTTP/1.1\r\n" +
                        "Host: api.twilio.com\r\n" +
                        "Authorization: Basic " + String(auth_header) + "\r\n" + 
                        "Cache-control: no-cache\r\n" +
                        "User-Agent: ESP32-Twilio-Whatsapp\r\n" +
                        "Content-Type: " +
                        "application/x-www-form-urlencoded\r\n" +
                        "Content-Length: " + post_data.length() + "\r\n" +
                        "Connection: close\r\n" +
                        "\r\n" + post_data + "\r\n";
  loggingStream.println(http_request);

  // Read the response into the 'response' string
  Serial.println("-- Getting response from twilio api --------------");
  char status[32] = {0};
  loggingStream.readBytesUntil('\r', status, sizeof(status));
  
  char endOfHeaders[] = "\r\n\r\n";
  if (!loggingStream.find(endOfHeaders)) {
    Serial.println("Invalid response"); return false;
  }
  // get capacity at arduinojson.org/assitant
  const size_t capacity = JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(20) + 870;
  DynamicJsonDocument doc(capacity);
  DeserializationError error = deserializeJson(doc, loggingStream);
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str()); return false;
  }

  if(doc['data_sent'].as<char*>() == "null"){
    Serial.println("Data was not sent :c");
  }
  Serial.println();
  Serial.println("-- Out of response from twilio --------------");
  clientTCP.stop();
  return true;
}

void loop() {

  #ifdef STATUS_LED_PLEASE
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    // if the LED is off turn it on and vice-versa:
    ledState = not(ledState);
    // set the LED with the ledState of the variable:
    digitalWrite(33,  ledState);
  }
  #endif

  #ifdef WEBSERVER 
  if (takeNewPhoto) {
    capturePhotoSaveSpiffs();
    takeNewPhoto = false;
  }

  if(sendPhoto) {
    imageUrl = uploadPhoto(imgbb_api_key, true);
    String message_body = "Esta es una foto recién tomada";
    send_wsapp_message(to_number, from_number, message_body, imageUrl);
    sendPhoto = false;
  }

  delay(1);
  #endif
}


