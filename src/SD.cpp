/* SD library compatibility wrapper for use of SdFat on Teensy
 * Copyright (c) 2020, Paul Stoffregen, paul@pjrc.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <Arduino.h>
#include <SD.h>

SDClass SD;

#if defined(ARDUINO_TEENSY41)
static const uint8_t _SD_DAT3=46;
#elif defined(ARDUINO_TEENSY40)
static const uint8_t _SD_DAT3=38;
#elif defined(ARDUINO_TEENSY_MICROMOD)
static const uint8_t _SD_DAT3=39;
#elif defined(ARDUINO_TEENSY35) || defined(ARDUINO_TEENSY36)
static const uint8_t _SD_DAT3=62;
#endif

#ifdef __arm__
void SDClass::dateTime(uint16_t *date, uint16_t *time)
{
	uint32_t now = Teensy3Clock.get();
	if (now < 315532800) { // before 1980
		*date = 0;
		*time = 0;
	} else {
		DateTimeFields datetime;
		breakTime(now, datetime);
		*date = FS_DATE(datetime.year + 1900, datetime.mon + 1, datetime.mday);
		*time = FS_TIME(datetime.hour, datetime.min, datetime.sec);
	}
}
#endif

bool SDClass::format(int type, char progressChar, Print& pr)
{
	SdCard *card = sdfs.card();
	if (!card) return false; // no SD card
	uint32_t sectors = card->sectorCount();
	if (sectors <= 12288) return false; // card too small
	uint8_t *buf = (uint8_t *)malloc(512);
	if (!buf) return false; // unable to allocate memory
	bool ret;
	if (sectors > 67108864) {
#ifdef __arm__
		ExFatFormatter exFatFormatter;
		ret = exFatFormatter.format(card, buf, &pr);
#else
		ret = false;
#endif
	} else {
		FatFormatter fatFormatter;
		ret = fatFormatter.format(card, buf, &pr);
	}
	free(buf);
	if (ret) {
		// TODO: Is begin() really necessary?  Is a quicker way possible?
		sdfs.restart(); // TODO: is sdfs.volumeBegin() enough??
	}
	return ret;
}

bool SDClass::begin(uint8_t csPin) {
#ifdef __arm__
	FsDateTime::setCallback(dateTime);
#endif
#ifdef BUILTIN_SDCARD
	csPin_ = csPin; // remember which one passed in. 
	if (csPin == BUILTIN_SDCARD) {
		bool ret = sdfs.begin(SdioConfig(FIFO_SDIO));
		cardPreviouslyPresent = ret;
		#if defined(__IMXRT1062__)
		// start off with just trying on T4.x
		if (!ret) pinMode(_SD_DAT3, INPUT_PULLDOWN);
		#endif
		return ret;
	}
#endif
	if (csPin < NUM_DIGITAL_PINS) {
		bool ret = sdfs.begin(SdSpiConfig(csPin, SHARED_SPI, SD_SCK_MHZ(25)));
		cardPreviouslyPresent = ret;
		return ret;
	}
	return false;
}

bool SDClass::mediaPresent()
{
	//Serial.print("mediaPresent: ");
	bool ret;
	SdCard *card = sdfs.card();
	if (card) {
		if (cardPreviouslyPresent) {
			#ifdef BUILTIN_SDCARD
			uint32_t s;
			if (csPin_ == BUILTIN_SDCARD) {
				#if defined(__MK64FX512__) || defined(__MK66FX1M0__)
				card->syncDevice();
				#endif  // defined(__MK64FX512__) || defined(__MK66FX1M0__)
				s = card->status();
			} else s = 0xFFFFFFFF;
			#else
			const uint32_t s = 0xFFFFFFFF;
			#endif
			if (s == 0xFFFFFFFF) {
				// SPI doesn't have 32 bit status, read CID register
				cid_t cid;
				ret = card->readCID(&cid);
				//Serial.print(ret ? "CID=ok" : "CID=unreadable");
			} else if (s == 0) {
				// assume zero status means card removed
				// bits 12:9 are card state, which should
				// normally be 101 = data transfer mode
				//Serial.print("status=offline");
				ret = false;
				#ifdef __IMXRT1062__
				if (csPin_ == BUILTIN_SDCARD) 
					pinMode(_SD_DAT3, INPUT_PULLDOWN);
				#endif
			} else {
				//Serial.print("status=present");
				ret = true;
			}
		} else {
			// TODO: need a quick test, only call begin if likely present
			#ifdef __IMXRT1062__
			if ((csPin_ == BUILTIN_SDCARD) && !digitalReadFast(_SD_DAT3))
				ret = false;
			else
			#endif
			{
				ret = sdfs.restart();
			}
			//Serial.print(ret ? "begin ok" : "begin nope");
		}
	} else {
		//Serial.print("no card");
		ret = false;
	}
	//Serial.println();
	cardPreviouslyPresent = ret;
	return ret;
}



