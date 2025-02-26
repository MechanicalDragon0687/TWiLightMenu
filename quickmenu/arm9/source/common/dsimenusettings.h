
#include <nds.h>
#include <string>
#include "common/singleton.h"

#pragma once
#ifndef _DSIMENUPPSETTINGS_H_
#define _DSIMENUPPSETTINGS_H_

/**
 * Multi use class for DSiMenuPlusPlus INI file.
 *
 * Try not to change settings that are not related to the current theme.
 */
class TWLSettings
{
  public:
    enum TScrollSpeed
    {
        EScrollFast = 4,
        EScrollMedium = 10,
        EScrollSlow = 16
    };

    enum TViewMode
    {
        EViewList = 0,
        EViewIcon = 1,
        EViewInternal = 2
    };

    /*
        0: DSTT/R4i Gold/R4i-SDHC/R4 SDHC Dual-Core/R4 SDHC Upgrade/SC DSONE
        1: R4DS (Original Non-SDHC version)/ M3 Simply
        2: R4iDSN/R4i Gold RTS/R4 Ultra
        3: Acekard 2(i)/Galaxy Eagle/M3DS Real
        4: Acekard RPG
        5: Ace 3DS+/Gateway Blue Card/R4iTT
        6: SuperCard DSTWO
    */

    enum TFlashCard
    {
        EDSTTClone = 0,
        ER4Original = 1,
        ER4iGoldClone = 2,
        EAcekard2i = 3,
        EAcekardRPG = 4,
        EGatewayBlue = 5,
        ESupercardDSTWO = 6
    };

    // Do not reorder these, just add to the end
    enum TLanguage
    {
        ELangDefault = -1,
        ELangJapanese = 0,
        ELangEnglish = 1,
        ELangFrench = 2,
        ELangGerman = 3,
        ELangItalian = 4,
        ELangSpanish = 5,
        ELangChineseS = 6,
        ELangKorean = 7,
        ELangChineseT = 8,
        ELangPolish = 9,
        ELangPortuguese = 10,
        ELangRussian = 11,
        ELangSwedish = 12,
        ELangDanish = 13,
        ELangTurkish = 14,
        ELangUkrainian = 15,
        ELangHungarian = 16,
        ELangNorwegian = 17,
        ELangHebrew = 18,
        ELangDutch = 19,
        ELangIndonesian = 20,
        ELangGreek = 21,
        ELangBulgarian = 22,
        ELangRomanian = 23,
        ELangArabic = 24,
        ELangPortugueseBrazil = 25,
    };

    enum TRegion
    {
        ERegionGame = -2,
        ERegionDefault = -1,
        ERegionJapan = 0,
        ERegionUSA = 1,
        ERegionEurope = 2,
        ERegionAustralia = 3,
        ERegionChina = 4,
        ERegionKorea = 5,
    };

    enum TRunIn
    {
        EDSMode = 0,
        EDSiMode = 1,
        EDSiModeForced = 2
    };

    enum TSlot1LaunchMethod
    {
        EReboot = 0,
        EDirect = 1
    };

    enum TBootstrapFile
    {
        EReleaseBootstrap = false,
        ENightlyBootstrap = true
    };

    // 0 = No launch, 1 = SD/Flash card, 2 = SD/Flash card (Direct boot), 3 = DSiWare, 4 = NES, 5 = (S)GB(C), 6 = SMS/GG
    enum TLaunchType
    {
        ENoLaunch = 0,
        ESDFlashcardLaunch = 1,
        ESDFlashcardDirectLaunch = 2,
        EDSiWareLaunch = 3,
        ENESDSLaunch = 4,
        EGameYobLaunch = 5,
        ES8DSLaunch = 6,
        ERVideoLaunch = 7,
        EMPEG4Launch = 8,
        EStellaDSLaunch = 9,
        EPicoDriveTWLLaunch = 10,
        EGBANativeLaunch = 11,
        EA7800DSLaunch = 12,
        EA5200DSLaunch = 13,
        ENitroGrafxLaunch = 14,
        EXEGSDSLaunch = 15,
        ENINTVDSLaunch = 16,
        EGBARunner2Launch = 17
    };

    /*
        0 = Nintendo DSi (Retail)
        1 = Nintendo DSi (Dev/Panda)
        2 = Nintendo 3DS
        3 = New Nintendo 3DS
    */
    enum TConsoleModel
    {
        EDSiRetail = 0,
        EDSiDebug = 1,
        E3DSOriginal = 2,
        E3DSNew = 3
    };

    enum TSoundFreq
    {
        EFreq32KHz = 0,
        EFreq47KHz = 1
    };

  public:
    TWLSettings();
    ~TWLSettings();

  public:
    void loadSettings();
    void saveSettings();

    TLanguage getGuiLanguage();
    TLanguage getTitleLanguage();
    TLanguage getGameLanguage();

    std::string getGuiLanguageString();

    // Get if the current language is right to left
    bool rtl();

    const char* getAppName();
  public:
    std::string romfolder[2];
    int pagenum[2];
    int cursorPosition[2];
    int startMenu_cursorPosition;
    int consoleModel;
    bool gotosettings;

    int guiLanguage;
    int titleLanguage;
    int fps;
    bool macroMode;
    int colorMode;
    int blfLevel;
    int dsiWareExploit;
    bool sdRemoveDetect;
    bool showMicroSd;
    bool gbar2DldiAccess;
    bool showSelectMenu;
    int theme;
    int dsiMusic;
    bool boxArtColorDeband;
    bool showNds;
    int showGba;
    bool showRvid;
    bool showXex;
    bool showA26;
    bool showA52;
    bool showA78;
    bool showNes;
    bool showGb;
    bool showSmsGg;
    int showMd;
    bool showSnes;
    bool showPce;
    bool showDirectories;
    bool showHidden;
    bool preventDeletion;
    int showBoxArt;
    bool animateDsiIcons;
    int sysRegion;
    int launcherApp;
    bool secondaryAccess;
    bool previousUsedDevice;
    bool secondaryDevice;
    bool fcSaveOnSd;
    bool updateRecentlyPlayedList;
    int sortMethod;

    int flashcard;
    int slot1LaunchMethod;
    bool useBootstrap;
    bool bootstrapFile;

    int gameLanguage;
    int gameRegion;
    bool boostCpu;
    bool boostVram;
    int bstrap_dsiMode;
    bool cardReadDMA;
    bool asyncCardRead;
    int extendedMemory;

    bool forceSleepPatch;
    bool slot1SCFGUnlock;
    bool dsiWareBooter;
	bool dsiWareToSD;
    bool autorun;
    bool show12hrClock;
    //bool snesEmulator;
    bool smsGgInRam;

    int ak_viewMode;
    int ak_scrollSpeed;
    bool ak_zoomIcons;
    std::string ak_theme;

    std::string dsiWareSrlPath;
    std::string dsiWarePubPath;
    std::string dsiWarePrvPath;

    std::string romPath[2];

    bool slot1Launched;
    int launchType[2];
    std::string homebrewArg;
    bool homebrewBootstrap;
    bool homebrewHasWide;
    bool soundfreq;
    bool showlogo;

    std::string r4_theme;
    std::string dsi_theme;
    std::string _3ds_theme;
    std::string gbaBorder;
    std::string unlaunchBg;
    std::string font;
    bool dsClassicCustomFont;

    bool wideScreen;

    bool dontShowClusterWarning;
};

typedef singleton<TWLSettings> menuSettings_s;
inline TWLSettings &ms() { return menuSettings_s::instance(); }

#endif //_DSIMENUPPSETTINGS_H_
