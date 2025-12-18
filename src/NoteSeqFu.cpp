#include <string.h>
#include <algorithm>
#include "JWModules.hpp"

#define ROWS 32
#define COLS 32
#define CELLS 1024
#define POLY 16
#define HW 11.75 //cell height and width

struct ColNotes {
	int *vals = new int[32];
	bool includeInactive;
	bool valid;
	int finalHigh;
	int finalLow;

	~ColNotes() {
		delete [] vals;
	}
};

struct PlayHead {
	dsp::PulseGenerator gatePulse;
	int seqPos = 0;
	int ticksSinceDivision = 0;
	bool goingForward = true;
	bool eocOn = false; 
};

struct NoteSeqFu : Module,QuantizeUtils {
	enum ParamIds {
		STEP_BTN_PARAM,
		RESET_BTN_PARAM,
		CLEAR_BTN_PARAM,
		RND_MODE_KNOB_PARAM,
		RND_TRIG_BTN_PARAM,
		RND_AMT_KNOB_PARAM,
		ROT_RIGHT_BTN_PARAM,
		ROT_LEFT_BTN_PARAM,
		FLIP_HORIZ_BTN_PARAM,
		FLIP_VERT_BTN_PARAM,
		SHIFT_UP_BTN_PARAM,
		SHIFT_DOWN_BTN_PARAM,
		LIFE_ON_SWITCH_PARAM,
		LIFE_SPEED_KNOB_PARAM,
		SCALE_KNOB_PARAM,
		NOTE_KNOB_PARAM,
		LOW_HIGH_SWITCH_PARAM,
		INCLUDE_INACTIVE_PARAM,
		SHIFT_AMT_KNOB_PARAM,
		PLAY_MODE_KNOB_PARAM, //EACH PLAYHEAD
		START_KNOB_PARAM = PLAY_MODE_KNOB_PARAM + 4, //EACH PLAYHEAD
		DIVISION_KNOB_PARAM = START_KNOB_PARAM + 4, //EACH PLAYHEAD
		OCTAVE_KNOB_PARAM = DIVISION_KNOB_PARAM + 4, //EACH PLAYHEAD
		SEMI_KNOB_PARAM = OCTAVE_KNOB_PARAM + 4, //EACH PLAYHEAD
		LENGTH_KNOB_PARAM = SEMI_KNOB_PARAM + 4, //EACH PLAYHEAD
		REPEATS_PARAM = LENGTH_KNOB_PARAM + 4, //EACH PLAYHEAD
		PLAYHEAD_ON_PARAM,
		SHIFT_CHAOS_BTN_PARAM = PLAYHEAD_ON_PARAM + 4,
		HIGHEST_NOTE_PARAM,
		LOWEST_NOTE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		RESET_INPUT,
		CLEAR_INPUT,
		RND_TRIG_INPUT,
		RND_AMT_INPUT,
		ROT_RIGHT_INPUT,
		ROT_LEFT_INPUT,
		FLIP_HORIZ_INPUT,
		FLIP_VERT_INPUT,
		SHIFT_UP_INPUT,
		SHIFT_DOWN_INPUT,
		ROOT_INPUT,
		SCALE_INPUT,
		LENGTH_INPUT,
		SHIFT_AMT_INPUT,
		SHIFT_CHAOS_INPUT,
		HIGHEST_NOTE_INPUT,
		LOWEST_NOTE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		POLY_VOCT_OUTPUT, //EACH PLAYHEAD
		POLY_GATE_OUTPUT = POLY_VOCT_OUTPUT + 4, //EACH PLAYHEAD
		EOC_OUTPUT = POLY_GATE_OUTPUT + 4, //EACH PLAYHEAD
		MERGED_VOCT_OUTPUT = EOC_OUTPUT + 4,
		MERGED_GATE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};
	enum PlayMode {
		PM_FWD_LOOP,
		PM_BWD_LOOP,
		PM_FWD_BWD_LOOP,
		PM_BWD_FWD_LOOP,
		PM_RANDOM_POS,
		NUM_PLAY_MODES
	};
	enum RndMode {
		RND_BASIC,
		RND_EUCLID,
		RND_SIN_WAVE,
		RND_LIFE_GLIDERS,
		RND_CHORDS,
		RND_MIRROR_X,
		RND_MIRROR_Y,
		NUM_RND_MODES
	};
	enum ShiftDirection {
		DIR_UP,
		DIR_DOWN,
		DIR_CHAOS
	};
	enum RotateDirection {
		DIR_LEFT,
		DIR_RIGHT
	};
	enum FlipDirection {
		DIR_HORIZ,
		DIR_VERT
	};

	float displayWidth = 0, displayHeight = 0;
	float rate = 1.0 / APP->engine->getSampleRate();
	long lifeCounter = 0;
	PlayHead *playHeads = new PlayHead[4];
	int channels = 1;
	float rndFloat0to1AtClockStep = random::uniform();
	bool resetMode = false;
	bool *hitEnd = new bool[4];
	bool *cells = new bool[CELLS];
	bool *newCells = new bool[CELLS];
	ColNotes *colNotesCache = new ColNotes[COLS];
	ColNotes *colNotesCache2 = new ColNotes[COLS];
	dsp::SchmittTrigger clockTrig, resetTrig, clearTrig;
	dsp::SchmittTrigger rndTrig, shiftUpTrig, shiftDownTrig, shiftChaosTrig;
	dsp::SchmittTrigger rotateRightTrig, rotateLeftTrig, flipHorizTrig, flipVertTrig;
	dsp::PulseGenerator mainClockPulse;

	// Gate pulse length in seconds (for TRIGGER/RETRIGGER modes)
	float gatePulseLenSec = 0.005f;

	enum GateMode { TRIGGER, RETRIGGER, CONTINUOUS };
	GateMode gateMode = TRIGGER;

