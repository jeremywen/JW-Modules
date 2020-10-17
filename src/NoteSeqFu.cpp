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
		NUM_PARAMS = PLAYHEAD_ON_PARAM + 4
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
		NUM_INPUTS
	};
	enum OutputIds {
		POLY_VOCT_OUTPUT, //EACH PLAYHEAD
		POLY_GATE_OUTPUT = POLY_VOCT_OUTPUT + 4, //EACH PLAYHEAD
		EOC_OUTPUT = POLY_GATE_OUTPUT + 4, //EACH PLAYHEAD
		NUM_OUTPUTS = EOC_OUTPUT + 4 //EACH PLAYHEAD
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
		RND_CHORD_LIKE,
		RND_MIRROR_X,
		RND_MIRROR_Y,
		NUM_RND_MODES
	};
	enum ShiftDirection {
		DIR_UP,
		DIR_DOWN
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
	PlayHead *playHeads = new PlayHead[4];
	int channels = 1;
	float rndFloat0to1AtClockStep = random::uniform();
	bool resetMode = false;
	bool *cells = new bool[CELLS];
	bool *newCells = new bool[CELLS];
	ColNotes *colNotesCache = new ColNotes[COLS];
	ColNotes *colNotesCache2 = new ColNotes[COLS];
	dsp::SchmittTrigger clockTrig, resetTrig, clearTrig;
	dsp::SchmittTrigger rndTrig, shiftUpTrig, shiftDownTrig;
	dsp::SchmittTrigger rotateRightTrig, rotateLeftTrig, flipHorizTrig, flipVertTrig;
	dsp::PulseGenerator mainClockPulse;

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
		configParam(SHIFT_AMT_KNOB_PARAM, -32, 32.0, 1.0, "Shift Amount");
		configParam(ROT_RIGHT_BTN_PARAM, 0.0, 1.0, 0.0, "Rotate Right");
		configParam(ROT_LEFT_BTN_PARAM, 0.0, 1.0, 0.0, "Rotate Left");
		configParam(FLIP_HORIZ_BTN_PARAM, 0.0, 1.0, 0.0, "Flip Horizontal");
		configParam(FLIP_VERT_BTN_PARAM, 0.0, 1.0, 0.0, "Flip Vertical");
		configParam(LIFE_ON_SWITCH_PARAM, 0.0, 1.0, 0.0, "Life Switch");
		configParam(LIFE_SPEED_KNOB_PARAM, 1.0, 16.0, 12.0, "Life Speed");
		configParam(INCLUDE_INACTIVE_PARAM, 0.0, 1.0, 0.0, "Drum Mode");
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

		if (resetTrig.process(params[RESET_BTN_PARAM].getValue() + inputs[RESET_INPUT].getVoltage())) {
			resetMode = true;
		}

		if (clockTrig.process(inputs[CLOCK_INPUT].getVoltage() + params[STEP_BTN_PARAM].getValue())) {
			if(resetMode){
				resetMode = false;
				resetSeqToEnd();
			}
			clockStep();
		}

		// ////////////////////////////////////////////// POLY OUTPUTS //////////////////////////////////////////////
		bool mainPulse = mainClockPulse.process(1.0 / args.sampleRate);
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
					}
					float gateVolts = pulse && cellActive ? 10.0 : 0.0;
					outputs[POLY_GATE_OUTPUT + p].setVoltage(gateVolts, i);
				}
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
		return 32;
	}

	int getFinalLowestNote1to32(){
		return 1;
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
		mainClockPulse.trigger(1e-1);
		lifeCounter++;
		rndFloat0to1AtClockStep = random::uniform();
		for(int i=0;i<4;i++){
			int seqLen = getSeqLen(i);
			int curPlayMode = getPlayMode(i);
			int seqPos = playHeads[i].seqPos;
			bool goingForward = playHeads[i].goingForward;
			bool eocOn = false;

			playHeads[i].ticksSinceDivision++;
			if(playHeads[i].ticksSinceDivision % int(params[DIVISION_KNOB_PARAM + i].getValue()) == 0){
				playHeads[i].gatePulse.trigger(1e-1);

				// i dono if i need this - somehow it stays past the end sometimes
				if(seqPos > seqLen){
					seqPos = seqLen - 1;
				}

				if(curPlayMode == PM_FWD_LOOP){
					seqPos = (seqPos + 1) % seqLen;
					goingForward = true;
					if(seqPos == 0){
						eocOn = true;
					}
				} else if(curPlayMode == PM_BWD_LOOP){
					seqPos = seqPos > 0 ? seqPos - 1 : seqLen - 1;
					goingForward = false;
					if(seqPos == seqLen - 1){
						eocOn = true;
					}
				} else if(curPlayMode == PM_FWD_BWD_LOOP || curPlayMode == PM_BWD_FWD_LOOP){
					if(goingForward){
						if(seqPos < seqLen - 1){
							seqPos++;
						} else {
							seqPos--;
							goingForward = false;
							eocOn = true;
						}
					} else {
						if(seqPos > 0){
							seqPos--;
						} else {
							if(seqPos + 1 < seqLen){
								seqPos++;
							}
							goingForward = true;
							eocOn = true;
						}
					}
				} else if(curPlayMode == PM_RANDOM_POS){
					seqPos = int(random::uniform() * seqLen);
				}
				playHeads[i].seqPos = clampijw(seqPos, 0, seqLen - 1);
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
				int newY = 0;
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

struct NoteSeqFuDisplay : Widget {
	NoteSeqFu *module;
	bool currentlyTurningOn = false;
	float initX = 0;
	float initY = 0;
	float dragX = 0;
	float dragY = 0;
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
			// e.target = this;
			initX = e.pos.x;
			initY = e.pos.y;
			currentlyTurningOn = !module->isCellOnByDisplayPos(e.pos.x, e.pos.y);
			module->setCellOnByDisplayPos(e.pos.x, e.pos.y, currentlyTurningOn);
		}
	}
	
	void onDragStart(const event::DragStart &e) override {
		dragX = APP->scene->rack->mousePos.x;
		dragY = APP->scene->rack->mousePos.y;
	}

	void onDragMove(const event::DragMove &e) override {
		float newDragX = APP->scene->rack->mousePos.x;
		float newDragY = APP->scene->rack->mousePos.y;
		module->setCellOnByDisplayPos(initX+(newDragX-dragX), initY+(newDragY-dragY), currentlyTurningOn);
	}

	void draw(const DrawArgs &args) override {
		//background
		nvgFillColor(args.vg, nvgRGB(20, 30, 33));
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

		for(int i=0;i<4;i++){
			if(module->params[NoteSeqFu::PLAYHEAD_ON_PARAM + i].getValue()){
				//seq length line
				float colLimitX = module->getSeqLen(i) * HW;
				nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, colLimitX, box.size.y * 0.125);
				nvgLineTo(args.vg, colLimitX, box.size.y);
				nvgStroke(args.vg);
				nvgStrokeColor(args.vg, colors[i]);
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, colLimitX, 0);
				nvgLineTo(args.vg, colLimitX, box.size.y * 0.125);
				nvgStroke(args.vg);

				//seq pos
				nvgStrokeColor(args.vg, colors[i]);
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, module->playHeads[i].seqPos * HW, 0);
				nvgLineTo(args.vg, module->playHeads[i].seqPos * HW, box.size.y);
				nvgStroke(args.vg);
			}
		}
	}
};

