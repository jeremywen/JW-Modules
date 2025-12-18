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

struct Trigs : Module {
	enum ParamIds {
		LENGTH_KNOB_PARAM,
		PLAY_MODE_KNOB_PARAM,
		CLEAR_BTN_PARAM,
		RND_TRIG_BTN_PARAM,
		RND_AMT_KNOB_PARAM,
		START_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		RESET_INPUT,
		RND_TRIG_INPUT,
		ROTATE_INPUT,
		FLIP_INPUT,
		SHIFT_INPUT,
		LENGTH_INPUT,
		START_INPUT,
		PLAY_MODE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		GATE_OUTPUT,
		EOC_OUTPUT = GATE_OUTPUT + 4,
		OR_OUTPUT,
		XOR_OUTPUT,
		NOR_OUTPUT,
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
	int seqPos[4] = {0, 0, 0, 0};
	int channels = 1;
	float rndFloat0to1AtClockStep = random::uniform();
	bool goingForward[4] = {true, true, true, true};
	bool resetMode[4] = {false, false, false, false};
	bool eocOn[4] = {false, false, false, false}; 
	bool hitEnd[4] = {false, false, false, false}; 
	bool cells[CELLS];
	dsp::SchmittTrigger clockTrig[4], resetTrig[4], clearTrig;
	dsp::SchmittTrigger rndTrig[4], shiftUpTrig, shiftDownTrig;
	dsp::SchmittTrigger rotateRightTrig, rotateLeftTrig, flipHorizTrig, flipVertTrig;
	dsp::PulseGenerator gatePulse[4];

	// Gate pulse length in seconds (for outputs)
	float gatePulseLenSec = 0.005f;

	Trigs() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(START_PARAM, 0.0, 63.0, 0.0, "Start");
		configParam(LENGTH_KNOB_PARAM, 1.0, 64.0, 64.0, "Length");
		configParam(PLAY_MODE_KNOB_PARAM, 0.0, NUM_PLAY_MODES - 1, 0.0, "Play Mode");
		configParam(CLEAR_BTN_PARAM, 0.0, 1.0, 0.0, "Clear");
		configParam(RND_TRIG_BTN_PARAM, 0.0, 1.0, 0.0, "Random Trigger");
		configParam(RND_AMT_KNOB_PARAM, 0.0, 1.0, 0.1, "Random Amount");
		configInput(CLOCK_INPUT, "Clock");
		configInput(RESET_INPUT, "Reset");
		configInput(RND_TRIG_INPUT, "Random Trigger");
		configInput(LENGTH_INPUT, "Length");
		configInput(START_INPUT, "Start");
		configInput(PLAY_MODE_INPUT, "Play Mode");

		const char *colors[4] = { "Orange", "Yellow", "Purple", "Blue" };
		for(int i=0; i<4; i++){
			configOutput(GATE_OUTPUT + i, "Gate " + std::string(colors[i]));
		}
		configOutput(EOC_OUTPUT, "End of Cycle");
		configOutput(OR_OUTPUT, "OR");
		configOutput(XOR_OUTPUT, "XOR");
		configOutput(NOR_OUTPUT, "NOR");
		for (int i = 0; i < 4; i++) {
			resetSeq(i);
		}
		std::fill(resetMode, resetMode + 4, true);
		for (int i = 0; i < 4; i++) {
			clearCells(i);
		}
	}

	~Trigs() {
	}

	void onRandomize() override {
		for (int i = 0; i < 4; i++) {
			randomizeCells(i);
		}
	}

	void onReset() override {
		for (int i = 0; i < 4; i++) {
			resetSeq(i);
		}
		std::fill(resetMode, resetMode + 4, true);
		for (int i = 0; i < 4; i++) {
			clearCells(i);
		}
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

		// gate pulse length
		json_t *gatePulseLenSecJ = json_object_get(rootJ, "gatePulseLenSec");
		if (gatePulseLenSecJ) {
			gatePulseLenSec = (float) json_number_value(gatePulseLenSecJ);
			gatePulseLenSec = clampfjw(gatePulseLenSec, 0.001f, 1.0f);
		}
	}

