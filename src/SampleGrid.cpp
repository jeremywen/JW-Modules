#include "JWModules.hpp"
#include <vector>
#include <string>
#include <fstream>
#include <cstdint>
#include <algorithm>
#include <unordered_map>
#include <cmath>
#include <cstring>
#include "osdialog.h"
#include "system.hpp"
#include <dirent.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#endif

struct SampleGrid : Module {
	enum ParamIds {
		RUN_PARAM,
		CLOCK_PARAM,
		RESET_PARAM,
		RIGHT_MOVE_BTN_PARAM,
		LEFT_MOVE_BTN_PARAM,
		DOWN_MOVE_BTN_PARAM,
		UP_MOVE_BTN_PARAM,
		RND_MOVE_BTN_PARAM,
		REP_MOVE_BTN_PARAM,
		CELL_GATE_PARAM,
		RND_SAMPLES_PARAM,
		SHUFFLE_SAMPLES_PARAM,
		SPLIT_SAMPLE_PARAM,
		REVERSE_RANDOM_PARAM,
		RND_MUTES_PARAM,
		OUTPUT_GAIN_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		EXT_CLOCK_INPUT,
		RESET_INPUT,
		RIGHT_INPUT, LEFT_INPUT, DOWN_INPUT, UP_INPUT,
		REPEAT_INPUT,
		RND_DIR_INPUT,
		VOLT_MAX_INPUT,
		VOCT_INPUT,
		GATE_INPUT,
		SHUFFLE_INPUT,
		REVERSE_RND_INPUT,
		RND_MUTES_INPUT,
		RND_SAMPLES_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		AUDIO_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		RUNNING_LIGHT,
		RESET_LIGHT,
		NUM_LIGHTS
	};

	dsp::SchmittTrigger rightTrigger;
	dsp::SchmittTrigger leftTrigger;
	dsp::SchmittTrigger downTrigger;
	dsp::SchmittTrigger upTrigger;

	dsp::SchmittTrigger repeatTrigger;
	dsp::SchmittTrigger rndPosTrigger;

	dsp::SchmittTrigger runningTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::SchmittTrigger rndNotesTrigger;
	dsp::SchmittTrigger rndGatesTrigger;
	dsp::SchmittTrigger rndProbsTrigger;
	dsp::SchmittTrigger gateTriggers[16];

	dsp::SchmittTrigger gateInTrigger;
	dsp::SchmittTrigger rndSamplesInTrigger;
	dsp::SchmittTrigger shuffleInTrigger;
	dsp::SchmittTrigger reverseRandomInTrigger;
	dsp::SchmittTrigger rndMutesInTrigger;

	// Button params handled as triggers in process()
	dsp::SchmittTrigger rndSamplesParamTrigger;
	dsp::SchmittTrigger shuffleSamplesParamTrigger;
	dsp::SchmittTrigger splitSampleParamTrigger;
	dsp::SchmittTrigger reverseRandomParamTrigger;
	dsp::SchmittTrigger rndMutesParamTrigger;

	int index = 0;
	int indexYX = 0;
	int posX = 0;
	int posY = 0;
	float phase = 0.0;
	float noteParamMax = 10.0;
	bool gateState[16] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
	bool running = true;
	bool ignoreGateOnPitchOut = false;
	bool resetMode = false;
	float rndFloat0to1AtClockStep = random::uniform();

	// Snake direction state for snake patterns
	bool snakeRight = true;
	bool snakeDown = true;

	enum GateMode { TRIGGER, RETRIGGER, CONTINUOUS };
	GateMode gateMode = TRIGGER;

	// René-like pattern modes
	enum PatternMode { ROW_MAJOR, COL_MAJOR, SNAKE_ROWS, SNAKE_COLS, SNAKE_DIAG_DR, SNAKE_DIAG_UR, ROW_WRAP_STAY };
	PatternMode patternMode = ROW_MAJOR; // default: row-major

	dsp::PulseGenerator gatePulse;

	// Gate pulse length in seconds (for TRIGGER/RETRIGGER modes)
	float gatePulseLenSec = 0.005f;

	// Audio fades
	bool fadesEnabled = true;
	float fadeLenSec = 0.005f;

	// Playback state for per-cell samples
	int playingCell = -1;
	double playbackPos = 0.0;
	double playbackStep = 1.0; // bufferRate / hostRate
	double playbackStartPos = 0.0; // position where current playback started

	// Per-cell sample storage (mono for display)
	std::vector<float> cellSamples[16];
	std::string cellSamplePath[16];
	int cellSampleRate[16] = {0};
	// Per-cell normalized start positions (0..1)
	float cellStartFrac[16] = {0.f};
	// Per-cell reversed state
	bool cellReversed[16] = { false };
	// Slice metadata per cell (when representing a slice of a larger file)
	bool cellIsSlice[16] = { false };
	float cellSliceStartFrac[16] = { 0.f };
	float cellSliceEndFrac[16] = { 1.f };

	// Selected directory for random sample loading
	std::string sampleDir;

	// Track whether we loaded audio from patch storage so JSON path loading can be skipped
	bool loadedFromPatchStorage = false;

	// UI-requested actions (performed on audio thread in process())
	bool reqShuffleSamples = false;
	bool reqReverseRandom = false;
	bool reqRandomizeMutes = false;
	bool reqSplitSampleInteractive = false;
	bool reqRandomSamplesInteractive = false;
	// Per-cell UI requests to avoid UI-thread mutation races
	bool reqClearCell[16] = {false};
	bool reqLoadRandomCell[16] = {false};
	bool reqLoadCellFromPath[16] = {false};
	std::string pendingCellPath[16];

	// Write a mono PCM16 WAV file
	static bool writeMonoWav(const std::string &path, const std::vector<float> &mono, int sRate) {
		if (mono.empty()) return false;
		uint16_t numChannels = 1;
		uint16_t bitsPerSample = 16;
		uint32_t sampleRate = (sRate > 0) ? (uint32_t)sRate : 44100u;
		uint16_t bytesPerSample = bitsPerSample / 8; // 2
		uint16_t blockAlign = numChannels * bytesPerSample; // 2
		uint32_t byteRate = sampleRate * blockAlign;
		uint32_t dataSize = (uint32_t)(mono.size() * blockAlign);
		std::ofstream out(path, std::ios::binary);
		if (!out.good()) return false;
		auto wU32 = [&](uint32_t v){ char b[4]; b[0]=(char)(v & 0xFF); b[1]=(char)((v>>8)&0xFF); b[2]=(char)((v>>16)&0xFF); b[3]=(char)((v>>24)&0xFF); out.write(b,4); };
		auto wU16 = [&](uint16_t v){ char b[2]; b[0]=(char)(v & 0xFF); b[1]=(char)((v>>8)&0xFF); out.write(b,2); };
		// RIFF
		out.write("RIFF", 4);
		wU32(36u + dataSize);
		out.write("WAVE", 4);
		// fmt
		out.write("fmt ", 4);
		wU32(16u);
		wU16(1u); // PCM
		wU16(numChannels);
		wU32(sampleRate);
		wU32(byteRate);
		wU16(blockAlign);
		wU16(bitsPerSample);
		// data
		out.write("data", 4);
		wU32(dataSize);
		for (size_t i = 0; i < mono.size(); ++i) {
			float f = std::max(-1.f, std::min(1.f, mono[i]));
			int s = (int)std::lround(f * 32767.0f);
			if (s < -32768) s = -32768; if (s > 32767) s = 32767;
			wU16((uint16_t)(s & 0xFFFF));
		}
		out.flush();
		bool ok = out.good();
		out.close();
		return ok;
	}

	void onAdd(const AddEvent& e) override {
		Module::onAdd(e);
		loadedFromPatchStorage = false;
		std::string dir = getPatchStorageDirectory();
		if (!dir.empty()) {
			int loadedCount = 0;
			for (int i = 0; i < 16; ++i) {
				char name[32]; snprintf(name, sizeof(name), "cell_%02d.wav", i);
				std::string p = rack::system::join(dir, std::string(name));
				std::ifstream f(p, std::ios::binary);
				if (f.good()) { f.close(); if (loadCellSample(i, p)) loadedCount++; }
			}
			if (loadedCount > 0) loadedFromPatchStorage = true;
		}
	}

	void onSave(const SaveEvent& e) override {
		Module::onSave(e);
		std::string dir = createPatchStorageDirectory();
		if (dir.empty()) return;
		for (int i = 0; i < 16; ++i) {
			char name[32]; snprintf(name, sizeof(name), "cell_%02d.wav", i);
			std::string p = rack::system::join(dir, std::string(name));
			if (cellSamples[i].empty()) {
				// Ensure unloaded cells don't reload: remove any existing patch-storage file
				rack::system::remove(p);
			}
			else {
				int sr = (cellSampleRate[i] > 0) ? cellSampleRate[i] : 44100;
				writeMonoWav(p, cellSamples[i], sr);
			}
		}
	}

	// Collect all WAV file paths in a directory (case-insensitive .wav)
	bool collectWavsInDir(const std::string &dir, std::vector<std::string> &wavs) {
		wavs.clear();
#ifdef _WIN32
		std::string pattern = dir;
		if (!pattern.empty()) {
			char last = pattern.back();
			if (last != '/' && last != '\\') pattern += '\\';
		}
		pattern += "*.wav";
		WIN32_FIND_DATAA ffd; HANDLE hFind = FindFirstFileA(pattern.c_str(), &ffd);
		if (hFind == INVALID_HANDLE_VALUE) return false;
		do {
			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
			std::string name = ffd.cFileName;
			std::string full = dir;
			char last = full.empty() ? '\\' : full.back();
			if (last != '/' && last != '\\') full += '\\';
			full += name; wavs.push_back(full);
		} while (FindNextFileA(hFind, &ffd));
		FindClose(hFind);
#else
		DIR *dp = opendir(dir.c_str()); if (!dp) return false;
		struct dirent *de;
		while ((de = readdir(dp)) != nullptr) {
			if (de->d_name[0] == '.') continue;
			std::string name = de->d_name;
			std::string full = dir + "/" + name;
			bool isFile = false;
#ifdef DT_REG
			if (de->d_type == DT_REG) isFile = true;
			else
#endif
			{
				struct stat st; if (stat(full.c_str(), &st) == 0 && S_ISREG(st.st_mode)) isFile = true;
			}
			if (!isFile) continue;
			size_t dot = name.find_last_of('.');
			if (dot != std::string::npos) {
				std::string ext = name.substr(dot);
				std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
				if (ext == ".wav") wavs.push_back(full);
			}
		}
		closedir(dp);
#endif
		return !wavs.empty();
	}

