#include "JWModules.hpp"

#include <string>
#include <vector>
#include <fstream>
#include <cstdint>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include "osdialog.h"

struct Grains : Module {
	enum ParamIds {
		GRAIN_SIZE_MS,
		GRAIN_DENSITY,
		GRAIN_SPREAD_MS,
		GRAIN_PITCH_SEMI,
		GRAIN_GAIN,
		WINDOW_TYPE,
		PAN_RANDOMNESS,
		RANDOM_BUTTON,
		NUM_PARAMS
	};
	enum InputIds {
		POSITION_INPUT,
		PITCH_INPUT,
		SIZE_CV,
		DENSITY_CV,
		SPREAD_CV,
		PAN_CV,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_L,
		OUT_R,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	// Sample data
	std::string samplePath;
	std::vector<float> sampleL;
	std::vector<float> sampleR;
	int fileSampleRate = 44100;
	double playPos = 0.0; // fractional position in sample frames
	std::string statusMsg = "Load WAV from context menu";
	float silenceThreshold = 0.02f;
	bool embedInPatch = false;
	bool bufferDirty = false;
	bool autoAdvance = false;

	struct Grain {
		double pos;
		double step;
		int dur;
		int age;
		bool active;
		float pan; // -1..1 per-grain random pan
		Grain() : pos(0.0), step(1.0), dur(0), age(0), active(false), pan(0.f) {}
	};
	std::vector<Grain> grains;
	double spawnAccum = 0.0;
 
	// Update coordination
	// Protect against out-of-range access when sample changes while grains are active
	// (engine vs UI). Keep simple guard in process; no heavy locking.
	// If needed later, this can be expanded to a proper lock.
	// std::atomic<bool> sampleUpdating; // reserved

	// Helpers
	bool loadSampleFromPath(const std::string &path);
	bool loadRandomSiblingSample();
	bool removeSilence(float threshold);
	// bool trimSilenceEdges(float threshold); // removed
	// bool suppressSilence(float threshold); // removed
	bool normalizeSample();
	// Base64 helpers for embedding samples in patch JSON
	static std::string b64Encode(const uint8_t* data, size_t len);
	static std::vector<uint8_t> b64Decode(const std::string& str);

	Grains() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configOutput(OUT_L, "Audio L");
		configOutput(OUT_R, "Audio R");
		configInput(POSITION_INPUT, "Position CV (0–10V)");
		configInput(PITCH_INPUT, "Pitch CV (1V/Oct)");
		configInput(SIZE_CV, "Grain Size CV (0–10V)");
		configInput(DENSITY_CV, "Grain Density CV (0–10V)");
		configInput(SPREAD_CV, "Grain Spread CV (0–10V)");
		configInput(PAN_CV, "Pan Randomness CV (0–10V)");
		configParam(GRAIN_SIZE_MS, 5.f, 2000.f, 50.f, "Grain size", " ms");
		configParam(GRAIN_DENSITY, 0.f, 200.f, 20.f, "Grain density", " grains/s");
		configParam(GRAIN_SPREAD_MS, 0.f, 2000.f, 30.f, "Grain spread", " ms");
		configParam(GRAIN_PITCH_SEMI, -24.f, 24.f, 0.f, "Pitch", " semitones");
		configParam(GRAIN_GAIN, 0.f, 1.f, 0.8f, "Output gain");
		configSwitch(WINDOW_TYPE, 0.f, 3.f, 0.f, "Window type");
		configParam(PAN_RANDOMNESS, 0.f, 1.f, 0.f, "Pan randomness");
		configButton(RANDOM_BUTTON, "Random sample");
		grains.resize(128);
	}

	void process(const ProcessArgs &args) override;

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		if (!samplePath.empty()) {
			json_object_set_new(rootJ, "path", json_string(samplePath.c_str()));
		}
		json_object_set_new(rootJ, "embed", json_boolean(embedInPatch));
		json_object_set_new(rootJ, "autoAdvance", json_boolean(autoAdvance));
		json_object_set_new(rootJ, "playPos", json_real(playPos));
		// Always embed if buffer was modified so patch reflects edits
		if ((embedInPatch || bufferDirty) && !sampleL.empty()) {
			// Serialize float32 arrays L/R as base64
			std::vector<uint8_t> bytesL(sampleL.size() * 4);
			std::vector<uint8_t> bytesR(sampleR.size() * 4);
			for (size_t i = 0; i < sampleL.size(); ++i) std::memcpy(&bytesL[i*4], &sampleL[i], 4);
			for (size_t i = 0; i < sampleR.size(); ++i) std::memcpy(&bytesR[i*4], &sampleR[i], 4);
			std::string b64L = b64Encode(bytesL.data(), bytesL.size());
			std::string b64R = b64Encode(bytesR.data(), bytesR.size());
			json_object_set_new(rootJ, "rate", json_integer(fileSampleRate));
			json_object_set_new(rootJ, "embeddedL", json_string(b64L.c_str()));
			json_object_set_new(rootJ, "embeddedR", json_string(b64R.c_str()));
		}

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		// Optional playhead position to restore after loading audio
		double savedPlayPos = -1.0;
		json_t *ppJ = json_object_get(rootJ, "playPos");
		if (ppJ && (json_is_real(ppJ) || json_is_integer(ppJ))) savedPlayPos = json_number_value(ppJ);

