#include <nds.h>
#include <nds/arm9/dldi.h>
#include "io_m3_common.h"
#include "io_g6_common.h"
#include "io_sc_common.h"
#include "exptools.h"

#include <stdio.h>
#include <fat.h>
#include "fat_ext.h"
#include <sys/stat.h>
#include <limits.h>

#include <string.h>
#include <unistd.h>
#include <gl2d.h>
#include <maxmod9.h>

#include "date.h"
#include "fileCopy.h"
#include "nand/nandio.h"

#include "graphics/graphics.h"

#include "myDSiMode.h"
#include "common/dsimenusettings.h"
#include "common/tonccpy.h"
#include "common/nitrofs.h"
#include "read_card.h"
#include "flashcard.h"
#include "ndsheaderbanner.h"
#include "gbaswitch.h"
#include "nds_loader_arm9.h"
#include "perGameSettings.h"
#include "errorScreen.h"

#include "iconTitle.h"
#include "graphics/fontHandler.h"

#include "common/inifile.h"
#include "tool/stringtool.h"

#include "language.h"

#include "cheat.h"
#include "crc.h"

#include "soundbank.h"
#include "soundbank_bin.h"

#include "twlClockExcludeMap.h"
#include "dmaExcludeMap.h"
#include "asyncReadExcludeMap.h"
#include "donorMap.h"
#include "saveMap.h"
#include "ROMList.h"

#include "sr_data_srllastran.h"	// For rebooting into the game

bool useTwlCfg = false;

bool whiteScreen = true;
bool fadeType = false;		// false = out, true = in
bool fadeSpeed = true;		// false = slow (for DSi launch effect), true = fast
bool controlTopBright = true;
bool controlBottomBright = true;
//bool widescreenEffects = false;

extern bool showProgressBar;
extern int progressBarLength;

bool cardEjected = false;
static bool cardRefreshed = false;

extern void ClearBrightness();
extern int boxArtType[2];

const char* settingsinipath = "sd:/_nds/TWiLightMenu/settings.ini";
const char* bootstrapinipath = "sd:/_nds/nds-bootstrap.ini";

const char *unlaunchAutoLoadID = "AutoLoadInfo";
static char16_t hiyaNdsPath[] = u"sdmc:/hiya.dsi";
char launcherPath[256];

static char pictochatPath[256];
static char dlplayPath[256];

bool dsiWramAccess = false;
bool arm7SCFGLocked = false;
bool isRegularDS = true;
bool isDSLite = false;

extern bool showdialogbox;

bool extention(const std::string& filename, const char* ext) {
	if(strcasecmp(filename.c_str() + filename.size() - strlen(ext), ext)) {
		return false;
	} else {
		return true;
	}
}

/**
 * Remove trailing slashes from a pathname, if present.
 * @param path Pathname to modify.
 */
void RemoveTrailingSlashes(std::string& path)
{
	while (!path.empty() && path[path.size()-1] == '/') {
		path.resize(path.size()-1);
	}
}

/**
 * Remove trailing spaces from a cheat code line, if present.
 * @param path Code line to modify.
 */
/*static void RemoveTrailingSpaces(std::string& code)
{
	while (!code.empty() && code[code.size()-1] == ' ') {
		code.resize(code.size()-1);
	}
}*/

std::string romfolder[2];

// These are used by flashcard functions and must retain their trailing slash.
static const std::string slashchar = "/";
static const std::string woodfat = "fat0:/";
static const std::string dstwofat = "fat1:/";

int mpuregion = 0;
int mpusize = 0;

bool applaunch = false;
bool showCursor = true;
bool startMenu = false;

bool pictochatFound = false;
bool dlplayFound = false;
bool pictochatReboot = false;
bool dlplayReboot = false;

bool isDSPhat(void) {
	return (isRegularDS && !isDSLite);
}

bool useBackend = false;

using namespace std;

bool showbubble = false;
bool showSTARTborder = false;

bool titleboxXmoveleft = false;
bool titleboxXmoveright = false;

bool applaunchprep = false;

int spawnedtitleboxes = 0;

//char usernameRendered[10];
//bool usernameRenderedDone = false;

touchPosition touch;

//---------------------------------------------------------------------------------
void stop (void) {
//---------------------------------------------------------------------------------
	while (1) {
		swiWaitForVBlank();
	}
}

/**
 * Disable TWL clock speed for a specific game.
 */
bool setClockSpeed() {
	if (perGameSettings_boostCpu == -1) {
		// TODO: If the list gets large enough, switch to bsearch().
		for (unsigned int i = 0; i < sizeof(twlClockExcludeList)/sizeof(twlClockExcludeList[0]); i++) {
			if (memcmp(gameTid[ms().secondaryDevice], twlClockExcludeList[i], 3) == 0) {
				// Found match
				return false;
			}
		}
	}

	return perGameSettings_boostCpu == -1 ? ms().boostCpu : perGameSettings_boostCpu;
}

/**
 * Disable card read DMA for a specific game.
 */
bool setCardReadDMA() {
	if (perGameSettings_cardReadDMA == -1) {
		// TODO: If the list gets large enough, switch to bsearch().
		for (unsigned int i = 0; i < sizeof(cardReadDMAExcludeList)/sizeof(cardReadDMAExcludeList[0]); i++) {
			if (memcmp(gameTid[ms().secondaryDevice], cardReadDMAExcludeList[i], 3) == 0) {
				// Found match
				return false;
			}
		}
	}

	return perGameSettings_cardReadDMA == -1 ? ms().cardReadDMA : perGameSettings_cardReadDMA;
}

/**
 * Disable asynch card read for a specific game.
 */
bool setAsyncReadDMA() {
	if (perGameSettings_asyncCardRead == -1) {
		// TODO: If the list gets large enough, switch to bsearch().
		for (unsigned int i = 0; i < sizeof(asyncReadExcludeList)/sizeof(asyncReadExcludeList[0]); i++) {
			if (memcmp(gameTid[ms().secondaryDevice], asyncReadExcludeList[i], 3) == 0) {
				// Found match
				return false;
			}
		}
	}

	return perGameSettings_asyncCardRead == -1 ? ms().asyncCardRead : perGameSettings_asyncCardRead;
}

/**
 * Set donor SDK version for a specific game.
 */
int SetDonorSDK() {
	char gameTid3[5];
	for (int i = 0; i < 3; i++) {
		gameTid3[i] = gameTid[ms().secondaryDevice][i];
	}

	for (auto i : donorMap) {
		if (i.first == 5 && gameTid3[0] == 'V')
			return 5;

		if (i.second.find(gameTid3) != i.second.cend())
			return i.first;
	}

	return 0;
}

/**
 * Set MPU settings for a specific game.
 */
void SetMPUSettings() {
	scanKeys();
	int pressed = keysHeld();
	
	if (pressed & KEY_B) {
		mpuregion = 1;
	} else if (pressed & KEY_X) {
		mpuregion = 2;
	} else if (pressed & KEY_Y) {
		mpuregion = 3;
	} else {
		mpuregion = 0;
	}

	if(pressed & KEY_RIGHT) {
		mpusize = 3145728;
	} else if(pressed & KEY_LEFT) {
		mpusize = 1;
	} else {
		mpusize = 0;
	}
}

/**
 * Fix AP for some games.
 */
std::string setApFix(const char *filename) {
	if (flashcardFound()) {
		remove("fat:/_nds/nds-bootstrap/apFix.ips");
		remove("fat:/_nds/nds-bootstrap/apFixCheat.bin");
	}

	bool ipsFound = false;
	bool cheatVer = true;
	char ipsPath[256];
	if (!ipsFound) {
		snprintf(ipsPath, sizeof(ipsPath), "%s:/_nds/TWiLightMenu/extras/apfix/cht/%s.bin", sdFound() ? "sd" : "fat", filename);
		ipsFound = (access(ipsPath, F_OK) == 0);
	}

	if (!ipsFound) {
		snprintf(ipsPath, sizeof(ipsPath), "%s:/_nds/TWiLightMenu/extras/apfix/cht/%s-%X.bin", sdFound() ? "sd" : "fat", gameTid[ms().secondaryDevice], headerCRC[ms().secondaryDevice]);
		ipsFound = (access(ipsPath, F_OK) == 0);
	}

	if (!ipsFound) {
		snprintf(ipsPath, sizeof(ipsPath), "%s:/_nds/TWiLightMenu/extras/apfix/%s.ips", sdFound() ? "sd" : "fat", filename);
		ipsFound = (access(ipsPath, F_OK) == 0);
		if (ipsFound) {
			cheatVer = false;
		}
	}

	if (!ipsFound) {
		snprintf(ipsPath, sizeof(ipsPath), "%s:/_nds/TWiLightMenu/extras/apfix/%s-%X.ips", sdFound() ? "sd" : "fat", gameTid[ms().secondaryDevice], headerCRC[ms().secondaryDevice]);
		ipsFound = (access(ipsPath, F_OK) == 0);
		if (ipsFound) {
			cheatVer = false;
		}
	}

	if (ipsFound) {
		if (ms().secondaryDevice && sdFound()) {
			mkdir("fat:/_nds", 0777);
			mkdir("fat:/_nds/nds-bootstrap", 0777);
			fcopy(ipsPath, cheatVer ? "fat:/_nds/nds-bootstrap/apFixCheat.bin" : "fat:/_nds/nds-bootstrap/apFix.ips");
			return cheatVer ? "fat:/_nds/nds-bootstrap/apFixCheat.bin" : "fat:/_nds/nds-bootstrap/apFix.ips";
		}
		return ipsPath;
	} else {
		FILE *file = fopen(sdFound() ? "sd:/_nds/TWiLightMenu/extras/apfix.pck" : "fat:/_nds/TWiLightMenu/extras/apfix.pck", "rb");
		if (file) {
			char buf[5] = {0};
			fread(buf, 1, 4, file);
			if (strcmp(buf, ".PCK") != 0) // Invalid file
				return "";

			u32 fileCount;
			fread(&fileCount, 1, sizeof(fileCount), file);

			u32 offset = 0, size = 0;

			// Try binary search for the game
			int left = 0;
			int right = fileCount;

			while (left <= right) {
				int mid = left + ((right - left) / 2);
				fseek(file, 16 + mid * 16, SEEK_SET);
				fread(buf, 1, 4, file);
				int cmp = strcmp(buf, gameTid[ms().secondaryDevice]);
				if (cmp == 0) { // TID matches, check CRC
					u16 crc;
					fread(&crc, 1, sizeof(crc), file);

					if (crc == headerCRC[ms().secondaryDevice]) { // CRC matches
						fread(&offset, 1, sizeof(offset), file);
						fread(&size, 1, sizeof(size), file);
						cheatVer = fgetc(file) & 1;
						break;
					} else if (crc < headerCRC[ms().secondaryDevice]) {
						left = mid + 1;
					} else {
						right = mid - 1;
					}
				} else if (cmp < 0) {
					left = mid + 1;
				} else {
					right = mid - 1;
				}
			}

			if (offset > 0 && size > 0) {
				fseek(file, offset, SEEK_SET);
				u8 *buffer = new u8[size];
				fread(buffer, 1, size, file);

				if (flashcardFound()) {
					mkdir("fat:/_nds", 0777);
					mkdir("fat:/_nds/nds-bootstrap", 0777);
				}
				snprintf(ipsPath, sizeof(ipsPath), "%s:/_nds/nds-bootstrap/apFix%s", ms().secondaryDevice ? "fat" : "sd", cheatVer ? "Cheat.bin" : ".ips");
				FILE *out = fopen(ipsPath, "wb");
				if(out) {
					fwrite(buffer, 1, size, out);
					fclose(out);
				}
				delete[] buffer;
				fclose(file);
				return ipsPath;
			}

			fclose(file);
		}
	}

	return "";
}

/**
 * Enable widescreen for some games.
 */
void SetWidescreen(const char *filename) {
	const char* wideCheatDataPath = ms().secondaryDevice && (!isDSiWare[ms().secondaryDevice] || (isDSiWare[ms().secondaryDevice] && !ms().dsiWareToSD)) ? "fat:/_nds/nds-bootstrap/wideCheatData.bin" : "sd:/_nds/nds-bootstrap/wideCheatData.bin";
	remove(wideCheatDataPath);

	bool useWidescreen = (perGameSettings_wideScreen == -1 ? ms().wideScreen : perGameSettings_wideScreen);

	if ((isDSiMode() && arm7SCFGLocked) || ms().consoleModel < 2
	|| !useWidescreen || ms().macroMode) {
		return;
	}
	
	if (isHomebrew[ms().secondaryDevice] && ms().homebrewHasWide && (access("sd:/_nds/TWiLightMenu/TwlBg/Widescreen.cxi", F_OK) == 0)) {
		if (access("sd:/luma/sysmodules/TwlBg.cxi", F_OK) == 0) {
			rename("sd:/luma/sysmodules/TwlBg.cxi", "sd:/_nds/TWiLightMenu/TwlBg/TwlBg.cxi.bak");
		}
		if (rename("sd:/_nds/TWiLightMenu/TwlBg/Widescreen.cxi", "sd:/luma/sysmodules/TwlBg.cxi") == 0) {
			tonccpy((u32 *)0x02000300, sr_data_srllastran, 0x020);
			DC_FlushAll();
			fifoSendValue32(FIFO_USER_02, 1);
			stop();
		}
		return;
	}

	bool wideCheatFound = false;
	char wideBinPath[256];
	if (ms().launchType[ms().secondaryDevice] == 1) {
		snprintf(wideBinPath, sizeof(wideBinPath), "sd:/_nds/TWiLightMenu/extras/widescreen/%s.bin", filename);
		wideCheatFound = (access(wideBinPath, F_OK) == 0);
	}

	char s1GameTid[5];

	if (ms().slot1Launched) {
		// Reset Slot-1 to allow reading card header
		sysSetCardOwner (BUS_OWNER_ARM9);
		disableSlot1();
		for(int i = 0; i < 25; i++) { swiWaitForVBlank(); }
		enableSlot1();
		for(int i = 0; i < 15; i++) { swiWaitForVBlank(); }

		cardReadHeader((uint8*)&ndsCardHeader);

		tonccpy(s1GameTid, ndsCardHeader.gameCode, 4);
		s1GameTid[4] = 0;

		snprintf(wideBinPath, sizeof(wideBinPath), "sd:/_nds/TWiLightMenu/extras/widescreen/%s-%X.bin", s1GameTid, ndsCardHeader.headerCRC16);
		wideCheatFound = (access(wideBinPath, F_OK) == 0);
	} else if (!wideCheatFound) {
		snprintf(wideBinPath, sizeof(wideBinPath), "sd:/_nds/TWiLightMenu/extras/widescreen/%s-%X.bin", gameTid[ms().secondaryDevice], headerCRC[ms().secondaryDevice]);
		wideCheatFound = (access(wideBinPath, F_OK) == 0);
	}

	if (isHomebrew[ms().secondaryDevice]) {
		return;
	}

	mkdir(ms().secondaryDevice && (!isDSiWare[ms().secondaryDevice] || (isDSiWare[ms().secondaryDevice] && !ms().dsiWareToSD)) ? "fat:/_nds" : "sd:/_nds", 0777);
	mkdir(ms().secondaryDevice && (!isDSiWare[ms().secondaryDevice] || (isDSiWare[ms().secondaryDevice] && !ms().dsiWareToSD)) ? "fat:/_nds/nds-bootstrap" : "sd:/_nds/nds-bootstrap", 0777);

	if (wideCheatFound) {
		if (fcopy(wideBinPath, wideCheatDataPath) != 0) {
			remove(wideCheatDataPath);
			clearText();
			printSmall(false, 0, 72, STR_FAILED_TO_COPY_WIDESCREEN, Alignment::center);
			fadeType = true; // Fade in from white
			for (int i = 0; i < 60 * 3; i++) {
				swiWaitForVBlank(); // Wait 3 seconds
			}
			fadeType = false;	   // Fade to white
			for (int i = 0; i < 25; i++) {
				swiWaitForVBlank();
			}
			return;
		}
	} else {
		char *tid = ms().slot1Launched ? s1GameTid : gameTid[ms().secondaryDevice];
		u16 crc16 = ms().slot1Launched ? ndsCardHeader.headerCRC16 : headerCRC[ms().secondaryDevice];

		FILE *file = fopen(sdFound() ? "sd:/_nds/TWiLightMenu/extras/widescreen.pck" : "fat:/_nds/TWiLightMenu/extras/widescreen.pck", "rb");
		if (file) {
			char buf[5] = {0};
			fread(buf, 1, 4, file);
			if (strcmp(buf, ".PCK") != 0) // Invalid file
				return;

			u32 fileCount;
			fread(&fileCount, 1, sizeof(fileCount), file);

			u32 offset = 0, size = 0;

			// Try binary search for the game
			int left = 0;
			int right = fileCount;

			while (left <= right) {
				int mid = left + ((right - left) / 2);
				fseek(file, 16 + mid * 16, SEEK_SET);
				fread(buf, 1, 4, file);
				int cmp = strcmp(buf, tid);
				if (cmp == 0) { // TID matches, check CRC
					u16 crc;
					fread(&crc, 1, sizeof(crc), file);

					if (crc == crc16) { // CRC matches
						fread(&offset, 1, sizeof(offset), file);
						fread(&size, 1, sizeof(size), file);
						wideCheatFound = true;
						break;
					} else if (crc < crc16) {
						left = mid + 1;
					} else {
						right = mid - 1;
					}
				} else if (cmp < 0) {
					left = mid + 1;
				} else {
					right = mid - 1;
				}
			}

			if (offset > 0) {
				fseek(file, offset, SEEK_SET);
				u8 *buffer = new u8[size];
				fread(buffer, 1, size, file);

				snprintf(wideBinPath, sizeof(wideBinPath), "%s:/_nds/nds-bootstrap/wideCheatData.bin", ms().secondaryDevice && (!isDSiWare[ms().secondaryDevice] || (isDSiWare[ms().secondaryDevice] && !ms().dsiWareToSD)) ? "fat" : "sd");
				FILE *out = fopen(wideBinPath, "wb");
				if(out) {
					fwrite(buffer, 1, size, out);
					fclose(out);
				}
				delete[] buffer;
			}

			fclose(file);
		}
	}
	if (wideCheatFound && (access("sd:/_nds/TWiLightMenu/TwlBg/Widescreen.cxi", F_OK) == 0)) {
		if (access("sd:/luma/sysmodules/TwlBg.cxi", F_OK) == 0) {
			rename("sd:/luma/sysmodules/TwlBg.cxi", "sd:/_nds/TWiLightMenu/TwlBg/TwlBg.cxi.bak");
		}
		if (rename("sd:/_nds/TWiLightMenu/TwlBg/Widescreen.cxi", "sd:/luma/sysmodules/TwlBg.cxi") == 0) {
			tonccpy((u32 *)0x02000300, sr_data_srllastran, 0x020);
			DC_FlushAll();
			fifoSendValue32(FIFO_USER_02, 1);
			stop();
		}
	}
}