	// Load up to 16 random wavs from directory; returns true if at least one loaded
	bool loadRandomSamplesFromDir(const std::string &dir) {
		std::vector<std::string> wavs; if (!collectWavsInDir(dir, wavs)) return false;
		for (int i = 0; i < 16; ++i) {
			if (wavs.empty()) break;
			size_t idx = (size_t)std::floor(random::uniform() * wavs.size());
			loadCellSample(i, wavs[idx]);
		}
		return true;
	}

	// Interactive: set dir if needed, then load random samples
	bool loadRandomSamplesInteractive() {
		std::string dir = sampleDir;
		if (dir.empty()) {
			char *path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, NULL);
			if (!path) return false;
			std::string p = path; free(path);
			struct stat st; if (stat(p.c_str(), &st) == 0) {
				if (S_ISDIR(st.st_mode)) dir = p;
				else {
					size_t q = p.find_last_of("/\\"); dir = (q == std::string::npos) ? std::string(".") : p.substr(0, q);
				}
			}
			sampleDir = dir;
		}
		return loadRandomSamplesFromDir(dir);
	}

	// Choose and set the directory used for random sample loading
	bool chooseSampleDir() {
		char *path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, NULL);
		if (!path) return false;
		std::string p = path; free(path);
		std::string dir;
		struct stat st; if (stat(p.c_str(), &st) == 0) {
			if (S_ISDIR(st.st_mode)) dir = p;
			else {
				size_t q = p.find_last_of("/\\"); dir = (q == std::string::npos) ? std::string(".") : p.substr(0, q);
			}
		}
		if (!dir.empty()) { sampleDir = dir; return true; }
		return false;
	}

	// Pick a single random WAV path from sampleDir, prompting to choose a directory if unset
	bool pickRandomWavPath(std::string &outPath) {
		std::string dir = sampleDir;
		if (dir.empty()) {
			char *path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, NULL);
			if (!path) return false;
			std::string p = path; free(path);
			struct stat st; if (stat(p.c_str(), &st) == 0) {
				if (S_ISDIR(st.st_mode)) dir = p;
				else {
					size_t q = p.find_last_of("/\\"); dir = (q == std::string::npos) ? std::string(".") : p.substr(0, q);
				}
			}
			sampleDir = dir;
		}
		std::vector<std::string> wavs; if (!collectWavsInDir(dir, wavs)) return false;
		size_t idx = (size_t)std::floor(random::uniform() * wavs.size());
		outPath = wavs[idx];
		return true;
	}

	// Load a random sample into a specific cell
	bool loadRandomSampleForCell(int idx) {
		std::string p; if (!pickRandomWavPath(p)) return false;
		return loadCellSample(idx, p);
	}

	// Load full mono WAV buffer (like loadCellSample but returning buffer and rate)
	static bool loadWavMonoToBuffer(const std::string &path, std::vector<float> &monoOut, int &sRateOut) {
		monoOut.clear(); sRateOut = 0;
		std::ifstream in(path, std::ios::binary);
		if (!in.good()) return false;
		char riff[4]; in.read(riff, 4);
		if (in.gcount() != 4 || std::string(riff, 4) != "RIFF") return false;
		(void)readU32(in);
		char wave[4]; in.read(wave, 4);
		if (in.gcount() != 4 || std::string(wave, 4) != "WAVE") return false;
		uint16_t audioFormat = 1; // 1=PCM, 3=FLOAT
		uint16_t numChannels = 1;
		uint32_t sRate = 44100;
		uint16_t bitsPerSample = 16;
		uint32_t dataSize = 0;
		std::streampos dataPos = 0;
		while (in.good() && !in.eof()) {
			char id[4]; in.read(id, 4); if (in.gcount() != 4) break;
			uint32_t chunkSize = readU32(in);
			std::string cid(id, 4);
			if (cid == "fmt ") {
				std::streampos start = in.tellg();
				audioFormat = readU16(in);
				numChannels = readU16(in);
				sRate = readU32(in);
				(void)readU32(in);
				(void)readU16(in);
				bitsPerSample = readU16(in);
				in.seekg(start + (std::streamoff)chunkSize);
			}
			else if (cid == "data") {
				dataPos = in.tellg();
				dataSize = chunkSize;
				in.seekg((std::streamoff)dataPos + (std::streamoff)dataSize);
			}
			else {
				in.seekg((std::streamoff)in.tellg() + (std::streamoff)chunkSize);
			}
		}
		if (dataSize == 0 || dataPos == 0) return false;
		in.clear(); in.seekg(dataPos);
		std::vector<float> mono; mono.reserve(dataSize / (bitsPerSample/8));
		if (audioFormat == 1 && bitsPerSample == 16) {
			const size_t frames = dataSize / (numChannels * 2);
			for (size_t i = 0; i < frames; ++i) {
				if (numChannels == 1) { int16_t s16 = (int16_t)readU16(in); mono.push_back((float)s16 / 32768.f); }
				else { int16_t l16 = (int16_t)readU16(in); int16_t r16 = (int16_t)readU16(in); mono.push_back(((float)l16 + (float)r16) / (2.f * 32768.f)); for (int ch = 2; ch < (int)numChannels; ++ch) { (void)readU16(in); } }
			}
		}
		else if (audioFormat == 1 && bitsPerSample == 24) {
			const size_t frames = dataSize / (numChannels * 3);
			for (size_t i = 0; i < frames; ++i) {
				auto read24 = [&in]() { uint8_t b[3]; in.read((char*)b, 3); int32_t v = (int32_t)(b[0] | (b[1] << 8) | (b[2] << 16)); if (v & 0x800000) v |= 0xFF000000; return (float)v / 8388608.f; };
				if (numChannels == 1) { mono.push_back(read24()); }
				else { float vl = read24(); float vr = read24(); mono.push_back(0.5f * (vl + vr)); for (int ch = 2; ch < (int)numChannels; ++ch) { (void)read24(); } }
			}
		}
		else if (audioFormat == 1 && bitsPerSample == 32) {
			const size_t frames = dataSize / (numChannels * 4);
			for (size_t i = 0; i < frames; ++i) {
				auto read32 = [&in]() { uint32_t u = readU32(in); int32_t s = (int32_t)u; return (float)s / 2147483648.f; };
				if (numChannels == 1) { mono.push_back(read32()); }
				else { float vl = read32(); float vr = read32(); mono.push_back(0.5f * (vl + vr)); for (int ch = 2; ch < (int)numChannels; ++ch) { (void)read32(); } }
			}
		}
		else if (audioFormat == 3 && bitsPerSample == 32) {
			const size_t frames = dataSize / (numChannels * 4);
			for (size_t i = 0; i < frames; ++i) {
				auto readf = [&in]() { float sf; in.read((char*)&sf, 4); return sf; };
				if (numChannels == 1) { mono.push_back(readf()); }
				else { float vl = readf(); float vr = readf(); mono.push_back(0.5f * (vl + vr)); for (int ch = 2; ch < (int)numChannels; ++ch) { (void)readf(); } }
			}
		}
		else { return false; }
		if (mono.empty()) return false;
		float maxAbs = 0.f; for (float v : mono) maxAbs = std::max(maxAbs, std::abs(v));
		if (maxAbs > 0.f) { float g = 1.f / maxAbs; for (float &v : mono) v *= g; }
		monoOut = std::move(mono);
		sRateOut = (int)sRate;
		return true;
	}

	bool loadSplitSampleAcrossCells(const std::string &path) {
		std::vector<float> mono; int sRate = 0;
		if (!loadWavMonoToBuffer(path, mono, sRate)) return false;
		if (mono.empty()) return false;
		size_t N = mono.size();
		size_t sliceLen = std::max((size_t)1, N / (size_t)16);
		for (int i = 0; i < 16; ++i) {
			size_t start = (size_t)i * sliceLen;
			size_t end = (i == 15) ? N : std::min(start + sliceLen, N);
			if (start >= N) { cellSamples[i].clear(); cellSampleRate[i] = sRate; cellSamplePath[i] = path; continue; }
			std::vector<float> slice; slice.reserve(end - start);
			for (size_t k = start; k < end; ++k) slice.push_back(mono[k]);
			cellSamples[i] = std::move(slice);
			cellSampleRate[i] = sRate;
			cellSamplePath[i] = path;
			cellIsSlice[i] = true;
			cellSliceStartFrac[i] = N > 0 ? (float)((double)start / (double)N) : 0.f;
			cellSliceEndFrac[i] = N > 0 ? (float)((double)end / (double)N) : 1.f;
			cellReversed[i] = false;
		}
		return true;
	}

	bool loadSplitSampleInteractive() {
		osdialog_filters *filters = osdialog_filters_parse("WAV:wav");
		char *path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, filters);
		osdialog_filters_free(filters);
		if (!path) return false; std::string p = path; free(path);
		return loadSplitSampleAcrossCells(p);
	}

	void shuffleSamples() {
		int n = 16;
		std::vector<int> order(n);
		for (int i = 0; i < n; ++i) order[i] = i;
		for (int i = n - 1; i > 0; --i) {
			int j = (int)std::floor(random::uniform() * (i + 1));
			std::swap(order[i], order[j]);
		}
		std::vector<float> tmpSamples[16];
		std::string tmpPaths[16];
		int tmpRates[16];
		bool tmpIsSlice[16];
		float tmpSliceStart[16];
		float tmpSliceEnd[16];
		bool tmpRev[16];
		for (int i = 0; i < n; ++i) {
			tmpSamples[i] = cellSamples[order[i]];
			tmpPaths[i] = cellSamplePath[order[i]];
			tmpRates[i] = cellSampleRate[order[i]];
			tmpIsSlice[i] = cellIsSlice[order[i]];
			tmpSliceStart[i] = cellSliceStartFrac[order[i]];
			tmpSliceEnd[i] = cellSliceEndFrac[order[i]];
			tmpRev[i] = cellReversed[order[i]];
		}
		for (int i = 0; i < n; ++i) {
			cellSamples[i] = std::move(tmpSamples[i]);
			cellSamplePath[i] = std::move(tmpPaths[i]);
			cellSampleRate[i] = tmpRates[i];
			cellIsSlice[i] = tmpIsSlice[i];
			cellSliceStartFrac[i] = tmpSliceStart[i];
			cellSliceEndFrac[i] = tmpSliceEnd[i];
			cellReversed[i] = tmpRev[i];
		}
	}

	void randomReverseSamples() {
		for (int i = 0; i < 16; ++i) {
			if (cellSamples[i].empty()) continue;
			bool newRev = random::uniform() > 0.5f;
			if (newRev != cellReversed[i]) {
				std::reverse(cellSamples[i].begin(), cellSamples[i].end());
			}
			cellReversed[i] = newRev;
		}
	}

	// Minimal WAV loader for per-cell samples (mono: stereo averaged)
	static uint32_t readU32(std::ifstream &in) {
		uint8_t b[4];
		in.read((char*)b, 4);
		return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
	}
	static uint16_t readU16(std::ifstream &in) {
		uint8_t b[2];
		in.read((char*)b, 2);
		return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
	}

	bool loadCellSample(int idx, const std::string &path) {
		if (idx < 0 || idx >= 16) return false;
		std::ifstream in(path, std::ios::binary);
		if (!in.good()) return false;
		char riff[4]; in.read(riff, 4);
		if (in.gcount() != 4 || std::string(riff, 4) != "RIFF") return false;
		(void)readU32(in);
		char wave[4]; in.read(wave, 4);
		if (in.gcount() != 4 || std::string(wave, 4) != "WAVE") return false;
		uint16_t audioFormat = 1; // 1=PCM, 3=FLOAT
		uint16_t numChannels = 1;
		uint32_t sRate = 44100;
		uint16_t bitsPerSample = 16;
		uint32_t dataSize = 0;
		std::streampos dataPos = 0;
		while (in.good() && !in.eof()) {
			char id[4]; in.read(id, 4); if (in.gcount() != 4) break;
			uint32_t chunkSize = readU32(in);
			std::string cid(id, 4);
			if (cid == "fmt ") {
				std::streampos start = in.tellg();
				audioFormat = readU16(in);
				numChannels = readU16(in);
				sRate = readU32(in);
				(void)readU32(in);
				(void)readU16(in);
				bitsPerSample = readU16(in);
				in.seekg(start + (std::streamoff)chunkSize);
			}
			else if (cid == "data") {
				dataPos = in.tellg();
				dataSize = chunkSize;
				in.seekg((std::streamoff)dataPos + (std::streamoff)dataSize);
			}
			else {
				in.seekg((std::streamoff)in.tellg() + (std::streamoff)chunkSize);
			}
		}
		if (dataSize == 0 || dataPos == 0) return false;
		in.clear(); in.seekg(dataPos);
		std::vector<float> mono; mono.reserve(dataSize / (bitsPerSample/8));
		if (audioFormat == 1 && bitsPerSample == 16) {
			const size_t frames = dataSize / (numChannels * 2);
			for (size_t i = 0; i < frames; ++i) {
				if (numChannels == 1) {
					int16_t s16 = (int16_t)readU16(in);
					mono.push_back((float)s16 / 32768.f);
				}
				else {
					int16_t l16 = (int16_t)readU16(in);
					int16_t r16 = (int16_t)readU16(in);
					mono.push_back(((float)l16 + (float)r16) / (2.f * 32768.f));
					for (int ch = 2; ch < (int)numChannels; ++ch) { (void)readU16(in); }
				}
			}
		}
		else if (audioFormat == 1 && bitsPerSample == 24) {
			const size_t frames = dataSize / (numChannels * 3);
			for (size_t i = 0; i < frames; ++i) {
				auto read24 = [&in]() {
					uint8_t b[3]; in.read((char*)b, 3);
					int32_t v = (int32_t)(b[0] | (b[1] << 8) | (b[2] << 16));
					if (v & 0x800000) v |= 0xFF000000;
					return (float)v / 8388608.f;
				};
				if (numChannels == 1) { mono.push_back(read24()); }
				else {
					float vl = read24(); float vr = read24(); mono.push_back(0.5f * (vl + vr));
					for (int ch = 2; ch < (int)numChannels; ++ch) { (void)read24(); }
				}
			}
		}
		else if (audioFormat == 1 && bitsPerSample == 32) {
			const size_t frames = dataSize / (numChannels * 4);
			for (size_t i = 0; i < frames; ++i) {
				auto read32 = [&in]() {
					uint32_t u = readU32(in); int32_t s = (int32_t)u; return (float)s / 2147483648.f;
				};
				if (numChannels == 1) { mono.push_back(read32()); }
				else {
					float vl = read32(); float vr = read32(); mono.push_back(0.5f * (vl + vr));
					for (int ch = 2; ch < (int)numChannels; ++ch) { (void)read32(); }
				}
			}
		}
		else if (audioFormat == 3 && bitsPerSample == 32) {
			const size_t frames = dataSize / (numChannels * 4);
			for (size_t i = 0; i < frames; ++i) {
				auto readf = [&in]() { float sf; in.read((char*)&sf, 4); return sf; };
				if (numChannels == 1) { mono.push_back(readf()); }
				else {
					float vl = readf(); float vr = readf(); mono.push_back(0.5f * (vl + vr));
					for (int ch = 2; ch < (int)numChannels; ++ch) { (void)readf(); }
				}
			}
		}
		else { return false; }
		if (mono.empty()) return false;
		// Normalize softly
		float maxAbs = 0.f; for (float v : mono) maxAbs = std::max(maxAbs, std::abs(v));
		if (maxAbs > 0.f) { float g = 1.f / maxAbs; for (float &v : mono) v *= g; }
		cellSamples[idx] = std::move(mono);
		cellSampleRate[idx] = (int)sRate;
		cellSamplePath[idx] = path;
		cellIsSlice[idx] = false;
		cellSliceStartFrac[idx] = 0.f;
		cellSliceEndFrac[idx] = 1.f;
		cellReversed[idx] = false;
		return true;
	}

	SampleGrid() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(RUN_PARAM, 0.0, 1.0, 0.0, "Run");
		configParam(RESET_PARAM, 0.0, 1.0, 0.0, "Reset");
		configParam(RIGHT_MOVE_BTN_PARAM, 0.0, 1.0, 0.0, "Click to Move Right");
		configParam(LEFT_MOVE_BTN_PARAM, 0.0, 1.0, 0.0, "Click to Move Left");
		configParam(DOWN_MOVE_BTN_PARAM, 0.0, 1.0, 0.0, "Click to Move Down");
		configParam(UP_MOVE_BTN_PARAM, 0.0, 1.0, 0.0, "Click to Move Up");
		configParam(RND_MOVE_BTN_PARAM, 0.0, 1.0, 0.0, "Click to Move Random");
		configParam(REP_MOVE_BTN_PARAM, 0.0, 1.0, 0.0, "Click to Repeat");
		configButton(RND_SAMPLES_PARAM, "Load 16 random WAVs");
		configButton(SHUFFLE_SAMPLES_PARAM, "Shuffle samples");
		configButton(SPLIT_SAMPLE_PARAM, "Slice WAV into 16");
		configButton(REVERSE_RANDOM_PARAM, "Randomly reverse samples");
		configButton(RND_MUTES_PARAM, "Randomize mutes");
		configParam(OUTPUT_GAIN_PARAM, 0.0, 2.0, 1.0, "Output Gain");
		configInput(RIGHT_INPUT, "Right");
		configInput(LEFT_INPUT, "Left");
		configInput(DOWN_INPUT, "Down");
		configInput(UP_INPUT, "Up");
		configInput(RND_DIR_INPUT, "Random");
		configInput(RND_SAMPLES_INPUT, "Load Random Samples Trigger");
		configInput(REPEAT_INPUT, "Repeat");
		configInput(RESET_INPUT, "Reset");
		configInput(VOLT_MAX_INPUT, "Range");
		configInput(VOCT_INPUT, "V/Oct Select");
		configInput(GATE_INPUT, "Gate In");
		configInput(SHUFFLE_INPUT, "Shuffle Trigger");
		configInput(REVERSE_RND_INPUT, "Reverse Random Trigger");
		configInput(RND_MUTES_INPUT, "Randomize Mutes Trigger");
		configOutput(AUDIO_OUTPUT, "Audio");

		// Ensure module starts with no loaded samples
		for (int i = 0; i < 16; ++i) {
			cellSamples[i].clear();
			cellSamplePath[i].clear();
			cellSampleRate[i] = 0;
			cellStartFrac[i] = 0.f;
			cellIsSlice[i] = false;
			cellSliceStartFrac[i] = 0.f;
			cellSliceEndFrac[i] = 1.f;
			cellReversed[i] = false;
		}
		playingCell = -1;
	}

	void process(const ProcessArgs &args) override;

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "running", json_boolean(running));
		json_object_set_new(rootJ, "ignoreGateOnPitchOut", json_boolean(ignoreGateOnPitchOut));

		// gates
		json_t *gatesJ = json_array();
		for (int i = 0; i < 16; i++) {
			json_t *gateJ = json_integer((int) gateState[i]);
			json_array_append_new(gatesJ, gateJ);
		}
		json_object_set_new(rootJ, "gates", gatesJ);

		// gateMode
		json_t *gateModeJ = json_integer((int) gateMode);
		json_object_set_new(rootJ, "gateMode", gateModeJ);

		// gate pulse length
		json_object_set_new(rootJ, "gatePulseLenSec", json_real(gatePulseLenSec));

		// pattern mode
		json_object_set_new(rootJ, "patternMode", json_integer((int)patternMode));

		// Audio fades settings
		json_object_set_new(rootJ, "fadesEnabled", json_boolean(fadesEnabled));
		json_object_set_new(rootJ, "fadeLenSec", json_real(fadeLenSec));

		// Per-cell sample paths
		json_t *pathsJ = json_array();
		for (int i = 0; i < 16; ++i) {
			const char *p = cellSamplePath[i].empty() ? NULL : cellSamplePath[i].c_str();
			json_array_append_new(pathsJ, p ? json_string(p) : json_string(""));
		}
		json_object_set_new(rootJ, "cellSamplePaths", pathsJ);

		// Per-cell start positions (normalized 0..1)
		json_t *startsJ = json_array();
		for (int i = 0; i < 16; ++i) {
			json_array_append_new(startsJ, json_real((double)cellStartFrac[i]));
		}
		json_object_set_new(rootJ, "cellStartFrac", startsJ);

		// Slice metadata
		json_t *isSliceJ = json_array();
		json_t *sliceStartFracJ = json_array();
		json_t *sliceEndFracJ = json_array();
		for (int i = 0; i < 16; ++i) {
			json_array_append_new(isSliceJ, json_integer(cellIsSlice[i] ? 1 : 0));
			json_array_append_new(sliceStartFracJ, json_real((double)cellSliceStartFrac[i]));
			json_array_append_new(sliceEndFracJ, json_real((double)cellSliceEndFrac[i]));
		}
		json_object_set_new(rootJ, "cellIsSlice", isSliceJ);
		json_object_set_new(rootJ, "cellSliceStartFrac", sliceStartFracJ);
		json_object_set_new(rootJ, "cellSliceEndFrac", sliceEndFracJ);

		// Per-cell reversed flags
		json_t *revJ = json_array();
		for (int i = 0; i < 16; ++i) {
			json_array_append_new(revJ, json_integer(cellReversed[i] ? 1 : 0));
		}
		json_object_set_new(rootJ, "cellReversed", revJ);

		// Selected directory for random samples
		json_object_set_new(rootJ, "sampleDir", json_string(sampleDir.c_str()));

		// snake state (not critical to persist; omit for simplicity)

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);

		json_t *ignoreGateOnPitchOutJ = json_object_get(rootJ, "ignoreGateOnPitchOut");
		if (ignoreGateOnPitchOutJ)
			ignoreGateOnPitchOut = json_is_true(ignoreGateOnPitchOutJ);

		// gates
		json_t *gatesJ = json_object_get(rootJ, "gates");
		if (gatesJ) {
			for (int i = 0; i < 16; i++) {
				json_t *gateJ = json_array_get(gatesJ, i);
				if (gateJ)
					gateState[i] = !!json_integer_value(gateJ);
			}
		}

		// gateMode
		json_t *gateModeJ = json_object_get(rootJ, "gateMode");
		if (gateModeJ)
			gateMode = (GateMode)json_integer_value(gateModeJ);

		// gate pulse length
		json_t *gatePulseLenSecJ = json_object_get(rootJ, "gatePulseLenSec");
		if (gatePulseLenSecJ) {
			gatePulseLenSec = (float) json_number_value(gatePulseLenSecJ);
			gatePulseLenSec = clampfjw(gatePulseLenSec, 0.001f, 10.0f);
		}

		// pattern mode
		json_t *patternModeJ = json_object_get(rootJ, "patternMode");
		if (patternModeJ) patternMode = (PatternMode) json_integer_value(patternModeJ);

		// Audio fades settings
		json_t *fadesEnabledJ = json_object_get(rootJ, "fadesEnabled");
		if (fadesEnabledJ) fadesEnabled = json_is_true(fadesEnabledJ);
		json_t *fadeLenSecJ = json_object_get(rootJ, "fadeLenSec");
		if (fadeLenSecJ) {
			fadeLenSec = (float) json_number_value(fadeLenSecJ);
			fadeLenSec = clampfjw(fadeLenSec, 0.0f, 1.0f);
		}

		// Slice metadata load
		for (int i = 0; i < 16; ++i) { cellIsSlice[i] = false; cellSliceStartFrac[i] = 0.f; cellSliceEndFrac[i] = 1.f; }
		json_t *isSliceJ = json_object_get(rootJ, "cellIsSlice");
		json_t *sliceStartFracJ = json_object_get(rootJ, "cellSliceStartFrac");
		json_t *sliceEndFracJ = json_object_get(rootJ, "cellSliceEndFrac");
		if (isSliceJ && json_is_array(isSliceJ)) {
			for (int i = 0; i < 16; ++i) {
				json_t *vJ = json_array_get(isSliceJ, i);
				if (vJ && (json_is_integer(vJ) || json_is_real(vJ))) cellIsSlice[i] = !!json_integer_value(vJ);
			}
		}
		if (sliceStartFracJ && json_is_array(sliceStartFracJ)) {
			for (int i = 0; i < 16; ++i) {
				json_t *vJ = json_array_get(sliceStartFracJ, i);
				if (vJ && (json_is_integer(vJ) || json_is_real(vJ))) {
					cellSliceStartFrac[i] = (float) json_number_value(vJ);
					cellSliceStartFrac[i] = clampfjw(cellSliceStartFrac[i], 0.f, 1.f);
				}
			}
		}
		if (sliceEndFracJ && json_is_array(sliceEndFracJ)) {
			for (int i = 0; i < 16; ++i) {
				json_t *vJ = json_array_get(sliceEndFracJ, i);
				if (vJ && (json_is_integer(vJ) || json_is_real(vJ))) {
					cellSliceEndFrac[i] = (float) json_number_value(vJ);
					cellSliceEndFrac[i] = clampfjw(cellSliceEndFrac[i], 0.f, 1.f);
				}
			}
		}

		// Per-cell reversed flags
		for (int i = 0; i < 16; ++i) cellReversed[i] = false;
		json_t *revJ = json_object_get(rootJ, "cellReversed");
		if (revJ && json_is_array(revJ)) {
			for (int i = 0; i < 16; ++i) {
				json_t *vJ = json_array_get(revJ, i);
				if (vJ && (json_is_integer(vJ) || json_is_real(vJ))) cellReversed[i] = !!json_integer_value(vJ);
			}
		}

		// Per-cell sample paths load, reconstructing slices when flagged
		// If we already loaded from patch storage in onAdd(), skip loading from external paths to let patch storage win.
		json_t *pathsJ = json_object_get(rootJ, "cellSamplePaths");
		if (!loadedFromPatchStorage && pathsJ && json_is_array(pathsJ)) {
			std::unordered_map<std::string, std::pair<std::vector<float>, int>> wavMonoCache;
			for (int i = 0; i < 16; ++i) {
				json_t *sJ = json_array_get(pathsJ, i);
				std::string p = (sJ && json_is_string(sJ)) ? std::string(json_string_value(sJ)) : std::string();
				cellSamplePath[i].clear();
				cellSamples[i].clear();
				cellSampleRate[i] = 0;
				cellIsSlice[i] = cellIsSlice[i]; // keep as loaded
				if (!p.empty()) {
					cellSamplePath[i] = p;
					if (cellIsSlice[i]) {
						// Cache full mono buffer per path
						auto it = wavMonoCache.find(p);
						if (it == wavMonoCache.end()) {
							std::vector<float> mono; int sRate = 0;
							if (loadWavMonoToBuffer(p, mono, sRate)) {
								wavMonoCache.emplace(p, std::make_pair(std::move(mono), sRate));
							}
						}
						it = wavMonoCache.find(p);
						if (it != wavMonoCache.end()) {
							const std::vector<float> &mono = it->second.first;
							int sRate = it->second.second;
							size_t N = mono.size();
							size_t start = (size_t) std::floor((double)cellSliceStartFrac[i] * (double)N);
							size_t end = (size_t) std::floor((double)cellSliceEndFrac[i] * (double)N);
							if (end > N) end = N;
							if (start > end) start = end;
							std::vector<float> slice;
							if (start < end) { slice.assign(mono.begin() + start, mono.begin() + end); }
							cellSamples[i] = std::move(slice);
							cellSampleRate[i] = sRate;
							if (cellReversed[i] && !cellSamples[i].empty()) {
								std::reverse(cellSamples[i].begin(), cellSamples[i].end());
							}
						}
						else {
							// Fallback: load whole sample
							loadCellSample(i, p);
							if (cellReversed[i] && !cellSamples[i].empty()) {
								std::reverse(cellSamples[i].begin(), cellSamples[i].end());
							}
						}
					}
					else {
						loadCellSample(i, p);
						if (cellReversed[i] && !cellSamples[i].empty()) {
							std::reverse(cellSamples[i].begin(), cellSamples[i].end());
						}
					}
				}
			}
		}

		// Per-cell start positions load
		json_t *startsJ = json_object_get(rootJ, "cellStartFrac");
		if (startsJ && json_is_array(startsJ)) {
			for (int i = 0; i < 16; ++i) {
				json_t *vJ = json_array_get(startsJ, i);
				if (vJ && (json_is_real(vJ) || json_is_integer(vJ))) {
					cellStartFrac[i] = (float) json_number_value(vJ);
					if (cellStartFrac[i] < 0.f) cellStartFrac[i] = 0.f;
					if (cellStartFrac[i] > 1.f) cellStartFrac[i] = 1.f;
				}
			}
		}

		// Selected directory
		json_t *dirJ = json_object_get(rootJ, "sampleDir");
		if (dirJ && json_is_string(dirJ)) sampleDir = json_string_value(dirJ);

		// ignore legacy auto-wrap keys if present
	}

	void onReset() override {
		// Reset gate states
		for (int i = 0; i < 16; i++) {
			gateState[i] = true;
		}
		// Clear all loaded samples and related metadata on initialize
		for (int i = 0; i < 16; ++i) {
			cellSamples[i].clear();
			cellSamplePath[i].clear();
			cellSampleRate[i] = 0;
			cellStartFrac[i] = 0.f;
			cellIsSlice[i] = false;
			cellSliceStartFrac[i] = 0.f;
			cellSliceEndFrac[i] = 1.f;
			cellReversed[i] = false;
		}
		playingCell = -1;
	}

	void onRandomize() override {
		randomizeGateStates();
	}

	void randomizeGateStates() {
		for (int i = 0; i < 16; i++) {
			gateState[i] = (random::uniform() > 0.50);
		}
	}

	float getOneRandomNote(){
		return random::uniform() * noteParamMax;
	}

	void handleMoveRight(){
		if (patternMode == SNAKE_ROWS) {
			if (snakeRight) {
				if (posX == 3) { snakeRight = false; posY = (posY + 1) % 4; }
				else { posX++; }
			} else {
				if (posX == 0) { snakeRight = true; posY = (posY + 1) % 4; }
				else { posX--; }
			}
		} else if (patternMode == SNAKE_DIAG_DR) {
			int dx = snakeRight ? 1 : -1;
			int dy = snakeDown ? 1 : -1;
			int nx = posX + dx;
			int ny = posY + dy;
			if (nx < 0 || nx > 3) { snakeRight = !snakeRight; nx = clampijw(nx, 0, 3); }
			if (ny < 0 || ny > 3) { snakeDown = !snakeDown; ny = clampijw(ny, 0, 3); }
			posX = nx; posY = ny;
		} else if (patternMode == SNAKE_DIAG_UR) {
			int dx = snakeRight ? 1 : -1;
			int dy = snakeDown ? -1 : 1;
			int nx = posX + dx;
			int ny = posY + dy;
			if (nx < 0 || nx > 3) { snakeRight = !snakeRight; nx = clampijw(nx, 0, 3); }
			if (ny < 0 || ny > 3) { snakeDown = !snakeDown; ny = clampijw(ny, 0, 3); }
			posX = nx; posY = ny;
		} else {
			posX = (posX + 1) % 4;
			// ROW_MAJOR advances to next row on wrap; ROW_WRAP_STAY stays on the same row
			if (patternMode == ROW_MAJOR && posX == 0) posY = (posY + 1) % 4;
		}
	}
	void handleMoveLeft(){
		if (patternMode == SNAKE_ROWS) {
			// same as right; external left triggers reverse sense
			handleMoveRight();
		} else if (patternMode == SNAKE_DIAG_DR || patternMode == SNAKE_DIAG_UR) {
			snakeRight = !snakeRight;
			handleMoveRight();
		} else {
			posX = (posX + 3) % 4;
			// ROW_MAJOR moves to previous row on wrap; ROW_WRAP_STAY stays on the same row
			if (patternMode == ROW_MAJOR && posX == 3) posY = (posY + 3) % 4;
		}
	}
	void handleMoveDown(){
		if (patternMode == SNAKE_COLS) {
			if (snakeDown) {
				if (posY == 3) { snakeDown = false; posX = (posX + 1) % 4; }
				else { posY++; }
			} else {
				if (posY == 0) { snakeDown = true; posX = (posX + 1) % 4; }
				else { posY--; }
			}
		} else if (patternMode == SNAKE_DIAG_DR || patternMode == SNAKE_DIAG_UR) {
			// Down mirrors right in diagonal modes
			handleMoveRight();
		} else {
			posY = (posY + 1) % 4;
			if (patternMode == COL_MAJOR && posY == 0) posX = (posX + 1) % 4;
		}
	}
	void handleMoveUp(){
		if (patternMode == SNAKE_COLS) {
			// same as down; external up triggers reverse sense
			handleMoveDown();
		} else if (patternMode == SNAKE_DIAG_DR || patternMode == SNAKE_DIAG_UR) {
			// Up reverses diagonal sense
			snakeDown = !snakeDown;
			handleMoveRight();
		} else {
			posY = (posY + 3) % 4;
			if (patternMode == COL_MAJOR && posY == 3) posX = (posX + 3) % 4;
		}
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// STEP
///////////////////////////////////////////////////////////////////////////////////////////////////
void SampleGrid::process(const ProcessArgs &args) {
	const float lightLambda = 0.10;
	// Run
	if (runningTrigger.process(params[RUN_PARAM].getValue())) {
		running = !running;
	}
	lights[RUNNING_LIGHT].value = running ? 1.0 : 0.0;
	
	bool nextStep = false;
	if (resetTrigger.process(params[RESET_PARAM].getValue() + inputs[RESET_INPUT].getVoltage())) {
		resetMode = true;
	}

	if(running){

		if (repeatTrigger.process(inputs[REPEAT_INPUT].getVoltage() + params[REP_MOVE_BTN_PARAM].getValue())) {
			nextStep = true;
		} 

		if (rndPosTrigger.process(inputs[RND_DIR_INPUT].getVoltage() + params[RND_MOVE_BTN_PARAM].getValue())) {
			nextStep = true;
			switch(int(4 * random::uniform())){
				case 0:handleMoveRight();break;
				case 1:handleMoveLeft();break;
				case 2:handleMoveDown();break;
				case 3:handleMoveUp();break;
			}
		} 
		if (rightTrigger.process(inputs[RIGHT_INPUT].getVoltage() + params[RIGHT_MOVE_BTN_PARAM].getValue())) {
			nextStep = true;
			handleMoveRight();
		} 
		if (leftTrigger.process(inputs[LEFT_INPUT].getVoltage() + params[LEFT_MOVE_BTN_PARAM].getValue())) {
			nextStep = true;
			handleMoveLeft();
		} 
		if (downTrigger.process(inputs[DOWN_INPUT].getVoltage() + params[DOWN_MOVE_BTN_PARAM].getValue())) {
			nextStep = true;
			handleMoveDown();
		} 
		if (upTrigger.process(inputs[UP_INPUT].getVoltage() + params[UP_MOVE_BTN_PARAM].getValue())) {
			nextStep = true;
			handleMoveUp();
		}
	}

	// Bottom control trigger inputs
	if (rndSamplesInTrigger.process(inputs[RND_SAMPLES_INPUT].getVoltage())) {
		// Avoid UI dialogs from audio thread; only load if directory is set
		if (!sampleDir.empty()) {
			loadRandomSamplesFromDir(sampleDir);
		}
	}
	// Button params as triggers
	if (rndSamplesParamTrigger.process(params[RND_SAMPLES_PARAM].getValue())) {
		if (!sampleDir.empty()) {
			loadRandomSamplesFromDir(sampleDir);
		} else {
			reqRandomSamplesInteractive = true;
		}
	}
	if (shuffleSamplesParamTrigger.process(params[SHUFFLE_SAMPLES_PARAM].getValue())) {
		reqShuffleSamples = true;
	}
	if (reverseRandomParamTrigger.process(params[REVERSE_RANDOM_PARAM].getValue())) {
		reqReverseRandom = true;
	}
	if (rndMutesParamTrigger.process(params[RND_MUTES_PARAM].getValue())) {
		reqRandomizeMutes = true;
	}
	if (splitSampleParamTrigger.process(params[SPLIT_SAMPLE_PARAM].getValue())) {
		reqSplitSampleInteractive = true;
	}
	// Apply any UI-requested operations on the audio thread to avoid races
	if (reqShuffleSamples) {
		reqShuffleSamples = false;
		shuffleSamples();
	}
	if (reqReverseRandom) {
		reqReverseRandom = false;
		randomReverseSamples();
	}
	if (reqRandomizeMutes) {
		reqRandomizeMutes = false;
		randomizeGateStates();
	}
	if (shuffleInTrigger.process(inputs[SHUFFLE_INPUT].getVoltage())) {
		shuffleSamples();
	}
	if (reverseRandomInTrigger.process(inputs[REVERSE_RND_INPUT].getVoltage())) {
		randomReverseSamples();
	}
	if (rndMutesInTrigger.process(inputs[RND_MUTES_INPUT].getVoltage())) {
		randomizeGateStates();
	}
	// Apply per-cell UI requests safely on audio thread
	for (int i = 0; i < 16; ++i) {
		if (reqClearCell[i]) {
			reqClearCell[i] = false;
			cellSamples[i].clear();
			cellSamplePath[i].clear();
			cellSampleRate[i] = 0;
			cellStartFrac[i] = 0.f;
			cellIsSlice[i] = false;
			cellSliceStartFrac[i] = 0.f;
			cellSliceEndFrac[i] = 1.f;
			cellReversed[i] = false;
			if (playingCell == i) playingCell = -1;
		}
		if (reqLoadCellFromPath[i]) {
			std::string p = pendingCellPath[i];
			pendingCellPath[i].clear();
			reqLoadCellFromPath[i] = false;
			if (!p.empty()) {
				loadCellSample(i, p);
			}
		}
		if (reqLoadRandomCell[i]) {
			reqLoadRandomCell[i] = false;
			// Only operate if a sample directory is set; never open dialogs here
			if (!sampleDir.empty()) {
				std::vector<std::string> wavs;
				if (collectWavsInDir(sampleDir, wavs) && !wavs.empty()) {
					size_t idx = (size_t)std::floor(random::uniform() * wavs.size());
					loadCellSample(i, wavs[idx]);
				}
			}
		}
	}
	if (nextStep) {
		if(resetMode){
			resetMode = false;
			phase = 0.0;
			posX = 0;
			posY = 0;
			index = 0;
			indexYX = 0;
			lights[RESET_LIGHT].value =  1.0;
		}
		rndFloat0to1AtClockStep = random::uniform();
		index = posX + (posY * 4);
		indexYX = posY + (posX * 4);
		gatePulse.trigger(gatePulseLenSec);
	}

	lights[RESET_LIGHT].value -= lights[RESET_LIGHT].value / lightLambda / args.sampleRate;
	bool pulse = gatePulse.process(1.0 / args.sampleRate);

	//////////////////////////////////////////////////////////////////////////////////////////	
	// AUDIO PLAYBACK (runs every sample)
	//////////////////////////////////////////////////////////////////////////////////////////	
	if (playingCell >= 0) {
		const auto &buf = cellSamples[playingCell];
		if (buf.empty() || playbackPos < 0.0) {
			playingCell = -1;
			outputs[AUDIO_OUTPUT].setVoltage(0.0f);
		}
		else {
			// Linear interpolation
			double N = (double)buf.size();
			int i0 = (int)playbackPos;
			if (i0 >= (int)N) { playingCell = -1; outputs[AUDIO_OUTPUT].setVoltage(0.0f); }
			else {
				int i1 = std::min(i0 + 1, (int)N - 1);
				double frac = playbackPos - (double)i0;
				double s = (1.0 - frac) * (double)buf[i0] + frac * (double)buf[i1];
					int br = cellSampleRate[playingCell] > 0 ? cellSampleRate[playingCell] : 44100;
					double remainingSamples = (double)buf.size() - playbackPos;
					double remainingSec = remainingSamples / (double)br;
					double startSec = std::max(0.0, (playbackPos - playbackStartPos) / (double)br);
					double startGain = 1.0;
					double tailGain = 1.0;
					if (fadesEnabled) {
						startGain = startSec < (double)fadeLenSec ? std::max(0.0, startSec / (double)fadeLenSec) : 1.0;
						tailGain = remainingSec < (double)fadeLenSec ? std::max(0.0, remainingSec / (double)fadeLenSec) : 1.0;
					}
					double gain = startGain * tailGain;
					float outGain = params[OUTPUT_GAIN_PARAM].getValue();
					outputs[AUDIO_OUTPUT].setVoltage((float)(s * 5.0 * gain * (double)outGain));
				playbackPos += playbackStep;
				if (playbackPos >= (double)buf.size()) {
					playingCell = -1;
				}
			}
		}
	}
	else {
		outputs[AUDIO_OUTPUT].setVoltage(0.0f);
	}

	//////////////////////////////////////////////////////////////////////////////////////////	
	// MAIN XY OUT (gates and V/Oct)
	//////////////////////////////////////////////////////////////////////////////////////////	

	// Gate buttons handled via cell overlay X; no per-cell gate params processed here

	// Cells
	bool gatesOn = (running && gateState[index]);
	if (gateMode == TRIGGER) gatesOn = gatesOn && pulse;
	else if (gateMode == RETRIGGER) gatesOn = gatesOn && !pulse;

	// Start sample playback once per step. If V/Oct input is connected, map notes from C3
	// upward to select which cell's sample to play (0..15 by semitone).
	if (nextStep) {
		int playIdx = index;
		if (inputs[VOCT_INPUT].isConnected()) {
			int semis = (int)std::round(inputs[VOCT_INPUT].getVoltage() * 12.0f);
			playIdx = clampijw(semis, 0, 15);
		}
		bool willFire = running && gateState[index] && gateState[playIdx];
		if (willFire && !cellSamples[playIdx].empty()) {
			playingCell = playIdx;
				{
					double N = (double)cellSamples[playIdx].size();
					double start = std::max(0.0, std::min(N - 1.0, (double)cellStartFrac[playIdx] * N));
					playbackPos = start;
					playbackStartPos = start;
				}
			int br = cellSampleRate[playIdx] > 0 ? cellSampleRate[playIdx] : 44100;
			playbackStep = (double)br / (double)args.sampleRate;
		}
	}

	// External gate input can trigger sample playback independent of sequencer
	if (gateInTrigger.process(inputs[GATE_INPUT].getVoltage())) {
		int playIdx = index;
		if (inputs[VOCT_INPUT].isConnected()) {
			int semis = (int)std::round(inputs[VOCT_INPUT].getVoltage() * 12.0f);
			playIdx = clampijw(semis, 0, 15);
		}
		if (gateState[playIdx] && !cellSamples[playIdx].empty()) {
			playingCell = playIdx;
			{
				double N = (double)cellSamples[playIdx].size();
				double start = std::max(0.0, std::min(N - 1.0, (double)cellStartFrac[playIdx] * N));
				playbackPos = start;
				playbackStartPos = start;
			}
			int br = cellSampleRate[playIdx] > 0 ? cellSampleRate[playIdx] : 44100;
			playbackStep = (double)br / (double)args.sampleRate;
		}
	}
}

struct SampleGridWidget : ModuleWidget {
    SampleGridWidget(SampleGrid *module);
    void appendContextMenu(Menu *menu) override;
    void step() override;
};

SampleGridWidget::SampleGridWidget(SampleGrid *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*20, RACK_GRID_HEIGHT);

	setPanel(createPanel(
		asset::plugin(pluginInstance, "res/SampleGrid.svg"), 
		asset::plugin(pluginInstance, "res/SampleGrid.svg")
	));

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	///// DIR CONTROLS /////
	addParam(createParam<RightMoveButton>(Vec(70, 15), module, SampleGrid::RIGHT_MOVE_BTN_PARAM));
	addParam(createParam<LeftMoveButton>(Vec(103, 15), module, SampleGrid::LEFT_MOVE_BTN_PARAM));
	addParam(createParam<DownMoveButton>(Vec(137, 15), module, SampleGrid::DOWN_MOVE_BTN_PARAM));
	addParam(createParam<UpMoveButton>(Vec(172, 15), module, SampleGrid::UP_MOVE_BTN_PARAM));
	addParam(createParam<RndMoveButton>(Vec(215, 15), module, SampleGrid::RND_MOVE_BTN_PARAM));
	addParam(createParam<RepMoveButton>(Vec(255, 15), module, SampleGrid::REP_MOVE_BTN_PARAM));

	addInput(createInput<PJ301MPort>(Vec(70, 37), module, SampleGrid::RIGHT_INPUT));
	addInput(createInput<PJ301MPort>(Vec(103, 37), module, SampleGrid::LEFT_INPUT));
	addInput(createInput<PJ301MPort>(Vec(137, 37), module, SampleGrid::DOWN_INPUT));
	addInput(createInput<PJ301MPort>(Vec(172, 37), module, SampleGrid::UP_INPUT));
	addInput(createInput<PJ301MPort>(Vec(212, 37), module, SampleGrid::RND_DIR_INPUT));
	addInput(createInput<PJ301MPort>(Vec(253, 37), module, SampleGrid::REPEAT_INPUT));
	
	//// MAIN SEQUENCER KNOBS ////
	int boxSize = 60;
	for (int y = 0; y < 4; y++) {
		for (int x = 0; x < 4; x++) {
			int knobX = x * boxSize + 60;
			int knobY = y * boxSize + 70;
			int idx = (x+(y*4));
			
			// Replace big knob with a small waveform display and click-to-load
			struct CellWaveform : TransparentWidget {
				SampleGrid *module = nullptr; int cell = 0;
				bool draggingStart = false; float lastX = 0.f;
				CellWaveform(SampleGrid *m, int i) { module = m; cell = i; }
				void onButton(const event::Button &e) override {
					if (!module) return;
					if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
						const float w = box.size.x, h = box.size.y;
						const float d = 12.f;
						// Regions for overlays
						const float diceRx = w - d - 2.f;
						const float diceRy = h - d - 2.f;
						const float xRx = w - d - 2.f;
						const float xRy = 2.f;
						const float folderRx = 2.f;
						const float folderRy = h - d - 2.f;
						Vec m = e.pos;
						// Dice click: random sample for this cell
						if (m.x >= diceRx && m.x <= diceRx + d && m.y >= diceRy && m.y <= diceRy + d) {
							// If a directory is set, request a random load on audio thread.
							// Otherwise, open a file dialog on UI and queue the chosen path.
							if (module->sampleDir.empty()) {
								osdialog_filters *filters = osdialog_filters_parse("WAV:wav");
								char *path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, filters);
								osdialog_filters_free(filters);
								if (path) { std::string p = path; free(path); module->pendingCellPath[cell] = p; module->reqLoadCellFromPath[cell] = true; }
							} else {
								module->reqLoadRandomCell[cell] = true;
							}
							e.consume(this);
							return;
						}
						// X click: unload (clear) this cell's sample
						if (m.x >= xRx && m.x <= xRx + d && m.y >= xRy && m.y <= xRy + d) {
							module->reqClearCell[cell] = true;
							e.consume(this);
							return;
						}
						// Folder click: replace this cell's sample via file dialog
						if (m.x >= folderRx && m.x <= folderRx + d && m.y >= folderRy && m.y <= folderRy + d) {
							osdialog_filters *filters = osdialog_filters_parse("WAV:wav");
							char *path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, filters);
							osdialog_filters_free(filters);
							if (path) { std::string p = path; free(path); module->pendingCellPath[cell] = p; module->reqLoadCellFromPath[cell] = true; }
							e.consume(this);
							return;
						}
						// Default: open file dialog to load a specific WAV
						const auto &buf = module->cellSamples[cell];
						if (!buf.empty()) {
							float x = e.pos.x; float wCell = box.size.x;
							float frac = wCell > 0.f ? std::max(0.f, std::min(1.f, x / wCell)) : 0.f;
							module->cellStartFrac[cell] = frac;
							// Begin dragging to adjust start continuously
							draggingStart = true;
							lastX = x;
						}
						else {
							osdialog_filters *filters = osdialog_filters_parse("WAV:wav");
							char *path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, filters);
							osdialog_filters_free(filters);
							if (path) { std::string p = path; free(path); module->pendingCellPath[cell] = p; module->reqLoadCellFromPath[cell] = true; }
						}
						e.consume(this);
					}
					else if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_RELEASE) {
						// End dragging when mouse released
						draggingStart = false;
						e.consume(this);
					}
					// Right-click anywhere on waveform opens file dialog to replace the cell sample
					else if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS) {
						osdialog_filters *filters = osdialog_filters_parse("WAV:wav");
						char *path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, filters);
						osdialog_filters_free(filters);
						if (path) { std::string p = path; free(path); module->pendingCellPath[cell] = p; module->reqLoadCellFromPath[cell] = true; }
						e.consume(this);
					}
				}
				void onDragMove(const event::DragMove &e) override {
					if (!draggingStart || !module) return;
					lastX += e.mouseDelta.x;
					if (lastX < 0.f) lastX = 0.f;
					float wCell = box.size.x;
					if (lastX > wCell) lastX = wCell;
					float frac = wCell > 0.f ? lastX / wCell : 0.f;
					module->cellStartFrac[cell] = frac;
					e.consume(this);
				}
				void draw(const DrawArgs &args) override {
					NVGcontext *vg = args.vg; const float w = box.size.x, h = box.size.y;
					// Guard against degenerate sizes
					if (w <= 0.f || h <= 0.f) return;
					nvgBeginPath(vg); nvgRect(vg, 0, 0, w, h);
					// Base background: solid black
					nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
					nvgFill(vg);
					nvgStrokeColor(vg, nvgRGBA(200,200,200,160)); nvgStrokeWidth(vg, 1.f); nvgStroke(vg);

					// If no module (browser preview), draw placeholder and exit
					if (!module) {
						nvgFontSize(vg, 10.f); nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
						nvgFillColor(vg, nvgRGBA(180,180,180,160)); nvgText(vg, w*0.5f, h*0.5f, "load", nullptr);
						return;
					}

					// Highlight background when this cell's sample is playing
					if (module->playingCell == cell) {
						nvgBeginPath(vg); nvgRect(vg, 0, 0, w, h);
						nvgFillColor(vg, nvgRGBA(255, 140, 0, 64));
						nvgFill(vg);
					}

					// Waveform rendering
					const int ci = cell;
					if (ci < 0 || ci >= 16) return;
					std::vector<float> buf = module->cellSamples[ci];
					if (buf.empty()) {
						nvgFontSize(vg, 10.f); nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
						nvgFillColor(vg, nvgRGBA(180,180,180,160)); nvgText(vg, w*0.5f, h*0.5f, "load", nullptr);
						return;
					}
					nvgBeginPath(vg);
					nvgStrokeColor(vg, nvgRGB(25,150,252));
					nvgStrokeWidth(vg, 1.0f);
					const size_t N = buf.size();
					const size_t wpx = (size_t)std::max(1.0f, std::floor(w));
					const size_t bucket = std::max<size_t>(1, (size_t)std::floor((double)N / (double)std::max<size_t>(1, wpx)));
					for (size_t xpix = 0; xpix < wpx; ++xpix) {
						size_t i0 = (size_t)std::floor((double)xpix * (double)N / (double)wpx);
						if (i0 >= N) i0 = N - 1;
						size_t iEnd = std::min(N, i0 + bucket);
						float minV = 1.f, maxV = -1.f;
						for (size_t i = i0; i < iEnd; ++i) {
							float v = buf[i];
							if (v < -10.f) v = -10.f; if (v > 10.f) v = 10.f; // guard extreme ranges
							minV = std::min(minV, v);
							maxV = std::max(maxV, v);
						}
						// Clamp to visible [-1,1] range for drawing, ensure at least a hairline
						minV = std::max(-1.f, std::min(1.f, minV));
						maxV = std::max(-1.f, std::min(1.f, maxV));
						if (maxV - minV < 1e-6f) { maxV += 0.01f; minV -= 0.01f; }
						float y1 = h * (0.5f - 0.45f * maxV);
						float y2 = h * (0.5f - 0.45f * minV);
						nvgMoveTo(vg, (float)xpix, y1);
						nvgLineTo(vg, (float)xpix, y2);
					}
					nvgStroke(vg);

					// Draw start position marker
					float sx = clampfjw(module->cellStartFrac[ci], 0.f, 1.f) * w;
					nvgBeginPath(vg);
					nvgMoveTo(vg, sx, 0);
					nvgLineTo(vg, sx, h);
					nvgStrokeColor(vg, nvgRGBA(255, 200, 0, 200));
					nvgStrokeWidth(vg, 1.0f);
					nvgStroke(vg);

					// If this cell would play on the current step but is muted, tint background red
					if (module->running && module->index == cell) {
						if (!module->gateState[cell]) {
							nvgBeginPath(vg);
							nvgRect(vg, 0, 0, w, h);
							nvgFillColor(vg, nvgRGBA(255, 80, 80, 72));
							nvgFill(vg);
						}
					}
						// Overlay icons size and positions
						const float d = 12.f;
						const float diceRx = w - d - 2.f;
						const float diceRy = h - d - 2.f;
						const float xRx = w - d - 2.f;
						const float xRy = 2.f;
						const float folderRx = 2.f;
						const float folderRy = h - d - 2.f;
						// Dice icon (5 pips) bottom-right
						nvgBeginPath(vg);
						nvgRoundedRect(vg, diceRx, diceRy, d, d, 2.f);
						nvgFillColor(vg, nvgRGBA(245,245,245,200));
						nvgFill(vg);
						nvgStrokeColor(vg, nvgRGBA(120,120,120,180)); nvgStrokeWidth(vg, 1.f); nvgStroke(vg);
						nvgBeginPath(vg); nvgFillColor(vg, nvgRGBA(30,30,30,220));
						float r = 1.5f; float m = 3.f;
						nvgCircle(vg, diceRx + m, diceRy + m, r);
						nvgCircle(vg, diceRx + d - m, diceRy + m, r);
						nvgCircle(vg, diceRx + m, diceRy + d - m, r);
						nvgCircle(vg, diceRx + d - m, diceRy + d - m, r);
						nvgCircle(vg, diceRx + d*0.5f, diceRy + d*0.5f, r);
						nvgFill(vg);

						// X icon top-right (unload sample)
						nvgBeginPath(vg);
						nvgRoundedRect(vg, xRx, xRy, d, d, 2.f);
						nvgFillColor(vg, module && module->cellSamples[cell].empty() ? nvgRGBA(200,200,200,140) : nvgRGBA(245,245,245,200));
						nvgFill(vg);
						nvgStrokeColor(vg, nvgRGBA(120,120,120,180)); nvgStrokeWidth(vg, 1.f); nvgStroke(vg);
						nvgBeginPath(vg);
						nvgStrokeColor(vg, nvgRGBA(30,30,30,220)); nvgStrokeWidth(vg, 1.5f);
						nvgMoveTo(vg, xRx + 3.f, xRy + 3.f); nvgLineTo(vg, xRx + d - 3.f, xRy + d - 3.f);
						nvgMoveTo(vg, xRx + d - 3.f, xRy + 3.f); nvgLineTo(vg, xRx + 3.f, xRy + d - 3.f);
						nvgStroke(vg);

						// Waveform icon bottom-left (choose directory)
						nvgBeginPath(vg);
						nvgRoundedRect(vg, folderRx, folderRy, d, d, 2.f);
						nvgFillColor(vg, nvgRGBA(245,245,245,200));
						nvgFill(vg);
						nvgStrokeColor(vg, nvgRGBA(120,120,120,180)); nvgStrokeWidth(vg, 1.f); nvgStroke(vg);
						// Tiny waveform glyph inside the box
						const float wx = folderRx + 2.f;
						const float wy = folderRy + 2.f;
						const float ww = d - 4.f;
						const float wh = d - 4.f;
						const float PI = 3.1415926f;
						nvgBeginPath(vg);
						nvgStrokeColor(vg, nvgRGBA(25,150,252,220));
						nvgStrokeWidth(vg, 1.0f);
						for (int xi = 0; xi <= (int)ww; ++xi) {
							float t = (float)xi / ww;
							float s = std::sin(t * 2.f * PI);
							float y = wy + wh * (0.5f - 0.45f * s);
							if (xi == 0) nvgMoveTo(vg, wx + xi, y);
							else nvgLineTo(vg, wx + xi, y);
						}
						nvgStroke(vg);
					}
				};
				CellWaveform *wf = new CellWaveform(module, idx);
				wf->box.pos = Vec(knobX-2, knobY);
				wf->box.size = Vec(55, 55);
				addChild(wf);
				
			}
		}
		
	///// RUN /////
	addParam(createParam<TinyButton>(Vec(23, 90), module, SampleGrid::RUN_PARAM));
	addChild(createLight<SmallLight<MyBlueValueLight>>(Vec(26.90, 90+3.90), module, SampleGrid::RUNNING_LIGHT));

	///// RESET /////
	addParam(createParam<TinyButton>(Vec(23, 138), module, SampleGrid::RESET_PARAM));
	addChild(createLight<SmallLight<MyBlueValueLight>>(Vec(26.90, 138+3.90), module, SampleGrid::RESET_LIGHT));
	addInput(createInput<PJ301MPort>(Vec(19, 157), module, SampleGrid::RESET_INPUT));

	addInput(createInput<PJ301MPort>(Vec(19, 207), module, SampleGrid::VOCT_INPUT));
	addInput(createInput<PJ301MPort>(Vec(19, 250), module, SampleGrid::GATE_INPUT));

	addParam(createParam<SmallButton>(Vec(71, 335), module, SampleGrid::RND_SAMPLES_PARAM));
	addParam(createParam<SmallButton>(Vec(128, 335), module, SampleGrid::SPLIT_SAMPLE_PARAM));
	addParam(createParam<SmallButton>(Vec(169, 335), module, SampleGrid::SHUFFLE_SAMPLES_PARAM));
	addParam(createParam<SmallButton>(Vec(210, 335), module, SampleGrid::REVERSE_RANDOM_PARAM));
	addParam(createParam<SmallButton>(Vec(250, 335), module, SampleGrid::RND_MUTES_PARAM));
	// Trigger inputs below control buttons
	// Trigger inputs below control buttons
	addInput(createInput<TinyPJ301MPort>(Vec(75, 360), module, SampleGrid::RND_SAMPLES_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(174, 360), module, SampleGrid::SHUFFLE_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(215, 360), module, SampleGrid::REVERSE_RND_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(255, 360), module, SampleGrid::RND_MUTES_INPUT));
	
	///// OUTPUTS /////
	addOutput(createOutput<PJ301MPort>(Vec(19, 300), module, SampleGrid::AUDIO_OUTPUT));
	addParam(createParam<JwTinyKnob>(Vec(36, 341), module, SampleGrid::OUTPUT_GAIN_PARAM));
}

