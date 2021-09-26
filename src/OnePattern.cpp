#include <string.h>
#include <algorithm>
#include "JWModules.hpp"

#define P_ROWS 16
#define P_COLS 1
#define P_CELLS 16
#define P_POLY 1
#define P_HW 11.75 //cell height and width

struct OnePattern : Module {
	enum ParamIds {
		CLEAR_BTN_PARAM,
		RND_TRIG_BTN_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		RESET_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OR_MAIN_OUTPUT,
		XOR_MAIN_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	float displayWidth = 0, displayHeight = 0;
	int channels = 1;
	bool resetMode = false;
	bool *cells = new bool[P_CELLS];
	int counters[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	dsp::SchmittTrigger clockTrig, resetTrig, clearTrig;
	dsp::SchmittTrigger rndTrig, rotateTrig, shiftTrig;

	OnePattern() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(CLEAR_BTN_PARAM, 0.0, 1.0, 0.0, "Clear");
		configParam(RND_TRIG_BTN_PARAM, 0.0, 1.0, 0.0, "Random");
		resetSeq();
		resetMode = true;
		clearCells();
	}

	~OnePattern() {
		delete [] cells;
	}

	void onRandomize() override {
	}

	void onReset() override {
		resetSeq();
		resetMode = true;
		clearCells();
	}

	void onSampleRateChange() override {
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		json_t *cellsJ = json_array();
		for (int i = 0; i < P_CELLS; i++) {
			json_t *cellJ = json_integer((int) cells[i]);
			json_array_append_new(cellsJ, cellJ);
		}
		json_object_set_new(rootJ, "cells", cellsJ);
		
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *cellsJ = json_object_get(rootJ, "cells");
		if (cellsJ) {
			for (int i = 0; i < P_CELLS; i++) {
				json_t *cellJ = json_array_get(cellsJ, i);
				if (cellJ)
					cells[i] = json_integer_value(cellJ);
			}
		}
	}

	void process(const ProcessArgs &args) override {
		if (clearTrig.process(params[CLEAR_BTN_PARAM].getValue())) { clearCells(); }
		if (rndTrig.process(params[RND_TRIG_BTN_PARAM].getValue())) { randomizeCells(); }
		if (resetTrig.process(inputs[RESET_INPUT].getVoltage())) { resetMode = true; }
		if (clockTrig.process(inputs[CLOCK_INPUT].getVoltage())) {
			if(resetMode){
				resetMode = false;
				resetSeq();
			}
			for(int i=0;i<P_ROWS;i++){
				counters[i]++;
				if(counters[i] > i){
					counters[i] = 0;
				}
			}
		}

		int firingInCol = 0;
		for(int i=0;i<P_CELLS;i++){			
			float voltage = inputs[CLOCK_INPUT].getVoltage();
			if(cells[i]){
				//below works as an "OR" if multiple divisions
				if(counters[i] % (i+1) == 0){
					outputs[OR_MAIN_OUTPUT].setVoltage(voltage);
					firingInCol++;
				}
			}
			if(i == P_ROWS - 1){//end of col
				//works like an "XOR" so only fire if one out of the many would fire
				if(firingInCol == 1){
					outputs[XOR_MAIN_OUTPUT].setVoltage(voltage);
				}
				firingInCol = 0;
			}
		}
	}

	void resetSeq(){
		for(int i=0;i<P_ROWS;i++){
			counters[i] = 0;
		}
	}

	void randomizeCells() {
		clearCells();
		float rndAmt = 0.25;
		for(int i=0;i<P_CELLS;i++){
			setCellOn(i, random::uniform() < rndAmt);
		}
	}
  
	void clearCells() {
		for(int i=0;i<P_CELLS;i++){
			cells[i] = false;
		}
	}

	void setCellOnByDisplayPos(float displayY, bool on){
		setCellOn(int(displayY / P_HW), on);
	}

	void setCellOn(int cellY, bool on){
		if(cellY >=0 && cellY < P_ROWS){
			cells[cellY] = on;
		}
	}