char filePath[PATH_MAX];

//---------------------------------------------------------------------------------
void doPause() {
//---------------------------------------------------------------------------------
	// iprintf("Press start...\n");
	// printSmall(false, x, y, "Press start...");
	while(1) {
		scanKeys();
		if(keysDown() & KEY_START)
			break;
		swiWaitForVBlank();
	}
	scanKeys();
}

mm_sound_effect snd_launch;
mm_sound_effect snd_select;
mm_sound_effect snd_stop;
mm_sound_effect snd_wrong;
mm_sound_effect snd_back;
mm_sound_effect snd_switch;
mm_sound_effect snd_backlight;

void InitSound() {
	mmInitDefaultMem((mm_addr)soundbank_bin);
	
	mmLoadEffect( SFX_LAUNCH );
	mmLoadEffect( SFX_SELECT );
	mmLoadEffect( SFX_STOP );
	mmLoadEffect( SFX_WRONG );
	mmLoadEffect( SFX_BACK );
	mmLoadEffect( SFX_SWITCH );
	mmLoadEffect( SFX_BACKLIGHT );

	snd_launch = {
		{ SFX_LAUNCH } ,			// id
		(int)(1.0f * (1<<10)),	// rate
		0,		// handle
		255,	// volume
		128,	// panning
	};
	snd_select = {
		{ SFX_SELECT } ,			// id
		(int)(1.0f * (1<<10)),	// rate
		0,		// handle
		255,	// volume
		128,	// panning
	};
	snd_stop = {
		{ SFX_STOP } ,			// id
		(int)(1.0f * (1<<10)),	// rate
		0,		// handle
		255,	// volume
		128,	// panning
	};
	snd_wrong = {
		{ SFX_WRONG } ,			// id
		(int)(1.0f * (1<<10)),	// rate
		0,		// handle
		255,	// volume
		128,	// panning
	};
	snd_back = {
		{ SFX_BACK } ,			// id
		(int)(1.0f * (1<<10)),	// rate
		0,		// handle
		255,	// volume
		128,	// panning
	};
	snd_switch = {
		{ SFX_SWITCH } ,			// id
		(int)(1.0f * (1<<10)),	// rate
		0,		// handle
		255,	// volume
		128,	// panning
	};
	snd_backlight = {
		{ SFX_BACKLIGHT } ,			// id
		(int)(1.0f * (1<<10)),	// rate
		0,		// handle
		255,	// volume
		128,	// panning
	};
}

void loadGameOnFlashcard (const char* ndsPath, bool dsGame) {
	bool runNds_boostCpu = false;
	bool runNds_boostVram = false;
	std::string filename = ndsPath;
	const size_t last_slash_idx = filename.find_last_of("/");
	if (std::string::npos != last_slash_idx) {
		filename.erase(0, last_slash_idx + 1);
	}

	loadPerGameSettings(filename);

	if (dsiFeatures() && dsGame) {
		runNds_boostCpu = perGameSettings_boostCpu == -1 ? ms().boostCpu : perGameSettings_boostCpu;
		runNds_boostVram = perGameSettings_boostVram == -1 ? ms().boostVram : perGameSettings_boostVram;
	}
	if (dsGame) {
		// Move .sav outside of "saves" folder for flashcard kernel usage
		const char *typeToReplace = ".nds";
		if (extention(filename, ".dsi")) {
			typeToReplace = ".dsi";
		} else if (extention(filename, ".ids")) {
			typeToReplace = ".ids";
		} else if (extention(filename, ".srl")) {
			typeToReplace = ".srl";
		} else if (extention(filename, ".app")) {
			typeToReplace = ".app";
		}

		std::string savename = replaceAll(filename, typeToReplace, getSavExtension());
		std::string savenameFc = replaceAll(filename, typeToReplace, ".sav");
		std::string romFolderNoSlash = romfolder[true];
		RemoveTrailingSlashes(romFolderNoSlash);
		std::string saveFolder = romFolderNoSlash + "/saves";
		mkdir(saveFolder.c_str(), 0777);
		std::string savepath = romFolderNoSlash + "/saves/" + savename;
		std::string savepathFc = romFolderNoSlash + "/" + savenameFc;
		rename(savepath.c_str(), savepathFc.c_str());
	}

	std::string fcPath;
	int err = 0;
	if ((memcmp(io_dldi_data->friendlyName, "R4(DS) - Revolution for DS", 26) == 0)
	 || (memcmp(io_dldi_data->friendlyName, "R4TF", 4) == 0)
	 || (memcmp(io_dldi_data->friendlyName, "R4iDSN", 6) == 0)) {
		CIniFile fcrompathini("fat:/_wfwd/lastsave.ini");
		fcPath = replaceAll(ndsPath, "fat:/", woodfat);
		fcrompathini.SetString("Save Info", "lastLoaded", fcPath);
		fcrompathini.SaveIniFile("fat:/_wfwd/lastsave.ini");
		err = runNdsFile("fat:/Wfwd.dat", 0, NULL, true, true, true, runNds_boostCpu, runNds_boostVram, -1);
	} else if (memcmp(io_dldi_data->friendlyName, "Acekard AK2", 0xB) == 0) {
		CIniFile fcrompathini("fat:/_afwd/lastsave.ini");
		fcPath = replaceAll(ndsPath, "fat:/", woodfat);
		fcrompathini.SetString("Save Info", "lastLoaded", fcPath);
		fcrompathini.SaveIniFile("fat:/_afwd/lastsave.ini");
		err = runNdsFile("fat:/Afwd.dat", 0, NULL, true, true, true, runNds_boostCpu, runNds_boostVram, -1);
	} else if (memcmp(io_dldi_data->friendlyName, "DSTWO(Slot-1)", 0xD) == 0) {
		CIniFile fcrompathini("fat:/_dstwo/autoboot.ini");
		fcPath = replaceAll(ndsPath, "fat:/", dstwofat);
		fcrompathini.SetString("Dir Info", "fullName", fcPath);
		fcrompathini.SaveIniFile("fat:/_dstwo/autoboot.ini");
		err = runNdsFile("fat:/_dstwo/autoboot.nds", 0, NULL, true, true, true, runNds_boostCpu, runNds_boostVram, -1);
	} else if ((memcmp(io_dldi_data->friendlyName, "TTCARD", 6) == 0)
			 || (memcmp(io_dldi_data->friendlyName, "DSTT", 4) == 0)
			 || (memcmp(io_dldi_data->friendlyName, "DEMON", 5) == 0)
			 || (memcmp(io_dldi_data->friendlyName, "DSONE", 5) == 0)
			 || (memcmp(io_dldi_data->friendlyName, "M3DS DLDI", 9) == 0)) {
		CIniFile fcrompathini("fat:/TTMenu/YSMenu.ini");
		fcPath = replaceAll(ndsPath, "fat:/", slashchar);
		fcrompathini.SetString("YSMENU", "AUTO_BOOT", fcPath);
		fcrompathini.SaveIniFile("fat:/TTMenu/YSMenu.ini");
		err = runNdsFile("fat:/YSMenu.nds", 0, NULL, true, true, true, runNds_boostCpu, runNds_boostVram, -1);
	}

	char text[64];
	snprintf(text, sizeof(text), STR_START_FAILED_ERROR.c_str(), err);
	clearText();
	printSmall(false, 4, 4, text);
	if (err == 0) {
		printSmall(false, 4, 20, STR_FLASHCARD_UNSUPPORTED);
		printSmall(false, 4, 52, STR_FLASHCARD_NAME);
		printSmall(false, 4, 68, io_dldi_data->friendlyName);
	}
	whiteScreen = true;
	fadeSpeed = true;
	controlTopBright = false;
	fadeType = true; // Fade in
	stop();
}

void loadROMselect()
{
	if (!isDSiMode()) {
		chdir("fat:/");
	} else if (sdFound()) {
		chdir("sd:/");
	}
	/*if (ms().theme == 3)
	{
		runNdsFile("/_nds/TWiLightMenu/akmenu.srldr", 0, NULL, true, false, false, true, true, -1);
	}
	else */if (ms().theme == 2 || ms().theme == 6)
	{
		runNdsFile("/_nds/TWiLightMenu/r4menu.srldr", 0, NULL, true, false, false, true, true, -1);
	}
	else
	{
		runNdsFile("/_nds/TWiLightMenu/dsimenu.srldr", 0, NULL, true, false, false, true, true, -1);
	}
}

void unlaunchRomBoot(std::string_view rom) {
	std::u16string path(FontGraphic::utf8to16(rom));
	if (path.substr(0, 3) == u"sd:") {
		path = u"sdmc:" + path.substr(3);
	}

	tonccpy((u8*)0x02000800, unlaunchAutoLoadID, 12);
	*(u16*)(0x0200080C) = 0x3F0;		// Unlaunch Length for CRC16 (fixed, must be 3F0h)
	*(u16*)(0x0200080E) = 0;			// Unlaunch CRC16 (empty)
	*(u32*)(0x02000810) = 0;			// Unlaunch Flags
	*(u32*)(0x02000810) |= BIT(0);		// Load the title at 2000838h
	*(u32*)(0x02000810) |= BIT(1);		// Use colors 2000814h
	*(u16*)(0x02000814) = 0x7FFF;		// Unlaunch Upper screen BG color (0..7FFFh)
	*(u16*)(0x02000816) = 0x7FFF;		// Unlaunch Lower screen BG color (0..7FFFh)
	toncset((u8*)0x02000818, 0, 0x20+0x208+0x1C0);		// Unlaunch Reserved (zero)
	for (uint i = 0; i < std::min(path.length(), 0x103u); i++) {
		((char16_t*)0x02000838)[i] = path[i];		// Unlaunch Device:/Path/Filename.ext (16bit Unicode,end by 0000h)
	}
	while (*(u16*)(0x0200080E) == 0) {	// Keep running, so that CRC16 isn't 0
		*(u16*)(0x0200080E) = swiCRC16(0xFFFF, (void*)0x02000810, 0x3F0);		// Unlaunch CRC16
	}

	DC_FlushAll();						// Make reboot not fail
	fifoSendValue32(FIFO_USER_02, 1);	// Reboot into DSiWare title, booted via Unlaunch
	stop();
}

void unlaunchSetHiyaBoot(void) {
	tonccpy((u8*)0x02000800, unlaunchAutoLoadID, 12);
	*(u16*)(0x0200080C) = 0x3F0;		// Unlaunch Length for CRC16 (fixed, must be 3F0h)
	*(u16*)(0x0200080E) = 0;			// Unlaunch CRC16 (empty)
	*(u32*)(0x02000810) |= BIT(0);		// Load the title at 2000838h
	*(u32*)(0x02000810) |= BIT(1);		// Use colors 2000814h
	*(u16*)(0x02000814) = 0x7FFF;		// Unlaunch Upper screen BG color (0..7FFFh)
	*(u16*)(0x02000816) = 0x7FFF;		// Unlaunch Lower screen BG color (0..7FFFh)
	toncset((u8*)0x02000818, 0, 0x20+0x208+0x1C0);		// Unlaunch Reserved (zero)
	for (uint i = 0; i < sizeof(hiyaNdsPath)/sizeof(hiyaNdsPath[0]); i++) {
		((char16_t*)0x02000838)[i] = hiyaNdsPath[i];		// Unlaunch Device:/Path/Filename.ext (16bit Unicode,end by 0000h)
	}
	while (*(u16*)(0x0200080E) == 0) {	// Keep running, so that CRC16 isn't 0
		*(u16*)(0x0200080E) = swiCRC16(0xFFFF, (void*)0x02000810, 0x3F0);		// Unlaunch CRC16
	}
}

/**
 * Reboot into an SD game when in DS mode.
 */
void ntrStartSdGame(void) {
	if (ms().consoleModel == 0) {
		unlaunchRomBoot("sd:/_nds/TWiLightMenu/resetgame.srldr");
	} else {
		tonccpy((u32 *)0x02000300, sr_data_srllastran, 0x020);
		DC_FlushAll();						// Make reboot not fail
		fifoSendValue32(FIFO_USER_02, 1);
		stop();
	}
}

void dsCardLaunch() {
	*(u32*)(0x02000300) = 0x434E4C54;	// Set "CNLT" warmboot flag
	*(u16*)(0x02000304) = 0x1801;
	*(u32*)(0x02000308) = 0x43415254;	// "CART"
	*(u32*)(0x0200030C) = 0x00000000;
	*(u32*)(0x02000310) = 0x43415254;	// "CART"
	*(u32*)(0x02000314) = 0x00000000;
	*(u32*)(0x02000318) = 0x00000013;
	*(u32*)(0x0200031C) = 0x00000000;
	while (*(u16*)(0x02000306) == 0) {	// Keep running, so that CRC16 isn't 0
		*(u16*)(0x02000306) = swiCRC16(0xFFFF, (void*)0x02000308, 0x18);
	}
	
	unlaunchSetHiyaBoot();

	DC_FlushAll();						// Make reboot not fail
	fifoSendValue32(FIFO_USER_02, 1);	// Reboot into DSiWare title, booted via Launcher
	stop();
}

void s2RamAccess(bool open) {
	if (io_dldi_data->ioInterface.features & FEATURE_SLOT_NDS) return;

	if (open) {
		if (*(u16*)(0x020000C0) == 0x334D) {
			_M3_changeMode(M3_MODE_RAM);
		} else if (*(u16*)(0x020000C0) == 0x3647) {
			_G6_SelectOperation(G6_MODE_RAM);
		} else if (*(u16*)(0x020000C0) == 0x4353) {
			_SC_changeMode(SC_MODE_RAM);
		}
	} else {
		if (*(u16*)(0x020000C0) == 0x334D) {
			_M3_changeMode(M3_MODE_MEDIA);
		} else if (*(u16*)(0x020000C0) == 0x3647) {
			_G6_SelectOperation(G6_MODE_MEDIA);
		} else if (*(u16*)(0x020000C0) == 0x4353) {
			_SC_changeMode(SC_MODE_MEDIA);
		}
	}
}