	NoteSeqFu() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(STEP_BTN_PARAM, 0.0, 1.0, 0.0, "Step");
		configParam(RESET_BTN_PARAM, 0.0, 1.0, 0.0, "Reset");
		configParam(CLEAR_BTN_PARAM, 0.0, 1.0, 0.0, "Clear");
		configParam(RND_MODE_KNOB_PARAM, 0.0, NUM_RND_MODES - 1, 0.0, "Random Mode");
		configParam(RND_TRIG_BTN_PARAM, 0.0, 1.0, 0.0, "Random Trigger");
		configParam(RND_AMT_KNOB_PARAM, 0.0, 1.0, 0.1, "Random Amount");
		configParam(SHIFT_UP_BTN_PARAM, 0.0, 1.0, 0.0, "Shift Up");
		configParam(SHIFT_DOWN_BTN_PARAM, 0.0, 1.0, 0.0, "Shift Down");
		configParam(SHIFT_CHAOS_BTN_PARAM, 0.0, 1.0, 0.0, "Shift Chaos");
		configParam(SHIFT_AMT_KNOB_PARAM, -32, 32.0, 1.0, "Shift Amount");
		configParam(ROT_RIGHT_BTN_PARAM, 0.0, 1.0, 0.0, "Rotate Right");
		configParam(ROT_LEFT_BTN_PARAM, 0.0, 1.0, 0.0, "Rotate Left");
		configParam(FLIP_HORIZ_BTN_PARAM, 0.0, 1.0, 0.0, "Flip Horizontal");
		configParam(FLIP_VERT_BTN_PARAM, 0.0, 1.0, 0.0, "Flip Vertical");
		configParam(LIFE_ON_SWITCH_PARAM, 0.0, 1.0, 0.0, "Life Switch");
		configParam(LIFE_SPEED_KNOB_PARAM, 1.0, 16.0, 12.0, "Life Speed");
		configParam(INCLUDE_INACTIVE_PARAM, 0.0, 1.0, 0.0, "Drum Mode");
		configParam(HIGHEST_NOTE_PARAM, 1.0, 32.0, 32.0, "Highest Note");
		configParam(LOWEST_NOTE_PARAM, 1.0, 32.0, 1.0, "Lowest Note");
		configParam(REPEATS_PARAM, 0.0, 1.0, 0.0, "Repeats");
		configParam(NOTE_KNOB_PARAM, 0.0, QuantizeUtils::NUM_NOTES-1, QuantizeUtils::NOTE_C, "Root Note");
		configParam(SCALE_KNOB_PARAM, 0.0, QuantizeUtils::NUM_SCALES-1, QuantizeUtils::MINOR, "Scale");
		for(int i=0;i<4;i++){
			configParam(LENGTH_KNOB_PARAM + i, 1.0, 32.0, 32.0 - i, "Length");
			configParam(PLAYHEAD_ON_PARAM + i, 0.0, 1.0, 1.0, "Play Head On/Off");
			configParam(START_KNOB_PARAM + i, 0.0, 31.0, i, "Start Offset");
			configParam(PLAY_MODE_KNOB_PARAM + i, 0.0, NUM_PLAY_MODES - 1, 0.0, "Play Mode");
			configParam(OCTAVE_KNOB_PARAM + i, -5.0, 7.0, 0.0, "Octave");
			configParam(SEMI_KNOB_PARAM + i, -11.0, 11.0, 0.0, "Semitones");
			configParam(DIVISION_KNOB_PARAM + i, 1, 32.0, 1.0, "Division");
		}
		configInput(CLOCK_INPUT, "Clock");
		configInput(RESET_INPUT, "Reset");
		configInput(CLEAR_INPUT, "Clear");
		configInput(RND_TRIG_INPUT, "Random Trigger");
		configInput(RND_AMT_INPUT, "Random Amount");
		configInput(ROT_RIGHT_INPUT, "Rotate Right");
		configInput(ROT_LEFT_INPUT, "Rotate Left");
		configInput(FLIP_HORIZ_INPUT, "Flip Horizontally");
		configInput(FLIP_VERT_INPUT, "Flip Vertically");
		configInput(SHIFT_UP_INPUT, "Shift Up");
		configInput(SHIFT_DOWN_INPUT, "Shift Down");
		configInput(HIGHEST_NOTE_INPUT, "Highest Note");
		configInput(LOWEST_NOTE_INPUT, "Lowest Note");
		configInput(ROOT_INPUT, "Root");
		configInput(SCALE_INPUT, "Scale");
		configInput(LENGTH_INPUT, "Length");
		configInput(SHIFT_AMT_INPUT, "Shift Amount");
		configInput(SHIFT_CHAOS_INPUT, "Shift Chaos");

		const char *colors[4] = { "Orange", "Yellow", "Purple", "Blue" };
		for(int i=0;i<4;i++){
			configOutput(POLY_VOCT_OUTPUT + i, "Poly V/Oct " + std::string(colors[i]));
			configOutput(POLY_GATE_OUTPUT + i, "Poly Gate " + std::string(colors[i]));
			configOutput(EOC_OUTPUT + i, "End of Cycle " + std::string(colors[i]));
		}
		configOutput(MERGED_VOCT_OUTPUT, "Merged Poly V/Oct");
		configOutput(MERGED_GATE_OUTPUT, "Merged Poly Gate");

