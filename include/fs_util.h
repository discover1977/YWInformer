#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <SD_MMC.h>
#include <FS.h>

#define SD_CS 10

// File system functions declaration
bool mountSD();
void SD_listDir(fs::FS &fs, const char * dirname, uint8_t levels);
void SD_createDir(fs::FS &fs, const char * path);
void SD_readFile(fs::FS &fs, const char * path);
void SD_writeFile(fs::FS &fs, const char * path, const char * message);
void SD_appendFile(fs::FS &fs, const char * path, const char * message);
void SD_renameFile(fs::FS &fs, const char * path1, const char * path2);
void SD_testFileIO(fs::FS &fs, const char * path);

bool mountSD() {
  if(!SD_MMC.begin("/sdcard", true)) {
    log_d("Card Mount Failed");
    return false;
  }

  uint8_t cardType = SD_MMC.cardType();

  if(cardType == CARD_NONE) {
    log_d("No SD card attached");
    return false;
  }

  Serial.print("SD Card Type: ");
  if(cardType == CARD_MMC) {
    log_d("MMC");
  } else if(cardType == CARD_SD) {
    log_d("SDSC");
  } else if(cardType == CARD_SDHC) {
    log_d("SDHC");
  } else {
    log_d("UNKNOWN");
  }

  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  log_d("SD Card Size: %lluMB\n", cardSize);
  return true;
}

/* FS functions */
void SD_listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  log_d("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if(!root){
    log_d("Failed to open directory");
    return;
  }
  if(!root.isDirectory()){
    log_d("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while(file){
    if(file.isDirectory()){
      log_d("  DIR : ");
      log_d("file name", file.name());
      if(levels){
        SD_listDir(fs, file.name(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void SD_createDir(fs::FS &fs, const char * path) {
  Serial.printf("Creating Dir: %s\n", path);
  if(fs.mkdir(path)){
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}

void SD_removeDir(fs::FS &fs, const char * path){
  Serial.printf("Removing Dir: %s\n", path);
  if(fs.rmdir(path)){
    Serial.println("Dir removed");
  } else {
    Serial.println("rmdir failed");
  }
}

void SD_readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while(file.available()){
    Serial.write(file.read());
  }
  file.close();
}

void SD_writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void SD_appendFile(fs::FS &fs, const char * path, const char * message) {
  //Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file){
    //Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)){
      //Serial.println("Message appended");
  } else {
    //Serial.println("Append failed");
  }
  file.close();
}

void SD_renameFile(fs::FS &fs, const char * path1, const char * path2) {
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("File renamed");
  } else {
    Serial.println("Rename failed");
  }
}

void SD_deleteFile(fs::FS &fs, const char * path) {
  Serial.printf("Deleting file: %s\n", path);
  if(fs.remove(path)){
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}

void SD_testFileIO(fs::FS &fs, const char * path) {
  File file = fs.open(path);
  static uint8_t buf[512];
  size_t len = 0;
  uint32_t start = millis();
  uint32_t end = start;
  if(file){
    len = file.size();
    size_t flen = len;
    start = millis();
    while(len){
      size_t toRead = len;
      if(toRead > 512){
        toRead = 512;
      }
      file.read(buf, toRead);
      len -= toRead;
    }
    end = millis() - start;
    Serial.printf("%u bytes read for %u ms\n", flen, end);
    file.close();
  } else {
    Serial.println("Failed to open file for reading");
  }


  file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }

  size_t i;
  start = millis();
  for(i=0; i<2048; i++){
    file.write(buf, 512);
  }
  end = millis() - start;
  Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
  file.close();
}