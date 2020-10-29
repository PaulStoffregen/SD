
#include <Arduino.h>
#include <SD.h>
#include <limits.h>

#ifndef __SD_t3_H__

#include <utility/SdFat.h>
#include <utility/SdFatUtil.h>


void SD_File::whoami()
{
	//Serial.printf("  SD_File this=%x, f=%x\n", (int)this, (int)f.get());
	Serial.printf("  SD_File this=%x, refcount=%u\n",
		(int)this, getRefcount());
	static int whoamicount = 0;
	if (++whoamicount > 20) while (1) ;
}


SD_File::SD_File(const SdFile &file, const char *n)
{
	_file = new SdFile();
	memcpy(_file, &file, sizeof(SdFile)); // should use a copy constructor?
	strncpy(_name, n, 12);
	_name[12] = 0;
	//f = (File *)this;
}

SD_File::SD_File(void)
{
	_file = nullptr;
	_name[0] = 0;
	//f = (File *)this;
}

SD_File::~SD_File(void)
{
	close();
}

// returns a pointer to the file name
const char * SD_File::name(void) {
	return _name;
}

// a directory is a special type of file
bool SD_File::isDirectory(void)
{
	return (_file && _file->isDir());
}


size_t SD_File::write(uint8_t val)
{
	return write(&val, 1);
}

size_t SD_File::write(const void *buf, size_t size)
{
	if (_file == nullptr) {
		setWriteError();
		return 0;
	}
	_file->clearWriteError();
	size_t t = _file->write(buf, size);
	if (_file->getWriteError()) {
		setWriteError();
		return 0;
	}
	return t;
}

int SD_File::peek()
{
	if (_file == nullptr) return 0;
	int c = _file->read();
	if (c != -1) _file->seekCur(-1);
	return c;
}

int SD_File::read()
{
	if (_file == nullptr) return -1;
	return _file->read();
}

// buffered read for more efficient, high speed reading
size_t SD_File::read(void *buf, size_t nbyte) {
	if (_file == nullptr) return 0;
	return _file->read(buf, nbyte);
}

int SD_File::available()
{
	if (_file == nullptr) return 0;
	uint32_t n = size() - position();
	if (n > INT_MAX) n = INT_MAX;
	return n;
}

void SD_File::flush()
{
	if (_file == nullptr) return;
	_file->sync();
}

bool SD_File::seek(uint32_t pos, int mode)
{
	if (_file == nullptr) return false;
	return _file->seekSet(pos);
}

uint32_t SD_File::position() const
{
	if (_file == nullptr) return 0;
	return _file->curPosition();
}

uint32_t SD_File::size() const
{
	if (_file == nullptr) return 0;
	return _file->fileSize();
}

void SD_File::close()
{
	if (_file == nullptr) return;
	_file->close();
	//delete _file; // TODO: how to handle this?
	_file = nullptr;
}

SD_File::operator bool() const
{
	if (_file == nullptr) return false;
	return _file->isOpen();
}
#endif
