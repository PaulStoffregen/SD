/* Optimized SD Library for Teensy 3.X
 * Copyright (c) 2015, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this SD library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing genuine Teensy or other PJRC products.
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

#if defined(__arm__)
#include "SD_t3.h"
#ifdef USE_TEENSY3_OPTIMIZED_CODE

#define cache_t  SDCache::cache_t
#define sector_t SDClass::sector_t

cache_t SDCache::cache[SD_CACHE_SIZE];

#define CACHE_FLAG_HAS_DATA  1
#define CACHE_FLAG_IS_DIRTY  2
#define CACHE_FLAG_IS_FAT    4

//#define PRINT_SECTORS

#ifdef PRINT_SECTORS
static void print_sector(const void *data)
{
	const uint8_t *p = (const uint8_t *)data;
	for (int i=0; i < 512; i++) {
		Serial.printf(" %02X", *p++);
		if ((i & 31) == 31) Serial.println();
	}
}
#endif

// Read a sector into the cache.  If the sector is already cached,
// of course no actual read occurs.  This is the primary function
// used to access the SD card.
//
sector_t * SDCache::read(uint32_t lba, bool is_fat)
{
	sector_t *ret = NULL;
	//uint32_t slot=0, ucount=0;

	// the entire read operation, including all cache manipulation,
	// needs to be protected with exclusive access to the hardware.
	//Serial.printf("cache read: lba = %d\n", lba);
	SPI.beginTransaction(SD_SPI_SPEED);
	// does the cache already have the sector?
	cache_t *c = find(lba);
	if (c) {
		ret = &c->data;
	} else {
		c = empty();
		if (c != NULL) {
			// TODO: if dirty, write to SD card
			if (SDClass::sd_read(lba, &c->data)) {
				item = c;
				c->lba = lba;
				c->usagecount = 1;
				c->flags = CACHE_FLAG_HAS_DATA;
				if (is_fat) c->flags |= CACHE_FLAG_IS_FAT;
				ret = &c->data;
				 #ifdef PRINT_SECTORS
				 Serial.printf("cache read %u\n", lba);
				 print_sector(&c->data);
				 #endif
			}
		}
	}
	//if (c) slot = c - cache, ucount = c->usagecount;
	SPI.endTransaction();
	//if (ret) {
		//Serial.printf("read    %u, %u, slot %u\n", lba, ucount, slot);
	//} else {
		//Serial.printf("read    %u, FAIL\n", lba);
	//}
	return ret;
}

// Read a whole 512 byte sector directly to memory.  If the sector is
// already cached, of course no actual read occurs and data is copied
// from the cache.  When the sector is not cached, it's transferred
// directly from SD card to memory, bypassing the cache.
//
bool SDCache::read(uint32_t lba, void *buffer)
{
	bool ret = true;

	SPI.beginTransaction(SD_SPI_SPEED);
	cache_t *c = find(lba);
	if (!c) ret = SDClass::sd_read(lba, buffer);
	SPI.endTransaction();
	if (c) memcpy(buffer, &c->data, 512);
	return ret;
}


sector_t * SDCache::alloc(uint32_t lba)
{

	return NULL;
}

void SDCache::priority(signed int n)
{
	if (!item) return;
	if (n > 0) {
		__disable_irq();
		signed int pri = (int)(item->priority) + n;
		if (pri > 255) pri = 255;
		item->priority = pri;
		__enable_irq();
	} else {
		__disable_irq();
		signed int pri = (int)(item->priority) + n;
		if (pri < 0) pri = 0;
		item->priority = pri;
		__enable_irq();
	}
}

void SDCache::priority(uint32_t lba, signed int n)
{
	// TODO: if any a specific sector is cached, adjust its priority
}

void SDCache::dirty(void)
{
	__disable_irq();
	item->usagecount |= CACHE_FLAG_IS_DIRTY;
	__enable_irq();
}

void SDCache::release(void)
{
	//Serial.printf("cache release\n");
	if (item) {
		__disable_irq();
		item->usagecount--;
		//uint32_t ucount = item->usagecount;
		//uint32_t lba = item->lba;
		//Serial.printf("release %d, %d, slot %u\n", item->lba, item->usagecount, item-cache);
		__enable_irq();
		item = NULL;
	}
}

cache_t * SDCache::find(uint32_t lba)
{
	//Serial.printf("SDCache::find, lba=%n\n", lba);
	// have we already acquired a cache entry?
	if (item) {
		//Serial.printf("  item exists, lba=%d\n", item->lba);
		// if it's the desired block, use it
		if (item->lba == lba) return item;
		// if not, release our hold on it
		//Serial.printf("cache find release\n");
		item->usagecount--;
		item = NULL;
	}
	// does the cache already have the sector we want?
	const cache_t *end=cache+SD_CACHE_SIZE;
	for (cache_t *c = cache; c < end; c++) {
		if ((c->flags) && (c->lba == lba)) {
			//Serial.printf("  item found\n");
			item = c;
			c->usagecount++;
			return c;
		}
	}
	//Serial.printf("  item not found\n");
	// the desired sector isn't in the cache
	return NULL;
}

cache_t * SDCache::empty(void)
{
	const cache_t *end=cache+SD_CACHE_SIZE;
	cache_t *useme = NULL;
	uint32_t lowest_priority = 0xFF;

	for (cache_t *c = cache; c < end; c++) {
		if (c->usagecount == 0 && c->priority < lowest_priority) {
			useme = c;
			lowest_priority = c->priority;
			if (lowest_priority == 0) break;
		}
	}
	return useme;
}

#endif
#endif
