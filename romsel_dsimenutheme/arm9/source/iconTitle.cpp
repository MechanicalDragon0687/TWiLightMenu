/*-----------------------------------------------------------------
 Copyright (C) 2005 - 2013
		Michael "Chishm" Chisholm
		Dave "WinterMute" Murphy
		Claudio "sverx"
		Michael "mtheall" Theall

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

------------------------------------------------------------------*/

#include "iconTitle.h"
#include "common/dsimenusettings.h"
#include "common/systemdetails.h"
#include "common/flashcard.h"
#include <gl2d.h>
#include "common/tonccpy.h"
#include "fileBrowse.h"
#include "graphics/fontHandler.h"
#include "graphics/iconHandler.h"
#include "graphics/queueControl.h"
#include "graphics/ThemeConfig.h"
#include "graphics/ThemeTextures.h"
#include "language.h"
#include "ndsheaderbanner.h"
#include "myDSiMode.h"
#include <ctype.h>
#include <nds.h>
#include <nds/arm9/dldi.h>
#include <stdio.h>
#include <sys/stat.h>
#include <vector>

#define ICON_POS_X 112
#define ICON_POS_Y 96

extern bool showdialogbox;
extern bool dbox_showIcon;
extern bool startMenu;

extern int currentBg;

extern int movingApp;

#define TITLE_CACHE_SIZE 0x80

static bool infoFound[41] = {false};
static const char16_t *cachedTitle[41];
static const char16_t *blankTitle = u"";

static u32 arm9StartSig[4];

u8 tilesModified[(32 * 256) / 2] = {0};

std::vector<std::tuple<u8 *, u16 *, int, bool>> queuedIconUpdateCache;

static void convertIconTilesToRaw(u8 *tilesSrc, u8 *tilesNew, bool twl) {
	int PY = 32;
	if (twl)
		PY = 32 * 8;
	const int PX = 16;
	const int TILE_SIZE_Y = 8;
	const int TILE_SIZE_X = 4;
	int index = 0;
	for (int tileY = 0; tileY < PY / TILE_SIZE_Y; ++tileY) {
		for (int tileX = 0; tileX < PX / TILE_SIZE_X; ++tileX)
			for (int pY = 0; pY < TILE_SIZE_Y; ++pY)
				for (int pX = 0; pX < TILE_SIZE_X;
				     ++pX) // TILE_SIZE/2 since one u8 equals two pixels (4 bit depth)
					tilesNew[pX + tileX * TILE_SIZE_X + PX * (pY + tileY * TILE_SIZE_Y)] =
					    tilesSrc[index++];
	}
}

/**
 * Queue the icon update.
 */
void deferLoadIcon(u8 *tilesSrc, u16 *palSrc, int num, bool twl) {
	queuedIconUpdateCache.emplace_back(std::move(std::make_tuple(tilesSrc, palSrc, num, twl)));
}

/**
 * This is called in graphics/vblank handler to process
 * any deferred updates.
 */
void execDeferredIconUpdates() {
	for (auto arg : queuedIconUpdateCache) {
		u8 *tilesSrc;
		u16 *palSrc;
		int num;
		bool twl;
		std::tie(tilesSrc, palSrc, num, twl) = arg;
		convertIconTilesToRaw(tilesSrc, tilesModified, twl);
		glLoadIcon(num, (u16 *)palSrc, (u8 *)tilesModified, twl ? TWL_TEX_HEIGHT : 32);
	}
	queuedIconUpdateCache.clear();
}

//(u8(*tilesSrc)[(32 * 32) / 2], u16(*palSrc)[16])
void loadIcon(u8 *tilesSrc, u16 *palSrc, int num, bool twl) {
	// Hack to prevent glitched icons on startup.
	if (currentBg == 1) {
		deferLoadIcon(tilesSrc, palSrc, num, twl);
	} else {
		convertIconTilesToRaw(tilesSrc, tilesModified, twl);
		glLoadIcon(num, (u16 *)palSrc, (u8 *)tilesModified, twl ? TWL_TEX_HEIGHT : 32);
	}
}

static inline void loadUnkIcon(int num) { glLoadIcon(num, tex().iconUnknownTexture()->palette(), tex().iconUnknownTexture()->bytes()); }

static inline void clearIcon(int num) { glClearIcon(num); }

