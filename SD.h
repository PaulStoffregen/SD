#ifndef __SD_H__
#define __SD_H__

#include <Arduino.h>
#include <FS.h>

class Sd2Card;
class SdVolume;
class SdFile;

class SD_File : public File {
public:
  SD_File(const SdFile &f, const char *name);
  SD_File(void);
  virtual ~SD_File(void);
  virtual size_t write(uint8_t);
  virtual size_t write(const void *buf, size_t size);
  virtual int read();
  virtual int peek();
  virtual int available();
  virtual void flush();
  virtual size_t read(void *buf, size_t nbyte);
  virtual bool seek(uint32_t pos, int mode = 0);
  virtual uint32_t position() const;
  virtual uint32_t size() const;
  virtual void close();
  virtual operator bool() const;
  virtual const char * name();
  virtual boolean isDirectory(void);
  virtual File openNextFile(uint8_t mode = 0 /*O_RDONLY*/) override;
  virtual void rewindDirectory(void);
  virtual void whoami();
  using Print::write;
private:
  char _name[13]; // our name
  SdFile *_file;  // underlying file pointer
};



class SDClass : public FS {

private:
  // These are required for initialisation and use of sdfatlib
  Sd2Card *card;
  SdVolume *volume;
  SdFile *root;
  
  // my quick&dirty iterator, should be replaced
  SdFile getParentDir(const char *filepath, int *indx);
public:
  // This needs to be called to set up the connection to the SD card
  // before other methods are used.
  bool begin(uint8_t csPin = 10 /*SD_CHIP_SELECT_PIN*/);
  
  // Open the specified file/directory with the supplied mode (e.g. read or
  // write, etc). Returns a File object for interacting with the file.
  // Note that currently only one file can be open at a time.
  File open(const char *filename, uint8_t mode = FILE_READ);

  // Methods to determine if the requested file path exists.
  bool exists(const char *filepath);

  // Create the requested directory heirarchy--if intermediate directories
  // do not exist they will be created.
  bool mkdir(const char *filepath);
  
  // Delete the file.
  bool remove(const char *filepath);
  
  bool rmdir(const char *filepath);

private:

  // This is used to determine the mode used to open a file
  // it's here because it's the easiest place to pass the 
  // information through the directory walking function. But
  // it's probably not the best place for it.
  // It shouldn't be set directly--it is set via the parameters to `open`.
  int fileOpenMode;
  
  friend class SD_File;
  friend boolean callback_openPath(SdFile&, char *, boolean, void *); 
};

extern SDClass SD;

#endif
