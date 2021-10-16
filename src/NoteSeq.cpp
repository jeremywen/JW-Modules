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

struct NoteSeq : Module,QuantizeUtils {
	enum ParamIds {
		STEP_BTN_PARAM,
		LENGTH_KNOB_PARAM,
		PLAY_MODE_KNOB_PARAM,
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
		OCTAVE_KNOB_PARAM,
		LOW_HIGH_SWITCH_PARAM,
		HIGHEST_NOTE_PARAM,
		LOWEST_NOTE_PARAM,
		INCLUDE_INACTIVE_PARAM,
		SHIFT_AMT_KNOB_PARAM,
		START_PARAM,
		SHIFT_CHAOS_BTN_PARAM,
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
		HIGHEST_NOTE_INPUT,
		LOWEST_NOTE_INPUT,
		ROOT_INPUT,
		OCTAVE_INPUT,
		SCALE_INPUT,
		LENGTH_INPUT,
		MODE_INPUT,
		SHIFT_AMT_INPUT,
		START_INPUT,
		SHIFT_CHAOS_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		VOCT_MAIN_OUTPUT,
		GATE_MAIN_OUTPUT = VOCT_MAIN_OUTPUT + POLY,
		MIN_VOCT_OUTPUT = GATE_MAIN_OUTPUT + POLY,
		MIN_GATE_OUTPUT,
		MID_VOCT_OUTPUT,
		MID_GATE_OUTPUT,
		MAX_VOCT_OUTPUT,
		MAX_GATE_OUTPUT,
		RND_VOCT_OUTPUT,
		RND_GATE_OUTPUT,
		POLY_VOCT_OUTPUT,
		POLY_GATE_OUTPUT,
		EOC_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		GATES_LIGHT,
		NUM_LIGHTS = GATES_LIGHT + POLY,
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
		RND_CHORD_LIKE,
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
	float lifeRate = 0.5 * APP->engine->getSampleRate();
	long lifeCounter = 0;
	int seqPos = 0;
	int channels = 1;
	float rndFloat0to1AtClockStep = random::uniform();
	bool goingForward = true;
	bool eocOn = false; 
	bool hitEnd = false;
	bool resetMode = false;
	bool *cells = new bool[CELLS];
	bool *newCells = new bool[CELLS];
	ColNotes *colNotesCache = new ColNotes[COLS];
	ColNotes *colNotesCache2 = new ColNotes[COLS];
	dsp::SchmittTrigger clockTrig, resetTrig, clearTrig;
	dsp::SchmittTrigger rndTrig, shiftUpTrig, shiftDownTrig, shiftChaosTrig;
	dsp::SchmittTrigger rotateRightTrig, rotateLeftTrig, flipHorizTrig, flipVertTrig;
	dsp::PulseGenerator gatePulse, eocPulse;

	enum GateMode { TRIGGER, RETRIGGER, CONTINUOUS };
	GateMode gateMode = TRIGGER;

	NoteSeq() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(START_PARAM, 0.0, 31.0, 0.0, "Start");
		configParam(STEP_BTN_PARAM, 0.0, 1.0, 0.0, "Step");
		configParam(LENGTH_KNOB_PARAM, 1.0, 32.0, 32.0, "Length");
		configParam(PLAY_MODE_KNOB_PARAM, 0.0, NUM_PLAY_MODES - 1, 0.0, "Play Mode");
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
		configParam(HIGHEST_NOTE_PARAM, 1.0, 32.0, 32.0, "Highest Note");
		configParam(LOWEST_NOTE_PARAM, 1.0, 32.0, 1.0, "Lowest Note");
		configParam(INCLUDE_INACTIVE_PARAM, 0.0, 1.0, 0.0, "Drum Mode");
		configParam(OCTAVE_KNOB_PARAM, -5.0, 7.0, 0.0, "Octave");
		configParam(NOTE_KNOB_PARAM, 0.0, QuantizeUtils::NUM_NOTES-1, QuantizeUtils::NOTE_C, "Root Note");
		configParam(SCALE_KNOB_PARAM, 0.0, QuantizeUtils::NUM_SCALES-1, QuantizeUtils::MINOR, "Scale");

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
		configInput(OCTAVE_INPUT, "Octave");
		configInput(SCALE_INPUT, "Scale");
		configInput(LENGTH_INPUT, "Length");
		configInput(MODE_INPUT, "Mode");
		configInput(SHIFT_AMT_INPUT, "Shift Amount");
		configInput(START_INPUT, "Start");
		configInput(SHIFT_CHAOS_INPUT, "Shift Chaos");