	bool isCellOnByDisplayPos(float displayY){
		return isCellOn(int(displayY / P_HW));
	}

	bool isCellOn(int cellY){
		return cells[cellY];
	}

};

struct OnePatternDisplay : LightWidget {
	OnePattern *module;
	bool currentlyTurningOn = false;
	float initX = 0;
	float initY = 0;
	float dragX = 0;
	float dragY = 0;
	OnePatternDisplay(){}

	void onButton(const event::Button &e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			e.consume(this);
			// e.target = this;
			initX = e.pos.x;
			initY = e.pos.y;
			currentlyTurningOn = !module->isCellOnByDisplayPos(e.pos.y);
			module->setCellOnByDisplayPos(e.pos.y, currentlyTurningOn);
		}
	}
	
	void onDragStart(const event::DragStart &e) override {
		dragX = APP->scene->mousePos.x;
		dragY = APP->scene->mousePos.y;
	}

	void onDragMove(const event::DragMove &e) override {
		float newDragX = APP->scene->mousePos.x;
		float newDragY = APP->scene->mousePos.y;
		module->setCellOnByDisplayPos(initY+(newDragY-dragY), currentlyTurningOn);
	}

	void draw(const DrawArgs &args) override {
		//background
		nvgFillColor(args.vg, nvgRGB(0, 0, 0));
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
		nvgFill(args.vg);

		//grid
		nvgStrokeColor(args.vg, nvgRGB(60, 70, 73)); //gray
		for(int i=1;i<P_COLS;i++){
			nvgStrokeWidth(args.vg, (i % 4 == 0) ? 2 : 1);
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, i * P_HW, 0);
			nvgLineTo(args.vg, i * P_HW, box.size.y);
			nvgStroke(args.vg);
		}
		for(int i=1;i<P_ROWS;i++){
			nvgStrokeWidth(args.vg, (i % 4 == 0) ? 2 : 1);
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, 0, i * P_HW);
			nvgLineTo(args.vg, box.size.x, i * P_HW);
			nvgStroke(args.vg);
		}

		if(module == NULL) return;

		//cells
		nvgFillColor(args.vg, nvgRGB(255, 243, 9));
		for(int i=0;i<P_CELLS;i++){
			if(module->cells[i]){
				int x = 0;
				int y = i;
				nvgBeginPath(args.vg);
				nvgRect(args.vg, x * P_HW, y * P_HW, box.size.x, P_HW);
				nvgFill(args.vg);
			}
		}

		nvgStrokeWidth(args.vg, 2);
	}
};

struct OnePatternWidget : ModuleWidget { 
	OnePatternWidget(OnePattern *module); 
	void appendContextMenu(Menu *menu) override;
};

OnePatternWidget::OnePatternWidget(OnePattern *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*3, RACK_GRID_HEIGHT);

	SVGPanel *panel = new SVGPanel();
	panel->box.size = box.size;
	panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/OnePattern.svg")));
	addChild(panel);

	OnePatternDisplay *display = new OnePatternDisplay();
	display->module = module;
	display->box.pos = Vec(7, 120);
	display->box.size = Vec(30, 188);
	addChild(display);
	if(module != NULL){
		module->displayWidth = display->box.size.x;
		module->displayHeight = display->box.size.y;
	}

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	addInput(createInput<TinyPJ301MPort>(Vec(15, 46), module, OnePattern::CLOCK_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(15, 75), module, OnePattern::RESET_INPUT));
	addParam(createParam<TinyButton>(Vec(5, 102), module, OnePattern::CLEAR_BTN_PARAM));
	addParam(createParam<TinyButton>(Vec(25, 102), module, OnePattern::RND_TRIG_BTN_PARAM));

	addOutput(createOutput<TinyPJ301MPort>(Vec(4, 325), module, OnePattern::OR_MAIN_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(26.5, 325), module, OnePattern::XOR_MAIN_OUTPUT));
}

void OnePatternWidget::appendContextMenu(Menu *menu) {

}


Model *modelOnePattern = createModel<OnePattern, OnePatternWidget>("1Pattern");
