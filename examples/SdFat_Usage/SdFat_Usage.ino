/*
  SdFat usage from SD library

  Starting with Teensyduino 1.54, the SD library is a thin wrapper for SdFat.

  You can access the main SdFat filesystem with "SD.sdfs".  See the comments
  in the code below for ways to make use of SdFat access.

  This example code is in the public domain.
*/
#include <SD.h>

void setup()
{
  //Uncomment these lines for Teensy 3.x Audio Shield (Rev C)
  //SPI.setMOSI(7);  // Audio shield has MOSI on pin 7
  //SPI.setSCK(14);  // Audio shield has SCK on pin 14

  Serial.begin(9600);
  while (!Serial); // wait for Arduino Serial Monitor

  Serial.print("Initializing SD card...");
  bool ok;
  const int chipSelect = 10;

  // Instead of the usual SD.begin(pin), you can access the underlying
  // SdFat library for much more control over how the SD card is
  // accessed.  Uncomment one of these, or craft your own if you wish
  // to use SdFat's many special features.

  // Faster SPI frequency.  16 MHz is default for longer / messy wiring.
  ok = SD.sdfs.begin(SdSpiConfig(chipSelect, SHARED_SPI, SD_SCK_MHZ(24)));

  // Very slow SPI frequency.  May be useful for hardware with slow buffers.
  //ok = SD.sdfs.begin(SdSpiConfig(chipSelect, SHARED_SPI, SD_SCK_MHZ(4)));

  // SdFat offers DEDICATED_SPI optimation when no other SPI chips are
  // connected.  More CPU time is used and results may vary depending on
  // interrupts, but for many cases speed is much faster.
  //ok = SD.sdfs.begin(SdSpiConfig(chipSelect, DEDICATED_SPI, SD_SCK_MHZ(16)));

  // Access the built in SD card on Teensy 3.5, 3.6, 4.1 using FIFO
  //ok = SD.sdfs.begin(SdioConfig(FIFO_SDIO));

  // Access the built in SD card on Teensy 3.5, 3.6, 4.1 using DMA (maybe faster)
  //ok = SD.sdfs.begin(SdioConfig(DMA_SDIO));

  if (!ok) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");
  Serial.println();

  // After the SD card is initialized, you can access is using the ordinary
  // SD library functions, regardless of whether it was initialized by
  // SD library SD.begin() or SdFat library SD.sdfs.begin().
  //
  Serial.println("Print directory using SD functions");
  File root = SD.open("/");
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break; // no more files
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
    } else {
      printSpaces(40 - strlen(entry.name()));
      Serial.print("  ");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }

  // You can also access the SD card with SdFat's functions
  //
  Serial.println();
  Serial.println("Print directory using SdFat ls() function");
  SD.sdfs.ls();
}

void loop()
{
  // nothing happens after setup finishes.
}


void printSpaces(int num) {
  for (int i = 0; i < num; i++) {
    Serial.print(" ");
  }
}