		resetSeq();
		resetMode = true;
		clearCells();
	}

	~NoteSeqFu() {
		delete [] cells;
		delete [] newCells;
		delete [] colNotesCache;
		delete [] colNotesCache2;
		delete [] playHeads;
		delete [] hitEnd;
	}

	void onRandomize() override {
		randomizeCells();
	}

	void onReset() override {
		resetSeq();
		resetMode = true;
		clearCells();
		lifeCounter = 0;
	}

	void onSampleRateChange() override {
		rate = 1.0 / APP->engine->getSampleRate();
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "channels", json_integer(channels));
		
		json_t *cellsJ = json_array();
		for (int i = 0; i < CELLS; i++) {
			json_t *cellJ = json_integer((int) cells[i]);
			json_array_append_new(cellsJ, cellJ);
		}
		json_object_set_new(rootJ, "cells", cellsJ);
		
		// gateMode
		json_t *gateModeJ = json_integer((int) gateMode);
		json_object_set_new(rootJ, "gateMode", gateModeJ);

		// gate pulse length
		json_object_set_new(rootJ, "gatePulseLenSec", json_real(gatePulseLenSec));

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *channelsJ = json_object_get(rootJ, "channels");
		if (channelsJ){
			channels = json_integer_value(channelsJ);
		} else {
			channels = 4;//hopefully this works for old patches
		}

		json_t *cellsJ = json_object_get(rootJ, "cells");
		if (cellsJ) {
			for (int i = 0; i < CELLS; i++) {
				json_t *cellJ = json_array_get(cellsJ, i);
				if (cellJ)
					cells[i] = json_integer_value(cellJ);
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
			gatePulseLenSec = clampfjw(gatePulseLenSec, 0.001f, 1.0f);
		}
		
		gridChanged();
	}

	void process(const ProcessArgs &args) override {
		if (clearTrig.process(params[CLEAR_BTN_PARAM].getValue() + inputs[CLEAR_INPUT].getVoltage())) { clearCells(); }
		if (rndTrig.process(params[RND_TRIG_BTN_PARAM].getValue() + inputs[RND_TRIG_INPUT].getVoltage())) { randomizeCells(); }

		if (rotateRightTrig.process(params[ROT_RIGHT_BTN_PARAM].getValue() + inputs[ROT_RIGHT_INPUT].getVoltage())) { rotateCells(DIR_RIGHT); }
		if (rotateLeftTrig.process(params[ROT_LEFT_BTN_PARAM].getValue() + inputs[ROT_LEFT_INPUT].getVoltage())) { rotateCells(DIR_LEFT); }

		if (flipHorizTrig.process(params[FLIP_HORIZ_BTN_PARAM].getValue() + inputs[FLIP_HORIZ_INPUT].getVoltage())) { flipCells(DIR_HORIZ); }
		if (flipVertTrig.process(params[FLIP_VERT_BTN_PARAM].getValue() + inputs[FLIP_VERT_INPUT].getVoltage())) { flipCells(DIR_VERT); }

		if (shiftUpTrig.process(params[SHIFT_UP_BTN_PARAM].getValue() + inputs[SHIFT_UP_INPUT].getVoltage())) { shiftCells(DIR_UP); }
		if (shiftDownTrig.process(params[SHIFT_DOWN_BTN_PARAM].getValue() + inputs[SHIFT_DOWN_INPUT].getVoltage())) { shiftCells(DIR_DOWN); }
		if (shiftChaosTrig.process(params[SHIFT_CHAOS_BTN_PARAM].getValue() + inputs[SHIFT_CHAOS_INPUT].getVoltage())) { shiftCells(DIR_CHAOS); }

		if (resetTrig.process(params[RESET_BTN_PARAM].getValue() + inputs[RESET_INPUT].getVoltage())) {
			resetMode = true;
			lifeCounter = 0;
		}

		if (clockTrig.process(inputs[CLOCK_INPUT].getVoltage() + params[STEP_BTN_PARAM].getValue())) {
			if(resetMode){
				resetMode = false;
				for(int i=0;i<4;i++){
					hitEnd[i] = false;
				}
				resetSeqToEnd();
			}
			clockStep();
		}

		if(params[LIFE_ON_SWITCH_PARAM].getValue()){
			if(lifeCounter % int(17.0 - params[LIFE_SPEED_KNOB_PARAM].getValue()) == 0){ 
				stepLife();
				lifeCounter = 1;
			}
		}

		// ////////////////////////////////////////////// POLY OUTPUTS //////////////////////////////////////////////
		bool mainPulse = mainClockPulse.process(1.0 / args.sampleRate);
		int maxChannelsPerPlayhead = fminf(4, channels);
		for(int p=0;p<4;p++){
			if(params[PLAYHEAD_ON_PARAM + p].getValue()){
				bool pulse = (mainPulse && params[REPEATS_PARAM].getValue()) || (playHeads[p].gatePulse.process(1.0 / args.sampleRate));
				int seqPos = playHeads[p].seqPos;
				int *polyYVals = getYValsFromBottomAtSeqPos(params[INCLUDE_INACTIVE_PARAM].getValue(), seqPos);
				for(int i=0;i<channels;i++){
					bool hasVal = polyYVals[i] > -1;
					bool cellActive = hasVal && cells[iFromXY(seqPos, ROWS - polyYVals[i] - 1)];
					if(cellActive){ 
						float volts = closestVoltageForRow(polyYVals[i], p);
						outputs[POLY_VOCT_OUTPUT + p].setVoltage(volts, i);
						if(i < 4){ //first four since we are merging 4 playheads into a max 16 channels
							outputs[MERGED_VOCT_OUTPUT].setVoltage(volts, i + p * maxChannelsPerPlayhead);
						}
					}
					bool gateOn = cellActive;
					if (gateMode == TRIGGER) gateOn = gateOn && pulse;
					else if (gateMode == RETRIGGER) gateOn = gateOn && !pulse;
					float gateVolts = gateOn ? 10.0 : 0.0;
					outputs[POLY_GATE_OUTPUT + p].setVoltage(gateVolts, i);
					if(i < 4){ //first four since we are merging 4 playheads into a max 16 channels
						outputs[MERGED_GATE_OUTPUT].setVoltage(gateVolts, i + p * maxChannelsPerPlayhead);
					}
				}
				outputs[MERGED_GATE_OUTPUT].setChannels(maxChannelsPerPlayhead * 4);
				outputs[MERGED_VOCT_OUTPUT].setChannels(maxChannelsPerPlayhead * 4);
				outputs[POLY_GATE_OUTPUT + p].setChannels(channels);
				outputs[POLY_VOCT_OUTPUT + p].setChannels(channels);
				outputs[EOC_OUTPUT + p].setVoltage((pulse && playHeads[p].eocOn) ? 10.0 : 0.0);
			} else {
				for(int i=0;i<channels;i++){
					outputs[POLY_VOCT_OUTPUT + p].setVoltage(0, i);
					outputs[POLY_GATE_OUTPUT + p].setVoltage(0, i);
				}
				outputs[POLY_GATE_OUTPUT + p].setChannels(channels);
				outputs[POLY_VOCT_OUTPUT + p].setChannels(channels);
				outputs[EOC_OUTPUT + p].setVoltage(0.0);
			}
		}
	}

	int * getYValsFromBottomAtSeqPos(bool includeInactive, int seqPos){
		int finalHigh = getFinalHighestNote1to32();
		int finalLow = getFinalLowestNote1to32();
		ColNotes *cache = includeInactive ? colNotesCache : colNotesCache2;
		if(cache[seqPos].valid && cache[seqPos].finalHigh == finalHigh && cache[seqPos].finalLow == finalLow){
			return cache[seqPos].vals;
		}
		
		cache[seqPos].valid = true;
		cache[seqPos].finalHigh = finalHigh;
		cache[seqPos].finalLow = finalLow;
		cache[seqPos].includeInactive = includeInactive;
		for(int i=0;i<COLS;i++){ cache[seqPos].vals[i] = -1; }

		int valIdx = 0;
		for(int i=CELLS-1;i>=0;i--){
			int x = xFromI(i);
			if(x == seqPos && (cells[i] || includeInactive)){
				int y = yFromI(i);
				int yFromBottom = ROWS - 1 - y;
				if(yFromBottom <= finalHigh-1 && yFromBottom >= finalLow-1){
					cache[seqPos].vals[valIdx++] = yFromBottom;
				}
			}
		}
		return cache[seqPos].vals;
	}

	int findYValIdx(int arr[], int valToFind){
		for(int i=0; i < ROWS; i++){
			if(arr[i] == valToFind){
				return i;
			}
		}		
		return -1;
	}

	void gridChanged(){
		for(int x=0; x < COLS; x++){
			colNotesCache[x].valid = false;
			colNotesCache2[x].valid = false;
		}
	}

	void swapCells() {
		std::swap(cells, newCells);
		gridChanged();

		for(int i=0;i<CELLS;i++){
			newCells[i] = false;
		}
	}

	int getFinalHighestNote1to32(){
		int inputOffset = inputs[HIGHEST_NOTE_INPUT].isConnected() ? int(rescalefjw(inputs[HIGHEST_NOTE_INPUT].getVoltage(), -5, 5, -17, 17)) : 0;
		return clampijw(params[HIGHEST_NOTE_PARAM].getValue() + inputOffset, 1, 32);
	}

	int getFinalLowestNote1to32(){
		int inputOffset = inputs[LOWEST_NOTE_INPUT].isConnected() ? int(rescalefjw(inputs[LOWEST_NOTE_INPUT].getVoltage(), -5, 5, -17, 17)) : 0;
		return clampijw(params[LOWEST_NOTE_PARAM].getValue() + inputOffset, 1, 32);
	}

	float closestVoltageForRow(int cellYFromBottom, int playHeadIdx){
		int octave = clampijw(params[OCTAVE_KNOB_PARAM + playHeadIdx].getValue(), -5.0, 7.0);
		int semi = clampijw(params[SEMI_KNOB_PARAM + playHeadIdx].getValue(), -11.0, 11.0);

		int rootInputOffset = inputs[ROOT_INPUT].isConnected() ? rescalefjw(inputs[ROOT_INPUT].getVoltage(), 0, 10, 0, QuantizeUtils::NUM_NOTES-1) : 0;
		int rootNote = clampijw(params[NOTE_KNOB_PARAM].getValue() + rootInputOffset, 0, QuantizeUtils::NUM_NOTES-1);

		int scaleInputOffset = inputs[SCALE_INPUT].isConnected() ? rescalefjw(inputs[SCALE_INPUT].getVoltage(), 0, 10, 0, QuantizeUtils::NUM_SCALES-1) : 0;
		int scale = clampijw(params[SCALE_KNOB_PARAM].getValue() + scaleInputOffset, 0, QuantizeUtils::NUM_SCALES-1);

		return closestVoltageInScale(octave + (semi * 0.0833) + (cellYFromBottom * 0.0833), rootNote, scale);
	}

	void clockStep(){
		mainClockPulse.trigger(gatePulseLenSec);
		lifeCounter++;
		rndFloat0to1AtClockStep = random::uniform();
		for(int i=0;i<4;i++){
			int seqLen = getSeqLen(i);
			int curPlayMode = getPlayMode(i);
			int seqPos = playHeads[i].seqPos;
			int seqStart = getSeqStart(i);
			int seqEnd = getSeqEnd(i);
			bool goingForward = playHeads[i].goingForward;
			bool eocOn = false;

			playHeads[i].ticksSinceDivision++;
			if(playHeads[i].ticksSinceDivision % int(params[DIVISION_KNOB_PARAM + i].getValue()) == 0){
				playHeads[i].gatePulse.trigger(gatePulseLenSec);

				if(curPlayMode == PM_FWD_LOOP){
					seqPos++;
					if(seqPos > seqEnd){ 
						seqPos = seqStart; 
						if(hitEnd[i]){eocOn = true;}
						hitEnd[i] = true;
					}
					goingForward = true;
				} else if(curPlayMode == PM_BWD_LOOP){
					seqPos = seqPos > seqStart ? seqPos - 1 : seqEnd;
					goingForward = false;
					if(seqPos == seqEnd){
						if(hitEnd[i]){eocOn = true;}
						hitEnd[i] = true;
					}
				} else if(curPlayMode == PM_FWD_BWD_LOOP || curPlayMode == PM_BWD_FWD_LOOP){
					if(goingForward){
						if(seqPos < seqEnd){
							seqPos++;
						} else {
							seqPos--;
							goingForward = false;
							if(hitEnd[i]){eocOn = true;}
							hitEnd[i] = true;
						}
					} else {
						if(seqPos > seqStart){
							seqPos--;
						} else {
							seqPos++;
							goingForward = true;
							if(hitEnd[i]){eocOn = true;}
							hitEnd[i] = true;
						}
					}
				} else if(curPlayMode == PM_RANDOM_POS){
					seqPos = seqStart + int(random::uniform() * seqLen);
				}
				playHeads[i].seqPos = clampijw(seqPos, seqStart, seqEnd);
				playHeads[i].goingForward = goingForward;
				playHeads[i].eocOn = eocOn;
			}
		}
	}

	void resetSeq(){
		for(int i=0;i<4;i++){
			int curPlayMode = getPlayMode(i);
			int seqLen = getSeqLen(i);
			int startOffset = int(params[START_KNOB_PARAM + i].getValue());
			if(curPlayMode == PM_FWD_LOOP || curPlayMode == PM_FWD_BWD_LOOP || curPlayMode == PM_RANDOM_POS){
				playHeads[i].seqPos = startOffset;
			} else if(curPlayMode == PM_BWD_LOOP || curPlayMode == PM_BWD_FWD_LOOP){
				playHeads[i].seqPos = (seqLen - 1 + startOffset) % seqLen;
			}
		}
	}

	void resetSeqToEnd(){
		for(int i=0;i<4;i++){
			int curPlayMode = getPlayMode(i);
			int seqLen = getSeqLen(i);
			int startOffset = int(params[START_KNOB_PARAM + i].getValue());
			if(curPlayMode == PM_FWD_LOOP || curPlayMode == PM_FWD_BWD_LOOP || curPlayMode == PM_RANDOM_POS){
				playHeads[i].seqPos = (seqLen - 1 + startOffset) % seqLen;
			} else if(curPlayMode == PM_BWD_LOOP || curPlayMode == PM_BWD_FWD_LOOP){
				playHeads[i].seqPos = startOffset;
			}
		}
	}

	int getSeqStart(int playHeadIdx){
		int start = clampijw(params[START_KNOB_PARAM + playHeadIdx].getValue(), 0.0, 31.0);
		return start;
	}

	int getSeqEnd(int playHeadIdx){
		int seqEnd = clampijw(getSeqStart(playHeadIdx) + getSeqLen(playHeadIdx) - 1, 0, 31);
		return seqEnd;
	}

	int getSeqLen(int playHeadIdx){
		int len = clampijw(params[LENGTH_KNOB_PARAM + playHeadIdx].getValue(), 1.0, 32.0);
		return len;
	}

	int getPlayMode(int playHeadIdx){
		int mode = clampijw(params[PLAY_MODE_KNOB_PARAM+playHeadIdx].getValue(), 0.0, NUM_PLAY_MODES - 1);
		return mode;
	}

	void rotateCells(RotateDirection dir){
		for(int x=0; x < COLS; x++){
			for(int y=0; y < ROWS; y++){
				switch(dir){
					case DIR_RIGHT:
						newCells[iFromXY(x, y)] = cells[iFromXY(y, COLS - x - 1)];
						break;
					case DIR_LEFT:
						newCells[iFromXY(x, y)] = cells[iFromXY(COLS - y - 1, x)];
						break;
				}

			}
		}
		swapCells();
	}

	void flipCells(FlipDirection dir){
		for(int x=0; x < COLS; x++){
			for(int y=0; y < ROWS; y++){
				switch(dir){
					case DIR_HORIZ:
						newCells[iFromXY(x, y)] = cells[iFromXY(COLS - 1 - x, y)];
						break;
					case DIR_VERT:
						newCells[iFromXY(x, y)] = cells[iFromXY(x, ROWS - 1 - y)];
						break;
				}

			}
		}
		swapCells();
	}

	void shiftCells(ShiftDirection dir){
		int inputOffset = inputs[SHIFT_AMT_INPUT].isConnected() ? rescalefjw(inputs[SHIFT_AMT_INPUT].getVoltage(), -5, 5, -32, 32) : 0;
		int amount = clampijw(params[SHIFT_AMT_KNOB_PARAM].getValue() + inputOffset, -32, 32);
		for(int x=0; x < COLS; x++){
			for(int y=0; y < ROWS; y++){
				int newX = 0, newY = 0;
				switch(dir){
					case DIR_UP:
						//if at top, start from bottom up
						newY = (y + amount) % ROWS;
						if(newY < 0) newY = ROWS + newY;
						newCells[iFromXY(x, y)] = cells[iFromXY(x, newY)];
						break;
					case DIR_DOWN:
						//if at bottom, start from top down
						newY = (y - amount) % ROWS;
						if(newY < 0) newY = ROWS + newY;
						newCells[iFromXY(x, y)] = cells[iFromXY(x, newY)];
						break;
					case DIR_CHAOS:
						bool rndX = random::uniform() < 0.5;
						newX = (x - amount * (rndX?1:-1)) % COLS;
						if(newX < 0) newX = COLS + newX;

						bool rndY = random::uniform() < 0.5;
						newY = (y - amount * (rndY?1:-1)) % ROWS;
						if(newY < 0) newY = ROWS + newY;
						if(cells[iFromXY(x, y)]){
							newCells[iFromXY(newX, newY)] = cells[iFromXY(x, y)];
						}
						break;
				}

			}
		}
		swapCells();
	}

	void clearCells() {
		for(int i=0;i<CELLS;i++){
			cells[i] = false;
			newCells[i] = false;
		}
		gridChanged();
	}

	void randomizeCells() {
		clearCells();
		float rndAmt = params[RND_AMT_KNOB_PARAM].getValue() + inputs[RND_AMT_INPUT].getVoltage()*0.1;
		switch(int(params[RND_MODE_KNOB_PARAM].getValue())){
			case RND_BASIC:{
				for(int i=0;i<CELLS;i++){
					setCellOn(xFromI(i), yFromI(i), random::uniform() < rndAmt);
				}
				break;
			}
			case RND_EUCLID:{
				for(int y=0; y < ROWS; y++){
					if(random::uniform() < rndAmt){
						int div = int(random::uniform() * COLS * 0.5) + 1;
						for(int x=0; x < COLS; x++){
							setCellOn(x, y, x % div == 0);
						}
					}
				}
				break;
			}
			case RND_SIN_WAVE:{
				int sinCount = int(rndAmt * 3) + 1;
				for(int i=0;i<sinCount;i++){
					float angle = 0;
					float angleInc = random::uniform();
					float offset = ROWS * 0.5;
					for(int x=0;x<COLS;x+=1){
						int y = int(offset + (sinf(angle)*(offset)));
						setCellOn(x, y, true);
						angle+=angleInc;
					}
				}
				break;
			}
			case RND_LIFE_GLIDERS:{
				int gliderCount = int(rndAmt * 20);
				int size = 3;
				for(int i=0;i<gliderCount;i++){
					int x = size + int(random::uniform() * (COLS-size*2));
					int y = size + int(random::uniform() * (ROWS-size*2));
					if(random::uniform() < 0.5){
						//down
						if(random::uniform() < 0.5){
							//right
							setCellOn(x, y, true);
							setCellOn(x+1, y+1, true);
							setCellOn(x+1, y+2, true);
							setCellOn(x, y+2, true);
							setCellOn(x-1, y+2, true);
						} else {
							//left
							setCellOn(x, y, true);
							setCellOn(x-1, y+1, true);
							setCellOn(x+1, y+2, true);
							setCellOn(x, y+2, true);
							setCellOn(x-1, y+2, true);
						}
					} else {
						//up
						if(random::uniform() < 0.5){
							//right
							setCellOn(x, y, true);
							setCellOn(x+1, y-1, true);
							setCellOn(x+1, y-2, true);
							setCellOn(x, y-2, true);
							setCellOn(x-1, y-2, true);
						} else {
							//left
							setCellOn(x, y, true);
							setCellOn(x-1, y-1, true);
							setCellOn(x+1, y-2, true);
							setCellOn(x, y-2, true);
							setCellOn(x-1, y-2, true);
						}
					}
				}
				break;
			}
			case RND_CHORDS:{
				for(int x=0;x<COLS;x++){
					if(random::uniform() < rndAmt){
						int y = 20 - int(random::uniform() * 8);
						int dist1 = 2 + int(random::uniform() * 8);
						int dist2 = 2 + int(random::uniform() * 8);
						int dist3 = 2 + int(random::uniform() * 8);
						//default to 3 notes
						setCellOn(x, y, true);
						setCellOn(x, y-dist1, true);
						setCellOn(x, y+dist2, true);
						//sometimes 4th note
						if(random::uniform() < 0.5){
							setCellOn(x, y+dist3, true);
						}
					}
				}
				break;
			}
			case RND_MIRROR_X:{
				for(int y=0; y < ROWS; y++){
					for(int i=0; i < 3; i++){
						if(random::uniform() < rndAmt){
							int xLeft = int(random::uniform() * 16);
							setCellOn(xLeft, y, true);
							setCellOn(COLS-xLeft-1, y, true);
						}
					}
				}
				break;
			}
			case RND_MIRROR_Y:{
				for(int x=0; x < COLS; x++){
					for(int i=0; i < 2; i++){
						if(random::uniform() < rndAmt){
							int yTop = int(random::uniform() * 16);
							setCellOn(x, yTop, true);
							setCellOn(x, ROWS-yTop-1, true);
						}
					}
				}
				break;
			}
		}
	}

	void stepLife(){
		for(int x=0; x < COLS; x++){
			for(int y=0; y < ROWS; y++){
				int cellIdx = iFromXY(x, y);
				int neighbors = getNeighborCount(x,y);
				newCells[cellIdx] = cells[cellIdx];
				if(neighbors<2 || neighbors>3) {
					newCells[cellIdx] = false;
				} else if (neighbors==3 && !cells[cellIdx]) {
					newCells[cellIdx] = true;
				}
			}
		}
		swapCells();
	}
	
	int getNeighborCount(int x, int y){
		int result = 0;
		for(int i=-1; i<=1; i++){
			for(int j=-1; j<=1; j++){
				int newX = x + i;
				int newY = y + j;
				if(newX==x && newY==y)continue;
				if(newX>=0 && newY>=0 && newX<COLS && newY<ROWS && 
					cells[iFromXY(newX, newY)]) result++;
			}
		}
		return result;
	}
  
	void setCellOnByDisplayPos(float displayX, float displayY, bool on){
		setCellOn(int(displayX / HW), int(displayY / HW), on);
	}

	void setCellOn(int cellX, int cellY, bool on){
		if(cellX >= 0 && cellX < COLS && 
		   cellY >=0 && cellY < ROWS){
			cells[iFromXY(cellX, cellY)] = on;
			colNotesCache[cellX].valid = false;
			colNotesCache2[cellX].valid = false;
		}
	}

	bool isCellOnByDisplayPos(float displayX, float displayY){
		return isCellOn(int(displayX / HW), int(displayY / HW));
	}

	bool isCellOn(int cellX, int cellY){
		return cells[iFromXY(cellX, cellY)];
	}

	int iFromXY(int cellX, int cellY){
		return cellX + cellY * ROWS;
	}

	int xFromI(int cellI){
		return cellI % COLS;
	}

	int yFromI(int cellI){
		return cellI / COLS;
	}
};

