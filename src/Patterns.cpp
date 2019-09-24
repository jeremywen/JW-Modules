#include <string.h>
#include <algorithm>
#include "JWModules.hpp"

#define ROWS 16
#define COLS 16
#define CELLS 1024
#define POLY 16
#define HW 11.75 //cell height and width

struct ColNotes {
	int *vals = new int[16];
	bool includeInactive;
	bool valid;
	int finalHigh;
	int finalLow;
};

struct Patterns : Module,QuantizeUtils {
	enum ParamIds {
		CLEAR_BTN_PARAM,
		RND_TRIG_BTN_PARAM,
		RND_AMT_KNOB_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		RESET_INPUT,
		RND_TRIG_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		POLY_GATE_OUTPUT,
		OR_MAIN_OUTPUT = POLY_GATE_OUTPUT + POLY,
		XOR_MAIN_OUTPUT = OR_MAIN_OUTPUT + POLY,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};
	enum RndMode { //TODO REMOVE?
		RND_BASIC,
		RND_EUCLID,
		RND_SIN_WAVE,
		RND_LIFE_GLIDERS,
		NUM_RND_MODES
	};

	float displayWidth = 0, displayHeight = 0;
	float rate = 1.0 / APP->engine->getSampleRate();
	int channels = 1;
	bool resetMode = false;
	bool *cells = new bool[CELLS];
	int counters[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	dsp::SchmittTrigger clockTrig, resetTrig, clearTrig;
	dsp::SchmittTrigger rndTrig;
	dsp::PulseGenerator gatePulse;

	Patterns() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(CLEAR_BTN_PARAM, 0.0, 1.0, 0.0);
		configParam(RND_TRIG_BTN_PARAM, 0.0, 1.0, 0.0);
		configParam(RND_AMT_KNOB_PARAM, 0.0, 1.0, 0.1);
		resetSeq();
		resetMode = true;
		clearCells();
	}

	~Patterns() {
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
		gridChanged();
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
				resetSeq();
			}
			gatePulse.trigger(1e-1);
			for(int i=0;i<COLS;i++){
				counters[i]++;
				if(counters[i] > i){
					counters[i] = 0;
				}
			}
		}

		bool pulse = gatePulse.process(1.0 / args.sampleRate);
		for(int i=0;i<CELLS;i++){			
			//TODO make it work like an "XOR" so only fire if one out of the many would fire
			//below works as an "OR" if multiple divisions
			if(cells[i]){
				int x = xFromI(i);//x determines clock division
				int y = yFromI(i);//y determines the row/channel
				if(counters[x] % (x+1) == 0){
					outputs[POLY_GATE_OUTPUT].setVoltage(pulse ? 10 : 0, y);
				}
			}
		}
		outputs[POLY_GATE_OUTPUT].setChannels(channels);
	}

	void gridChanged(){
	}

	void resetSeq(){
		for(int i=0;i<COLS;i++){
			counters[i] = 0;
		}
	}

	void clearCells() {
		for(int i=0;i<CELLS;i++){
			cells[i] = false;
		}
		gridChanged();
	}

	void randomizeCells() {
		clearCells();
		float rndAmt = params[RND_AMT_KNOB_PARAM].getValue();
		for(int i=0;i<CELLS;i++){
			setCellOn(xFromI(i), yFromI(i), random::uniform() < rndAmt);
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

struct PatternsDisplay : Widget {
	Patterns *module;
	bool currentlyTurningOn = false;
	float initX = 0;
	float initY = 0;
	float dragX = 0;
	float dragY = 0;
	PatternsDisplay(){}

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
		nvgFillColor(args.vg, nvgRGB(255, 243, 9));
		for(int i=0;i<CELLS;i++){
			if(module->cells[i]){
				int y = i / ROWS;
				int x = i % COLS;
				nvgBeginPath(args.vg);
				nvgRect(args.vg, x * HW, y * HW, HW, HW);
				nvgFill(args.vg);
			}
		}

		nvgStrokeWidth(args.vg, 2);
	}
};

struct PatternsWidget : ModuleWidget { 
	PatternsWidget(Patterns *module); 
	void appendContextMenu(Menu *menu) override;
};

struct PattChannelValueItem : MenuItem {
	Patterns *module;
	int channels;
	void onAction(const event::Action &e) override {
		module->channels = channels;
	}
};

struct PattChannelItem : MenuItem {
	Patterns *module;
	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		for (int channels = 1; channels <= 16; channels++) {
			PattChannelValueItem *item = new PattChannelValueItem;
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


PatternsWidget::PatternsWidget(Patterns *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*17, RACK_GRID_HEIGHT);

	SVGPanel *panel = new SVGPanel();
	panel->box.size = box.size;
	panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Patterns.svg")));
	addChild(panel);

	PatternsDisplay *display = new PatternsDisplay();
	display->module = module;
	display->box.pos = Vec(3, 75);
	display->box.size = Vec(188, 188);
	addChild(display);
	if(module != NULL){
		module->displayWidth = display->box.size.x;
		module->displayHeight = display->box.size.y;
	}

	addChild(createWidget<Screw_J>(Vec(16, 1)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 1)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	///////////////////////////////////////////////////// LEFT SIDE /////////////////////////////////////////////////////

	addInput(createInput<TinyPJ301MPort>(Vec(22, 40), module, Patterns::CLOCK_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(56, 40), module, Patterns::RESET_INPUT));
	addParam(createParam<SmallButton>(Vec(156, 36), module, Patterns::CLEAR_BTN_PARAM));

	addInput(createInput<TinyPJ301MPort>(Vec(5, 301), module, Patterns::RND_TRIG_INPUT));
	addParam(createParam<SmallButton>(Vec(25, 296), module, Patterns::RND_TRIG_BTN_PARAM));
	addParam(createParam<SmallWhiteKnob>(Vec(51, 295), module, Patterns::RND_AMT_KNOB_PARAM));
	addOutput(createOutput<TinyPJ301MPort>(Vec(55, 350), module, Patterns::POLY_GATE_OUTPUT));

	///////////////////////////////////////////////////// RIGHT SIDE /////////////////////////////////////////////////////

	float outputRowTop = 35.0;
	float outputRowDist = 21.0;
	for(int i=0;i<POLY;i++){
		int paramIdx = POLY - i - 1;
		addOutput(createOutput<TinyPJ301MPort>(Vec(220.081, outputRowTop + i * outputRowDist), module, Patterns::OR_MAIN_OUTPUT + paramIdx)); //param # from bottom up
		// addChild(createLight<SmallLight<MyBlueValueLight>>(Vec(580, (outputRowTop+3) + i * outputRowDist), module, NoteSeq::GATES_LIGHT + paramIdx));
		addOutput(createOutput<TinyPJ301MPort>(Vec(240.858, outputRowTop + i * outputRowDist), module, Patterns::XOR_MAIN_OUTPUT + paramIdx)); //param # from bottom up
	}
}

void PatternsWidget::appendContextMenu(Menu *menu) {
	Patterns *patterns = dynamic_cast<Patterns*>(module);
	MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);

	PattChannelItem *channelItem = new PattChannelItem;
	channelItem->text = "Polyphony channels";
	channelItem->rightText = string::f("%d", patterns->channels) + " " +RIGHT_ARROW;
	channelItem->module = patterns;
	menu->addChild(channelItem);
}


Model *modelPatterns = createModel<Patterns, PatternsWidget>("Patterns");