struct SampleGridPitchMenuItem : MenuItem {
	SampleGrid *sampleGrid;
	void onAction(const event::Action &e) override {
		sampleGrid->ignoreGateOnPitchOut = !sampleGrid->ignoreGateOnPitchOut;
	}
	void step() override {
		rightText = (sampleGrid->ignoreGateOnPitchOut) ? "✔" : "";
		MenuItem::step();
	}
};

struct SampleGridGateModeItem : MenuItem {
	SampleGrid *sampleGrid;
	SampleGrid::GateMode gateMode;
	void onAction(const event::Action &e) override {
		sampleGrid->gateMode = gateMode;
	}
	void step() override {
		rightText = (sampleGrid->gateMode == gateMode) ? "✔" : "";
		MenuItem::step();
	}
};

// Local quantity/slider removed; using shared GatePulseMsQuantity/GatePulseMsSlider from JWModules.hpp

struct SampleGridPatternModeItem : MenuItem {
	SampleGrid *sampleGrid;
	SampleGrid::PatternMode mode;
	void onAction(const event::Action &e) override {
		sampleGrid->patternMode = mode;
	}
	void step() override {
		rightText = (sampleGrid->patternMode == mode) ? "✔" : "";
		MenuItem::step();
	}
};

// Submenu wrapper for pattern mode options
struct SampleGridPatternSubMenuItem : MenuItem {
	SampleGrid *sampleGrid = nullptr;
	Menu *createChildMenu() override {
		Menu *submenu = new Menu;
		// Row Wrap Stay
		auto *rowWrapStayItem = new SampleGridPatternModeItem();
		rowWrapStayItem->text = "Row Wrap (Stay Row)";
		rowWrapStayItem->sampleGrid = sampleGrid;
		rowWrapStayItem->mode = SampleGrid::ROW_WRAP_STAY;
		submenu->addChild(rowWrapStayItem);
		// Row-Major
		auto *rowMajorItem = new SampleGridPatternModeItem();
		rowMajorItem->text = "Row-Major";
		rowMajorItem->sampleGrid = sampleGrid;
		rowMajorItem->mode = SampleGrid::ROW_MAJOR;
		submenu->addChild(rowMajorItem);
		// Column-Major
		auto *colMajorItem = new SampleGridPatternModeItem();
		colMajorItem->text = "Column-Major";
		colMajorItem->sampleGrid = sampleGrid;
		colMajorItem->mode = SampleGrid::COL_MAJOR;
		submenu->addChild(colMajorItem);
		// Snake Rows
		auto *snakeRowsItem = new SampleGridPatternModeItem();
		snakeRowsItem->text = "Snake Rows";
		snakeRowsItem->sampleGrid = sampleGrid;
		snakeRowsItem->mode = SampleGrid::SNAKE_ROWS;
		submenu->addChild(snakeRowsItem);
		// Snake Columns
		auto *snakeColsItem = new SampleGridPatternModeItem();
		snakeColsItem->text = "Snake Columns";
		snakeColsItem->sampleGrid = sampleGrid;
		snakeColsItem->mode = SampleGrid::SNAKE_COLS;
		submenu->addChild(snakeColsItem);
		// Snake Diagonal Down-Right
		auto *snakeDiagDRItem = new SampleGridPatternModeItem();
		snakeDiagDRItem->text = "Snake Diagonal Down-Right";
		snakeDiagDRItem->sampleGrid = sampleGrid;
		snakeDiagDRItem->mode = SampleGrid::SNAKE_DIAG_DR;
		submenu->addChild(snakeDiagDRItem);
		// Snake Diagonal Up-Right
		auto *snakeDiagURItem = new SampleGridPatternModeItem();
		snakeDiagURItem->text = "Snake Diagonal Up-Right";
		snakeDiagURItem->sampleGrid = sampleGrid;
		snakeDiagURItem->mode = SampleGrid::SNAKE_DIAG_UR;
		submenu->addChild(snakeDiagURItem);
		return submenu;
	}
	void step() override {
		rightText = "\u25B6"; // triangle indicating submenu
		MenuItem::step();
	}
};