struct NoteSeqFuDisplay : LightWidget {
	NoteSeqFu *module;
	bool currentlyTurningOn = false;
	Vec dragPos;
	NVGcolor *colors = new NVGcolor[4];
	NoteSeqFuDisplay(){
		colors[0] = nvgRGB(255, 151, 9);//orange
		colors[1] = nvgRGB(255, 243, 9);//yellow
		colors[2] = nvgRGB(144, 26, 252);//purple
		colors[3] = nvgRGB(25, 150, 252);//blue
	}
	~NoteSeqFuDisplay(){
		delete [] colors;
	}

	void onButton(const event::Button &e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			e.consume(this);
			dragPos = e.pos;
			currentlyTurningOn = !module->isCellOnByDisplayPos(e.pos.x, e.pos.y);
			module->setCellOnByDisplayPos(e.pos.x, e.pos.y, currentlyTurningOn);
		}
	}
	
	void onDragStart(const event::DragStart &e) override {
	}

	void onDragMove(const event::DragMove &e) override {
		dragPos = dragPos.plus(e.mouseDelta.div(getAbsoluteZoom()));
		module->setCellOnByDisplayPos(dragPos.x, dragPos.y, currentlyTurningOn);
	}
			//highlight white keys
	void drawLayer(const DrawArgs &args, int layer) override {
		//background
		nvgFillColor(args.vg, nvgRGB(0, 0, 0));
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
		nvgFill(args.vg);

		if(layer == 1){
			//highlight white keys
			int rootNote = module == NULL ? 0 : module->params[NoteSeqFu::NOTE_KNOB_PARAM].getValue();
			for(int i=0;i<ROWS;i++){
				if(!isBlackKey(ROWS-1-i+rootNote)){
					nvgFillColor(args.vg, nvgRGB(40, 40, 40));
					nvgBeginPath(args.vg);
					nvgRect(args.vg, 0, i*HW, box.size.x, HW);
					nvgFill(args.vg);
				}
			}

			//grid
			nvgStrokeColor(args.vg, nvgRGB(60, 70, 73));
			for(int i=1;i<COLS;i++){
				nvgStrokeWidth(args.vg, (i % 4 == 0) ? 2 : 1);
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, i * HW, 0);
				nvgLineTo(args.vg, i * HW, box.size.y);
				nvgStroke(args.vg);
			}
			for(int i=1;i<ROWS;i++){
				// nvgStrokeWidth(args.vg, (i % 4 == 0) ? 2 : 1);
				nvgStrokeWidth(args.vg, 1);
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, 0, i * HW);
				nvgLineTo(args.vg, box.size.x, i * HW);
				nvgStroke(args.vg);
			}

			if(module == NULL) return;

			//highest note line
			float rowHighLimitY = (32-module->getFinalHighestNote1to32()) * HW;
			nvgStrokeColor(args.vg, nvgRGB(120, 120, 120));
			nvgStrokeWidth(args.vg, 2);
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, 0, rowHighLimitY);
			nvgLineTo(args.vg, box.size.x, rowHighLimitY);
			nvgStroke(args.vg);

			//lowest note line
			float rowLowLimitY = (33-module->getFinalLowestNote1to32()) * HW;
			nvgStrokeColor(args.vg, nvgRGB(120, 120, 120));
			nvgStrokeWidth(args.vg, 2);
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, 0, rowLowLimitY);
			nvgLineTo(args.vg, box.size.x, rowLowLimitY);
			nvgStroke(args.vg);

			//cells
			nvgFillColor(args.vg, nvgRGB(25, 150, 252)); //blue
			for(int i=0;i<CELLS;i++){
				if(module->cells[i]){
					int x = module->xFromI(i);
					int y = module->yFromI(i);
					nvgBeginPath(args.vg);
					nvgRect(args.vg, x * HW, y * HW, HW, HW);
					nvgFill(args.vg);
				}
			}

			for(int i=0;i<4;i++){
				if(module->params[NoteSeqFu::PLAYHEAD_ON_PARAM + i].getValue()){
					float startX = module->getSeqStart(i) * HW;
					float endX = (module->getSeqEnd(i) + 1) * HW;
					float endLines[2] = {startX, endX};
					nvgStrokeWidth(args.vg, 2);
					for(int j=0;j<2;j++){
						float endLineX = endLines[j];
						
						//seq length line TOP COLOR
						nvgStrokeColor(args.vg, colors[i]);
						nvgBeginPath(args.vg);
						nvgMoveTo(args.vg, endLineX, 0);
						nvgLineTo(args.vg, endLineX, HW);
						nvgStroke(args.vg);

						//seq length line BOTTOM COLOR
						nvgStrokeColor(args.vg, colors[i]);
						nvgBeginPath(args.vg);
						nvgMoveTo(args.vg, endLineX, box.size.y - HW);
						nvgLineTo(args.vg, endLineX, box.size.y);
						nvgStroke(args.vg);
					}
					//seq pos
					int pos = module->resetMode ? module->getSeqStart(i) : module->playHeads[i].seqPos;
					nvgStrokeWidth(args.vg, 2);
					nvgStrokeColor(args.vg, colors[i]);
					nvgBeginPath(args.vg);
					nvgMoveTo(args.vg, pos * HW, 0);
					nvgLineTo(args.vg, pos * HW, box.size.y);
					nvgStroke(args.vg);
				}
			}
		}
		Widget::drawLayer(args, layer);
	}
};

