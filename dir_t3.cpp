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

#define sector_t SDClass::sector_t
#define fatdir_t SDClass::fatdir_t

File SDClass::open(const char *path, uint8_t mode)
{
	File root = rootDir;
	File f;

	if (mode > FILE_READ) return f;

	// TODO: this needs the path traversal feature

	//Serial.println("SDClass::open");

	//Serial.printf("rootDir.start_cluster = %d\n", rootDir.start_cluster);
	//Serial.printf("root.start_cluster = %d\n", root.start_cluster);

	root.find(path, &f);
	return f;
}

File File::openNextFile(uint8_t mode)
{
	File f;

	if (mode > FILE_READ) return f;

	return f;
}


bool File::find(const char *filename, File *found)
{
	bool find_unused = true;
	char name83[11];
	uint32_t lba, sector_count;

	//Serial.println("File::open");

	const char *f = filename;
	char *p = name83;
	while (p < name83 + 11) {
		char c = *f++;
		if (c == 0) {
			while (p < name83 + 11) *p++ = ' ';
			break;
		}
		if (c == '.') {
			while (p < name83 + 8) *p++ = ' ';
			continue;
		}
		if (c > 126) continue;
		if (c >= 'a' && c <= 'z') c -= 32;
		*p++ = c;
	}
	//Serial.print("name83 = ");
	//for (uint32_t i=0; i < 11; i++) {
		//Serial.printf(" %02X", name83[i]);
	//}
	//Serial.println();

	if (type == FILE_DIR_ROOT16) {
		lba = start_cluster;
		sector_count = length >> 9;
	} else if (type == FILE_DIR) {
		current_cluster = start_cluster;
		lba = custer_to_sector(start_cluster);
		sector_count = (1 << SDClass::sector2cluster);
	} else {
		return false; // not a directory
	}
	while (1) {
		for (uint32_t i=0; i < sector_count; i++) {
			SDCache sector;
			sector_t *s = sector.read(lba);
			if (!s) return false;
			fatdir_t *dirent = s->dir;
			for (uint32_t j=0; j < 16; j++) {
				if (dirent->attrib == ATTR_LONG_NAME) {
					// TODO: how to match long names?
				}
				if (memcmp(dirent->name, name83, 11) == 0) {
					//Serial.printf("found 8.3, j=%d\n", j);
					found->init(dirent);
					return true;
				}
				uint8_t b0 = dirent->name[0];
				if (find_unused && (b0 == 0 || b0 == 0xE5)) {
					found->dirent_lba = lba;
					found->dirent_index = j;
					find_unused = false;
				}
				if (b0 == 0) return false;
				offset += 32;
				dirent++;
			}
			lba++;
		}
		if (type == FILE_DIR_ROOT16) break;
		if (!next_cluster()) break;
		lba = custer_to_sector(current_cluster);
		//Serial.printf("  next lba = %d\n", lba);
	}
	return false;
}

void File::init(fatdir_t *dirent)
{
	offset = 0;
	length = dirent->size;
	start_cluster = (dirent->cluster_high << 16) | dirent->cluster_low;
	current_cluster = start_cluster;
	type = (dirent->attrib & ATTR_DIRECTORY) ? FILE_DIR : FILE_READ;
	//Serial.printf("File::init, cluster = %d, length = %d\n", start_cluster, length);
}

#endif
#endif
