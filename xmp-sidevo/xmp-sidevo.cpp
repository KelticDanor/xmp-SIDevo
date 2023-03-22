// XMPlay SIDevo input plugin (c) 2023 Nathan Hindley
#define _CRT_SECURE_NO_WARNINGS

#include "xmpin.h"

static XMPFUNC_IN* xmpfin;
static XMPFUNC_MISC* xmpfmisc;
static XMPFUNC_FILE* xmpffile;
static XMPFUNC_TEXT* xmpftext;
static XMPFUNC_REGISTRY* xmpfreg;

#include "xmp-sidevo.h"
#include "utils/SidDatabase.h"
#include "utils/STILview/stil.h"
#include <builders/residfp-builder/residfp.h>
#include <sidplayfp/SidInfo.h>
#include <sidplayfp/SidTune.h>
#include <sidplayfp/SidTuneInfo.h>
#include <sidplayfp/sidplayfp.h>
#include <sidid.h>

#include <commctrl.h>
#include <assert.h>
#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <iterator>
#include <functional>
#include <cctype>
#include <shlobj.h>

#ifndef _WIN32
#include <libgen.h>
#else
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

static HINSTANCE ghInstance;

// fancy data containers
typedef struct
{
	sidplayfp* m_engine;
	ReSIDfpBuilder* m_builder;
	SidTune* p_song;
	SidConfig m_config;
	SidDatabase d_songdbase;
	STIL d_stilbase;
	SidId d_sididbase;
	bool d_loadeddbase;
	bool d_loadedstil;
	bool d_loadedsidid;
	char p_sididplayer[25];
	const SidTuneInfo* p_songinfo;
	int p_songcount;
	int p_subsong;
	int p_defsubsong;
	int p_songlength;
	int* p_subsonglength;
	char p_sidmodel[10];
	char p_clockspeed[10];
	bool b_loaded;
	bool b_reloadcfg = false;

	int o_sidchips;
	char o_sidmodel[10];
	char o_clockspeed[10];
	const char* o_filename;

	float fadein;
	float fadeout;
	int fadeouttrigger;
	bool skiptrigger;
} SIDengine;
static SIDengine sidEngine;

typedef struct
{
	char c_sidmodel[10];
	char c_clockspeed[10];
	int c_powerdelay;
	int c_defaultlength;
	int c_minlength;
	int c_6581filter;
	int c_8580filter;
	int c_fadeinms;
	int c_fadeoutms;
	char c_samplemethod[10];
	char c_dbpath[250];
	bool c_locksidmodel;
	bool c_lockclockspeed;
	bool c_enablefilter;
	bool c_enabledigiboost;
	bool c_powerdelayrandom;
	bool c_forcelength;
	bool c_skipshort;
	bool c_fadein;
	bool c_fadeout;
	bool c_disableseek;
	bool c_detectplayer;
	bool c_defaultskip;
	bool c_defaultonly;
} SIDsetting;
static SIDsetting sidSetting;

