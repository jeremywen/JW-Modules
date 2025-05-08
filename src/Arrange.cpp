#include <string.h>
#include <algorithm>
#include "JWModules.hpp"

#define ROWS 16
#define COLS 64
#define CELLS 1024
#define CELLW 10
#define CELLH 20

struct ColBlocks {
	int *vals = new int[16];
	bool valid;

	~ColBlocks() {
		delete [] vals;
	}
};

struct Arrange : Module {
	enum ParamIds {
		STEP_BTN_PARAM,
		LENGTH_KNOB_PARAM,
		PLAY_MODE_KNOB_PARAM,
		RESET_BTN_PARAM,
		CLEAR_BTN_PARAM,
		RND_MODE_KNOB_PARAM,
		RND_TRIG_BTN_PARAM,
		RND_AMT_KNOB_PARAM,
		START_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		MAIN_INPUT,
		CLOCK_INPUT = MAIN_INPUT + ROWS,
		RESET_INPUT,
		CLEAR_INPUT,
		RND_TRIG_INPUT,
		RND_AMT_INPUT,
		LENGTH_INPUT,
		MODE_INPUT,
		START_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		MAIN_OUTPUT,
		EOC_OUTPUT = MAIN_OUTPUT + ROWS,
		NUM_OUTPUTS
	};
	enum LightIds {
		GATES_LIGHT,
		NUM_LIGHTS = GATES_LIGHT + ROWS,
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
	int seqPos = 0;
	int channels = 1;
	float rndFloat0to1AtClockStep = random::uniform();
	bool goingForward = true;
	bool eocOn = false; 
	bool hitEnd = false;
	bool resetMode = false;
	bool *cells = new bool[CELLS];
	bool *newCells = new bool[CELLS];
	ColBlocks *colBlocksCache = new ColBlocks[COLS];
	ColBlocks *colBlocksCache2 = new ColBlocks[COLS];
	dsp::SchmittTrigger clockTrig, resetTrig, clearTrig;
	dsp::SchmittTrigger rndTrig, shiftUpTrig, shiftDownTrig, shiftChaosTrig;
	dsp::SchmittTrigger rotateRightTrig, rotateLeftTrig, flipHorizTrig, flipVertTrig;
	dsp::PulseGenerator gatePulse, eocPulse;

	enum GateMode { TRIGGER, RETRIGGER, CONTINUOUS };
	GateMode gateMode = TRIGGER;

	Arrange() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(START_PARAM, 0.0, COLS-1, 0.0, "Start");
		configParam(STEP_BTN_PARAM, 0.0, 1.0, 0.0, "Step");
		configParam(LENGTH_KNOB_PARAM, 1.0, COLS, COLS, "Length");
		configParam(PLAY_MODE_KNOB_PARAM, 0.0, NUM_PLAY_MODES - 1, 0.0, "Play Mode");
		configParam(RESET_BTN_PARAM, 0.0, 1.0, 0.0, "Reset");
		configParam(CLEAR_BTN_PARAM, 0.0, 1.0, 0.0, "Clear");
		configParam(RND_MODE_KNOB_PARAM, 0.0, NUM_RND_MODES - 1, 0.0, "Random Mode");
		configParam(RND_TRIG_BTN_PARAM, 0.0, 1.0, 0.0, "Random Trigger");
		configParam(RND_AMT_KNOB_PARAM, 0.0, 1.0, 0.1, "Random Amount");

		configInput(CLOCK_INPUT, "Clock");
		configInput(RESET_INPUT, "Reset");
		configInput(CLEAR_INPUT, "Clear");
		configInput(RND_TRIG_INPUT, "Random Trigger");
		configInput(RND_AMT_INPUT, "Random Amount");
		configInput(LENGTH_INPUT, "Length");
		configInput(MODE_INPUT, "Mode");
		configInput(START_INPUT, "Start");


		for (int i = 0; i < ROWS; i++) {
			configOutput(MAIN_OUTPUT + i, "Output " + std::to_string(i+1));
			configInput(MAIN_INPUT + i, "Input " + std::to_string(i+1));
		}
		configOutput(EOC_OUTPUT, "End of Cycle");

