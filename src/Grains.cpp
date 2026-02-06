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
#ifdef _WIN32
#include <windows.h>
#endif
#include "osdialog.h"
#include "system.hpp"

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
		REC_SWITCH,
		POSITION_KNOB,
		REVERSE_AMOUNT,
		AUTO_ADV_RATE,
		RATE_AMOUNT,
		AUTO_ADV_SWITCH,
		SYNC_SWITCH,
		NUM_PARAMS
	};
	enum InputIds {
		POSITION_INPUT,
		PITCH_INPUT,
		SIZE_CV,
		DENSITY_CV,
		SPREAD_CV,
		PAN_CV,
		REC_TOGGLE,
		REC_INPUT,
		CLOCK_INPUT,
		REVERSE_CV,
		RATE_CV,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_L,
		OUT_R,
		NUM_OUTPUTS
	};
	enum LightIds {
		REC_LIGHT,
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
	bool isRecording = false;
	bool normalPlayback = false;
	bool syncGrains = false;
	dsp::SchmittTrigger clockTrig;
	// Live recording baseline tracking to minimize post-record visual shift
	double recSumL = 0.0;
	double recSumR = 0.0;
	uint64_t recCount = 0;
	// UI override: when dragging the playhead in the display, ignore knob/CV position writes
	bool uiDraggingPlayhead = false;

	// DC blocker state to suppress clicks/pops due to DC offsets
	float dcYL = 0.f, dcYR = 0.f;      // previous HPF output per channel
	float dcPrevXL = 0.f, dcPrevXR = 0.f; // previous input sample per channel
	float dcR = 0.995f;                // HPF coefficient (close to 1)

	// Output transition envelope to suppress clicks when switching record/playback
	float playTransEnv = 1.f;      // current envelope value (0..1)
	int playTransHold = 0;         // samples to hold at 0 before rising
	int playTransRelRemain = 0;    // samples remaining in release (fade-out)
	int playTransAtkRemain = 0;    // samples remaining in attack (fade-in)
	float playTransRelStep = 0.f;  // per-sample decrement during release
	float playTransAtkStep = 0.f;  // per-sample increment during attack

	// Position jump de-click envelope (for sudden playhead jumps)
	double lastPlayPosForJump = 0.0;
	int posJumpRemain = 0;
	float posJumpEnv = 1.f;
	float posJumpStep = 0.f;

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
	bool saveBufferToWav(const std::string &path);
	// Base64 helpers for embedding samples in patch JSON
	static std::string b64Encode(const uint8_t* data, size_t len);
	static std::vector<uint8_t> b64Decode(const std::string& str);

	Grains() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configOutput(OUT_L, "Audio L");
		configOutput(OUT_R, "Audio R");
		configInput(POSITION_INPUT, "Position CV (0–10V)");
		configParam(POSITION_KNOB, 0.f, 1.f, 0.5f, "Position");
		configInput(PITCH_INPUT, "Pitch CV (1V/Oct)");
		configInput(SIZE_CV, "Grain Size CV (0–10V)");
		configInput(DENSITY_CV, "Grain Density CV (0–10V) Active if Rate is 0V");
		configInput(RATE_CV, "Rate CV (0–10V) When > 0 Density is Rate");
		configInput(SPREAD_CV, "Grain Spread CV (0–10V)");
		configInput(PAN_CV, "Pan Randomness CV (0–10V)");
		configInput(REC_TOGGLE, "Record Toggle (gate)");
		configInput(REC_INPUT, "Record In (audio)");
		configInput(CLOCK_INPUT, "Clock (gate)");
		configInput(REVERSE_CV, "Reverse Amount CV (0–10V)");
		configParam(GRAIN_SIZE_MS, 5.f, 2000.f, 50.f, "Grain size", " ms");
		configParam(GRAIN_DENSITY, 0.f, 200.f, 50.f, "Grain density (Active if Rate is 0)", " grains/s");
		configParam(RATE_AMOUNT, 0.f, 200.f, 0.f, "Grain rate (When > 0 Density is Rate)", " grains/s");
		configParam(AUTO_ADV_RATE, 0.f, 4.f, 1.f, "Auto-advance speed", " x");
		configParam(GRAIN_SPREAD_MS, 0.f, 2000.f, 50.f, "Grain spread", " ms");
		configSwitch(AUTO_ADV_SWITCH, 0.f, 1.f, 0.f, "Auto-advance");
		configSwitch(SYNC_SWITCH, 0.f, 1.f, 0.f, "Sync to clock");
		configParam(GRAIN_PITCH_SEMI, -24.f, 24.f, 0.f, "Pitch", " semitones");
		configParam(GRAIN_GAIN, 0.f, 1.f, 0.8f, "Output gain");
		configSwitch(WINDOW_TYPE, 0.f, 3.f, 0.f, "Window type",
			{"Hann – smooth cosine taper",
			 "Hamming – similar, slightly brighter",
			 "Triangular – linear ramps",
			 "Rectangular – sharp (soft 2% edges)"});
		configParam(PAN_RANDOMNESS, 0.f, 1.f, 0.f, "Pan randomness");
		configParam(REVERSE_AMOUNT, 0.f, 1.f, 0.f, "Reverse grains amount");
		configButton(RANDOM_BUTTON, "Random sample");
		configSwitch(REC_SWITCH, 0.f, 1.f, 0.f, "Record");
		grains.resize(128);
	}

	void process(const ProcessArgs &args) override;
	void onAdd(const AddEvent& e) override;
	void onSave(const SaveEvent& e) override;

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		if (!samplePath.empty()) {
			json_object_set_new(rootJ, "path", json_string(samplePath.c_str()));
		}
		json_object_set_new(rootJ, "autoAdvance", json_boolean(autoAdvance));
		json_object_set_new(rootJ, "playPos", json_real(playPos));
		json_object_set_new(rootJ, "normalPlayback", json_boolean(normalPlayback));
		json_object_set_new(rootJ, "syncGrains", json_boolean(syncGrains));

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
		else {
			// No external path saved; try to restore from patch storage (recorded clip)
			std::string dir = getPatchStorageDirectory();
			if (!dir.empty()) {
				std::string p = rack::system::join(dir, "recording.wav");
				std::ifstream f(p, std::ios::binary);
				if (f.good()) {
					f.close();
					if (loadSampleFromPath(p)) {
						// Clear path so we don't serialize a patch-storage absolute path
						samplePath.clear();
						if (savedPlayPos >= 0.0) {
							double maxPos = (!sampleL.empty()) ? (double)sampleL.size() - 1.0 : 0.0;
							playPos = std::min(std::max(0.0, savedPlayPos), maxPos);
						}
					}
				}
			}
		}
		json_t *autoAdvJ = json_object_get(rootJ, "autoAdvance");
		json_t *normalJ = json_object_get(rootJ, "normalPlayback");
		json_t *syncJ = json_object_get(rootJ, "syncGrains");
		if (autoAdvJ && json_is_boolean(autoAdvJ)) autoAdvance = json_boolean_value(autoAdvJ);
		if (normalJ && json_is_boolean(normalJ)) normalPlayback = json_boolean_value(normalJ);
		if (syncJ && json_is_boolean(syncJ)) syncGrains = json_boolean_value(syncJ);
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
	// Handle recording toggle and capture first
	bool recOn = params[REC_SWITCH].getValue() > 0.5f
		|| (inputs[REC_TOGGLE].isConnected() && inputs[REC_TOGGLE].getVoltage() > 0.5f);
	if (recOn && !isRecording) {
		// Start a new recording session
		// Begin a short fade-out to avoid clicks on transition
		playTransRelRemain = (int)std::round(0.008 * args.sampleRate); // ~8ms release
		playTransRelStep = (playTransRelRemain > 0) ? (playTransEnv / (float)playTransRelRemain) : playTransEnv;
		playTransHold = (int)std::round(0.01 * args.sampleRate); // ~10ms hold at 0
		playTransAtkRemain = 0;
		isRecording = true;
		sampleL.clear();
		sampleR.clear();
		fileSampleRate = (int)args.sampleRate;
		samplePath.clear();
				// Reset DC blocker state to avoid pops when switching to monitor
				dcYL = dcYR = 0.f; dcPrevXL = dcPrevXR = 0.f;
		// Reset live recording accumulators
		recSumL = 0.0; recSumR = 0.0; recCount = 0;
		// Pre-reserve buffer to minimize reallocations during recording (reduces pops)
		{
			size_t reserveFrames = (size_t)std::max(1.0, std::round(args.sampleRate * 60.0)); // ~60s
			sampleL.reserve(reserveFrames);
			sampleR.reserve(reserveFrames);
		}
		playPos = 0.0;
		for (auto &g : grains) g.active = false;
		spawnAccum = 0.0;
		bufferDirty = false;
		statusMsg = "Recording...";
	}
	else if (!recOn && isRecording) {
		// Stop recording
		// Begin a short fade-out to avoid clicks on transition
		playTransRelRemain = (int)std::round(0.008 * args.sampleRate); // ~8ms release
		playTransRelStep = (playTransRelRemain > 0) ? (playTransEnv / (float)playTransRelRemain) : playTransEnv;
		playTransHold = (int)std::round(0.01 * args.sampleRate); // ~10ms hold at 0
		playTransAtkRemain = 0;
		isRecording = false;
		bufferDirty = true;
		for (auto &g : grains) g.active = false;
		spawnAccum = 0.0;
		// Remove DC bias from recorded buffers and align playhead to a near zero-cross
		{
			if (!sampleL.empty()) {
				// Remove DC bias (mean) from L/R
				double sumL = 0.0, sumR = 0.0; size_t N = sampleL.size();
				for (size_t i = 0; i < N; ++i) { sumL += sampleL[i]; sumR += sampleR[i]; }
				double meanL = sumL / (double)N; double meanR = sumR / (double)N;
				for (size_t i = 0; i < N; ++i) { sampleL[i] = (float)(sampleL[i] - meanL); sampleR[i] = (float)(sampleR[i] - meanR); }
				// Find a zero-cross near the start to avoid an initial pop
				int bestIdx = 0; float bestAbs = std::abs(sampleL[0]);
				int scan = std::min<int>(2048, (int)N - 1);
				for (int i = 1; i <= scan; ++i) {
					float a = sampleL[i - 1]; float b = sampleL[i];
					if ((a <= 0.f && b >= 0.f) || (a >= 0.f && b <= 0.f)) { bestIdx = i; break; }
					float ab = std::abs(b); if (ab < bestAbs) { bestAbs = ab; bestIdx = i; }
				}
				playPos = (double)bestIdx;
				// Apply a short fade-in for the playhead jump to avoid a click
				posJumpRemain = (int)std::round(0.003 * args.sampleRate); // ~3ms
				posJumpEnv = 0.f;
				posJumpStep = (posJumpRemain > 0) ? (1.f / (float)posJumpRemain) : 1.f;
			}
			else {
				playPos = 0.0;
			}
		}
		// Reset DC blocker state to avoid pops when switching to playback
		dcYL = dcYR = 0.f; dcPrevXL = dcPrevXR = 0.f;
		statusMsg = "Recording stopped";
	}

	if (isRecording && inputs[REC_INPUT].isConnected()) {
		// Append one frame; assume mono input, map ±5V to ±1 (Rack audio convention)
		float v = inputs[REC_INPUT].getVoltage();
		// Normalize from ±5V to ±1. If users send ±10V, the clamp still protects.
		float s = std::max(-1.f, std::min(1.f, v / 5.f));
		sampleL.push_back(s);
		sampleR.push_back(s);
		// Update live baseline trackers
		recSumL += (double)s;
		recSumR += (double)s;
		recCount++;
	}

	// During recording, always monitor REC_INPUT and do not play back the buffer to avoid clicks.
	if (isRecording) {
		if (inputs[REC_INPUT].isConnected()) {
			float v = inputs[REC_INPUT].getVoltage();
			float s = std::max(-1.f, std::min(1.f, v / 5.f));
			// Simple DC blocker
			double fc = 10.0;
			double r = 1.0 - (2.0 * M_PI * fc / args.sampleRate);
			if (r < 0.90) r = 0.90; if (r > 0.9999) r = 0.9999;
			dcR = (float)r;
			double y = (double)(s - (double)dcPrevXL) + (double)dcR * (double)dcYL;
			dcPrevXL = s; dcYL = (float)y;
			float og = (float)(params[GRAIN_GAIN].getValue() * playTransEnv * (double)posJumpEnv * 5.0);
			outputs[OUT_L].setVoltage((float)(y * og));
			outputs[OUT_R].setVoltage((float)(y * og));
		}
		else {
			outputs[OUT_L].setVoltage(0.f);
			outputs[OUT_R].setVoltage(0.f);
		}
		lights[REC_LIGHT].setBrightness(1.0f);
		return;
	}

	// Read auto-advance state from the new front-panel switch
	autoAdvance = params[AUTO_ADV_SWITCH].getValue() > 0.5f;
	// Read sync-to-clock switch
	syncGrains = params[SYNC_SWITCH].getValue() > 0.5f;

	// Position: knob is base, CV adds an offset (in 0..1 per 0..10V)
	// During auto-advance, ignore external position unless explicitly allowed with CV.
	if (!uiDraggingPlayhead) {
		bool cvConn = inputs[POSITION_INPUT].isConnected();
		// If CV is connected, always follow external position; otherwise, follow knob only when not auto-advancing
		if (cvConn || !autoAdvance) {
			float f = std::max(0.f, std::min(1.f, params[POSITION_KNOB].getValue()));
			if (cvConn) {
				float v = inputs[POSITION_INPUT].getVoltage();
				float offset = v / 10.f; // allow negative CV to subtract
				f += offset;
				if (f < 0.f) f = 0.f;
				if (f > 1.f) f = 1.f;
			}
			double newPos = (double)f * std::max(0.0, (double)sampleL.size() - 1.0);
			// Detect sudden jumps and trigger a short fade-in to de-click
			double jumpThresh = std::max(64.0, 0.002 * (double)fileSampleRate); // >=64 frames or ~2ms
			if (std::abs(newPos - lastPlayPosForJump) > jumpThresh) {
				posJumpRemain = (int)std::round(0.003 * args.sampleRate); // ~3ms fade-in
				posJumpEnv = 0.f;
				posJumpStep = (posJumpRemain > 0) ? (1.f / (float)posJumpRemain) : 1.f;
			}
			playPos = newPos;
		}
	}

	// Update position jump envelope
	if (posJumpRemain > 0) {
		posJumpEnv = std::min(1.f, posJumpEnv + posJumpStep);
		posJumpRemain--;
	}
	lastPlayPosForJump = playPos;
	// Update transition envelope: release -> hold -> attack
	{
		if (playTransRelRemain > 0) {
			playTransEnv = std::max(0.f, playTransEnv - playTransRelStep);
			playTransRelRemain--;
		}
		else if (playTransHold > 0) {
			playTransEnv = 0.f;
			playTransHold--;
			if (playTransHold == 0) {
				playTransAtkRemain = (int)std::round(0.012 * args.sampleRate); // ~12ms attack
				playTransAtkStep = (playTransAtkRemain > 0) ? (1.f / (float)playTransAtkRemain) : 1.f;
			}
		}
		else if (playTransAtkRemain > 0) {
			playTransEnv = std::min(1.f, playTransEnv + playTransAtkStep);
			playTransAtkRemain--;
		}
		else {
			playTransEnv = 1.f;
		}
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
		// Dedicated rate control
		double rateParam = params[RATE_AMOUNT].getValue(); // knob
		double rate = rateParam;
		if (inputs[RATE_CV].isConnected()) {
			double v = inputs[RATE_CV].getVoltage();
			rate = std::max(denMin, std::min(denMax, rateParam + v * (denMax - denMin) / 10.0));
		}
		// When not synced, rate sets density (grains/s). When synced, rate is interpreted as grains-per-clock multiple.
		if (!syncGrains || !inputs[CLOCK_INPUT].isConnected()) {
			if (inputs[RATE_CV].isConnected() || rateParam > 0.0) {
				density = rate;
			}
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

	// Reverse amount: 0 = none reversed, 1 = all reversed (probability)
	double reverseAmt = params[REVERSE_AMOUNT].getValue();
	if (inputs[REVERSE_CV].isConnected()) {
		double v = inputs[REVERSE_CV].getVoltage();
		reverseAmt = std::max(0.0, std::min(1.0, reverseAmt + v / 10.0));
	}

	// Compute an effective playback size when recording to avoid reading the tail being written
	int sizeL = (int)sampleL.size();
	int effSize = sizeL;
	if (isRecording && fileSampleRate > 0) {
		int guard = std::max(1, (int)std::round(0.01 * (double)fileSampleRate)); // ~10ms tail guard
		if (effSize > guard) effSize -= guard;
	}

	// Normal playback mode: directly read the sample at playPos
	if (normalPlayback) {
		int i0 = (int)playPos;
		if (i0 < 0) i0 = 0;
		if (i0 >= effSize) i0 = std::max(0, effSize - 1);
		int i1 = std::min(i0 + 1, std::max(0, effSize - 1));
		double frac = playPos - (double)i0;
		double sL = (1.0 - frac) * (double)sampleL[i0] + frac * (double)sampleL[i1];
		double sR = (1.0 - frac) * (double)sampleR[i0] + frac * (double)sampleR[i1];
		// Gentle equal-power crossfade near wrap to suppress clicks when auto-advancing
		if (autoAdvance && effSize > 1) {
			int xfade = 256; // ~5.8ms at 44.1kHz
			if (xfade > effSize - 1) xfade = effSize - 1;
			double distToEnd = (double)(effSize - 1) - playPos;
			if (distToEnd > 0.0 && distToEnd < (double)xfade) {
				double w = distToEnd / (double)xfade; // 1->0 approaching wrap
				double a = std::sin(0.5 * M_PI * w);
				double b = std::cos(0.5 * M_PI * w);
				// Head sample from wrapped position
				double headPos = playPos - ((double)effSize - 1.0);
				if (headPos < 0.0) headPos = 0.0; // clamp
				int h0 = (int)headPos;
				int h1 = std::min(h0 + 1, std::max(0, effSize - 1));
				double hfrac = headPos - (double)h0;
				double hL = (1.0 - hfrac) * (double)sampleL[h0] + hfrac * (double)sampleL[h1];
				double hR = (1.0 - hfrac) * (double)sampleR[h0] + hfrac * (double)sampleR[h1];
				sL = a * sL + b * hL;
				sR = a * sR + b * hR;
			}
		}
		// Apply simple DC blocker HPF to reduce pops from DC offsets
		double srHost = args.sampleRate; // local sample rate
		double fc = 10.0; // cutoff ~10 Hz
		double r = 1.0 - (2.0 * M_PI * fc / srHost);
		if (r < 0.90) r = 0.90; if (r > 0.9999) r = 0.9999;
		dcR = (float)r;
		double yL = (double)(sL - (double)dcPrevXL) + (double)dcR * (double)dcYL;
		double yR = (double)(sR - (double)dcPrevXR) + (double)dcR * (double)dcYR;
		dcPrevXL = (float)sL; dcPrevXR = (float)sR;
		dcYL = (float)yL; dcYR = (float)yR;
		float og = (float)(gain * playTransEnv * (double)posJumpEnv * 5.0);
		outputs[OUT_L].setVoltage((float)(yL * og));
		outputs[OUT_R].setVoltage((float)(yR * og));
		if (autoAdvance && !inputs[POSITION_INPUT].isConnected()) {
			double advMul = std::max(0.0, (double)params[AUTO_ADV_RATE].getValue());
			double step = pitch * (double)fileSampleRate / srHost * advMul;
			playPos += step;
			if (playPos >= (double)effSize) playPos = 0.0;
		}
		return;
	}

	if (syncGrains && inputs[CLOCK_INPUT].isConnected()) {
		// Compute grains-per-tick from Rate knob/CV as a multiplier of the clock.
		int grainsPerTick = 1;
		{
			double rateParam = params[RATE_AMOUNT].getValue();
			double rateVal = rateParam;
			if (inputs[RATE_CV].isConnected()) {
				double v = inputs[RATE_CV].getVoltage();
				const double denMin = 0.0, denMax = 200.0;
				rateVal = std::max(denMin, std::min(denMax, rateParam + v * (denMax - denMin) / 10.0));
			}
			if (inputs[RATE_CV].isConnected() || rateParam > 0.0) {
				grainsPerTick = (int)std::lround(rateVal);
				if (grainsPerTick <= 0) grainsPerTick = 1;
				if (grainsPerTick > 32) grainsPerTick = 32; // guard against extreme bursts
			} else {
				grainsPerTick = 1;
			}
		}
		// On rising clock edge, spawn N grains immediately (spread/jitter applies to start).
		if (clockTrig.process(inputs[CLOCK_INPUT].getVoltage())) {
			for (int n = 0; n < grainsPerTick; ++n) {
				for (auto &g : grains) {
					if (!g.active) {
						double spreadFrames = spreadMs * (double)fileSampleRate / 1000.0;
						double offset = (random::uniform() * 2.0 - 1.0) * spreadFrames;
						double start = playPos + offset;
						if (start < 0.0) start = 0.0;
						if (start > (double)effSize - 1.0) start = std::max(0.0, (double)effSize - 1.0);
						g.pos = start;
						g.step = pitch * (double)fileSampleRate / srHost;
						if (random::uniform() < reverseAmt) g.step = -g.step;
						// Duration should be in host samples for a stable window length
						g.dur = (int)std::max(1.0, grainSizeMs * srHost / 1000.0);
						g.age = 0;
						g.active = true;
						g.pan = (float)((random::uniform() * 2.0 - 1.0) * panAmount);
						break;
					}
				}
			}
		}
	}
	else {
		spawnAccum += density / srHost;
		while (spawnAccum >= 1.0) {
			spawnAccum -= 1.0;
			for (auto &g : grains) {
				if (!g.active) {
					double spreadFrames = spreadMs * (double)fileSampleRate / 1000.0;
					double offset = (random::uniform() * 2.0 - 1.0) * spreadFrames;
					double start = playPos + offset;
					if (start < 0.0) start = 0.0;
					if (start > (double)effSize - 1.0) start = std::max(0.0, (double)effSize - 1.0);
					g.pos = start;
					g.step = pitch * (double)fileSampleRate / srHost;
					if (random::uniform() < reverseAmt) g.step = -g.step;
					// Duration should be in host samples for a stable window length
					g.dur = (int)std::max(1.0, grainSizeMs * srHost / 1000.0);
					g.age = 0;
					g.active = true;
					g.pan = (float)((random::uniform() * 2.0 - 1.0) * panAmount);
					break;
				}
			}
		}
	}

	double outL = 0.0;
	double outR = 0.0;
	int wtype = (int) std::round(params[WINDOW_TYPE].getValue());
	for (auto &g : grains) {
		if (!g.active) continue;
		// Clamp position within buffer to avoid abrupt hard-stops
		double posClamped = g.pos;
		if (posClamped < 0.0) posClamped = 0.0;
		else if (posClamped > (double)effSize - 1.0) posClamped = std::max(0.0, (double)effSize - 1.0);
		int i0 = (int)posClamped;
		int i1 = std::min(i0 + 1, (int)sampleL.size() - 1);
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
			case 3: // Rectangular (softened edges to reduce clicks)
			{
				double e = 0.02; // 2% edges
				if (t < e) w = t / e;
				else if (t > 1.0 - e) w = (1.0 - t) / e;
				else w = 1.0;
				break;
			}
		}
		// Compute equal-power pan weights from g.pan in [-1,1]
		double theta = (g.pan + 1.0) * 0.5 * 1.5707963267948966; // 0..pi/2
		double wL = std::cos(theta);
		double wR = std::sin(theta);
		outL += sMono * w * wL;
		outR += sMono * w * wR;
		g.pos += g.step;
		// If grain hits buffer edges before its window ends, shorten its duration and clamp
		if (g.pos < 0.0 || g.pos >= (double)effSize) {
			int edgeFade = (int)std::round(0.002 * srHost); // ~2ms fade-out
			int targetDur = g.age + edgeFade;
			if (targetDur < g.dur) g.dur = targetDur;
			if (g.pos < 0.0) g.pos = 0.0;
			else if (g.pos >= (double)effSize) g.pos = std::max(0.0, (double)effSize - 1.0);
		}
		g.age++;
		if (g.age >= g.dur) g.active = false;
	}

	// Apply DC blocker on granular output as well
	double srHost2 = args.sampleRate;
	double fc2 = 10.0;
	double r2 = 1.0 - (2.0 * M_PI * fc2 / srHost2);
	if (r2 < 0.90) r2 = 0.90; if (r2 > 0.9999) r2 = 0.9999;
	dcR = (float)r2;
	double yLg = (double)(outL - (double)dcPrevXL) + (double)dcR * (double)dcYL;
	double yRg = (double)(outR - (double)dcPrevXR) + (double)dcR * (double)dcYR;
	dcPrevXL = (float)outL; dcPrevXR = (float)outR;
	dcYL = (float)yLg; dcYR = (float)yRg;
	float og2 = (float)(gain * playTransEnv * (double)posJumpEnv * 5.0);
	outputs[OUT_L].setVoltage((float)(yLg * og2));
	outputs[OUT_R].setVoltage((float)(yRg * og2));

	if (autoAdvance && !inputs[POSITION_INPUT].isConnected()) {
		double advMul = std::max(0.0, (double)params[AUTO_ADV_RATE].getValue());
		double step = (double)fileSampleRate / srHost * advMul;
		playPos += step;
		if (playPos >= (double)effSize) playPos = 0.0;
	}
	// Update recording LED
	lights[REC_LIGHT].setBrightness(isRecording ? 1.0f : 0.0f);
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

static void writeU32(std::ofstream &out, uint32_t v) {
	char b[4];
	b[0] = (char)(v & 0xFF);
	b[1] = (char)((v >> 8) & 0xFF);
	b[2] = (char)((v >> 16) & 0xFF);
	b[3] = (char)((v >> 24) & 0xFF);
	out.write(b, 4);
}
static void writeU16(std::ofstream &out, uint16_t v) {
	char b[2];
	b[0] = (char)(v & 0xFF);
	b[1] = (char)((v >> 8) & 0xFF);
	out.write(b, 2);
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
		size_t p = base.find_last_of("/\\");
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
	isRecording = false;
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

// Save current buffer to a stereo PCM16 WAV file
bool Grains::saveBufferToWav(const std::string &path) {
	if (sampleL.empty()) { statusMsg = "No sample loaded"; return false; }
	size_t frames = std::min(sampleL.size(), sampleR.size());
	if (frames == 0) { statusMsg = "No audio frames"; return false; }
	uint16_t numChannels = 2;
	uint16_t bitsPerSample = 16;
	uint32_t sRate = (fileSampleRate > 0) ? (uint32_t)fileSampleRate : 44100u;
	uint16_t bytesPerSample = bitsPerSample / 8; // 2
	uint16_t blockAlign = numChannels * bytesPerSample; // 4
	uint32_t byteRate = sRate * blockAlign;
	uint32_t dataSize = (uint32_t)(frames * blockAlign);

	std::ofstream out(path, std::ios::binary);
	if (!out.good()) { statusMsg = "Could not write file"; return false; }

	// RIFF header
	out.write("RIFF", 4);
	writeU32(out, 36u + dataSize); // file size minus 8
	out.write("WAVE", 4);
	// fmt chunk
	out.write("fmt ", 4);
	writeU32(out, 16u); // PCM fmt chunk size
	writeU16(out, 1u); // PCM
	writeU16(out, numChannels);
	writeU32(out, sRate);
	writeU32(out, byteRate);
	writeU16(out, blockAlign);
	writeU16(out, bitsPerSample);
	// data chunk
	out.write("data", 4);
	writeU32(out, dataSize);
	// samples
	for (size_t i = 0; i < frames; ++i) {
		float fl = std::max(-1.f, std::min(1.f, sampleL[i]));
		float fr = std::max(-1.f, std::min(1.f, sampleR[i]));
		int sl = (int)std::lround(fl * 32767.0f);
		int sr = (int)std::lround(fr * 32767.0f);
		if (sl < -32768) sl = -32768; if (sl > 32767) sl = 32767;
		if (sr < -32768) sr = -32768; if (sr > 32767) sr = 32767;
		writeU16(out, (uint16_t)(uint16_t)(sl & 0xFFFF));
		writeU16(out, (uint16_t)(uint16_t)(sr & 0xFFFF));
	}
	out.flush();
	bool ok = out.good();
	out.close();
	if (ok) {
		// Update status
		std::string base = path;
		size_t p = base.find_last_of("/\\");
		if (p != std::string::npos) base = base.substr(p + 1);
		statusMsg = "Saved: " + base;
	} else {
		statusMsg = "Write error";
	}
	return ok;
}

// Load any previously-saved audio from the patch storage directory
void Grains::onAdd(const AddEvent& e) {
	Module::onAdd(e);
	std::string dir = getPatchStorageDirectory();
	if (!dir.empty()) {
		std::string path = rack::system::join(dir, "recording.wav");
		std::ifstream f(path, std::ios::binary);
		if (f.good()) {
			f.close();
			if (loadSampleFromPath(path)) {
				// Avoid persisting the absolute path of patch-storage audio in JSON
				samplePath.clear();
			}
		}
	}
}

// Save current buffer as WAV into the patch storage directory
void Grains::onSave(const SaveEvent& e) {
	Module::onSave(e);
	if (!sampleL.empty()) {
		std::string dir = createPatchStorageDirectory();
		if (!dir.empty()) {
			std::string path = rack::system::join(dir, "recording.wav");
			saveBufferToWav(path);
		}
	}
}

// Load a random .wav from the same directory as the current samplePath
bool Grains::loadRandomSiblingSample() {
	if (samplePath.empty()) { statusMsg = "No current file"; return false; }
	// Determine directory from current path (handle both '/' and '\\')
	std::string dir;
	{
		size_t p = samplePath.find_last_of("/\\");
		if (p == std::string::npos) { statusMsg = "Folder not found"; return false; }
		dir = samplePath.substr(0, p);
	}
	std::vector<std::string> wavs;
#ifdef _WIN32
	// Build pattern: dir\\*.wav
	std::string pattern = dir;
	if (!pattern.empty()) {
		char last = pattern.back();
		if (last != '/' && last != '\\') pattern += '\\';
	}
	pattern += "*.wav";
	WIN32_FIND_DATAA ffd;
	HANDLE hFind = FindFirstFileA(pattern.c_str(), &ffd);
	if (hFind == INVALID_HANDLE_VALUE) {
		statusMsg = "No WAVs in folder";
		return false;
	}
	do {
		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
		std::string name = ffd.cFileName;
		// Full path
		std::string full = dir;
		char last = full.empty() ? '\\' : full.back();
		if (last != '/' && last != '\\') full += '\\';
		full += name;
		wavs.push_back(full);
	} while (FindNextFileA(hFind, &ffd));
	FindClose(hFind);
#else
	DIR *dp = opendir(dir.c_str());
	if (!dp) { statusMsg = "Folder not found"; return false; }
	struct dirent *de;
	while ((de = readdir(dp)) != nullptr) {
		// d_name is a fixed array in dirent; only check leading '.'
		if (de->d_name[0] == '.') continue;
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
		// Check extension case-insensitive
		size_t dot = name.find_last_of('.');
		if (dot != std::string::npos) {
			std::string ext = name.substr(dot);
			std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
			if (ext == ".wav") wavs.push_back(full);
		}
	}
	closedir(dp);
#endif
	if (wavs.empty()) { statusMsg = "No WAVs in folder"; return false; }
	// Pick random index; try to avoid reloading the same file
	size_t idx = (size_t) std::floor(random::uniform() * wavs.size());
	if (wavs.size() > 1) {
		// Extract current filename (handle both separators)
		size_t p = samplePath.find_last_of("/\\");
		std::string curName = (p == std::string::npos) ? samplePath : samplePath.substr(p + 1);
		int guard = 8;
		while (guard-- > 0) {
			size_t q = wavs[idx].find_last_of("/\\");
			std::string pickName = (q == std::string::npos) ? wavs[idx] : wavs[idx].substr(q + 1);
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
	// Fixed time window to display during recording to prevent shifting/compression
	float recordWindowSecs = 8.0f; // unused when drawing full buffer; keep for future
	// Stable DC baseline during a recording session to prevent vertical shifting
	bool prevRec = false; // kept for potential future use
	double dcMeanL = 0.0;
	double dcMeanR = 0.0;
	// Compute once per recording session to avoid baseline drift
	void setPosFromX(float x) {
		if (!module || module->sampleL.empty()) return;
		float w = box.size.x;
		if (w <= 0.f) return;
		if (x < 0.f) x = 0.f; if (x > w) x = w;
		// Map drag across the full visible buffer (excluding guard during recording)
		int fs = module->fileSampleRate > 0 ? module->fileSampleRate : 44100;
		int guard = std::max(1, (int)std::round(0.01 * (double)fs));
		double Nfull = (double)module->sampleL.size();
		double Ndraw = Nfull;
		if (module->isRecording && Nfull > (double)guard) Ndraw = Nfull - (double)guard;
		double denom = std::max(1.0, Ndraw - 1.0);
		module->playPos = (double)x / (double)w * denom;
	}
	void onButton(const event::Button &e) override {
		if (!module) return;
		if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
			if (e.action == GLFW_PRESS) {
				lastX = e.pos.x;
				setPosFromX(lastX);
				dragging = true;
				module->uiDraggingPlayhead = true;
				e.consume(this);
			}
			else if (e.action == GLFW_RELEASE) {
				dragging = false;
				module->uiDraggingPlayhead = false;
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
		// Determine lengths; in recording, exclude tail-guard and draw the full visible buffer
		size_t NLfull = module->sampleL.size();
		int fs = module->fileSampleRate > 0 ? module->fileSampleRate : 44100;
		int guard = std::max(1, (int)std::round(0.01 * (double)fs)); // ~10ms
		size_t NLdraw = NLfull;
		if (module->isRecording && NLfull > (size_t)guard) NLdraw = NLfull - (size_t)guard;
		// Full segment indices
		size_t winStart = 0;
		size_t winEnd = NLdraw;

		// Compute DC baseline from live accumulators during recording (cheap, stable)
		if (module->isRecording) {
			if (module->recCount > 0) {
				dcMeanL = module->recSumL / (double)module->recCount;
				dcMeanR = module->recSumR / (double)module->recCount;
			}
		}
		else {
			dcMeanL = 0.0; dcMeanR = 0.0;
		}

		// Left channel (bucketed min/max per pixel with clamping) over window
		nvgBeginPath(vg);
		nvgStrokeColor(vg, nvgRGB(25, 150, 252));
		nvgStrokeWidth(vg, 1.5f);
		const size_t NL = NLdraw;
		const size_t wpxL = (size_t)std::max(1.0f, std::floor(w));
		const size_t bucketL = std::max<size_t>(1, (size_t)std::floor((double)NL / (double)std::max<size_t>(1, wpxL)));
		for (size_t xpix = 0; xpix < wpxL; ++xpix) {
			size_t i0 = (size_t)std::floor((double)xpix * (double)NL / (double)wpxL);
			if (i0 >= NL) i0 = NL - 1;
			size_t iEnd = std::min(NL, i0 + bucketL);
			float minV = 1.f, maxV = -1.f;
			for (size_t i = i0; i < iEnd; ++i) {
				float v = module->sampleL[winStart + i];
				if (module->isRecording) v -= (float)dcMeanL;
				// Clamp to display range symmetrically
				if (v < -1.f) v = -1.f; if (v > 1.f) v = 1.f;
				minV = std::min(minV, v);
				maxV = std::max(maxV, v);
			}
			minV = std::max(-1.f, std::min(1.f, minV));
			maxV = std::max(-1.f, std::min(1.f, maxV));
			if (maxV - minV < 1e-6f) { maxV += 0.01f; minV -= 0.01f; }
			float y1 = h * (0.5f - 0.44f * maxV);
			float y2 = h * (0.5f - 0.44f * minV);
			nvgMoveTo(vg, (float)xpix, y1);
			nvgLineTo(vg, (float)xpix, y2);
		}
		nvgStroke(vg);
		// Right channel overlay over window
		nvgBeginPath(vg);
		nvgStrokeColor(vg, nvgRGB(25, 150, 252));
		nvgStrokeWidth(vg, 1.0f);
		const size_t NRfull = module->sampleR.size();
		size_t NR = (winEnd <= NRfull) ? NLdraw : std::min(NLdraw, NRfull);
		// Use stable DC baseline for R during recording
		const size_t wpxR = (size_t)std::max(1.0f, std::floor(w));
		const size_t bucketR = std::max<size_t>(1, (size_t)std::floor((double)NR / (double)std::max<size_t>(1, wpxR)));
		for (size_t xpix = 0; xpix < wpxR; ++xpix) {
			size_t i0 = (size_t)std::floor((double)xpix * (double)NR / (double)wpxR);
			if (i0 >= NR) i0 = NR - 1;
			size_t iEnd = std::min(NR, i0 + bucketR);
			float minV = 1.f, maxV = -1.f;
			for (size_t i = i0; i < iEnd; ++i) {
				size_t idx = winStart + i;
				if (idx >= NRfull) idx = NRfull - 1;
				float v = module->sampleR[idx];
				if (module->isRecording) v -= (float)dcMeanR;
				// Clamp to display range symmetrically
				if (v < -1.f) v = -1.f; if (v > 1.f) v = 1.f;
				minV = std::min(minV, v);
				maxV = std::max(maxV, v);
			}
			minV = std::max(-1.f, std::min(1.f, minV));
			maxV = std::max(-1.f, std::min(1.f, maxV));
			if (maxV - minV < 1e-6f) { maxV += 0.01f; minV -= 0.01f; }
			float y1 = h * (0.5f - 0.44f * maxV);
			float y2 = h * (0.5f - 0.44f * minV);
			nvgMoveTo(vg, (float)xpix, y1);
			nvgLineTo(vg, (float)xpix, y2);
		}
		nvgStroke(vg);

		// Playback position line (mapped within the full visible buffer)
		nvgBeginPath(vg);
		double denom = (NLdraw >= 2) ? ((double)NLdraw - 1.0) : 1.0;
		double clampedPos = (double)module->playPos;
		if (clampedPos < (double)winStart) clampedPos = (double)winStart;
		if (clampedPos > (double)(winEnd - 1)) clampedPos = (double)(winEnd - 1);
		double rel = (NLdraw > 1) ? ((clampedPos - (double)winStart) / denom) : 0.0;
		float px = (float)(rel * (double)w);
		if (px < 0.f) px = 0.f; if (px > w - 1.f) px = w - 1.f;
		nvgMoveTo(vg, px, 0.f);
		nvgLineTo(vg, px, h);
		nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 200));
		nvgStrokeWidth(vg, 2.0f);
		nvgStroke(vg);
		// Indicate the hidden tail-guard during recording so the end doesn't look chopped
		if (module->isRecording && NLfull > (size_t)guard) {
			// Shade a fraction of the right edge proportional to guard/total length
			double guardFrac = (double)guard / (double)NLfull;
			float shadeW = (float)(guardFrac * (double)w);
			if (shadeW < 1.f) shadeW = 1.f;
			nvgBeginPath(vg);
			nvgRect(vg, w - shadeW, 0.f, shadeW, h);
			nvgFillColor(vg, nvgRGBA(255, 255, 255, 24));
			nvgFill(vg);
		}

		// Grain dots overlay (cloud of dots); show during recording too so grains are visible
		if (module && !module->normalPlayback) {
			bool recording = module->isRecording;
			// Use full buffer for overlay mapping to align with display
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
					nvgFillColor(vg, recording ? nvgRGBA(255, 255, 255, 180) : nvgRGBA(255, 255, 255, 220));
					nvgFill(vg);
					// Draw right dot (white) if R exists
					if (NR == NL) {
						int j1 = std::min(i0 + 1, (int)NR - 1);
						double sR = (1.0 - frac) * (double)module->sampleR[i0] + frac * (double)module->sampleR[j1];
						float gyR = h * (0.5f - 0.45f * (float)sR);
						nvgBeginPath(vg);
						nvgCircle(vg, gx, gyR, 3.0f);
						nvgFillColor(vg, recording ? nvgRGBA(255, 255, 255, 180) : nvgRGBA(255, 255, 255, 220));
						nvgFill(vg);
					}
				}
			}
		}

		// Info overlay: sample name, frames, length
		{
			// Compute name
			std::string name;
			if (module->samplePath.empty()) {
				name = module->isRecording ? "recording" : "patch storage";
			} else {
				name = module->samplePath;
				size_t p = name.find_last_of("/\\");
				if (p != std::string::npos) name = name.substr(p + 1);
			}
			size_t frames = module->sampleL.size();
			double secs = (module->fileSampleRate > 0) ? (double)frames / (double)module->fileSampleRate : 0.0;
			double mb = (double)(frames * 2ull * sizeof(float)) / 1048576.0;
			char buf[256];
			snprintf(buf, sizeof(buf), "%s  |  %.2f s @ %d Hz  |  %.2f MB",
				name.c_str(), secs, module->fileSampleRate, mb);
			// Draw at bottom-left
			nvgFontSize(vg, 12.f);
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
			nvgFillColor(vg, nvgRGBA(235, 235, 235, 220));
			nvgText(vg, 8.f, h - 6.f, buf, nullptr);
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

	addInput(createInput<TinyPJ301MPort>(Vec(45, 15), module, Grains::REC_INPUT));
	addParam(createParam<CKSS>(Vec(80, 15), module, Grains::REC_SWITCH));
	addInput(createInput<TinyPJ301MPort>(Vec(100, 15), module, Grains::REC_TOGGLE));
	addChild(createLight<SmallLight<RedLight>>(Vec(120, 21), module, Grains::REC_LIGHT));

	addInput(createInput<TinyPJ301MPort>(Vec(150, 15), module, Grains::CLOCK_INPUT));
	addParam(createParam<CKSS>(Vec(192, 15), module, Grains::SYNC_SWITCH));
	addParam(createParam<SmallButton>(Vec(485, 10), module, Grains::RANDOM_BUTTON));

	addParam(createParam<CKSS>(Vec(410, 15), module, Grains::AUTO_ADV_SWITCH));	
	addParam(createParam<JwTinyKnob>(Vec(445, 23), module, Grains::AUTO_ADV_RATE));

	float topY = 342;
	
	addInput(createInput<TinyPJ301MPort>(Vec(35, topY), module, Grains::POSITION_INPUT));
	addParam(createParam<JwTinyKnob>(Vec(55, topY), module, Grains::POSITION_KNOB));

	addInput(createInput<TinyPJ301MPort>(Vec(95, topY), module, Grains::PITCH_INPUT));
	addParam(createParam<JwTinyKnob>(Vec(115, topY), module, Grains::GRAIN_PITCH_SEMI));
	
	addInput(createInput<TinyPJ301MPort>(Vec(152, topY), module, Grains::SIZE_CV));
	addParam(createParam<JwTinyKnob>(Vec(172, topY), module, Grains::GRAIN_SIZE_MS));
	
	addInput(createInput<TinyPJ301MPort>(Vec(215, topY), module, Grains::DENSITY_CV));
	addParam(createParam<JwTinyKnob>(Vec(235, topY), module, Grains::GRAIN_DENSITY));
	
	addInput(createInput<TinyPJ301MPort>(Vec(270, topY), module, Grains::RATE_CV));
	addParam(createParam<JwTinyKnob>(Vec(290, topY), module, Grains::RATE_AMOUNT));

	addInput(createInput<TinyPJ301MPort>(Vec(320, topY), module, Grains::SPREAD_CV));
	addParam(createParam<JwTinyKnob>(Vec(340, topY), module, Grains::GRAIN_SPREAD_MS));

	addParam(createParam<JwTinyKnob>(Vec(380, topY), module, Grains::WINDOW_TYPE));

	addInput(createInput<TinyPJ301MPort>(Vec(425, topY), module, Grains::REVERSE_CV));
	addParam(createParam<JwTinyKnob>(Vec(445, topY), module, Grains::REVERSE_AMOUNT));
	addInput(createInput<TinyPJ301MPort>(Vec(475, topY), module, Grains::PAN_CV));
	addParam(createParam<JwTinyKnob>(Vec(495, topY), module, Grains::PAN_RANDOMNESS));
	
	addOutput(createOutput<TinyPJ301MPort>(Vec(522, topY + 1), module, Grains::OUT_L));
	addOutput(createOutput<TinyPJ301MPort>(Vec(541, topY + 1), module, Grains::OUT_R));
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
	// Auto-advance moved to a front-panel switch

	// Auto-advance CV override removed; front-panel switch governs behavior

	// Toggle normal playback (bypass grains)
	struct NormalPlaybackItem : MenuItem {
		Grains *grains;
		void onAction(const event::Action &e) override { if (grains) grains->normalPlayback = !grains->normalPlayback; }
		void step() override { rightText = (grains && grains->normalPlayback) ? "✔" : ""; MenuItem::step(); }
	};
	NormalPlaybackItem *normPlayItem = new NormalPlaybackItem();
	normPlayItem->text = "Normal Playback (no grains)";
	normPlayItem->grains = grains;
	menu->addChild(normPlayItem);
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

	// Save buffer as WAV
	struct SaveWavItem : MenuItem {
		Grains *grains;
		void onAction(const event::Action &e) override {
			if (!grains) return;
			osdialog_filters *filters = osdialog_filters_parse("WAV:wav");
			char *path = osdialog_file(OSDIALOG_SAVE, NULL, NULL, filters);
			osdialog_filters_free(filters);
			if (path) {
				std::string p = path;
				free(path);
				// Ensure .wav extension
				auto hasExt = [](const std::string &s){ if (s.size() < 4) return false; std::string ext = s.substr(s.size()-4); std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower); return ext == ".wav"; };
				if (!hasExt(p)) p += ".wav";
				grains->saveBufferToWav(p);
			}
		}
	};
	SaveWavItem *save = new SaveWavItem();
	save->text = "Save Buffer as WAV...";
	save->grains = grains;
	menu->addChild(save);
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
