#include <string.h>
#include "JWModules.hpp"

#define ROWS 8
#define COLS 4
#define CELLS ROWS * COLS
#define HW 30

struct Pres1t : Module {
	enum ParamIds {
		SAVE_PARAM,
		LOAD_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		BPM_INPUT,
		X_INPUT,
		Y_INPUT,
		SAVE_INPUT,
		LOAD_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		BPM_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

    float smpRate = APP->engine->getSampleRate();
	float *cells = new float[CELLS];
	int selectedCellIdx = 0;
	dsp::SchmittTrigger saveTrigger;
	dsp::SchmittTrigger loadTrigger;

	Pres1t() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(SAVE_PARAM, 0.0, 1.0, 0.0, "");
		configParam(LOAD_PARAM, 0.0, 1.0, 0.0, "");
		onReset();
	}

	~Pres1t() {
		delete [] cells;
	}

	void onRandomize() override {
	}

	void onReset() override {
		clearCells();
	}

	void onSampleRateChange() override {
		smpRate = APP->engine->getSampleRate();
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		json_t *cellsJ = json_array();
		for (int i = 0; i < CELLS; i++) {
			json_t *cellJ = json_real((float) cells[i]);
			json_array_append_new(cellsJ, cellJ);
		}
		json_object_set_new(rootJ, "cells", cellsJ);
		
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *cellsJ = json_object_get(rootJ, "cells");
		if (cellsJ) {
			for (int i = 0; i < CELLS; i++) {
				json_t *cellJ = json_array_get(cellsJ, i);
				if (cellJ)
					cells[i] = json_real_value(cellJ);
			}
		}
	}

	void process(const ProcessArgs &args) override {
		if(inputs[X_INPUT].isConnected()){
			setCellOn(int(rescalefjw(inputs[X_INPUT].getVoltage(), 0, 10, 0, COLS)), yFromI(selectedCellIdx), true);
		}

		if(inputs[Y_INPUT].isConnected()){
			setCellOn(xFromI(selectedCellIdx), int(rescalefjw(inputs[Y_INPUT].getVoltage(), 0, 10, 0, ROWS)), true);
		}

		if(inputs[BPM_INPUT].isConnected() && saveTrigger.process(params[SAVE_PARAM].getValue() + inputs[SAVE_INPUT].getVoltage())){
			cells[selectedCellIdx] = inputs[BPM_INPUT].getVoltage();
		}

		// This loadTrigger won't work for some reason
		// if(outputs[BPM_OUTPUT].isConnected() && loadTrigger.process(params[LOAD_PARAM].getValue() + inputs[LOAD_INPUT].getVoltage()) && cells[selectedCellIdx] != 0){
		if(outputs[BPM_OUTPUT].isConnected() && (params[LOAD_PARAM].getValue() + inputs[LOAD_INPUT].getVoltage() >= 1) && cells[selectedCellIdx] != 0){
			outputs[BPM_OUTPUT].setVoltage(cells[selectedCellIdx]);
		}
	}

	void clearCells() {
		for(int i=0;i<CELLS;i++){
			cells[i] = 0;
		}
	}

	void setCellOnByDisplayPos(float displayX, float displayY, bool on){
		setCellOn(int(displayX / HW), int(displayY / HW), on);
	}

	void setCellOn(int cellX, int cellY, bool on){
		if(cellX >= 0 && cellX < COLS && 
		   cellY >=0 && cellY < ROWS){
			if(on){
				selectedCellIdx = iFromXY(cellX, cellY);
			}
		}
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

struct Pres1tDisplay : Widget {
	Pres1t *module;
	Pres1tDisplay(){}

	void onButton(const event::Button &e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			e.consume(this);
			module->setCellOnByDisplayPos(e.pos.x, e.pos.y, true);
		}
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
			nvgStrokeWidth(args.vg, 1);
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, i * HW, 0);
			nvgLineTo(args.vg, i * HW, HW*ROWS);
			nvgStroke(args.vg);
		}
		for(int i=1;i<ROWS+1;i++){
			nvgStrokeWidth(args.vg, 1);
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, 0, i * HW);
			nvgLineTo(args.vg, box.size.x, i * HW);
			nvgStroke(args.vg);
		}

		if(!module){ return; }

		//cells
		for(int i=0;i<CELLS;i++){
			int y = i / COLS;
			int x = i % COLS;
			if(module->selectedCellIdx == i){
				nvgStrokeColor(args.vg, nvgRGB(25, 150, 252)); //blue
				nvgStrokeWidth(args.vg, 2);
				nvgBeginPath(args.vg);
				nvgRect(args.vg, x * HW, y * HW, HW, HW);
				nvgStroke(args.vg);
			}
			
			if(module->cells[i] != 0){
				nvgFillColor(args.vg, nvgRGB(255, 243, 9)); //yellow
				nvgBeginPath(args.vg);
				nvgRect(args.vg, x * HW+2, y * HW+2, HW-4, HW-4);
				nvgFill(args.vg);
			}
		}

	}
};

struct Pres1tWidget : ModuleWidget {
	Pres1tWidget(Pres1t *module);
};

Pres1tWidget::Pres1tWidget(Pres1t *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*8, RACK_GRID_HEIGHT);

	SVGPanel *panel = new SVGPanel();
	panel->box.size = box.size;
	panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Pres1t.svg")));
	addChild(panel);

	Pres1tDisplay *display = new Pres1tDisplay();
	display->module = module;
	display->box.pos = Vec(0, 15);
	display->box.size = Vec(box.size.x, 250);
	addChild(display);

	addChild(createWidget<Screw_J>(Vec(16, 1)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 1)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	addParam(createParam<SmallButton>(Vec(7, 280), module, Pres1t::SAVE_PARAM));
	addParam(createParam<SmallButton>(Vec(90, 280), module, Pres1t::LOAD_PARAM));

	addInput(createInput<TinyPJ301MPort>(Vec(12, 307), module, Pres1t::SAVE_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(95, 307), module, Pres1t::LOAD_INPUT));

	addInput(createInput<TinyPJ301MPort>(Vec(57, 286), module, Pres1t::X_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(57, 307), module, Pres1t::Y_INPUT));

	addInput(createInput<TinyPJ301MPort>(Vec(18, 340), module, Pres1t::BPM_INPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(85, 340), module, Pres1t::BPM_OUTPUT));
}

Model *modelPres1t = createModel<Pres1t, Pres1tWidget>("Pres1t");