void drawIcon(int Xpos, int Ypos, int num) {
	if(num == -1) { // Moving app icon
		glSprite(Xpos, Ypos, bannerFlip[40], &getIcon(6)[bnriconframenumY[40]]);
	} else {
		glSprite(Xpos, Ypos, bannerFlip[num], &getIcon(num % 6)[bnriconframenumY[num]]);
	}
}

void drawIconGBA(int Xpos, int Ypos) { glSprite(Xpos, Ypos, GL_FLIP_NONE, &getIcon(GBA_ICON)[0 & 31]); }
void drawSmallIconGBA(int Xpos, int Ypos) { glSprite(Xpos, Ypos, GL_FLIP_NONE, &getIcon(GBA_ICON)[1 & 31]); }
void drawIconGB(int Xpos, int Ypos) { glSprite(Xpos, Ypos, GL_FLIP_NONE, &getIcon(GBC_ICON)[0 & 31]); }
void drawIconGBC(int Xpos, int Ypos) { glSprite(Xpos, Ypos, GL_FLIP_NONE, &getIcon(GBC_ICON)[1 & 31]); }
void drawIconNES(int Xpos, int Ypos) { glSprite(Xpos, Ypos, GL_FLIP_NONE, getIcon(NES_ICON)); }
void drawIconSMS(int Xpos, int Ypos) { glSprite(Xpos, Ypos, GL_FLIP_NONE, getIcon(SMS_ICON)); }
void drawIconGG(int Xpos, int Ypos) { glSprite(Xpos, Ypos, GL_FLIP_NONE, getIcon(GG_ICON)); }
void drawIconMD(int Xpos, int Ypos) { glSprite(Xpos, Ypos, GL_FLIP_NONE, getIcon(MD_ICON)); }
void drawIconSNES(int Xpos, int Ypos) { glSprite(Xpos, Ypos, GL_FLIP_NONE, getIcon(SNES_ICON)); }
void drawIconPLG(int Xpos, int Ypos) { glSprite(Xpos, Ypos, GL_FLIP_NONE, getIcon(PLG_ICON)); }
void drawIconA26(int Xpos, int Ypos) { glSprite(Xpos, Ypos, GL_FLIP_NONE, getIcon(A26_ICON)); }
void drawIconINT(int Xpos, int Ypos) { glSprite(Xpos, Ypos, GL_FLIP_NONE, getIcon(INT_ICON)); }
void drawIconPCE(int Xpos, int Ypos) { glSprite(Xpos, Ypos, GL_FLIP_NONE, getIcon(PCE_ICON)); }

void clearTitle(int num) {
	cachedTitle[num] = blankTitle;
}