		json_t *pathJ = json_object_get(rootJ, "path");
		if (pathJ && json_is_string(pathJ)) {
			samplePath = json_string_value(pathJ);
			if (!loadSampleFromPath(samplePath)) {
				statusMsg = "Unsupported or unreadable WAV";
			}
			else if (savedPlayPos >= 0.0) {
				// Clamp to loaded buffer length
				double maxPos = (!sampleL.empty()) ? (double)sampleL.size() - 1.0 : 0.0;
				playPos = std::min(std::max(0.0, savedPlayPos), maxPos);
			}
		}
		json_t *embedJ = json_object_get(rootJ, "embed");
		if (embedJ && json_is_boolean(embedJ)) embedInPatch = json_boolean_value(embedJ);
		json_t *embLJ = json_object_get(rootJ, "embeddedL");
		json_t *embRJ = json_object_get(rootJ, "embeddedR");
		json_t *rateJ = json_object_get(rootJ, "rate");
		json_t *autoAdvJ = json_object_get(rootJ, "autoAdvance");
		if (autoAdvJ && json_is_boolean(autoAdvJ)) autoAdvance = json_boolean_value(autoAdvJ);
		if (embLJ && json_is_string(embLJ) && embRJ && json_is_string(embRJ)) {
			std::string b64L = json_string_value(embLJ);
			std::string b64R = json_string_value(embRJ);
			auto bytesL = b64Decode(b64L);
			auto bytesR = b64Decode(b64R);
			if (!bytesL.empty() && bytesL.size() % 4 == 0 && !bytesR.empty() && bytesR.size() % 4 == 0) {
				sampleL.resize(bytesL.size() / 4);
				sampleR.resize(bytesR.size() / 4);
				for (size_t i = 0; i < sampleL.size(); ++i) std::memcpy(&sampleL[i], &bytesL[i*4], 4);
				for (size_t i = 0; i < sampleR.size(); ++i) std::memcpy(&sampleR[i], &bytesR[i*4], 4);
				fileSampleRate = (rateJ && json_is_integer(rateJ)) ? (int)json_integer_value(rateJ) : fileSampleRate;
				samplePath.clear();
				statusMsg = "Loaded: embedded";
				for (auto &g : grains) g.active = false;
				spawnAccum = 0.0;
				if (savedPlayPos >= 0.0) {
					double maxPos = (!sampleL.empty()) ? (double)sampleL.size() - 1.0 : 0.0;
					playPos = std::min(std::max(0.0, savedPlayPos), maxPos);
				}
			}
		}
	}


	void onReset() override {
		sampleL.clear();
		sampleR.clear();
		samplePath.clear();
		playPos = 0.0;
		statusMsg = "Load WAV from context menu";
		for (auto &g : grains) g.active = false;
		spawnAccum = 0.0;
	}

	void onRandomize() override {
	}
};
// Local base64 helpers (simple RFC 4648)
std::string Grains::b64Encode(const uint8_t* data, size_t dataLen) {
	static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string out; out.reserve(((dataLen + 2) / 3) * 4);
	for (size_t i = 0; i < dataLen; i += 3) {
		uint32_t b = 0; int n = 0;
		for (; n < 3 && i + n < dataLen; ++n) b |= uint32_t(data[i + n]) << (8 * (2 - n));
		out += alphabet[(b >> 18) & 0x3F];
		out += alphabet[(b >> 12) & 0x3F];
		out += (n > 1) ? alphabet[(b >> 6) & 0x3F] : '=';
		out += (n > 2) ? alphabet[b & 0x3F] : '=';
	}
	return out;
}

