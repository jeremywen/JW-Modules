#include <string.h>
#include <algorithm>
#include <array>
#include <chrono>
#include "JWModules.hpp"

#define GRID_ROWS 16
#define GRID_COLS 32
#define GRID_CELLS (GRID_ROWS * GRID_COLS)

struct Trigs128 : Module, QuantizeUtils {
	struct CvRangeSpec {
		float min = -10.f;
		float max = 10.f;
		const char *label = "+/-10V";

		CvRangeSpec() {}
		CvRangeSpec(float min, float max, const char *label) : min(min), max(max), label(label) {}
	};

	struct CellProps {
		bool active = false;
		float pitch = 0.0f;  // 0..10V domain (scaled by Range)
		int octave = 0;      // -4..4
		float cv = 0.0f;     // -10..10V
		float cv2 = 0.0f;    // -10..10V
		float probability = 1.0f;
		int division = 1;    // 1..16
		int ratchets = 1;    // 1..8
	};

	enum ParamIds {
		MANUAL_CLOCK_BTN_PARAM,
		MANUAL_RESET_BTN_PARAM,
		START1_KNOB_PARAM,
		START2_KNOB_PARAM,
		START3_KNOB_PARAM,
		START4_KNOB_PARAM,
		LENGTH1_KNOB_PARAM,
		LENGTH2_KNOB_PARAM,
		LENGTH3_KNOB_PARAM,
		LENGTH4_KNOB_PARAM,
		PLAY_MODE1_KNOB_PARAM,
		PLAY_MODE2_KNOB_PARAM,
		PLAY_MODE3_KNOB_PARAM,
		PLAY_MODE4_KNOB_PARAM,
		RND_AMT_KNOB_PARAM,
		RND1_TRIG_BTN_PARAM,
		RND2_TRIG_BTN_PARAM,
		RND3_TRIG_BTN_PARAM,
		RND4_TRIG_BTN_PARAM,
		NOTE_KNOB_PARAM,
		SCALE_KNOB_PARAM,
		GLOBAL_OCTAVE_KNOB_PARAM,
		VOCT_RANGE_KNOB_PARAM,
		CELL_PITCH_RND_BTN_PARAM,
		CELL_OCTAVE_RND_BTN_PARAM,
		CELL_CV_RND_BTN_PARAM,
		CELL_CV2_RND_BTN_PARAM,
		CELL_PROB_RND_BTN_PARAM,
		CELL_DIV_RND_BTN_PARAM,
		CELL_RATCHET_RND_BTN_PARAM,
		CELL_ACTIVE_RND_BTN_PARAM,
		CELL_PITCH_INIT_BTN_PARAM,
		CELL_OCTAVE_INIT_BTN_PARAM,
		CELL_CV_INIT_BTN_PARAM,
		CELL_CV2_INIT_BTN_PARAM,
		CELL_PROB_INIT_BTN_PARAM,
		CELL_DIV_INIT_BTN_PARAM,
		CELL_RATCHET_INIT_BTN_PARAM,
		CELL_PITCH_PARAM,
		CELL_OCTAVE_PARAM,
		CELL_CV_PARAM,
		CELL_CV2_PARAM,
		CELL_PROB_PARAM,
		CELL_DIV_PARAM,
		CELL_RATCHET_PARAM,
		CELL_ACTIVE_RND_AMT_PARAM,
		CLEAR1_BTN_PARAM,
		CLEAR2_BTN_PARAM,
		CLEAR3_BTN_PARAM,
		CLEAR4_BTN_PARAM,
		LIFE_ON_SWITCH_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		RESET_INPUT,
		START_INPUT,
		RND_TRIG_INPUT,
		CELL_ACTIVE_RND_INPUT,
		CLEAR_INPUT,
		LENGTH_INPUT,
		PLAY_MODE_INPUT,
		SEED_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		VOCT_OUTPUT,
		GATE_OUTPUT = VOCT_OUTPUT + 4,
		EOC_OUTPUT = GATE_OUTPUT + 4,
		CV_OUTPUT = EOC_OUTPUT + 4,
		CV2_OUTPUT = CV_OUTPUT + 4,
		LIFE_STOPPED_OUTPUT = CV2_OUTPUT + 4,
		NUM_OUTPUTS = LIFE_STOPPED_OUTPUT + 1
	};
	enum LightIds {
		NUM_LIGHTS
	};
	enum CvRangeMode {
		CV_RANGE_BIPOLAR_10V,
		CV_RANGE_BIPOLAR_5V,
		CV_RANGE_BIPOLAR_3V,
		CV_RANGE_BIPOLAR_2V,
		CV_RANGE_BIPOLAR_1V,
		CV_RANGE_UNIPOLAR_10V,
		CV_RANGE_UNIPOLAR_5V,
		CV_RANGE_UNIPOLAR_3V,
		CV_RANGE_UNIPOLAR_2V,
		CV_RANGE_UNIPOLAR_1V,
		NUM_CV_RANGE_MODES
	};
	enum LifeRateMode {
		LIFE_RATE_DIV_16,
		LIFE_RATE_DIV_8,
		LIFE_RATE_DIV_4,
		LIFE_RATE_DIV_3,
		LIFE_RATE_DIV_2,
		LIFE_RATE_X1,
		LIFE_RATE_X2,
		LIFE_RATE_X3,
		LIFE_RATE_X4,
		LIFE_RATE_X8,
		LIFE_RATE_X16,
		NUM_LIFE_RATE_MODES
	};
	enum GridShadeMode {
		GS_PROBABILITY,
		GS_PITCH,
		GS_CV,
		GS_CV2,
		GS_RATCHETS,
		GS_DIVISION,
		NUM_GRID_SHADE_MODES
	};

	std::array<CellProps, GRID_CELLS> cells;
	int selectedX = 0;
	int selectedY = 0;
	bool selectedDirty = true;

	int seqPos[4] = {-1, -1, -1, -1};
	int stepCounter[4] = {0, 0, 0, 0};
	std::array<std::array<int, GRID_CELLS>, 4> cellVisitCounter{};
	bool goingForward[4] = {true, true, true, true};
	int ratchetsRemaining[4] = {0, 0, 0, 0};
	int ratchetInterval[4] = {1, 1, 1, 1};
	int nextRatchetSample[4] = {0, 0, 0, 0};
	float lastVoct[4] = {0.f, 0.f, 0.f, 0.f};
	float lastCv[4] = {0.f, 0.f, 0.f, 0.f};
	float lastCv2[4] = {0.f, 0.f, 0.f, 0.f};
	bool resetMode[4] = {true, true, true, true};

	dsp::SchmittTrigger clockTrig[4];
	dsp::SchmittTrigger resetTrig;
	dsp::SchmittTrigger resetInputTrig[4];
	dsp::SchmittTrigger clearInputTrig[4];
	dsp::SchmittTrigger clearBtnTrig[4];
	dsp::SchmittTrigger rndTrig[4];
	dsp::SchmittTrigger cellRndTrig[7];
	dsp::SchmittTrigger cellActiveRndTrig;
	dsp::SchmittTrigger cellActiveRndInputTrig[4];
	dsp::SchmittTrigger cellInitTrig[7];
	dsp::PulseGenerator gatePulse[4];
	dsp::PulseGenerator eocPulse[4];
	dsp::PulseGenerator lifeStoppedPulse;

	float gatePulseLenSec = 0.005f;
	int samplesSinceClock[4] = {0, 0, 0, 0};
	int lastStepSamples[4] = {4410, 4410, 4410, 4410};
	int uniformTrackLength = GRID_COLS * 4;
	float minProbability = 0.f;
	bool polyphonicFirstRowOutputs = false;
	int cvRangeMode = CV_RANGE_UNIPOLAR_10V;
	int cv2RangeMode = CV_RANGE_UNIPOLAR_10V;
	int gridShadeMode = GS_PROBABILITY;
	int lifeRateMode = LIFE_RATE_X1;
	int lifeClockCounter = 0;
	bool lifeStationaryLatched = false;
	std::array<bool, GRID_CELLS> lifePrevActive{};
	bool lifePrevActiveValid = false;
	std::array<CellProps, GRID_COLS * 4> trackClipboard{};
	bool trackClipboardValid = false;
	CellProps cellClipboard{};
	bool cellClipboardValid = false;

	Trigs128() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(MANUAL_CLOCK_BTN_PARAM, 0.f, 1.f, 0.f, "Manual Clock Tick");
		configParam(MANUAL_RESET_BTN_PARAM, 0.f, 1.f, 0.f, "Manual Reset");
		configParam(START1_KNOB_PARAM, 0.f, (float) (GRID_COLS * 4 - 1), 0.f, "Start Orange");
		configParam(START2_KNOB_PARAM, 0.f, (float) (GRID_COLS * 4 - 1), 0.f, "Start Yellow");
		configParam(START3_KNOB_PARAM, 0.f, (float) (GRID_COLS * 4 - 1), 0.f, "Start Purple");
		configParam(START4_KNOB_PARAM, 0.f, (float) (GRID_COLS * 4 - 1), 0.f, "Start Blue");
		configParam(LENGTH1_KNOB_PARAM, 1.f, (float) (GRID_COLS * 4), (float) (GRID_COLS * 4), "Length Orange");
		configParam(LENGTH2_KNOB_PARAM, 1.f, (float) (GRID_COLS * 4), (float) (GRID_COLS * 4), "Length Yellow");
		configParam(LENGTH3_KNOB_PARAM, 1.f, (float) (GRID_COLS * 4), (float) (GRID_COLS * 4), "Length Purple");
		configParam(LENGTH4_KNOB_PARAM, 1.f, (float) (GRID_COLS * 4), (float) (GRID_COLS * 4), "Length Blue");
		configParam<JwPlayModeQuantity>(PLAY_MODE1_KNOB_PARAM, 0.f, NUM_PLAY_MODES - 1, 0.f, "Play Mode Orange");
		configParam<JwPlayModeQuantity>(PLAY_MODE2_KNOB_PARAM, 0.f, NUM_PLAY_MODES - 1, 0.f, "Play Mode Yellow");
		configParam<JwPlayModeQuantity>(PLAY_MODE3_KNOB_PARAM, 0.f, NUM_PLAY_MODES - 1, 0.f, "Play Mode Purple");
		configParam<JwPlayModeQuantity>(PLAY_MODE4_KNOB_PARAM, 0.f, NUM_PLAY_MODES - 1, 0.f, "Play Mode Blue");
		configParam(RND_AMT_KNOB_PARAM, 0.f, 1.f, 0.5f, "Random Amount");
		configParam(RND1_TRIG_BTN_PARAM, 0.f, 1.f, 0.f, "Randomize Orange Track");
		configParam(RND2_TRIG_BTN_PARAM, 0.f, 1.f, 0.f, "Randomize Yellow Track");
		configParam(RND3_TRIG_BTN_PARAM, 0.f, 1.f, 0.f, "Randomize Purple Track");
		configParam(RND4_TRIG_BTN_PARAM, 0.f, 1.f, 0.f, "Randomize Blue Track");
		configParam(GLOBAL_OCTAVE_KNOB_PARAM, -5.f, 7.f, -1.f, "Octave");
		configParam(NOTE_KNOB_PARAM, 0.f, QuantizeUtils::NUM_NOTES - 1, QuantizeUtils::NOTE_C, "Root Note");
		configParam(SCALE_KNOB_PARAM, 0.f, QuantizeUtils::NUM_SCALES - 1, QuantizeUtils::MINOR, "Scale");
		configParam(VOCT_RANGE_KNOB_PARAM, 0.f, 10.f, 1.f, "Pitch Range");
		configParam(CELL_PITCH_RND_BTN_PARAM, 0.f, 1.f, 0.f, "Randomize All Cell Pitch");
		configParam(CELL_OCTAVE_RND_BTN_PARAM, 0.f, 1.f, 0.f, "Randomize All Cell Octave");
		configParam(CELL_CV_RND_BTN_PARAM, 0.f, 1.f, 0.f, "Randomize All Cell CV");
		configParam(CELL_CV2_RND_BTN_PARAM, 0.f, 1.f, 0.f, "Randomize All Cell CV2");
		configParam(CELL_PROB_RND_BTN_PARAM, 0.f, 1.f, 0.f, "Randomize All Cell Probability");
		configParam(CELL_DIV_RND_BTN_PARAM, 0.f, 1.f, 0.f, "Randomize All Cell Division");
		configParam(CELL_RATCHET_RND_BTN_PARAM, 0.f, 1.f, 0.f, "Randomize All Cell Ratchets");
		configParam(CELL_ACTIVE_RND_BTN_PARAM, 0.f, 1.f, 0.f, "Randomize All Cell Gate Status");
		configParam(CELL_PITCH_INIT_BTN_PARAM, 0.f, 1.f, 0.f, "Initialize All Cell Pitch");
		configParam(CELL_OCTAVE_INIT_BTN_PARAM, 0.f, 1.f, 0.f, "Initialize All Cell Octave");
		configParam(CELL_CV_INIT_BTN_PARAM, 0.f, 1.f, 0.f, "Initialize All Cell CV");
		configParam(CELL_CV2_INIT_BTN_PARAM, 0.f, 1.f, 0.f, "Initialize All Cell CV2");
		configParam(CELL_PROB_INIT_BTN_PARAM, 0.f, 1.f, 0.f, "Initialize All Cell Probability");
		configParam(CELL_DIV_INIT_BTN_PARAM, 0.f, 1.f, 0.f, "Initialize All Cell Division");
		configParam(CELL_RATCHET_INIT_BTN_PARAM, 0.f, 1.f, 0.f, "Initialize All Cell Ratchets");
		configParam(CELL_PITCH_PARAM, 0.f, 10.f, 0.f, "Cell Pitch");
		configParam(CELL_OCTAVE_PARAM, -4.f, 4.f, 0.f, "Cell Octave");
		configParam(CELL_CV_PARAM, -10.f, 10.f, 0.f, "Cell CV");
		configParam(CELL_CV2_PARAM, -10.f, 10.f, 0.f, "Cell CV2");
		configParam(CELL_PROB_PARAM, 0.f, 1.f, 1.f, "Cell Probability");
		configParam(CELL_DIV_PARAM, 1.f, 16.f, 1.f, "Cell Division");
		configParam(CELL_RATCHET_PARAM, 1.f, 8.f, 1.f, "Cell Ratchets");
		configParam(CELL_ACTIVE_RND_AMT_PARAM, 0.f, 1.f, 0.5f, "Cell Gate Status Random Amount");
		configParam(CLEAR1_BTN_PARAM, 0.f, 1.f, 0.f, "Clear Orange Track");
		configParam(CLEAR2_BTN_PARAM, 0.f, 1.f, 0.f, "Clear Yellow Track");
		configParam(CLEAR3_BTN_PARAM, 0.f, 1.f, 0.f, "Clear Purple Track");
		configParam(CLEAR4_BTN_PARAM, 0.f, 1.f, 0.f, "Clear Blue Track");
		configParam(LIFE_ON_SWITCH_PARAM, 0.f, 1.f, 0.f, "Game of Life Enable");

