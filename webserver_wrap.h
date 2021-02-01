//#pragma once
#include <Arduino.h> //needed for Serial.println
#include <string.h> //needed for memcpy
#include <FS.h>

extern boolean takeNewPhoto;
extern boolean sendPhoto;
extern String FILE_PHOTO;
extern void initServer();
extern void initSPIFFS();
extern bool checkPhoto( fs::FS &fs );
extern void capturePhotoSaveSpiffs( void );
