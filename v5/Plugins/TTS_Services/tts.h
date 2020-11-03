//@AUTOHEADER@BEGIN@
/**********************************************************************\
|                          ShoutIRC RadioBot                           |
|           Copyright 2004-2020 Drift Solutions / Indy Sams            |
|        More information available at https://www.shoutirc.com        |
|                                                                      |
|                    This file is part of RadioBot.                    |
|                                                                      |
|   RadioBot is free software: you can redistribute it and/or modify   |
| it under the terms of the GNU General Public License as published by |
|  the Free Software Foundation, either version 3 of the License, or   |
|                 (at your option) any later version.                  |
|                                                                      |
|     RadioBot is distributed in the hope that it will be useful,      |
|    but WITHOUT ANY WARRANTY; without even the implied warranty of    |
|     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the     |
|             GNU General Public License for more details.             |
|                                                                      |
|  You should have received a copy of the GNU General Public License   |
|  along with RadioBot. If not, see <https://www.gnu.org/licenses/>.   |
\**********************************************************************/
//@AUTOHEADER@END@

/**
 * \defgroup tts Text-to-Speech API
 * @{
*/

///< Voices engines are only for Linux/BSD only, these options will have no effect on Windows.
enum VOICE_ENGINES {
	VE_NO_OVERRIDE	= 0,
	VE_DEFAULT		= 0,
	VE_ESPEAK		= 1,
	VE_FESTIVAL		= 2,
	VE_WIN32		= 3,
	VE_GTRANS		= 4
};

enum VOICE_OUTPUT_FORMATS {
	VOF_MP3,
	VOF_WAV
};

struct TTS_JOB {
	union {
		const char * text; ///< text to convert
		const char * inputFN; ///< input filename
	};
	const char * outputFN; ///< output filename

	// these options only apply to MP3 jobs and WAV jobs on Windows (ignored for Linux WAV jobs)
	int32 channels; ///< Windows-WAV note: 1 or 2 for mono/stereo respectively. MP3: 0 = LAME default, 1 = Mono, 2 = Joint Stereo, 3 = Stereo
	int32 sample; ///< samplerate

	// these options only apply to MP3 jobs
	int32 bitrate; ///< 0 for LAME default

	VOICE_ENGINES EngineOverride; ///< You can use this to force the TTS processor to use a different voice engine than the one it's configured to
	char VoiceOverride[128]; ///< You can use this to force the TTS processor to use a different voice than the one it's configured to
};

/** \struct TTS_INTERFACE
 * On Windows, allowed samplerates for WAV files are: 8,11,12,16,22,24,32,44,48 with channels = 1 or 2.
 * Any other combination will result in a fallback of 22kHz Mono
 */
struct TTS_INTERFACE {
	VOICE_OUTPUT_FORMATS (*GetPreferredOutputFormat)(VOICE_ENGINES Engine);
	bool (*MakeMP3FromText)(TTS_JOB * job);
	bool (*MakeWAVFromText)(TTS_JOB * job);
	bool (*MakeMP3FromFile)(TTS_JOB * job);
	bool (*MakeWAVFromFile)(TTS_JOB * job);
};

/**@}*/