		configInput(CLOCK_INPUT, "Clock (polyphonic for tracks 1-4)");
		configInput(RESET_INPUT, "Reset (polyphonic for tracks 1-4)");
		configInput(START_INPUT, "Start (polyphonic for tracks 1-4)");
		configInput(RND_TRIG_INPUT, "Random Trigger (polyphonic for tracks 1-4)");
		configInput(CELL_ACTIVE_RND_INPUT, "Random Gate Status Trigger (polyphonic for tracks 1-4)");
		configInput(CLEAR_INPUT, "Clear (polyphonic for tracks 1-4)");
		configInput(LENGTH_INPUT, "Length (polyphonic for tracks 1-4)");
		configInput(PLAY_MODE_INPUT, "Play Mode (polyphonic for tracks 1-4)");
		configInput(SEED_INPUT, "Seed input for repeatable randomization");

		const char *colors[4] = { "Orange", "Yellow", "Purple", "Blue" };
		for (int i = 0; i < 4; i++) {
			configOutput(VOCT_OUTPUT + i, "V/Oct " + std::string(colors[i]));
			configOutput(GATE_OUTPUT + i, "Gate " + std::string(colors[i]));
			configOutput(EOC_OUTPUT + i, "End of Cycle " + std::string(colors[i]));
			configOutput(CV_OUTPUT + i, "CV " + std::string(colors[i]));
			configOutput(CV2_OUTPUT + i, "CV2 " + std::string(colors[i]));
		}
		configOutput(LIFE_STOPPED_OUTPUT, "Game of Life Stopped");