std::vector<uint8_t> Grains::b64Decode(const std::string& str) {
	auto val = [](char c)->int{
		if ('A'<=c && c<='Z') return c - 'A';
		if ('a'<=c && c<='z') return c - 'a' + 26;
		if ('0'<=c && c<='9') return c - '0' + 52;
		if (c=='+') return 62; if (c=='/') return 63; if (c=='=') return -2; return -1;
	};
	std::vector<uint8_t> out; uint32_t block=0; int i=0; int pad=0;
	for (char c : str) {
		int d = val(c); if (d < 0) { if (d==-2) pad++; else continue; }
		block |= uint32_t(d & 0x3F) << (6 * (3 - i)); i++;
		if (i==4) {
			out.push_back((block >> 16) & 0xFF);
			if (pad < 2) out.push_back((block >> 8) & 0xFF);
			if (pad < 1) out.push_back(block & 0xFF);
			block=0; i=0; pad=0;
		}
	}
	return out;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
// STEP
///////////////////////////////////////////////////////////////////////////////////////////////////
void Grains::process(const ProcessArgs &args) {
	if (sampleL.empty()) {
		outputs[OUT_L].setVoltage(0.f);
		outputs[OUT_R].setVoltage(0.f);
		return;
	}

	if (inputs[POSITION_INPUT].isConnected()) {
		float v = inputs[POSITION_INPUT].getVoltage();
		float f = std::max(0.f, std::min(1.f, v / 10.f));
		playPos = f * std::max(0.0, (double)sampleL.size() - 1.0);
	}

	double srHost = args.sampleRate;
	// Base params
	double grainSizeMs = params[GRAIN_SIZE_MS].getValue();
	double spreadMs = params[GRAIN_SPREAD_MS].getValue();
	double density = params[GRAIN_DENSITY].getValue();
	double pitchSemi = params[GRAIN_PITCH_SEMI].getValue();
	double pitchParam = std::pow(2.0, pitchSemi / 12.0);
	double pitchCv = 1.0;
	if (inputs[PITCH_INPUT].isConnected()) {
		double vOct = inputs[PITCH_INPUT].getVoltage();
		pitchCv = std::pow(2.0, vOct);
	}
	double pitch = pitchParam * pitchCv;
	double panAmount = params[PAN_RANDOMNESS].getValue();

	// Apply CV modulation (0–10V maps to full parameter range)
	{
		const double sizeMin = 5.0, sizeMax = 2000.0;
		const double denMin = 0.0, denMax = 200.0;
		const double sprMin = 0.0, sprMax = 2000.0;
		const double panMin = 0.0, panMax = 1.0;
		if (inputs[SIZE_CV].isConnected()) {
			double v = inputs[SIZE_CV].getVoltage();
			grainSizeMs = std::max(sizeMin, std::min(sizeMax, grainSizeMs + v * (sizeMax - sizeMin) / 10.0));
		}
		if (inputs[DENSITY_CV].isConnected()) {
			double v = inputs[DENSITY_CV].getVoltage();
			density = std::max(denMin, std::min(denMax, density + v * (denMax - denMin) / 10.0));
		}
		if (inputs[SPREAD_CV].isConnected()) {
			double v = inputs[SPREAD_CV].getVoltage();
			spreadMs = std::max(sprMin, std::min(sprMax, spreadMs + v * (sprMax - sprMin) / 10.0));
		}
		if (inputs[PAN_CV].isConnected()) {
			double v = inputs[PAN_CV].getVoltage();
			panAmount = std::max(panMin, std::min(panMax, panAmount + v * (panMax - panMin) / 10.0));
		}
	}
	double gain = params[GRAIN_GAIN].getValue();

	spawnAccum += density / srHost;
	while (spawnAccum >= 1.0) {
		spawnAccum -= 1.0;
		for (auto &g : grains) {
			if (!g.active) {
				double spreadFrames = spreadMs * (double)fileSampleRate / 1000.0;
				double offset = (random::uniform() * 2.0 - 1.0) * spreadFrames;
				double start = playPos + offset;
				if (start < 0.0) start = 0.0;
				if (start > (double)sampleL.size() - 1.0) start = (double)sampleL.size() - 1.0;
				g.pos = start;
				g.step = pitch * (double)fileSampleRate / srHost;
				g.dur = (int)std::max(1.0, grainSizeMs * (double)fileSampleRate / 1000.0);
				g.age = 0;
				g.active = true;
				g.pan = (float)((random::uniform() * 2.0 - 1.0) * panAmount);
				break;
			}
		}
	}

	double outL = 0.0;
	double outR = 0.0;
	int wtype = (int) std::round(params[WINDOW_TYPE].getValue());
	for (auto &g : grains) {
		if (!g.active) continue;
		int i0 = (int)g.pos;
		int i1 = std::min(i0 + 1, (int)sampleL.size() - 1);
        // Guard against out-of-range indices if a new sample was loaded
        if (i0 < 0 || i0 >= (int)sampleL.size()) { g.active = false; continue; }
		double frac = g.pos - (double)i0;
		double sL = (1.0 - frac) * (double)sampleL[i0] + frac * (double)sampleL[i1];
		double sR = (1.0 - frac) * (double)sampleR[i0] + frac * (double)sampleR[i1];
		// Pan the grain's contribution using equal-power law on mono mix
		double sMono = 0.5 * (sL + sR);
		double t = (double)g.age / (double)std::max(1, g.dur);
		double w = 1.0;
		switch (wtype) {
			case 0: // Hann
				w = 0.5 - 0.5 * std::cos(6.283185307179586 * t);
				break;
			case 1: // Hamming
				w = 0.54 - 0.46 * std::cos(6.283185307179586 * t);
				break;
			case 2: // Triangular
				w = 1.0 - std::abs(2.0 * t - 1.0);
				break;
			case 3: // Rectangular
				w = 1.0;
				break;
		}
		// Compute equal-power pan weights from g.pan in [-1,1]
		double theta = (g.pan + 1.0) * 0.5 * 1.5707963267948966; // 0..pi/2
		double wL = std::cos(theta);
		double wR = std::sin(theta);
		outL += sMono * w * wL;
		outR += sMono * w * wR;
		g.pos += g.step;
		g.age++;
		if (g.age >= g.dur || g.pos < 0.0 || g.pos >= (double)sampleL.size()) {
			g.active = false;
		}
	}

	outputs[OUT_L].setVoltage((float)(outL * gain * 5.0));
	outputs[OUT_R].setVoltage((float)(outR * gain * 5.0));

    if (autoAdvance && !inputs[POSITION_INPUT].isConnected()) {
		double step = (double)fileSampleRate / srHost;
		playPos += step;
		if (playPos >= (double)sampleL.size()) playPos = 0.0;
	}
};

// Minimal WAV loader: supports PCM16 and Float32, mono/stereo (mixed to mono)
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

bool Grains::loadSampleFromPath(const std::string &path) {
	std::ifstream in(path, std::ios::binary);
	if (!in.good()) { statusMsg = "Could not open file"; return false; }

	char riff[4]; in.read(riff, 4);
	if (in.gcount() != 4 || std::string(riff, 4) != "RIFF") { statusMsg = "Not a WAV/RIFF file"; return false; }
	(void)readU32(in); // file size
	char wave[4]; in.read(wave, 4);
	if (in.gcount() != 4 || std::string(wave, 4) != "WAVE") { statusMsg = "Missing WAVE header"; return false; }

	// Read chunks
	uint16_t audioFormat = 1; // 1=PCM, 3=FLOAT
	uint16_t numChannels = 1;
	uint32_t sRate = 44100;
	uint16_t bitsPerSample = 16;
	uint32_t dataSize = 0;
	std::streampos dataPos = 0;

	while (in.good() && !in.eof()) {
		char id[4];
		in.read(id, 4);
		if (in.gcount() != 4) break;
		uint32_t chunkSize = readU32(in);
		std::string cid(id, 4);
		if (cid == "fmt ") {
			std::streampos start = in.tellg();
			audioFormat = readU16(in);
			numChannels = readU16(in);
			sRate = readU32(in);
			(void)readU32(in); // byteRate
			(void)readU16(in); // blockAlign
			bitsPerSample = readU16(in);
			// Skip any extra fmt bytes
			in.seekg(start + (std::streamoff)chunkSize);
		}
		else if (cid == "data") {
			dataPos = in.tellg();
			dataSize = chunkSize;
			in.seekg((std::streamoff)dataPos + (std::streamoff)dataSize);
		}
		else {
			// Skip unknown chunk
			in.seekg((std::streamoff)in.tellg() + (std::streamoff)chunkSize);
		}
	}

	if (dataSize == 0 || dataPos == 0) { statusMsg = "No data chunk"; return false; }
	// Load samples
	in.clear();
	in.seekg(dataPos);
	std::vector<float> left;
	std::vector<float> right;
	left.reserve(dataSize / (bitsPerSample / 8));
	right.reserve(dataSize / (bitsPerSample / 8));
	if (audioFormat == 1 && bitsPerSample == 16) {
		// PCM16
		const size_t frames = dataSize / (numChannels * 2);
		for (size_t i = 0; i < frames; ++i) {
			if (numChannels == 1) {
				int16_t s16 = (int16_t)readU16(in);
				float v = (float)s16 / 32768.f;
				left.push_back(v);
				right.push_back(v);
			} else {
				int16_t l16 = (int16_t)readU16(in);
				int16_t r16 = (int16_t)readU16(in);
				float vl = (float)l16 / 32768.f;
				float vr = (float)r16 / 32768.f;
				left.push_back(vl);
				right.push_back(vr);
				// Skip extra channels if >2
				for (int ch = 2; ch < (int)numChannels; ++ch) {
					(void)readU16(in);
				}
			}
		}
	}
	else if (audioFormat == 1 && bitsPerSample == 24) {
		// PCM24
		const size_t frames = dataSize / (numChannels * 3);
		for (size_t i = 0; i < frames; ++i) {
			auto read24 = [&in]() {
				uint8_t b[3]; in.read((char*)b, 3);
				int32_t v = (int32_t)(b[0] | (b[1] << 8) | (b[2] << 16));
				if (v & 0x800000) v |= 0xFF000000; // sign-extend
				return (float)v / 8388608.f;
			};
			if (numChannels == 1) {
				float v = read24();
				left.push_back(v);
				right.push_back(v);
			} else {
				float vl = read24();
				float vr = read24();
				left.push_back(vl);
				right.push_back(vr);
				for (int ch = 2; ch < (int)numChannels; ++ch) { (void)read24(); }
			}
		}
	}
	else if (audioFormat == 1 && bitsPerSample == 32) {
		// PCM32 int
		const size_t frames = dataSize / (numChannels * 4);
		for (size_t i = 0; i < frames; ++i) {
			auto read32 = [&in]() {
				uint32_t u = readU32(in);
				int32_t s = (int32_t)u;
				return (float)s / 2147483648.f;
			};
			if (numChannels == 1) {
				float v = read32();
				left.push_back(v);
				right.push_back(v);
			} else {
				float vl = read32();
				float vr = read32();
				left.push_back(vl);
				right.push_back(vr);
				for (int ch = 2; ch < (int)numChannels; ++ch) { (void)read32(); }
			}
		}
	}
	else if (audioFormat == 3 && bitsPerSample == 32) {
		// Float32
		const size_t frames = dataSize / (numChannels * 4);
		for (size_t i = 0; i < frames; ++i) {
			auto readf = [&in]() {
				float sf; in.read((char*)&sf, 4); return sf;
			};
			if (numChannels == 1) {
				float v = readf();
				left.push_back(v);
				right.push_back(v);
			} else {
				float vl = readf();
				float vr = readf();
				left.push_back(vl);
				right.push_back(vr);
				for (int ch = 2; ch < (int)numChannels; ++ch) { (void)readf(); }
			}
		}
	}
	else {
		statusMsg = "Unsupported WAV format";
		return false; // unsupported format
	}

	if (left.empty()) { statusMsg = "No audio frames"; return false; }
	// Normalize softly to avoid clipping
	float maxAbs = 0.f;
	for (float v : left) maxAbs = std::max(maxAbs, std::abs(v));
	for (float v : right) maxAbs = std::max(maxAbs, std::abs(v));
	if (maxAbs > 0.f) {
		float gain = 1.f / maxAbs;
		for (float &v : left) v *= gain;
		for (float &v : right) v *= gain;
	}

	sampleL = std::move(left);
	sampleR = std::move(right);
	fileSampleRate = (int)sRate;
	samplePath = path;
	playPos = 0.0;
    // Reset grains to avoid referencing old positions after a load
    for (auto &g : grains) g.active = false;
    spawnAccum = 0.0;
	bufferDirty = false;
	// Status
	std::string base = path;
	{
		size_t p = base.find_last_of('/');
		if (p != std::string::npos) base = base.substr(p + 1);
	}
	statusMsg = "Loaded: " + base;
	return true;
}

// Remove frames where both channels are below a threshold (absolute amplitude)


// Remove frames across the entire sample where both channels are below threshold
bool Grains::removeSilence(float threshold) {
	if (sampleL.empty()) { statusMsg = "No sample loaded"; return false; }
	// Remove low-amplitude frames across the entire sample
	std::vector<float> newL; newL.reserve(sampleL.size());
	std::vector<float> newR; newR.reserve(sampleR.size());
	for (size_t i = 0; i < sampleL.size(); ++i) {
		float vl = std::abs(sampleL[i]);
		float vr = (i < sampleR.size()) ? std::abs(sampleR[i]) : vl;
		if (vl >= threshold || vr >= threshold) {
			newL.push_back(sampleL[i]);
			newR.push_back((i < sampleR.size()) ? sampleR[i] : sampleL[i]);
		}
	}
	if (newL.empty()) {
		// Keep at least one zero sample to avoid edge cases
		sampleL.assign(1, 0.f);
		sampleR.assign(1, 0.f);
		statusMsg = "All silence removed";
	} else {
		sampleL = std::move(newL);
		sampleR = std::move(newR);
		statusMsg = "Silence removed";
	}
	// Reset playback/grains after destructive edit
	playPos = 0.0;
	for (auto &g : grains) g.active = false;
	spawnAccum = 0.0;
	// If embedding in patch, clear file path so next save re-embeds updated buffers
	if (embedInPatch) {
		samplePath.clear();
	}
	bufferDirty = true;
	return true;
}

// Zero out frames below threshold across entire sample to preserve length/pitch


// Normalize buffers by peak amplitude across both channels
bool Grains::normalizeSample() {
	if (sampleL.empty()) { statusMsg = "No sample loaded"; return false; }
	float maxAbs = 0.f;
	for (float v : sampleL) maxAbs = std::max(maxAbs, std::abs(v));
	for (float v : sampleR) maxAbs = std::max(maxAbs, std::abs(v));
	if (maxAbs <= 0.f) { statusMsg = "Already flat"; return false; }
	float gain = 1.f / maxAbs;
	for (float &v : sampleL) v *= gain;
	for (float &v : sampleR) v *= gain;
	statusMsg = "Normalized";
	// Reset playback/grains after destructive edit
	playPos = 0.0;
	for (auto &g : grains) g.active = false;
	spawnAccum = 0.0;
	if (embedInPatch) samplePath.clear();
	bufferDirty = true;
	return true;
}

// Load a random .wav from the same directory as the current samplePath
bool Grains::loadRandomSiblingSample() {
	if (samplePath.empty()) { statusMsg = "No current file"; return false; }
	// Determine directory from current path
	std::string dir;
	{
		size_t p = samplePath.find_last_of('/');
		if (p == std::string::npos) { statusMsg = "Folder not found"; return false; }
		dir = samplePath.substr(0, p);
	}
	DIR *dp = opendir(dir.c_str());
	if (!dp) { statusMsg = "Folder not found"; return false; }
	std::vector<std::string> wavs;
	struct dirent *de;
	while ((de = readdir(dp)) != nullptr) {
		if (!de->d_name || de->d_name[0] == '.') continue;
		std::string name = de->d_name;
		// Build full path
		std::string full = dir + "/" + name;
		// Check if regular file when d_type is unknown
		bool isFile = false;
#ifdef DT_REG
		if (de->d_type == DT_REG) isFile = true;
		else
#endif
		{
			struct stat st; if (stat(full.c_str(), &st) == 0 && S_ISREG(st.st_mode)) isFile = true;
		}
		if (!isFile) continue;
		// Check extension
		std::string ext;
		size_t dot = name.find_last_of('.');
		if (dot != std::string::npos) {
			ext = name.substr(dot);
			std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		}
		if (ext == ".wav") wavs.push_back(full);
	}
	closedir(dp);
	if (wavs.empty()) { statusMsg = "No WAVs in folder"; return false; }
	// Pick random index; try to avoid reloading the same file
	size_t idx = (size_t) std::floor(random::uniform() * wavs.size());
	if (wavs.size() > 1) {
		// Extract current filename
		std::string curName = samplePath.substr(samplePath.find_last_of('/') + 1);
		int guard = 8;
		while (guard-- > 0) {
			std::string pickName = wavs[idx].substr(wavs[idx].find_last_of('/') + 1);
			if (pickName != curName) break;
			idx = (size_t) std::floor(random::uniform() * wavs.size());
		}
	}
	return loadSampleFromPath(wavs[idx]);
}

// Waveform display
struct WaveformDisplay : TransparentWidget {
	Grains *module = nullptr;
	WaveformDisplay(Grains *m) { module = m; }
	bool dragging = false;
	float lastX = 0.f;
	void setPosFromX(float x) {
		if (!module || module->sampleL.empty()) return;
		float w = box.size.x;
		if (w <= 0.f) return;
		if (x < 0.f) x = 0.f; if (x > w) x = w;
		double N = (double)module->sampleL.size();
		module->playPos = (double)x / (double)w * std::max(0.0, N - 1.0);
	}
	void onButton(const event::Button &e) override {
		if (!module) return;
		if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
			if (e.action == GLFW_PRESS) {
				lastX = e.pos.x;
				setPosFromX(lastX);
				dragging = true;
				e.consume(this);
			}
			else if (e.action == GLFW_RELEASE) {
				dragging = false;
				e.consume(this);
			}
		}
	}
	void onDragMove(const event::DragMove &e) override {
		if (!dragging) return;
		lastX += e.mouseDelta.x;
		setPosFromX(lastX);
		e.consume(this);
	}
	void draw(const DrawArgs &args) override {
		NVGcontext *vg = args.vg;
		const float w = box.size.x;
		const float h = box.size.y;
		// Background
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, w, h);
		nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
		nvgFill(vg);
		// Border
		nvgBeginPath(vg);
		nvgRect(vg, 0.5f, 0.5f, w-1.f, h-1.f);
		nvgStrokeColor(vg, nvgRGBA(180, 180, 180, 160));
		nvgStrokeWidth(vg, 1.f);
		nvgStroke(vg);

		if (!module || module->sampleL.empty()) {
			// Placeholder text
			nvgFontSize(vg, 16.f);
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFillColor(vg, nvgRGBA(200, 200, 200, 180));
			nvgText(vg, w * 0.5f, h * 0.5f, module ? module->statusMsg.c_str() : "", NULL);
			return;
		}
		// Waveform
		// Left channel
		nvgBeginPath(vg);
		nvgStrokeColor(vg, nvgRGB(25, 150, 252));
		nvgStrokeWidth(vg, 1.5f);
		const size_t NL = module->sampleL.size();
		for (int x = 0; x < (int)w; ++x) {
			size_t i0 = (size_t)((double)x / (double)w * (double)NL);
			size_t span = std::max((size_t)1, (size_t)(NL / (size_t)w));
			float minV = 1.f, maxV = -1.f;
			for (size_t i = i0; i < std::min(i0 + span, NL); ++i) {
				float v = module->sampleL[i];
				minV = std::min(minV, v);
				maxV = std::max(maxV, v);
			}
			float y1 = h * (0.5f - 0.45f * maxV);
			float y2 = h * (0.5f - 0.45f * minV);
			nvgMoveTo(vg, (float)x, y1);
			nvgLineTo(vg, (float)x, y2);
		}
		nvgStroke(vg);
		// Right channel overlay
		nvgBeginPath(vg);
		nvgStrokeColor(vg, nvgRGB(25, 150, 252));
		nvgStrokeWidth(vg, 1.0f);
		const size_t NR = module->sampleR.size();
		for (int x = 0; x < (int)w; ++x) {
			size_t i0 = (size_t)((double)x / (double)w * (double)NR);
			size_t span = std::max((size_t)1, (size_t)(NR / (size_t)w));
			float minV = 1.f, maxV = -1.f;
			for (size_t i = i0; i < std::min(i0 + span, NR); ++i) {
				float v = module->sampleR[i];
				minV = std::min(minV, v);
				maxV = std::max(maxV, v);
			}
			float y1 = h * (0.5f - 0.45f * maxV);
			float y2 = h * (0.5f - 0.45f * minV);
			nvgMoveTo(vg, (float)x, y1);
			nvgLineTo(vg, (float)x, y2);
		}
		nvgStroke(vg);

		// Playback position line
		nvgBeginPath(vg);
		double N = (double)module->sampleL.size();
		float px = (float)((N > 0.0) ? (module->playPos / N * w) : 0.f);
		if (px < 0.f) px = 0.f; if (px > w - 1.f) px = w - 1.f;
		nvgMoveTo(vg, px, 0.f);
		nvgLineTo(vg, px, h);
		nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 200));
		nvgStrokeWidth(vg, 2.0f);
		nvgStroke(vg);

		// Grain dots overlay (cloud of dots)
		if (module) {
			const size_t NL = module->sampleL.size();
			const size_t NR = module->sampleR.size();
			if (NL > 0) {
				for (const auto &g : module->grains) {
					if (!g.active) continue;
					// X from grain position
					float gx = (float)(g.pos / (double)NL * w);
					if (gx < 0.f || gx > w) continue;
					// Y from current sample value (map like waveform)
					int i0 = (int)g.pos;
					int i1 = std::min(i0 + 1, (int)NL - 1);
		                if (i0 < 0 || i0 >= (int)NL) continue;
					double frac = g.pos - (double)i0;
					double sL = (1.0 - frac) * (double)module->sampleL[i0] + frac * (double)module->sampleL[i1];
					float gyL = h * (0.5f - 0.45f * (float)sL);
					// Draw left dot (white)
					nvgBeginPath(vg);
					nvgCircle(vg, gx, gyL, 3.0f);
					nvgFillColor(vg, nvgRGBA(255, 255, 255, 220));
					nvgFill(vg);
					// Draw right dot (white) if R exists
					if (NR == NL) {
						int j1 = std::min(i0 + 1, (int)NR - 1);
						double sR = (1.0 - frac) * (double)module->sampleR[i0] + frac * (double)module->sampleR[j1];
						float gyR = h * (0.5f - 0.45f * (float)sR);
						nvgBeginPath(vg);
						nvgCircle(vg, gx, gyR, 3.0f);
						nvgFillColor(vg, nvgRGBA(255, 255, 255, 220));
						nvgFill(vg);
					}
				}
			}
		}
	}
};