	void process(const ProcessArgs &args) override {
		if (clearTrig.process(params[CLEAR_BTN_PARAM].getValue())) { 
			for (int i = 0; i < 4; i++) {
				clearCells(i);
			}
		}
		
		//if more than one reset input channel, then process separately
		int rndChannelCount = inputs[RND_TRIG_INPUT].getChannels();
		if(rndChannelCount > 1) {
			for (int i = 0; i < rndChannelCount; i++) {
				if (rndTrig[i].process(params[RND_TRIG_BTN_PARAM].getValue() + inputs[RND_TRIG_INPUT].getVoltage(i))) {
					randomizeCells(i);
				}
			}
		} else {
			for(int i=0;i<4;i++){
				if (rndTrig[i].process(params[RND_TRIG_BTN_PARAM].getValue() + inputs[RND_TRIG_INPUT].getVoltage())) {
					randomizeCells(i);
				}
			}			
		}

		//if more than one reset input channel, then process separately
		int resetChannelCount = inputs[RESET_INPUT].getChannels();
		if(resetChannelCount > 1) {
			for (int i = 0; i < resetChannelCount; i++) {
				if (resetTrig[i].process(inputs[RESET_INPUT].getVoltage(i))) {
					resetMode[i] = true;
				}
			}
		} else {
			for(int i=0;i<4;i++){
				if (resetTrig[i].process(inputs[RESET_INPUT].getVoltage())) {
					resetMode[i] = true;
				}			
			}			
		}

		//if more than one clock input channel, then process separately
		int clockChannelCount = inputs[CLOCK_INPUT].getChannels();
		if(clockChannelCount > 1) {
			for (int i = 0; i < clockChannelCount; i++) {
				if (clockTrig[i].process(inputs[CLOCK_INPUT].getVoltage(i))) {
					if (resetMode[i]) {
						resetMode[i] = false;
						hitEnd[i] = false;
						resetSeqToEnd(i);
					}
					clockStep(i);
				}
			}
		} else {
			for(int i=0;i<4;i++){
				if (clockTrig[i].process(inputs[CLOCK_INPUT].getVoltage())) {
					if (resetMode[i]) {
						resetMode[i] = false;
						hitEnd[i] = false;
						resetSeqToEnd(i);
					}
					clockStep(i);
				}
			}
		}

		int trigCount = 0;
		bool atLeastOnePulse = false;
		for(int i=0;i<4;i++){
			bool pulse = gatePulse[i].process(1.0 / args.sampleRate);
			if (pulse) {
				atLeastOnePulse = true;
			}
			int seqPosX = xFromSeqPos(i);
			bool cellActive = cells[iFromXY(seqPosX, (seqPos[i]/16) + i*4)];
			float gateVolts = (pulse && cellActive) ? 10.0 : 0.0;
			trigCount += (pulse && cellActive);
			outputs[GATE_OUTPUT + i].setVoltage(gateVolts);
			outputs[EOC_OUTPUT].setVoltage((pulse && eocOn[i]) ? 10.0 : 0.0, i);
		}
		outputs[EOC_OUTPUT].setChannels(clockChannelCount);

		outputs[OR_OUTPUT].setVoltage((atLeastOnePulse && trigCount>0) ? 10.0 : 0.0);
		outputs[NOR_OUTPUT].setVoltage((atLeastOnePulse && trigCount==0) ? 10.0 : 0.0);
		outputs[XOR_OUTPUT].setVoltage((atLeastOnePulse && trigCount==1) ? 10.0 : 0.0);
	}

	int xFromSeqPos(int seqIndex) {
		return seqPos[seqIndex] % 16;
	}

	int findYValIdx(int arr[], int valToFind){
		for(int i=0; i < ROWS; i++){
			if(arr[i] == valToFind){
				return i;
			}
		}		
		return -1;
	}

	int getFinalHighestNote1to16(){
		return 16;
	}

	int getFinalLowestNote1to16(){
		return 1;
	} 