		refreshCvParamRanges();
		pushSelectedCellToParams();
	}

	static CvRangeSpec getCvRangeSpec(int mode) {
		switch (clampijw(mode, 0, NUM_CV_RANGE_MODES - 1)) {
			case CV_RANGE_BIPOLAR_10V: return {-10.f, 10.f, "+/-10V"};
			case CV_RANGE_BIPOLAR_5V: return {-5.f, 5.f, "+/-5V"};
			case CV_RANGE_BIPOLAR_3V: return {-3.f, 3.f, "+/-3V"};
			case CV_RANGE_BIPOLAR_2V: return {-2.f, 2.f, "+/-2V"};
			case CV_RANGE_BIPOLAR_1V: return {-1.f, 1.f, "+/-1V"};
			case CV_RANGE_UNIPOLAR_10V: return {0.f, 10.f, "0-10V"};
			case CV_RANGE_UNIPOLAR_5V: return {0.f, 5.f, "0-5V"};
			case CV_RANGE_UNIPOLAR_3V: return {0.f, 3.f, "0-3V"};
			case CV_RANGE_UNIPOLAR_2V: return {0.f, 2.f, "0-2V"};
			case CV_RANGE_UNIPOLAR_1V: return {0.f, 1.f, "0-1V"};
			default: return {-10.f, 10.f, "+/-10V"};
		}
	}

	float clampCvRangeValue(float value, int mode) const {
		CvRangeSpec spec = getCvRangeSpec(mode);
		return clampfjw(value, spec.min, spec.max);
	}

	float randomCvRangeValue(int mode) const {
		CvRangeSpec spec = getCvRangeSpec(mode);
		return rescalefjw(random::uniform(), 0.f, 1.f, spec.min, spec.max);
	}

	void applyCvParamRange(int paramId, int mode) {
		CvRangeSpec spec = getCvRangeSpec(mode);
		if (paramId >= 0 && (size_t)paramId < paramQuantities.size() && paramQuantities[paramId]) {
			paramQuantities[paramId]->minValue = spec.min;
			paramQuantities[paramId]->maxValue = spec.max;
			paramQuantities[paramId]->defaultValue = 0.f;
			paramQuantities[paramId]->setValue(clampfjw(paramQuantities[paramId]->getValue(), spec.min, spec.max));
		}
	}

	void refreshCvParamRanges() {
		applyCvParamRange(CELL_CV_PARAM, cvRangeMode);
		applyCvParamRange(CELL_CV2_PARAM, cv2RangeMode);
	}

	void setCvRangeMode(bool secondOutput, int mode) {
		int clampedMode = clampijw(mode, 0, NUM_CV_RANGE_MODES - 1);
		if (secondOutput) {
			cv2RangeMode = clampedMode;
		}
		else {
			cvRangeMode = clampedMode;
		}
		refreshCvParamRanges();
		selectedDirty = true;
	}

	static const char *getGridShadeLabel(int mode) {
		switch (clampijw(mode, 0, NUM_GRID_SHADE_MODES - 1)) {
			case GS_PROBABILITY: return "Probability";
			case GS_PITCH: return "Pitch";
			case GS_CV: return "CV";
			case GS_CV2: return "CV2";
			case GS_RATCHETS: return "Ratchets";
			case GS_DIVISION: return "Division";
			default: return "Probability";
		}
	}

	float getGridShadeNorm(const CellProps &c) const {
		switch (clampijw(gridShadeMode, 0, NUM_GRID_SHADE_MODES - 1)) {
			case GS_PROBABILITY:
				return clampfjw(c.probability, 0.f, 1.f);
			case GS_PITCH:
				return clampfjw(c.pitch / 10.f, 0.f, 1.f);
			case GS_CV: {
				CvRangeSpec spec = getCvRangeSpec(cvRangeMode);
				float denom = std::max(0.0001f, spec.max - spec.min);
				return clampfjw((c.cv - spec.min) / denom, 0.f, 1.f);
			}
			case GS_CV2: {
				CvRangeSpec spec = getCvRangeSpec(cv2RangeMode);
				float denom = std::max(0.0001f, spec.max - spec.min);
				return clampfjw((c.cv2 - spec.min) / denom, 0.f, 1.f);
			}
			case GS_RATCHETS:
				return clampfjw((float)(c.ratchets - 1) / 7.f, 0.f, 1.f);
			case GS_DIVISION:
				return clampfjw((float)(c.division - 1) / 15.f, 0.f, 1.f);
			default:
				return clampfjw(c.probability, 0.f, 1.f);
		}
	}

	void invalidateLifeStationaryState() {
		lifeStationaryLatched = false;
		lifePrevActiveValid = false;
	}

	static const char *getLifeRateLabel(int mode) {
		switch (clampijw(mode, 0, NUM_LIFE_RATE_MODES - 1)) {
			case LIFE_RATE_DIV_16: return "/16";
			case LIFE_RATE_DIV_8: return "/8";
			case LIFE_RATE_DIV_4: return "/4";
			case LIFE_RATE_DIV_3: return "/3";
			case LIFE_RATE_DIV_2: return "/2";
			case LIFE_RATE_X1: return "x1";
			case LIFE_RATE_X2: return "x2";
			case LIFE_RATE_X3: return "x3";
			case LIFE_RATE_X4: return "x4";
			case LIFE_RATE_X8: return "x8";
			case LIFE_RATE_X16: return "x16";
			default: return "x1";
		}
	}

	int getLifeClockDivider() const {
		switch (clampijw(lifeRateMode, 0, NUM_LIFE_RATE_MODES - 1)) {
			case LIFE_RATE_DIV_16: return 16;
			case LIFE_RATE_DIV_8: return 8;
			case LIFE_RATE_DIV_4: return 4;
			case LIFE_RATE_DIV_3: return 3;
			case LIFE_RATE_DIV_2: return 2;
			default: return 1;
		}
	}

	int getLifeClockMultiplier() const {
		switch (clampijw(lifeRateMode, 0, NUM_LIFE_RATE_MODES - 1)) {
			case LIFE_RATE_X16: return 16;
			case LIFE_RATE_X8: return 8;
			case LIFE_RATE_X4: return 4;
			case LIFE_RATE_X3: return 3;
			case LIFE_RATE_X2: return 2;
			case LIFE_RATE_X1: return 1;
			default: return 0;
		}
	}

	void onReset() override {
		params[START1_KNOB_PARAM].setValue(0.f);
		params[START2_KNOB_PARAM].setValue(0.f);
		params[START3_KNOB_PARAM].setValue(0.f);
		params[START4_KNOB_PARAM].setValue(0.f);
		params[PLAY_MODE1_KNOB_PARAM].setValue(0.f);
		params[PLAY_MODE2_KNOB_PARAM].setValue(0.f);
		params[PLAY_MODE3_KNOB_PARAM].setValue(0.f);
		params[PLAY_MODE4_KNOB_PARAM].setValue(0.f);
		params[LENGTH1_KNOB_PARAM].setValue((float)(GRID_COLS * 4));
		params[LENGTH2_KNOB_PARAM].setValue((float)(GRID_COLS * 4));
		params[LENGTH3_KNOB_PARAM].setValue((float)(GRID_COLS * 4));
		params[LENGTH4_KNOB_PARAM].setValue((float)(GRID_COLS * 4));
		params[LIFE_ON_SWITCH_PARAM].setValue(0.f);
		for (int ch = 0; ch < 4; ch++) {
			seqPos[ch] = -1;
			stepCounter[ch] = 0;
			for (int i = 0; i < GRID_CELLS; i++) {
				cellVisitCounter[ch][i] = 0;
			}
			resetMode[ch] = false;
			goingForward[ch] = true;
			ratchetsRemaining[ch] = 0;
			ratchetInterval[ch] = 1;
			nextRatchetSample[ch] = 0;
			gatePulse[ch].reset();
			eocPulse[ch].reset();
		}
		for (int ch = 0; ch < 4; ch++) {
			samplesSinceClock[ch] = 0;
			lastStepSamples[ch] = 4410;
		}
		lifeClockCounter = 0;
		lifeStationaryLatched = false;
		lifePrevActiveValid = false;
		lifeStoppedPulse.reset();
		clearGrid();
		selectedX = 0;
		selectedY = 0;
		selectedDirty = true;
	}

	void onRandomize() override {
		randomizeGrid();
	}

	void setAllTrackLengths(int length) {
		int l = clampijw(length, 1, GRID_COLS * 4);
		params[LENGTH1_KNOB_PARAM].setValue((float)l);
		params[LENGTH2_KNOB_PARAM].setValue((float)l);
		params[LENGTH3_KNOB_PARAM].setValue((float)l);
		params[LENGTH4_KNOB_PARAM].setValue((float)l);
	}

	void repeatTrackSelectedLengthToMax(int track) {
		int t = clampijw(track, 0, 3);
		int base = clampijw(uniformTrackLength, 1, GRID_COLS * 4);
		std::array<CellProps, GRID_COLS * 4> repeated{};
		for (int step = 0; step < GRID_COLS * 4; step++) {
			int srcStep = step % base;
			int srcCol = srcStep % GRID_COLS;
			int srcRow = t * 4 + (srcStep / GRID_COLS);
			repeated[step] = cells[iFromXY(srcCol, srcRow)];
		}
		for (int step = 0; step < GRID_COLS * 4; step++) {
			int col = step % GRID_COLS;
			int row = t * 4 + (step / GRID_COLS);
			cells[iFromXY(col, row)] = repeated[step];
		}
		selectedDirty = true;
	}

	void repeatSelectedLengthToMax() {
		for (int track = 0; track < 4; track++) {
			repeatTrackSelectedLengthToMax(track);
		}
	}

	void copyTrackToClipboard(int track) {
		int t = clampijw(track, 0, 3);
		for (int step = 0; step < GRID_COLS * 4; step++) {
			int col = step % GRID_COLS;
			int row = t * 4 + (step / GRID_COLS);
			trackClipboard[step] = cells[iFromXY(col, row)];
		}
		trackClipboardValid = true;
	}

	void pasteClipboardToTrack(int track) {
		if (!trackClipboardValid) return;
		invalidateLifeStationaryState();
		int t = clampijw(track, 0, 3);
		for (int step = 0; step < GRID_COLS * 4; step++) {
			int col = step % GRID_COLS;
			int row = t * 4 + (step / GRID_COLS);
			cells[iFromXY(col, row)] = trackClipboard[step];
		}
		selectedDirty = true;
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_t *cellsJ = json_array();
		for (int i = 0; i < GRID_CELLS; i++) {
			json_t *cellJ = json_object();
			json_object_set_new(cellJ, "a", json_boolean(cells[i].active));
			json_object_set_new(cellJ, "p", json_real(cells[i].pitch));
			json_object_set_new(cellJ, "o", json_integer(cells[i].octave));
			json_object_set_new(cellJ, "cv", json_real(cells[i].cv));
			json_object_set_new(cellJ, "cv2", json_real(cells[i].cv2));
			json_object_set_new(cellJ, "pr", json_real(cells[i].probability));
			json_object_set_new(cellJ, "d", json_integer(cells[i].division));
			json_object_set_new(cellJ, "r", json_integer(cells[i].ratchets));
			json_array_append_new(cellsJ, cellJ);
		}
		json_object_set_new(rootJ, "cells", cellsJ);
		json_object_set_new(rootJ, "selectedX", json_integer(selectedX));
		json_object_set_new(rootJ, "selectedY", json_integer(selectedY));
		json_object_set_new(rootJ, "gatePulseLenSec", json_real(gatePulseLenSec));
		json_object_set_new(rootJ, "uniformTrackLength", json_integer(uniformTrackLength));
		json_object_set_new(rootJ, "minProbability", json_real(minProbability));
		json_object_set_new(rootJ, "polyphonicFirstRowOutputs", json_boolean(polyphonicFirstRowOutputs));
		json_object_set_new(rootJ, "cvRangeMode", json_integer(cvRangeMode));
		json_object_set_new(rootJ, "cv2RangeMode", json_integer(cv2RangeMode));
		json_object_set_new(rootJ, "gridShadeMode", json_integer(gridShadeMode));
		json_object_set_new(rootJ, "lifeEnabled", json_boolean(params[LIFE_ON_SWITCH_PARAM].getValue() > 0.5f));
		json_object_set_new(rootJ, "lifeRateMode", json_integer(lifeRateMode));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *cellsJ = json_object_get(rootJ, "cells");
		if (cellsJ && json_is_array(cellsJ)) {
			size_t n = std::min((size_t)GRID_CELLS, json_array_size(cellsJ));
			for (size_t i = 0; i < n; i++) {
				json_t *cellJ = json_array_get(cellsJ, i);
				if (!cellJ || !json_is_object(cellJ)) continue;
				cells[i].active = json_boolean_value(json_object_get(cellJ, "a"));
				cells[i].pitch = clampfjw((float)json_number_value(json_object_get(cellJ, "p")), 0.f, 10.f);
				cells[i].octave = clampijw((int)json_integer_value(json_object_get(cellJ, "o")), -4, 4);
				json_t *cvJ = json_object_get(cellJ, "cv");
				if (cvJ) cells[i].cv = clampfjw((float)json_number_value(cvJ), -10.f, 10.f);
				json_t *cv2J = json_object_get(cellJ, "cv2");
				if (cv2J) cells[i].cv2 = clampfjw((float)json_number_value(cv2J), -10.f, 10.f);
				json_t *prJ = json_object_get(cellJ, "pr");
				if (prJ) cells[i].probability = clampfjw((float)json_number_value(prJ), 0.f, 1.f);
				cells[i].division = clampijw((int)json_integer_value(json_object_get(cellJ, "d")), 1, 16);
				cells[i].ratchets = clampijw((int)json_integer_value(json_object_get(cellJ, "r")), 1, 8);
			}
		}
		json_t *sxJ = json_object_get(rootJ, "selectedX");
		json_t *syJ = json_object_get(rootJ, "selectedY");
		if (sxJ) selectedX = clampijw((int)json_integer_value(sxJ), 0, GRID_COLS - 1);
		if (syJ) selectedY = clampijw((int)json_integer_value(syJ), 0, GRID_ROWS - 1);
		json_t *gJ = json_object_get(rootJ, "gatePulseLenSec");
		if (gJ) gatePulseLenSec = clampfjw((float)json_number_value(gJ), 0.001f, 10.0f);
		json_t *uJ = json_object_get(rootJ, "uniformTrackLength");
		if (uJ) uniformTrackLength = clampijw((int)json_integer_value(uJ), 1, GRID_COLS * 4);
		json_t *minProbJ = json_object_get(rootJ, "minProbability");
		if (minProbJ) minProbability = clampfjw((float)json_number_value(minProbJ), 0.f, 1.f);
		json_t *polyJ = json_object_get(rootJ, "polyphonicFirstRowOutputs");
		if (polyJ) polyphonicFirstRowOutputs = json_boolean_value(polyJ);
		json_t *cvRangeJ = json_object_get(rootJ, "cvRangeMode");
		if (cvRangeJ) cvRangeMode = clampijw((int)json_integer_value(cvRangeJ), 0, NUM_CV_RANGE_MODES - 1);
		json_t *cv2RangeJ = json_object_get(rootJ, "cv2RangeMode");
		if (cv2RangeJ) cv2RangeMode = clampijw((int)json_integer_value(cv2RangeJ), 0, NUM_CV_RANGE_MODES - 1);
		json_t *gridShadeJ = json_object_get(rootJ, "gridShadeMode");
		if (gridShadeJ) gridShadeMode = clampijw((int)json_integer_value(gridShadeJ), 0, NUM_GRID_SHADE_MODES - 1);
		json_t *lifeEnabledJ = json_object_get(rootJ, "lifeEnabled");
		if (lifeEnabledJ) params[LIFE_ON_SWITCH_PARAM].setValue(json_boolean_value(lifeEnabledJ) ? 1.f : 0.f);
		json_t *lifeRateModeJ = json_object_get(rootJ, "lifeRateMode");
		if (lifeRateModeJ) lifeRateMode = clampijw((int)json_integer_value(lifeRateModeJ), 0, NUM_LIFE_RATE_MODES - 1);
		lifeClockCounter = 0;
		lifeStationaryLatched = false;
		lifePrevActiveValid = false;
		lifeStoppedPulse.reset();
		refreshCvParamRanges();
		selectedDirty = true;
	}

	void clearGrid() {
		invalidateLifeStationaryState();
		for (int i = 0; i < GRID_CELLS; i++) {
			cells[i].active = false;
			initCellParams(cells[i]);
		}
		selectedDirty = true;
	}

	void clearTrack(int track) {
		invalidateLifeStationaryState();
		int t = clampijw(track, 0, 3);
		int y0 = t * 4;
		int y1 = y0 + 4;
		for (int y = y0; y < y1; y++) {
			for (int x = 0; x < GRID_COLS; x++) {
				CellProps &c = cells[iFromXY(x, y)];
				c.active = false;
				initCellParams(c);
			}
		}
		selectedDirty = true;
	}

	void initCellParams(CellProps &c) {
		c.pitch = 0.f;
		c.octave = 0;
		c.cv = 0.f;
		c.cv2 = 0.f;
		c.probability = 1.f;
		c.division = 1;
		c.ratchets = 1;
	}

	void setCellActive(int x, int y, bool active) {
		x = clampijw(x, 0, GRID_COLS - 1);
		y = clampijw(y, 0, GRID_ROWS - 1);
		CellProps &c = cells[iFromXY(x, y)];
		c.active = active;
		if (!active) {
			initCellParams(c);
		}
		if (x == selectedX && y == selectedY) {
			selectedDirty = true;
		}
	}

	void copySelectedCellToClipboard() {
		cellClipboard = cells[iFromXY(selectedX, selectedY)];
		cellClipboardValid = true;
	}

	void pasteClipboardToSelectedCell() {
		if (!cellClipboardValid) return;
		invalidateLifeStationaryState();
		cells[iFromXY(selectedX, selectedY)] = cellClipboard;
		selectedDirty = true;
	}

	void randomizeGrid() {
		invalidateLifeStationaryState();
		float rndAmt = clampfjw(params[RND_AMT_KNOB_PARAM].getValue(), 0.f, 1.f);
		for (int i = 0; i < GRID_CELLS; i++) {
			cells[i].active = (random::uniform() < rndAmt);
			cells[i].pitch = random::uniform() * 10.f;
			cells[i].octave = (int)std::floor(random::uniform() * 9.f) - 4;
			cells[i].cv = randomCvRangeValue(cvRangeMode);
			cells[i].cv2 = randomCvRangeValue(cv2RangeMode);
			cells[i].probability = random::uniform();
			cells[i].division = (int)std::floor(random::uniform() * 16.f) + 1;
			cells[i].ratchets = (int)std::floor(random::uniform() * 8.f) + 1;
		}
		selectedDirty = true;
	}

	void randomizeTrack(int track) {
		invalidateLifeStationaryState();
		int t = clampijw(track, 0, 3);
		int y0 = t * 4;
		int y1 = y0 + 4;
		float rndAmt = clampfjw(params[RND_AMT_KNOB_PARAM].getValue(), 0.f, 1.f);
		for (int y = y0; y < y1; y++) {
			for (int x = 0; x < GRID_COLS; x++) {
				CellProps &c = cells[iFromXY(x, y)];
				c.active = (random::uniform() < rndAmt);
				c.pitch = random::uniform() * 10.f;
				c.octave = (int)std::floor(random::uniform() * 9.f) - 4;
				c.cv = randomCvRangeValue(cvRangeMode);
				c.cv2 = randomCvRangeValue(cv2RangeMode);
				c.probability = random::uniform();
				c.division = (int)std::floor(random::uniform() * 16.f) + 1;
				c.ratchets = (int)std::floor(random::uniform() * 8.f) + 1;
			}
		}
		selectedDirty = true;
	}

	void randomizeCellPitchAll() {
		for (int i = 0; i < GRID_CELLS; i++) {
			cells[i].pitch = random::uniform() * 10.f;
		}
		selectedDirty = true;
	}

	void randomizeCellOctaveAll() {
		for (int i = 0; i < GRID_CELLS; i++) {
			cells[i].octave = (int)std::floor(random::uniform() * 9.f) - 4;
		}
		selectedDirty = true;
	}

	void randomizeCellCvAll() {
		for (int i = 0; i < GRID_CELLS; i++) {
			cells[i].cv = randomCvRangeValue(cvRangeMode);
		}
		selectedDirty = true;
	}

	void randomizeCellCv2All() {
		for (int i = 0; i < GRID_CELLS; i++) {
			cells[i].cv2 = randomCvRangeValue(cv2RangeMode);
		}
		selectedDirty = true;
	}

	void randomizeCellProbAll() {
		for (int i = 0; i < GRID_CELLS; i++) {
			cells[i].probability = random::uniform();
		}
		selectedDirty = true;
	}

	void randomizeCellDivAll() {
		for (int i = 0; i < GRID_CELLS; i++) {
			cells[i].division = (int)std::floor(random::uniform() * 16.f) + 1;
		}
		selectedDirty = true;
	}

	void randomizeCellRatchetAll() {
		for (int i = 0; i < GRID_CELLS; i++) {
			cells[i].ratchets = (int)std::floor(random::uniform() * 8.f) + 1;
		}
		selectedDirty = true;
	}

	void randomizeCellActiveAll() {
		invalidateLifeStationaryState();
		float rndAmt = clampfjw(params[CELL_ACTIVE_RND_AMT_PARAM].getValue(), 0.f, 1.f);
		for (int i = 0; i < GRID_CELLS; i++) {
			cells[i].active = (random::uniform() < rndAmt);
		}
		selectedDirty = true;
	}

	void randomizeCellActiveTrack(int track) {
		invalidateLifeStationaryState();
		int t = clampijw(track, 0, 3);
		int y0 = t * 4;
		int y1 = y0 + 4;
		float rndAmt = clampfjw(params[CELL_ACTIVE_RND_AMT_PARAM].getValue(), 0.f, 1.f);
		for (int y = y0; y < y1; y++) {
			for (int x = 0; x < GRID_COLS; x++) {
				cells[iFromXY(x, y)].active = (random::uniform() < rndAmt);
			}
		}
		selectedDirty = true;
	}

	int getLifeNeighborCount(int x, int y) const {
		int result = 0;
		for (int dx = -1; dx <= 1; dx++) {
			for (int dy = -1; dy <= 1; dy++) {
				if (dx == 0 && dy == 0) {
					continue;
				}
				int nx = x + dx;
				int ny = y + dy;
				if (nx >= 0 && ny >= 0 && nx < GRID_COLS && ny < GRID_ROWS && cells[iFromXY(nx, ny)].active) {
					result++;
				}
			}
		}
		return result;
	}

	void stepLife() {
		std::array<bool, GRID_CELLS> nextActive{};
		bool changed = false;
		bool repeatsPrevious = lifePrevActiveValid;
		for (int x = 0; x < GRID_COLS; x++) {
			for (int y = 0; y < GRID_ROWS; y++) {
				int cellIndex = iFromXY(x, y);
				int neighbors = getLifeNeighborCount(x, y);
				bool isActive = cells[cellIndex].active;
				nextActive[cellIndex] = isActive;
				if (neighbors < 2 || neighbors > 3) {
					nextActive[cellIndex] = false;
				}
				else if (neighbors == 3 && !isActive) {
					nextActive[cellIndex] = true;
				}
			}
		}
		for (int i = 0; i < GRID_CELLS; i++) {
			if (repeatsPrevious && nextActive[i] != lifePrevActive[i]) {
				repeatsPrevious = false;
			}
			if (cells[i].active != nextActive[i]) {
				changed = true;
			}
			lifePrevActive[i] = cells[i].active;
			cells[i].active = nextActive[i];
		}
		lifePrevActiveValid = true;
		if (changed) {
			lifeStationaryLatched = false;
		}
		if ((!changed || repeatsPrevious) && !lifeStationaryLatched) {
			lifeStoppedPulse.trigger(gatePulseLenSec);
			lifeStationaryLatched = true;
		}
	}

	void advanceLifeClock() {
		if (params[LIFE_ON_SWITCH_PARAM].getValue() < 0.5f) {
			lifeClockCounter = 0;
			return;
		}
		int multiplier = getLifeClockMultiplier();
		if (multiplier > 0) {
			for (int i = 0; i < multiplier; i++) {
				stepLife();
			}
			lifeClockCounter = 0;
			return;
		}
		int divider = std::max(1, getLifeClockDivider());
		lifeClockCounter++;
		if (lifeClockCounter >= divider) {
			stepLife();
			lifeClockCounter = 0;
		}
	}

	void initCellPitchAll() {
		for (int i = 0; i < GRID_CELLS; i++) {
			cells[i].pitch = 0.f;
		}
		selectedDirty = true;
	}

	void initCellOctaveAll() {
		for (int i = 0; i < GRID_CELLS; i++) {
			cells[i].octave = 0;
		}
		selectedDirty = true;
	}

	void initCellCvAll() {
		for (int i = 0; i < GRID_CELLS; i++) {
			cells[i].cv = 0.f;
		}
		selectedDirty = true;
	}

	void initCellCv2All() {
		for (int i = 0; i < GRID_CELLS; i++) {
			cells[i].cv2 = 0.f;
		}
		selectedDirty = true;
	}

	void initCellProbAll() {
		for (int i = 0; i < GRID_CELLS; i++) {
			cells[i].probability = 1.f;
		}
		selectedDirty = true;
	}

	void initCellDivAll() {
		for (int i = 0; i < GRID_CELLS; i++) {
			cells[i].division = 1;
		}
		selectedDirty = true;
	}

	void initCellRatchetAll() {
		for (int i = 0; i < GRID_CELLS; i++) {
			cells[i].ratchets = 1;
		}
		selectedDirty = true;
	}

	inline int iFromXY(int x, int y) const {
		return x + y * GRID_COLS;
	}

	void selectCell(int x, int y, bool toggle) {
		x = clampijw(x, 0, GRID_COLS - 1);
		y = clampijw(y, 0, GRID_ROWS - 1);
		selectedX = x;
		selectedY = y;
		if (toggle) {
			CellProps &c = cells[iFromXY(x, y)];
			setCellActive(x, y, !c.active);
		}
		selectedDirty = true;
	}

	void pushSelectedCellToParams() {
		CellProps &c = cells[iFromXY(selectedX, selectedY)];
		params[CELL_PITCH_PARAM].setValue(c.pitch);
		params[CELL_OCTAVE_PARAM].setValue((float)c.octave);
		params[CELL_CV_PARAM].setValue(clampCvRangeValue(c.cv, cvRangeMode));
		params[CELL_CV2_PARAM].setValue(clampCvRangeValue(c.cv2, cv2RangeMode));
		params[CELL_PROB_PARAM].setValue(c.probability);
		params[CELL_DIV_PARAM].setValue((float)c.division);
		params[CELL_RATCHET_PARAM].setValue((float)c.ratchets);
	}

	void syncParamsToSelectedCell() {
		CellProps &c = cells[iFromXY(selectedX, selectedY)];
		c.pitch = clampfjw(params[CELL_PITCH_PARAM].getValue(), 0.f, 10.f);
		c.octave = clampijw((int)std::round(params[CELL_OCTAVE_PARAM].getValue()), -4, 4);
		c.cv = clampCvRangeValue(params[CELL_CV_PARAM].getValue(), cvRangeMode);
		c.cv2 = clampCvRangeValue(params[CELL_CV2_PARAM].getValue(), cv2RangeMode);
		c.probability = clampfjw(params[CELL_PROB_PARAM].getValue(), 0.f, 1.f);
		c.division = clampijw((int)std::round(params[CELL_DIV_PARAM].getValue()), 1, 16);
		c.ratchets = clampijw((int)std::round(params[CELL_RATCHET_PARAM].getValue()), 1, 8);
	}

	float getTrackInputVoltage(int inputId, int track) {
		if (!inputs[inputId].isConnected()) {
			return 0.f;
		}
		int channels = inputs[inputId].getChannels();
		int inputChannel = (channels > 1) ? clampijw(track, 0, channels - 1) : 0;
		return inputs[inputId].getVoltage(inputChannel);
	}

	int getSeqStart(int seqIndex) {
		int maxLen = GRID_COLS * 4;
		int inputOffset = inputs[START_INPUT].isConnected()
			? int(rescalefjw(getTrackInputVoltage(START_INPUT, seqIndex), 0.f, 10.f, 0.f, float(maxLen - 1)))
			: 0;
		int startParamId = START1_KNOB_PARAM + clampijw(seqIndex, 0, 3);
		int start = clampijw(params[startParamId].getValue() + inputOffset, 0.f, float(maxLen - 1));
		return start;
	}

	int getSeqLen(int seqIndex) {
		int maxLen = GRID_COLS * 4;
		int seqStart = getSeqStart(seqIndex);
		int lenParamId = LENGTH1_KNOB_PARAM + clampijw(seqIndex, 0, 3);
		int inputOffset = 0;
		if (inputs[LENGTH_INPUT].isConnected()) {
			float lenCv = getTrackInputVoltage(LENGTH_INPUT, seqIndex);
			inputOffset = int(rescalefjw(lenCv, 0.f, 10.f, 0.f, float(maxLen - 1)));
		}
		int maxLenFromStart = std::max(1, maxLen - seqStart);
		int len = clampijw(params[lenParamId].getValue() + inputOffset, 1.f, float(maxLenFromStart));
		return len;
	}

	int getPlayMode(int seqIndex) {
		int modeOffset = inputs[PLAY_MODE_INPUT].isConnected()
			? int(rescalefjw(getTrackInputVoltage(PLAY_MODE_INPUT, seqIndex), 0.f, 10.f, 0.f, NUM_PLAY_MODES - 1))
			: 0;
		int modeParamId = PLAY_MODE1_KNOB_PARAM + clampijw(seqIndex, 0, 3);
		int mode = clampijw(params[modeParamId].getValue() + modeOffset, 0.f, NUM_PLAY_MODES - 1);
		return mode;
	}

	void advanceSeqPos(int ch) {
					lifeStationaryLatched = false;
		int maxLen = GRID_COLS * 4;
		int seqStart = getSeqStart(ch);
		int seqLen = getSeqLen(ch);
		int seqEnd = clampijw(seqStart + seqLen - 1, seqStart, maxLen - 1);
		int curPlayMode = getPlayMode(ch);
		int &pos = seqPos[ch];

		if (curPlayMode == PM_FWD_LOOP) {
			if (pos < (seqStart - 1) || pos > seqEnd) pos = seqStart - 1;
			pos++;
			if (pos > seqEnd) {
				pos = seqStart;
				eocPulse[ch].trigger(gatePulseLenSec);
			}
			goingForward[ch] = true;
		}
		else if (curPlayMode == PM_BWD_LOOP) {
			if (pos < seqStart || pos > seqEnd) pos = seqStart;
			pos = (pos > seqStart) ? (pos - 1) : seqEnd;
			goingForward[ch] = false;
			if (pos == seqEnd) eocPulse[ch].trigger(gatePulseLenSec);
		}
		else if (curPlayMode == PM_FWD_BWD_LOOP || curPlayMode == PM_BWD_FWD_LOOP) {
			if (curPlayMode == PM_BWD_FWD_LOOP && pos < 0) goingForward[ch] = false;
			if (goingForward[ch]) {
				if (pos < seqEnd) {
					pos++;
				}
				else {
					pos = std::max(seqStart, seqEnd - 1);
					goingForward[ch] = false;
					eocPulse[ch].trigger(gatePulseLenSec);
				}
			}
			else {
				if (pos > seqStart) {
					pos--;
				}
				else {
					pos = std::min(seqEnd, seqStart + 1);
					goingForward[ch] = true;
					eocPulse[ch].trigger(gatePulseLenSec);
				}
			}
		}
		else { // PM_RANDOM_POS
			int seqSpan = std::max(1, seqEnd - seqStart + 1);
			pos = seqStart + int(random::uniform() * seqSpan);
		}

		pos = clampijw(pos, seqStart, seqEnd);
	}

	float cellVoltage(const CellProps &c) {
		int globalOct = (int)params[GLOBAL_OCTAVE_KNOB_PARAM].getValue();
		int rootNote = (int)params[NOTE_KNOB_PARAM].getValue();
		int scale = (int)params[SCALE_KNOB_PARAM].getValue();
		float rangeCtrl = clampfjw(params[VOCT_RANGE_KNOB_PARAM].getValue(), 0.f, 10.f);
		// Map range control to musically useful spread: 1 semitone .. 10 octaves.
		float totalMax = rescalefjw(rangeCtrl, 0.f, 10.f, 1.f / 12.f, 10.f);
		float voltsScaled = rescalefjw(c.pitch, 0.f, 10.f, 0.f, totalMax);
		return closestVoltageInScale((float)(c.octave + globalOct) + voltsScaled, rootNote, scale);
	}

	void process(const ProcessArgs &args) override {
		ScopedLocalRngSeed scopedSeed(inputs[SEED_INPUT].isConnected(), inputs[SEED_INPUT].getVoltage());
		float manualClockGate = 10.f * params[MANUAL_CLOCK_BTN_PARAM].getValue();
		float manualResetGate = 10.f * params[MANUAL_RESET_BTN_PARAM].getValue();

		if (selectedDirty) {
			pushSelectedCellToParams();
			selectedDirty = false;
		}
		syncParamsToSelectedCell();

		if (inputs[CLEAR_INPUT].isConnected()) {
			int clearChannels = inputs[CLEAR_INPUT].getChannels();
			if (clearChannels <= 1) {
				if (clearInputTrig[0].process(inputs[CLEAR_INPUT].getVoltage())) {
					clearGrid();
				}
			}
			else {
				for (int i = 0; i < 4; i++) {
					if (clearInputTrig[i].process(getTrackInputVoltage(CLEAR_INPUT, i))) {
						clearTrack(i);
					}
				}
			}
		}
		for (int i = 0; i < 4; i++) {
			if (clearBtnTrig[i].process(params[CLEAR1_BTN_PARAM + i].getValue())) {
				clearTrack(i);
			}
		}

		bool refreshSelectedParamsNow = false;

		if (cellRndTrig[0].process(params[CELL_PITCH_RND_BTN_PARAM].getValue())) { randomizeCellPitchAll(); refreshSelectedParamsNow = true; }
		if (cellRndTrig[1].process(params[CELL_OCTAVE_RND_BTN_PARAM].getValue())) { randomizeCellOctaveAll(); refreshSelectedParamsNow = true; }
		if (cellRndTrig[2].process(params[CELL_CV_RND_BTN_PARAM].getValue())) { randomizeCellCvAll(); refreshSelectedParamsNow = true; }
		if (cellRndTrig[3].process(params[CELL_CV2_RND_BTN_PARAM].getValue())) { randomizeCellCv2All(); refreshSelectedParamsNow = true; }
		if (cellRndTrig[4].process(params[CELL_PROB_RND_BTN_PARAM].getValue())) { randomizeCellProbAll(); refreshSelectedParamsNow = true; }
		if (cellRndTrig[5].process(params[CELL_DIV_RND_BTN_PARAM].getValue())) { randomizeCellDivAll(); refreshSelectedParamsNow = true; }
		if (cellRndTrig[6].process(params[CELL_RATCHET_RND_BTN_PARAM].getValue())) { randomizeCellRatchetAll(); refreshSelectedParamsNow = true; }
		if (cellActiveRndTrig.process(params[CELL_ACTIVE_RND_BTN_PARAM].getValue())) {
			randomizeCellActiveAll();
			refreshSelectedParamsNow = true;
		}
		if (inputs[CELL_ACTIVE_RND_INPUT].isConnected()) {
			int rndChannels = inputs[CELL_ACTIVE_RND_INPUT].getChannels();
			if (rndChannels <= 1) {
				if (cellActiveRndTrig.process(inputs[CELL_ACTIVE_RND_INPUT].getVoltage())) {
					randomizeCellActiveAll();
					refreshSelectedParamsNow = true;
				}
			}
			else {
				for (int i = 0; i < 4; i++) {
					if (cellActiveRndInputTrig[i].process(getTrackInputVoltage(CELL_ACTIVE_RND_INPUT, i))) {
						randomizeCellActiveTrack(i);
						refreshSelectedParamsNow = true;
					}
				}
			}
		}

		if (cellInitTrig[0].process(params[CELL_PITCH_INIT_BTN_PARAM].getValue())) { initCellPitchAll(); refreshSelectedParamsNow = true; }
		if (cellInitTrig[1].process(params[CELL_OCTAVE_INIT_BTN_PARAM].getValue())) { initCellOctaveAll(); refreshSelectedParamsNow = true; }
		if (cellInitTrig[2].process(params[CELL_CV_INIT_BTN_PARAM].getValue())) { initCellCvAll(); refreshSelectedParamsNow = true; }
		if (cellInitTrig[3].process(params[CELL_CV2_INIT_BTN_PARAM].getValue())) { initCellCv2All(); refreshSelectedParamsNow = true; }
		if (cellInitTrig[4].process(params[CELL_PROB_INIT_BTN_PARAM].getValue())) { initCellProbAll(); refreshSelectedParamsNow = true; }
		if (cellInitTrig[5].process(params[CELL_DIV_INIT_BTN_PARAM].getValue())) { initCellDivAll(); refreshSelectedParamsNow = true; }
		if (cellInitTrig[6].process(params[CELL_RATCHET_INIT_BTN_PARAM].getValue())) { initCellRatchetAll(); refreshSelectedParamsNow = true; }

		if (refreshSelectedParamsNow) {
			pushSelectedCellToParams();
			selectedDirty = false;
		}

		for (int i = 0; i < 4; i++) {
			float rndInput = 0.f;
			if (inputs[RND_TRIG_INPUT].isConnected()) {
				rndInput = getTrackInputVoltage(RND_TRIG_INPUT, i);
			}
			if (rndTrig[i].process(params[RND1_TRIG_BTN_PARAM + i].getValue() + rndInput)) {
				randomizeTrack(i);
				refreshSelectedParamsNow = true;
			}
		}

		if (refreshSelectedParamsNow) {
			pushSelectedCellToParams();
			selectedDirty = false;
		}

		bool resetTrack[4] = {false, false, false, false};
		bool globalReset = false;
		if (resetTrig.process(manualResetGate)) {
			globalReset = true;
			for (int i = 0; i < 4; i++) {
				resetTrack[i] = true;
			}
		}
		if (inputs[RESET_INPUT].isConnected()) {
			int resetChannels = inputs[RESET_INPUT].getChannels();
			if (resetChannels <= 1) {
				if (resetInputTrig[0].process(inputs[RESET_INPUT].getVoltage())) {
					globalReset = true;
					for (int i = 0; i < 4; i++) {
						resetTrack[i] = true;
					}
				}
			}
			else {
				for (int i = 0; i < 4; i++) {
					if (resetInputTrig[i].process(getTrackInputVoltage(RESET_INPUT, i))) {
						resetTrack[i] = true;
					}
				}
			}
		}
		for (int i = 0; i < 4; i++) {
			if (!resetTrack[i]) {
				continue;
			}
			seqPos[i] = -1;
			stepCounter[i] = 0;
			for (int c = 0; c < GRID_CELLS; c++) {
				cellVisitCounter[i][c] = 0;
			}
			resetMode[i] = false;
			goingForward[i] = true;
			ratchetsRemaining[i] = 0;
			eocPulse[i].trigger(gatePulseLenSec);
		}
		if (globalReset) {
			lifeClockCounter = 0;
			lifeStationaryLatched = false;
			lifeStoppedPulse.reset();
		}

		bool clockEdge[4] = {false, false, false, false};
		bool masterClockEdge = false;
		for (int ch = 0; ch < 4; ch++) {
			samplesSinceClock[ch]++;
			float trackClockIn = getTrackInputVoltage(CLOCK_INPUT, ch) + manualClockGate;
			if (clockTrig[ch].process(trackClockIn)) {
				clockEdge[ch] = true;
				if (samplesSinceClock[ch] > 1) {
					lastStepSamples[ch] = samplesSinceClock[ch];
				}
				samplesSinceClock[ch] = 0;
				if (ch == 0) {
					masterClockEdge = true;
				}
			}
		}

		if (masterClockEdge) {
			advanceLifeClock();
		}

		for (int ch = 0; ch < 4; ch++) {
			if (clockEdge[ch]) {
				advanceSeqPos(ch);
				int col = seqPos[ch] % GRID_COLS;
				int row = ch * 4 + (seqPos[ch] / GRID_COLS);
				int cellIndex = iFromXY(col, row);
				CellProps &c = cells[cellIndex];
				stepCounter[ch]++;
				ratchetsRemaining[ch] = 0;
				if (c.active) {
					cellVisitCounter[ch][cellIndex]++;
				}
				float effectiveProb = std::max(c.probability, minProbability);
				if (c.active && (((cellVisitCounter[ch][cellIndex] - 1) % c.division) == 0) && random::uniform() <= effectiveProb) {
					int ratchets = std::max(1, c.ratchets);
					// Fire the first pulse immediately on this clock edge.
					gatePulse[ch].trigger(gatePulseLenSec);
					ratchetsRemaining[ch] = ratchets - 1;
					ratchetInterval[ch] = std::max(1, lastStepSamples[ch] / ratchets);
					nextRatchetSample[ch] = ratchetInterval[ch];
					lastVoct[ch] = cellVoltage(c);
					lastCv[ch] = clampCvRangeValue(c.cv, cvRangeMode);
					lastCv2[ch] = clampCvRangeValue(c.cv2, cv2RangeMode);
				}
			}
		}

		float gateOut[4] = {};
		float eocOut[4] = {};
		for (int ch = 0; ch < 4; ch++) {
			while (ratchetsRemaining[ch] > 0 && samplesSinceClock[ch] >= nextRatchetSample[ch]) {
				gatePulse[ch].trigger(gatePulseLenSec);
				ratchetsRemaining[ch]--;
				nextRatchetSample[ch] += ratchetInterval[ch];
			}

			gateOut[ch] = gatePulse[ch].process(1.0f / args.sampleRate) ? 10.f : 0.f;
			eocOut[ch] = eocPulse[ch].process(1.0f / args.sampleRate) ? 10.f : 0.f;
		}

		if (polyphonicFirstRowOutputs) {
			outputs[VOCT_OUTPUT + 0].setChannels(4);
			outputs[GATE_OUTPUT + 0].setChannels(4);
			outputs[EOC_OUTPUT + 0].setChannels(4);
			outputs[CV_OUTPUT + 0].setChannels(4);
			outputs[CV2_OUTPUT + 0].setChannels(4);
			for (int ch = 0; ch < 4; ch++) {
				outputs[VOCT_OUTPUT + 0].setVoltage(lastVoct[ch], ch);
				outputs[GATE_OUTPUT + 0].setVoltage(gateOut[ch], ch);
				outputs[EOC_OUTPUT + 0].setVoltage(eocOut[ch], ch);
				outputs[CV_OUTPUT + 0].setVoltage(lastCv[ch], ch);
				outputs[CV2_OUTPUT + 0].setVoltage(lastCv2[ch], ch);
			}
			for (int ch = 1; ch < 4; ch++) {
				outputs[VOCT_OUTPUT + ch].setChannels(0);
				outputs[GATE_OUTPUT + ch].setChannels(0);
				outputs[EOC_OUTPUT + ch].setChannels(0);
				outputs[CV_OUTPUT + ch].setChannels(0);
				outputs[CV2_OUTPUT + ch].setChannels(0);
			}
		}
		else {
			for (int ch = 0; ch < 4; ch++) {
				outputs[VOCT_OUTPUT + ch].setChannels(1);
				outputs[GATE_OUTPUT + ch].setChannels(1);
				outputs[EOC_OUTPUT + ch].setChannels(1);
				outputs[CV_OUTPUT + ch].setChannels(1);
				outputs[CV2_OUTPUT + ch].setChannels(1);
				outputs[VOCT_OUTPUT + ch].setVoltage(lastVoct[ch]);
				outputs[GATE_OUTPUT + ch].setVoltage(gateOut[ch]);
				outputs[EOC_OUTPUT + ch].setVoltage(eocOut[ch]);
				outputs[CV_OUTPUT + ch].setVoltage(lastCv[ch]);
				outputs[CV2_OUTPUT + ch].setVoltage(lastCv2[ch]);
			}
		}
		float lifeStoppedOut = lifeStoppedPulse.process(1.0f / args.sampleRate) ? 10.f : 0.f;
		outputs[LIFE_STOPPED_OUTPUT].setChannels(1);
		outputs[LIFE_STOPPED_OUTPUT].setVoltage(lifeStoppedOut);
	}
};