		for (int i = 0; i < POLY; i++) {
			configOutput(VOCT_MAIN_OUTPUT + i, "V/Oct " + std::to_string(i+1));
			configOutput(GATE_MAIN_OUTPUT + i, "Gate " + std::to_string(i+1));
		}
		configOutput(MIN_VOCT_OUTPUT, "Minimum V/Oct");
		configOutput(MIN_GATE_OUTPUT, "Minimum Gate");
		configOutput(MID_VOCT_OUTPUT, "Middle V/Oct");
		configOutput(MID_GATE_OUTPUT, "Middle Gate");
		configOutput(MAX_VOCT_OUTPUT, "Maximum V/Oct");
		configOutput(MAX_GATE_OUTPUT, "Maximum Gate");
		configOutput(RND_VOCT_OUTPUT, "Random V/Oct");
		configOutput(RND_GATE_OUTPUT, "Random Gate");
		configOutput(POLY_VOCT_OUTPUT, "Poly V/Oct");
		configOutput(POLY_GATE_OUTPUT, "Poly Gate");
		configOutput(EOC_OUTPUT, "End of Cycle");

		resetSeq();
		resetMode = true;
		clearCells();
	}

	~NoteSeq() {
		delete [] cells;
		delete [] newCells;
		delete [] colNotesCache;
		delete [] colNotesCache2;
	}

	void onRandomize() override {
		randomizeCells();
	}

	void onReset() override {
		resetSeq();
		resetMode = true;
		clearCells();
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

		gridChanged();
	}

	void process(const ProcessArgs &args) override {
		if(params[LIFE_ON_SWITCH_PARAM].getValue()){
			if(lifeCounter % int(17.0 - params[LIFE_SPEED_KNOB_PARAM].getValue()) == 0){ 
				stepLife();
				lifeCounter = 1;
			}
		}

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
		}

		if (clockTrig.process(inputs[CLOCK_INPUT].getVoltage() + params[STEP_BTN_PARAM].getValue())) {
			if(resetMode){
				resetMode = false;
				hitEnd = false;
				resetSeqToEnd();
			}
			clockStep();
		}

		bool pulse = gatePulse.process(1.0 / args.sampleRate);
		// ////////////////////////////////////////////// POLY OUTPUTS //////////////////////////////////////////////
		
		int *polyYVals = getYValsFromBottomAtSeqPos(params[INCLUDE_INACTIVE_PARAM].getValue());
		//OLD POLY OUTPUTS
		for(int i=0;i<POLY;i++){ //param # starts from bottom
			bool hasVal = polyYVals[i] > -1;
			bool cellActive = hasVal && cells[iFromXY(seqPos, ROWS - polyYVals[i] - 1)];
			if(cellActive){ 
				float volts = closestVoltageForRow(polyYVals[i]);
				outputs[VOCT_MAIN_OUTPUT + i].setVoltage(volts);
			}
			bool gateOn = cellActive;
			if (gateMode == TRIGGER) gateOn = gateOn && pulse;
			else if (gateMode == RETRIGGER) gateOn = gateOn && !pulse;
			outputs[GATE_MAIN_OUTPUT + i].setVoltage(gateOn ? 10.0 : 0.0);
			lights[GATES_LIGHT + i].value = gateOn ? 1.0 : 0.0;			
		}
		//ONLY NEW MAIN POLY OUTPUT
		for(int i=0;i<channels;i++){
			bool hasVal = polyYVals[i] > -1;
			bool cellActive = hasVal && cells[iFromXY(seqPos, ROWS - polyYVals[i] - 1)];
			if(cellActive){ 
				float volts = closestVoltageForRow(polyYVals[i]);
				outputs[POLY_VOCT_OUTPUT].setVoltage(volts, i);
			}
			bool gateOn = cellActive;
			if (gateMode == TRIGGER) gateOn = gateOn && pulse;
			else if (gateMode == RETRIGGER) gateOn = gateOn && !pulse;
			outputs[POLY_GATE_OUTPUT].setVoltage(gateOn ? 10.0 : 0.0, i);
		}
		outputs[POLY_GATE_OUTPUT].setChannels(channels);
		outputs[POLY_VOCT_OUTPUT].setChannels(channels);