		resetSeq();
		resetMode = true;
		clearCells();
	}

	~Arrange() {
		delete [] cells;
		delete [] newCells;
		delete [] colBlocksCache;
		delete [] colBlocksCache2;
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
		if (clearTrig.process(params[CLEAR_BTN_PARAM].getValue() + inputs[CLEAR_INPUT].getVoltage())) { clearCells(); }
		if (rndTrig.process(params[RND_TRIG_BTN_PARAM].getValue() + inputs[RND_TRIG_INPUT].getVoltage())) { randomizeCells(); }

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

		for(int i=0;i<NUM_INPUTS;i++){
			int channels = inputs[MAIN_INPUT + i].getChannels();
			for (int c = 0; c < channels; c++) {
				outputs[MAIN_OUTPUT + i].setVoltage(isCellOn(seqPos, i) ? inputs[i].getVoltage(c) : 0, c);
			}
			outputs[MAIN_OUTPUT + i].setChannels(channels);
		}

		bool pulse = gatePulse.process(1.0 / args.sampleRate);		
		outputs[EOC_OUTPUT].setVoltage((pulse && eocOn) ? 10.0 : 0.0);
	}

	int * getYValsFromBottomAtSeqPos(){
		int finalHigh = ROWS;
		int finalLow = 1;
		ColBlocks *cache = colBlocksCache2;
		if(cache[seqPos].valid){
			return cache[seqPos].vals;
		}
		
		cache[seqPos].valid = true;
		for(int i=0;i<COLS;i++){ cache[seqPos].vals[i] = -1; }

		int valIdx = 0;
		for(int i=CELLS-1;i>=0;i--){
			int x = xFromI(i);
			if(x == seqPos && (cells[i])){
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
			colBlocksCache[x].valid = false;
			colBlocksCache2[x].valid = false;
		}
	}

	void swapCells() {
		std::swap(cells, newCells);
		gridChanged();

		for(int i=0;i<CELLS;i++){
			newCells[i] = false;
		}
	}

	void clockStep(){
		gatePulse.trigger(1e-1);
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
			seqPos = clampijw(getSeqStart() + getSeqLen(), 0, COLS-1);
		}
	}

	void resetSeqToEnd(){
		int curPlayMode = getPlayMode();
		if(curPlayMode == PM_FWD_LOOP || curPlayMode == PM_FWD_BWD_LOOP || curPlayMode == PM_RANDOM_POS){
			seqPos = clampijw(getSeqStart() + getSeqLen(), 0, COLS-1);
		} else if(curPlayMode == PM_BWD_LOOP || curPlayMode == PM_BWD_FWD_LOOP){
			seqPos = getSeqStart();
		}
	}

	int getSeqStart(){
		int inputOffset = int(rescalefjw(inputs[START_INPUT].getVoltage(), 0, 10.0, 0.0, COLS-1));
		int start = clampijw(params[START_PARAM].getValue() + inputOffset, 0.0, COLS-1);
		return start;
	}

	int getSeqLen(){
		int inputOffset = int(rescalefjw(inputs[LENGTH_INPUT].getVoltage(), 0, 10.0, 0.0, COLS-1));
		int len = clampijw(params[LENGTH_KNOB_PARAM].getValue() + inputOffset, 1.0, COLS);
		return len;
	}

	int getSeqEnd(){
		int seqEnd = clampijw(getSeqStart() + getSeqLen() - 1, 0, COLS-1);
		return seqEnd;
	}

