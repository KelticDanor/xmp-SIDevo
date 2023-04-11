# SIDevo Input Plug-in
SID Input Plugin for the XMPlay audio player


### Description
This is a partial rewrite and update of my old SIDex v1.0~ plugin, it has now been rewritten using Visual Studio 2022 and the most recent libsidplayfp-2.4.2 library.

- Fade-in support to try and remove clicks
- SLDB, STIL, BUGlist from HVSC supported
- Configurable SIDId support via sidid.cfg files


### SIDId Support
xmp-sidevo does not keep a copy of the SIDId database internally but a sidid.cfg file is
included in the archive. The sidid.cfg must be in the same location as the plugin.

You can also download the latest sidid.cfg file from the SIDId GitHub here: https://github.com/cadaver/sidid


### Change Log
v4.1
- libsidplayfp library updated to 2.5.0a

v4.0
- Initial new release
- libsidplayfp library updated to 2.4.2
- Sid Info added to info window showing no. chips/model/clock speed + overrides
- sidid.cfg updated to version from 23 December 2022
- Emulation config options now apply on the next loaded sid instead of restart
- Skip to Default Subsong ...and only play the Default options added
- Seeking is no longer disabled by default
- MUS and STR files are now supported so enjoy that Compute's Gazette Sid Collection
- Tunes are now loaded from memory instead of the file system which fixes...
	- Loading of foreign files with incompatible characters
	- Loading of sids directly from web urls
- sidid.cfg is now loaded from the plugin directly instead of HVSC DOCUMENTS/
- HVSC Documents path purpose made clearer and added browse & test buttons
- Windows XP support added


### Big Thanks To
- Ian Luck ~~~ Code references, logarithmic fade effects and a metric TON of code fixes throughout
- drfiemost (Leandro Nini) ~~~ All the help along the way
- emoon (Daniel Collin) ~~~ Help building libsidplayfp in the first place & code references in HippoPlayer
- hermansr (Roland Hermans) ~~~ Code references in PSID64 adding SIDId support
- cadaver (Lasse Öörni) ~~~ Code references in SIDId and sidid.cfg files
- kode54 (Christopher Snowhill) ~~~ Code references in foo_sid
- z80maniac (Alexey Parfenov) ~~~ Code references in xmp-zxtune
- zbych-r / in_sidplay2 ~~~ Code references in in_sidplay2
- Malade (William Cronin) / xmp-sidex v2 ~~~ Code references in xmp-sidex v2