struct Trigs128Display : LightWidget {
	Trigs128 *module = nullptr;
	Vec dragPos;
	bool dragging = false;
	bool shiftPainting = false;
	bool shiftPaintState = false;
	std::chrono::steady_clock::time_point lastClickTime = std::chrono::steady_clock::time_point::min();
	int lastClickX = -1;
	int lastClickY = -1;

	void onButton(const event::Button &e) override {
		if (!module) return;
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
			e.consume(this);
			dragging = true;
			dragPos = e.pos;
			int x = clampijw((int)(e.pos.x / (box.size.x / GRID_COLS)), 0, GRID_COLS - 1);
			int y = clampijw((int)(e.pos.y / (box.size.y / GRID_ROWS)), 0, GRID_ROWS - 1);
			bool shiftDown = (e.mods & GLFW_MOD_SHIFT) != 0;

			if (shiftDown) {
				shiftPainting = true;
				auto &c = module->cells[module->iFromXY(x, y)];
				shiftPaintState = !c.active;
				module->setCellActive(x, y, shiftPaintState);
				module->selectCell(x, y, false);
				lastClickTime = std::chrono::steady_clock::time_point::min();
				lastClickX = -1;
				lastClickY = -1;
				return;
			}

			shiftPainting = false;

			auto now = std::chrono::steady_clock::now();
			double dt = std::chrono::duration<double>(now - lastClickTime).count();
			bool isDoubleClick = (x == lastClickX && y == lastClickY && dt <= 0.3);
			// Single click selects cell only; double click toggles active state.
			module->selectCell(x, y, isDoubleClick);

			lastClickTime = now;
			lastClickX = x;
			lastClickY = y;
		}
	}

	void onDragMove(const event::DragMove &e) override {
		if (!module || !dragging) return;
		dragPos = dragPos.plus(e.mouseDelta.div(getAbsoluteZoom()));
		int x = clampijw((int)(dragPos.x / (box.size.x / GRID_COLS)), 0, GRID_COLS - 1);
		int y = clampijw((int)(dragPos.y / (box.size.y / GRID_ROWS)), 0, GRID_ROWS - 1);
		if (shiftPainting) {
			module->setCellActive(x, y, shiftPaintState);
		}
		module->selectCell(x, y, false);
	}

	void onDragEnd(const event::DragEnd &e) override {
		dragging = false;
		shiftPainting = false;
		LightWidget::onDragEnd(e);
	}

	void drawLayer(const DrawArgs &args, int layer) override {
		nvgFillColor(args.vg, nvgRGB(0, 0, 0));
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
		nvgFill(args.vg);

		if (layer == 1 && module) {
			float cw = box.size.x / GRID_COLS;
			float ch = box.size.y / GRID_ROWS;
			NVGcolor trackColors[4] = {
				nvgRGB(255, 151, 9),
				nvgRGB(255, 243, 9),
				nvgRGB(144, 26, 252),
				nvgRGB(25, 150, 252)
			};

			for (int y = 0; y < GRID_ROWS; y++) {
				for (int x = 0; x < GRID_COLS; x++) {
					auto &c = module->cells[module->iFromXY(x, y)];
					if (c.active) {
						int track = clampijw(y / 4, 0, 3);
						NVGcolor color = trackColors[track];
						float shade = module->getGridShadeNorm(c);
						color.a = clampfjw(0.35f + (shade * 0.65f), 0.f, 1.f);
						nvgFillColor(args.vg, color);
						nvgBeginPath(args.vg);
						nvgRect(args.vg, x * cw, y * ch, cw, ch);
						nvgFill(args.vg);
					}
				}
			}

			nvgStrokeColor(args.vg, nvgRGB(60, 70, 73));
			for (int x = 1; x < GRID_COLS; x++) {
				nvgStrokeWidth(args.vg, (x % 4 == 0) ? 2.f : 1.f);
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, x * cw, 0);
				nvgLineTo(args.vg, x * cw, box.size.y);
				nvgStroke(args.vg);
			}
			for (int y = 1; y < GRID_ROWS; y++) {
				nvgStrokeWidth(args.vg, (y % 4 == 0) ? 2.f : 1.f);
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, 0, y * ch);
				nvgLineTo(args.vg, box.size.x, y * ch);
				nvgStroke(args.vg);
			}

			// Draw per-track start/end-of-sequence markers (vertical white lines at step boundaries).
			nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
			nvgStrokeWidth(args.vg, 2.f);
			for (int track = 0; track < 4; track++) {
				int start = module->getSeqStart(track);
				int len = module->getSeqLen(track);
				int end = clampijw(start + len - 1, start, GRID_COLS * 4 - 1);
				int startCol = start % GRID_COLS;
				int startRow = track * 4 + (start / GRID_COLS);
				int endCol = end % GRID_COLS;
				int endRow = track * 4 + (end / GRID_COLS);
				float sx = startCol * cw;
				float x = (endCol + 1) * cw;
				float sy0 = startRow * ch;
				float sy1 = (startRow + 1) * ch;
				float y0 = endRow * ch;
				float y1 = (endRow + 1) * ch;
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, sx, sy0);
				nvgLineTo(args.vg, sx, sy1);
				nvgStroke(args.vg);
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, x, y0);
				nvgLineTo(args.vg, x, y1);
				nvgStroke(args.vg);
			}

			// Per-track playhead indicator (white box), like Trigs.
			nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
			nvgStrokeWidth(args.vg, 2.f);
			for (int track = 0; track < 4; track++) {
				int start = module->getSeqStart(track);
				int len = module->getSeqLen(track);
				int end = start + len - 1;
				int pos = module->seqPos[track];
				if (pos < start || pos > end) pos = start;
				int row = track * 4 + (pos / GRID_COLS);
				int col = pos % GRID_COLS;
				nvgBeginPath(args.vg);
				nvgRect(args.vg, col * cw, row * ch, cw, ch);
				nvgStroke(args.vg);
			}

			nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
			nvgStrokeWidth(args.vg, 4.f);
			nvgBeginPath(args.vg);
			nvgRect(args.vg, module->selectedX * cw - 2, module->selectedY * ch - 2, cw + 3, ch + 3);
			nvgStroke(args.vg);
		}

		Widget::drawLayer(args, layer);
	}
};