struct PlayModeKnob : JwSmallSnapKnob {
	PlayModeKnob(){}
	std::string formatCurrentValue() override {
		if(getParamQuantity() != NULL){
			switch(int(getParamQuantity()->getDisplayValue())){
				case NoteSeqFu::PM_FWD_LOOP:return "→";
				case NoteSeqFu::PM_BWD_LOOP:return "←";
				case NoteSeqFu::PM_FWD_BWD_LOOP:return "→←";
				case NoteSeqFu::PM_BWD_FWD_LOOP:return "←→";
				case NoteSeqFu::PM_RANDOM_POS:return "*";
			}
		}
		return "";
	}
};

struct RndModeKnob : JwSmallSnapKnob {
	RndModeKnob(){}
	std::string formatCurrentValue() override {
		if(getParamQuantity() != NULL){
			switch(int(getParamQuantity()->getDisplayValue())){
				case NoteSeqFu::RND_BASIC:return "Basic";
				case NoteSeqFu::RND_EUCLID:return "Euclid";
				case NoteSeqFu::RND_SIN_WAVE:return "Sine";
				case NoteSeqFu::RND_LIFE_GLIDERS:return "Gliders";
				case NoteSeqFu::RND_CHORDS:return "Chords";
				case NoteSeqFu::RND_MIRROR_X:return "Mirror X";
				case NoteSeqFu::RND_MIRROR_Y:return "Mirror Y";
			}
		}
		return "";
	}
};