// pretty up the format
static const char* simpleFormat(const char* songFormat) {
	if (std::string(songFormat).find("PSID") != std::string::npos) {
		return "PSID";
	} else if (std::string(songFormat).find("RSID") != std::string::npos) {
		return "RSID";
	} else if (std::string(songFormat).find("STR") != std::string::npos) {
		return "STR";
	} else if (std::string(songFormat).find("MUS") != std::string::npos) {
		return "MUS";
	} else {
		return songFormat;
	}
}
// pretty up the length
static const char* simpleLength(int songLength, char* buf) {
	int rsSecond = songLength;
	int rsMinute = songLength / 60;
	int rsHour = rsMinute / 60;

	if (rsHour > 0) {
		sprintf(buf, "%u:%02u:%02u", rsHour, rsMinute % 60, rsSecond % 60);
	} else {
		sprintf(buf, "%02u:%02u", rsMinute, rsSecond % 60);
	}
	return buf;
}
// config load/save/apply functions
static void loadConfig()
{
	if (!sidEngine.b_loaded) {
		strncpy(sidSetting.c_sidmodel, "6581", 10);
		strncpy(sidSetting.c_clockspeed, "PAL", 10);
		strncpy(sidSetting.c_samplemethod, "Normal", 10);
		strncpy(sidSetting.c_dbpath, "", 250);
		sidSetting.c_defaultlength = 120;
		sidSetting.c_minlength = 3;
		sidSetting.c_6581filter = 25;
		sidSetting.c_8580filter = 50;
		sidSetting.c_powerdelay = 0;
		sidSetting.c_fadeinms = 80;
		sidSetting.c_fadeoutms = 500;
		sidSetting.c_lockclockspeed = FALSE;
		sidSetting.c_locksidmodel = FALSE;
		sidSetting.c_enabledigiboost = FALSE;
		sidSetting.c_enablefilter = TRUE;
		sidSetting.c_powerdelayrandom = TRUE;
		sidSetting.c_forcelength = FALSE;
		sidSetting.c_skipshort = FALSE;
		sidSetting.c_fadein = TRUE;
		sidSetting.c_fadeout = FALSE;
		sidSetting.c_disableseek = FALSE;
		sidSetting.c_detectplayer = TRUE;
		sidSetting.c_defaultskip = FALSE;
		sidSetting.c_defaultonly = FALSE;

		if (xmpfreg->GetString("SIDevo", "c_sidmodel", sidSetting.c_sidmodel, 10) != 0) {
			xmpfreg->GetString("SIDevo", "c_clockspeed", sidSetting.c_clockspeed, 10);
			xmpfreg->GetString("SIDevo", "c_samplemethod", sidSetting.c_samplemethod, 10);
			xmpfreg->GetString("SIDevo", "c_dbpath", sidSetting.c_dbpath, 250);
			xmpfreg->GetInt("SIDevo", "c_defaultlength", &sidSetting.c_defaultlength);
			xmpfreg->GetInt("SIDevo", "c_minlength", &sidSetting.c_minlength);
			xmpfreg->GetInt("SIDevo", "c_6581filter", &sidSetting.c_6581filter);
			xmpfreg->GetInt("SIDevo", "c_8580filter", &sidSetting.c_8580filter);
			xmpfreg->GetInt("SIDevo", "c_powerdelay", &sidSetting.c_powerdelay);
			xmpfreg->GetInt("SIDevo", "c_fadeinms", &sidSetting.c_fadeinms);
			xmpfreg->GetInt("SIDevo", "c_fadeoutms", &sidSetting.c_fadeoutms);

			int ival;
			if (xmpfreg->GetInt("SIDevo", "c_lockclockspeed", &ival))
				sidSetting.c_lockclockspeed = ival;
			if (xmpfreg->GetInt("SIDevo", "c_locksidmodel", &ival))
				sidSetting.c_locksidmodel = ival;
			if (xmpfreg->GetInt("SIDevo", "c_enabledigiboost", &ival))
				sidSetting.c_enabledigiboost = ival;
			if (xmpfreg->GetInt("SIDevo", "c_enablefilter", &ival))
				sidSetting.c_enablefilter = ival;
			if (xmpfreg->GetInt("SIDevo", "c_powerdelayrandom", &ival))
				sidSetting.c_powerdelayrandom = ival;
			if (xmpfreg->GetInt("SIDevo", "c_forcelength", &ival))
				sidSetting.c_forcelength = ival;
			if (xmpfreg->GetInt("SIDevo", "c_skipshort", &ival))
				sidSetting.c_skipshort = ival;
			if (xmpfreg->GetInt("SIDevo", "c_fadein", &ival))
				sidSetting.c_fadein = ival;
			if (xmpfreg->GetInt("SIDevo", "c_fadeout", &ival))
				sidSetting.c_fadeout = ival;
			if (xmpfreg->GetInt("SIDevo", "c_disableseek", &ival))
				sidSetting.c_disableseek = ival;
			if (xmpfreg->GetInt("SIDevo", "c_detectplayer", &ival))
				sidSetting.c_detectplayer = ival;
			if (xmpfreg->GetInt("SIDevo", "c_defaultskip", &ival))
				sidSetting.c_defaultskip = ival;
			if (xmpfreg->GetInt("SIDevo", "c_defaultonly", &ival))
				sidSetting.c_defaultonly = ival;
		}
	}
}
static bool applyConfig(bool initThis) {
	if (initThis) {
		sidEngine.m_config = sidEngine.m_engine->config();

		// apply sid model
		if (std::string(sidSetting.c_sidmodel).find("6581") != std::string::npos) {
			sidEngine.m_config.defaultSidModel = SidConfig::MOS6581;
		} else if (std::string(sidSetting.c_sidmodel).find("8580") != std::string::npos) {
			sidEngine.m_config.defaultSidModel = SidConfig::MOS8580;
		}

		// apply clock speed
		if (std::string(sidSetting.c_clockspeed).find("PAL") != std::string::npos) {
			sidEngine.m_config.defaultC64Model = SidConfig::PAL;
		} else if (std::string(sidSetting.c_clockspeed).find("NTSC") != std::string::npos) {
			sidEngine.m_config.defaultC64Model = SidConfig::NTSC;
		}

		// apply power delay
		if (sidSetting.c_powerdelayrandom) {
			sidEngine.m_config.powerOnDelay = SidConfig::DEFAULT_POWER_ON_DELAY;
		} else {
			sidEngine.m_config.powerOnDelay = sidSetting.c_powerdelay;
		}

		// apply sample method
		if (std::string(sidSetting.c_samplemethod).find("Normal") != std::string::npos) {
			sidEngine.m_config.samplingMethod = SidConfig::INTERPOLATE;
		} else if (std::string(sidSetting.c_samplemethod).find("Accurate") != std::string::npos) {
			sidEngine.m_config.samplingMethod = SidConfig::RESAMPLE_INTERPOLATE;
		}

		// apply sid model & clock speed lock
		sidEngine.m_config.forceSidModel = sidSetting.c_locksidmodel;
		sidEngine.m_config.forceC64Model = sidSetting.c_lockclockspeed;

		// apply digi boost
		sidEngine.m_config.digiBoost = sidSetting.c_enabledigiboost;
	} else {
		sidEngine.b_reloadcfg = true;
	}

	// apply filter status & levels
	sidEngine.m_builder->filter(sidSetting.c_enablefilter);
	float temp6581set = (float)sidSetting.c_6581filter / 100;
	sidEngine.m_builder->filter6581Curve(temp6581set);
	float temp8580set = (float)sidSetting.c_8580filter / 100;
	sidEngine.m_builder->filter8580Curve(temp8580set);

	// apply config
	sidEngine.m_config.sidEmulation = sidEngine.m_builder;
	if (sidEngine.m_engine->config(sidEngine.m_config)) {
		return TRUE;
	} else {
		return FALSE;
	}
}
static void saveConfig()
{
	xmpfreg->SetString("SIDevo", "c_sidmodel", sidSetting.c_sidmodel);
	xmpfreg->SetString("SIDevo", "c_samplemethod", sidSetting.c_samplemethod);
	xmpfreg->SetString("SIDevo", "c_dbpath", sidSetting.c_dbpath);
	xmpfreg->SetString("SIDevo", "c_clockspeed", sidSetting.c_clockspeed);
	xmpfreg->SetInt("SIDevo", "c_powerdelay", &sidSetting.c_powerdelay);
	xmpfreg->SetInt("SIDevo", "c_6581filter", &sidSetting.c_6581filter);
	xmpfreg->SetInt("SIDevo", "c_8580filter", &sidSetting.c_8580filter);
	xmpfreg->SetInt("SIDevo", "c_defaultlength", &sidSetting.c_defaultlength);
	xmpfreg->SetInt("SIDevo", "c_minlength", &sidSetting.c_minlength);
	xmpfreg->SetInt("SIDevo", "c_fadeinms", &sidSetting.c_fadeinms);
	xmpfreg->SetInt("SIDevo", "c_fadeoutms", &sidSetting.c_fadeoutms);

	int ival;
	ival = sidSetting.c_lockclockspeed;
	xmpfreg->SetInt("SIDevo", "c_lockclockspeed", &ival);
	ival = sidSetting.c_locksidmodel;
	xmpfreg->SetInt("SIDevo", "c_locksidmodel", &ival);
	ival = sidSetting.c_enablefilter;
	xmpfreg->SetInt("SIDevo", "c_enablefilter", &ival);
	ival = sidSetting.c_enabledigiboost;
	xmpfreg->SetInt("SIDevo", "c_enabledigiboost", &ival);
	ival = sidSetting.c_powerdelayrandom;
	xmpfreg->SetInt("SIDevo", "c_powerdelayrandom", &ival);
	ival = sidSetting.c_forcelength;
	xmpfreg->SetInt("SIDevo", "c_forcelength", &ival);
	ival = sidSetting.c_skipshort;
	xmpfreg->SetInt("SIDevo", "c_skipshort", &ival);
	ival = sidSetting.c_fadein;
	xmpfreg->SetInt("SIDevo", "c_fadein", &ival);
	ival = sidSetting.c_fadeout;
	xmpfreg->SetInt("SIDevo", "c_fadeout", &ival);
	ival = sidSetting.c_disableseek;
	xmpfreg->SetInt("SIDevo", "c_disableseek", &ival);
	ival = sidSetting.c_detectplayer;
	xmpfreg->SetInt("SIDevo", "c_detectplayer", &ival);
	ival = sidSetting.c_defaultskip;
	xmpfreg->SetInt("SIDevo", "c_defaultskip", &ival);
	ival = sidSetting.c_defaultonly;
	xmpfreg->SetInt("SIDevo", "c_defaultonly", &ival);

	if (sidEngine.b_loaded) {
		applyConfig(FALSE);
	}
}
// trim from both ends
static inline std::string& trim(std::string& s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(),
		std::not1(std::ptr_fun<int, int>(std::isspace))));
	s.erase(std::find_if(s.rbegin(), s.rend(),
		std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
	return s;
}
// test and folder functions for settings dialog
int CALLBACK callbackFolder(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData) {
	LPITEMIDLIST pidlNavigate;
	switch (uMsg) {
		case BFFM_INITIALIZED: {
			pidlNavigate = (LPITEMIDLIST)lpData;
			if (pidlNavigate != NULL)
				SendMessage(hwnd, BFFM_SETSELECTION, (WPARAM)FALSE, (LPARAM)pidlNavigate);
		}
		break;
	}
	return 0;
}
static std::string WINAPI selectFolder(HWND hWnd) {
	char folderPath[MAX_PATH + 1]{};
	BROWSEINFO bSet{};
	LPITEMIDLIST pidlStart, pidlSelected;
	pidlStart = ILCreateFromPathA(sidSetting.c_dbpath);

	bSet.hwndOwner = hWnd;
	bSet.pszDisplayName = folderPath;
	bSet.lpszTitle = "Choose C64Music/DOCUMENTS Directory";
	bSet.ulFlags = BIF_NEWDIALOGSTYLE | BIF_RETURNONLYFSDIRS;
	bSet.lpfn = callbackFolder;
	bSet.lParam = (LPARAM)pidlStart;
	pidlSelected = SHBrowseForFolder(&bSet);
	if (pidlSelected != NULL && SHGetPathFromIDList(pidlSelected, folderPath))
	{
		return folderPath;
	} else {
		return "";
	}
}
static std::string pathTest() {
	std::string relpathName, testpathName, pathState;

	// get dll path
	if ((sidSetting.c_dbpath[0]) == '.') {
		TCHAR exepathName[FILENAME_MAX];
		GetModuleFileName(nullptr, exepathName, FILENAME_MAX);
		std::string::size_type slashPos = std::string(exepathName).find_last_of("\\/");
		relpathName = std::string(exepathName).substr(0, slashPos);
		relpathName.append("/");
		relpathName.append(sidSetting.c_dbpath);
		relpathName.append("/");
	} else {
		relpathName = sidSetting.c_dbpath;
		relpathName.append("/");
	}

	// test songlengths
	testpathName = relpathName;
	testpathName.append("Songlengths.md5");
	if (FILE* file = fopen(testpathName.c_str(), "r")) {
		fclose(file);
		pathState.append("Songlengths");

		// test stil
		testpathName = relpathName;
		testpathName.append("STIL.txt");
		if (FILE* file = fopen(testpathName.c_str(), "r")) {
			fclose(file);
			pathState.append(" && STIL Found");
		} else {
			pathState.append(" Found, STIL Not Found");
		}
	} else {
		pathState.append("HVSC Path Invalid");
	}

	return pathState;
}

// functions to load and fetch the SIDId
static void loadSIDId() {
	if (sidSetting.c_detectplayer && !sidEngine.d_loadedsidid) {
		TCHAR pluginPath[FILENAME_MAX];
		std::string configPath;
		GetModuleFileName(ghInstance, pluginPath, FILENAME_MAX);
		std::string::size_type slashPos = std::string(pluginPath).find_last_of("\\/");
		configPath = std::string(pluginPath).substr(0, slashPos + 1);
		configPath.append("sidid.cfg");
		
		if (FILE* file = fopen(configPath.c_str(), "r")) {
			fclose(file);
			sidEngine.d_loadedsidid = sidEngine.d_sididbase.readConfigFile(configPath);
		} else {
			MessageBoxA(0, "Unable to find sidid.cfg in the plugin folder, disable detect music player if you would prefer not to use SIDid.", "sidid.cfg Not Found", MB_OK);
		}
	}
}
static void fetchSIDId(std::vector<uint8_t> c64buf) {
	if (sidEngine.d_loadedsidid) {
		// adapted sidid code, could be more efficient
		std::string c64player;
		c64player = sidEngine.d_sididbase.identify(c64buf);
		while (c64player.find("_") != -1)
			c64player.replace(c64player.find("_"), 1, " ");

		memcpy(sidEngine.p_sididplayer, c64player.c_str(), 25);
	}
}
// functions to load and fetch the songlengthdbase
static void loadSonglength() {
	if (!sidSetting.c_forcelength && !sidEngine.d_loadeddbase && strlen(sidSetting.c_dbpath) > 10) {
		std::string relpathName;
		if ((sidSetting.c_dbpath[0]) == '.') {
			TCHAR exepathName[FILENAME_MAX];
			GetModuleFileName(nullptr, exepathName, FILENAME_MAX);
			std::string::size_type slashPos = std::string(exepathName).find_last_of("\\/");
			relpathName = std::string(exepathName).substr(0, slashPos);
			relpathName.append("/");
			relpathName.append(sidSetting.c_dbpath);
			relpathName.append("/");
		} else {
			relpathName = sidSetting.c_dbpath;
			relpathName.append("/");
		}

		relpathName.append("Songlengths.md5");
		if (FILE* file = fopen(relpathName.c_str(), "r")) {
			fclose(file);
			sidEngine.d_loadeddbase = sidEngine.d_songdbase.open(relpathName.c_str());
		} else {
			MessageBoxA(0, relpathName.c_str(), "Songlengths.md5 Path Invalid", MB_OK);
		}
	}
}
static int fetchSonglength(SidTune* sidSong, int sidSubsong) {
	char md5[SidTune::MD5_LENGTH + 1];
	int32_t md5duration = 0;
	int32_t defaultduration = sidSetting.c_defaultlength;

	if (!sidSetting.c_forcelength && sidEngine.d_loadeddbase) {
		sidSong->createMD5New(md5);
		if (strlen(md5) > 0) {
			md5duration = sidEngine.d_songdbase.length(md5, sidSubsong);
		}
		if (md5duration > 0) {
			defaultduration = md5duration;
		}
	}

	return defaultduration;
}
// get song's tags
static char* GetTags(const SidTuneInfo* p_songinfo)
{
	static const char* tagname[3] = { "title", "artist", "date" };
	std::string taginfo;
	for (int a = 0; a < 3; a++) {
		const char* tag = p_songinfo->infoString(a);
		if (tag[0]) {
			char* tagval = xmpftext->Utf8(tag, -1);
			taginfo.append(tagname[a]);
			taginfo.push_back('\0');
			taginfo.append(tagval);
			taginfo.push_back('\0');
			xmpfmisc->Free(tagval);
		}
	}
	taginfo.append("filetype");
	taginfo.push_back('\0');
	taginfo.append(simpleFormat(p_songinfo->formatString()));
	taginfo.push_back('\0');
	taginfo.push_back('\0');
	int tagsLength = taginfo.size();
	char* tags = (char*)xmpfmisc->Alloc(tagsLength);
	memcpy(tags, taginfo.data(), tagsLength);
	return tags;
}
// try to load STIL database
static void loadSTILbase() {
	if (!sidEngine.d_loadedstil && strlen(sidSetting.c_dbpath) > 10) {
		std::string relpathName;
		if ((sidSetting.c_dbpath[0]) == '.') {
			TCHAR exepathName[FILENAME_MAX];
			GetModuleFileName(nullptr, exepathName, FILENAME_MAX);
			std::string::size_type slashPos = std::string(exepathName).find_last_of("\\/");
			relpathName = std::string(exepathName).substr(0, slashPos);
			relpathName.append("/");
			relpathName.append(sidSetting.c_dbpath);

			char abspathName[_MAX_PATH];
			_fullpath(abspathName, relpathName.c_str(), _MAX_PATH);
			relpathName = abspathName;
		} else {
			relpathName = sidSetting.c_dbpath;
		}

		relpathName.replace((relpathName.length() - 10), 10, "");
		sidEngine.d_loadedstil = sidEngine.d_stilbase.setBaseDir(relpathName.c_str());
		if (!sidEngine.d_loadedstil) {
			MessageBoxA(0, relpathName.c_str(), "STIL Path Invalid", MB_OK);
		}
	}
}
static void formatSTILbase(const char* stilData, char** buf) {
	if (stilData != NULL) {
		std::istringstream stilDatastr(stilData);
		std::string labetTxt;
		std::string stilOutput;
		while (std::getline(stilDatastr, stilOutput)) {
			if (!stilOutput.empty()) {
				stilOutput = trim(stilOutput);
				labetTxt = "";
				if (stilOutput.find("COMMENT: ") != -1) {
					stilOutput.replace(stilOutput.find("COMMENT: "), 9, "");
					labetTxt = "Comment";
				} else if (stilOutput.find("AUTHOR: ") != -1) {
					stilOutput.replace(stilOutput.find("AUTHOR: "), 8, "");
					labetTxt = "Author";
				} else if (stilOutput.find("ARTIST: ") != -1) {
					stilOutput.replace(stilOutput.find("ARTIST: "), 8, "");
					labetTxt = "Artist";
				} else if (stilOutput.find("TITLE: ") != -1) {
					stilOutput.replace(stilOutput.find("TITLE: "), 7, "");
					labetTxt = "Title";
				} else if (stilOutput.find("NAME: ") != -1) {
					stilOutput.replace(stilOutput.find("NAME: "), 6, "");
					labetTxt = "Name";
				} else if (stilOutput.find("BUG: ") != -1) {
					stilOutput.replace(stilOutput.find("BUG: "), 5, "");
					labetTxt = "Bug";
				}
				char* value = xmpftext->Utf8(stilOutput.c_str(), -1);
				*buf = xmpfmisc->FormatInfoText(*buf, labetTxt.c_str(), value);
				xmpfmisc->Free(value);
			} else {
				break;
			}
		}
	}
}
// initialise the plugin
static void WINAPI SIDevo_Init()
{
	if (sidEngine.b_reloadcfg && sidEngine.b_loaded) {
		delete sidEngine.m_builder;
		delete sidEngine.m_engine;
		sidEngine.b_loaded = FALSE;
	}
	if (!sidEngine.b_loaded) {
		// set default config
		loadConfig();

		// initialise the engine
		sidEngine.m_engine = new sidplayfp();
		sidEngine.m_engine->setRoms(kernel, basic, chargen);
		sidEngine.m_builder = new ReSIDfpBuilder("ReSIDfp");
		sidEngine.m_builder->create(sidEngine.m_engine->info().maxsids());
		if (!sidEngine.m_builder->getStatus()) {
			delete sidEngine.m_engine;
			sidEngine.b_loaded = FALSE;
		} else {
			// apply configuraton
			if (applyConfig(TRUE)) {
				sidEngine.b_loaded = TRUE;
			} else {
				delete sidEngine.m_builder;
				delete sidEngine.m_engine;
				sidEngine.b_loaded = FALSE;
			}
		}
	}
}

// general purpose
static void WINAPI SIDevo_About(HWND win)
{
	MessageBoxA(win,
		"XMPlay SIDevo plugin (v4.0)\nCopyright (c) 2023 Nathan Hindley\n\nThis plugin allows XMPlay to play sid,mus and str tunes from the Commodore 64 using the libsidplayfp-2.4.2 library.\n\nAdditional Credits:\nCopyright (c) 2000 - 2001 Simon White\nCopyright (c) 2007 - 2010 Antti Lankila\nCopyright (c) 2010 - 2021 Leandro Nini\n\nFREE FOR USE WITH XMPLAY",
		"About...",
		MB_ICONINFORMATION);
}
static BOOL WINAPI SIDevo_CheckFile(const char* filename, XMPFILE file)
{
	// +PSID, +RSID, +MUS, -P00, -PRG, -DAT, +STR
	std::string musExtension = ".mus";
	std::string strExtension = ".str";
	if (std::string(filename).length() >= musExtension.length()) {
		if (0 == std::string(filename).compare(std::string(filename).length() - musExtension.length(), musExtension.length(), musExtension)) {
			return TRUE;
		} else if (0 == std::string(filename).compare(std::string(filename).length() - strExtension.length(), strExtension.length(), strExtension)) {
			return TRUE;
		}
	}
	DWORD sidHeader;
	xmpffile->Read(file, &sidHeader, 4);
	return sidHeader == MAKEFOURCC('P', 'S', 'I', 'D') || sidHeader == MAKEFOURCC('R', 'S', 'I', 'D');
}
static DWORD WINAPI SIDevo_GetFileInfo(const char* filename, XMPFILE file, float** length, char** tags)
{
	SidTune* lu_song;
	const SidTuneInfo* lu_songinfo;
	int lu_songcount;

	std::vector<uint8_t> c64buf(xmpffile->GetSize(file));
	xmpffile->Read(file, c64buf.data(), c64buf.size());
	lu_song = new SidTune(c64buf.data(), c64buf.size());

	if (!lu_song->getStatus()) {
		delete lu_song;
		return 0;
	}

	lu_songinfo = lu_song->getInfo();
	lu_songcount = lu_songinfo->songs();

	if (length) {
		// load lengths
		loadSonglength();
		*length = (float*)xmpfmisc->Alloc(lu_songcount * sizeof(float));
		for (int si = 1; si <= lu_songcount; si++) {
			(*length)[si - 1] = fetchSonglength(lu_song, si);
		}
	}

	if (tags)
		*tags = GetTags(lu_songinfo);

	delete lu_song;

	return lu_songcount | XMPIN_INFO_NOSUBTAGS;
}
static DWORD WINAPI SIDevo_GetSubSongs(float* length) {
	*length = sidEngine.p_songlength;
	return sidEngine.p_songcount;
}
static char* WINAPI SIDevo_GetTags()
{
	return GetTags(sidEngine.p_songinfo);
}
static void WINAPI SIDevo_GetInfoText(char* format, char* length)
{
	if (format) {
		if (strlen(sidEngine.p_sididplayer) > 0)
			sprintf(format, "%s - %s", simpleFormat(sidEngine.p_songinfo->formatString()), sidEngine.p_sididplayer);
		else
			sprintf(format, "%s", simpleFormat(sidEngine.p_songinfo->formatString()));
	}
	if (length) {
		if (length[0]) // got length text in the buffer, append to it
			sprintf(length + strlen(length), " - %s - %s", sidEngine.p_sidmodel, sidEngine.p_clockspeed);
		else
			sprintf(length, "%s - %s", sidEngine.p_sidmodel, sidEngine.p_clockspeed);
	}
}
static void WINAPI SIDevo_GetGeneralInfo(char* buf)
{
	static char temp[32]; // buffer for simpleLength

	buf += sprintf(buf, "%s\t%s\r", "Format", sidEngine.p_songinfo->formatString());
	if (strlen(sidEngine.p_sididplayer) > 0)
		buf += sprintf(buf, "%s\t%s\r", "Player", sidEngine.p_sididplayer);

	if (sidEngine.o_sidchips > 0) {
		buf += sprintf(buf, "%s\t#%d", "Sid Info", sidEngine.o_sidchips);
		if (strlen(sidEngine.o_sidmodel) > 0) {
			buf += sprintf(buf, " - %s", sidEngine.o_sidmodel);
			if (strlen(sidEngine.o_sidmodel) > 0 && strcmp(sidEngine.o_sidmodel, sidEngine.p_sidmodel) != 0) {
				buf += sprintf(buf, " (%s)", sidEngine.p_sidmodel);
			}
		}
		if (strlen(sidEngine.o_clockspeed) > 0) {
			buf += sprintf(buf, " - %s", sidEngine.o_clockspeed);
			if (strlen(sidEngine.o_clockspeed) > 0 && strcmp(sidEngine.o_clockspeed, sidEngine.p_clockspeed) != 0) {
				buf += sprintf(buf, " (%s)", sidEngine.p_clockspeed);
			}
		}
		buf += sprintf(buf, "\r");
	}

	if (sidEngine.p_songcount > 1) { // only display subsong info if there are more than 1
		buf += sprintf(buf, "Subsongs");
		for (int si = 1; si <= sidEngine.p_songcount; si++) {
			buf += sprintf(buf, "\t%d. %s", si, simpleLength(sidEngine.p_subsonglength[si], temp));
			if (si == sidEngine.p_defsubsong) {
				buf += sprintf(buf, " (Default)");
			}
			buf += sprintf(buf, "\r");
		}
	}

	buf += sprintf(buf, "%s\t%s\r", "Length", simpleLength(sidEngine.p_songlength, temp));
	buf += sprintf(buf, "%s\t%s\r", "Library", "libsidplayfp-2.4.2");
}
static void WINAPI SIDevo_GetMessage(char* buf)
{
	// Load STIL database
	loadSTILbase();
	if (sidEngine.d_loadedstil) {
		const char* stilComment = NULL;
		const char* stilEntry = NULL;
		const char* stilBug = NULL;

		// txt stil lookup
		if (sidEngine.d_loadedstil) {
			stilComment = sidEngine.d_stilbase.getAbsGlobalComment(sidEngine.o_filename);
			stilEntry = sidEngine.d_stilbase.getAbsEntry(sidEngine.o_filename, 0, STIL::all);
			stilBug = sidEngine.d_stilbase.getAbsBug(sidEngine.o_filename, sidEngine.p_subsong);
		}

		if (stilComment != NULL) {
			buf += sprintf(buf, "STIL Global Comment\t-=-\r");
			formatSTILbase(stilComment, &buf);
			buf += sprintf(buf, "\r");
		}
		if (stilEntry != NULL) {
			buf += sprintf(buf, "STIL Tune Entry\t-=-\r");
			formatSTILbase(stilEntry, &buf);
			buf += sprintf(buf, "\r");
		}
		if (stilBug != NULL) {
			buf += sprintf(buf, "STIL Tune Bug\t-=-\r");
			formatSTILbase(stilBug, &buf);
			buf += sprintf(buf, "\r");
		}
	}
}

// handle playback
static DWORD WINAPI SIDevo_Open(const char* filename, XMPFILE file)
{
	SIDevo_Init();
	if (sidEngine.b_loaded) {
		// load sid file information into struct for reference
		sidEngine.o_filename = filename;

		std::vector<uint8_t> c64buf(xmpffile->GetSize(file));
		xmpffile->Read(file, c64buf.data(), c64buf.size());
		sidEngine.p_song = new SidTune(c64buf.data(), c64buf.size());
		if (!sidEngine.p_song->getStatus()) {
			return 0;
		} else {
			sidEngine.p_songinfo = sidEngine.p_song->getInfo();
			sidEngine.p_songcount = sidEngine.p_songinfo->songs();
			sidEngine.p_subsong = 1;
			sidEngine.p_song->selectSong(sidEngine.p_subsong);

			// Set defaults for sid module
			sidEngine.o_sidchips = sidEngine.p_songinfo->sidChips();
			if (sidEngine.p_songinfo->sidModel(0) == SidTuneInfo::SIDMODEL_6581) {
				strncpy(sidEngine.o_sidmodel, "6581", 10);
			} else if (sidEngine.p_songinfo->sidModel(0) == SidTuneInfo::SIDMODEL_8580) {
				strncpy(sidEngine.o_sidmodel, "8580", 10);
			} else if (sidEngine.p_songinfo->sidModel(0) == SidTuneInfo::SIDMODEL_ANY) {
				strncpy(sidEngine.o_sidmodel, "Any", 10);
			} else if (sidEngine.p_songinfo->sidModel(0) == SidTuneInfo::SIDMODEL_UNKNOWN) {
				strncpy(sidEngine.o_sidmodel, "Unknown", 10);
			}
			if (sidEngine.p_songinfo->clockSpeed() == SidTuneInfo::CLOCK_PAL) {
				strncpy(sidEngine.o_clockspeed, "PAL", 10);
			} else if (sidEngine.p_songinfo->clockSpeed() == SidTuneInfo::CLOCK_NTSC) {
				strncpy(sidEngine.o_clockspeed, "NTSC", 10);
			} else if (sidEngine.p_songinfo->clockSpeed() == SidTuneInfo::CLOCK_ANY) {
				strncpy(sidEngine.o_clockspeed, "Any", 10);
			} else if (sidEngine.p_songinfo->clockSpeed() == SidTuneInfo::CLOCK_UNKNOWN) {
				strncpy(sidEngine.o_clockspeed, "Unknown", 10);
			}

			// Apply overrides or unsets out sid model and clock speed
			if (sidEngine.m_config.forceSidModel || sidEngine.p_songinfo->sidModel(0) == SidTuneInfo::SIDMODEL_ANY || sidEngine.p_songinfo->sidModel(0) == SidTuneInfo::SIDMODEL_UNKNOWN) {
				if (sidEngine.m_config.defaultSidModel == SidConfig::MOS6581) {
					strncpy(sidEngine.p_sidmodel, "6581", 10);
				} else if (sidEngine.m_config.defaultSidModel == SidConfig::MOS8580) {
					strncpy(sidEngine.p_sidmodel, "8580", 10);
				}
			} else {
				strncpy(sidEngine.p_sidmodel, sidEngine.o_sidmodel, 10);
			}
			if (sidEngine.m_config.forceC64Model || sidEngine.p_songinfo->clockSpeed() == SidTuneInfo::CLOCK_ANY || sidEngine.p_songinfo->clockSpeed() == SidTuneInfo::CLOCK_UNKNOWN) {
				if (sidEngine.m_config.defaultC64Model == SidConfig::PAL) {
					strncpy(sidEngine.p_clockspeed, "PAL", 10);
				} else if (sidEngine.m_config.defaultC64Model == SidConfig::NTSC) {
					strncpy(sidEngine.p_clockspeed, "NTSC", 10);
				}
			} else {
				strncpy(sidEngine.p_clockspeed, sidEngine.o_clockspeed, 10);
			}

			// enforce default options
			sidEngine.p_defsubsong = sidEngine.p_songinfo->startSong();
			if (sidSetting.c_defaultskip && sidEngine.p_defsubsong > 1) {
				sidEngine.skiptrigger = TRUE;
			} else {
				sidEngine.skiptrigger = FALSE;
			}

			// detect player
			loadSIDId();
			fetchSIDId(c64buf);

			// load lengths
			loadSonglength();
			sidEngine.p_subsonglength = new int[sidEngine.p_songcount + 1];
			sidEngine.p_songlength = 0;
			for (int si = 1; si <= sidEngine.p_songcount; si++) {
				int defaultduration = fetchSonglength(sidEngine.p_song, si);
				sidEngine.p_subsonglength[si] = defaultduration;
				sidEngine.p_songlength += defaultduration;
			}

			if (sidEngine.m_engine->load(sidEngine.p_song)) {
				if (sidSetting.c_defaultlength == 0 || sidSetting.c_disableseek) {
					xmpfin->SetLength(sidEngine.p_subsonglength[sidEngine.p_subsong], FALSE);
				} else {
					xmpfin->SetLength(sidEngine.p_subsonglength[sidEngine.p_subsong], TRUE);
				}
				sidEngine.fadein = 0; // trigger fade-in
				sidEngine.fadeout = 1; // trigger fade-out
				return 2;
			} else {
				return 0;
			}
		}
	} else {
		return 0;
	}
}
static void WINAPI SIDevo_Close()
{
	if (sidEngine.p_song) {
		if (sidEngine.m_engine->isPlaying()) {
			sidEngine.m_engine->stop();
		}
		delete sidEngine.p_subsonglength;
		delete sidEngine.p_sididplayer;
		delete sidEngine.p_song;
	}
}
static DWORD WINAPI SIDevo_Process(float* buffer, DWORD count)
{
	// enforce default options
	if (sidSetting.c_defaultskip && sidSetting.c_defaultonly && sidEngine.p_subsong != sidEngine.p_defsubsong) {
		return 0;
	} else if (sidEngine.skiptrigger && sidEngine.p_subsong < sidEngine.p_defsubsong) {
		return 0;
	} else if (sidEngine.skiptrigger) {
		sidEngine.skiptrigger = FALSE;
	}

	// skip short song
	if (sidSetting.c_skipshort && sidSetting.c_minlength >= sidEngine.p_subsonglength[sidEngine.p_subsong]) {
		return 0;
	}

	// process
	if (sidEngine.m_engine->time() < sidEngine.p_subsonglength[sidEngine.p_subsong] || sidSetting.c_defaultlength == 0) {
		// set-up fade-in & fade-out
		float fadestep;
		if (sidEngine.fadein < 1) {
			if (sidSetting.c_fadein && sidSetting.c_fadeinms > 0) {
				if (!sidEngine.fadein) sidEngine.fadein = 0.001;
				fadestep = pow(10, 3000.0 / sidSetting.c_fadeinms / (sidEngine.m_config.frequency * sidEngine.m_config.playback));
			} else
				sidEngine.fadein = 1;
		} else if (sidEngine.fadeout > 0) {
			if (sidSetting.c_fadeout && sidSetting.c_fadeoutms > 0) {
				sidEngine.fadeouttrigger = sidEngine.p_subsonglength[sidEngine.p_subsong] - sidSetting.c_fadeoutms / 1000; // calc trigger fade-out
				if (sidEngine.fadeouttrigger + 1 > 0) {
					if (sidEngine.fadeout == 1) sidEngine.fadeout = 0.999;
					fadestep = 1.0 / (float)((sidSetting.c_fadeoutms / static_cast<float>(1000)) * sidEngine.m_config.frequency * sidEngine.m_config.playback);
				} else
					sidEngine.fadeout = 0;
			} else
				sidEngine.fadeout = 0;
		}

		int sidDone, i;
		short* sidbuffer = new short[count];
		sidDone = sidEngine.m_engine->play(sidbuffer, count);
		for (i = 0; i < sidDone; i++) {
			float scale = 1 / 32768.f;
			// perform fade-in & fade-out
			if (sidEngine.fadein < 1) {
				sidEngine.fadein *= fadestep;
				if (sidEngine.fadein > 1) sidEngine.fadein = 1;
				scale *= sidEngine.fadein;
			} else if (sidEngine.fadeout > 0 && sidEngine.m_engine->time() > sidEngine.fadeouttrigger) {
				//sidEngine.fadeout /= sidEngine.fadeoutstep;
				sidEngine.fadeout -= fadestep;
				if (sidEngine.fadeout < 0) sidEngine.fadeout = 0;
				scale *= sidEngine.fadeout;
			}
			buffer[i] = (float)(sidbuffer[i]) * scale;
		}
		delete sidbuffer;

		return sidDone;
	} else {
		return 0;
	}
}
static void WINAPI SIDevo_SetFormat(XMPFORMAT* form)
{
	// changing format seems to rewind the SID decoder? so only do that at start
	if (!sidEngine.m_engine->timeMs()) {
		sidEngine.m_config.frequency = std::max<DWORD>(form->rate, 8000);
		sidEngine.m_config.playback = (SidConfig::playback_t)std::min<DWORD>(form->chan, 2);
		applyConfig(FALSE);
	}
	form->rate = sidEngine.m_config.frequency;
	form->chan = sidEngine.m_config.playback;
	form->res = 16 / 8;
}
static double WINAPI SIDevo_GetGranularity()
{
	return 0.001;
}
static double WINAPI SIDevo_SetPosition(DWORD pos)
{
	if (pos & XMPIN_POS_SUBSONG1 || pos & XMPIN_POS_SUBSONG) {
		if (sidEngine.m_engine->isPlaying()) {
			sidEngine.m_engine->stop();
		}
		sidEngine.p_subsong = LOWORD(pos) + 1;
		sidEngine.p_song->selectSong(sidEngine.p_subsong);
		sidEngine.m_engine->load(sidEngine.p_song);
		//
		if (sidSetting.c_defaultlength == 0 || sidSetting.c_disableseek) {
			xmpfin->SetLength(sidEngine.p_subsonglength[sidEngine.p_subsong], FALSE);
		} else {
			xmpfin->SetLength(sidEngine.p_subsonglength[sidEngine.p_subsong], TRUE);
		}
		sidEngine.fadein = 0; // trigger fade-in (needed?)
		sidEngine.fadeout = 1; // trigger fade-out (needed?)
		xmpfin->UpdateTitle(NULL);
		//
		return 0;
	} else {
		double seekTarget = pos * SIDevo_GetGranularity();
		double seekState = sidEngine.m_engine->timeMs() / 1000.0;
		int seekResult;

		if (seekTarget == seekState)
			return seekTarget;

		//oh dear we have to go back
		if (seekTarget < seekState) {
			if (sidEngine.m_engine->isPlaying()) {
				sidEngine.m_engine->stop();
				sidEngine.m_engine->load(0);
				sidEngine.m_engine->load(sidEngine.p_song);
			}
			seekState = 0;
		}

		//attempt to seek
		int seekCount = (int)((seekTarget - seekState) * sidEngine.m_config.frequency) * sidEngine.m_config.playback;
		short* seekBuffer = new short[seekCount];
		seekResult = sidEngine.m_engine->play(seekBuffer, seekCount);
		delete[] seekBuffer;
		if (seekResult == seekCount) {
			sidEngine.fadein = pos ? 1 : 0; // trigger fade-in if restarting
			if (!pos || pos < sidEngine.fadeouttrigger)
				sidEngine.fadeout = 1; // trigger fade-out if restarting

			return seekTarget;
		} else {
			return -1;
		}
	}
}

// handle configuration
#define MESS(id,m,w,l) SendDlgItemMessage(hWnd,id,m,(WPARAM)(w),(LPARAM)(l))
static BOOL CALLBACK CFGDialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_NOTIFY:
		switch (LOWORD(wParam)) {
		case IDC_CHECK_ENABLEFILTER:
			if (MESS(IDC_CHECK_ENABLEFILTER, BM_GETCHECK, 0, 0)) {
				EnableWindow(GetDlgItem(hWnd, IDC_SLIDE_6581LEVEL), TRUE);
				EnableWindow(GetDlgItem(hWnd, IDC_SLIDE_8580LEVEL), TRUE);
				EnableWindow(GetDlgItem(hWnd, IDC_LABEL_6581LEVEL), TRUE);
				EnableWindow(GetDlgItem(hWnd, IDC_LABEL_8580LEVEL), TRUE);
				EnableWindow(GetDlgItem(hWnd, IDC_TITLE_6581LEVEL), TRUE);
				EnableWindow(GetDlgItem(hWnd, IDC_TITLE_8580LEVEL), TRUE);
			} else {
				EnableWindow(GetDlgItem(hWnd, IDC_SLIDE_6581LEVEL), FALSE);
				EnableWindow(GetDlgItem(hWnd, IDC_SLIDE_8580LEVEL), FALSE);
				EnableWindow(GetDlgItem(hWnd, IDC_LABEL_6581LEVEL), FALSE);
				EnableWindow(GetDlgItem(hWnd, IDC_LABEL_8580LEVEL), FALSE);
				EnableWindow(GetDlgItem(hWnd, IDC_TITLE_6581LEVEL), FALSE);
				EnableWindow(GetDlgItem(hWnd, IDC_TITLE_8580LEVEL), FALSE);
			}
		case IDC_CHECK_FADEIN:
			if (MESS(IDC_CHECK_FADEIN, BM_GETCHECK, 0, 0)) {
				EnableWindow(GetDlgItem(hWnd, IDC_SLIDE_FADEINLEVEL), TRUE);
				EnableWindow(GetDlgItem(hWnd, IDC_LABEL_FADEINLEVEL), TRUE);
			} else {
				EnableWindow(GetDlgItem(hWnd, IDC_SLIDE_FADEINLEVEL), FALSE);
				EnableWindow(GetDlgItem(hWnd, IDC_LABEL_FADEINLEVEL), FALSE);
			}
		case IDC_CHECK_FADEOUT:
			if (MESS(IDC_CHECK_FADEOUT, BM_GETCHECK, 0, 0)) {
				EnableWindow(GetDlgItem(hWnd, IDC_SLIDE_FADEOUTLEVEL), TRUE);
				EnableWindow(GetDlgItem(hWnd, IDC_LABEL_FADEOUTLEVEL), TRUE);
			} else {
				EnableWindow(GetDlgItem(hWnd, IDC_SLIDE_FADEOUTLEVEL), FALSE);
				EnableWindow(GetDlgItem(hWnd, IDC_LABEL_FADEOUTLEVEL), FALSE);
			}
		case IDC_CHECK_SKIPSHORT:
			if (MESS(IDC_CHECK_SKIPSHORT, BM_GETCHECK, 0, 0)) {
				EnableWindow(GetDlgItem(hWnd, IDC_EDIT_MINLENGTH), TRUE);
				EnableWindow(GetDlgItem(hWnd, IDC_LABEL_MINLENGTH), TRUE);
			} else {
				EnableWindow(GetDlgItem(hWnd, IDC_EDIT_MINLENGTH), FALSE);
				EnableWindow(GetDlgItem(hWnd, IDC_LABEL_MINLENGTH), FALSE);
			}
		case IDC_CHECK_RANDOMDELAY:
			if (MESS(IDC_CHECK_RANDOMDELAY, BM_GETCHECK, 0, 0)) {
				EnableWindow(GetDlgItem(hWnd, IDC_EDIT_POWERDELAY), FALSE);
			} else {
				EnableWindow(GetDlgItem(hWnd, IDC_EDIT_POWERDELAY), TRUE);
			}
		case IDC_CHECK_DEFAULTSKIP:
			if (MESS(IDC_CHECK_DEFAULTSKIP, BM_GETCHECK, 0, 0)) {
				EnableWindow(GetDlgItem(hWnd, IDC_CHECK_DEFAULTONLY), TRUE);
			} else {
				EnableWindow(GetDlgItem(hWnd, IDC_CHECK_DEFAULTONLY), FALSE);
			}
		case IDC_SLIDE_6581LEVEL:
			MESS(IDC_LABEL_6581LEVEL, WM_SETTEXT, 0, std::to_string(SendDlgItemMessage(hWnd, IDC_SLIDE_6581LEVEL, (UINT)TBM_GETPOS, (WPARAM)0, (LPARAM)0)).append("%").c_str());
		case IDC_SLIDE_8580LEVEL:
			MESS(IDC_LABEL_8580LEVEL, WM_SETTEXT, 0, std::to_string(SendDlgItemMessage(hWnd, IDC_SLIDE_8580LEVEL, (UINT)TBM_GETPOS, (WPARAM)0, (LPARAM)0)).append("%").c_str());
		case IDC_SLIDE_FADEINLEVEL:
			MESS(IDC_LABEL_FADEINLEVEL, WM_SETTEXT, 0, std::to_string((float)SendDlgItemMessage(hWnd, IDC_SLIDE_FADEINLEVEL, (UINT)TBM_GETPOS, (WPARAM)0, (LPARAM)0) / 100.f).substr(0, 4).append("s").c_str());
		case IDC_SLIDE_FADEOUTLEVEL:
			if (SendDlgItemMessage(hWnd, IDC_SLIDE_FADEOUTLEVEL, (UINT)TBM_GETPOS, (WPARAM)0, (LPARAM)0) >= 100) {
				MESS(IDC_LABEL_FADEOUTLEVEL, WM_SETTEXT, 0, std::to_string((float)SendDlgItemMessage(hWnd, IDC_SLIDE_FADEOUTLEVEL, (UINT)TBM_GETPOS, (WPARAM)0, (LPARAM)0) / 10.f).substr(0, 5).append("s").c_str());
			} else {
				MESS(IDC_LABEL_FADEOUTLEVEL, WM_SETTEXT, 0, std::to_string((float)SendDlgItemMessage(hWnd, IDC_SLIDE_FADEOUTLEVEL, (UINT)TBM_GETPOS, (WPARAM)0, (LPARAM)0) / 10.f).substr(0, 4).append("s").c_str());
			}
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			sidSetting.c_locksidmodel = (BST_CHECKED == MESS(IDC_CHECK_LOCKSID, BM_GETCHECK, 0, 0));
			sidSetting.c_lockclockspeed = (BST_CHECKED == MESS(IDC_CHECK_LOCKCLOCK, BM_GETCHECK, 0, 0));
			sidSetting.c_enabledigiboost = (BST_CHECKED == MESS(IDC_CHECK_DIGIBOOST, BM_GETCHECK, 0, 0));
			sidSetting.c_enablefilter = (BST_CHECKED == MESS(IDC_CHECK_ENABLEFILTER, BM_GETCHECK, 0, 0));
			sidSetting.c_powerdelayrandom = (BST_CHECKED == MESS(IDC_CHECK_RANDOMDELAY, BM_GETCHECK, 0, 0));
			sidSetting.c_forcelength = (BST_CHECKED == MESS(IDC_CHECK_FORCELENGTH, BM_GETCHECK, 0, 0));
			sidSetting.c_skipshort = (BST_CHECKED == MESS(IDC_CHECK_SKIPSHORT, BM_GETCHECK, 0, 0));
			sidSetting.c_fadein = (BST_CHECKED == MESS(IDC_CHECK_FADEIN, BM_GETCHECK, 0, 0));
			sidSetting.c_fadeout = (BST_CHECKED == MESS(IDC_CHECK_FADEOUT, BM_GETCHECK, 0, 0));
			sidSetting.c_disableseek = (BST_CHECKED == MESS(IDC_CHECK_DISABLESEEK, BM_GETCHECK, 0, 0));
			sidSetting.c_detectplayer = (BST_CHECKED == MESS(IDC_CHECK_DETECTPLAYER, BM_GETCHECK, 0, 0));
			sidSetting.c_defaultskip = (BST_CHECKED == MESS(IDC_CHECK_DEFAULTSKIP, BM_GETCHECK, 0, 0));
			sidSetting.c_defaultonly = (BST_CHECKED == MESS(IDC_CHECK_DEFAULTONLY, BM_GETCHECK, 0, 0));
			MESS(IDC_COMBO_SID, WM_GETTEXT, 10, sidSetting.c_sidmodel);
			MESS(IDC_COMBO_CLOCK, WM_GETTEXT, 10, sidSetting.c_clockspeed);
			MESS(IDC_COMBO_SAMPLEMETHOD, WM_GETTEXT, 10, sidSetting.c_samplemethod);
			MESS(IDC_EDIT_DBPATH, WM_GETTEXT, 250, sidSetting.c_dbpath);
			sidSetting.c_defaultlength = GetDlgItemInt(hWnd, IDC_EDIT_DEFAULTLENGTH, NULL, false);
			sidSetting.c_minlength = GetDlgItemInt(hWnd, IDC_EDIT_MINLENGTH, NULL, false);
			sidSetting.c_powerdelay = GetDlgItemInt(hWnd, IDC_EDIT_POWERDELAY, NULL, false);
			sidSetting.c_6581filter = SendDlgItemMessage(hWnd, IDC_SLIDE_6581LEVEL, TBM_GETPOS, 0, 0);
			sidSetting.c_8580filter = SendDlgItemMessage(hWnd, IDC_SLIDE_8580LEVEL, TBM_GETPOS, 0, 0);
			sidSetting.c_fadeinms = SendDlgItemMessage(hWnd, IDC_SLIDE_FADEINLEVEL, TBM_GETPOS, 0, 0) * 10;
			sidSetting.c_fadeoutms = SendDlgItemMessage(hWnd, IDC_SLIDE_FADEOUTLEVEL, TBM_GETPOS, 0, 0) * 100;

			// apply configuraton
			saveConfig();
			EndDialog(hWnd, 0);
			break;
		case IDC_BUTTON_FOLDER:
			SetDlgItemTextA(hWnd, IDC_EDIT_DBPATH, selectFolder(hWnd).c_str());
			MESS(IDC_EDIT_DBPATH, WM_GETTEXT, 250, sidSetting.c_dbpath);
			MESS(IDC_LABEL_STATUS, WM_SETTEXT, 0, pathTest().c_str());
			break;
		case IDC_BUTTON_TEST:
			MESS(IDC_EDIT_DBPATH, WM_GETTEXT, 250, sidSetting.c_dbpath);
			MESS(IDC_LABEL_STATUS, WM_SETTEXT, 0, pathTest().c_str());
			break;
		case IDCANCEL:
			EndDialog(hWnd, 0);
			break;
		}
		break;
	case WM_INITDIALOG:
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_SID), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)TEXT("6581"));
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_SID), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)TEXT("8580"));
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_SID), CB_SELECTSTRING, (WPARAM)-1, (LPARAM)sidSetting.c_sidmodel);
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_CLOCK), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)TEXT("PAL"));
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_CLOCK), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)TEXT("NTSC"));
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_CLOCK), CB_SELECTSTRING, (WPARAM)-1, (LPARAM)sidSetting.c_clockspeed);
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_SAMPLEMETHOD), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)TEXT("Normal"));
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_SAMPLEMETHOD), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)TEXT("Accurate"));
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_SAMPLEMETHOD), CB_SELECTSTRING, (WPARAM)-1, (LPARAM)sidSetting.c_samplemethod);
		//
		MESS(IDC_SLIDE_6581LEVEL, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
		MESS(IDC_SLIDE_6581LEVEL, TBM_SETPOS, TRUE, sidSetting.c_6581filter);
		MESS(IDC_SLIDE_8580LEVEL, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
		MESS(IDC_SLIDE_8580LEVEL, TBM_SETPOS, TRUE, sidSetting.c_8580filter);
		MESS(IDC_SLIDE_FADEINLEVEL, TBM_SETRANGE, TRUE, MAKELONG(0, 30));
		MESS(IDC_SLIDE_FADEINLEVEL, TBM_SETPOS, TRUE, sidSetting.c_fadeinms / 10);
		MESS(IDC_SLIDE_FADEOUTLEVEL, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
		MESS(IDC_SLIDE_FADEOUTLEVEL, TBM_SETPOS, TRUE, sidSetting.c_fadeoutms / 100);
		//
		MESS(IDC_CHECK_LOCKSID, BM_SETCHECK, sidSetting.c_locksidmodel ? BST_CHECKED : BST_UNCHECKED, 0);
		MESS(IDC_CHECK_LOCKCLOCK, BM_SETCHECK, sidSetting.c_lockclockspeed ? BST_CHECKED : BST_UNCHECKED, 0);
		MESS(IDC_CHECK_ENABLEFILTER, BM_SETCHECK, sidSetting.c_enablefilter ? BST_CHECKED : BST_UNCHECKED, 0);
		MESS(IDC_CHECK_DIGIBOOST, BM_SETCHECK, sidSetting.c_enabledigiboost ? BST_CHECKED : BST_UNCHECKED, 0);
		MESS(IDC_CHECK_FORCELENGTH, BM_SETCHECK, sidSetting.c_forcelength ? BST_CHECKED : BST_UNCHECKED, 0);
		MESS(IDC_CHECK_RANDOMDELAY, BM_SETCHECK, sidSetting.c_powerdelayrandom ? BST_CHECKED : BST_UNCHECKED, 0);
		MESS(IDC_CHECK_DISABLESEEK, BM_SETCHECK, sidSetting.c_disableseek ? BST_CHECKED : BST_UNCHECKED, 0);
		MESS(IDC_CHECK_DETECTPLAYER, BM_SETCHECK, sidSetting.c_detectplayer ? BST_CHECKED : BST_UNCHECKED, 0);
		MESS(IDC_CHECK_DEFAULTSKIP, BM_SETCHECK, sidSetting.c_defaultskip ? BST_CHECKED : BST_UNCHECKED, 0);
		MESS(IDC_CHECK_DEFAULTONLY, BM_SETCHECK, sidSetting.c_defaultonly ? BST_CHECKED : BST_UNCHECKED, 0);
		MESS(IDC_CHECK_SKIPSHORT, BM_SETCHECK, sidSetting.c_skipshort ? BST_CHECKED : BST_UNCHECKED, 0);
		MESS(IDC_CHECK_FADEIN, BM_SETCHECK, sidSetting.c_fadein ? BST_CHECKED : BST_UNCHECKED, 0);
		MESS(IDC_CHECK_FADEOUT, BM_SETCHECK, sidSetting.c_fadeout ? BST_CHECKED : BST_UNCHECKED, 0);
		SetDlgItemInt(hWnd, IDC_EDIT_DEFAULTLENGTH, sidSetting.c_defaultlength, false);
		SetDlgItemInt(hWnd, IDC_EDIT_MINLENGTH, sidSetting.c_minlength, false);
		SetDlgItemInt(hWnd, IDC_EDIT_POWERDELAY, sidSetting.c_powerdelay, false);
		SetDlgItemTextA(hWnd, IDC_EDIT_DBPATH, sidSetting.c_dbpath);
		MESS(IDC_LABEL_STATUS, WM_SETTEXT, 0, pathTest().c_str());
		//
		return TRUE;
	}
	return FALSE;
}
static void WINAPI SIDevo_Config(HWND win)
{
	DialogBox(ghInstance, MAKEINTRESOURCE(IDD_DIALOG_CONFIG), win, &CFGDialogProc);
}

// plugin interface
static XMPIN xmpin = {
	0,
	"SIDevo (v4.0)",
	"SIDevo\0sid/mus/str",
	SIDevo_About,
	SIDevo_Config,
	SIDevo_CheckFile,
	SIDevo_GetFileInfo,
	SIDevo_Open,
	SIDevo_Close,
	NULL, // reserved1
	SIDevo_SetFormat,
	SIDevo_GetTags,
	SIDevo_GetInfoText,
	SIDevo_GetGeneralInfo,
	SIDevo_GetMessage,
	SIDevo_SetPosition,
	SIDevo_GetGranularity,
	NULL, // GetBuffering
	SIDevo_Process,
	NULL, // WriteFile
	NULL, // GetSamples
	SIDevo_GetSubSongs, // GetSubSongs
	NULL, // reserved3
	NULL, // GetDownloaded
	NULL, // visname
	NULL, // VisOpen
	NULL, // VisClose
	NULL, // VisSize
	NULL, // VisRender
	NULL, // VisRenderDC
	NULL, // VisButton
	NULL, // reserved2
	NULL, // GetConfig,
	NULL, // SetConfig
};

// get the plugin's XMPIN interface
#ifdef __cplusplus
extern "C"
#endif
XMPIN * WINAPI XMPIN_GetInterface(DWORD face, InterfaceProc faceproc)
{
	if (face != XMPIN_FACE) { // unsupported version
		static int shownerror = 0;
		if (face < XMPIN_FACE && !shownerror) {
			MessageBoxA(0, "The XMP-SIDevo plugin requires XMPlay 3.8 or above", 0, MB_ICONEXCLAMATION);
			shownerror = 1;
		}
		return NULL;
	}
	xmpfin = (XMPFUNC_IN*)faceproc(XMPFUNC_IN_FACE);
	xmpfmisc = (XMPFUNC_MISC*)faceproc(XMPFUNC_MISC_FACE);
	xmpffile = (XMPFUNC_FILE*)faceproc(XMPFUNC_FILE_FACE);
	xmpftext = (XMPFUNC_TEXT*)faceproc(XMPFUNC_TEXT_FACE);
	xmpfreg = (XMPFUNC_REGISTRY*)faceproc(XMPFUNC_REGISTRY_FACE);

	loadConfig();

	return &xmpin;
}

BOOL WINAPI DllMain(HINSTANCE hDLL, DWORD reason, LPVOID reserved)
{
	switch (reason) {
	case DLL_PROCESS_ATTACH:
		ghInstance = (HINSTANCE)hDLL;
		DisableThreadLibraryCalls((HMODULE)hDLL);
		break;
	}
	return TRUE;
}