	int getPlayMode(){
		int inputOffset = int(rescalefjw(inputs[MODE_INPUT].getVoltage(), 0, 10.0, 0.0, NUM_PLAY_MODES - 1));
		int mode = clampijw(params[PLAY_MODE_KNOB_PARAM].getValue() + inputOffset, 0.0, NUM_PLAY_MODES - 1);
		return mode;
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

  
	void setCellOnByDisplayPos(float displayX, float displayY, bool on){
		setCellOn(int(displayX / CELLW), int(displayY / CELLH), on);
	}

	void setCellOn(int cellX, int cellY, bool on){
		if(cellX >= 0 && cellX < COLS && 
		   cellY >=0 && cellY < ROWS){
			cells[iFromXY(cellX, cellY)] = on;
			colBlocksCache[cellX].valid = false;
			colBlocksCache2[cellX].valid = false;
		}
	}

	bool isCellOnByDisplayPos(float displayX, float displayY){
		return isCellOn(int(displayX / CELLW), int(displayY / CELLH));
	}

	bool isCellOn(int cellX, int cellY){
		return cells[iFromXY(cellX, cellY)];
	}

	int iFromXY(int cellX, int cellY){
		return cellX + cellY * COLS;
	}

	int xFromI(int cellI){
		return cellI % COLS;
	}

	int yFromI(int cellI){
		return cellI / COLS;
	}
};

struct ArrangeDisplay : LightWidget {
	Arrange *module;
	bool currentlyTurningOn = false;
	Vec dragPos;
	ArrangeDisplay(){}

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
			for(int i=0;i<COLS+1;i++){
				nvgStrokeWidth(args.vg, (i % 4 == 0) ? 2 : 1);
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, i * CELLW, 0);
				nvgLineTo(args.vg, i * CELLW, box.size.y);
				nvgStroke(args.vg);
			}
			for(int i=0;i<ROWS+1;i++){
				nvgStrokeWidth(args.vg, (i % 4 == 0) ? 2 : 1);
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, 0, i * CELLH);
				nvgLineTo(args.vg, box.size.x, i * CELLH);
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
					nvgRect(args.vg, x * CELLW, y * CELLH, CELLW, CELLH);
					nvgFill(args.vg);
				}
			}

			nvgStrokeWidth(args.vg, 2);

			//seq start line
			float startX = module->getSeqStart() * CELLW;
			nvgStrokeColor(args.vg, nvgRGB(25, 150, 252));//blue
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, startX, 0);
			nvgLineTo(args.vg, startX, box.size.y);
			nvgStroke(args.vg);

			//seq length line
			float endX = (module->getSeqEnd() + 1) * CELLW;
			nvgStrokeColor(args.vg, nvgRGB(144, 26, 252));//purple
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, endX, 0);
			nvgLineTo(args.vg, endX, box.size.y);
			nvgStroke(args.vg);

			//seq pos
			int pos = module->resetMode ? module->getSeqStart() : module->seqPos;
			nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
			nvgBeginPath(args.vg);
			nvgRect(args.vg, pos * CELLW, 0, CELLW, box.size.y);
			nvgStroke(args.vg);
		}
		Widget::drawLayer(args, layer);
	}
};

struct PlayModeKnob : JwSmallSnapKnob {
	PlayModeKnob(){}
	std::string formatCurrentValue() override {
		if(getParamQuantity() != NULL){
			switch(int(getParamQuantity()->getDisplayValue())){
				case Arrange::PM_FWD_LOOP:return "→";
				case Arrange::PM_BWD_LOOP:return "←";
				case Arrange::PM_FWD_BWD_LOOP:return "→←";
				case Arrange::PM_BWD_FWD_LOOP:return "←→";
				case Arrange::PM_RANDOM_POS:return "*";
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
				case Arrange::RND_BASIC:return "Basic";
				case Arrange::RND_EUCLID:return "Euclid";
				case Arrange::RND_SIN_WAVE:return "Sine";
				case Arrange::RND_MIRROR_X:return "Mirror X";
				case Arrange::RND_MIRROR_Y:return "Mirror Y";
			}
		}
		return "";
	}
};

struct ArrangeWidget : ModuleWidget { 
	ArrangeWidget(Arrange *module); 
	void appendContextMenu(Menu *menu) override;
};

