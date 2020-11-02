#include <string.h>
#include <algorithm>
#include "JWModules.hpp"

#define P_ROWS 16
#define P_COLS 16
#define P_CELLS 256
#define P_POLY 16
#define P_HW 11.75 //cell height and width

struct Patterns : Module {
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
		ROTATE_INPUT,
		SHIFT_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OR_MAIN_OUTPUT,
		XOR_MAIN_OUTPUT = OR_MAIN_OUTPUT + 16,
		POLY_OR_OUTPUT = XOR_MAIN_OUTPUT + 16,
		POLY_XOR_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
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
	int channels = 1;
	bool resetMode = false;
	bool *cells = new bool[P_CELLS];
	bool *newCells = new bool[P_CELLS];
	int counters[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int currentY = 0;
	dsp::SchmittTrigger clockTrig, resetTrig, clearTrig;
	dsp::SchmittTrigger rndTrig, rotateTrig, shiftTrig;

	Patterns() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(CLEAR_BTN_PARAM, 0.0, 1.0, 0.0, "Clear");
		configParam(RND_TRIG_BTN_PARAM, 0.0, 1.0, 0.0, "Random Trigger");
		configParam(RND_AMT_KNOB_PARAM, 0.0, 1.0, 0.1, "Random Amount");
		resetSeq();
		resetMode = true;
		clearCells();
	}

	~Patterns() {
		delete [] cells;
		delete [] newCells;
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
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "channels", json_integer(channels));
		
		json_t *cellsJ = json_array();
		for (int i = 0; i < P_CELLS; i++) {
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
			for (int i = 0; i < P_CELLS; i++) {
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
		if (resetTrig.process(inputs[RESET_INPUT].getVoltage())) { resetMode = true; }
		if (rotateTrig.process(inputs[ROTATE_INPUT].getVoltage())) { rotateCells(DIR_RIGHT); }
		if (shiftTrig.process(inputs[SHIFT_INPUT].getVoltage())) { shiftCells(DIR_UP); }

		if (clockTrig.process(inputs[CLOCK_INPUT].getVoltage())) {
			if(resetMode){
				resetMode = false;
				resetSeq();
			}
			for(int i=0;i<P_COLS;i++){
				counters[i]++;
				if(counters[i] > i){
					counters[i] = 0;
				}
			}
		}

		int firingInRow = 0;
		for(int i=0;i<P_CELLS;i++){			
			int y = yFromI(i);//y determines the row/channel
			if(y == currentY){
				int x = xFromI(i);//x determines clock division
				int yInv = P_POLY - y - 1;
				float voltage = inputs[CLOCK_INPUT].getVoltage();
				if(cells[i]){
					//below works as an "OR" if multiple divisions
					if(counters[x] % (x+1) == 0){
						outputs[OR_MAIN_OUTPUT + yInv].setVoltage(voltage);
						outputs[POLY_OR_OUTPUT].setVoltage(voltage, yInv);
						firingInRow++;
					}
				}
				if(x == P_COLS - 1){//end of row
					//works like an "XOR" so only fire if one out of the many would fire
					if(firingInRow == 1){
						outputs[XOR_MAIN_OUTPUT + yInv].setVoltage(voltage);
						outputs[POLY_XOR_OUTPUT].setVoltage(voltage, yInv);
					}
					firingInRow = 0;
				}
			}
		}
		currentY++;
		if(currentY == P_ROWS){
			currentY = 0;
		}
		outputs[POLY_OR_OUTPUT].setChannels(channels);
		outputs[POLY_XOR_OUTPUT].setChannels(channels);
	}

	void gridChanged(){
	}

	void swapCells() {
		std::swap(cells, newCells);
		gridChanged();

		for(int i=0;i<P_CELLS;i++){
			newCells[i] = false;
		}
	}

	void resetSeq(){
		for(int i=0;i<P_COLS;i++){
			counters[i] = 0;
		}
	}

	void clearCells() {
		for(int i=0;i<P_CELLS;i++){
			cells[i] = false;
		}
		gridChanged();
	}

	void randomizeCells() {
		clearCells();
		float rndAmt = params[RND_AMT_KNOB_PARAM].getValue();
		for(int i=0;i<P_CELLS;i++){
			setCellOn(xFromI(i), yFromI(i), random::uniform() < rndAmt);
		}
	}
  
	void rotateCells(RotateDirection dir){
		for(int x=0; x < P_COLS; x++){
			for(int y=0; y < P_ROWS; y++){
				switch(dir){
					case DIR_RIGHT:
						newCells[iFromXY(x, y)] = cells[iFromXY(y, P_COLS - x - 1)];
						break;
					case DIR_LEFT:
						newCells[iFromXY(x, y)] = cells[iFromXY(P_COLS - y - 1, x)];
						break;
				}

			}
		}
		swapCells();
	}

	void flipCells(FlipDirection dir){
		for(int x=0; x < P_COLS; x++){
			for(int y=0; y < P_ROWS; y++){
				switch(dir){
					case DIR_HORIZ:
						newCells[iFromXY(x, y)] = cells[iFromXY(P_COLS - 1 - x, y)];
						break;
					case DIR_VERT:
						newCells[iFromXY(x, y)] = cells[iFromXY(x, P_ROWS - 1 - y)];
						break;
				}

			}
		}
		swapCells();
	}

	void shiftCells(ShiftDirection dir){
		for(int x=0; x < P_COLS; x++){
			for(int y=0; y < P_ROWS; y++){
				switch(dir){
					case DIR_UP:
						newCells[iFromXY(x, y)] = cells[iFromXY(x, y==P_ROWS-1 ? 0 : y + 1)];
						break;
					case DIR_DOWN:
						newCells[iFromXY(x, y)] = cells[iFromXY(x, y==0 ? P_ROWS-1 : y - 1)];
						break;
				}

			}
		}
		swapCells();
	}
	void setCellOnByDisplayPos(float displayX, float displayY, bool on){
		setCellOn(int(displayX / P_HW), int(displayY / P_HW), on);
	}

	void setCellOn(int cellX, int cellY, bool on){
		if(cellX >= 0 && cellX < P_COLS && 
		   cellY >=0 && cellY < P_ROWS){
			cells[iFromXY(cellX, cellY)] = on;
		}
	}

	bool isCellOnByDisplayPos(float displayX, float displayY){
		return isCellOn(int(displayX / P_HW), int(displayY / P_HW));
	}

	bool isCellOn(int cellX, int cellY){
		return cells[iFromXY(cellX, cellY)];
	}

	int iFromXY(int cellX, int cellY){
		return cellX + cellY * P_ROWS;
	}

	int xFromI(int cellI){
		return cellI % P_COLS;
	}

	int yFromI(int cellI){
		return cellI / P_COLS;
	}
};

struct PatternsDisplay : LightWidget {
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
				int x = module->xFromI(i);
				int y = module->yFromI(i);
				nvgBeginPath(args.vg);
				nvgRect(args.vg, x * P_HW, y * P_HW, P_HW, P_HW);
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
	box.size = Vec(RACK_GRID_WIDTH*16, RACK_GRID_HEIGHT);

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

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	///////////////////////////////////////////////////// LEFT SIDE /////////////////////////////////////////////////////