struct TrackTintTinyGraySnapKnob : JwTinyGraySnapKnob {
	NVGcolor tint = nvgRGBA(255, 255, 255, 70);
	void draw(const DrawArgs &args) override {
		JwTinyGraySnapKnob::draw(args);
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, box.size.x * 0.5f, box.size.y * 0.5f, box.size.x * 0.36f);
		nvgFillColor(args.vg, tint);
		nvgFill(args.vg);
	}
};

struct OrangeTrackTinyGraySnapKnob : TrackTintTinyGraySnapKnob {
	OrangeTrackTinyGraySnapKnob() { tint = nvgRGBA(255, 151, 9, 80); }
};
struct YellowTrackTinyGraySnapKnob : TrackTintTinyGraySnapKnob {
	YellowTrackTinyGraySnapKnob() { tint = nvgRGBA(255, 243, 9, 80); }
};
struct PurpleTrackTinyGraySnapKnob : TrackTintTinyGraySnapKnob {
	PurpleTrackTinyGraySnapKnob() { tint = nvgRGBA(144, 26, 252, 80); }
};
struct BlueTrackTinyGraySnapKnob : TrackTintTinyGraySnapKnob {
	BlueTrackTinyGraySnapKnob() { tint = nvgRGBA(25, 150, 252, 80); }
};