void gbaSramAccess(bool open) {
	if (open) {
		if (*(u16*)(0x020000C0) == 0x334D) {
			_M3_changeMode(M3_MODE_RAM);
		} else if (*(u16*)(0x020000C0) == 0x3647) {
			_G6_SelectOperation(G6_MODE_RAM);
		} else if (*(u16*)(0x020000C0) == 0x4353) {
			_SC_changeMode(SC_MODE_RAM_RO);
		}
	} else {
		if (*(u16*)(0x020000C0) == 0x334D) {
			_M3_changeMode((io_dldi_data->ioInterface.features & FEATURE_SLOT_GBA) ? M3_MODE_MEDIA : M3_MODE_RAM);
		} else if (*(u16*)(0x020000C0) == 0x3647) {
			_G6_SelectOperation((io_dldi_data->ioInterface.features & FEATURE_SLOT_GBA) ? G6_MODE_MEDIA : G6_MODE_RAM);
		} else if (*(u16*)(0x020000C0) == 0x4353) {
			_SC_changeMode((io_dldi_data->ioInterface.features & FEATURE_SLOT_GBA) ? SC_MODE_MEDIA : SC_MODE_RAM);
		}
	}
}

void directCardLaunch() {
	/*if (memcmp(ndsCardHeader.gameCode, "ALXX", 4) == 0) {
		u16 alxxBannerCrc = 0;
		extern u32 arm9StartSig[4];
		cardRead(0x75600, &arm9StartSig, 0x10);
		cardRead(0x174602, &alxxBannerCrc, sizeof(u16));
		if ((arm9StartSig[0] == 0xE58D0008
		 && arm9StartSig[1] == 0xE1500005
		 && arm9StartSig[2] == 0xBAFFFFC5
		 && arm9StartSig[3] == 0xE59D100C)
		 || alxxBannerCrc != 0xBA52)
		{
			if (sdFound()) {
				chdir("sd:/");
			}
			int err = runNdsFile ("/_nds/TWiLightMenu/dstwoLaunch.srldr", 0, NULL, true, true, true, ms().boostCpu, ms().boostVram, -1);
			char text[64];
			snprintf(text, sizeof(text), STR_START_FAILED_ERROR.c_str(), err);
			ClearBrightness();
			printSmall(false, 4, 4, text);
			stop();
		}
	}*/
	SetWidescreen(NULL);
	if (sdFound()) {
		chdir("sd:/");
	}
	int err = runNdsFile ("/_nds/TWiLightMenu/slot1launch.srldr", 0, NULL, true, true, false, true, true, -1);
	char text[64];
	snprintf(text, sizeof(text), STR_START_FAILED_ERROR.c_str(), err);
	ClearBrightness();
	clearText();
	printSmall(false, 4, 4, text);
	stop();
}

void printLastPlayedText(int num) {
	printSmall(false, BOX_PX, iconYpos[num] + BOX_PY - (calcSmallFontHeight(STR_LAST_PLAYED_HERE) / 2), STR_LAST_PLAYED_HERE, Alignment::center);
}

void refreshNdsCard() {
	if (cardRefreshed) return;

	if (arm7SCFGLocked && ms().showBoxArt) {
		loadBoxArt("nitro:/graphics/boxart_unknown.png", true);
	} else if ((cardInit(true) == 0) && ms().showBoxArt) {
		char game_TID[5] = {0};
		tonccpy(&game_TID, ndsCardHeader.gameCode, 4);

		char boxArtPath[256];
		sprintf (boxArtPath, (sdFound() ? "sd:/_nds/TWiLightMenu/boxart/%s.png" : "fat:/_nds/TWiLightMenu/boxart/%s.png"), game_TID);
		loadBoxArt(boxArtPath, true);	// Load box art
	} else if (ms().showBoxArt) {
		loadBoxArt("nitro:/graphics/boxart_unknown.png", true);
	}

	getGameInfo(1, false, "slot1");
	iconUpdate (1, false, "slot1");
	bnrRomType[1] = 0;
	boxArtType[1] = 0;

	// Power off after done retrieving info
	disableSlot1();

	cardRefreshed = true;
	cardEjected = false;
}

void printNdsCartBannerText() {
	if (cardEjected) {
		printSmall(false, BOX_PX, iconYpos[0] + BOX_PY - (calcSmallFontHeight(STR_NO_GAME_CARD) / 2), STR_NO_GAME_CARD, Alignment::center);
	} else if (arm7SCFGLocked) {
		printSmall(false, BOX_PX, iconYpos[0] + BOX_PY - (calcSmallFontHeight(STR_START_GAME_CARD) / 2), STR_START_GAME_CARD, Alignment::center);
	} else {
		titleUpdate(1, false, "slot1");
	}
}