void getGameInfo(bool isDir, const char *name, int num) {
	if (num == -1)
		num = 40;

	bnriconPalLine[num] = 0;
	bnriconframenumY[num] = 0;
	bannerFlip[num] = GL_FLIP_NONE;
	bnriconisDSi[num] = false;
	bnrWirelessIcon[num] = 0;
	isDSiWare[num] = false;
	isHomebrew[num] = false;
	isModernHomebrew[num] = false;
	requiresRamDisk[num] = false;
	requiresDonorRom[num] = false;
	infoFound[num] = false;

	if (isDir) {
		clearTitle(num);
		clearBannerSequence(num); // banner sequence
	} else if (extension(name, {".argv"})) {
		// look through the argv file for the corresponding nds file
		FILE *fp;
		char *line = NULL, *p = NULL;
		size_t size = 0;
		ssize_t rc;

		// open the argv file
		fp = fopen(name, "rb");
		if (fp == NULL) {
			clearTitle(num);
			clearBannerSequence(num);
			fclose(fp);
			return;
		}

		// read each line
		while ((rc = __getline(&line, &size, fp)) > 0) {
			// remove comments
			if ((p = strchr(line, '#')) != NULL)
				*p = 0;

			// skip leading whitespace
			for (p = line; *p && isspace((int)*p); ++p)
				;

			if (*p)
				break;
		}

		// done with the file at this point
		fclose(fp);

		if (p && *p) {
			// we found an argument
			struct stat st;

			// truncate everything after first argument
			strtok(p, "\n\r\t ");

			if (extension(p, {".nds", ".dsi", ".ids", ".srl", ".app"})) {
				// let's see if this is a file or directory
				rc = stat(p, &st);
				if (rc != 0) {
					// stat failed
					clearTitle(num);
					clearBannerSequence(num);
				} else if (S_ISDIR(st.st_mode)) {
					// this is a directory!
					clearTitle(num);
					clearBannerSequence(num);
				} else {
					getGameInfo(false, p, num);
				}
			} else {
				// this is not an nds/app file!
				clearTitle(num);
				clearBannerSequence(num);
			}
		} else {
			clearTitle(num);
			clearBannerSequence(num);
		}
		// clean up the allocated line
		free(line);
	} else if (extension(name, {".nds", ".dsi", ".ids", ".srl", ".app"})) {
		// this is an nds/app file!
		FILE *fp;
		int ret;

		// open file for reading info
		fp = fopen(name, "rb");
		if (fp == NULL) {
			clearTitle(num);
			clearBannerSequence(num); // banner sequence
			fclose(fp);
			return;
		}

		sNDSHeaderExt ndsHeader;

		ret = fseek(fp, 0, SEEK_SET);
		if (ret == 0)
			ret = fread(&ndsHeader, sizeof(ndsHeader), 1, fp); // read if seek succeed
		else
			ret = 0; // if seek fails set to !=1

		if (ret != 1) {
			// try again, but using regular header size
			ret = fseek(fp, 0, SEEK_SET);
			if (ret == 0)
				ret = fread(&ndsHeader, 0x160, 1, fp); // read if seek succeed
			else
				ret = 0; // if seek fails set to !=1

			if (ret != 1) {
				clearTitle(num);
				clearBannerSequence(num);
				fclose(fp);
				return;
			}
		}

		bool usingB4DS = (!dsiFeatures() && ms().secondaryDevice);
		bool hasCycloDSi = (isDSiMode() && memcmp(io_dldi_data->friendlyName, "CycloDS iEvolution", 18) == 0);

		if (num < 40) {
			tonccpy(gameTid[num], ndsHeader.gameCode, 4);
			romVersion[num] = ndsHeader.romversion;
			unitCode[num] = ndsHeader.unitCode;
			headerCRC[num] = ndsHeader.headerCRC16;
			a7mbk6[num] = ndsHeader.a7mbk6;
			if (a7mbk6[num] == (hasCycloDSi ? 0x080037C0 : 0x00403000) && isDSiMode() && sys().arm7SCFGLocked()) {
				requiresDonorRom[num] = hasCycloDSi ? 51 : 52;
			} else
			switch (ndsHeader.arm7binarySize) {
				case 0x22B40:
				case 0x22BCC:
					if (usingB4DS || hasCycloDSi) requiresDonorRom[num] = 51;
					break;
				case 0x23708:
				case 0x2378C:
				case 0x237F0:
					if (usingB4DS || hasCycloDSi) requiresDonorRom[num] = 5;
					break;
				case 0x235DC:
				case 0x23CAC:
					if (usingB4DS || hasCycloDSi) requiresDonorRom[num] = 20;
					break;
				case 0x24DA8:
				case 0x24F50:
					requiresDonorRom[num] = 2;
					break;
				case 0x2434C:
				case 0x2484C:
				case 0x249DC:
				case 0x25D04:
				case 0x25D94:
				case 0x25FFC:
					if (usingB4DS || hasCycloDSi) requiresDonorRom[num] = 3;
					break;
				case 0x27618:
				case 0x2762C:
				case 0x29CEC:
					requiresDonorRom[num] = 5;
					break;
				default:
					break;
			}
		}

		fseek(fp, (ndsHeader.arm9romOffset <= 0x200 ? ndsHeader.arm9romOffset : ndsHeader.arm9romOffset+0x800), SEEK_SET);
		fread(arm9StartSig, sizeof(u32), 4, fp);
		if (ndsHeader.arm9romOffset <= 0x200 && arm9StartSig[0] == 0 && arm9StartSig[1] == 0 && arm9StartSig[2] == 0 && arm9StartSig[3] == 0) {
			fseek(fp, ndsHeader.arm9romOffset+0x800, SEEK_SET);
			fread(arm9StartSig, sizeof(u32), 4, fp);
		}
		if (arm9StartSig[0] == 0xE3A00301
		 && arm9StartSig[1] == 0xE5800208
		 && arm9StartSig[2] == 0xE3A00013
		 && arm9StartSig[3] == 0xE129F000) {
			isHomebrew[num] = true;
			isModernHomebrew[num] = true; // Homebrew is recent (supports reading from SD without a DLDI driver)
			if (ndsHeader.arm7executeAddress >= 0x037F0000 && ndsHeader.arm7destination >= 0x037F0000) {
				if ((ndsHeader.arm9binarySize == 0xC9F68 && ndsHeader.arm7binarySize == 0x12814)	// Colors! v1.1
				|| (ndsHeader.arm9binarySize == 0x1B0864 && ndsHeader.arm7binarySize == 0xDB50)	// Mario Paint Composer DS v2 (Bullet Bill)
				|| (ndsHeader.arm9binarySize == 0xD45C0 && ndsHeader.arm7binarySize == 0x2B7C)		// ikuReader v0.058
				|| (ndsHeader.arm9binarySize == 0x54620 && ndsHeader.arm7binarySize == 0x1538)		// XRoar 0.24fp3
				|| (ndsHeader.arm9binarySize == 0x2C9A8 && ndsHeader.arm7binarySize == 0xFB98)		// NitroGrafx v0.7
				|| (ndsHeader.arm9binarySize == 0x22AE4 && ndsHeader.arm7binarySize == 0xA764)) {	// It's 1975 and this man is about to show you the future
					isModernHomebrew[num] = false; // Have nds-bootstrap load it (in case if it doesn't)
				}
			}
		} else if ((memcmp(ndsHeader.gameTitle, "NDS.TinyFB", 10) == 0)
				 || (memcmp(ndsHeader.gameTitle, "UNLAUNCH.DSI", 12) == 0)) {
			isHomebrew[num] = true;
			isModernHomebrew[num] = true; // No need to use nds-bootstrap
		} else if ((memcmp(ndsHeader.gameTitle, "NMP4BOOT", 8) == 0)
		 || (ndsHeader.arm7executeAddress >= 0x037F0000 && ndsHeader.arm7destination >= 0x037F0000)) {
			isHomebrew[num] = true; // Homebrew is old (requires a DLDI driver to read from SD)
		} else if ((ndsHeader.gameCode[0] == 0x48 && ndsHeader.makercode[0] != 0 && ndsHeader.makercode[1] != 0)
		 || (ndsHeader.gameCode[0] == 0x4B && ndsHeader.makercode[0] != 0 && ndsHeader.makercode[1] != 0)
		 || (ndsHeader.gameCode[0] == 0x5A && ndsHeader.makercode[0] != 0 && ndsHeader.makercode[1] != 0)
		 || (ndsHeader.gameCode[0] == 0x42 && ndsHeader.gameCode[1] == 0x38 && ndsHeader.gameCode[2] == 0x38))
		{ if (ndsHeader.unitCode != 0)
			isDSiWare[num] = true; // Is a DSiWare game
		}

		if ((memcmp(ndsHeader.gameCode, "KPP", 3) == 0
		  || memcmp(ndsHeader.gameCode, "KPF", 3) == 0)
		&& !dsiFeatures()) {
			isDSiWare[num] = false;
		}

		if (isHomebrew[num] == true && !ms().secondaryDevice && num < 40) {
			if ((ndsHeader.arm9binarySize == 0x98F70 && ndsHeader.arm7binarySize == 0xED94)		// jEnesisDS 0.7.4
			|| (ndsHeader.arm9binarySize == 0x48950 && ndsHeader.arm7binarySize == 0x74C4)			// SNEmulDS06-WIP2
			|| (ndsHeader.arm9binarySize == 0xD45C0 && ndsHeader.arm7binarySize == 0x2B7C)			// ikuReader v0.058
			|| (ndsHeader.arm9binarySize == 0x54620 && ndsHeader.arm7binarySize == 0x1538)) {		// XRoar 0.24fp3
				requiresRamDisk[num] = true;
			}
		}

		bnrSysSettings[num] =
		    (ndsHeader.gameCode[0] == 0x48 && ndsHeader.gameCode[1] == 0x4E && ndsHeader.gameCode[2] == 0x42);

		if (ndsHeader.dsi_flags & BIT(4))
			bnrWirelessIcon[num] = 1;
		else if (ndsHeader.dsi_flags & BIT(3))
			bnrWirelessIcon[num] = 2;

		sNDSBannerExt &ndsBanner = bnriconTile[num];

		if (ndsHeader.bannerOffset == 0) {
			fclose(fp);

			FILE *bannerFile = fopen("nitro:/noinfo.bnr", "rb");
			fread(&ndsBanner, 1, NDS_BANNER_SIZE_ZH_KO, bannerFile);
			fclose(bannerFile);


			if (ndsBanner.version == NDS_BANNER_VER_ZH || ndsBanner.version == NDS_BANNER_VER_ZH_KO || ndsBanner.version == NDS_BANNER_VER_DSi) {
				if (ndsBanner.titles[ms().getGameLanguage()][0] == 0) {
					cachedTitle[num] = (char16_t*)&ndsBanner.titles[ms().getTitleLanguage()];
				} else {
					cachedTitle[num] = (char16_t*)&ndsBanner.titles[ms().getGameLanguage()];
				}
			} else {
				cachedTitle[num] = (char16_t*)&ndsBanner.titles[ms().getTitleLanguage()];
			}

			return;
		}
		ret = fseek(fp, ndsHeader.bannerOffset, SEEK_SET);
		if (ret == 0)
			ret = fread(&ndsBanner, sizeof(ndsBanner), 1, fp); // read if seek succeed
		else
			ret = 0; // if seek fails set to !=1

		if (ret != 1) {
			// try again, but using regular banner size
			ret = fseek(fp, ndsHeader.bannerOffset, SEEK_SET);
			if (ret == 0)
				ret = fread(&ndsBanner, NDS_BANNER_SIZE_ORIGINAL, 1, fp); // read if seek succeed
			else
				ret = 0; // if seek fails set to !=1

			if (ret != 1) {
				fclose(fp);

				FILE *bannerFile = fopen("nitro:/noinfo.bnr", "rb");
				fread(&ndsBanner, 1, NDS_BANNER_SIZE_ZH_KO, bannerFile);
				fclose(bannerFile);

				cachedTitle[num] = (char16_t*)&ndsBanner.titles[ms().getGameLanguage()];

				return;
			}
		}

		// close file!
		fclose(fp);

		int currentLang = 0;
		if (ndsBanner.version == NDS_BANNER_VER_ZH || ndsBanner.version == NDS_BANNER_VER_ZH_KO || ndsBanner.version == NDS_BANNER_VER_DSi) {
			currentLang = ms().getGameLanguage();
		} else {
			currentLang = ms().getTitleLanguage();
		}
		while (ndsBanner.titles[currentLang][0] == 0) {
			if (currentLang == 0) break;
			currentLang--;
		}

		cachedTitle[num] = (char16_t*)&ndsBanner.titles[currentLang];

		infoFound[num] = true;

		// banner sequence
		if (ms().animateDsiIcons && ndsBanner.version == NDS_BANNER_VER_DSi) {
			u16 crc16 = swiCRC16(0xFFFF, ndsBanner.dsi_icon, 0x1180);
			if (ndsBanner.crc[3] == crc16) { // Check if CRC16 is valid
				grabBannerSequence(num);
				bnriconisDSi[num] = true;
			}
		}
	}
}

