#include <string.h>
#include <algorithm>
#include "JWModules.hpp"

#define ROWS 16
#define COLS 16
#define CELLS 256
#define POLY 16
#define HW 11.75 //cell height and width

struct ColNotes {
	int *vals = new int[16];
	bool includeInactive;
	bool valid;
	int finalHigh;
	int finalLow;

	~ColNotes() {
		delete [] vals;
	}
};

struct NoteSeq16 : Module,QuantizeUtils {
	enum ParamIds {
		LENGTH_KNOB_PARAM,
		PLAY_MODE_KNOB_PARAM,
		CLEAR_BTN_PARAM,
		RND_TRIG_BTN_PARAM,
		RND_AMT_KNOB_PARAM,
		SCALE_KNOB_PARAM,
		NOTE_KNOB_PARAM,
		OCTAVE_KNOB_PARAM,
		LOW_HIGH_SWITCH_PARAM,
		INCLUDE_INACTIVE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		RESET_INPUT,
		RND_TRIG_INPUT,
		ROTATE_INPUT,
		FLIP_INPUT,
		SHIFT_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		POLY_VOCT_OUTPUT,
		POLY_GATE_OUTPUT,
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
	int seqPos = 0;
	int channels = 1;
	float rndFloat0to1AtClockStep = random::uniform();
	bool goingForward = true;
	bool resetMode = false;
	bool *cells = new bool[CELLS];
	bool *newCells = new bool[CELLS];
	ColNotes *colNotesCache = new ColNotes[COLS];
	ColNotes *colNotesCache2 = new ColNotes[COLS];
	dsp::SchmittTrigger clockTrig, resetTrig, clearTrig;
	dsp::SchmittTrigger rndTrig, shiftUpTrig, shiftDownTrig;
	dsp::SchmittTrigger rotateRightTrig, rotateLeftTrig, flipHorizTrig, flipVertTrig;
	dsp::PulseGenerator gatePulse;

	NoteSeq16() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(LENGTH_KNOB_PARAM, 1.0, 16.0, 16.0);
		configParam(PLAY_MODE_KNOB_PARAM, 0.0, NUM_PLAY_MODES - 1, 0.0);
		configParam(CLEAR_BTN_PARAM, 0.0, 1.0, 0.0);
		configParam(RND_TRIG_BTN_PARAM, 0.0, 1.0, 0.0);
		configParam(RND_AMT_KNOB_PARAM, 0.0, 1.0, 0.1);
		configParam(INCLUDE_INACTIVE_PARAM, 0.0, 1.0, 0.0);
		configParam(OCTAVE_KNOB_PARAM, -5.0, 7.0, 0.0);
		configParam(NOTE_KNOB_PARAM, 0.0, QuantizeUtils::NUM_NOTES-1, QuantizeUtils::NOTE_C);
		configParam(SCALE_KNOB_PARAM, 0.0, QuantizeUtils::NUM_SCALES-1, QuantizeUtils::MINOR);
		resetSeq();
		resetMode = true;
		clearCells();
	}

