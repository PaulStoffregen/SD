#ifndef __SD_H__
#define __SD_H__

#include <Arduino.h>
#include <FS.h>
#include <SdFat.h>

class SDFile : public File
{
public:
	SDFile(const FsFile &file) : sdfatfile(file), filename(nullptr) { }
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
			filename = (char *)malloc(256);
			if (filename) {
				sdfatfile.getName(filename, 256);
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
		FsFile file = sdfatfile.openNextFile();
		if (file) return File(new SDFile(file));
		return File();
	}
	virtual void rewindDirectory(void) {
		sdfatfile.rewindDirectory();
	}
	using Print::write;
private:
	FsFile sdfatfile;
	char *filename;
};



class SDClass : public FS
{
public:
	SDClass() { }
	bool begin(uint8_t csPin = 10) {
		return sdfs.begin(csPin, SD_SCK_MHZ(24));
	}
	File open(const char *filepath, uint8_t mode = FILE_READ) {
		oflag_t flags = O_READ;
		if (mode == FILE_WRITE) flags = O_READ | O_WRITE | O_CREAT;
		FsFile file = sdfs.open(filepath, flags);
		// TODO: Arduino's default to seek to end of writable file
		if (file) return File(new SDFile(file));
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
	SdFs sdfs;
};

extern SDClass SD;

#endif