// Local quantity for fade length in seconds (0.0 .. 1.0)
struct FadeSecondsQuantity : Quantity {
	SampleGrid* sampleGrid = nullptr;
	float defaultSeconds = 0.005f;
	std::string label = "Fade Length";
	std::string unit = "s";
	FadeSecondsQuantity(SampleGrid* sg) : sampleGrid(sg) {}
	void setValue(float value) override {
		if (!sampleGrid) return;
		float v = clampfjw(value, getMinValue(), getMaxValue());
		sampleGrid->fadeLenSec = v;
	}
	float getValue() override {
		if (!sampleGrid) return defaultSeconds;
		return sampleGrid->fadeLenSec;
	}
	float getMinValue() override { return 0.0f; }
	float getMaxValue() override { return 1.0f; }
	float getDefaultValue() override { return defaultSeconds; }
	float getDisplayValue() override { return getValue(); }
	void setDisplayValue(float displayValue) override { setValue(displayValue); }
	int getDisplayPrecision() override { return 3; }
	std::string getLabel() override { return label; }
	std::string getUnit() override { return unit; }
	std::string getDisplayValueString() override {
		char buf[32];
		snprintf(buf, sizeof(buf), "%.3f", (double)getDisplayValue());
		return std::string(buf);
	}
	void setDisplayValueString(std::string s) override {
		try { float v = std::stof(s); setDisplayValue(v); } catch (...) {}
	}
};