		////////////////////////////////////////////// MONO OUTPUTS //////////////////////////////////////////////
		if(outputs[MIN_VOCT_OUTPUT].isConnected() || outputs[MIN_GATE_OUTPUT].isConnected() || 
		   outputs[MID_VOCT_OUTPUT].isConnected() || outputs[MID_GATE_OUTPUT].isConnected() ||
		   outputs[MAX_VOCT_OUTPUT].isConnected() || outputs[MAX_GATE_OUTPUT].isConnected() ||
		   outputs[RND_VOCT_OUTPUT].isConnected() || outputs[RND_GATE_OUTPUT].isConnected()){
			int *monoYVals = getYValsFromBottomAtSeqPos(false);
			bool atLeastOne = monoYVals[0] > -1;
			if(outputs[MIN_VOCT_OUTPUT].isConnected() && atLeastOne){
				outputs[MIN_VOCT_OUTPUT].setVoltage(closestVoltageForRow(monoYVals[0]));
			}
			if(outputs[MIN_GATE_OUTPUT].isConnected()){
				bool gateOn = atLeastOne;
				if (gateMode == TRIGGER) gateOn = gateOn && pulse;
				else if (gateMode == RETRIGGER) gateOn = gateOn && !pulse;
				outputs[MIN_GATE_OUTPUT].setVoltage(gateOn ? 10.0 : 0.0);
			}

			if(outputs[MID_VOCT_OUTPUT].isConnected() && atLeastOne){
				int minPos = 0;
				int maxPos = findYValIdx(monoYVals, -1) - 1;
				outputs[MID_VOCT_OUTPUT].setVoltage(closestVoltageForRow((monoYVals[minPos] + monoYVals[maxPos]) * 0.5));
			}
			if(outputs[MID_GATE_OUTPUT].isConnected()){
				bool gateOn = atLeastOne;
				if (gateMode == TRIGGER) gateOn = gateOn && pulse;
				else if (gateMode == RETRIGGER) gateOn = gateOn && !pulse;
				outputs[MID_GATE_OUTPUT].setVoltage(gateOn ? 10.0 : 0.0);
			}

			if(outputs[MAX_VOCT_OUTPUT].isConnected() && atLeastOne){
				int maxPos = findYValIdx(monoYVals, -1) - 1;
				outputs[MAX_VOCT_OUTPUT].setVoltage(closestVoltageForRow(monoYVals[maxPos]));
			}
			if(outputs[MAX_GATE_OUTPUT].isConnected()){
				bool gateOn = atLeastOne;
				if (gateMode == TRIGGER) gateOn = gateOn && pulse;
				else if (gateMode == RETRIGGER) gateOn = gateOn && !pulse;
				outputs[MAX_GATE_OUTPUT].setVoltage(gateOn ? 10.0 : 0.0);
			}

			if(outputs[RND_VOCT_OUTPUT].isConnected() && atLeastOne){
				int maxPos = findYValIdx(monoYVals, -1);// - 1;
				outputs[RND_VOCT_OUTPUT].setVoltage(closestVoltageForRow(monoYVals[int(rndFloat0to1AtClockStep * maxPos)]));
			}
			if(outputs[RND_GATE_OUTPUT].isConnected()){
				bool gateOn = atLeastOne;
				if (gateMode == TRIGGER) gateOn = gateOn && pulse;
				else if (gateMode == RETRIGGER) gateOn = gateOn && !pulse;
				outputs[RND_GATE_OUTPUT].setVoltage(gateOn ? 10.0 : 0.0);
			}
		}
		outputs[EOC_OUTPUT].setVoltage((pulse && eocOn) ? 10.0 : 0.0);
	}

	int * getYValsFromBottomAtSeqPos(bool includeInactive){
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

	float closestVoltageForRow(int cellYFromBottom){
		int octaveInputOffset = inputs[OCTAVE_INPUT].isConnected() ? int(inputs[OCTAVE_INPUT].getVoltage()) : 0;
		int octave = clampijw(params[OCTAVE_KNOB_PARAM].getValue() + octaveInputOffset, -5.0, 7.0);

		int rootInputOffset = inputs[ROOT_INPUT].isConnected() ? rescalefjw(inputs[ROOT_INPUT].getVoltage(), 0, 10, 0, QuantizeUtils::NUM_NOTES-1) : 0;
		int rootNote = clampijw(params[NOTE_KNOB_PARAM].getValue() + rootInputOffset, 0, QuantizeUtils::NUM_NOTES-1);

		int scaleInputOffset = inputs[SCALE_INPUT].isConnected() ? rescalefjw(inputs[SCALE_INPUT].getVoltage(), 0, 10, 0, QuantizeUtils::NUM_SCALES-1) : 0;
		int scale = clampijw(params[SCALE_KNOB_PARAM].getValue() + scaleInputOffset, 0, QuantizeUtils::NUM_SCALES-1);

		return closestVoltageInScale(octave + (cellYFromBottom * 0.0833), rootNote, scale);
	}

	void clockStep(){
		gatePulse.trigger(1e-1);
		lifeCounter++;
		rndFloat0to1AtClockStep = random::uniform();

		int curPlayMode = getPlayMode();
		int seqLen = getSeqLen();
		int seqStart = getSeqStart();
		int seqEnd = getSeqEnd();
		eocOn = false;

		if(curPlayMode == PM_FWD_LOOP){
			seqPos++;
			if(seqPos > seqEnd){ 
				seqPos = seqStart; 
				if(hitEnd){eocOn = true;}
				hitEnd = true;
			}
			goingForward = true;
		} else if(curPlayMode == PM_BWD_LOOP){
			seqPos = seqPos > seqStart ? seqPos - 1 : seqEnd;
			goingForward = false;
			if(seqPos == seqEnd){
				if(hitEnd){eocOn = true;}
				hitEnd = true;
			}
		} else if(curPlayMode == PM_FWD_BWD_LOOP || curPlayMode == PM_BWD_FWD_LOOP){
			if(goingForward){
				if(seqPos < seqEnd){
					seqPos++;
				} else {
					seqPos--;
					goingForward = false;
					if(hitEnd){eocOn = true;}
					hitEnd = true;
				}
			} else {
				if(seqPos > seqStart){
					seqPos--;
				} else {
					seqPos++;
					goingForward = true;
					if(hitEnd){eocOn = true;}
					hitEnd = true;
				}
			}
		} else if(curPlayMode == PM_RANDOM_POS){
			seqPos = seqStart + int(random::uniform() * seqLen);
		}
		seqPos = clampijw(seqPos, seqStart, seqEnd);
	}

	void resetSeq(){
		int curPlayMode = getPlayMode();
		if(curPlayMode == PM_FWD_LOOP || curPlayMode == PM_FWD_BWD_LOOP || curPlayMode == PM_RANDOM_POS){
			seqPos = getSeqStart();
		} else if(curPlayMode == PM_BWD_LOOP || curPlayMode == PM_BWD_FWD_LOOP){
			seqPos = clampijw(getSeqStart() + getSeqLen(), 0, 31);
		}
	}

	void resetSeqToEnd(){
		int curPlayMode = getPlayMode();
		if(curPlayMode == PM_FWD_LOOP || curPlayMode == PM_FWD_BWD_LOOP || curPlayMode == PM_RANDOM_POS){
			seqPos = clampijw(getSeqStart() + getSeqLen(), 0, 31);
		} else if(curPlayMode == PM_BWD_LOOP || curPlayMode == PM_BWD_FWD_LOOP){
			seqPos = getSeqStart();
		}
	}

	int getSeqStart(){
		int inputOffset = int(rescalefjw(inputs[START_INPUT].getVoltage(), 0, 10.0, 0.0, 31.0));
		int start = clampijw(params[START_PARAM].getValue() + inputOffset, 0.0, 31.0);
		return start;
	}

	int getSeqLen(){
		int inputOffset = int(rescalefjw(inputs[LENGTH_INPUT].getVoltage(), 0, 10.0, 0.0, 31.0));
		int len = clampijw(params[LENGTH_KNOB_PARAM].getValue() + inputOffset, 1.0, 32.0);
		return len;
	}

	int getSeqEnd(){
		int seqEnd = clampijw(getSeqStart() + getSeqLen() - 1, 0, 31);
		return seqEnd;
	}

	int getPlayMode(){
		int inputOffset = int(rescalefjw(inputs[MODE_INPUT].getVoltage(), 0, 10.0, 0.0, NUM_PLAY_MODES - 1));
		int mode = clampijw(params[PLAY_MODE_KNOB_PARAM].getValue() + inputOffset, 0.0, NUM_PLAY_MODES - 1);
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
			case RND_CHORD_LIKE:{
				// int chordCount = int(rndAmt * 20);
				for(int x=0;x<COLS;x++){
					if(random::uniform() < rndAmt){
						int y = 20 - int(random::uniform() * 8);
						int dist1 = 2 + int(random::uniform() * 8);
						int dist2 = 2 + int(random::uniform() * 8);
						setCellOn(x, y, true);
						setCellOn(x, y-dist1, true);
						setCellOn(x, y+dist2, true);
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

struct NoteSeqDisplay : LightWidget {
	NoteSeq *module;
	bool currentlyTurningOn = false;
	float initX = 0;
	float initY = 0;
	float dragX = 0;
	float dragY = 0;
	NoteSeqDisplay(){}

	void onButton(const event::Button &e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			e.consume(this);
			// e.target = this;
			initX = e.pos.x;
			initY = e.pos.y;
			currentlyTurningOn = !module->isCellOnByDisplayPos(e.pos.x, e.pos.y);
			module->setCellOnByDisplayPos(e.pos.x, e.pos.y, currentlyTurningOn);
		}
	}
	
	void onDragStart(const event::DragStart &e) override {
		dragX = APP->scene->mousePos.x;
		dragY = APP->scene->mousePos.y;
	}

	void onDragMove(const event::DragMove &e) override {
		float newDragX = APP->scene->mousePos.x;
		float newDragY = APP->scene->mousePos.y;
		module->setCellOnByDisplayPos(initX+(newDragX-dragX), initY+(newDragY-dragY), currentlyTurningOn);
	}

	void draw(const DrawArgs &args) override {
		nvgGlobalTint(args.vg, color::WHITE);
		//background
		nvgFillColor(args.vg, nvgRGB(0, 0, 0));
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
		nvgFill(args.vg);

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
			nvgStrokeWidth(args.vg, (i % 4 == 0) ? 2 : 1);
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, 0, i * HW);
			nvgLineTo(args.vg, box.size.x, i * HW);
			nvgStroke(args.vg);
		}

		if(module == NULL) return;

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

		nvgStrokeWidth(args.vg, 2);

		//highest note line
		float rowHighLimitY = (32-module->getFinalHighestNote1to32()) * HW;
		nvgStrokeColor(args.vg, nvgRGB(255, 151, 9));//orange
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 0, rowHighLimitY);
		nvgLineTo(args.vg, box.size.x, rowHighLimitY);
		nvgStroke(args.vg);

		//lowest note line
		float rowLowLimitY = (33-module->getFinalLowestNote1to32()) * HW;
		nvgStrokeColor(args.vg, nvgRGB(255, 243, 9));//yellow
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 0, rowLowLimitY);
		nvgLineTo(args.vg, box.size.x, rowLowLimitY);
		nvgStroke(args.vg);

		//seq start line
		float startX = module->getSeqStart() * HW;
		nvgStrokeColor(args.vg, nvgRGB(25, 150, 252));//blue
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, startX, 0);
		nvgLineTo(args.vg, startX, box.size.y);
		nvgStroke(args.vg);

		//seq length line
		float endX = (module->getSeqEnd() + 1) * HW;
		nvgStrokeColor(args.vg, nvgRGB(144, 26, 252));//purple
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, endX, 0);
		nvgLineTo(args.vg, endX, box.size.y);
		nvgStroke(args.vg);

		//seq pos
		int pos = module->resetMode ? module->getSeqStart() : module->seqPos;
		nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
		nvgBeginPath(args.vg);
		nvgRect(args.vg, pos * HW, 0, HW, box.size.y);
		nvgStroke(args.vg);
	}
};