struct NoteSeqFuWidget : ModuleWidget { 
	NoteSeqFuWidget(NoteSeqFu *module); 
	void appendContextMenu(Menu *menu) override;
};

struct NSFChannelValueItem : MenuItem {
	NoteSeqFu *module;
	int channels;
	void onAction(const event::Action &e) override {
		module->channels = channels;
	}
};

struct NSFChannelItem : MenuItem {
	NoteSeqFu *module;
	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		for (int channels = 1; channels <= 16; channels++) {
			NSFChannelValueItem *item = new NSFChannelValueItem;
			if (channels == 1)
				item->text = "Monophonic";
			else
				item->text = string::f("%d", channels);
			item->rightText = CHECKMARK(module->channels == channels);
			item->module = module;
			item->channels = channels;
			menu->addChild(item);
		}
		return menu;
	}
};

NoteSeqFuWidget::NoteSeqFuWidget(NoteSeqFu *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*48, RACK_GRID_HEIGHT);
	
	setPanel(createPanel(
		asset::plugin(pluginInstance, "res/NoteSeqFu.svg"), 
		asset::plugin(pluginInstance, "res/NoteSeqFu.svg")
	));

	NoteSeqFuDisplay *display = new NoteSeqFuDisplay();
	display->module = module;
	display->box.pos = Vec(172, 2);
	display->box.size = Vec(376, RACK_GRID_HEIGHT - 4);
	addChild(display);
	if(module != NULL){
		module->displayWidth = display->box.size.x;
		module->displayHeight = display->box.size.y;
	}

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	///////////////////////////////////////////////////// LEFT SIDE /////////////////////////////////////////////////////

	//row 1
	addInput(createInput<TinyPJ301MPort>(Vec(11, 40), module, NoteSeqFu::CLOCK_INPUT));

	///// NOTE AND SCALE CONTROLS /////
	float pitchParamYVal = 35;
	float labelY = 36;
	NoteKnob *noteKnob = dynamic_cast<NoteKnob*>(createParam<NoteKnob>(Vec(43, pitchParamYVal), module, NoteSeqFu::NOTE_KNOB_PARAM));
	CenteredLabel* const noteLabel = new CenteredLabel;
	noteLabel->box.pos = Vec(28, labelY);
	noteLabel->text = "";
	noteKnob->connectLabel(noteLabel, module);
	addChild(noteLabel);
	addParam(noteKnob);

	ScaleKnob *scaleKnob = dynamic_cast<ScaleKnob*>(createParam<ScaleKnob>(Vec(80, pitchParamYVal), module, NoteSeqFu::SCALE_KNOB_PARAM));
	CenteredLabel* const scaleLabel = new CenteredLabel;
	scaleLabel->box.pos = Vec(47, labelY);
	scaleLabel->text = "";
	scaleKnob->connectLabel(scaleLabel, module);
	addChild(scaleLabel);
	addParam(scaleKnob);

	addParam(createParam<JwSmallSnapKnob>(Vec(114, pitchParamYVal), module, NoteSeqFu::LOWEST_NOTE_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(119.5, pitchParamYVal+27), module, NoteSeqFu::LOWEST_NOTE_INPUT));
	addParam(createParam<JwSmallSnapKnob>(Vec(143, pitchParamYVal), module, NoteSeqFu::HIGHEST_NOTE_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(148.5, pitchParamYVal+27), module, NoteSeqFu::HIGHEST_NOTE_INPUT));

	//row 2
	addInput(createInput<TinyPJ301MPort>(Vec(10, 95), module, NoteSeqFu::RESET_INPUT));
	addParam(createParam<SmallButton>(Vec(30, 90), module, NoteSeqFu::RESET_BTN_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(60, 95), module, NoteSeqFu::CLEAR_INPUT));
	addParam(createParam<SmallButton>(Vec(80, 90), module, NoteSeqFu::CLEAR_BTN_PARAM));

	RndModeKnob *rndModeKnob = dynamic_cast<RndModeKnob*>(createParam<RndModeKnob>(Vec(120, 90), module, NoteSeqFu::RND_MODE_KNOB_PARAM));
	CenteredLabel* const rndModeLabel = new CenteredLabel;
	rndModeLabel->box.pos = Vec(67, 63);
	rndModeLabel->text = "";
	rndModeKnob->connectLabel(rndModeLabel, module);
	addChild(rndModeLabel);
	addParam(rndModeKnob);

	//row 3
	addInput(createInput<TinyPJ301MPort>(Vec(60, 150), module, NoteSeqFu::RND_TRIG_INPUT));
	addParam(createParam<SmallButton>(Vec(80, 145), module, NoteSeqFu::RND_TRIG_BTN_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(118, 150), module, NoteSeqFu::RND_AMT_INPUT));
	addParam(createParam<SmallWhiteKnob>(Vec(138, 145), module, NoteSeqFu::RND_AMT_KNOB_PARAM));

	addInput(createInput<TinyPJ301MPort>(Vec(60, 201), module, NoteSeqFu::FLIP_HORIZ_INPUT));
	addParam(createParam<SmallButton>(Vec(80, 196), module, NoteSeqFu::FLIP_HORIZ_BTN_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(118, 201), module, NoteSeqFu::FLIP_VERT_INPUT));
	addParam(createParam<SmallButton>(Vec(138, 196), module, NoteSeqFu::FLIP_VERT_BTN_PARAM));

	addInput(createInput<TinyPJ301MPort>(Vec(60, 252), module, NoteSeqFu::ROT_RIGHT_INPUT));
	addParam(createParam<SmallButton>(Vec(80, 247), module, NoteSeqFu::ROT_RIGHT_BTN_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(118, 252), module, NoteSeqFu::ROT_LEFT_INPUT));
	addParam(createParam<SmallButton>(Vec(138, 247), module, NoteSeqFu::ROT_LEFT_BTN_PARAM));

	addInput(createInput<TinyPJ301MPort>(Vec(43, 302), module, NoteSeqFu::SHIFT_UP_INPUT));
	addParam(createParam<TinyButton>(Vec(43+19, 302), module, NoteSeqFu::SHIFT_UP_BTN_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(88, 302), module, NoteSeqFu::SHIFT_DOWN_INPUT));
	addParam(createParam<TinyButton>(Vec(88+19, 302), module, NoteSeqFu::SHIFT_DOWN_BTN_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(134, 302), module, NoteSeqFu::SHIFT_CHAOS_INPUT));
	addParam(createParam<TinyButton>(Vec(134+19, 302), module, NoteSeqFu::SHIFT_CHAOS_BTN_PARAM));

	addInput(createInput<TinyPJ301MPort>(Vec(30, 350), module, NoteSeqFu::SHIFT_AMT_INPUT));
	addParam(createParam<JwSmallSnapKnob>(Vec(50, 345), module, NoteSeqFu::SHIFT_AMT_KNOB_PARAM));

	addParam(createParam<JwHorizontalSwitch>(Vec(102, 350), module, NoteSeqFu::LIFE_ON_SWITCH_PARAM));
	addParam(createParam<JwSmallSnapKnob>(Vec(138, 345), module, NoteSeqFu::LIFE_SPEED_KNOB_PARAM));

	///////////////////////////////////////////////////// RIGHT SIDE /////////////////////////////////////////////////////

	addOutput(createOutput<Blue_TinyPJ301MPort>(Vec(623, 3), module, NoteSeqFu::MERGED_VOCT_OUTPUT));
	addOutput(createOutput<Blue_TinyPJ301MPort>(Vec(668, 3), module, NoteSeqFu::MERGED_GATE_OUTPUT));

	float yTop = 31;
	for(int i=0;i<4;i++){
		float knobX = 556;
		addParam(createParam<JwSmallSnapKnob>(Vec(knobX, yTop), module, NoteSeqFu::START_KNOB_PARAM + i));
		addParam(createParam<JwSmallSnapKnob>(Vec(knobX+=33, yTop), module, NoteSeqFu::LENGTH_KNOB_PARAM + i));
		addParam(createParam<JwSmallSnapKnob>(Vec(knobX+=33, yTop), module, NoteSeqFu::DIVISION_KNOB_PARAM + i));
		addParam(createParam<JwSmallSnapKnob>(Vec(knobX+=33, yTop), module, NoteSeqFu::OCTAVE_KNOB_PARAM + i));
		addParam(createParam<JwSmallSnapKnob>(Vec(knobX+=33, yTop), module, NoteSeqFu::SEMI_KNOB_PARAM + i));

		addParam(createParam<JwHorizontalSwitch>(Vec(560, yTop+43), module, NoteSeqFu::PLAYHEAD_ON_PARAM + i));

		PlayModeKnob *playModeKnob = dynamic_cast<PlayModeKnob*>(createParam<PlayModeKnob>(Vec(589, yTop+38), module, NoteSeqFu::PLAY_MODE_KNOB_PARAM + i));
		CenteredLabel* const playModeLabel = new CenteredLabel;
		playModeLabel->box.pos = Vec(301.25, 50.5+(i*43)); 
		playModeLabel->text = "";
		playModeKnob->connectLabel(playModeLabel, module);
		addChild(playModeLabel);
		addParam(playModeKnob);

		addOutput(createOutput<TinyPJ301MPort>(Vec(630, yTop+43), module, NoteSeqFu::EOC_OUTPUT + i));
		addOutput(createOutput<Blue_TinyPJ301MPort>(Vec(660, yTop+43), module, NoteSeqFu::POLY_VOCT_OUTPUT + i));
		addOutput(createOutput<Blue_TinyPJ301MPort>(Vec(690, yTop+43), module, NoteSeqFu::POLY_GATE_OUTPUT + i));

		yTop+=85.5;
	}

	addParam(createParam<JwHorizontalSwitch>(Vec(591, 362), module, NoteSeqFu::REPEATS_PARAM));
	addParam(createParam<JwHorizontalSwitch>(Vec(668, 362), module, NoteSeqFu::INCLUDE_INACTIVE_PARAM));
}