void SampleGridWidget::appendContextMenu(Menu *menu) {
	MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);

	SampleGrid *sampleGrid = dynamic_cast<SampleGrid*>(module);
	if (!sampleGrid) {
		MenuLabel *info = new MenuLabel();
		info->text = "Module not active (browser preview)";
		menu->addChild(info);
		return;
	}

	MenuLabel *modeLabel = new MenuLabel();
	modeLabel->text = "Gate Mode";
	menu->addChild(modeLabel);

	SampleGridGateModeItem *triggerItem = new SampleGridGateModeItem();
	triggerItem->text = "Trigger";
	triggerItem->sampleGrid = sampleGrid;
	triggerItem->gateMode = SampleGrid::TRIGGER;
	menu->addChild(triggerItem);

	SampleGridGateModeItem *retriggerItem = new SampleGridGateModeItem();
	retriggerItem->text = "Retrigger";
	retriggerItem->sampleGrid = sampleGrid;
	retriggerItem->gateMode = SampleGrid::RETRIGGER;
	menu->addChild(retriggerItem);

	SampleGridGateModeItem *continuousItem = new SampleGridGateModeItem();
	continuousItem->text = "Continuous";
	continuousItem->sampleGrid = sampleGrid;
	continuousItem->gateMode = SampleGrid::CONTINUOUS;
	menu->addChild(continuousItem);

	SampleGridPitchMenuItem *pitchMenuItem = new SampleGridPitchMenuItem();
	pitchMenuItem->text = "Ignore Gate for V/OCT Out";
	pitchMenuItem->sampleGrid = sampleGrid;
	menu->addChild(pitchMenuItem);

	MenuLabel *spacerLabel2 = new MenuLabel();
	menu->addChild(spacerLabel2);

	// Auto-wrap options removed in favor of snake pattern modes

	MenuLabel *patternLabel = new MenuLabel();
	patternLabel->text = "Pattern Mode";
	menu->addChild(patternLabel);
	SampleGridPatternSubMenuItem *patternSub = new SampleGridPatternSubMenuItem();
	patternSub->text = "Select Pattern";
	patternSub->sampleGrid = sampleGrid;
	menu->addChild(patternSub);

	// Fades toggle
	MenuLabel *spacerLabelFade = new MenuLabel();
	menu->addChild(spacerLabelFade);
	MenuLabel *fadeLabel = new MenuLabel();
	fadeLabel->text = "Audio Fades";
	menu->addChild(fadeLabel);

	struct SampleGridFadesItem : MenuItem {
		SampleGrid *sampleGrid;
		void onAction(const event::Action &e) override { sampleGrid->fadesEnabled = !sampleGrid->fadesEnabled; }
		void step() override { rightText = (sampleGrid->fadesEnabled) ? "✔" : ""; MenuItem::step(); }
	};
	SampleGridFadesItem *fadesItem = new SampleGridFadesItem();
	fadesItem->text = "Enable Fades";
	fadesItem->sampleGrid = sampleGrid;
	menu->addChild(fadesItem);

	// Fade length slider (seconds, max 2.0)
	ui::Slider* fadeSlider = new ui::Slider();
	fadeSlider->quantity = new FadeSecondsQuantity(sampleGrid);
	fadeSlider->box.size.x = 175.0f;
	menu->addChild(fadeSlider);

	// Reset all start points to zero
	struct SampleGridResetStartsItem : MenuItem {
		SampleGrid *sampleGrid;
		void onAction(const event::Action &e) override {
			if (!sampleGrid) return;
			for (int i = 0; i < 16; ++i) sampleGrid->cellStartFrac[i] = 0.f;
		}
	};
	SampleGridResetStartsItem *resetStartsItem = new SampleGridResetStartsItem();
	resetStartsItem->text = "Reset All Start Points";
	resetStartsItem->sampleGrid = sampleGrid;
	menu->addChild(resetStartsItem);

	// Mute/Unmute all cells
	struct SampleGridMuteAllItem : MenuItem {
		SampleGrid *sampleGrid;
		void onAction(const event::Action &e) override {
			if (!sampleGrid) return;
			for (int i = 0; i < 16; ++i) sampleGrid->gateState[i] = false;
		}
	};
	struct SampleGridUnmuteAllItem : MenuItem {
		SampleGrid *sampleGrid;
		void onAction(const event::Action &e) override {
			if (!sampleGrid) return;
			for (int i = 0; i < 16; ++i) sampleGrid->gateState[i] = true;
		}
	};
	SampleGridMuteAllItem *muteAllItem = new SampleGridMuteAllItem();
	muteAllItem->text = "Mute All Cells";
	muteAllItem->sampleGrid = sampleGrid;
	menu->addChild(muteAllItem);
	SampleGridUnmuteAllItem *unmuteAllItem = new SampleGridUnmuteAllItem();
	unmuteAllItem->text = "Unmute All Cells";
	unmuteAllItem->sampleGrid = sampleGrid;
	menu->addChild(unmuteAllItem);

	// Random samples directory section
	MenuLabel *spacerLabelDir = new MenuLabel();
	menu->addChild(spacerLabelDir);
	MenuLabel *dirLabelTitle = new MenuLabel();
	dirLabelTitle->text = "Random Samples Directory";
	menu->addChild(dirLabelTitle);

	struct SampleGridDirLabel : MenuLabel {
		SampleGrid *sampleGrid;
		void step() override {
			text = sampleGrid && !sampleGrid->sampleDir.empty() ? ("Current: " + sampleGrid->sampleDir) : "Current: (not set)";
			MenuLabel::step();
		}
	};
	SampleGridDirLabel *dirCurrentLabel = new SampleGridDirLabel();
	dirCurrentLabel->sampleGrid = sampleGrid;
	menu->addChild(dirCurrentLabel);

	struct SampleGridChangeDirItem : MenuItem {
		SampleGrid *sampleGrid;
		void onAction(const event::Action &e) override {
			if (!sampleGrid) return;
			if (sampleGrid->chooseSampleDir()) {
				if (!sampleGrid->sampleDir.empty()) {
					sampleGrid->loadRandomSamplesFromDir(sampleGrid->sampleDir);
				}
			}
		}
	};
	SampleGridChangeDirItem *changeDirItem = new SampleGridChangeDirItem();
	changeDirItem->text = "Change Directory…";
	changeDirItem->sampleGrid = sampleGrid;
	menu->addChild(changeDirItem);
}

Model *modelSampleGrid = createModel<SampleGrid, SampleGridWidget>("SampleGrid");

void SampleGridWidget::step() {
	ModuleWidget::step();
	SampleGrid *m = dynamic_cast<SampleGrid*>(module);
	if (!m) return;
	// Handle interactive actions requested by process() via flags
	if (m->reqRandomSamplesInteractive) {
		m->reqRandomSamplesInteractive = false;
		m->loadRandomSamplesInteractive();
		m->params[SampleGrid::RND_SAMPLES_PARAM].setValue(0.f);
	}
	if (m->reqSplitSampleInteractive) {
		m->reqSplitSampleInteractive = false;
		m->loadSplitSampleInteractive();
		m->params[SampleGrid::SPLIT_SAMPLE_PARAM].setValue(0.f);
	}
}