struct PlayModeKnob : JwSmallSnapKnob {
	PlayModeKnob(){}
	std::string formatCurrentValue() override {
		if(getParamQuantity() != NULL){
			switch(int(getParamQuantity()->getDisplayValue())){
				case NoteSeq::PM_FWD_LOOP:return "→";
				case NoteSeq::PM_BWD_LOOP:return "←";
				case NoteSeq::PM_FWD_BWD_LOOP:return "→←";
				case NoteSeq::PM_BWD_FWD_LOOP:return "←→";
				case NoteSeq::PM_RANDOM_POS:return "*";
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
				case NoteSeq::RND_BASIC:return "Basic";
				case NoteSeq::RND_EUCLID:return "Euclid";
				case NoteSeq::RND_SIN_WAVE:return "Sine";
				case NoteSeq::RND_LIFE_GLIDERS:return "Gliders";
				case NoteSeq::RND_CHORD_LIKE:return "Chord-Like";
				case NoteSeq::RND_MIRROR_X:return "Mirror X";
				case NoteSeq::RND_MIRROR_Y:return "Mirror Y";
			}
		}
		return "";
	}
};

struct NoteSeqWidget : ModuleWidget { 
	NoteSeqWidget(NoteSeq *module); 
	void appendContextMenu(Menu *menu) override;
};