	~NoteSeq16() {
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

	//TODO maybe add length input
	//TODO maybe add start pos knob and input

	void process(const ProcessArgs &args) override {
		if (clearTrig.process(params[CLEAR_BTN_PARAM].getValue())) { clearCells(); }
		if (rndTrig.process(params[RND_TRIG_BTN_PARAM].getValue() + inputs[RND_TRIG_INPUT].getVoltage())) { randomizeCells(); }
		if (rotateRightTrig.process(inputs[ROTATE_INPUT].getVoltage())) { rotateCells(DIR_RIGHT); }
		if (flipVertTrig.process(inputs[FLIP_INPUT].getVoltage())) { flipCells(DIR_VERT); }
		if (shiftUpTrig.process(inputs[SHIFT_INPUT].getVoltage())) { shiftCells(DIR_UP); }
		if (resetTrig.process(inputs[RESET_INPUT].getVoltage())) {
			resetMode = true;
		}

		if (clockTrig.process(inputs[CLOCK_INPUT].getVoltage())) {
			if(resetMode){
				resetMode = false;
				resetSeqToEnd();
			}
			clockStep();
		}

		bool pulse = gatePulse.process(1.0 / args.sampleRate);
		// ////////////////////////////////////////////// POLY OUTPUTS //////////////////////////////////////////////
		
		int *polyYVals = getYValsFromBottomAtSeqPos(params[INCLUDE_INACTIVE_PARAM].getValue());
		for(int i=0;i<channels;i++){ //param # starts from bottom
			bool hasVal = polyYVals[i] > -1;
			bool cellActive = hasVal && cells[iFromXY(seqPos, ROWS - polyYVals[i] - 1)];
			if(cellActive){ 
				float volts = closestVoltageForRow(polyYVals[i]);
				// outputs[VOCT_MAIN_OUTPUT + i].setVoltage(volts);
				outputs[POLY_VOCT_OUTPUT].setVoltage(volts, i);
			}
			float gateVolts = pulse && cellActive ? 10.0 : 0.0;
			// outputs[GATE_MAIN_OUTPUT + i].setVoltage(gateVolts);
			outputs[POLY_GATE_OUTPUT].setVoltage(gateVolts, i);
			// lights[GATES_LIGHT + i].value = cellActive ? 1.0 : 0.0;			
		}
		outputs[POLY_GATE_OUTPUT].setChannels(channels);
		outputs[POLY_VOCT_OUTPUT].setChannels(channels);
	}

	int * getYValsFromBottomAtSeqPos(bool includeInactive){
		int finalHigh = getFinalHighestNote1to16();
		int finalLow = getFinalLowestNote1to16();
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

	int getFinalHighestNote1to16(){
		return 16;
	}

	int getFinalLowestNote1to16(){
		return 1;
	}

	float closestVoltageForRow(int cellYFromBottom){
		int octave = params[OCTAVE_KNOB_PARAM].getValue();
		int rootNote = params[NOTE_KNOB_PARAM].getValue();
		int scale = params[SCALE_KNOB_PARAM].getValue();
		return closestVoltageInScale(octave + (cellYFromBottom * 0.0833), rootNote, scale);
	}

	void clockStep(){
		gatePulse.trigger(1e-1);
		lifeCounter++;
		rndFloat0to1AtClockStep = random::uniform();

		//iterate seq pos
		int curPlayMode = int(params[PLAY_MODE_KNOB_PARAM].getValue());
		int seqLen = int(params[NoteSeq16::LENGTH_KNOB_PARAM].getValue());
		if(curPlayMode == PM_FWD_LOOP){
			seqPos = (seqPos + 1) % seqLen;
			goingForward = true;
		} else if(curPlayMode == PM_BWD_LOOP){
			seqPos = seqPos > 0 ? seqPos - 1 : seqLen - 1;
			goingForward = false;
		} else if(curPlayMode == PM_FWD_BWD_LOOP || curPlayMode == PM_BWD_FWD_LOOP){
			if(goingForward){
				if(seqPos < seqLen - 1){
					seqPos++;
				} else {
					seqPos--;
					goingForward = false;
				}
			} else {
				if(seqPos > 0){
					seqPos--;
				} else {
					seqPos++;
					goingForward = true;
				}
			}
		} else if(curPlayMode == PM_RANDOM_POS){
			seqPos = int(random::uniform() * seqLen);
		}
	}

	void resetSeq(){
		int curPlayMode = int(params[PLAY_MODE_KNOB_PARAM].getValue());
		if(curPlayMode == PM_FWD_LOOP || curPlayMode == PM_FWD_BWD_LOOP || curPlayMode == PM_RANDOM_POS){
			seqPos = 0;
		} else if(curPlayMode == PM_BWD_LOOP || curPlayMode == PM_BWD_FWD_LOOP){
			seqPos = int(params[NoteSeq16::LENGTH_KNOB_PARAM].getValue()) - 1;
		}
	}

	void resetSeqToEnd(){
		int curPlayMode = int(params[PLAY_MODE_KNOB_PARAM].getValue());
		if(curPlayMode == PM_FWD_LOOP || curPlayMode == PM_FWD_BWD_LOOP || curPlayMode == PM_RANDOM_POS){
			seqPos = int(params[NoteSeq16::LENGTH_KNOB_PARAM].getValue()) - 1;
		} else if(curPlayMode == PM_BWD_LOOP || curPlayMode == PM_BWD_FWD_LOOP){
			seqPos = 0;
		}
	}

	void clearCells() {
		for(int i=0;i<CELLS;i++){
			cells[i] = false;
		}
		gridChanged();
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
		int amount = 1;
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

	void swapCells() {
		std::swap(cells, newCells);
		gridChanged();

		for(int i=0;i<CELLS;i++){
			newCells[i] = false;
		}
	}
	void randomizeCells() {
		clearCells();
		float rndAmt = params[RND_AMT_KNOB_PARAM].getValue();
		switch(0){
		// switch(int(params[RND_MODE_KNOB_PARAM].getValue())){
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
		}
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

struct NoteSeq16Display : Widget {
	NoteSeq16 *module;
	bool currentlyTurningOn = false;
	float initX = 0;
	float initY = 0;
	float dragX = 0;
	float dragY = 0;
	NoteSeq16Display(){}

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
		// nvgFillColor(args.vg, nvgRGB(25, 150, 252)); //blue
		nvgFillColor(args.vg, nvgRGB(255, 151, 9));//orange
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

		//seq length line
		float colLimitX = module->params[NoteSeq16::LENGTH_KNOB_PARAM].getValue() * HW;
		// nvgStrokeColor(args.vg, nvgRGB(144, 26, 252));//purple
		nvgStrokeColor(args.vg, nvgRGB(25, 150, 252));//blue
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, colLimitX, 0);
		nvgLineTo(args.vg, colLimitX, box.size.y);
		nvgStroke(args.vg);

		//seq pos
		nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
		nvgBeginPath(args.vg);
		nvgRect(args.vg, module->seqPos * HW, 0, HW, box.size.y);
		nvgStroke(args.vg);
	}
};

struct PlayModeKnob : JwSmallSnapKnob {
	PlayModeKnob(){}
	std::string formatCurrentValue() override {
		if(paramQuantity != NULL){
			switch(int(paramQuantity->getValue())){
				case NoteSeq16::PM_FWD_LOOP:return "→";
				case NoteSeq16::PM_BWD_LOOP:return "←";
				case NoteSeq16::PM_FWD_BWD_LOOP:return "→←";
				case NoteSeq16::PM_BWD_FWD_LOOP:return "←→";
				case NoteSeq16::PM_RANDOM_POS:return "*";
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
				case NoteSeq16::RND_BASIC:return "Basic";
				case NoteSeq16::RND_EUCLID:return "Euclid";
				case NoteSeq16::RND_SIN_WAVE:return "Sine";
				case NoteSeq16::RND_LIFE_GLIDERS:return "Gliders";
			}
		}
		return "";
	}
};

struct NoteSeq16Widget : ModuleWidget { 
	NoteSeq16Widget(NoteSeq16 *module); 
	void appendContextMenu(Menu *menu) override;
};

struct NS16ChannelValueItem : MenuItem {
	NoteSeq16 *module;
	int channels;
	void onAction(const event::Action &e) override {
		module->channels = channels;
	}
};

struct NS16ChannelItem : MenuItem {
	NoteSeq16 *module;
	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		for (int channels = 1; channels <= 16; channels++) {
			NS16ChannelValueItem *item = new NS16ChannelValueItem;
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


NoteSeq16Widget::NoteSeq16Widget(NoteSeq16 *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*13, RACK_GRID_HEIGHT);

	SVGPanel *panel = new SVGPanel();
	panel->box.size = box.size;
	panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/NoteSeq16.svg")));
	addChild(panel);

	NoteSeq16Display *display = new NoteSeq16Display();
	display->module = module;
	display->box.pos = Vec(3, 75);
	display->box.size = Vec(188, 188);
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
	addInput(createInput<TinyPJ301MPort>(Vec(22, 40), module, NoteSeq16::CLOCK_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(56, 40), module, NoteSeq16::RESET_INPUT));
	addParam(createParam<JwSmallSnapKnob>(Vec(87, 35), module, NoteSeq16::LENGTH_KNOB_PARAM));
	
	PlayModeKnob *playModeKnob = dynamic_cast<PlayModeKnob*>(createParam<PlayModeKnob>(Vec(120, 35), module, NoteSeq16::PLAY_MODE_KNOB_PARAM));
	CenteredLabel* const playModeLabel = new CenteredLabel;
	playModeLabel->box.pos = Vec(66.5, 35);
	playModeLabel->text = "";
	playModeKnob->connectLabel(playModeLabel, module);
	addChild(playModeLabel);
	addParam(playModeKnob);

	addParam(createParam<SmallButton>(Vec(156, 36), module, NoteSeq16::CLEAR_BTN_PARAM));


	//row 3
	addInput(createInput<TinyPJ301MPort>(Vec(5, 301), module, NoteSeq16::RND_TRIG_INPUT));
	addParam(createParam<SmallButton>(Vec(25, 296), module, NoteSeq16::RND_TRIG_BTN_PARAM));
	addParam(createParam<SmallWhiteKnob>(Vec(51, 295), module, NoteSeq16::RND_AMT_KNOB_PARAM));

	float bottomInpY = 338;
	addInput(createInput<TinyPJ301MPort>(Vec(38, bottomInpY), module, NoteSeq16::ROTATE_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(68, bottomInpY), module, NoteSeq16::FLIP_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(96, bottomInpY), module, NoteSeq16::SHIFT_INPUT));

	///////////////////////////////////////////////////// RIGHT SIDE /////////////////////////////////////////////////////

	addOutput(createOutput<Blue_TinyPJ301MPort>(Vec(139, bottomInpY), module, NoteSeq16::POLY_VOCT_OUTPUT));
	addOutput(createOutput<Blue_TinyPJ301MPort>(Vec(171, bottomInpY), module, NoteSeq16::POLY_GATE_OUTPUT));
	addParam(createParam<JwHorizontalSwitch>(Vec(140, 361), module, NoteSeq16::INCLUDE_INACTIVE_PARAM));

	///// NOTE AND SCALE CONTROLS /////
	float pitchParamYVal = 280;
	float labelY = 160;

	NoteKnob *noteKnob = dynamic_cast<NoteKnob*>(createParam<NoteKnob>(Vec(93, pitchParamYVal), module, NoteSeq16::NOTE_KNOB_PARAM));
	CenteredLabel* const noteLabel = new CenteredLabel;
	noteLabel->box.pos = Vec(53, labelY);
	noteLabel->text = "";
	noteKnob->connectLabel(noteLabel, module);
	addChild(noteLabel);
	addParam(noteKnob);

	addParam(createParam<JwSmallSnapKnob>(Vec(125, pitchParamYVal), module, NoteSeq16::OCTAVE_KNOB_PARAM));

	ScaleKnob *scaleKnob = dynamic_cast<ScaleKnob*>(createParam<ScaleKnob>(Vec(156, pitchParamYVal), module, NoteSeq16::SCALE_KNOB_PARAM));
	CenteredLabel* const scaleLabel = new CenteredLabel;
	scaleLabel->box.pos = Vec(84, labelY);
	scaleLabel->text = "";
	scaleKnob->connectLabel(scaleLabel, module);
	addChild(scaleLabel);
	addParam(scaleKnob);
}

void NoteSeq16Widget::appendContextMenu(Menu *menu) {
	NoteSeq16 *noteSeq16 = dynamic_cast<NoteSeq16*>(module);
	MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);

	NS16ChannelItem *channelItem = new NS16ChannelItem;
	channelItem->text = "Polyphony channels";
	channelItem->rightText = string::f("%d", noteSeq16->channels) + " " +RIGHT_ARROW;
	channelItem->module = noteSeq16;
	menu->addChild(channelItem);
}


Model *modelNoteSeq16 = createModel<NoteSeq16, NoteSeq16Widget>("NoteSeq16");