struct TrackTintTinyButton : TinyButton {
	NVGcolor tint = nvgRGBA(255, 255, 255, 70);
	void draw(const DrawArgs &args) override {
		TinyButton::draw(args);
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 1.5f, 1.5f, box.size.x - 3.f, box.size.y - 3.f, 1.8f);
		nvgFillColor(args.vg, tint);
		nvgFill(args.vg);
	}
};

static inline void drawDiceFiveIcon(NVGcontext *vg, float cx, float cy) {
	const float half = 2.6f;
	const float pipOffset = 1.45f;
	const float pipR = 0.52f;

	nvgBeginPath(vg);
	nvgRoundedRect(vg, cx - half, cy - half, half * 2.f, half * 2.f, 0.7f);
	nvgFillColor(vg, nvgRGBA(12, 12, 12, 220));
	nvgFill(vg);

	nvgFillColor(vg, nvgRGB(245, 245, 245));
	nvgBeginPath(vg);
	nvgCircle(vg, cx - pipOffset, cy - pipOffset, pipR);
	nvgCircle(vg, cx + pipOffset, cy - pipOffset, pipR);
	nvgCircle(vg, cx, cy, pipR);
	nvgCircle(vg, cx - pipOffset, cy + pipOffset, pipR);
	nvgCircle(vg, cx + pipOffset, cy + pipOffset, pipR);
	nvgFill(vg);
}

struct TinyRandomButton : TinyButton {
	void draw(const DrawArgs &args) override {
		TinyButton::draw(args);
		float tx = box.size.x * 0.5f;
		float ty = box.size.y * 0.5f + 0.2f;
		drawDiceFiveIcon(args.vg, tx, ty - 0.1f);
	}
};

struct TrackTintTinyRandomButton : TrackTintTinyButton {
	void draw(const DrawArgs &args) override {
		TrackTintTinyButton::draw(args);
		float tx = box.size.x * 0.5f;
		float ty = box.size.y * 0.5f + 0.2f;
		drawDiceFiveIcon(args.vg, tx, ty - 0.1f);
	}
};

struct OrangeTrackTinyButton : TrackTintTinyButton {
	OrangeTrackTinyButton() { tint = nvgRGBA(255, 151, 9, 85); }
};
struct YellowTrackTinyButton : TrackTintTinyButton {
	YellowTrackTinyButton() { tint = nvgRGBA(255, 243, 9, 85); }
};
struct PurpleTrackTinyButton : TrackTintTinyButton {
	PurpleTrackTinyButton() { tint = nvgRGBA(144, 26, 252, 85); }
};
struct BlueTrackTinyButton : TrackTintTinyButton {
	BlueTrackTinyButton() { tint = nvgRGBA(25, 150, 252, 85); }
};

struct OrangeTrackTinyRandomButton : TrackTintTinyRandomButton {
	OrangeTrackTinyRandomButton() {
		tint = nvgRGBA(255, 151, 9, 85);
	}
};
struct YellowTrackTinyRandomButton : TrackTintTinyRandomButton {
	YellowTrackTinyRandomButton() {
		tint = nvgRGBA(255, 243, 9, 85);
	}
};
struct PurpleTrackTinyRandomButton : TrackTintTinyRandomButton {
	PurpleTrackTinyRandomButton() {
		tint = nvgRGBA(144, 26, 252, 85);
	}
};
struct BlueTrackTinyRandomButton : TrackTintTinyRandomButton {
	BlueTrackTinyRandomButton() {
		tint = nvgRGBA(25, 150, 252, 85);
	}
};