struct NoteSeqFuGateModeItem : MenuItem {
	NoteSeqFu *noteSeqFu;
	NoteSeqFu::GateMode gateMode;
	void onAction(const event::Action &e) override {
		noteSeqFu->gateMode = gateMode;
	}
	void step() override {
		rightText = (noteSeqFu->gateMode == gateMode) ? "✔" : "";
		MenuItem::step();
	}
};

// Local quantity/slider removed; using shared GatePulseMsQuantity/GatePulseMsSlider from JWModules.hpp

void NoteSeqFuWidget::appendContextMenu(Menu *menu) {
	NoteSeqFu *noteSeqFu = dynamic_cast<NoteSeqFu*>(module);
	MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);

	NSFChannelItem *channelItem = new NSFChannelItem;
	channelItem->text = "Polyphony channels";
	channelItem->rightText = string::f("%d", noteSeqFu->channels) + " " +RIGHT_ARROW;
	channelItem->module = noteSeqFu;
	menu->addChild(channelItem);

	MenuLabel *spacerLabel2 = new MenuLabel();
	menu->addChild(spacerLabel2);

	MenuLabel *modeLabel = new MenuLabel();
	modeLabel->text = "Gate Mode";
	menu->addChild(modeLabel);

	NoteSeqFuGateModeItem *triggerItem = new NoteSeqFuGateModeItem();
	triggerItem->text = "Trigger";
	triggerItem->noteSeqFu = noteSeqFu;
	triggerItem->gateMode = NoteSeqFu::TRIGGER;
	menu->addChild(triggerItem);

	NoteSeqFuGateModeItem *retriggerItem = new NoteSeqFuGateModeItem();
	retriggerItem->text = "Retrigger";
	retriggerItem->noteSeqFu = noteSeqFu;
	retriggerItem->gateMode = NoteSeqFu::RETRIGGER;
	menu->addChild(retriggerItem);

	NoteSeqFuGateModeItem *continuousItem = new NoteSeqFuGateModeItem();
	continuousItem->text = "Continuous";
	continuousItem->noteSeqFu = noteSeqFu;
	continuousItem->gateMode = NoteSeqFu::CONTINUOUS;
	menu->addChild(continuousItem);

	// Gate pulse length slider
	MenuLabel *spacerLabelGate = new MenuLabel();
	menu->addChild(spacerLabelGate);
	MenuLabel *gatePulseLabel = new MenuLabel();
	gatePulseLabel->text = "Gate Length";
	menu->addChild(gatePulseLabel);

	GatePulseMsSlider* gateSlider = new GatePulseMsSlider();
	{
		auto qp = static_cast<GatePulseMsQuantity*>(gateSlider->quantity);
		qp->getSeconds = [noteSeqFu](){ return noteSeqFu->gatePulseLenSec; };
		qp->setSeconds = [noteSeqFu](float v){ noteSeqFu->gatePulseLenSec = v; };
		qp->defaultSeconds = 0.1f;
		qp->label = "Gate Length";
	}
	gateSlider->box.size.x = 220.0f;
	menu->addChild(gateSlider);
}

Model *modelNoteSeqFu = createModel<NoteSeqFu, NoteSeqFuWidget>("NoteSeqFu");