struct NSChannelValueItem : MenuItem {
	NoteSeq *module;
	int channels;
	void onAction(const event::Action &e) override {
		module->channels = channels;
	}
};

struct NSChannelItem : MenuItem {
	NoteSeq *module;
	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		for (int channels = 1; channels <= 16; channels++) {
			NSChannelValueItem *item = new NSChannelValueItem;
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

NoteSeqWidget::NoteSeqWidget(NoteSeq *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*48, RACK_GRID_HEIGHT);

	SVGPanel *panel = new SVGPanel();
	panel->box.size = box.size;
	panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/NoteSeq.svg")));
	addChild(panel);

	NoteSeqDisplay *display = new NoteSeqDisplay();
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
	addInput(createInput<TinyPJ301MPort>(Vec(7.5, 40), module, NoteSeq::CLOCK_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(33, 40), module, NoteSeq::START_INPUT));
	addParam(createParam<JwSmallSnapKnob>(Vec(49, 35), module, NoteSeq::START_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(80, 40), module, NoteSeq::LENGTH_INPUT));
	addParam(createParam<JwSmallSnapKnob>(Vec(96, 35), module, NoteSeq::LENGTH_KNOB_PARAM));

	addInput(createInput<TinyPJ301MPort>(Vec(128, 40), module, NoteSeq::MODE_INPUT));
	PlayModeKnob *playModeKnob = dynamic_cast<PlayModeKnob*>(createParam<PlayModeKnob>(Vec(144, 35), module, NoteSeq::PLAY_MODE_KNOB_PARAM));
	CenteredLabel* const playModeLabel = new CenteredLabel;
	playModeLabel->box.pos = Vec(76.5, 35);
	playModeLabel->text = "";
	playModeKnob->connectLabel(playModeLabel, module);
	addChild(playModeLabel);
	addParam(playModeKnob);

	//row 2
	addInput(createInput<TinyPJ301MPort>(Vec(10, 92), module, NoteSeq::RESET_INPUT));
	addParam(createParam<SmallButton>(Vec(30, 87), module, NoteSeq::RESET_BTN_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(60, 92), module, NoteSeq::CLEAR_INPUT));
	addParam(createParam<SmallButton>(Vec(80, 87), module, NoteSeq::CLEAR_BTN_PARAM));

	RndModeKnob *rndModeKnob = dynamic_cast<RndModeKnob*>(createParam<RndModeKnob>(Vec(120, 87), module, NoteSeq::RND_MODE_KNOB_PARAM));
	CenteredLabel* const rndModeLabel = new CenteredLabel;
	rndModeLabel->box.pos = Vec(67, 61);
	rndModeLabel->text = "";
	rndModeKnob->connectLabel(rndModeLabel, module);
	addChild(rndModeLabel);
	addParam(rndModeKnob);

	//row 3
	addInput(createInput<TinyPJ301MPort>(Vec(60, 150), module, NoteSeq::RND_TRIG_INPUT));
	addParam(createParam<SmallButton>(Vec(80, 145), module, NoteSeq::RND_TRIG_BTN_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(118, 150), module, NoteSeq::RND_AMT_INPUT));
	addParam(createParam<SmallWhiteKnob>(Vec(138, 145), module, NoteSeq::RND_AMT_KNOB_PARAM));

	addInput(createInput<TinyPJ301MPort>(Vec(60, 201), module, NoteSeq::FLIP_HORIZ_INPUT));
	addParam(createParam<SmallButton>(Vec(80, 196), module, NoteSeq::FLIP_HORIZ_BTN_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(118, 201), module, NoteSeq::FLIP_VERT_INPUT));
	addParam(createParam<SmallButton>(Vec(138, 196), module, NoteSeq::FLIP_VERT_BTN_PARAM));

	addInput(createInput<TinyPJ301MPort>(Vec(60, 252), module, NoteSeq::ROT_RIGHT_INPUT));
	addParam(createParam<SmallButton>(Vec(80, 247), module, NoteSeq::ROT_RIGHT_BTN_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(118, 252), module, NoteSeq::ROT_LEFT_INPUT));
	addParam(createParam<SmallButton>(Vec(138, 247), module, NoteSeq::ROT_LEFT_BTN_PARAM));

	addInput(createInput<TinyPJ301MPort>(Vec(43, 302), module, NoteSeq::SHIFT_UP_INPUT));
	addParam(createParam<TinyButton>(Vec(43+19, 302), module, NoteSeq::SHIFT_UP_BTN_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(88, 302), module, NoteSeq::SHIFT_DOWN_INPUT));
	addParam(createParam<TinyButton>(Vec(88+19, 302), module, NoteSeq::SHIFT_DOWN_BTN_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(134, 302), module, NoteSeq::SHIFT_CHAOS_INPUT));
	addParam(createParam<TinyButton>(Vec(134+19, 302), module, NoteSeq::SHIFT_CHAOS_BTN_PARAM));

	addInput(createInput<TinyPJ301MPort>(Vec(30, 350), module, NoteSeq::SHIFT_AMT_INPUT));
	addParam(createParam<JwSmallSnapKnob>(Vec(50, 345), module, NoteSeq::SHIFT_AMT_KNOB_PARAM));

	addParam(createParam<JwHorizontalSwitch>(Vec(102, 350), module, NoteSeq::LIFE_ON_SWITCH_PARAM));
	addParam(createParam<JwSmallSnapKnob>(Vec(138, 345), module, NoteSeq::LIFE_SPEED_KNOB_PARAM));

	///////////////////////////////////////////////////// RIGHT SIDE /////////////////////////////////////////////////////

	float outputRowTop = 35.0;
	float outputRowDist = 21.0;
	for(int i=0;i<POLY;i++){
		int paramIdx = POLY - i - 1;
		addOutput(createOutput<TinyPJ301MPort>(Vec(559.081, outputRowTop + i * outputRowDist), module, NoteSeq::VOCT_MAIN_OUTPUT + paramIdx)); //param # from bottom up
		addChild(createLight<SmallLight<MyBlueValueLight>>(Vec(580, (outputRowTop+3) + i * outputRowDist), module, NoteSeq::GATES_LIGHT + paramIdx));
		addOutput(createOutput<TinyPJ301MPort>(Vec(591.858, outputRowTop + i * outputRowDist), module, NoteSeq::GATE_MAIN_OUTPUT + paramIdx)); //param # from bottom up
	}

	addOutput(createOutput<TinyPJ301MPort>(Vec(656.303, outputRowTop), module, NoteSeq::MIN_VOCT_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(689.081, outputRowTop), module, NoteSeq::MIN_GATE_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(656.303, outputRowTop + outputRowDist), module, NoteSeq::MID_VOCT_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(689.081, outputRowTop + outputRowDist), module, NoteSeq::MID_GATE_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(656.303, outputRowTop + 2*outputRowDist), module, NoteSeq::MAX_VOCT_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(689.081, outputRowTop + 2*outputRowDist), module, NoteSeq::MAX_GATE_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(656.303, outputRowTop + 3*outputRowDist), module, NoteSeq::RND_VOCT_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(689.081, outputRowTop + 3*outputRowDist), module, NoteSeq::RND_GATE_OUTPUT));

	addInput(createInput<TinyPJ301MPort>(Vec(643, 152), module, NoteSeq::HIGHEST_NOTE_INPUT));
	addParam(createParam<JwSmallSnapKnob>(Vec(663, 147), module, NoteSeq::HIGHEST_NOTE_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(643, 195), module, NoteSeq::LOWEST_NOTE_INPUT));
	addParam(createParam<JwSmallSnapKnob>(Vec(663, 190), module, NoteSeq::LOWEST_NOTE_PARAM));

	addParam(createParam<JwHorizontalSwitch>(Vec(675, 224), module, NoteSeq::INCLUDE_INACTIVE_PARAM));

	float pitchParamYVal = 259;
	addParam(createParam<JwSmallSnapKnob>(Vec(652, pitchParamYVal), module, NoteSeq::OCTAVE_KNOB_PARAM));

	///// NOTE AND SCALE CONTROLS /////
	float labelY = 148;

	NoteKnob *noteKnob = dynamic_cast<NoteKnob*>(createParam<NoteKnob>(Vec(620, pitchParamYVal), module, NoteSeq::NOTE_KNOB_PARAM));
	CenteredLabel* const noteLabel = new CenteredLabel;
	noteLabel->box.pos = Vec(316, labelY);
	noteLabel->text = "";
	noteKnob->connectLabel(noteLabel, module);
	addChild(noteLabel);
	addParam(noteKnob);

	ScaleKnob *scaleKnob = dynamic_cast<ScaleKnob*>(createParam<ScaleKnob>(Vec(683, pitchParamYVal), module, NoteSeq::SCALE_KNOB_PARAM));
	CenteredLabel* const scaleLabel = new CenteredLabel;
	scaleLabel->box.pos = Vec(348, labelY);
	scaleLabel->text = "";
	scaleKnob->connectLabel(scaleLabel, module);
	addChild(scaleLabel);
	addParam(scaleKnob);

	float quantInpY = 300;
	addInput(createInput<TinyPJ301MPort>(Vec(623, quantInpY), module, NoteSeq::ROOT_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(656, quantInpY), module, NoteSeq::OCTAVE_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(690, quantInpY), module, NoteSeq::SCALE_INPUT));

	addOutput(createOutput<TinyPJ301MPort>(Vec(623, 345), module, NoteSeq::EOC_OUTPUT));
	addOutput(createOutput<Blue_TinyPJ301MPort>(Vec(656, 345), module, NoteSeq::POLY_VOCT_OUTPUT));
	addOutput(createOutput<Blue_TinyPJ301MPort>(Vec(690, 345), module, NoteSeq::POLY_GATE_OUTPUT));
}

struct NoteSeqGateModeItem : MenuItem {
	NoteSeq *noteSeq;
	NoteSeq::GateMode gateMode;
	void onAction(const event::Action &e) override {
		noteSeq->gateMode = gateMode;
	}
	void step() override {
		rightText = (noteSeq->gateMode == gateMode) ? "✔" : "";
		MenuItem::step();
	}
};


void NoteSeqWidget::appendContextMenu(Menu *menu) {
	NoteSeq *noteSeq = dynamic_cast<NoteSeq*>(module);
	MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);