	void clockStep(int seqIndex){
		gatePulse[seqIndex].trigger(gatePulseLenSec);
		rndFloat0to1AtClockStep = random::uniform();
		
		int curPlayMode = getPlayMode(seqIndex);
		int seqLen = getSeqLen(seqIndex);
		int seqStart = getSeqStart(seqIndex);
		int seqEnd = getSeqEnd(seqIndex);
		eocOn[seqIndex] = false;
		
		if(curPlayMode == PM_FWD_LOOP){
			seqPos[seqIndex]++;
			if(seqPos[seqIndex] > seqEnd){ 
				seqPos[seqIndex] = seqStart; 
				if(hitEnd[seqIndex]){eocOn[seqIndex] = true;}
				hitEnd[seqIndex] = true;
			}
			goingForward[seqIndex] = true;
		} else if(curPlayMode == PM_BWD_LOOP){
			seqPos[seqIndex] = seqPos[seqIndex] > seqStart ? seqPos[seqIndex] - 1 : seqEnd;
			goingForward[seqIndex] = false;
			if(seqPos[seqIndex] == seqEnd){
				if(hitEnd[seqIndex]){eocOn[seqIndex] = true;}
				hitEnd[seqIndex] = true;
			}
		} else if(curPlayMode == PM_FWD_BWD_LOOP || curPlayMode == PM_BWD_FWD_LOOP){
			if(goingForward[seqIndex]){
				if(seqPos[seqIndex] < seqEnd){
					seqPos[seqIndex]++;
				} else {
					seqPos[seqIndex]--;
					goingForward[seqIndex] = false;
					if(hitEnd[seqIndex]){eocOn[seqIndex] = true;}
					hitEnd[seqIndex] = true;
				}
			} else {
				if(seqPos[seqIndex] > seqStart){
					seqPos[seqIndex]--;
				} else {
					seqPos[seqIndex]++;
					goingForward[seqIndex] = true;
					if(hitEnd[seqIndex]){eocOn[seqIndex] = true;}
					hitEnd[seqIndex] = true;
				}
			}
		} else if(curPlayMode == PM_RANDOM_POS){
			seqPos[seqIndex] = seqStart + int(random::uniform() * seqLen);
		}
		seqPos[seqIndex] = clampijw(seqPos[seqIndex], seqStart, seqEnd);
	}

	void resetSeq(int seqIndex){
		int curPlayMode = getPlayMode(seqIndex);
		if(curPlayMode == PM_FWD_LOOP || curPlayMode == PM_FWD_BWD_LOOP || curPlayMode == PM_RANDOM_POS){
			seqPos[seqIndex] = getSeqStart(seqIndex);
		} else if(curPlayMode == PM_BWD_LOOP || curPlayMode == PM_BWD_FWD_LOOP){
			seqPos[seqIndex] = clampijw(getSeqStart(seqIndex) + getSeqLen(seqIndex), 0, 63);
		}
	}

	void resetSeqToEnd(int seqIndex){
		int curPlayMode = getPlayMode(seqIndex);
		if(curPlayMode == PM_FWD_LOOP || curPlayMode == PM_FWD_BWD_LOOP || curPlayMode == PM_RANDOM_POS){
			seqPos[seqIndex] = clampijw(getSeqStart(seqIndex) + getSeqLen(seqIndex), 0, 63);
		} else if(curPlayMode == PM_BWD_LOOP || curPlayMode == PM_BWD_FWD_LOOP){
			seqPos[seqIndex] = getSeqStart(seqIndex);
		}
	}

	int getSeqLen(int seqIndex){
		int inputOffset = int(rescalefjw(inputs[LENGTH_INPUT].getVoltage(seqIndex), 0, 10.0, 0.0, 63.0));
		int len = clampijw(params[LENGTH_KNOB_PARAM].getValue() + inputOffset, 1.0, 64.0);
		return len;
	}

	int getSeqStart(int seqIndex){
		int inputOffset = int(rescalefjw(inputs[START_INPUT].getVoltage(seqIndex), 0, 10.0, 0.0, 63.0));
		int start = clampijw(params[START_PARAM].getValue() + inputOffset, 0.0, 63.0);
		return start;
	}

	int getSeqEnd(int seqIndex){
		int seqEnd = clampijw(getSeqStart(seqIndex) + getSeqLen(seqIndex) - 1, 0, 63);
		return seqEnd;
	}