ArrangeWidget::ArrangeWidget(Arrange *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*48, RACK_GRID_HEIGHT);

	setPanel(createPanel(
		asset::plugin(pluginInstance, "res/Arrange.svg"), 
		asset::plugin(pluginInstance, "res/dark/Arrange.svg")
	));

	ArrangeDisplay *display = new ArrangeDisplay();
	display->module = module;
	display->box.pos = Vec(40, 40);
	display->box.size = Vec(COLS*CELLW, ROWS*CELLH);
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
	float topIn = 25.0;
	float topKnob = 20.0;

	//row 1
	addInput(createInput<TinyPJ301MPort>(Vec(47.5, topIn), module, Arrange::CLOCK_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(73, topIn), module, Arrange::START_INPUT));
	addParam(createParam<JwSmallSnapKnob>(Vec(89, topKnob), module, Arrange::START_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(120, 20), module, Arrange::LENGTH_INPUT));
	addParam(createParam<JwSmallSnapKnob>(Vec(136, topKnob), module, Arrange::LENGTH_KNOB_PARAM));

	addInput(createInput<TinyPJ301MPort>(Vec(168, 20), module, Arrange::MODE_INPUT));
	PlayModeKnob *playModeKnob = dynamic_cast<PlayModeKnob*>(createParam<PlayModeKnob>(Vec(184, topKnob), module, Arrange::PLAY_MODE_KNOB_PARAM));
	CenteredLabel* const playModeLabel = new CenteredLabel;
	playModeLabel->box.pos = Vec(90, 25);
	playModeLabel->text = "";
	playModeKnob->connectLabel(playModeLabel, module);
	addChild(playModeLabel);
	addParam(playModeKnob);

	addInput(createInput<TinyPJ301MPort>(Vec(240, topIn), module, Arrange::RESET_INPUT));
	addParam(createParam<SmallButton>(Vec(260, topKnob), module, Arrange::RESET_BTN_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(290, topIn), module, Arrange::CLEAR_INPUT));
	addParam(createParam<SmallButton>(Vec(310, topKnob), module, Arrange::CLEAR_BTN_PARAM));

	RndModeKnob *rndModeKnob = dynamic_cast<RndModeKnob*>(createParam<RndModeKnob>(Vec(500, topKnob), module, Arrange::RND_MODE_KNOB_PARAM));
	CenteredLabel* const rndModeLabel = new CenteredLabel;
	rndModeLabel->box.pos = Vec(200, 25);
	rndModeLabel->text = "";
	rndModeKnob->connectLabel(rndModeLabel, module);
	addChild(rndModeLabel);
	addParam(rndModeKnob);

	addInput(createInput<TinyPJ301MPort>(Vec(300, topIn), module, Arrange::RND_TRIG_INPUT));
	addParam(createParam<SmallButton>(Vec(320, topKnob), module, Arrange::RND_TRIG_BTN_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(358, topIn), module, Arrange::RND_AMT_INPUT));
	addParam(createParam<SmallWhiteKnob>(Vec(378, topKnob), module, Arrange::RND_AMT_KNOB_PARAM));

	///////////////////////////////////////////////////// RIGHT SIDE /////////////////////////////////////////////////////

	float outputRowTop = 41.0;
	float outputRowDist = 20.0;
	for(int i=0;i<ROWS;i++){
		addInput(createInput<TinyPJ301MPort>(Vec(5, outputRowTop + i * outputRowDist), module, Arrange::MAIN_INPUT + i));
		addOutput(createOutput<TinyPJ301MPort>(Vec(690, outputRowTop + i * outputRowDist), module, Arrange::MAIN_OUTPUT + i));
		addChild(createLight<SmallLight<MyBlueValueLight>>(Vec(710, (outputRowTop+3) + i * outputRowDist), module, Arrange::GATES_LIGHT + i));
	}

	addOutput(createOutput<TinyPJ301MPort>(Vec(623, topIn), module, Arrange::EOC_OUTPUT));
}

void ArrangeWidget::appendContextMenu(Menu *menu) {
}

Model *modelArrange = createModel<Arrange, ArrangeWidget>("Arrange");