struct PlayModeKnob : JwSmallSnapKnob {
	PlayModeKnob(){}
	std::string formatCurrentValue() override {
		if(paramQuantity != NULL){
			switch(int(paramQuantity->getValue())){
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
		if(paramQuantity != NULL){
			switch(int(paramQuantity->getValue())){
				case NoteSeqFu::RND_BASIC:return "Basic";
				case NoteSeqFu::RND_EUCLID:return "Euclid";
				case NoteSeqFu::RND_SIN_WAVE:return "Sine";
				case NoteSeqFu::RND_LIFE_GLIDERS:return "Gliders";
				case NoteSeqFu::RND_CHORD_LIKE:return "Chord-Like";
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

	SVGPanel *panel = new SVGPanel();
	panel->box.size = box.size;
	panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/NoteSeqFu.svg")));
	addChild(panel);

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
	addInput(createInput<TinyPJ301MPort>(Vec(15, 40), module, NoteSeqFu::CLOCK_INPUT));

	///// NOTE AND SCALE CONTROLS /////
	float pitchParamYVal = 35;
	float labelY = 36;
	NoteKnob *noteKnob = dynamic_cast<NoteKnob*>(createParam<NoteKnob>(Vec(97, pitchParamYVal), module, NoteSeqFu::NOTE_KNOB_PARAM));
	CenteredLabel* const noteLabel = new CenteredLabel;
	noteLabel->box.pos = Vec(55, labelY);
	noteLabel->text = "";
	noteKnob->connectLabel(noteLabel, module);
	addChild(noteLabel);
	addParam(noteKnob);

	ScaleKnob *scaleKnob = dynamic_cast<ScaleKnob*>(createParam<ScaleKnob>(Vec(135, pitchParamYVal), module, NoteSeqFu::SCALE_KNOB_PARAM));
	CenteredLabel* const scaleLabel = new CenteredLabel;
	scaleLabel->box.pos = Vec(73, labelY);
	scaleLabel->text = "";
	scaleKnob->connectLabel(scaleLabel, module);
	addChild(scaleLabel);
	addParam(scaleKnob);

	// float quantInpY = 300;
	// addInput(createInput<TinyPJ301MPort>(Vec(623, quantInpY), module, NoteSeqFu::ROOT_INPUT));
	// addInput(createInput<TinyPJ301MPort>(Vec(656, quantInpY), module, NoteSeqFu::OCTAVE_INPUT));
	// addInput(createInput<TinyPJ301MPort>(Vec(690, quantInpY), module, NoteSeqFu::SCALE_INPUT));

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

	addInput(createInput<TinyPJ301MPort>(Vec(60, 304), module, NoteSeqFu::SHIFT_UP_INPUT));
	addParam(createParam<SmallButton>(Vec(80, 299), module, NoteSeqFu::SHIFT_UP_BTN_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(118, 304), module, NoteSeqFu::SHIFT_DOWN_INPUT));
	addParam(createParam<SmallButton>(Vec(138, 299), module, NoteSeqFu::SHIFT_DOWN_BTN_PARAM));

	addInput(createInput<TinyPJ301MPort>(Vec(30, 350), module, NoteSeqFu::SHIFT_AMT_INPUT));
	addParam(createParam<JwSmallSnapKnob>(Vec(50, 345), module, NoteSeqFu::SHIFT_AMT_KNOB_PARAM));

	addParam(createParam<JwHorizontalSwitch>(Vec(102, 350), module, NoteSeqFu::LIFE_ON_SWITCH_PARAM));
	addParam(createParam<JwSmallSnapKnob>(Vec(138, 345), module, NoteSeqFu::LIFE_SPEED_KNOB_PARAM));

	///////////////////////////////////////////////////// RIGHT SIDE /////////////////////////////////////////////////////
	
	float yTop = 31;
	for(int i=0;i<4;i++){

		addParam(createParam<JwSmallSnapKnob>(Vec(555, yTop), module, NoteSeqFu::START_KNOB_PARAM + i));
		addParam(createParam<JwSmallSnapKnob>(Vec(595, yTop), module, NoteSeqFu::LENGTH_KNOB_PARAM + i));
		addParam(createParam<JwSmallSnapKnob>(Vec(630, yTop), module, NoteSeqFu::DIVISION_KNOB_PARAM + i));
		addParam(createParam<JwSmallSnapKnob>(Vec(660, yTop), module, NoteSeqFu::OCTAVE_KNOB_PARAM + i));
		addParam(createParam<JwSmallSnapKnob>(Vec(690, yTop), module, NoteSeqFu::SEMI_KNOB_PARAM + i));

		addParam(createParam<JwHorizontalSwitch>(Vec(566, yTop+48), module, NoteSeqFu::PLAYHEAD_ON_PARAM + i));

		PlayModeKnob *playModeKnob = dynamic_cast<PlayModeKnob*>(createParam<PlayModeKnob>(Vec(595, yTop+40), module, NoteSeqFu::PLAY_MODE_KNOB_PARAM + i));
		CenteredLabel* const playModeLabel = new CenteredLabel;
		playModeLabel->box.pos = Vec(303.5, 52+(i*42)); 
		playModeLabel->text = "";
		playModeKnob->connectLabel(playModeLabel, module);
		addChild(playModeLabel);
		addParam(playModeKnob);

		addOutput(createOutput<TinyPJ301MPort>(Vec(620, yTop+48), module, NoteSeqFu::EOC_OUTPUT + i));
		addOutput(createOutput<Blue_TinyPJ301MPort>(Vec(650, yTop+48), module, NoteSeqFu::POLY_VOCT_OUTPUT + i));
		addOutput(createOutput<Blue_TinyPJ301MPort>(Vec(680, yTop+48), module, NoteSeqFu::POLY_GATE_OUTPUT + i));

		yTop+=85.5;
	}

	addParam(createParam<JwHorizontalSwitch>(Vec(590, 362), module, NoteSeqFu::REPEATS_PARAM));
	addParam(createParam<JwHorizontalSwitch>(Vec(668, 362), module, NoteSeqFu::INCLUDE_INACTIVE_PARAM));
}

void NoteSeqFuWidget::appendContextMenu(Menu *menu) {
	NoteSeqFu *noteSeq = dynamic_cast<NoteSeqFu*>(module);
	MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);

	NSFChannelItem *channelItem = new NSFChannelItem;
	channelItem->text = "Polyphony channels";
	channelItem->rightText = string::f("%d", noteSeq->channels) + " " +RIGHT_ARROW;
	channelItem->module = noteSeq;
	menu->addChild(channelItem);
}

Model *modelNoteSeqFu = createModel<NoteSeqFu, NoteSeqFuWidget>("NoteSeqFu");