	int getPlayMode(int seqIndex){
		int modeOffset = int(rescalefjw(inputs[PLAY_MODE_INPUT].getVoltage(seqIndex), 0, 10.0, 0.0, NUM_PLAY_MODES - 1));
		int mode = clampijw(params[PLAY_MODE_KNOB_PARAM].getValue() + modeOffset, 0.0, NUM_PLAY_MODES - 1);
		return mode;
	}

	void clearCells(int seqIndex) {
		for(int i=0;i<CELLS;i++){
			int y = yFromI(i);
			if(y >= seqIndex * 4 && y < (seqIndex + 1) * 4){
				setCellOn(xFromI(i), y, false);
			}
		}
	}

	void randomizeCells(int seqIndex) {
		clearCells(seqIndex);
		float rndAmt = params[RND_AMT_KNOB_PARAM].getValue();
		for(int i=0;i<CELLS;i++){
			int y = yFromI(i);
			if(y >= seqIndex * 4 && y < (seqIndex + 1) * 4){
				setCellOn(xFromI(i), y, random::uniform() < rndAmt);
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

struct TrigsDisplay : LightWidget {
	Trigs *module;
	bool currentlyTurningOn = false;
	Vec dragPos;
	NVGcolor *colors = new NVGcolor[4];
	TrigsDisplay(){
		colors[0] = nvgRGB(255, 151, 9);//orange
		colors[1] = nvgRGB(255, 243, 9);//yellow
		colors[2] = nvgRGB(144, 26, 252);//purple
		colors[3] = nvgRGB(25, 150, 252);//blue
	}

	~TrigsDisplay() {
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

	void drawLayer(const DrawArgs &args, int layer) override {
		//background
		nvgFillColor(args.vg, nvgRGB(0, 0, 0));
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
		nvgFill(args.vg);

		if(layer == 1){
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
			for(int i=0;i<CELLS;i++){
				if(module->cells[i]){
					nvgFillColor(args.vg, colors[i/64]);
					int x = module->xFromI(i);
					int y = module->yFromI(i);
					nvgBeginPath(args.vg);
					nvgRect(args.vg, x * HW, y * HW, HW, HW);
					nvgFill(args.vg);
				}
			}

			nvgStrokeWidth(args.vg, 2);

			for(int i=0;i<4;i++){
				int seqPos = module->resetMode[i] ? module->getSeqStart(i) : module->seqPos[i];
				//seq start line
				float startX = (module->getSeqStart(i)%16) * HW;
				float startY = ((module->getSeqStart(i)/16) + i*4) * HW;
				nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, startX, startY);
				nvgLineTo(args.vg, startX, startY+HW);
				nvgStroke(args.vg);
				
				//seq length line
				float endX = ((module->getSeqEnd(i) + 1)%16) * HW;
				float endY = ((module->getSeqEnd(i)/16) + i*4) * HW;
				nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, endX, endY);
				nvgLineTo(args.vg, endX, endY+HW);
				nvgStroke(args.vg);
				
				//seq position
				int posX = seqPos % 16;
				int posY = (seqPos/16) + i*4;
				nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
				nvgBeginPath(args.vg);
				nvgRect(args.vg, posX * HW, posY * HW, HW, HW);
				nvgStroke(args.vg);
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
				case Trigs::PM_FWD_LOOP:return "→";
				case Trigs::PM_BWD_LOOP:return "←";
				case Trigs::PM_FWD_BWD_LOOP:return "→←";
				case Trigs::PM_BWD_FWD_LOOP:return "←→";
				case Trigs::PM_RANDOM_POS:return "*";
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
				case Trigs::RND_BASIC:return "Basic";
				case Trigs::RND_EUCLID:return "Euclid";
				case Trigs::RND_SIN_WAVE:return "Sine";
				case Trigs::RND_LIFE_GLIDERS:return "Gliders";
			}
		}
		return "";
	}
};

struct TrigsWidget : ModuleWidget { 
	TrigsWidget(Trigs *module); 
	void appendContextMenu(Menu *menu) override;
};

struct NS16ChannelValueItem : MenuItem {
	Trigs *module;
	int channels;
	void onAction(const event::Action &e) override {
		module->channels = channels;
	}
};

struct NS16ChannelItem : MenuItem {
	Trigs *module;
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


TrigsWidget::TrigsWidget(Trigs *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*13, RACK_GRID_HEIGHT);

	setPanel(createPanel(
		asset::plugin(pluginInstance, "res/Trigs.svg"), 
		asset::plugin(pluginInstance, "res/Trigs.svg")
	));

	TrigsDisplay *display = new TrigsDisplay();
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

	addInput(createInput<TinyPJ301MPort>(Vec(7.5, 40), module, Trigs::CLOCK_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(33, 40), module, Trigs::RESET_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(58, 40), module, Trigs::START_INPUT));
	addParam(createParam<JwSmallSnapKnob>(Vec(75, 35), module, Trigs::START_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(103, 40), module, Trigs::LENGTH_INPUT));
	addParam(createParam<JwSmallSnapKnob>(Vec(120, 35), module, Trigs::LENGTH_KNOB_PARAM));
	
	addInput(createInput<TinyPJ301MPort>(Vec(148, 40), module, Trigs::PLAY_MODE_INPUT));
	PlayModeKnob *playModeKnob = dynamic_cast<PlayModeKnob*>(createParam<PlayModeKnob>(Vec(165, 35), module, Trigs::PLAY_MODE_KNOB_PARAM));
	CenteredLabel* const playModeLabel = new CenteredLabel;
	playModeLabel->box.pos = Vec(85.5, 35);
	playModeLabel->text = "";
	playModeKnob->connectLabel(playModeLabel, module);
	addChild(playModeLabel);
	addParam(playModeKnob);

	addParam(createParam<SmallButton>(Vec(33, 282), module, Trigs::CLEAR_BTN_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(5, 330), module, Trigs::RND_TRIG_INPUT));
	addParam(createParam<SmallButton>(Vec(25, 327), module, Trigs::RND_TRIG_BTN_PARAM));
	addParam(createParam<SmallWhiteKnob>(Vec(51, 327), module, Trigs::RND_AMT_KNOB_PARAM));

	addOutput(createOutput<Orange_TinyPJ301MPort>(Vec(95, 285), module, Trigs::GATE_OUTPUT));
	addOutput(createOutput<Yellow_TinyPJ301MPort>(Vec(120, 285), module, Trigs::GATE_OUTPUT+1));
	addOutput(createOutput<Purple_TinyPJ301MPort>(Vec(145, 285), module, Trigs::GATE_OUTPUT+2));
	addOutput(createOutput<Blue_TinyPJ301MPort>(Vec(170, 285), module, Trigs::GATE_OUTPUT+3));

	addOutput(createOutput<TinyPJ301MPort>(Vec(95, 325), module, Trigs::OR_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(120, 325), module, Trigs::XOR_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(145, 325), module, Trigs::NOR_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(170, 325), module, Trigs::EOC_OUTPUT));
}

void TrigsWidget::appendContextMenu(Menu *menu) {
	Trigs *trigs = dynamic_cast<Trigs*>(module);

	// Gate pulse length slider
	{
		MenuLabel *spacerLabelGate = new MenuLabel();
		menu->addChild(spacerLabelGate);
		MenuLabel *gatePulseLabel = new MenuLabel();
		gatePulseLabel->text = "Gate Length";
		menu->addChild(gatePulseLabel);

		GatePulseMsSlider* gateSlider = new GatePulseMsSlider();
		{
			auto qp = static_cast<GatePulseMsQuantity*>(gateSlider->quantity);
			qp->getSeconds = [trigs](){ return trigs->gatePulseLenSec; };
			qp->setSeconds = [trigs](float v){ trigs->gatePulseLenSec = v; };
			qp->defaultSeconds = 0.1f;
			qp->label = "Gate Length";
		}
		gateSlider->box.size.x = 220.0f;
		menu->addChild(gateSlider);
	}
}


Model *modelTrigs = createModel<Trigs, TrigsWidget>("Trigs");