struct GrainsWidget : ModuleWidget {
	GrainsWidget(Grains *module);
	~GrainsWidget(){ 
	}
	void appendContextMenu(Menu *menu) override;
    void step() override;
private:
    bool randomBtnLatched = false;
};

GrainsWidget::GrainsWidget(Grains *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*40, RACK_GRID_HEIGHT);

	setPanel(createPanel(
		asset::plugin(pluginInstance, "res/Grains.svg"), 
		asset::plugin(pluginInstance, "res/Grains.svg")
	));

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));
	
	addParam(createParam<SmallButton>(Vec(485, 10), module, Grains::RANDOM_BUTTON));

	float topY = 342;
	
	addInput(createInput<TinyPJ301MPort>(Vec(85, topY), module, Grains::POSITION_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(141, topY), module, Grains::PITCH_INPUT));
	addParam(createParam<JwTinyKnob>(Vec(161, topY), module, Grains::GRAIN_PITCH_SEMI));
	
	addInput(createInput<TinyPJ301MPort>(Vec(200, topY), module, Grains::SIZE_CV));
	addParam(createParam<JwTinyKnob>(Vec(220, topY), module, Grains::GRAIN_SIZE_MS));
	addInput(createInput<TinyPJ301MPort>(Vec(259, topY), module, Grains::DENSITY_CV));
	addParam(createParam<JwTinyKnob>(Vec(279, topY), module, Grains::GRAIN_DENSITY));
	addInput(createInput<TinyPJ301MPort>(Vec(319, topY), module, Grains::SPREAD_CV));
	addParam(createParam<JwTinyKnob>(Vec(339, topY), module, Grains::GRAIN_SPREAD_MS));
	addParam(createParam<JwTinyKnob>(Vec(386, topY), module, Grains::WINDOW_TYPE));
	addInput(createInput<TinyPJ301MPort>(Vec(438, topY), module, Grains::PAN_CV));
	addParam(createParam<JwTinyKnob>(Vec(458, topY), module, Grains::PAN_RANDOMNESS));
	
	addOutput(createOutput<TinyPJ301MPort>(Vec(505, topY + 1), module, Grains::OUT_L));
	addOutput(createOutput<TinyPJ301MPort>(Vec(529, topY + 1), module, Grains::OUT_R));
	addParam(createParam<JwTinyKnob>(Vec(570, topY), module, Grains::GRAIN_GAIN));
	
	WaveformDisplay *disp = new WaveformDisplay(module);
	disp->box.pos = Vec(10, 40);
	disp->box.size = Vec(box.size.x - 20, box.size.y - 100);
	addChild(disp);


};