void printGbaBannerText() {
	printSmall(false, BOX_PY, iconYpos[3] + BOX_PY - (calcSmallFontHeight(isRegularDS ? STR_START_GBA_GAME : STR_FEATURE_UNAVAILABLE) / 2), isRegularDS ? STR_START_GBA_GAME : STR_FEATURE_UNAVAILABLE, Alignment::center);
}

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------

	defaultExceptionHandler();

	useTwlCfg = (dsiFeatures() && (*(u8*)0x02000400 != 0) && (*(u8*)0x02000401 == 0) && (*(u8*)0x02000402 == 0) && (*(u8*)0x02000404 == 0) && (*(u8*)0x02000448 != 0));

	extern const DISC_INTERFACE __my_io_dsisd;

	*(u32*)(0x2FFFD0C) = 0x54494D52;	// Run reboot timer
	fatMountSimple("sd", &__my_io_dsisd);
	fatMountSimple("fat", dldiGetInternal());
	bool fatInited = (sdFound() || flashcardFound());
	*(u32*)(0x2FFFD0C) = 0;
	chdir(sdFound()&&isDSiMode() ? "sd:/" : "fat:/");

	if (!fatInited) {
		SetBrightness(0, 0);
		SetBrightness(1, 0);
		consoleDemoInit();
		iprintf("FAT init failed!");
		stop();
	}

	// Read user name
	/*char *username = (char*)PersonalData->name;
		
	// text
	for (int i = 0; i < 10; i++) {
		if (username[i*2] == 0x00)
			username[i*2/2] = 0;
		else
			username[i*2/2] = username[i*2];
	}*/
	
	nitroFSInit("/_nds/TWiLightMenu/mainmenu.srldr");

	if (access(settingsinipath, F_OK) != 0 && flashcardFound()) {
		settingsinipath = "fat:/_nds/TWiLightMenu/settings.ini";		// Fallback to .ini path on flashcard, if not found on SD card, or if SD access is disabled
	}

	std::string filename[2];

	if (isDSiMode()) {
		u32 wordBak = *(vu32*)0x03700000;
		*(vu32*)0x03700000 = 0x414C5253;
		dsiWramAccess = *(vu32*)0x03700000 == 0x414C5253;
		*(vu32*)0x03700000 = wordBak;
	}

	fifoWaitValue32(FIFO_USER_06);
	if (fifoGetValue32(FIFO_USER_03) == 0) arm7SCFGLocked = true;	// If TWiLight Menu++ is being run from DSiWarehax or flashcard, then arm7 SCFG is locked.
	u16 arm7_SNDEXCNT = fifoGetValue32(FIFO_USER_07);
	if (arm7_SNDEXCNT != 0) isRegularDS = false;	// If sound frequency setting is found, then the console is not a DS Phat/Lite
	isDSLite = fifoGetValue32(FIFO_USER_04);
	fifoSendValue32(FIFO_USER_07, 0);

	ms().loadSettings();
	//widescreenEffects = (ms().consoleModel >= 2 && ms().wideScreen && access("sd:/luma/sysmodules/TwlBg.cxi", F_OK) == 0);

	snprintf(pictochatPath, sizeof(pictochatPath), "/_nds/pictochat.nds");
	pictochatFound = (access(pictochatPath, F_OK) == 0);

	if (isDSiMode() && arm7SCFGLocked) {
		if (ms().consoleModel < 2 && !pictochatFound) {
			pictochatFound = true;
			pictochatReboot = true;
		}
		dlplayFound = true;
		dlplayReboot = true;
	} else {
		bool nandInited = false;
		char srcPath[256];
		u8 regions[3] = {0x41, 0x43, 0x4B};

		if (!pictochatFound && ms().consoleModel == 0) {
			for (int i = 0; i < 3; i++)
			{
				snprintf(pictochatPath, sizeof(pictochatPath), "/title/00030005/484e45%x/content/00000000.app", regions[i]);
				if (access(pictochatPath, F_OK) == 0)
				{
					pictochatFound = true;
					break;
				}
			}
		}
		if (!pictochatFound && isDSiMode() && sdFound() && ms().consoleModel == 0) {
			if (!nandInited) {
				fatMountSimple("nand", &io_dsi_nand);
				nandInited = true;
			}
			if (access("nand:/", F_OK) == 0) {
				for (int i = 0; i < 3; i++)
				{
					snprintf(srcPath, sizeof(srcPath), "nand:/title/00030005/484e45%x/content/00000000.app", regions[i]);
					if (access(srcPath, F_OK) == 0)
					{
						snprintf(pictochatPath, sizeof(pictochatPath), "/_nds/pictochat.nds");
						remove(pictochatPath);
						fcopy(srcPath, pictochatPath);	// Copy from NAND
						pictochatFound = true;
						break;
					}
				}
			}
		}

		snprintf(dlplayPath, sizeof(dlplayPath), "/_nds/dlplay.nds");
		dlplayFound = (access(dlplayPath, F_OK) == 0);
		if (!dlplayFound && ms().consoleModel == 0) {
			for (int i = 0; i < 3; i++)
			{
				snprintf(dlplayPath, sizeof(dlplayPath), "/title/00030005/484e44%x/content/00000000.app", regions[i]);
				if (access(dlplayPath, F_OK) == 0) {
					dlplayFound = true;
					break;
				} else if (regions[i] != 0x43 && regions[i] != 0x4B) {
					snprintf(dlplayPath, sizeof(dlplayPath), "/title/00030005/484e4441/content/00000001.app");
					if (access(dlplayPath, F_OK) == 0) {
						dlplayFound = true;
						break;
					}
				}
			}
		}
		if (!dlplayFound && isDSiMode() && sdFound() && ms().consoleModel == 0) {
			if (!nandInited) {
				fatMountSimple("nand", &io_dsi_nand);
				nandInited = true;
			}
			if (access("nand:/", F_OK) == 0) {
				for (int i = 0; i < 3; i++)
				{
					snprintf(srcPath, sizeof(srcPath), "nand:/title/00030005/484e44%x/content/00000000.app", regions[i]);
					if (access(srcPath, F_OK) == 0) {
						snprintf(dlplayPath, sizeof(dlplayPath), "/_nds/dlplay.nds");
						remove(dlplayPath);
						fcopy(srcPath, dlplayPath);	// Copy from NAND
						dlplayFound = true;
						break;
					} else if (regions[i] != 0x43 && regions[i] != 0x4B) {
						snprintf(srcPath, sizeof(srcPath), "nand:/title/00030005/484e4441/content/00000001.app");
						if (access(srcPath, F_OK) == 0) {
							snprintf(dlplayPath, sizeof(dlplayPath), "/_nds/dlplay.nds");
							remove(dlplayPath);
							fcopy(srcPath, dlplayPath);	// Copy from NAND
							dlplayFound = true;
							break;
						}
					}
				}
			}
		}
		if (!dlplayFound && ms().consoleModel >= 2) {
			dlplayFound = true;
			dlplayReboot = true;
		}
	}

	if (sdFound() && ms().consoleModel < 2 && ms().launcherApp != -1) {
		u8 setRegion = 0;
		if (ms().sysRegion == -1) {
			// Determine SysNAND region by searching region of System Settings on SDNAND
			char tmdpath[256];
			for (u8 i = 0x41; i <= 0x5A; i++)
			{
				snprintf(tmdpath, sizeof(tmdpath), "sd:/title/00030015/484e42%x/content/title.tmd", i);
				if (access(tmdpath, F_OK) == 0)
				{
					setRegion = i;
					break;
				}
			}
		} else {
			switch(ms().sysRegion) {
				case 0:
				default:
					setRegion = 0x4A;	// JAP
					break;
				case 1:
					setRegion = 0x45;	// USA
					break;
				case 2:
					setRegion = 0x50;	// EUR
					break;
				case 3:
					setRegion = 0x55;	// AUS
					break;
				case 4:
					setRegion = 0x43;	// CHN
					break;
				case 5:
					setRegion = 0x4B;	// KOR
					break;
			}
		}

		if (ms().sysRegion == 9) {
			sprintf(launcherPath, "nand:/launcher.dsi");
		} else {
			sprintf(launcherPath,
				 "nand:/title/00030017/484E41%X/content/0000000%i.app", setRegion, ms().launcherApp);
		}
	}

	graphicsInit();
	fontInit();
	langInit();

	iconTitleInit();

	InitSound();

	keysSetRepeat(25,5);

	srand(time(NULL));
	
	bool menuButtonPressed = false;
	
	if (ms().showGba == 1) {
		if (*(u16*)(0x020000C0) != 0) {
			sysSetCartOwner(BUS_OWNER_ARM9); // Allow arm9 to access GBA ROM
		} else {
			ms().showGba = 0;	// Hide GBA ROMs
		}
	}

	if (ms().previousUsedDevice && bothSDandFlashcard() && ms().launchType[ms().previousUsedDevice] == 3
	&& ((access(ms().dsiWarePubPath.c_str(), F_OK) == 0 && access("sd:/_nds/TWiLightMenu/tempDSiWare.pub", F_OK) == 0)
	 || (access(ms().dsiWarePrvPath.c_str(), F_OK) == 0 && access("sd:/_nds/TWiLightMenu/tempDSiWare.prv", F_OK) == 0))) {
		controlTopBright = false;
		whiteScreen = true;
		fadeType = true;	// Fade in from white
		printSmall(false, 0, 24, STR_TAKEWHILE_CLOSELID, Alignment::center);
		printSmall(false, 0, 86, STR_NOW_COPYING_DATA, Alignment::center);
		printSmall(false, 0, 100, STR_DO_NOT_TURN_OFF_POWER, Alignment::center);
		for (int i = 0; i < 30; i++) swiWaitForVBlank();
		if (access(ms().dsiWarePubPath.c_str(), F_OK) == 0) {
			fcopy("sd:/_nds/TWiLightMenu/tempDSiWare.pub", ms().dsiWarePubPath.c_str());
			rename("sd:/_nds/TWiLightMenu/tempDSiWare.pub", "sd:/_nds/TWiLightMenu/tempDSiWare.pub.bak");
		}
		if (access(ms().dsiWarePrvPath.c_str(), F_OK) == 0) {
			fcopy("sd:/_nds/TWiLightMenu/tempDSiWare.prv", ms().dsiWarePrvPath.c_str());
			rename("sd:/_nds/TWiLightMenu/tempDSiWare.prv", "sd:/_nds/TWiLightMenu/tempDSiWare.prv.bak");
		}
		fadeType = false;	// Fade to white
		for (int i = 0; i < 30; i++) swiWaitForVBlank();
		clearText(false);
		whiteScreen = false;
		controlTopBright = true;
	}

	topBgLoad();
	bottomBgLoad();
	
	bool romFound[2] = {false};
	char boxArtPath[2][256];

	// SD card
	if (sdFound() && ms().romPath[0] != "" && access(ms().romPath[0].c_str(), F_OK) == 0) {
		romFound[0] = true;

		romfolder[0] = ms().romPath[0];
		while (!romfolder[0].empty() && romfolder[0][romfolder[0].size()-1] != '/') {
			romfolder[0].resize(romfolder[0].size()-1);
		}
		chdir(romfolder[0].c_str());

		filename[0] = ms().romPath[0];
		const size_t last_slash_idx = filename[0].find_last_of("/");
		if (std::string::npos != last_slash_idx)
		{
			filename[0].erase(0, last_slash_idx + 1);
		}

		if (extention(filename[0], ".nds") || extention(filename[0], ".dsi") || extention(filename[0], ".ids") || extention(filename[0], ".app") || extention(filename[0], ".srl") || extention(filename[0], ".argv")) {
			getGameInfo(0, false, filename[0].c_str());
			iconUpdate (0, false, filename[0].c_str());
			bnrRomType[0] = 0;
			boxArtType[0] = 0;
		} else if (extention(filename[0], ".pce")) {
			bnrRomType[0] = 11;
			boxArtType[0] = 0;
		} else if (extention(filename[0], ".xex") || extention(filename[0], ".atr")
				 || extention(filename[0], ".a26") || extention(filename[0], ".a52") || extention(filename[0], ".a78")) {
			bnrRomType[0] = 10;
			boxArtType[0] = 0;
		} else if (extention(filename[0], ".int")) {
			bnrRomType[0] = 12;
			boxArtType[0] = 0;
		} else if (extention(filename[0], ".plg")) {
			bnrRomType[0] = 9;
			boxArtType[0] = 0;
		} else if (extention(filename[0], ".rvid") || extention(filename[0], ".fv")) {
			bnrRomType[0] = 9;
			boxArtType[0] = 2;
		} else if (extention(filename[0], ".agb") || extention(filename[0], ".gba") || extention(filename[0], ".mb")) {
			bnrRomType[0] = 1;
			boxArtType[0] = 1;
		} else if (extention(filename[0], ".gb") || extention(filename[0], ".sgb")) {
			bnrRomType[0] = 2;
			boxArtType[0] = 1;
		} else if (extention(filename[0], ".gbc")) {
			bnrRomType[0] = 3;
			boxArtType[0] = 1;
		} else if (extention(filename[0], ".nes")) {
			bnrRomType[0] = 4;
			boxArtType[0] = 2;
		} else if (extention(filename[0], ".fds")) {
			bnrRomType[0] = 4;
			boxArtType[0] = 1;
		} else if (extention(filename[0], ".sms")) {
			bnrRomType[0] = 5;
			boxArtType[0] = 2;
		} else if (extention(filename[0], ".gg")) {
			bnrRomType[0] = 6;
			boxArtType[0] = 2;
		} else if (extention(filename[0], ".gen")) {
			bnrRomType[0] = 7;
			boxArtType[0] = 2;
		} else if (extention(filename[0], ".smc")) {
			bnrRomType[0] = 8;
			boxArtType[0] = 3;
		} else if (extention(filename[0], ".sfc")) {
			bnrRomType[0] = 8;
			boxArtType[0] = 2;
		}

		if (ms().showBoxArt) {
			// Store box art path
			std::string temp_filename = filename[0];
			sprintf (boxArtPath[0], (sdFound() ? "sd:/_nds/TWiLightMenu/boxart/%s.png" : "fat:/_nds/TWiLightMenu/boxart/%s.png"), filename[0].c_str());
			if ((access(boxArtPath[0], F_OK) != 0) && (bnrRomType[0] == 0)) {
				if(extention(filename[0], ".argv")) {
					vector<char*> argarray;

					FILE *argfile = fopen(filename[0].c_str(),"rb");
						char str[PATH_MAX], *pstr;
					const char seps[]= "\n\r\t ";

					while( fgets(str, PATH_MAX, argfile) ) {
						// Find comment and end string there
						if( (pstr = strchr(str, '#')) )
							*pstr= '\0';

						// Tokenize arguments
						pstr= strtok(str, seps);

						while( pstr != NULL ) {
							argarray.push_back(strdup(pstr));
							pstr= strtok(NULL, seps);
						}
					}
					fclose(argfile);
					temp_filename = argarray.at(0);
				}
				sprintf (boxArtPath[0], (sdFound() ? "sd:/_nds/TWiLightMenu/boxart/%s.png" : "fat:/_nds/TWiLightMenu/boxart/%s.png"), gameTid[0]);
			}
		}
	}

	// Flashcard (Secondary device)
	if (flashcardFound()) {
		CIniFile autoruninf("fat:/autorun.inf");
		std::string autorunOpen = autoruninf.GetString("autorun.twl", "open", "");

	  if (dsiFeatures() && autorunOpen != "" && access(autorunOpen.c_str(), F_OK) == 0) {
		ms().romPath[1] = autorunOpen;
		romFound[1] = true;
	  } else if (ms().romPath[1] != "" && access(ms().romPath[1].c_str(), F_OK) == 0) {
		romFound[1] = true;
	  }

	  if (romFound[1]) {
		romfolder[1] = ms().romPath[1];
		while (!romfolder[1].empty() && romfolder[1][romfolder[1].size()-1] != '/') {
			romfolder[1].resize(romfolder[1].size()-1);
		}
		chdir(romfolder[1].c_str());

		filename[1] = ms().romPath[1];
		const size_t last_slash_idx = filename[1].find_last_of("/");
		if (std::string::npos != last_slash_idx)
		{
			filename[1].erase(0, last_slash_idx + 1);
		}

		if (extention(filename[1], ".nds") || extention(filename[1], ".dsi") || extention(filename[1], ".ids") || extention(filename[1], ".app") || extention(filename[1], ".srl") || extention(filename[1], ".argv")) {
			getGameInfo(1, false, filename[1].c_str());
			iconUpdate (1, false, filename[1].c_str());
			bnrRomType[1] = 0;
			boxArtType[1] = 0;
		} else if (extention(filename[1], ".pce")) {
			bnrRomType[1] = 11;
			boxArtType[1] = 0;
		} else if (extention(filename[1], ".xex") || extention(filename[1], ".atr")
				 || extention(filename[1], ".a26") || extention(filename[1], ".a52") || extention(filename[1], ".a78")) {
			bnrRomType[1] = 10;
			boxArtType[1] = 0;
		} else if (extention(filename[1], ".int")) {
			bnrRomType[1] = 12;
			boxArtType[1] = 0;
		} else if (extention(filename[1], ".plg")) {
			bnrRomType[1] = 9;
			boxArtType[1] = 0;
		} else if (extention(filename[1], ".rvid") || extention(filename[1], ".fv")) {
			bnrRomType[1] = 9;
			boxArtType[1] = 2;
		} else if (extention(filename[1], ".agb") || extention(filename[1], ".gba") || extention(filename[1], ".mb")) {
			bnrRomType[1] = 1;
			boxArtType[1] = 1;
		} else if (extention(filename[1], ".gb") || extention(filename[1], ".sgb")) {
			bnrRomType[1] = 2;
			boxArtType[1] = 1;
		} else if (extention(filename[1], ".gbc")) {
			bnrRomType[1] = 3;
			boxArtType[1] = 1;
		} else if (extention(filename[1], ".nes")) {
			bnrRomType[1] = 4;
			boxArtType[1] = 2;
		} else if (extention(filename[1], ".fds")) {
			bnrRomType[1] = 4;
			boxArtType[1] = 1;
		} else if (extention(filename[1], ".sms")) {
			bnrRomType[1] = 5;
			boxArtType[1] = 2;
		} else if (extention(filename[1], ".gg")) {
			bnrRomType[1] = 6;
			boxArtType[1] = 2;
		} else if (extention(filename[1], ".gen")) {
			bnrRomType[1] = 7;
			boxArtType[1] = 2;
		} else if (extention(filename[1], ".smc")) {
			bnrRomType[1] = 8;
			boxArtType[1] = 3;
		} else if (extention(filename[1], ".sfc")) {
			bnrRomType[1] = 8;
			boxArtType[1] = 2;
		}

		if (ms().showBoxArt) {
			// Store box art path
			std::string temp_filename = filename[1];
			sprintf (boxArtPath[1], (sdFound() ? "sd:/_nds/TWiLightMenu/boxart/%s.png" : "fat:/_nds/TWiLightMenu/boxart/%s.png"), filename[1].c_str());
			if ((access(boxArtPath[1], F_OK) != 0) && (bnrRomType[1] == 0)) {
				if(extention(filename[1], ".argv")) {
					vector<char*> argarray;

					FILE *argfile = fopen(filename[1].c_str(),"rb");
						char str[PATH_MAX], *pstr;
					const char seps[]= "\n\r\t ";

					while( fgets(str, PATH_MAX, argfile) ) {
						// Find comment and end string there
						if( (pstr = strchr(str, '#')) )
							*pstr= '\0';

						// Tokenize arguments
						pstr= strtok(str, seps);

						while( pstr != NULL ) {
							argarray.push_back(strdup(pstr));
							pstr= strtok(NULL, seps);
						}
					}
					fclose(argfile);
					temp_filename = argarray.at(0);
				}
				sprintf (boxArtPath[1], (sdFound() ? "sd:/_nds/TWiLightMenu/boxart/%s.png" : "fat:/_nds/TWiLightMenu/boxart/%s.png"), gameTid[1]);
			}
		}
	  }
	}

	if (ms().showBoxArt && (sdFound() ? !ms().slot1Launched : flashcardFound())) {
		loadBoxArt(boxArtPath[ms().previousUsedDevice], ms().previousUsedDevice);	// Load box art
	}

	if (isDSiMode() && !flashcardFound() && ms().slot1Launched) {
		if (REG_SCFG_MC == 0x11) {
			cardEjected = true;
			if (ms().showBoxArt) loadBoxArt("nitro:/graphics/boxart_unknown.png", true);
		} else {
			refreshNdsCard();
		}
	}

	whiteScreen = false;
	fadeType = true;	// Fade in from white
	for (int i = 0; i < 30; i++) {
		swiWaitForVBlank();
	}
	topBarLoad();
	startMenu = true;	// Show bottom screen graphics
	fadeSpeed = false;

	while(1) {

		if (startMenu) {
			int pressed = 0;

			do {
				clearText();
				printSmall(false, ms().rtl() ? 72 : -72, 6, STR_B_BACK, Alignment::center);
				printSmall(false, ms().rtl() ? -72 : 72, 6, retTime(), Alignment::center);
				if (flashcardFound()) {
					if (romFound[1]) {
						titleUpdate(1, false, filename[1].c_str());
					} else {
						printLastPlayedText(0);
					}
				} else if (isDSiMode()) {
					printNdsCartBannerText();
				}
				if (!sdFound()) {
					printGbaBannerText();
				} else if (romFound[0]) {
					titleUpdate(0, false, filename[0].c_str());
				} else {
					printLastPlayedText(3);
				}

				if (isDSiMode() && !flashcardFound()) {
					if (REG_SCFG_MC == 0x11) {
						if (cardRefreshed && ms().showBoxArt) {
							loadBoxArt(ms().slot1Launched ? "nitro:/graphics/boxart_unknown.png" : boxArtPath[ms().previousUsedDevice], ms().slot1Launched ? true : ms().previousUsedDevice);
						}
						cardRefreshed = false;
						cardEjected = true;
					} else if (cardEjected) {
						refreshNdsCard();
					}
				}

				scanKeys();
				pressed = keysDownRepeat();
				touchRead(&touch);
				checkSdEject();
				swiWaitForVBlank();
			} while (!pressed);

			if (pressed & KEY_UP) {
				if (ms().startMenu_cursorPosition == 2 || ms().startMenu_cursorPosition == 3 || ms().startMenu_cursorPosition == 5) {
					ms().startMenu_cursorPosition -= 2;
					mmEffectEx(&snd_select);
				} else if (ms().startMenu_cursorPosition == 6) {
					ms().startMenu_cursorPosition -= 3;
					mmEffectEx(&snd_select);
				} else {
					ms().startMenu_cursorPosition--;
					mmEffectEx(&snd_select);
				}
			}

			if (pressed & KEY_DOWN) {
				if (ms().startMenu_cursorPosition == 1 || ms().startMenu_cursorPosition == 3) {
					ms().startMenu_cursorPosition += 2;
					mmEffectEx(&snd_select);
				} else if (ms().startMenu_cursorPosition >= 0 && ms().startMenu_cursorPosition <= 3) {
					ms().startMenu_cursorPosition++;
					mmEffectEx(&snd_select);
				}
			}

			if (pressed & KEY_LEFT) {
				if (ms().startMenu_cursorPosition == 2 || (ms().startMenu_cursorPosition == 5 && isDSiMode() && ms().consoleModel < 2)
				|| ms().startMenu_cursorPosition == 6) {
					ms().startMenu_cursorPosition--;
					mmEffectEx(&snd_select);
				}
			}

			if (pressed & KEY_RIGHT) {
				if (ms().startMenu_cursorPosition == 1 || ms().startMenu_cursorPosition == 4 || ms().startMenu_cursorPosition == 5) {
					ms().startMenu_cursorPosition++;
					mmEffectEx(&snd_select);
				}
			}

			if (pressed & KEY_TOUCH) {
				if (touch.px >= 33 && touch.px <= 221 && touch.py >= 25 && touch.py <= 69) {
					ms().startMenu_cursorPosition = 0;
					menuButtonPressed = true;
				} else if (touch.px >= 33 && touch.px <= 125 && touch.py >= 73 && touch.py <= 117) {
					ms().startMenu_cursorPosition = 1;
					menuButtonPressed = true;
				} else if (touch.px >= 129 && touch.px <= 221 && touch.py >= 73 && touch.py <= 117) {
					ms().startMenu_cursorPosition = 2;
					menuButtonPressed = true;
				} else if (touch.px >= 33 && touch.px <= 221 && touch.py >= 121 && touch.py <= 165) {
					ms().startMenu_cursorPosition = 3;
					menuButtonPressed = true;
				} else if (touch.px >= 10 && touch.px <= 20 && touch.py >= 175 && touch.py <= 185
							&& isDSiMode() && ms().consoleModel < 2)
				{
					ms().startMenu_cursorPosition = 4;
					menuButtonPressed = true;
				} else if (touch.px >= 117 && touch.px <= 137 && touch.py >= 170 && touch.py <= 190) {
					ms().startMenu_cursorPosition = 5;
					menuButtonPressed = true;
				} else if (touch.px >= 235 && touch.px <= 244 && touch.py >= 175 && touch.py <= 185) {
					ms().startMenu_cursorPosition = 6;
					menuButtonPressed = true;
				}
			}

			if (pressed & KEY_A) {
				menuButtonPressed = true;
			}

			if (ms().startMenu_cursorPosition < 0) ms().startMenu_cursorPosition = 0;
			if (ms().startMenu_cursorPosition > 6) ms().startMenu_cursorPosition = 6;

			if (menuButtonPressed) {
				switch (ms().startMenu_cursorPosition) {
					case -1:
					default:
						break;
					case 0:
						if (flashcardFound()) {
							// Launch last-run ROM (Secondary)
						  if (ms().launchType[1] == 0) {
							showCursor = false;
							fadeType = false;	// Fade to white
							mmEffectEx(&snd_launch);
							moveIconUp[0] = true;
							for (int i = 0; i < 50; i++) {
								swiWaitForVBlank();
							}
							loadROMselect();
						  } else if (ms().launchType[1] > 0) {
							showCursor = false;
							fadeType = false;	// Fade to white
							mmEffectEx(&snd_launch);
							moveIconUp[0] = true;
							if (romFound[1]) {
								applaunch = true;
							} else {
								for (int i = 0; i < 50; i++) {
									swiWaitForVBlank();
								}
								loadROMselect();
							}
						  }
						  ms().secondaryDevice = true;
						} else if (!flashcardFound() && REG_SCFG_MC != 0x11) {
							// Launch Slot-1
							showCursor = false;
							fadeType = false;	// Fade to white
							mmEffectEx(&snd_launch);
							moveIconUp[0] = true;
							for (int i = 0; i < 50; i++) {
								swiWaitForVBlank();
							}
							ms().slot1Launched = true;
							ms().saveSettings();

							if (ms().slot1LaunchMethod == 0 || arm7SCFGLocked) {
								dsCardLaunch();
							} else if (ms().slot1LaunchMethod == 2) {
								unlaunchRomBoot("cart:");
							} else {
								directCardLaunch();
							}
						} else {
							mmEffectEx(&snd_wrong);
						}
						break;
					case 1:
						if (pictochatFound) {
							showCursor = false;
							fadeType = false;	// Fade to white
							mmEffectEx(&snd_launch);
							moveIconUp[1] = true;
							for (int i = 0; i < 50; i++) {
								swiWaitForVBlank();
							}

							// Clear screen with white
							whiteScreen = true;
							controlTopBright = false;
							clearText();

							if (pictochatReboot) {
								*(u32 *)(0x02000300) = 0x434E4C54; // Set "CNLT" warmboot flag
								*(u16 *)(0x02000304) = 0x1801;

								switch (ms().sysRegion) {
									case 4:
										*(u32 *)(0x02000308) = 0x484E4543;
										*(u32 *)(0x0200030C) = 0x00030005;
										*(u32 *)(0x02000310) = 0x484E4543;
										break;
									case 5:
										*(u32 *)(0x02000308) = 0x484E454B;
										*(u32 *)(0x0200030C) = 0x00030005;
										*(u32 *)(0x02000310) = 0x484E454B;
										break;
									default:
										*(u32 *)(0x02000308) = 0x484E4541;	// "HNEA"
										*(u32 *)(0x0200030C) = 0x00030005;
										*(u32 *)(0x02000310) = 0x484E4541;	// "HNEA"
								}

								*(u32 *)(0x02000314) = 0x00030005;
								*(u32 *)(0x02000318) = 0x00000017;
								*(u32 *)(0x0200031C) = 0x00000000;
								while (*(u16 *)(0x02000306) == 0x0000)
								{ // Keep running, so that CRC16 isn't 0
									*(u16 *)(0x02000306) = swiCRC16(0xFFFF, (void *)0x02000308, 0x18);
								}

								if (ms().consoleModel < 2) {
									unlaunchSetHiyaBoot();
								}

								fifoSendValue32(FIFO_USER_02, 1); // Reboot into DSiWare title, booted via Launcher
								for (int i = 0; i < 15; i++) swiWaitForVBlank();
							} else {
								if (sdFound()) {
									chdir("sd:/");
								}
								int err = runNdsFile (pictochatPath, 0, NULL, true, true, true, false, false, ms().gameLanguage);
								char text[64];
								snprintf (text, sizeof(text), STR_START_FAILED_ERROR.c_str(), err);
								clearText();
								ClearBrightness();
								printSmall(false, 4, 4, text);
								stop();
							}
						} else {
							mmEffectEx(&snd_wrong);
						}
						break;
					case 2:
						if (dlplayFound) {
							showCursor = false;
							fadeType = false;	// Fade to white
							mmEffectEx(&snd_launch);
							moveIconUp[2] = true;
							for (int i = 0; i < 50; i++) {
								swiWaitForVBlank();
							}

							// Clear screen with white
							whiteScreen = true;
							controlTopBright = false;
							clearText();

							if (dlplayReboot) {
								*(u32 *)(0x02000300) = 0x434E4C54; // Set "CNLT" warmboot flag
								*(u16 *)(0x02000304) = 0x1801;

								switch (ms().sysRegion) {
									case 4:
										*(u32 *)(0x02000308) = 0x484E4443;
										*(u32 *)(0x0200030C) = 0x00030005;
										*(u32 *)(0x02000310) = 0x484E4443;
										break;
									case 5:
										*(u32 *)(0x02000308) = 0x484E444B;
										*(u32 *)(0x0200030C) = 0x00030005;
										*(u32 *)(0x02000310) = 0x484E444B;
										break;
									default:
										*(u32 *)(0x02000308) = 0x484E4441;	// "HNDA"
										*(u32 *)(0x0200030C) = 0x00030005;
										*(u32 *)(0x02000310) = 0x484E4441;	// "HNDA"
								}

								*(u32 *)(0x02000314) = 0x00030005;
								*(u32 *)(0x02000318) = 0x00000017;
								*(u32 *)(0x0200031C) = 0x00000000;
								while (*(u16 *)(0x02000306) == 0x0000)
								{ // Keep running, so that CRC16 isn't 0
									*(u16 *)(0x02000306) = swiCRC16(0xFFFF, (void *)0x02000308, 0x18);
								}

								if (ms().consoleModel < 2) {
									unlaunchSetHiyaBoot();
								}

								fifoSendValue32(FIFO_USER_02, 1); // Reboot into DSiWare title, booted via Launcher
								for (int i = 0; i < 15; i++) swiWaitForVBlank();
							} else {
								if (sdFound()) {
									chdir("sd:/");
								}
								int err = runNdsFile (dlplayPath, 0, NULL, true, true, true, false, false, ms().gameLanguage);
								char text[64];
								snprintf (text, sizeof(text), STR_START_FAILED_ERROR.c_str(), err);
								clearText();
								ClearBrightness();
								printSmall(false, 4, 4, text);
								stop();
							}
						} else {
							mmEffectEx(&snd_wrong);
						}
						break;
					case 3:
						if (sdFound()) {
							// Launch last-run ROM (SD)
						  if (ms().launchType[0] == 0) {
							showCursor = false;
							fadeType = false;	// Fade to white
							mmEffectEx(&snd_launch);
							moveIconUp[3] = true;
							for (int i = 0; i < 50; i++) {
								swiWaitForVBlank();
							}
							loadROMselect();
						  } else if (ms().launchType[0] > 0) {
							showCursor = false;
							fadeType = false;	// Fade to white
							mmEffectEx(&snd_launch);
							moveIconUp[3] = true;
							for (int i = 0; i < 50; i++) {
								swiWaitForVBlank();
							}
							if (romFound[0]) {
								applaunch = true;
							} else {
								loadROMselect();
							}
						  }
						  ms().secondaryDevice = false;
						} else if (isRegularDS) {
							// Switch to GBA mode
							showCursor = false;
							fadeType = false;	// Fade to white
							mmEffectEx(&snd_launch);
							moveIconUp[3] = true;
							for (int i = 0; i < 50; i++) {
								swiWaitForVBlank();
							}
							gbaSwitch();
						} else {
							mmEffectEx(&snd_wrong);
						} 
						break;
					case 4:
						// Adjust backlight level
						if (isDSiMode() && ms().consoleModel < 2) {
							fifoSendValue32(FIFO_USER_04, 1);
							mmEffectEx(&snd_backlight);
						}
						break;
					case 5:
						// Launch settings
						showCursor = false;
						fadeType = false;	// Fade to white
						mmEffectEx(&snd_launch);
						moveIconUp[5] = true;
						for (int i = 0; i < 50; i++) {
							swiWaitForVBlank();
						}

						ms().gotosettings = true;
						//SaveSettings();
						if (!isDSiMode()) {
							chdir("fat:/");
						} else if (sdFound()) {
							chdir("sd:/");
						}
						int err = runNdsFile ("/_nds/TWiLightMenu/settings.srldr", 0, NULL, true, false, false, true, true, -1);
						iprintf (STR_START_FAILED_ERROR.c_str(), err);
						break;
				}
				if (ms().startMenu_cursorPosition == 6) {
					// Open manual
					showCursor = false;
					fadeType = false;	// Fade to white
					mmEffectEx(&snd_launch);
					moveIconUp[6] = true;
					for (int i = 0; i < 50; i++) {
						swiWaitForVBlank();
					}
					if (!isDSiMode()) {
						chdir("fat:/");
					} else if (sdFound()) {
						chdir("sd:/");
					}
					int err = runNdsFile ("/_nds/TWiLightMenu/manual.srldr", 0, NULL, true, false, false, true, true, -1);
					iprintf (STR_START_FAILED_ERROR.c_str(), err);
				}

				menuButtonPressed = false;
			}

			if (pressed & KEY_B) {
				mmEffectEx(&snd_back);
				fadeType = false;	// Fade to white
				for (int i = 0; i < 50; i++) swiWaitForVBlank();
				loadROMselect();
			}

			if ((pressed & KEY_X) && !isRegularDS) {
				mmEffectEx(&snd_back);
				fadeType = false;	// Fade to white
				for (int i = 0; i < 50; i++) swiWaitForVBlank();
				if (!sdFound() || ms().launcherApp == -1) {
					*(u32*)(0x02000300) = 0x434E4C54;	// Set "CNLT" warmboot flag
					*(u16*)(0x02000304) = 0x1801;
					*(u32*)(0x02000310) = 0x4D454E55;	// "MENU"
					unlaunchSetHiyaBoot();
				} else {
					unlaunchRomBoot(launcherPath);
				}
				fifoSendValue32(FIFO_USER_02, 1);	// ReturntoDSiMenu
			}

		}

		////////////////////////////////////
		// Launch the item

		if (applaunch) {
			// Delete previously launched DSiWare copied from flashcard to SD
			if (!ms().gotosettings) {
				if (access("sd:/_nds/TWiLightMenu/tempDSiWare.dsi", F_OK) == 0) {
					remove("sd:/_nds/TWiLightMenu/tempDSiWare.dsi");
				}
				if (access("sd:/_nds/TWiLightMenu/tempDSiWare.pub.bak", F_OK) == 0) {
					remove("sd:/_nds/TWiLightMenu/tempDSiWare.pub.bak");
				}
				if (access("sd:/_nds/TWiLightMenu/tempDSiWare.prv.bak", F_OK) == 0) {
					remove("sd:/_nds/TWiLightMenu/tempDSiWare.prv.bak");
				}
				if (access("sd:/_nds/nds-bootstrap/patchOffsetCache/tempDSiWare.bin", F_OK) == 0) {
					remove("sd:/_nds/nds-bootstrap/patchOffsetCache/tempDSiWare.bin");
				}
			}

			chdir(romfolder[ms().secondaryDevice].c_str());

			// Construct a command line
			getcwd (filePath, PATH_MAX);
			int pathLen = strlen(filePath);
			vector<char*> argarray;

			if (extention(filename[ms().secondaryDevice], ".argv"))
			{
				FILE *argfile = fopen(filename[ms().secondaryDevice].c_str(),"rb");
					char str[PATH_MAX], *pstr;
				const char seps[]= "\n\r\t ";

				while( fgets(str, PATH_MAX, argfile) ) {
					// Find comment and end string there
					if( (pstr = strchr(str, '#')) )
						*pstr= '\0';

					// Tokenize arguments
					pstr= strtok(str, seps);

					while( pstr != NULL ) {
						argarray.push_back(strdup(pstr));
						pstr= strtok(NULL, seps);
					}
				}
				fclose(argfile);
				filename[ms().secondaryDevice] = argarray.at(0);
			} else {
				argarray.push_back(strdup(filename[ms().secondaryDevice].c_str()));
			}

			ms().slot1Launched = false;

			// Launch DSiWare .nds via Unlaunch
			if ((isDSiMode() || sdFound()) && isDSiWare[ms().secondaryDevice]) {
				const char *typeToReplace = ".nds";
				if (extention(filename[ms().secondaryDevice], ".dsi")) {
					typeToReplace = ".dsi";
				} else if (extention(filename[ms().secondaryDevice], ".ids")) {
					typeToReplace = ".ids";
				} else if (extention(filename[ms().secondaryDevice], ".srl")) {
					typeToReplace = ".srl";
				} else if (extention(filename[ms().secondaryDevice], ".app")) {
					typeToReplace = ".app";
				}

				char *name = argarray.at(0);
				strcpy (filePath + pathLen, name);
				free(argarray.at(0));
				argarray.at(0) = filePath;

				std::string romFolderNoSlash = romfolder[ms().secondaryDevice];
				RemoveTrailingSlashes(romFolderNoSlash);
				mkdir ("saves", 0777);

				ms().dsiWareSrlPath = std::string(argarray[0]);
				ms().dsiWarePubPath = romFolderNoSlash + "/saves/" + filename[ms().secondaryDevice];
				ms().dsiWarePubPath = replaceAll(ms().dsiWarePubPath, typeToReplace, (strncmp(gameTid[ms().secondaryDevice], "Z2E", 3) == 0 && ms().secondaryDevice && (!ms().dsiWareToSD || (isDSiMode() && memcmp(io_dldi_data->friendlyName, "CycloDS iEvolution", 18) == 0))) ? getSavExtension() : getPubExtension());
				ms().dsiWarePrvPath = romFolderNoSlash + "/saves/" + filename[ms().secondaryDevice];
				ms().dsiWarePrvPath = replaceAll(ms().dsiWarePrvPath, typeToReplace, getPrvExtension());
				ms().homebrewBootstrap = isHomebrew[ms().secondaryDevice];
				ms().launchType[ms().secondaryDevice] = 3;
				ms().saveSettings();

				sNDSHeaderExt NDSHeader;

				FILE *f_nds_file = fopen(filename[ms().secondaryDevice].c_str(), "rb");

				fread(&NDSHeader, 1, sizeof(NDSHeader), f_nds_file);
				fclose(f_nds_file);

				if ((getFileSize(ms().dsiWarePubPath.c_str()) == 0) && (NDSHeader.pubSavSize > 0)) {
					while (!screenFadedOut()) {
						swiWaitForVBlank();
					}
					whiteScreen = true;
					fadeSpeed = true;
					controlTopBright = false;
					clearText();
					if (memcmp(io_dldi_data->friendlyName, "CycloDS iEvolution", 18) == 0) {
						printSmall(false, 0, 20, STR_TAKEWHILE_RESTART, Alignment::center);
					} else if (ms().consoleModel >= 2) {
						printSmall(false, 0, 20, STR_TAKEWHILE_PRESSHOME, Alignment::center);
					} else {
						printSmall(false, 0, 20, STR_TAKEWHILE_CLOSELID, Alignment::center);
					}
					printSmall(false, 2, 80, STR_CREATING_PUBLIC_SAVE);
					if (!fadeType) {
						fadeType = true;	// Fade in from white
						for (int i = 0; i < 35; i++) swiWaitForVBlank();
					}

					static const int BUFFER_SIZE = 4096;
					char buffer[BUFFER_SIZE];
					memset(buffer, 0, sizeof(buffer));
					char savHdrPath[64];
					snprintf(savHdrPath, sizeof(savHdrPath), "nitro:/DSiWareSaveHeaders/%X.savhdr", (unsigned int)NDSHeader.pubSavSize);
					FILE *hdrFile = fopen(savHdrPath, "rb");
					if (hdrFile) fread(buffer, 1, 0x200, hdrFile);
					fclose(hdrFile);

					FILE *pFile = fopen(ms().dsiWarePubPath.c_str(), "wb");
					if (pFile) {
						fwrite(buffer, 1, sizeof(buffer), pFile);
						fseek(pFile, NDSHeader.pubSavSize - 1, SEEK_SET);
						fputc('\0', pFile);
						fclose(pFile);
					}
					printSmall(false, 2, 88, STR_PUBLIC_SAVE_CREATED);
					for (int i = 0; i < 60; i++) swiWaitForVBlank();
				}

				if ((getFileSize(ms().dsiWarePrvPath.c_str()) == 0) && (NDSHeader.prvSavSize > 0)) {
					while (!fadeType && !screenFadedOut()) {
						swiWaitForVBlank();
					}
					whiteScreen = true;
					fadeSpeed = true;
					controlTopBright = false;
					clearText();
					if (memcmp(io_dldi_data->friendlyName, "CycloDS iEvolution", 18) == 0) {
						printSmall(false, 0, 20, STR_TAKEWHILE_RESTART, Alignment::center);
					} else if (ms().consoleModel >= 2) {
						printSmall(false, 0, 20, STR_TAKEWHILE_PRESSHOME, Alignment::center);
					} else {
						printSmall(false, 0, 20, STR_TAKEWHILE_CLOSELID, Alignment::center);
					}
					printSmall(false, 2, 80, STR_CREATING_PRIVATE_SAVE);
					if (!fadeType) {
						fadeType = true;	// Fade in from white
						for (int i = 0; i < 35; i++) swiWaitForVBlank();
					}

					static const int BUFFER_SIZE = 4096;
					char buffer[BUFFER_SIZE];
					memset(buffer, 0, sizeof(buffer));
					char savHdrPath[64];
					snprintf(savHdrPath, sizeof(savHdrPath), "nitro:/DSiWareSaveHeaders/%X.savhdr", (unsigned int)NDSHeader.prvSavSize);
					FILE *hdrFile = fopen(savHdrPath, "rb");
					if (hdrFile) fread(buffer, 1, 0x200, hdrFile);
					fclose(hdrFile);

					FILE *pFile = fopen(ms().dsiWarePrvPath.c_str(), "wb");
					if (pFile) {
						fwrite(buffer, 1, sizeof(buffer), pFile);
						fseek(pFile, NDSHeader.prvSavSize - 1, SEEK_SET);
						fputc('\0', pFile);
						fclose(pFile);
					}
					printSmall(false, 2, 88, STR_PRIVATE_SAVE_CREATED);
					for (int i = 0; i < 60; i++) swiWaitForVBlank();
				}

				fadeType = false;	// Fade to white

				if (ms().secondaryDevice && (ms().dsiWareToSD || (!ms().dsiWareBooter && ms().consoleModel == 0)) && sdFound()) {
					while (!fadeType && !screenFadedOut()) {
						swiWaitForVBlank();
					}
					whiteScreen = true;
					fadeSpeed = true;
					controlTopBright = false;
					clearText();
					printSmall(false, 0, 20, ms().consoleModel >= 2 ? STR_TAKEWHILE_PRESSHOME : STR_TAKEWHILE_CLOSELID, Alignment::center);
					printSmall(false, 0, 86, STR_NOW_COPYING_DATA, Alignment::center);
					printSmall(false, 0, 100, STR_DO_NOT_TURN_OFF_POWER, Alignment::center);
					fadeType = true;	// Fade in from white
					for (int i = 0; i < 35; i++) swiWaitForVBlank();
					fcopy(ms().dsiWareSrlPath.c_str(), "sd:/_nds/TWiLightMenu/tempDSiWare.dsi");
					if ((access(ms().dsiWarePubPath.c_str(), F_OK) == 0) && (NDSHeader.pubSavSize > 0)) {
						fcopy(ms().dsiWarePubPath.c_str(), "sd:/_nds/TWiLightMenu/tempDSiWare.pub");
					}
					if ((access(ms().dsiWarePrvPath.c_str(), F_OK) == 0) && (NDSHeader.prvSavSize > 0)) {
						fcopy(ms().dsiWarePrvPath.c_str(), "sd:/_nds/TWiLightMenu/tempDSiWare.prv");
					}
					fadeType = false;	// Fade to white

					if ((access(ms().dsiWarePubPath.c_str(), F_OK) == 0 && (NDSHeader.pubSavSize > 0))
					 || (access(ms().dsiWarePrvPath.c_str(), F_OK) == 0 && (NDSHeader.prvSavSize > 0))) {
						for (int i = 0; i < 25; i++) swiWaitForVBlank();
						clearText();
						printSmall(false, 0, 8, STR_RESTART_AFTER_SAVE, Alignment::center);
						fadeType = true;	// Fade in from white
						for (int i = 0; i < 60*3; i++) swiWaitForVBlank();		// Wait 3 seconds
						fadeType = false;	// Fade to white
						for (int i = 0; i < 25; i++) swiWaitForVBlank();
					}
				}

				if (ms().dsiWareBooter || (memcmp(io_dldi_data->friendlyName, "CycloDS iEvolution", 18) == 0) || ms().consoleModel > 0) {
					CheatCodelist codelist;
					u32 gameCode, crc32;

					bool cheatsEnabled = true;
					const char* cheatDataBin = (ms().secondaryDevice && ms().dsiWareToSD && sdFound()) ? "sd:/_nds/nds-bootstrap/cheatData.bin" : "/_nds/nds-bootstrap/cheatData.bin";
					mkdir((ms().secondaryDevice && ms().dsiWareToSD && sdFound()) ? "sd:/_nds" : "/_nds", 0777);
					mkdir((ms().secondaryDevice && ms().dsiWareToSD && sdFound()) ? "sd:/_nds/nds-bootstrap" : "/_nds/nds-bootstrap", 0777);
					if(codelist.romData(ms().dsiWareSrlPath,gameCode,crc32)) {
						long cheatOffset; size_t cheatSize;
						FILE* dat=fopen(sdFound() ? "sd:/_nds/TWiLightMenu/extras/usrcheat.dat" : "fat:/_nds/TWiLightMenu/extras/usrcheat.dat","rb");
						if (dat) {
							if (codelist.searchCheatData(dat, gameCode, crc32, cheatOffset, cheatSize)) {
								codelist.parse(ms().dsiWareSrlPath);
								codelist.writeCheatsToFile(cheatDataBin);
								FILE* cheatData=fopen(cheatDataBin,"rb");
								if (cheatData) {
									u32 check[2];
									fread(check, 1, 8, cheatData);
									fclose(cheatData);
									if (check[1] == 0xCF000000 || getFileSize(cheatDataBin) > 0x1C00) {
										cheatsEnabled = false;
									}
								}
							} else {
								cheatsEnabled = false;
							}
							fclose(dat);
						} else {
							cheatsEnabled = false;
						}
					} else {
						cheatsEnabled = false;
					}
					if (!cheatsEnabled) {
						remove(cheatDataBin);
					}
				}

				if ((ms().dsiWareBooter || (memcmp(io_dldi_data->friendlyName, "CycloDS iEvolution", 18) == 0) || ms().consoleModel > 0) && !ms().homebrewBootstrap) {
					// Use nds-bootstrap
					loadPerGameSettings(filename[ms().secondaryDevice]);

					char sfnSrl[62];
					char sfnPub[62];
					char sfnPrv[62];
					if (ms().secondaryDevice && ms().dsiWareToSD && sdFound()) {
						fatGetAliasPath("sd:/", "sd:/_nds/TWiLightMenu/tempDSiWare.dsi", sfnSrl);
						fatGetAliasPath("sd:/", "sd:/_nds/TWiLightMenu/tempDSiWare.pub", sfnPub);
						fatGetAliasPath("sd:/", "sd:/_nds/TWiLightMenu/tempDSiWare.prv", sfnPrv);
					} else {
						fatGetAliasPath(ms().secondaryDevice ? "fat:/" : "sd:/", ms().dsiWareSrlPath.c_str(), sfnSrl);
						fatGetAliasPath(ms().secondaryDevice ? "fat:/" : "sd:/", ms().dsiWarePubPath.c_str(), sfnPub);
						fatGetAliasPath(ms().secondaryDevice ? "fat:/" : "sd:/", ms().dsiWarePrvPath.c_str(), sfnPrv);
					}

					bootstrapinipath = (memcmp(io_dldi_data->friendlyName, "CycloDS iEvolution", 18) == 0) ? "fat:/_nds/nds-bootstrap.ini" : "sd:/_nds/nds-bootstrap.ini";
					CIniFile bootstrapini(bootstrapinipath);
					bootstrapini.SetString("NDS-BOOTSTRAP", "NDS_PATH", ms().secondaryDevice && ms().dsiWareToSD && sdFound() ? "sd:/_nds/TWiLightMenu/tempDSiWare.dsi" : ms().dsiWareSrlPath);
					bootstrapini.SetString("NDS-BOOTSTRAP", "APP_PATH", sfnSrl);
					bootstrapini.SetString("NDS-BOOTSTRAP", "SAV_PATH", sfnPub);
					bootstrapini.SetString("NDS-BOOTSTRAP", "PRV_PATH", sfnPrv);
					bootstrapini.SetString("NDS-BOOTSTRAP", "AP_FIX_PATH", "");
					bootstrapini.SetString("NDS-BOOTSTRAP", "GUI_LANGUAGE", ms().getGuiLanguageString());
					bootstrapini.SetInt("NDS-BOOTSTRAP", "LANGUAGE", perGameSettings_language == -2 ? ms().gameLanguage : perGameSettings_language);
					bootstrapini.SetInt("NDS-BOOTSTRAP", "REGION", perGameSettings_region == -3 ? ms().gameRegion : perGameSettings_region);
					bootstrapini.SetInt("NDS-BOOTSTRAP", "DSI_MODE", true);
					bootstrapini.SetInt("NDS-BOOTSTRAP", "BOOST_CPU", true);
					bootstrapini.SetInt("NDS-BOOTSTRAP", "BOOST_VRAM", true);
					bootstrapini.SetInt("NDS-BOOTSTRAP", "DONOR_SDK_VER", 5);
					bootstrapini.SetInt("NDS-BOOTSTRAP", "GAME_SOFT_RESET", 1);
					bootstrapini.SetInt("NDS-BOOTSTRAP", "PATCH_MPU_REGION", 0);
					bootstrapini.SetInt("NDS-BOOTSTRAP", "PATCH_MPU_SIZE", 0);
					bootstrapini.SetInt("NDS-BOOTSTRAP", "FORCE_SLEEP_PATCH", 
						(ms().forceSleepPatch
					|| (memcmp(io_dldi_data->friendlyName, "TTCARD", 6) == 0 && !isRegularDS)
					|| (memcmp(io_dldi_data->friendlyName, "DSTT", 4) == 0 && !isRegularDS)
					|| (memcmp(io_dldi_data->friendlyName, "DEMON", 5) == 0 && !isRegularDS)
					|| (memcmp(io_dldi_data->friendlyName, "R4iDSN", 6) == 0 && !isRegularDS))
					);
					bootstrapini.SaveIniFile(bootstrapinipath);

					while (!screenFadedOut()) {
						swiWaitForVBlank();
					}

					bool useNightly = (perGameSettings_bootstrapFile == -1 ? ms().bootstrapFile : perGameSettings_bootstrapFile);
					bool useWidescreen = (perGameSettings_wideScreen == -1 ? ms().wideScreen : perGameSettings_wideScreen);

					if (!isDSiMode() && (!ms().secondaryDevice || (ms().secondaryDevice && ms().dsiWareToSD))) {
						*(u32*)(0x02000000) |= BIT(3);
						*(u32*)(0x02000004) = 0;
						*(bool*)(0x02000010) = useNightly;
						*(bool*)(0x02000014) = useWidescreen;
					}
					if (isDSiMode() || !ms().secondaryDevice) {
						SetWidescreen(filename[ms().secondaryDevice].c_str());
					}
					if (!isDSiMode() && (!ms().secondaryDevice || (ms().secondaryDevice && ms().dsiWareToSD))) {
						ntrStartSdGame();
					}

					char ndsToBoot[256];
					sprintf(ndsToBoot, "sd:/_nds/nds-bootstrap-%s.nds", useNightly ? "nightly" : "release");
					if(access(ndsToBoot, F_OK) != 0) {
						sprintf(ndsToBoot, "fat:/_nds/nds-bootstrap-%s.nds", useNightly ? "nightly" : "release");
					}

					argarray.at(0) = (char *)ndsToBoot;
					int err = runNdsFile(argarray[0], argarray.size(), (const char **)&argarray[0], true, true, false, true, true, -1);
					char text[64];
					snprintf (text, sizeof(text), STR_START_FAILED_ERROR.c_str(), err);
					clearText();
					printSmall(false, 4, 4, text);
					if (err == 1) {
						printSmall(false, 4, 24, useNightly ? STR_BOOTSTRAP_NIGHTLY_NOT_FOUND : STR_BOOTSTRAP_RELEASE_NOT_FOUND);
					}
					whiteScreen = true;
					fadeSpeed = true;
					controlTopBright = false;
					fadeType = true; // Fade in
					stop();
				}

				// Move .pub and/or .prv out of "saves" folder
				std::string pubnameUl = replaceAll(filename[ms().secondaryDevice], typeToReplace, ".pub");
				std::string prvnameUl = replaceAll(filename[ms().secondaryDevice], typeToReplace, ".prv");
				std::string pubpathUl = romFolderNoSlash + "/" + pubnameUl;
				std::string prvpathUl = romFolderNoSlash + "/" + prvnameUl;
				if (access(ms().dsiWarePubPath.c_str(), F_OK) == 0)
				{
					rename(ms().dsiWarePubPath.c_str(), pubpathUl.c_str());
				}
				if (access(ms().dsiWarePrvPath.c_str(), F_OK) == 0)
				{
					rename(ms().dsiWarePrvPath.c_str(), prvpathUl.c_str());
				}

				while (!screenFadedOut()) {
					swiWaitForVBlank();
				}

				unlaunchRomBoot(ms().secondaryDevice ? "sdmc:/_nds/TWiLightMenu/tempDSiWare.dsi" : ms().dsiWareSrlPath.c_str());
			}

			// Launch .nds directly or via nds-bootstrap
			if (extention(filename[ms().secondaryDevice], ".nds") || extention(filename[ms().secondaryDevice], ".dsi")
			 || extention(filename[ms().secondaryDevice], ".ids") || extention(filename[ms().secondaryDevice], ".srl")
			 || extention(filename[ms().secondaryDevice], ".app")) {
				const char *typeToReplace = ".nds";
				if (extention(filename[ms().secondaryDevice], ".dsi")) {
					typeToReplace = ".dsi";
				} else if (extention(filename[ms().secondaryDevice], ".ids")) {
					typeToReplace = ".ids";
				} else if (extention(filename[ms().secondaryDevice], ".srl")) {
					typeToReplace = ".srl";
				} else if (extention(filename[ms().secondaryDevice], ".app")) {
					typeToReplace = ".app";
				}

				bool dsModeSwitch = false;
				bool dsModeDSiWare = false;

				FILE *f_nds_file = fopen(argarray[0], "rb");
				bool dsiBinariesFound = checkDsiBinaries(f_nds_file);
				fclose(f_nds_file);

				if (memcmp(gameTid[ms().secondaryDevice], "HND", 3) == 0 || memcmp(gameTid[ms().secondaryDevice], "HNE", 3) == 0) {
					dsModeSwitch = true;
					dsModeDSiWare = true;
					useBackend = false;	// Bypass nds-bootstrap
					ms().homebrewBootstrap = true;
				} else if (isHomebrew[ms().secondaryDevice]) {
					loadPerGameSettings(filename[ms().secondaryDevice]);
					if (perGameSettings_directBoot || (ms().useBootstrap && ms().secondaryDevice)) {
						useBackend = false;	// Bypass nds-bootstrap
					} else {
						useBackend = true;
					}
					if (isDSiMode() && !perGameSettings_dsiMode) {
						dsModeSwitch = true;
					}
					ms().homebrewBootstrap = true;
				} else {
					loadPerGameSettings(filename[ms().secondaryDevice]);
					useBackend = true;
					ms().homebrewBootstrap = false;
				}

				char *name = argarray.at(0);
				strcpy (filePath + pathLen, name);
				free(argarray.at(0));
				argarray.at(0) = filePath;
				if(useBackend) {
					if((ms().useBootstrap || !ms().secondaryDevice) || (dsiFeatures() && unitCode[ms().secondaryDevice] > 0 && (perGameSettings_dsiMode == -1 ? ms().bstrap_dsiMode : perGameSettings_dsiMode))) {
						std::string path = argarray[0];
						std::string savename = replaceAll(filename[ms().secondaryDevice], typeToReplace, getSavExtension());
						std::string ramdiskname = replaceAll(filename[ms().secondaryDevice], typeToReplace, getImgExtension());
						std::string romFolderNoSlash = romfolder[ms().secondaryDevice];
						RemoveTrailingSlashes(romFolderNoSlash);
						mkdir (isHomebrew[ms().secondaryDevice] ? "ramdisks" : "saves", 0777);
						std::string savepath = romFolderNoSlash+"/saves/"+savename;
						if (sdFound() && ms().secondaryDevice && ms().fcSaveOnSd) {
							savepath = replaceAll(savepath, "fat:/", "sd:/");
						}
						std::string ramdiskpath = romFolderNoSlash+"/ramdisks/"+ramdiskname;

						if (!isHomebrew[ms().secondaryDevice] && (strncmp(gameTid[ms().secondaryDevice], "NTR", 3) != 0)) {
							// Create or expand save if game isn't homebrew
							u32 orgsavesize = getFileSize(savepath.c_str());
							u32 savesize = 524288;	// 512KB (default size)

							u32 gameTidHex = 0;
							tonccpy(&gameTidHex, &gameTid[ms().secondaryDevice], 4);

							for (int i = 0; i < (int)sizeof(ROMList)/12; i++) {
								ROMListEntry* curentry = &ROMList[i];
								if (gameTidHex == curentry->GameCode) {
									if (curentry->SaveMemType != 0xFFFFFFFF) savesize = sramlen[curentry->SaveMemType];
									break;
								}
							}

							bool saveSizeFixNeeded = false;

							// TODO: If the list gets large enough, switch to bsearch().
							for (unsigned int i = 0; i < sizeof(saveSizeFixList) / sizeof(saveSizeFixList[0]); i++) {
								if (memcmp(gameTid[ms().secondaryDevice], saveSizeFixList[i], 3) == 0) {
									// Found a match.
									saveSizeFixNeeded = true;
									break;
								}
							}

							if ((orgsavesize == 0 && savesize > 0) || (orgsavesize < savesize && saveSizeFixNeeded)) {
								while (!screenFadedOut()) {
									swiWaitForVBlank();
								}
								whiteScreen = true;
								fadeSpeed = true; // Fast fading
								clearText();
								if (isDSiMode() && memcmp(io_dldi_data->friendlyName, "CycloDS iEvolution", 18) == 0) {
									printSmall(false, 0, 20, STR_TAKEWHILE_RESTART, Alignment::center);
								} else if (dsiFeatures() && ms().consoleModel >= 2) {
									printSmall(false, 0, 20, STR_TAKEWHILE_PRESSHOME, Alignment::center);
								} else {
									printSmall(false, 0, 20, STR_TAKEWHILE_CLOSELID, Alignment::center);
								}
								printSmall(false, 0, 88, (orgsavesize == 0) ? STR_CREATING_SAVE : STR_EXPANDING_SAVE, Alignment::center);

								fadeType = true; // Fade in from white

								if (orgsavesize > 0) {
									fsizeincrease(savepath.c_str(), sdFound() ? "sd:/_nds/TWiLightMenu/temp.sav" : "fat:/_nds/TWiLightMenu/temp.sav", savesize);
								} else {
									FILE *pFile = fopen(savepath.c_str(), "wb");
									if (pFile) {
										fseek(pFile, savesize - 1, SEEK_SET);
										fputc('\0', pFile);
										fclose(pFile);
									}
								}
								clearText();
								printSmall(false, 0, 88, (orgsavesize == 0) ? STR_SAVE_CREATED : STR_SAVE_EXPANDED, Alignment::center);
								for (int i = 0; i < 30; i++) swiWaitForVBlank();
								fadeType = false; // Fade out
							}
						}

						SetMPUSettings();

						bool useWidescreen = (perGameSettings_wideScreen == -1 ? ms().wideScreen : perGameSettings_wideScreen);

						bootstrapinipath = ((!ms().secondaryDevice || sdFound()) ? "sd:/_nds/nds-bootstrap.ini" : "fat:/_nds/nds-bootstrap.ini");
						CIniFile bootstrapini( bootstrapinipath );
						bootstrapini.SetString("NDS-BOOTSTRAP", "NDS_PATH", path);
						bootstrapini.SetString("NDS-BOOTSTRAP", "SAV_PATH", savepath);
						if (!isHomebrew[ms().secondaryDevice]) {
							bootstrapini.SetString("NDS-BOOTSTRAP", "AP_FIX_PATH", setApFix(argarray[0]));
						}
						bootstrapini.SetString("NDS-BOOTSTRAP", "HOMEBREW_ARG", (useWidescreen && (gameTid[ms().secondaryDevice][0] == 'W' || romVersion[ms().secondaryDevice] == 0x57)) ? "wide" : "");
						bootstrapini.SetString("NDS-BOOTSTRAP", "RAM_DRIVE_PATH", (perGameSettings_ramDiskNo >= 0 && !ms().secondaryDevice) ? ramdiskpath : "sd:/null.img");
						bootstrapini.SetString("NDS-BOOTSTRAP", "GUI_LANGUAGE", ms().getGuiLanguageString());
						bootstrapini.SetInt("NDS-BOOTSTRAP", "LANGUAGE", perGameSettings_language == -2 ? ms().gameLanguage : perGameSettings_language);
						bootstrapini.SetInt("NDS-BOOTSTRAP", "REGION", perGameSettings_region == -3 ? ms().gameRegion : perGameSettings_region);
						bootstrapini.SetInt("NDS-BOOTSTRAP", "DSI_MODE", !dsiBinariesFound ? 0 : (perGameSettings_dsiMode == -1 ? ms().bstrap_dsiMode : perGameSettings_dsiMode));
						if (dsiFeatures() || !ms().secondaryDevice) {
							bootstrapini.SetInt("NDS-BOOTSTRAP", "BOOST_CPU", setClockSpeed());
							bootstrapini.SetInt("NDS-BOOTSTRAP", "BOOST_VRAM", perGameSettings_boostVram == -1 ? ms().boostVram : perGameSettings_boostVram);
							bootstrapini.SetInt("NDS-BOOTSTRAP", "CARD_READ_DMA", setCardReadDMA());
							bootstrapini.SetInt("NDS-BOOTSTRAP", "ASYNC_CARD_READ", setAsyncReadDMA());
						}
						bootstrapini.SetInt("NDS-BOOTSTRAP", "EXTENDED_MEMORY", perGameSettings_expandRomSpace == -1 ? ms().extendedMemory : perGameSettings_expandRomSpace);
						bootstrapini.SetInt("NDS-BOOTSTRAP", "DONOR_SDK_VER", SetDonorSDK());
						bootstrapini.SetInt("NDS-BOOTSTRAP", "PATCH_MPU_REGION", mpuregion);
						bootstrapini.SetInt("NDS-BOOTSTRAP", "PATCH_MPU_SIZE", mpusize);
						bootstrapini.SetInt("NDS-BOOTSTRAP", "FORCE_SLEEP_PATCH", 
							(ms().forceSleepPatch
						|| (memcmp(io_dldi_data->friendlyName, "TTCARD", 6) == 0 && !isRegularDS)
						|| (memcmp(io_dldi_data->friendlyName, "DSTT", 4) == 0 && !isRegularDS)
						|| (memcmp(io_dldi_data->friendlyName, "DEMON", 5) == 0 && !isRegularDS)
						|| (memcmp(io_dldi_data->friendlyName, "R4iDSN", 6) == 0 && !isRegularDS))
						);
						bootstrapini.SaveIniFile( bootstrapinipath );

						CheatCodelist codelist;
						u32 gameCode,crc32;

						if (!isHomebrew[ms().secondaryDevice]) {
							bool cheatsEnabled = true;
							const char* cheatDataBin = "/_nds/nds-bootstrap/cheatData.bin";
							mkdir("/_nds", 0777);
							mkdir("/_nds/nds-bootstrap", 0777);
							if(codelist.romData(path,gameCode,crc32)) {
								long cheatOffset; size_t cheatSize;
								FILE* dat=fopen(sdFound() ? "sd:/_nds/TWiLightMenu/extras/usrcheat.dat" : "fat:/_nds/TWiLightMenu/extras/usrcheat.dat","rb");
								if (dat) {
									if (codelist.searchCheatData(dat, gameCode,
																 crc32, cheatOffset,
																 cheatSize)) {
										codelist.parse(path);
										codelist.writeCheatsToFile(cheatDataBin);
										FILE* cheatData=fopen(cheatDataBin,"rb");
										if (cheatData) {
											u32 check[2];
											fread(check, 1, 8, cheatData);
											fclose(cheatData);
											if (check[1] == 0xCF000000
											|| getFileSize(cheatDataBin) > 0x8000) {
												cheatsEnabled = false;
											}
										}
									} else {
										cheatsEnabled = false;
									}
									fclose(dat);
								} else {
									cheatsEnabled = false;
								}
							} else {
								cheatsEnabled = false;
							}
							if (!cheatsEnabled) {
								remove(cheatDataBin);
							}
						}

						ms().launchType[ms().secondaryDevice] = 1;
						ms().previousUsedDevice = ms().secondaryDevice;
						ms().saveSettings();

						while (!screenFadedOut()) {
							swiWaitForVBlank();
						}

						if (dsiFeatures() || !ms().secondaryDevice) {
							SetWidescreen(filename[ms().secondaryDevice].c_str());
						}
						if (!isDSiMode() && !ms().secondaryDevice) {
							ntrStartSdGame();
						}

						bool useNightly = (perGameSettings_bootstrapFile == -1 ? ms().bootstrapFile : perGameSettings_bootstrapFile);

						char ndsToBoot[256];
						sprintf(ndsToBoot, "sd:/_nds/nds-bootstrap-%s%s.nds", ms().homebrewBootstrap ? "hb-" : "", useNightly ? "nightly" : "release");
						if(!isDSiMode() || access(ndsToBoot, F_OK) != 0) {
							sprintf(ndsToBoot, "fat:/_nds/nds-bootstrap-%s%s.nds", ms().homebrewBootstrap ? "hb-" : "", useNightly ? "nightly" : "release");
						}

						argarray.at(0) = (char *)ndsToBoot;
						int err = runNdsFile (argarray[0], argarray.size(), (const char **)&argarray[0], (ms().homebrewBootstrap ? false : true), true, false, true, true, -1);
						char text[64];
						snprintf (text, sizeof(text), STR_START_FAILED_ERROR.c_str(), err);
						clearText();
						printSmall(false, 4, 4, text);
						if (err == 1) {
							if(ms().homebrewBootstrap == true) {
								printSmall(false, 4, 24, useNightly ? STR_BOOTSTRAP_HB_NIGHTLY_NOT_FOUND : STR_BOOTSTRAP_HB_RELEASE_NOT_FOUND);
							} else {
								printSmall(false, 4, 24, useNightly ? STR_BOOTSTRAP_NIGHTLY_NOT_FOUND : STR_BOOTSTRAP_RELEASE_NOT_FOUND);
							}
						}
						fadeType = true; // Fade in
						stop();
					} else {
						ms().launchType[ms().secondaryDevice] = 1;
						ms().previousUsedDevice = ms().secondaryDevice;
						ms().saveSettings();

						while (!screenFadedOut()) {
							swiWaitForVBlank();
						}

						loadGameOnFlashcard(argarray[0], true);
					}
				} else {
					ms().homebrewHasWide = (isHomebrew[ms().secondaryDevice] && (gameTid[ms().secondaryDevice][0] == 'W' || romVersion[ms().secondaryDevice] == 0x57));
					ms().launchType[ms().secondaryDevice] = 2;
					ms().previousUsedDevice = ms().secondaryDevice;
					ms().saveSettings();

					while (!screenFadedOut()) {
						swiWaitForVBlank();
					}

					if (!isDSiMode() && !ms().secondaryDevice) {
						ntrStartSdGame();
					}

					int language = perGameSettings_language == -2 ? ms().gameLanguage : perGameSettings_language;
					int gameRegion = perGameSettings_region == -3 ? ms().gameRegion : perGameSettings_region;

					// Set region flag
					if (gameRegion == -2 && gameTid[ms().secondaryDevice][3] != 'A' && gameTid[ms().secondaryDevice][3] != '#') {
						if (gameTid[ms().secondaryDevice][3] == 'J') {
							*(u8*)(0x02FFFD70) = 0;
						} else if (gameTid[ms().secondaryDevice][3] == 'E' || gameTid[ms().secondaryDevice][3] == 'T') {
							*(u8*)(0x02FFFD70) = 1;
						} else if (gameTid[ms().secondaryDevice][3] == 'P' || gameTid[ms().secondaryDevice][3] == 'V') {
							*(u8*)(0x02FFFD70) = 2;
						} else if (gameTid[ms().secondaryDevice][3] == 'U') {
							*(u8*)(0x02FFFD70) = 3;
						} else if (gameTid[ms().secondaryDevice][3] == 'C') {
							*(u8*)(0x02FFFD70) = 4;
						} else if (gameTid[ms().secondaryDevice][3] == 'K') {
							*(u8*)(0x02FFFD70) = 5;
						}
					} else if (gameRegion == -1 || (gameRegion == -2 && (gameTid[ms().secondaryDevice][3] == 'A' || gameTid[ms().secondaryDevice][3] == '#'))) {
					  if (useTwlCfg) {
						u8 country = *(u8*)0x02000405;
						if (country == 0x01) {
							*(u8*)(0x02FFFD70) = 0;	// Japan
						} else if (country == 0xA0) {
							*(u8*)(0x02FFFD70) = 4;	// China
						} else if (country == 0x88) {
							*(u8*)(0x02FFFD70) = 5;	// Korea
						} else if (country == 0x41 || country == 0x5F) {
							*(u8*)(0x02FFFD70) = 3;	// Australia
						} else if ((country >= 0x08 && country <= 0x34) || country == 0x99 || country == 0xA8) {
							*(u8*)(0x02FFFD70) = 1;	// USA
						} else if (country >= 0x40 && country <= 0x70) {
							*(u8*)(0x02FFFD70) = 2;	// Europe
						}
					  } else {
						u8 consoleType = 0;
						readFirmware(0x1D, &consoleType, 1);
						if (consoleType == 0x43 || consoleType == 0x63) {
							*(u8*)(0x02FFFD70) = 4;	// China
						} else if (PersonalData->language == 0) {
							*(u8*)(0x02FFFD70) = 0;	// Japan
						} else if (PersonalData->language == 1 /*|| PersonalData->language == 2 || PersonalData->language == 5*/) {
							*(u8*)(0x02FFFD70) = 1;	// USA
						} else /*if (PersonalData->language == 3 || PersonalData->language == 4)*/ {
							*(u8*)(0x02FFFD70) = 2;	// Europe
						} /*else {
							*(u8*)(0x02FFFD70) = 5;	// Korea
						}*/
					  }
					} else {
						*(u8*)(0x02FFFD70) = gameRegion;
					}

					if (useTwlCfg && language >= 0 && language <= 7 && *(u8*)0x02000406 != language) {
						tonccpy((char*)0x02000600, (char*)0x02000400, 0x200);
						*(u8*)0x02000606 = language;
						*(u32*)0x02FFFDFC = 0x02000600;
					}

					bool useWidescreen = (perGameSettings_wideScreen == -1 ? ms().wideScreen : perGameSettings_wideScreen);

					if (ms().consoleModel >= 2 && useWidescreen && ms().homebrewHasWide) {
						//argarray.push_back((char*)"wide");
						SetWidescreen(NULL);
					}

					bool runNds_boostCpu = false;
					bool runNds_boostVram = false;
					if (dsiFeatures() && !dsModeDSiWare) {
						runNds_boostCpu = perGameSettings_boostCpu == -1 ? ms().boostCpu : perGameSettings_boostCpu;
						runNds_boostVram = perGameSettings_boostVram == -1 ? ms().boostVram : perGameSettings_boostVram;
					}
					//iprintf ("Running %s with %d parameters\n", argarray[0], argarray.size());
					int err = runNdsFile (argarray[0], argarray.size(), (const char **)&argarray[0], true, true, dsModeSwitch, runNds_boostCpu, runNds_boostVram, language);
					char text[64];
					snprintf (text, sizeof(text), STR_START_FAILED_ERROR.c_str(), err);
					clearText();
					whiteScreen = true;
					fadeSpeed = true;
					controlTopBright = false;
					fadeType = true; // Fade in
					printSmall(false, 4, 4, text);
					stop();
				}
			} else {
				bool useNDSB = false;
				bool dsModeSwitch = false;
				ms().boostCpu = true;
				ms().boostVram = false;

				std::string romfolderNoSlash = romfolder[ms().secondaryDevice];
				RemoveTrailingSlashes(romfolderNoSlash);
				char ROMpath[256];
				snprintf (ROMpath, sizeof(ROMpath), "%s/%s", romfolderNoSlash.c_str(), filename[ms().secondaryDevice].c_str());
				ms().romPath[ms().secondaryDevice] = ROMpath;
				ms().previousUsedDevice = ms().secondaryDevice;
				ms().homebrewBootstrap = true;

				const char *ndsToBoot = "sd:/_nds/nds-bootstrap-release.nds";
				if (extention(filename[ms().secondaryDevice], ".plg")) {
					ndsToBoot = "fat:/_nds/TWiLightMenu/bootplg.srldr";
					dsModeSwitch = true;

					// Print .plg path without "fat:" at the beginning
					char ROMpathDS2[256];
					if (ms().secondaryDevice) {
						for (int i = 0; i < 252; i++) {
							ROMpathDS2[i] = ROMpath[4+i];
							if (ROMpath[4+i] == '\x00') break;
						}
					} else {
						sprintf(ROMpathDS2, "/_nds/TWiLightMenu/tempPlugin.plg");
						fcopy(ROMpath, "fat:/_nds/TWiLightMenu/tempPlugin.plg");
					}

					CIniFile dstwobootini( "fat:/_dstwo/twlm.ini" );
					dstwobootini.SetString("boot_settings", "file", ROMpathDS2);
					dstwobootini.SaveIniFile( "fat:/_dstwo/twlm.ini" );
				} else if (extention(filename[ms().secondaryDevice], ".rvid")) {
					ms().launchType[ms().secondaryDevice] = 7;

					ndsToBoot = "sd:/_nds/TWiLightMenu/apps/RocketVideoPlayer.nds";
					if(!isDSiMode() || access(ndsToBoot, F_OK) != 0) {
						ndsToBoot = "fat:/_nds/TWiLightMenu/apps/RocketVideoPlayer.nds";
						ms().boostVram = true;
					}
				} else if (extention(filename[ms().secondaryDevice], ".fv")) {
					ms().launchType[ms().secondaryDevice] = 8;

					ndsToBoot = "sd:/_nds/TWiLightMenu/apps/FastVideoDS.nds";
					if(!isDSiMode() || access(ndsToBoot, F_OK) != 0) {
						ndsToBoot = "fat:/_nds/TWiLightMenu/apps/FastVideoDS.nds";
						ms().boostVram = true;
					}
				} else if (extention(filename[ms().secondaryDevice], ".agb")
						|| extention(filename[ms().secondaryDevice], ".gba")
						|| extention(filename[ms().secondaryDevice], ".mb")) {
					ms().launchType[ms().secondaryDevice] = (ms().showGba == 1) ? 11 : 1;

					if (ms().showGba == 1) {
						while (!screenFadedOut()) {
							swiWaitForVBlank();
						}
						clearText();
						if (*(u16*)(0x020000C0) == 0x5A45) {
							printSmall(false, 0, 88, STR_PLEASE_WAIT, Alignment::center);
						}
						fadeType = true; // Fade in

						showProgressBar = true;
						progressBarLength = 0;

						u32 ptr = 0x08000000;
						u32 romSize = getFileSize(filename[ms().secondaryDevice].c_str());
						char titleID[4];
						FILE* gbaFile = fopen(filename[ms().secondaryDevice].c_str(), "rb");
						fseek(gbaFile, 0xAC, SEEK_SET);
						fread(&titleID, 1, 4, gbaFile);
						if (strncmp(titleID, "AGBJ", 4) == 0 && romSize <= 0x40000) {
							ptr += 0x400;
						}
						u32 curPtr = ptr;
						fseek(gbaFile, 0, SEEK_SET);

						extern char copyBuf[0x8000];
						if (romSize > 0x2000000) romSize = 0x2000000;

						bool nor = false;
						if (*(u16*)(0x020000C0) == 0x5A45 && strncmp(titleID, "AGBJ", 4) != 0) {
							cExpansion::SetRompage(0);
							expansion().SetRampage(cExpansion::ENorPage);
							cExpansion::OpenNorWrite();
							cExpansion::SetSerialMode();
							for(u32 address=0;address<romSize&&address<0x2000000;address+=0x40000)
							{
								expansion().Block_Erase(address);
								progressBarLength = (address+0x40000)/(romSize/192);
								if (progressBarLength > 192) progressBarLength = 192;
							}
							nor = true;
						} else if (*(u16*)(0x020000C0) == 0x4353 && romSize > 0x1FFFFFE) {
							romSize = 0x1FFFFFE;
						}

						clearText();
						printSmall(false, 0, 20, STR_BARSTOPPED_CLOSELID, Alignment::center);
						printSmall(false, 0, 88, STR_NOW_LOADING, Alignment::center);

						for (u32 len = romSize; len > 0; len -= 0x8000) {
							if (fread(&copyBuf, 1, (len>0x8000 ? 0x8000 : len), gbaFile) > 0) {
								s2RamAccess(true);
								if (nor) {
									expansion().WriteNorFlash(curPtr-ptr, (u8*)copyBuf, (len>0x8000 ? 0x8000 : len));
								} else {
									tonccpy((u16*)curPtr, &copyBuf, (len>0x8000 ? 0x8000 : len));
								}
								s2RamAccess(false);
								curPtr += 0x8000;
								progressBarLength = ((curPtr-ptr)+0x8000)/(romSize/192);
								if (progressBarLength > 192) progressBarLength = 192;
							} else {
								break;
							}
						}
						fclose(gbaFile);

						ptr = 0x0A000000;

						std::string savename = replaceAll(filename[ms().secondaryDevice], ".gba", ".sav");
						u32 savesize = getFileSize(savename.c_str());
						if (savesize > 0x10000) savesize = 0x10000;

						if (savesize > 0) {
							FILE* savFile = fopen(savename.c_str(), "rb");
							for (u32 len = savesize; len > 0; len -= 0x8000) {
								if (fread(&copyBuf, 1, (len>0x8000 ? 0x8000 : len), savFile) > 0) {
									gbaSramAccess(true);	// Switch to GBA SRAM
									cExpansion::WriteSram(ptr,(u8*)copyBuf,0x8000);
									gbaSramAccess(false);	// Switch out of GBA SRAM
									ptr += 0x8000;
								} else {
									break;
								}
							}
							fclose(savFile);
						}

						ndsToBoot = "fat:/_nds/TWiLightMenu/gbapatcher.srldr";
					} else if (ms().secondaryDevice) {
						ms().launchType[ms().secondaryDevice] = 17;

						ndsToBoot = ms().gbar2DldiAccess ? "sd:/_nds/GBARunner2_arm7dldi_ds.nds" : "sd:/_nds/GBARunner2_arm9dldi_ds.nds";
						if (dsiFeatures()) {
							ndsToBoot = ms().consoleModel>0 ? "sd:/_nds/GBARunner2_arm7dldi_3ds.nds" : "sd:/_nds/GBARunner2_arm7dldi_dsi.nds";
						}
						if(!isDSiMode() || access(ndsToBoot, F_OK) != 0) {
							ndsToBoot = ms().gbar2DldiAccess ? "fat:/_nds/GBARunner2_arm7dldi_ds.nds" : "fat:/_nds/GBARunner2_arm9dldi_ds.nds";
							if (dsiFeatures()) {
								ndsToBoot = ms().consoleModel>0 ? "fat:/_nds/GBARunner2_arm7dldi_3ds.nds" : "fat:/_nds/GBARunner2_arm7dldi_dsi.nds";
							}
						}
						ms().boostVram = false;
					} else {
						useNDSB = true;

						const char* gbar2Path = ms().consoleModel>0 ? "sd:/_nds/GBARunner2_arm7dldi_3ds.nds" : "sd:/_nds/GBARunner2_arm7dldi_dsi.nds";
						if (isDSiMode() && arm7SCFGLocked && !dsiWramAccess) {
							gbar2Path = ms().consoleModel>0 ? "sd:/_nds/GBARunner2_arm7dldi_nodsp_3ds.nds" : "sd:/_nds/GBARunner2_arm7dldi_nodsp_dsi.nds";
						}

						ndsToBoot = (ms().bootstrapFile ? "sd:/_nds/nds-bootstrap-hb-nightly.nds" : "sd:/_nds/nds-bootstrap-hb-release.nds");
						CIniFile bootstrapini("sd:/_nds/nds-bootstrap.ini");

						bootstrapini.SetInt("NDS-BOOTSTRAP", "LANGUAGE", ms().gameLanguage);
						bootstrapini.SetInt("NDS-BOOTSTRAP", "DSI_MODE", 0);
						bootstrapini.SetString("NDS-BOOTSTRAP", "NDS_PATH", gbar2Path);
						bootstrapini.SetString("NDS-BOOTSTRAP", "HOMEBREW_ARG", ROMpath);
						bootstrapini.SetString("NDS-BOOTSTRAP", "RAM_DRIVE_PATH", "");
						bootstrapini.SetInt("NDS-BOOTSTRAP", "BOOST_CPU", 1);
						bootstrapini.SetInt("NDS-BOOTSTRAP", "BOOST_VRAM", 0);

						bootstrapini.SaveIniFile("sd:/_nds/nds-bootstrap.ini");
					}
				} else if (extention(filename[ms().secondaryDevice], ".xex")
						 || extention(filename[ms().secondaryDevice], ".atr")) {
					ms().launchType[ms().secondaryDevice] = 15;

					ndsToBoot = "sd:/_nds/TWiLightMenu/emulators/XEGS-DS.nds";
					if(!isDSiMode() || access(ndsToBoot, F_OK) != 0) {
						ndsToBoot = "fat:/_nds/TWiLightMenu/emulators/XEGS-DS.nds";
						ms().boostVram = true;
					}
				} else if (extention(filename[ms().secondaryDevice], ".a26")) {
					ms().launchType[ms().secondaryDevice] = 9;

					ndsToBoot = "sd:/_nds/TWiLightMenu/emulators/StellaDS.nds";
					if(!isDSiMode() || access(ndsToBoot, F_OK) != 0) {
						ndsToBoot = "fat:/_nds/TWiLightMenu/emulators/StellaDS.nds";
						ms().boostVram = true;
					}
				} else if (extention(filename[ms().secondaryDevice], ".a52")) {
					ms().launchType[ms().secondaryDevice] = 13;

					ndsToBoot = "sd:/_nds/TWiLightMenu/emulators/A5200DS.nds";
					if(!isDSiMode() || access(ndsToBoot, F_OK) != 0) {
						ndsToBoot = "fat:/_nds/TWiLightMenu/emulators/A5200DS.nds";
						ms().boostVram = true;
					}
				} else if (extention(filename[ms().secondaryDevice], ".a78")) {
					ms().launchType[ms().secondaryDevice] = 12;

					ndsToBoot = "sd:/_nds/TWiLightMenu/emulators/A7800DS.nds";
					if(!dsiFeatures()) {
						ndsToBoot = "fat:/_nds/TWiLightMenu/emulators/A7800DS-LITE.nds";
					}
					if((!isDSiMode() && dsiFeatures()) || access(ndsToBoot, F_OK) != 0) {
						ndsToBoot = "fat:/_nds/TWiLightMenu/emulators/A7800DS.nds";
						ms().boostVram = true;
					}
				} else if (extention(filename[ms().secondaryDevice], ".int")) {
					ms().launchType[ms().secondaryDevice] = 16;

					ndsToBoot = "sd:/_nds/TWiLightMenu/emulators/NINTV-DS.nds";
					if(!isDSiMode() || access(ndsToBoot, F_OK) != 0) {
						ndsToBoot = "fat:/_nds/TWiLightMenu/emulators/NINTV-DS.nds";
						ms().boostVram = true;
					}
				} else if (extention(filename[ms().secondaryDevice], ".gb") || extention(filename[ms().secondaryDevice], ".sgb") || extention(filename[ms().secondaryDevice], ".gbc")) {
					ms().launchType[ms().secondaryDevice] = 5;

					ndsToBoot = "sd:/_nds/TWiLightMenu/emulators/gameyob.nds";
					if(!isDSiMode() || access(ndsToBoot, F_OK) != 0) {
						ndsToBoot = "fat:/_nds/TWiLightMenu/emulators/gameyob.nds";
						dsModeSwitch = !isDSiMode();
						ms().boostVram = true;
					}
				} else if (extention(filename[ms().secondaryDevice], ".nes") || extention(filename[ms().secondaryDevice], ".fds")) {
					ms().launchType[ms().secondaryDevice] = 4;

					ndsToBoot = (ms().secondaryDevice ? "sd:/_nds/TWiLightMenu/emulators/nesds.nds" : "sd:/_nds/TWiLightMenu/emulators/nestwl.nds");
					if(!isDSiMode() || access(ndsToBoot, F_OK) != 0) {
						ndsToBoot = "fat:/_nds/TWiLightMenu/emulators/nesds.nds";
						ms().boostVram = true;
					}
				} else if (extention(filename[ms().secondaryDevice], ".sms") || extention(filename[ms().secondaryDevice], ".gg")) {
					mkdir(ms().secondaryDevice ? "fat:/data" : "sd:/data", 0777);
					mkdir(ms().secondaryDevice ? "fat:/data/s8ds" : "sd:/data/s8ds", 0777);

					if (!ms().secondaryDevice && !arm7SCFGLocked && ms().smsGgInRam) {
						ms().launchType[ms().secondaryDevice] = 1;

						useNDSB = true;

						ndsToBoot = (ms().bootstrapFile ? "sd:/_nds/nds-bootstrap-hb-nightly.nds" : "sd:/_nds/nds-bootstrap-hb-release.nds");
						CIniFile bootstrapini("sd:/_nds/nds-bootstrap.ini");

						bootstrapini.SetString("NDS-BOOTSTRAP", "GUI_LANGUAGE", ms().getGuiLanguageString());
						bootstrapini.SetInt("NDS-BOOTSTRAP", "LANGUAGE", ms().gameLanguage);
						bootstrapini.SetInt("NDS-BOOTSTRAP", "DSI_MODE", 0);
						bootstrapini.SetString("NDS-BOOTSTRAP", "NDS_PATH", "sd:/_nds/TWiLightMenu/emulators/S8DS07.nds");
						bootstrapini.SetString("NDS-BOOTSTRAP", "HOMEBREW_ARG", "");
						bootstrapini.SetInt("NDS-BOOTSTRAP", "BOOST_CPU", 1);
						bootstrapini.SetInt("NDS-BOOTSTRAP", "BOOST_VRAM", 0);

						bootstrapini.SetString("NDS-BOOTSTRAP", "RAM_DRIVE_PATH", ROMpath);
						bootstrapini.SaveIniFile("sd:/_nds/nds-bootstrap.ini");
					} else {
						ms().launchType[ms().secondaryDevice] = 6;

						ndsToBoot = "sd:/_nds/TWiLightMenu/emulators/S8DS.nds";
						if(!isDSiMode() || access(ndsToBoot, F_OK) != 0) {
							ndsToBoot = "fat:/_nds/TWiLightMenu/emulators/S8DS.nds";
							ms().boostVram = true;
						}
					}
				} else if (extention(filename[ms().secondaryDevice], ".gen")) {
					bool usePicoDrive = ((isDSiMode() && sdFound() && arm7SCFGLocked)
						|| ms().showMd==2 || (ms().showMd==3 && getFileSize(filename[ms().secondaryDevice].c_str()) > 0x300000));
					ms().launchType[ms().secondaryDevice] = (usePicoDrive ? 10 : 1);

					if (usePicoDrive || ms().secondaryDevice) {
						ndsToBoot = usePicoDrive ? "sd:/_nds/TWiLightMenu/emulators/PicoDriveTWL.nds" : "sd:/_nds/TWiLightMenu/emulators/jEnesisDS.nds";
						if(!isDSiMode() || access(ndsToBoot, F_OK) != 0) {
							ndsToBoot = usePicoDrive ? "fat:/_nds/TWiLightMenu/emulators/PicoDriveTWL.nds" : "fat:/_nds/TWiLightMenu/emulators/jEnesisDS.nds";
							ms().boostVram = true;
						}
						dsModeSwitch = !usePicoDrive;
					} else {
						useNDSB = true;

						ndsToBoot = (ms().bootstrapFile ? "sd:/_nds/nds-bootstrap-hb-nightly.nds" : "sd:/_nds/nds-bootstrap-hb-release.nds");
						CIniFile bootstrapini("sd:/_nds/nds-bootstrap.ini");

						bootstrapini.SetString("NDS-BOOTSTRAP", "GUI_LANGUAGE", ms().getGuiLanguageString());
						bootstrapini.SetInt("NDS-BOOTSTRAP", "LANGUAGE", ms().gameLanguage);
						bootstrapini.SetInt("NDS-BOOTSTRAP", "DSI_MODE", 0);
						bootstrapini.SetString("NDS-BOOTSTRAP", "NDS_PATH", "sd:/_nds/TWiLightMenu/emulators/jEnesisDS.nds");
						bootstrapini.SetString("NDS-BOOTSTRAP", "HOMEBREW_ARG", "fat:/ROM.BIN");
						bootstrapini.SetInt("NDS-BOOTSTRAP", "BOOST_CPU", 1);
						bootstrapini.SetInt("NDS-BOOTSTRAP", "BOOST_VRAM", 0);

						bootstrapini.SetString("NDS-BOOTSTRAP", "RAM_DRIVE_PATH", ROMpath);
						bootstrapini.SaveIniFile("sd:/_nds/nds-bootstrap.ini");
					}
				} else if (extention(filename[ms().secondaryDevice], ".smc") || extention(filename[ms().secondaryDevice], ".sfc")) {
					ms().launchType[ms().secondaryDevice] = 1;

					if (ms().secondaryDevice) {
						ndsToBoot = "sd:/_nds/TWiLightMenu/emulators/SNEmulDS.nds";
						if(!isDSiMode() || access(ndsToBoot, F_OK) != 0) {
							ndsToBoot = "fat:/_nds/TWiLightMenu/emulators/SNEmulDS.nds";
							ms().boostCpu = false;
							ms().boostVram = true;
						}
						dsModeSwitch = true;
					} else {
						useNDSB = true;

						ndsToBoot = (ms().bootstrapFile ? "sd:/_nds/nds-bootstrap-hb-nightly.nds" : "sd:/_nds/nds-bootstrap-hb-release.nds");
						CIniFile bootstrapini("sd:/_nds/nds-bootstrap.ini");

						bootstrapini.SetString("NDS-BOOTSTRAP", "GUI_LANGUAGE", ms().getGuiLanguageString());
						bootstrapini.SetInt("NDS-BOOTSTRAP", "LANGUAGE", ms().gameLanguage);
						bootstrapini.SetInt("NDS-BOOTSTRAP", "DSI_MODE", 0);
						bootstrapini.SetString("NDS-BOOTSTRAP", "NDS_PATH", "sd:/_nds/TWiLightMenu/emulators/SNEmulDS.nds");
						bootstrapini.SetString("NDS-BOOTSTRAP", "HOMEBREW_ARG", "fat:/snes/ROM.SMC");
						bootstrapini.SetInt("NDS-BOOTSTRAP", "BOOST_CPU", 0);
						bootstrapini.SetInt("NDS-BOOTSTRAP", "BOOST_VRAM", 0);

						bootstrapini.SetString("NDS-BOOTSTRAP", "RAM_DRIVE_PATH", ROMpath);
						bootstrapini.SaveIniFile("sd:/_nds/nds-bootstrap.ini");
					}
				} else if (extention(filename[ms().secondaryDevice], ".pce")) {
					mkdir(ms().secondaryDevice ? "fat:/data" : "sd:/data", 0777);
					mkdir(ms().secondaryDevice ? "fat:/data/NitroGrafx" : "sd:/data/NitroGrafx", 0777);

					ms().launchType[ms().secondaryDevice] = 14;

					ndsToBoot = "sd:/_nds/TWiLightMenu/emulators/NitroGrafx.nds";
					if(!isDSiMode() || access(ndsToBoot, F_OK) != 0) {
						ndsToBoot = "fat/_nds/TWiLightMenu/emulators/NitroGrafx.nds";
						ms().boostVram = true;
					}
				}

				ms().saveSettings();

				while (!screenFadedOut()) {
					swiWaitForVBlank();
				}

				if (!isDSiMode() && !ms().secondaryDevice && !extention(filename[ms().secondaryDevice], ".plg")) {
					ntrStartSdGame();
				}
				argarray.push_back(ROMpath);
				argarray.at(0) = (char *)ndsToBoot;

				int err = runNdsFile (argarray[0], argarray.size(), (const char **)&argarray[0], !useNDSB, true, dsModeSwitch, ms().boostCpu, ms().boostVram, -1);	// Pass ROM to emulator as argument

				char text[64];
				snprintf (text, sizeof(text), STR_START_FAILED_ERROR.c_str(), err);
				clearText();
				printSmall(false, 4, 4, text);
				if (err == 1 && useNDSB) {
					printSmall(false, 4, 24, ms().bootstrapFile ? STR_BOOTSTRAP_HB_NIGHTLY_NOT_FOUND : STR_BOOTSTRAP_HB_RELEASE_NOT_FOUND);
				}
				whiteScreen = true;
				fadeSpeed = true;
				controlTopBright = false;
				fadeType = true; // Fade in
				stop();

				while(argarray.size() !=0 ) {
					free(argarray.at(0));
					argarray.erase(argarray.begin());
				}
			}
		}
	}

	return 0;
}
