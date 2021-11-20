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
	int seqPos = 0;//4 rows of 16
	int channels = 1;
	float rndFloat0to1AtClockStep = random::uniform();
	bool goingForward = true;
	bool resetMode = false;
	bool eocOn = false; 
	bool hitEnd = false;
	bool *cells = new bool[CELLS];
	dsp::SchmittTrigger clockTrig, resetTrig, clearTrig;
	dsp::SchmittTrigger rndTrig, shiftUpTrig, shiftDownTrig;
	dsp::SchmittTrigger rotateRightTrig, rotateLeftTrig, flipHorizTrig, flipVertTrig;
	dsp::PulseGenerator gatePulse;

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

		const char *colors[4] = { "Orange", "Yellow", "Purple", "Blue" };
		for(int i=0; i<4; i++){
			configOutput(GATE_OUTPUT + i, "Gate " + std::string(colors[i]));
		}
		configOutput(EOC_OUTPUT, "End of Cycle");
		configOutput(OR_OUTPUT, "OR");
		configOutput(XOR_OUTPUT, "XOR");
		configOutput(NOR_OUTPUT, "NOR");
		resetSeq();
		resetMode = true;
		clearCells();
	}

	~Trigs() {
		delete [] cells;
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
	}

	void process(const ProcessArgs &args) override {
		if (clearTrig.process(params[CLEAR_BTN_PARAM].getValue())) { clearCells(); }
		if (rndTrig.process(params[RND_TRIG_BTN_PARAM].getValue() + inputs[RND_TRIG_INPUT].getVoltage())) { randomizeCells(); }
		if (resetTrig.process(inputs[RESET_INPUT].getVoltage())) {
			resetMode = true;
		}
		if (clockTrig.process(inputs[CLOCK_INPUT].getVoltage())) {
			if(resetMode){
				resetMode = false;
				hitEnd = false;
				resetSeqToEnd();
			}
			clockStep();
		}

		bool pulse = gatePulse.process(1.0 / args.sampleRate);
		int seqPosX = xFromSeqPos();
		int trigCount = 0;
		for(int i=0;i<4;i++){
			bool cellActive = cells[iFromXY(seqPosX, (seqPos/16) + i*4)];
			float gateVolts = (pulse && cellActive) ? 10.0 : 0.0;
			trigCount += (pulse && cellActive);
			outputs[GATE_OUTPUT + i].setVoltage(gateVolts);
		}
		outputs[OR_OUTPUT].setVoltage((pulse && trigCount>0) ? 10.0 : 0.0);
		outputs[NOR_OUTPUT].setVoltage((pulse && trigCount==0) ? 10.0 : 0.0);
		outputs[XOR_OUTPUT].setVoltage((pulse && trigCount==1) ? 10.0 : 0.0);
		outputs[EOC_OUTPUT].setVoltage((pulse && eocOn) ? 10.0 : 0.0);
	}

	int xFromSeqPos(){
		return seqPos % 16;
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

	void clockStep(){
		gatePulse.trigger(1e-3);
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
			seqPos = clampijw(getSeqStart() + getSeqLen(), 0, 63);
		}
	}

	void resetSeqToEnd(){
		int curPlayMode = getPlayMode();
		if(curPlayMode == PM_FWD_LOOP || curPlayMode == PM_FWD_BWD_LOOP || curPlayMode == PM_RANDOM_POS){
			seqPos = clampijw(getSeqStart() + getSeqLen(), 0, 63);
		} else if(curPlayMode == PM_BWD_LOOP || curPlayMode == PM_BWD_FWD_LOOP){
			seqPos = getSeqStart();
		}
	}

	int getSeqLen(){
		int inputOffset = int(rescalefjw(inputs[LENGTH_INPUT].getVoltage(), 0, 10.0, 0.0, 63.0));
		int len = clampijw(params[LENGTH_KNOB_PARAM].getValue() + inputOffset, 1.0, 64.0);
		return len;
	}

	int getSeqStart(){
		int inputOffset = int(rescalefjw(inputs[START_INPUT].getVoltage(), 0, 10.0, 0.0, 63.0));
		int start = clampijw(params[START_PARAM].getValue() + inputOffset, 0.0, 63.0);
		return start;
	}

	int getSeqEnd(){
		int seqEnd = clampijw(getSeqStart() + getSeqLen() - 1, 0, 63);
		return seqEnd;
	}

	int getPlayMode(){
		int mode = clampijw(params[PLAY_MODE_KNOB_PARAM].getValue(), 0.0, NUM_PLAY_MODES - 1);
		return mode;
	}

	void clearCells() {
		for(int i=0;i<CELLS;i++){
			cells[i] = false;
		}
	}

	void randomizeCells() {
		clearCells();
		float rndAmt = params[RND_AMT_KNOB_PARAM].getValue();
		switch(0){
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
	float initX = 0;
	float initY = 0;
	float dragX = 0;
	float dragY = 0;
	NVGcolor *colors = new NVGcolor[4];
	TrigsDisplay(){
		colors[0] = nvgRGB(255, 151, 9);//orange
		colors[1] = nvgRGB(255, 243, 9);//yellow
		colors[2] = nvgRGB(144, 26, 252);//purple
		colors[3] = nvgRGB(25, 150, 252);//blue
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
		dragX = APP->scene->mousePos.x;
		dragY = APP->scene->mousePos.y;
	}

	void onDragMove(const event::DragMove &e) override {
		float newDragX = APP->scene->mousePos.x;
		float newDragY = APP->scene->mousePos.y;
		module->setCellOnByDisplayPos(initX+(newDragX-dragX), initY+(newDragY-dragY), currentlyTurningOn);
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

			//seq pos
			int pos = module->resetMode ? module->getSeqStart() : module->seqPos;
			for(int i=0;i<4;i++){
			//seq start line
			float startX = (module->getSeqStart()%16) * HW;
			float startY = ((module->getSeqStart()/16) + i*4) * HW;
			nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, startX, startY);
			nvgLineTo(args.vg, startX, startY+HW);
			nvgStroke(args.vg);

			//seq length line
			float endX = ((module->getSeqEnd() + 1)%16) * HW;
			float endY = ((module->getSeqEnd()/16) + i*4) * HW;
			nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, endX, endY);
			nvgLineTo(args.vg, endX, endY+HW);
			nvgStroke(args.vg);

			//seq position
			int posX = pos % 16;
			int posY = (pos/16) + i*4;
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

	SVGPanel *panel = new SVGPanel();
	panel->box.size = box.size;
	panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Trigs.svg")));
	addChild(panel);

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
	addInput(createInput<TinyPJ301MPort>(Vec(108, 40), module, Trigs::LENGTH_INPUT));
	addParam(createParam<JwSmallSnapKnob>(Vec(125, 35), module, Trigs::LENGTH_KNOB_PARAM));
	
	PlayModeKnob *playModeKnob = dynamic_cast<PlayModeKnob*>(createParam<PlayModeKnob>(Vec(158, 35), module, Trigs::PLAY_MODE_KNOB_PARAM));
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
}


Model *modelTrigs = createModel<Trigs, TrigsWidget>("Trigs");