void iconUpdate(bool isDir, const char *name, int num) {
	int spriteIdx = num == -1 ? 6 : num % 6;
	if (num == -1)
		num = 40;

	if (isDir) {
		clearIcon(spriteIdx);
	} else if (extension(name, {".argv"})) {
		// look through the argv file for the corresponding nds file
		FILE *fp;
		char *line = NULL, *p = NULL;
		size_t size = 0;
		ssize_t rc;

		// open the argv file
		fp = fopen(name, "rb");
		if (fp == NULL) {
			clearIcon(spriteIdx);
			fclose(fp);
			return;
		}

		// read each line
		while ((rc = __getline(&line, &size, fp)) > 0) {
			// remove comments
			if ((p = strchr(line, '#')) != NULL)
				*p = 0;

			// skip leading whitespace
			for (p = line; *p && isspace((int)*p); ++p);

			if (*p)
				break;
		}

		// done with the file at this point
		fclose(fp);

		if (p && *p) {
			// we found an argument
			struct stat st;

			// truncate everything after first argument
			strtok(p, "\n\r\t ");

			if (extension(p, {".nds", ".dsi", ".ids", ".srl", ".app"})) {
				// let's see if this is a file or directory
				rc = stat(p, &st);
				if (rc != 0) {
					// stat failed
					clearIcon(spriteIdx);
				} else if (S_ISDIR(st.st_mode)) {
					// this is a directory!
					clearIcon(spriteIdx);
				} else {
					iconUpdate(false, p, spriteIdx);
				}
			} else {
				// this is not an nds/app file!
				clearIcon(spriteIdx);
			}
		} else {
			clearIcon(spriteIdx);
		}
		// clean up the allocated line
		free(line);
	} else if (extension(name, {".nds", ".dsi", ".ids", ".srl", ".app"})) {
		// this is an nds/app file!
		sNDSBannerExt &ndsBanner = bnriconTile[num];
		if (bnriconisDSi[num]) {
			loadIcon(ndsBanner.dsi_icon[0], ndsBanner.dsi_palette[0], spriteIdx, true);
		} else {
			loadIcon(ndsBanner.icon, ndsBanner.palette, spriteIdx, false);
		}
	}
}