void GrainsWidget::appendContextMenu(Menu *menu) {
	MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);

	Grains *grains = dynamic_cast<Grains*>(module);
	// Toggle embed in patch
	struct EmbedInPatchItem : MenuItem {
		Grains *grains;
		void onAction(const event::Action &e) override { if (grains) grains->embedInPatch = !grains->embedInPatch; }
		void step() override { rightText = (grains && grains->embedInPatch) ? "✔" : ""; MenuItem::step(); }
	};
	EmbedInPatchItem *embedItem = new EmbedInPatchItem();
	embedItem->text = "Embed WAV in Patch";
	embedItem->grains = grains;
	menu->addChild(embedItem);

	// Toggle auto advance
	struct AutoAdvanceItem : MenuItem {
		Grains *grains;
		void onAction(const event::Action &e) override { if (grains) grains->autoAdvance = !grains->autoAdvance; }
		void step() override { rightText = (grains && grains->autoAdvance) ? "✔" : ""; MenuItem::step(); }
	};
	AutoAdvanceItem *advItem = new AutoAdvanceItem();
	advItem->text = "Auto-Advance Position";
	advItem->grains = grains;
	menu->addChild(advItem);
	// Silence threshold slider
	struct SilenceThresholdQuantity : Quantity {
		std::function<void(float)> setFn;
		std::function<float()> getFn;
		float def = 0.02f;
		void setValue(float value) override { if (setFn) setFn(clampfjw(value, getMinValue(), getMaxValue())); }
		float getValue() override { return getFn ? getFn() : def; }
		float getMinValue() override { return 0.0f; }
		float getMaxValue() override { return 1.0f; }
		float getDefaultValue() override { return def; }
		float getDisplayValue() override { return getValue(); }
		void setDisplayValue(float displayValue) override { setValue(displayValue); }
		int getDisplayPrecision() override { return 2; }
		std::string getLabel() override { return "Silence Threshold"; }
		std::string getUnit() override { return ""; }
		std::string getDisplayValueString() override { char buf[32]; snprintf(buf, sizeof(buf), "%.2f", getDisplayValue()); return std::string(buf); }
		void setDisplayValueString(std::string s) override { try { float v = std::stof(s); setValue(v); } catch (...) {} }
	};
	struct SilenceThresholdSlider : ui::Slider {
		SilenceThresholdSlider() { quantity = new SilenceThresholdQuantity; }
		~SilenceThresholdSlider() { delete quantity; }
	};
	MenuLabel *silenceLabel = new MenuLabel();
	silenceLabel->text = "Silence Threshold";
	menu->addChild(silenceLabel);
	SilenceThresholdSlider *thrSlider = new SilenceThresholdSlider();
	{
		auto qt = static_cast<SilenceThresholdQuantity*>(thrSlider->quantity);
		qt->getFn = [grains](){ return grains ? grains->silenceThreshold : 0.02f; };
		qt->setFn = [grains](float v){ if (grains) grains->silenceThreshold = v; };
	}
	thrSlider->box.size.x = 175.0f;
	menu->addChild(thrSlider);

	// Remove silence action
	struct RemoveSilenceItem : MenuItem {
		Grains *grains;
		void onAction(const event::Action &e) override {
			if (grains) grains->removeSilence(grains->silenceThreshold);
		}
	};
	RemoveSilenceItem *rem = new RemoveSilenceItem();
	rem->text = "Remove Silence";
	rem->grains = grains;
	menu->addChild(rem);

	// Removed: Trim Edge Silence action

	// Normalize WAV
	struct NormalizeItem : MenuItem {
		Grains *grains;
		void onAction(const event::Action &e) override {
			if (grains) grains->normalizeSample();
		}
	};
	NormalizeItem *norm = new NormalizeItem();
	norm->text = "Normalize WAV";
	norm->grains = grains;
	menu->addChild(norm);

	// Removed: Suppress Silence (preserve length/pitch)
	struct LoadWavItem : MenuItem {
		Grains *grains;
		void onAction(const event::Action &e) override {
			osdialog_filters *filters = osdialog_filters_parse("WAV:wav");
			char *path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, filters);
			osdialog_filters_free(filters);
			if (path) {
				std::string p = path;
				free(path);
				grains->loadSampleFromPath(p);
			}
		}
	};
	LoadWavItem *load = new LoadWavItem();
	load->text = "Load WAV...";
	load->grains = grains;
	menu->addChild(load);
}

void GrainsWidget::step() {
    ModuleWidget::step();
    Grains *m = dynamic_cast<Grains*>(module);
    if (!m) return;
    float v = m->params[Grains::RANDOM_BUTTON].getValue();
    if (v > 0.5f && !randomBtnLatched) {
        // Trigger random load on UI thread; reset button immediately
        m->loadRandomSiblingSample();
        m->params[Grains::RANDOM_BUTTON].setValue(0.f);
        randomBtnLatched = true;
    }
    else if (v <= 0.5f) {
        randomBtnLatched = false;
    }
}



Model *modelGrains = createModel<Grains, GrainsWidget>("Grains");
