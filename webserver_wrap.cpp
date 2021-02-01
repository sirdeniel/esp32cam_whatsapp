#include "webserver_wrap.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <SPIFFS.h>

// others that may not be used (?)
#include "esp_timer.h"
#include "img_converters.h"
#include "driver/rtc_io.h"
#include <StringArray.h>

// #define DEBUG_PARAMS

AsyncWebServer server(80);
boolean takeNewPhoto = false;
boolean sendPhoto = false;

#define FILE_PHOTO "/photo.jpg"

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { text-align:center; }
    .vert { margin-bottom: 10%; }
    .hori{ margin-bottom: 0%; }
  </style>
</head>
<body>
  <div id="container">
    <h2>ESP32-CAM Last Photo</h2>
    <p>It might take more than 5 seconds to capture a photo.</p>
    <p>
      <button onclick="rotatePhoto();">ROTATE</button>
      <button onclick="capturePhoto()">CAPTURE PHOTO</button>
      <button onclick="sendPhoto()">SEND PHOTO</button>
      <button onclick="location.reload();">REFRESH PAGE</button>
    </p>
  </div>
  <div><img src="saved-photo" id="photo" width="70%"></div>
</body>
<script>
  var deg = 0;
  function capturePhoto() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', "/capture", true);
    xhr.send();
  }
  function sendPhoto() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', "/sendphoto", true);
    xhr.send();
  }
  function rotatePhoto() {
    var img = document.getElementById("photo");
    deg += 90;
    if(isOdd(deg/90)){ document.getElementById("container").className = "vert"; }
    else{ document.getElementById("container").className = "hori"; }
    img.style.transform = "rotate(" + deg + "deg)";
  }
  function isOdd(n) { return Math.abs(n % 2) == 1; }
</script>
</html>)rawliteral";

void initSPIFFS(){
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    ESP.restart();
  }
  else {
    delay(500);
    Serial.println("SPIFFS mounted successfully");
  }
}

void initServer(){
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/capture", HTTP_GET, [](AsyncWebServerRequest * request) {
    takeNewPhoto = true;
    request->send_P(200, "text/plain", "Taking Photo");
  });

  server.on("/sendphoto", HTTP_GET, [](AsyncWebServerRequest * request) {
    sendPhoto = true;
    request->send_P(200, "text/plain", "Uploading photo");
  });

  server.on("/message", HTTP_POST, [](AsyncWebServerRequest * request) {
    
    int params = request->params();
    if(params > 8){ //its probably from Twilio (9 params)
      String clientMessage = request->getParam(4)->value().c_str(); // mensaje de whatsapp
      String fromNumber = request->getParam(9)->value().c_str(); // desde número
      if(fromNumber.endsWith("982418320")){ // stablished for security reasons
        // means our client request something
        if(clientMessage.indexOf("foto") >= 0){ // returns -1 if "foto" not found
          sendPhoto = true;
          request->send_P(200, "text/plain", "Enviando foto actual");
        } else {
          request->send_P(200, "text/plain", "Puedo enviarte una foto si me escribes 'foto' sin comilla :)");
        }
      }
    }
    
    /*
     * Message from Twilio is (from 0 to n param):
     * POST[SmsMessageSid]: SMd3ee3a968a5609667dc340fb1502fac7
     * POST[NumMedia]: 0
     * POST[SmsSid]: SMd3ee3a968a5609667dc340fb1502fac7
     * POST[SmsStatus]: received
     * POST[Body]: Hi
     * POST[To]: whatsapp:+14155238886
     * POST[NumSegments]: 1
     * POST[MessageSid]: SMd3ee3a968a5609667dc340fb1502fac7
     * POST[AccountSid]: ACf8fe1bd5dc7da8edecef2c9d8ab688e0
     * POST[From]: whatsapp:+593982418320
     * POST[ApiVersion]: 2010-04-01
    */
    #ifdef DEBUG_PARAMS
    // int params = request->params();
    for(int i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isFile()){ //p->isPost() is also true
        Serial.printf("FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
      } else if(p->isPost()){
        Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      } else {
        Serial.printf("GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }
    request->send_P(200, "text/plain", "Arrived well");
    #endif
  });

  server.on("/saved-photo", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, FILE_PHOTO, "image/jpg", false);
  });

  // Start server
  server.begin();
}

bool checkPhoto( fs::FS &fs ) {
  File f_pic = fs.open( FILE_PHOTO );
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 );
}

// Capture Photo and Save it to SPIFFS
void capturePhotoSaveSpiffs( void ) {
  camera_fb_t * fb = NULL; // pointer
  bool ok = 0; // Boolean indicating if the picture has been taken correctly

  do {
    // Take a photo with the camera
    Serial.println("Taking a photo...");

    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }

    // Photo file name
    Serial.printf("Picture file name: %s\n", FILE_PHOTO);
    File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE);

    // Insert the data in the photo file
    if (!file) {
      Serial.println("Failed to open file in writing mode");
    }
    else {
      file.write(fb->buf, fb->len); // payload (image), payload length
      Serial.print("The picture has been saved in ");
      Serial.print(FILE_PHOTO);
      Serial.print(" - Size: ");
      Serial.print(file.size());
      Serial.println(" bytes");
    }
    // Close the file
    file.close();
    esp_camera_fb_return(fb);

    // check if file has been correctly saved in SPIFFS
    ok = checkPhoto(SPIFFS);
  } while ( !ok );
}