struct Trigs128Widget : ModuleWidget {
	Trigs128Widget(Trigs128 *module) {
		setModule(module);
		box.size = Vec(RACK_GRID_WIDTH * 26, RACK_GRID_HEIGHT);

		setPanel(createPanel(
			asset::plugin(pluginInstance, "res/Trigs128.svg"),
			asset::plugin(pluginInstance, "res/Trigs128.svg")
		));

		Trigs128Display *display = new Trigs128Display();
		display->module = module;
		display->box.pos = Vec(8, 70);
		display->box.size = Vec((box.size.x - 16), 188);
		addChild(display);

		addChild(createWidget<Screw_J>(Vec(16, 2)));
		addChild(createWidget<Screw_J>(Vec(16, 365)));
		addChild(createWidget<Screw_W>(Vec(box.size.x - 29, 2)));
		addChild(createWidget<Screw_W>(Vec(box.size.x - 29, 365)));

		// Top row order: Clock, Reset, Start, Length, Mode, Clear, Random Trigger, Seed
		addInput(createInput<TinyPJ301MPort>(Vec(10, 35), module, Trigs128::CLOCK_INPUT));
		addInput(createInput<TinyPJ301MPort>(Vec(40, 35), module, Trigs128::RESET_INPUT));
		addParam(createParam<TinyButton>(Vec(11, 52), module, Trigs128::MANUAL_CLOCK_BTN_PARAM));
		addParam(createParam<TinyButton>(Vec(41, 52), module, Trigs128::MANUAL_RESET_BTN_PARAM));

		addInput(createInput<TinyPJ301MPort>(Vec(70, 35), module, Trigs128::START_INPUT));
		addParam(createParam<OrangeTrackTinyGraySnapKnob>(Vec(86, 28), module, Trigs128::START1_KNOB_PARAM));
		addParam(createParam<YellowTrackTinyGraySnapKnob>(Vec(101, 28), module, Trigs128::START2_KNOB_PARAM));
		addParam(createParam<PurpleTrackTinyGraySnapKnob>(Vec(86, 43), module, Trigs128::START3_KNOB_PARAM));
		addParam(createParam<BlueTrackTinyGraySnapKnob>(Vec(101, 43), module, Trigs128::START4_KNOB_PARAM));

		addInput(createInput<TinyPJ301MPort>(Vec(118, 35), module, Trigs128::LENGTH_INPUT));
		addParam(createParam<OrangeTrackTinyGraySnapKnob>(Vec(132, 28), module, Trigs128::LENGTH1_KNOB_PARAM));
		addParam(createParam<YellowTrackTinyGraySnapKnob>(Vec(147, 28), module, Trigs128::LENGTH2_KNOB_PARAM));
		addParam(createParam<PurpleTrackTinyGraySnapKnob>(Vec(132, 43), module, Trigs128::LENGTH3_KNOB_PARAM));
		addParam(createParam<BlueTrackTinyGraySnapKnob>(Vec(147, 43), module, Trigs128::LENGTH4_KNOB_PARAM));

		addInput(createInput<TinyPJ301MPort>(Vec(166, 35), module, Trigs128::PLAY_MODE_INPUT));
		addParam(createParam<OrangeTrackTinyGraySnapKnob>(Vec(182, 28), module, Trigs128::PLAY_MODE1_KNOB_PARAM));
		addParam(createParam<YellowTrackTinyGraySnapKnob>(Vec(197, 28), module, Trigs128::PLAY_MODE2_KNOB_PARAM));
		addParam(createParam<PurpleTrackTinyGraySnapKnob>(Vec(182, 43), module, Trigs128::PLAY_MODE3_KNOB_PARAM));
		addParam(createParam<BlueTrackTinyGraySnapKnob>(Vec(197, 43), module, Trigs128::PLAY_MODE4_KNOB_PARAM));

		addInput(createInput<TinyPJ301MPort>(Vec(214, 35), module, Trigs128::CLEAR_INPUT));
		addParam(createParam<OrangeTrackTinyButton>(Vec(230, 28), module, Trigs128::CLEAR1_BTN_PARAM));
		addParam(createParam<YellowTrackTinyButton>(Vec(245, 28), module, Trigs128::CLEAR2_BTN_PARAM));
		addParam(createParam<PurpleTrackTinyButton>(Vec(230, 43), module, Trigs128::CLEAR3_BTN_PARAM));
		addParam(createParam<BlueTrackTinyButton>(Vec(245, 43), module, Trigs128::CLEAR4_BTN_PARAM));

		addInput(createInput<TinyPJ301MPort>(Vec(258, 35), module, Trigs128::RND_TRIG_INPUT));
		addParam(createParam<OrangeTrackTinyRandomButton>(Vec(274, 28), module, Trigs128::RND1_TRIG_BTN_PARAM));
		addParam(createParam<YellowTrackTinyRandomButton>(Vec(289, 28), module, Trigs128::RND2_TRIG_BTN_PARAM));
		addParam(createParam<PurpleTrackTinyRandomButton>(Vec(274, 43), module, Trigs128::RND3_TRIG_BTN_PARAM));
		addParam(createParam<BlueTrackTinyRandomButton>(Vec(289, 43), module, Trigs128::RND4_TRIG_BTN_PARAM));
		addParam(createParam<JwTinyGrayKnob>(Vec(300, 35), module, Trigs128::RND_AMT_KNOB_PARAM));

		addInput(createInput<TinyPJ301MPort>(Vec(330, 35), module, Trigs128::SEED_INPUT));

		addParam(createParam<JwHorizontalSwitch>(Vec(8, 326), module, Trigs128::LIFE_ON_SWITCH_PARAM));
		addOutput(createOutput<TinyPJ301MPort>(Vec(10, 350), module, Trigs128::LIFE_STOPPED_OUTPUT));
		
		addInput(createInput<TinyPJ301MPort>(Vec(45, 350), module, Trigs128::CELL_ACTIVE_RND_INPUT));
		addParam(createParam<TinyRandomButton>(Vec(60, 350), module, Trigs128::CELL_ACTIVE_RND_BTN_PARAM));
		addParam(createParam<JwTinyGrayKnob>(Vec(75, 350), module, Trigs128::CELL_ACTIVE_RND_AMT_PARAM));

		float quantParamYVal = 340.f;
		float noteKnobX = 100.f;
		float scaleKnobX = 188.f;

		NoteKnob *noteKnob = dynamic_cast<NoteKnob*>(createParam<NoteKnob>(Vec(noteKnobX, quantParamYVal), module, Trigs128::NOTE_KNOB_PARAM));
		CenteredLabel* const noteLabel = new CenteredLabel;
		noteLabel->box.pos = Vec(57, 188);
		noteLabel->text = "";
		noteKnob->connectLabel(noteLabel, module);
		addChild(noteLabel);
		addParam(noteKnob);

		addParam(createParam<JwSmallSnapKnob>(Vec(143, quantParamYVal), module, Trigs128::GLOBAL_OCTAVE_KNOB_PARAM));

		ScaleKnob *scaleKnob = dynamic_cast<ScaleKnob*>(createParam<ScaleKnob>(Vec(scaleKnobX, quantParamYVal), module, Trigs128::SCALE_KNOB_PARAM));
		CenteredLabel* const scaleLabel = new CenteredLabel;
		scaleLabel->box.pos = Vec(100, 188);
		scaleLabel->text = "";
		scaleKnob->connectLabel(scaleLabel, module);
		addChild(scaleLabel);
		addParam(scaleKnob);

		addParam(createParam<JwSmallSnapKnob>(Vec(238, quantParamYVal), module, Trigs128::VOCT_RANGE_KNOB_PARAM));

		float knobsY = 284.f;
		float x0 = 28.f;
		float dx = 36.f;
		float rndBtnY = knobsY + 14.f;
		float initBtnOffsetX = 8.f;
		addParam(createParam<TinyButton>(Vec(x0 + dx * 0 - 7.f, rndBtnY + 7), module, Trigs128::CELL_PITCH_INIT_BTN_PARAM));
		addParam(createParam<TinyButton>(Vec(x0 + dx * 1 - 7.f, rndBtnY + 7), module, Trigs128::CELL_OCTAVE_INIT_BTN_PARAM));
		addParam(createParam<TinyButton>(Vec(x0 + dx * 2 - 7.f, rndBtnY + 7), module, Trigs128::CELL_PROB_INIT_BTN_PARAM));
		addParam(createParam<TinyButton>(Vec(x0 + dx * 3 - 7.f, rndBtnY + 7), module, Trigs128::CELL_DIV_INIT_BTN_PARAM));
		addParam(createParam<TinyButton>(Vec(x0 + dx * 4 - 7.f, rndBtnY + 7), module, Trigs128::CELL_RATCHET_INIT_BTN_PARAM));
		addParam(createParam<TinyButton>(Vec(x0 + dx * 5 - 7.f, rndBtnY + 7), module, Trigs128::CELL_CV_INIT_BTN_PARAM));
		addParam(createParam<TinyButton>(Vec(x0 + dx * 6 - 7.f, rndBtnY + 7), module, Trigs128::CELL_CV2_INIT_BTN_PARAM));
		addParam(createParam<TinyRandomButton>(Vec(x0 + dx * 0 + initBtnOffsetX, rndBtnY + 7), module, Trigs128::CELL_PITCH_RND_BTN_PARAM));
		addParam(createParam<TinyRandomButton>(Vec(x0 + dx * 1 + initBtnOffsetX, rndBtnY + 7), module, Trigs128::CELL_OCTAVE_RND_BTN_PARAM));
		addParam(createParam<TinyRandomButton>(Vec(x0 + dx * 2 + initBtnOffsetX, rndBtnY + 7), module, Trigs128::CELL_PROB_RND_BTN_PARAM));
		addParam(createParam<TinyRandomButton>(Vec(x0 + dx * 3 + initBtnOffsetX, rndBtnY + 7), module, Trigs128::CELL_DIV_RND_BTN_PARAM));
		addParam(createParam<TinyRandomButton>(Vec(x0 + dx * 4 + initBtnOffsetX, rndBtnY + 7), module, Trigs128::CELL_RATCHET_RND_BTN_PARAM));
		addParam(createParam<TinyRandomButton>(Vec(x0 + dx * 5 + initBtnOffsetX, rndBtnY + 7), module, Trigs128::CELL_CV_RND_BTN_PARAM));
		addParam(createParam<TinyRandomButton>(Vec(x0 + dx * 6 + initBtnOffsetX, rndBtnY + 7), module, Trigs128::CELL_CV2_RND_BTN_PARAM));
		addParam(createParam<JwTinyKnob>(Vec(x0 + dx * 0, knobsY), module, Trigs128::CELL_PITCH_PARAM));
		addParam(createParam<JwTinySnapKnob>(Vec(x0 + dx * 1, knobsY), module, Trigs128::CELL_OCTAVE_PARAM));
		addParam(createParam<JwTinyKnob>(Vec(x0 + dx * 2, knobsY), module, Trigs128::CELL_PROB_PARAM));
		addParam(createParam<JwTinySnapKnob>(Vec(x0 + dx * 3, knobsY), module, Trigs128::CELL_DIV_PARAM));
		addParam(createParam<JwTinySnapKnob>(Vec(x0 + dx * 4, knobsY), module, Trigs128::CELL_RATCHET_PARAM));
		addParam(createParam<JwTinyKnob>(Vec(x0 + dx * 5, knobsY), module, Trigs128::CELL_CV_PARAM));
		addParam(createParam<JwTinyKnob>(Vec(x0 + dx * 6, knobsY), module, Trigs128::CELL_CV2_PARAM));

		float outX = box.size.x - 115.f;
		for (int i = 0; i < 4; i++) {
			float oy = 280.f + i * 23.f;
			if (i == 0) {
				addOutput(createOutput<Orange_TinyPJ301MPort>(Vec(outX, oy), module, Trigs128::VOCT_OUTPUT + i));
				addOutput(createOutput<Orange_TinyPJ301MPort>(Vec(outX + 23.f, oy), module, Trigs128::GATE_OUTPUT + i));
				addOutput(createOutput<Orange_TinyPJ301MPort>(Vec(outX + 46.f, oy), module, Trigs128::CV_OUTPUT + i));
				addOutput(createOutput<Orange_TinyPJ301MPort>(Vec(outX + 69.f, oy), module, Trigs128::CV2_OUTPUT + i));
				addOutput(createOutput<Orange_TinyPJ301MPort>(Vec(outX + 92.f, oy), module, Trigs128::EOC_OUTPUT + i));
			}
			else if (i == 1) {
				addOutput(createOutput<Yellow_TinyPJ301MPort>(Vec(outX, oy), module, Trigs128::VOCT_OUTPUT + i));
				addOutput(createOutput<Yellow_TinyPJ301MPort>(Vec(outX + 23.f, oy), module, Trigs128::GATE_OUTPUT + i));
				addOutput(createOutput<Yellow_TinyPJ301MPort>(Vec(outX + 46.f, oy), module, Trigs128::CV_OUTPUT + i));
				addOutput(createOutput<Yellow_TinyPJ301MPort>(Vec(outX + 69.f, oy), module, Trigs128::CV2_OUTPUT + i));
				addOutput(createOutput<Yellow_TinyPJ301MPort>(Vec(outX + 92.f, oy), module, Trigs128::EOC_OUTPUT + i));
			}
			else if (i == 2) {
				addOutput(createOutput<Purple_TinyPJ301MPort>(Vec(outX, oy), module, Trigs128::VOCT_OUTPUT + i));
				addOutput(createOutput<Purple_TinyPJ301MPort>(Vec(outX + 23.f, oy), module, Trigs128::GATE_OUTPUT + i));
				addOutput(createOutput<Purple_TinyPJ301MPort>(Vec(outX + 46.f, oy), module, Trigs128::CV_OUTPUT + i));
				addOutput(createOutput<Purple_TinyPJ301MPort>(Vec(outX + 69.f, oy), module, Trigs128::CV2_OUTPUT + i));
				addOutput(createOutput<Purple_TinyPJ301MPort>(Vec(outX + 92.f, oy), module, Trigs128::EOC_OUTPUT + i));
			}
			else {
				addOutput(createOutput<Blue_TinyPJ301MPort>(Vec(outX, oy), module, Trigs128::VOCT_OUTPUT + i));
				addOutput(createOutput<Blue_TinyPJ301MPort>(Vec(outX + 23.f, oy), module, Trigs128::GATE_OUTPUT + i));
				addOutput(createOutput<Blue_TinyPJ301MPort>(Vec(outX + 46.f, oy), module, Trigs128::CV_OUTPUT + i));
				addOutput(createOutput<Blue_TinyPJ301MPort>(Vec(outX + 69.f, oy), module, Trigs128::CV2_OUTPUT + i));
				addOutput(createOutput<Blue_TinyPJ301MPort>(Vec(outX + 92.f, oy), module, Trigs128::EOC_OUTPUT + i));
			}
		}
	}