void writeBannerText(std::string_view text) { writeBannerText(FontGraphic::utf8to16(text)); }
void writeBannerText(std::u16string text) {
	// Split to lines since DS game titles have manual line breaks
	std::vector<std::u16string> lines;
	size_t newline = text.find('\n');
	while (newline != text.npos) {
		lines.push_back(text.substr(0, newline));
		text = text.substr(newline + 1);
		newline = text.find('\n');
	}
	lines.push_back(text.data());
	
	// Insert line breaks if lines are too long
	for(uint i = 0; i < lines.size(); i++) {
		int width = tc().titleboxTextLarge() ? calcLargeFontWidth(lines[i]) : calcSmallFontWidth(lines[i]);
		if(width > tc().titleboxTextW()) {
			int mid = lines[i].length() / 2;
			bool foundSpace = false;
			for(uint j = 0; j < lines[i].length() / 2; j++) {
				if(lines[i][mid + j] == ' ') {
					lines.insert(lines.begin() + i, lines[i].substr(0, mid + j));
					lines[i + 1] = lines[i + 1].substr(mid + j + 1);
					i--;
					foundSpace = true;
					break;
				} else if(lines[i][mid - j] == ' ') {
					lines.insert(lines.begin() + i, lines[i].substr(0, mid - j));
					lines[i + 1] = lines[i + 1].substr(mid - j + 1);
					i--;
					foundSpace = true;
					break;
				}
			}
			if(!foundSpace) {
				lines.insert(lines.begin() + i, lines[i].substr(0, mid));
				lines[i + 1] = lines[i + 1].substr(mid);
				i--;
			}
		}
	}

	// Trim to the max lines if too big
	if((int)lines.size() > tc().titleboxMaxLines())
		lines.resize(tc().titleboxMaxLines());

	// Re-combine to a single string
	std::u16string out;
	for(auto line : lines) {
		out += line + u'\n';
	}
	if (tc().titleboxTextLarge() && !ms().macroMode) {
		printLarge(false, 0, tc().titleboxTextY() - (((lines.size() - 1) * largeFontHeight()) / 2), out, Alignment::center);
	} else {
		printSmall(false, 0, tc().titleboxTextY() - (((lines.size() - 1) * smallFontHeight()) / 2), out, Alignment::center);
	}
}

static inline void writeDialogTitle(std::u16string text) {
	int lines = 0;
	for(auto c : text) {
		if(c == '\n') {
			lines++;
		}
	}

	printLarge(false, ms().rtl() ? 256 - 70 : 70, 31 - (lines * largeFontHeight() / 2), text, ms().rtl() ? Alignment::right : Alignment::left);
}

void titleUpdate(bool isDir, std::string_view name, int num) {
	if (isDir) {
		if (name == "..") {
			writeBannerText(STR_BACK);
		} else {
			writeBannerText(name);
		}
	} else if (!extension(name, {".nds", ".dsi", ".ids", ".srl", ".app"})) {
		writeBannerText(name.substr(0, name.rfind('.')));
	} else {
		// this is an nds/app file!

		bool theme_showdialogbox = (showdialogbox || (ms().theme == 4 && currentBg == 1) || (ms().theme == 5 && dbox_showIcon));
		if (theme_showdialogbox) {
			writeDialogTitle(cachedTitle[num]);
		} else if (infoFound[num]) {
			writeBannerText(cachedTitle[num]);
		} else {
			writeBannerText(name);
		}
	}
}
