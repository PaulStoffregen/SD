#ifndef __SD_H__
#define __SD_H__

#include <Arduino.h>
#include <FS.h>
#include <SdFat.h>

#if defined(__MK64FX512__) || defined(__MK66FX1M0__) || defined(__IMXRT1062__)
#define BUILTIN_SDCARD 254
#endif

#if defined(__arm__)
  // Support everything on 32 bit boards with enough memory
  #define SDFAT_FILE FsFile
  #define SDFAT_BASE SdFs
  #define MAX_FILENAME_LEN 256
#elif defined(__AVR__)
  // Limit to 32GB cards on 8 bit Teensy with only limited memory
  #define SDFAT_FILE File32
  #define SDFAT_BASE SdFat32
  #define MAX_FILENAME_LEN 64
#endif

class SDFile : public File
{
public:
	SDFile(const SDFAT_FILE &file) : sdfatfile(file), filename(nullptr) { }
	virtual ~SDFile(void) {
		if (sdfatfile) sdfatfile.close();
		if (filename) free(filename);
	}
#ifdef FILE_WHOAMI
	virtual void whoami() {
		Serial.printf("   SDFile this=%x, refcount=%u\n",
			(int)this, getRefcount());
	}
#endif
	virtual size_t write(const void *buf, size_t size) {
		return sdfatfile.write(buf, size);
	}
	virtual int peek() {
		return sdfatfile.peek();
	}
	virtual int available() {
		return sdfatfile.available();
	}
	virtual void flush() {
		sdfatfile.flush();
	}
	virtual size_t read(void *buf, size_t nbyte) {
		return sdfatfile.read(buf, nbyte);
	}
	virtual bool seek(uint32_t pos, int mode = SeekSet) {
		if (mode == SeekSet) return sdfatfile.seekSet(pos);
		if (mode == SeekCur) return sdfatfile.seekCur(pos);
		if (mode == SeekEnd) return sdfatfile.seekEnd(pos);
		return false;
	}
	virtual uint32_t position() {
		return sdfatfile.curPosition();
	}
	virtual uint32_t size() {
		return sdfatfile.size();
	}
	virtual void close() {
		if (filename) {
			free(filename);
			filename = nullptr;
		}
		sdfatfile.close();
	}
	virtual operator bool() {
		return sdfatfile.isOpen();
	}
	virtual const char * name() {
		if (!filename) {
			filename = (char *)malloc(MAX_FILENAME_LEN);
			if (filename) {
				sdfatfile.getName(filename, MAX_FILENAME_LEN);
			} else {
				static char zeroterm = 0;
				filename = &zeroterm;
			}
		}
		return filename;
	}
	virtual boolean isDirectory(void) {
		return sdfatfile.isDirectory();
	}
	virtual File openNextFile(uint8_t mode=0) {
		SDFAT_FILE file = sdfatfile.openNextFile();
		if (file) return File(new SDFile(file));
		return File();
	}
	virtual void rewindDirectory(void) {
		sdfatfile.rewindDirectory();
	}
	using Print::write;
private:
	SDFAT_FILE sdfatfile;
	char *filename;
};



class SDClass : public FS
{
public:
	SDClass() { }
	bool begin(uint8_t csPin = 10) {
#ifdef BUILTIN_SDCARD
		if (csPin == BUILTIN_SDCARD) {
			return sdfs.begin(SdioConfig(FIFO_SDIO));
			//return sdfs.begin(SdioConfig(DMA_SDIO));
		}
#endif
		return sdfs.begin(csPin, SD_SCK_MHZ(24));
	}
	File open(const char *filepath, uint8_t mode = FILE_READ) {
		oflag_t flags = O_READ;
		if (mode == FILE_WRITE) flags = O_READ | O_WRITE | O_CREAT;
		SDFAT_FILE file = sdfs.open(filepath, flags);
		if (file) {
			// Arduino's default FILE_WRITE starts at end of file
			if (mode == FILE_WRITE) file.seekEnd(0);
			return File(new SDFile(file));
		}
		return File();
	}
	bool exists(const char *filepath) {
		return sdfs.exists(filepath);
	}
	bool mkdir(const char *filepath) {
		return sdfs.mkdir(filepath);
	}
	bool remove(const char *filepath) {
		return sdfs.remove(filepath);
	}
	bool rmdir(const char *filepath) {
		return sdfs.rmdir(filepath);
	}
public: // allow access, so users can mix SD & SdFat APIs
	SDFAT_BASE sdfs;
};

extern SDClass SD;

// do not expose these defines in Arduino sketches or other libraries
#undef SDFAT_FILE
#undef SDFAT_BASE
#undef MAX_FILENAME_LEN


#define SD_CARD_TYPE_SD1 0
#define SD_CARD_TYPE_SD2 1
#define SD_CARD_TYPE_SDHC 3
class Sd2Card
{
public:
	bool init(uint8_t speed, uint8_t csPin) {
		return SD.begin(csPin);
	}
	uint8_t type() {
		return SD.sdfs.card()->type();
	}
};
class SdVolume
{
public:
	bool init(Sd2Card &card) {
		return SD.sdfs.vol() != nullptr;
	}
	uint8_t fatType() {
		return SD.sdfs.vol()->fatType();
	}
	uint32_t blocksPerCluster() {
		return SD.sdfs.vol()->sectorsPerCluster();
	}
	uint32_t clusterCount() {
		return SD.sdfs.vol()->clusterCount();
	}
};

#endif