	addInput(createInput<TinyPJ301MPort>(Vec(15, 40), module, Patterns::CLOCK_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(46, 40), module, Patterns::RESET_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(85, 40), module, Patterns::ROTATE_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(122, 40), module, Patterns::SHIFT_INPUT));
	addParam(createParam<SmallButton>(Vec(156, 36), module, Patterns::CLEAR_BTN_PARAM));

	addInput(createInput<TinyPJ301MPort>(Vec(5, 301), module, Patterns::RND_TRIG_INPUT));
	addParam(createParam<SmallButton>(Vec(25, 296), module, Patterns::RND_TRIG_BTN_PARAM));
	addParam(createParam<SmallWhiteKnob>(Vec(51, 295), module, Patterns::RND_AMT_KNOB_PARAM));
	
	addOutput(createOutput<Blue_TinyPJ301MPort>(Vec(116, 315), module, Patterns::POLY_OR_OUTPUT));
	addOutput(createOutput<Blue_TinyPJ301MPort>(Vec(151, 315), module, Patterns::POLY_XOR_OUTPUT));

	///////////////////////////////////////////////////// RIGHT SIDE /////////////////////////////////////////////////////

	float outputRowTop = 35.0;
	float outputRowDist = 20.0;
	for(int i=0;i<P_POLY;i++){
		int paramIdx = P_POLY - i - 1;
		addOutput(createOutput<TinyPJ301MPort>(Vec(200, outputRowTop + i * outputRowDist), module, Patterns::OR_MAIN_OUTPUT + paramIdx)); //param # from bottom up
		addOutput(createOutput<TinyPJ301MPort>(Vec(220, outputRowTop + i * outputRowDist), module, Patterns::XOR_MAIN_OUTPUT + paramIdx)); //param # from bottom up
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