	void appendContextMenu(Menu *menu) override {
		Trigs128 *module = dynamic_cast<Trigs128*>(this->module);
		menu->addChild(new MenuSeparator());
		MenuLabel *helpLabel1 = new MenuLabel();
		helpLabel1->text = "Hold shift and click to paint cells on/off.";
		menu->addChild(helpLabel1);
		MenuLabel *helpLabel2 = new MenuLabel();
		helpLabel2->text = "Double-click a cell to toggle its active state.";
		menu->addChild(helpLabel2);

		struct UniformLengthQuantity : Quantity {
			Trigs128 *module = nullptr;
			void setValue(float value) override {
				if (!module) return;
				module->uniformTrackLength = clampijw((int)std::round(value), 1, GRID_COLS * 4);
			}
			float getValue() override {
				if (!module) return (float)(GRID_COLS * 4);
				return (float)module->uniformTrackLength;
			}
			float getMinValue() override { return 1.f; }
			float getMaxValue() override { return (float)(GRID_COLS * 4); }
			float getDefaultValue() override { return (float)(GRID_COLS * 4); }
			float getDisplayValue() override { return getValue(); }
			void setDisplayValue(float displayValue) override { setValue(displayValue); }
			int getDisplayPrecision() override { return 0; }
			std::string getLabel() override { return "Uniform Track Length"; }
			std::string getDisplayValueString() override {
				return string::f("%d", (int)std::round(getDisplayValue()));
			}
		};

		struct ApplyUniformLengthItem : MenuItem {
			Trigs128 *module = nullptr;
			void onAction(const event::Action &e) override {
				if (!module) return;
				module->setAllTrackLengths(module->uniformTrackLength);
			}
		};

		struct RepeatTrackLengthItem : MenuItem {
			Trigs128 *module = nullptr;
			int track = 0;
			void onAction(const event::Action &e) override {
				if (!module) return;
				module->repeatTrackSelectedLengthToMax(track);
			}
		};

		struct CopyTrackItem : MenuItem {
			Trigs128 *module = nullptr;
			int track = 0;
			void onAction(const event::Action &e) override {
				if (!module) return;
				module->copyTrackToClipboard(track);
			}
		};

		struct PasteTrackItem : MenuItem {
			Trigs128 *module = nullptr;
			int track = 0;
			void onAction(const event::Action &e) override {
				if (!module) return;
				module->pasteClipboardToTrack(track);
			}
			void step() override {
				disabled = !(module && module->trackClipboardValid);
				MenuItem::step();
			}
		};

		struct TrackActionsMenuItem : MenuItem {
			Trigs128 *module = nullptr;
			int track = 0;
			Menu *createChildMenu() override {
				Menu *child = new Menu;

				RepeatTrackLengthItem *repeatItem = new RepeatTrackLengthItem;
				repeatItem->module = module;
				repeatItem->track = track;
				repeatItem->text = "Repeat track length across all steps";
				child->addChild(repeatItem);

				CopyTrackItem *copyItem = new CopyTrackItem;
				copyItem->module = module;
				copyItem->track = track;
				copyItem->text = "Copy";
				child->addChild(copyItem);

				PasteTrackItem *pasteItem = new PasteTrackItem;
				pasteItem->module = module;
				pasteItem->track = track;
				pasteItem->text = "Paste";
				child->addChild(pasteItem);

				return child;
			}
			void step() override {
				rightText = "▶";
				MenuItem::step();
			}
		};

		struct CopyCellItem : MenuItem {
			Trigs128 *module = nullptr;
			void onAction(const event::Action &e) override {
				if (!module) return;
				module->copySelectedCellToClipboard();
			}
		};

		struct PasteCellItem : MenuItem {
			Trigs128 *module = nullptr;
			void onAction(const event::Action &e) override {
				if (!module) return;
				module->pasteClipboardToSelectedCell();
			}
			void step() override {
				disabled = !(module && module->cellClipboardValid);
				MenuItem::step();
			}
		};

		struct PolyphonicFirstRowOutputsItem : MenuItem {
			Trigs128 *module = nullptr;
			void onAction(const event::Action &e) override {
				if (!module) return;
				module->polyphonicFirstRowOutputs = !module->polyphonicFirstRowOutputs;
			}
			void step() override {
				rightText = (module && module->polyphonicFirstRowOutputs) ? "✔" : "";
				MenuItem::step();
			}
		};

		struct CvRangeChoiceItem : MenuItem {
			Trigs128 *module = nullptr;
			bool secondOutput = false;
			int mode = 0;
			void onAction(const event::Action &e) override {
				if (!module) return;
				module->setCvRangeMode(secondOutput, mode);
			}
			void step() override {
				int currentMode = secondOutput ? Trigs128::CV_RANGE_BIPOLAR_10V : Trigs128::CV_RANGE_BIPOLAR_10V;
				if (module) {
					currentMode = secondOutput ? module->cv2RangeMode : module->cvRangeMode;
				}
				rightText = (module && currentMode == mode) ? "✔" : "";
				MenuItem::step();
			}
		};

		struct CvRangeMenuItem : MenuItem {
			Trigs128 *module = nullptr;
			bool secondOutput = false;
			Menu *createChildMenu() override {
				Menu *child = new Menu;
				for (int mode = 0; mode < Trigs128::NUM_CV_RANGE_MODES; mode++) {
					CvRangeChoiceItem *item = new CvRangeChoiceItem;
					item->module = module;
					item->secondOutput = secondOutput;
					item->mode = mode;
					item->text = Trigs128::getCvRangeSpec(mode).label;
					child->addChild(item);
				}
				return child;
			}
			void step() override {
				rightText = "▶";
				MenuItem::step();
			}
		};

		struct GridShadeChoiceItem : MenuItem {
			Trigs128 *module = nullptr;
			int mode = 0;
			void onAction(const event::Action &e) override {
				if (!module) return;
				module->gridShadeMode = clampijw(mode, 0, Trigs128::NUM_GRID_SHADE_MODES - 1);
			}
			void step() override {
				rightText = (module && module->gridShadeMode == mode) ? "✔" : "";
				MenuItem::step();
			}
		};

		struct GridShadeMenuItem : MenuItem {
			Trigs128 *module = nullptr;
			Menu *createChildMenu() override {
				Menu *child = new Menu;
				for (int mode = 0; mode < Trigs128::NUM_GRID_SHADE_MODES; mode++) {
					GridShadeChoiceItem *item = new GridShadeChoiceItem;
					item->module = module;
					item->mode = mode;
					item->text = Trigs128::getGridShadeLabel(mode);
					child->addChild(item);
				}
				return child;
			}
			void step() override {
				rightText = "▶";
				MenuItem::step();
			}
		};

		struct LifeRateQuantity : Quantity {
			Trigs128 *module = nullptr;
			void setValue(float value) override {
				if (!module) return;
				module->lifeRateMode = clampijw((int)std::round(value), 0, Trigs128::NUM_LIFE_RATE_MODES - 1);
				module->lifeClockCounter = 0;
			}
			float getValue() override {
				if (!module) return (float)Trigs128::LIFE_RATE_X1;
				return (float)module->lifeRateMode;
			}
			float getMinValue() override { return 0.f; }
			float getMaxValue() override { return (float)(Trigs128::NUM_LIFE_RATE_MODES - 1); }
			float getDefaultValue() override { return (float)Trigs128::LIFE_RATE_X1; }
			float getDisplayValue() override { return getValue(); }
			void setDisplayValue(float displayValue) override { setValue(displayValue); }
			int getDisplayPrecision() override { return 0; }
			std::string getLabel() override { return "Game of Life Rate"; }
			std::string getDisplayValueString() override {
				return Trigs128::getLifeRateLabel((int)std::round(getValue()));
			}
		};

		struct MinProbabilityQuantity : Quantity {
			Trigs128 *module = nullptr;
			void setValue(float value) override {
				if (!module) return;
				module->minProbability = clampfjw(value, 0.f, 1.f);
			}
			float getValue() override {
				if (!module) return 0.f;
				return module->minProbability;
			}
			float getMinValue() override { return 0.f; }
			float getMaxValue() override { return 1.f; }
			float getDefaultValue() override { return 0.f; }
			float getDisplayValue() override { return getValue() * 100.f; }
			void setDisplayValue(float displayValue) override { setValue(displayValue / 100.f); }
			int getDisplayPrecision() override { return 0; }
			std::string getLabel() override { return "Minimum Probability"; }
			std::string getUnit() override { return "%"; }
			std::string getDisplayValueString() override {
				return string::f("%d", (int)std::round(getDisplayValue()));
			}
		};

		menu->addChild(new MenuSeparator());
		MenuLabel *gatePulseLabel = new MenuLabel();
		gatePulseLabel->text = "Gate Length";
		menu->addChild(gatePulseLabel);

		GatePulseMsSlider* gateSlider = new GatePulseMsSlider();
		{
			auto qp = static_cast<GatePulseMsQuantity*>(gateSlider->quantity);
			qp->getSeconds = [module](){ return module->gatePulseLenSec; };
			qp->setSeconds = [module](float v){ module->gatePulseLenSec = v; };
			qp->defaultSeconds = 0.005f;
			qp->label = "Gate Length";
		}
		gateSlider->box.size.x = 175.f;
		menu->addChild(gateSlider);

		MenuLabel *uniformLengthLabel = new MenuLabel();
		uniformLengthLabel->text = "Track Length";
		menu->addChild(uniformLengthLabel);

		ui::Slider *uniformLengthSlider = new ui::Slider;
		UniformLengthQuantity *uniformLengthQuantity = new UniformLengthQuantity;
		uniformLengthQuantity->module = module;
		uniformLengthSlider->quantity = uniformLengthQuantity;
		uniformLengthSlider->box.size.x = 175.f;
		menu->addChild(uniformLengthSlider);

		ApplyUniformLengthItem *applyUniformLengthItem = new ApplyUniformLengthItem;
		applyUniformLengthItem->module = module;
		applyUniformLengthItem->text = "Set All Track Lengths To Selected";
		menu->addChild(applyUniformLengthItem);

		MenuLabel *minProbabilityLabel = new MenuLabel();
		minProbabilityLabel->text = "Minimum Probability";
		menu->addChild(minProbabilityLabel);

		ui::Slider *minProbabilitySlider = new ui::Slider;
		MinProbabilityQuantity *minProbabilityQuantity = new MinProbabilityQuantity;
		minProbabilityQuantity->module = module;
		minProbabilitySlider->quantity = minProbabilityQuantity;
		minProbabilitySlider->box.size.x = 175.f;
		menu->addChild(minProbabilitySlider);

		CopyCellItem *copyCellItem = new CopyCellItem;
		copyCellItem->module = module;
		copyCellItem->text = "Copy Cell";
		menu->addChild(copyCellItem);

		PasteCellItem *pasteCellItem = new PasteCellItem;
		pasteCellItem->module = module;
		pasteCellItem->text = "Paste Cell";
		menu->addChild(pasteCellItem);

		PolyphonicFirstRowOutputsItem *polyphonicFirstRowOutputsItem = new PolyphonicFirstRowOutputsItem;
		polyphonicFirstRowOutputsItem->module = module;
		polyphonicFirstRowOutputsItem->text = "Polyphonic Outputs On First Row";
		menu->addChild(polyphonicFirstRowOutputsItem);

		CvRangeMenuItem *cvRangeItem = new CvRangeMenuItem;
		cvRangeItem->module = module;
		cvRangeItem->text = "CV Range";
		menu->addChild(cvRangeItem);

		CvRangeMenuItem *cv2RangeItem = new CvRangeMenuItem;
		cv2RangeItem->module = module;
		cv2RangeItem->secondOutput = true;
		cv2RangeItem->text = "CV2 Range";
		menu->addChild(cv2RangeItem);

		GridShadeMenuItem *gridShadeItem = new GridShadeMenuItem;
		gridShadeItem->module = module;
		gridShadeItem->text = "Grid Shading";
		menu->addChild(gridShadeItem);

		menu->addChild(new MenuSeparator());
		ui::Slider *lifeRateSlider = new ui::Slider;
		LifeRateQuantity *lifeRateQuantity = new LifeRateQuantity;
		lifeRateQuantity->module = module;
		lifeRateSlider->quantity = lifeRateQuantity;
		lifeRateSlider->box.size.x = 175.f;
		menu->addChild(lifeRateSlider);

		MenuLabel *trackToolsLabel = new MenuLabel();
		trackToolsLabel->text = "Track Actions";
		menu->addChild(trackToolsLabel);

		const char *trackNames[4] = {"Orange", "Yellow", "Purple", "Blue"};
		for (int t = 0; t < 4; t++) {
			TrackActionsMenuItem *trackMenuItem = new TrackActionsMenuItem;
			trackMenuItem->module = module;
			trackMenuItem->track = t;
			trackMenuItem->text = trackNames[t];
			menu->addChild(trackMenuItem);
		}
	}
};

Model *modelTrigs128 = createModel<Trigs128, Trigs128Widget>("Trigs128");