	NSChannelItem *channelItem = new NSChannelItem;
	channelItem->text = "Polyphony channels";
	channelItem->rightText = string::f("%d", noteSeq->channels) + " " +RIGHT_ARROW;
	channelItem->module = noteSeq;
	menu->addChild(channelItem);
	
	MenuLabel *spacerLabel2 = new MenuLabel();
	menu->addChild(spacerLabel2);

	MenuLabel *modeLabel = new MenuLabel();
	modeLabel->text = "Gate Mode";
	menu->addChild(modeLabel);

	NoteSeqGateModeItem *triggerItem = new NoteSeqGateModeItem();
	triggerItem->text = "Trigger";
	triggerItem->noteSeq = noteSeq;
	triggerItem->gateMode = NoteSeq::TRIGGER;
	menu->addChild(triggerItem);

	NoteSeqGateModeItem *retriggerItem = new NoteSeqGateModeItem();
	retriggerItem->text = "Retrigger";
	retriggerItem->noteSeq = noteSeq;
	retriggerItem->gateMode = NoteSeq::RETRIGGER;
	menu->addChild(retriggerItem);

	NoteSeqGateModeItem *continuousItem = new NoteSeqGateModeItem();
	continuousItem->text = "Continuous";
	continuousItem->noteSeq = noteSeq;
	continuousItem->gateMode = NoteSeq::CONTINUOUS;
	menu->addChild(continuousItem);
}

Model *modelNoteSeq = createModel<NoteSeq, NoteSeqWidget>("NoteSeq");
