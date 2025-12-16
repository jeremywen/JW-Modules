#include <string.h>
#include <algorithm>
#include "JWModules.hpp"

#define ARRANGE_ROWS 16
#define ARRANGE_COLS 64
#define CELLS 1024
#define CELLW 10
#define CELLH 20

struct Arrange : Module {
	enum ParamIds {
		STEP_BTN_PARAM,//not used
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
		CLOCK_INPUT = MAIN_INPUT + ARRANGE_ROWS,
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
		EOC_OUTPUT = MAIN_OUTPUT + ARRANGE_ROWS,
		POS_OUTPUT,
		INTENSITY_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS,
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

	float displayWidth = 0, displayHeight = 0;
	int seqPos = 0;
	float rndFloat0to1AtClockStep = random::uniform();
	bool goingForward = true;
	bool eocOn = false; 
	bool hitEnd = false;
	bool resetMode = false;
	bool absolutePosMode = false;
	bool defaultToTenVoltsWhenNoInput = false;
	bool *dirtyNames = new bool[ARRANGE_ROWS];
	bool *cells = new bool[CELLS];
	dsp::SchmittTrigger clockTrig, resetTrig, clearTrig, rndTrig;
	dsp::PulseGenerator eocPulse, gatePulse;
	std::string rowNames[16] = {	
		"", "", "", "",
		"", "", "", "",
		"", "", "", "",
		"", "", "", ""
	};
	NVGcolor colors[4] = {
		nvgRGB(255, 151, 9), //orange
		nvgRGB(255, 243, 9), //yellow
		nvgRGB(144, 26, 252), //purple
		nvgRGB(25, 150, 252)  //blue	
	};

	Arrange() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(START_PARAM, 0.0, ARRANGE_COLS-1, 0.0, "Start");
		configParam(LENGTH_KNOB_PARAM, 1.0, ARRANGE_COLS, ARRANGE_COLS, "Length");
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
		configInput(MODE_INPUT, "Play Mode");
		configInput(START_INPUT, "Start");

		for (int i = 0; i < ARRANGE_ROWS; i++) {
			configOutput(MAIN_OUTPUT + i, "Output " + std::to_string(i+1));
			configInput(MAIN_INPUT + i, "Input " + std::to_string(i+1));
		}
		configOutput(EOC_OUTPUT, "End of Cycle");
		configOutput(POS_OUTPUT, "Position");
		configOutput(INTENSITY_OUTPUT, "Intensity");

		resetSeq();
		resetMode = true;
		clearCells();
	}

	~Arrange() {
		if (cells) {
			delete[] cells;
			cells = nullptr; // Avoid dangling pointer
		}
		if (dirtyNames) {
			delete[] dirtyNames;
			dirtyNames = nullptr; // Avoid dangling pointer
		}
	}

	void onRandomize() override {
		randomizeCells();
	}
	
	void onReset() override {
		absolutePosMode = false;
		defaultToTenVoltsWhenNoInput = false;
		resetSeq();
		resetMode = true;
		clearCells();
		for (int i = 0; i < ARRANGE_ROWS; ++i) {
			rowNames[i].clear();
		}
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		json_object_set_new(rootJ, "absolutePosMode", json_boolean(absolutePosMode));
		json_object_set_new(rootJ, "defaultToTenVoltsWhenNoInput", json_boolean(defaultToTenVoltsWhenNoInput));
		json_t *cellsJ = json_array();
		for (int i = 0; i < CELLS; i++) {
			json_t *cellJ = json_integer((int) cells[i]);
			json_array_append_new(cellsJ, cellJ);
		}
		json_object_set_new(rootJ, "cells", cellsJ);

		json_t *rowNamesJ = json_array();
		for (int i = 0; i < ARRANGE_ROWS; i++) {
			json_array_append_new(rowNamesJ, json_stringn(rowNames[i].c_str(), rowNames[i].size()));
		}
		json_object_set_new(rootJ, "rowNames", rowNamesJ);
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *absolutePosModeJ = json_object_get(rootJ, "absolutePosMode");
		if (absolutePosModeJ) {
			absolutePosMode = json_is_true(absolutePosModeJ);
		}

		json_t *defaultToTenVoltsWhenNoInputJ = json_object_get(rootJ, "defaultToTenVoltsWhenNoInput");
		if (defaultToTenVoltsWhenNoInputJ) {
			defaultToTenVoltsWhenNoInput = json_is_true(defaultToTenVoltsWhenNoInputJ);
		}

		json_t *cellsJ = json_object_get(rootJ, "cells");
		if (cellsJ && json_array_size(cellsJ) == CELLS) {
			for (int i = 0; i < CELLS; i++) {
				json_t *cellJ = json_array_get(cellsJ, i);
				if (cellJ)
					cells[i] = json_integer_value(cellJ);
			}
		}

		json_t *rowNamesJ = json_object_get(rootJ, "rowNames");
		if (rowNamesJ && json_array_size(rowNamesJ) == ARRANGE_ROWS) {
			for (int i = 0; i < ARRANGE_ROWS; i++) {
				json_t *rowNameJ = json_array_get(rowNamesJ, i);
				if (rowNameJ) {
					rowNames[i] = json_string_value(rowNameJ);
					getOutputInfo(Arrange::MAIN_OUTPUT + i)->description = rowNames[i];
					dirtyNames[i] = true;
				}
			}
		}
	}

	void process(const ProcessArgs &args) override {
		if (clearTrig.process(params[CLEAR_BTN_PARAM].getValue() + inputs[CLEAR_INPUT].getVoltage())) { clearCells(); }
		if (rndTrig.process(params[RND_TRIG_BTN_PARAM].getValue() + inputs[RND_TRIG_INPUT].getVoltage())) { randomizeCells(); }

		if (resetTrig.process(params[RESET_BTN_PARAM].getValue() + inputs[RESET_INPUT].getVoltage())) {
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

		int pos = resetMode ? getSeqStart() : seqPos;
		int rowsOn = 0;
		float defVolts = (defaultToTenVoltsWhenNoInput) ? 10.0f : 0.0f;
		for(int i=0;i<ARRANGE_ROWS;i++){
			bool on = isCellOn(pos, i);
			int channels = inputs[MAIN_INPUT + i].getChannels();
			if (channels > 0) {
				for (int c = 0; c < channels; c++) {
					float onVoltageOut = inputs[MAIN_INPUT + i].isConnected() ? inputs[MAIN_INPUT + i].getVoltage(c) : 0;
					outputs[MAIN_OUTPUT + i].setVoltage(on ? onVoltageOut : 0, c);
				}
				outputs[MAIN_OUTPUT + i].setChannels(channels);
			} else {
				//NO INPUT
				outputs[MAIN_OUTPUT + i].setVoltage(on ? defVolts : 0.0f);
				outputs[MAIN_OUTPUT + i].setChannels(1);

			}
			if(on){
				rowsOn++;
			}
		}
		if (absolutePosMode) {
			outputs[POS_OUTPUT].setVoltage(rescalefjw(pos, 0, ARRANGE_COLS-1, 0.0, 10.0));
		} else {
			outputs[POS_OUTPUT].setVoltage(rescalefjw(pos, getSeqStart(), getSeqEnd(), 0.0, 10.0));
		}
		outputs[INTENSITY_OUTPUT].setVoltage(rescalefjw(rowsOn, 0.0, ARRANGE_ROWS, 0.0, 10.0));

		bool pulse = gatePulse.process(1.0 / args.sampleRate);		
		outputs[EOC_OUTPUT].setVoltage((pulse && eocOn) ? 10.0 : 0.0);
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
			seqPos = clampijw(getSeqStart() + getSeqLen(), 0, ARRANGE_COLS-1);
		}
	}

	void resetSeqToEnd(){
		int curPlayMode = getPlayMode();
		if(curPlayMode == PM_FWD_LOOP || curPlayMode == PM_FWD_BWD_LOOP || curPlayMode == PM_RANDOM_POS){
			seqPos = clampijw(getSeqStart() + getSeqLen(), 0, ARRANGE_COLS-1);
		} else if(curPlayMode == PM_BWD_LOOP || curPlayMode == PM_BWD_FWD_LOOP){
			seqPos = getSeqStart();
		}
	}

	int getSeqStart(){
		int inputOffset = int(rescalefjw(inputs[START_INPUT].getVoltage(), 0, 10.0, 0.0, ARRANGE_COLS-1));
		int start = clampijw(params[START_PARAM].getValue() + inputOffset, 0.0, ARRANGE_COLS-1);
		return start;
	}

	int getSeqLen(){
		int inputOffset = int(rescalefjw(inputs[LENGTH_INPUT].getVoltage(), 0, 10.0, 0.0, ARRANGE_COLS-1));
		int len = clampijw(params[LENGTH_KNOB_PARAM].getValue() + inputOffset, 1.0, ARRANGE_COLS);
		return len;
	}

	int getSeqEnd(){
		int seqEnd = clampijw(getSeqStart() + getSeqLen() - 1, 0, ARRANGE_COLS-1);
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
		}
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
				for(int y=0; y < ARRANGE_ROWS; y++){
					if(random::uniform() < rndAmt){
						int div = int(random::uniform() * ARRANGE_COLS * 0.5) + 1;
						for(int x=0; x < ARRANGE_COLS; x++){
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
					float offset = ARRANGE_ROWS * 0.5;
					for(int x=0;x<ARRANGE_COLS;x+=1){
						int y = int(offset + (sinf(angle)*(offset)));
						setCellOn(x, y, true);
						angle+=angleInc;
					}
				}
				break;
			}
			case RND_MIRROR_X:{
				for(int y=0; y < ARRANGE_ROWS; y++){
					for(int i=0; i < 3; i++){
						if(random::uniform() < rndAmt){
							int xLeft = int(random::uniform() * ARRANGE_COLS);
							setCellOn(xLeft, y, true);
							setCellOn(ARRANGE_COLS-xLeft-1, y, true);
						}
					}
				}
				break;
			}
			case RND_MIRROR_Y:{
				for(int x=0; x < ARRANGE_COLS; x++){
					for(int i=0; i < 2; i++){
						if(random::uniform() < rndAmt){
							int yTop = int(random::uniform() * ARRANGE_ROWS);
							setCellOn(x, yTop, true);
							setCellOn(x, ARRANGE_ROWS-yTop-1, true);
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

	void setCellOn(int cellX, int cellY, bool on) {
	    if (cellX >= 0 && cellX < ARRANGE_COLS && 
	        cellY >= 0 && cellY < ARRANGE_ROWS) {
	        int index = iFromXY(cellX, cellY);
	        if (index >= 0 && index < CELLS) {
	            cells[index] = on;
	        }
	    }
	}

	bool isCellOnByDisplayPos(float displayX, float displayY){
		return isCellOn(int(displayX / CELLW), int(displayY / CELLH));
	}

	bool isCellOn(int cellX, int cellY){
		return cells[iFromXY(cellX, cellY)];
	}

	int iFromXY(int cellX, int cellY){
		return cellX + cellY * ARRANGE_COLS;
	}

	int xFromI(int cellI){
		return cellI % ARRANGE_COLS;
	}

	int yFromI(int cellI){
		return cellI / ARRANGE_COLS;
	}
};

struct ArrangeDisplay : LightWidget {
	Arrange *module;
	bool currentlyTurningOn = false;
	Vec dragPos;
	ArrangeDisplay(){
	}
	~ArrangeDisplay(){
	}
	void onButton(const event::Button &e) override {
		if (module && e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			e.consume(this);
			// If Shift is held, move playhead to nearest column instead of toggling a cell
			bool shiftDown = (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT;
			if (shiftDown) {
				int clickedCol = int(e.pos.x / CELLW);
				int seqStart = module->getSeqStart();
				int seqEnd = module->getSeqEnd();
				clickedCol = clampijw(clickedCol, seqStart, seqEnd);
				module->seqPos = clickedCol;
				module->resetMode = false;
				module->hitEnd = false;
				return;
			}
			dragPos = e.pos;
			currentlyTurningOn = !module->isCellOnByDisplayPos(e.pos.x, e.pos.y);
			module->setCellOnByDisplayPos(e.pos.x, e.pos.y, currentlyTurningOn);
		}
	}
	
	void onDragStart(const event::DragStart &e) override {
	}

	void onDragMove(const event::DragMove &e) override {
		if(module == NULL) return;
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
			for(int i=0;i<ARRANGE_COLS+1;i++){
				nvgStrokeWidth(args.vg, (i % 4 == 0) ? 2 : 1);
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, i * CELLW, 0);
				nvgLineTo(args.vg, i * CELLW, box.size.y);
				nvgStroke(args.vg);
			}
			for(int i=0;i<ARRANGE_ROWS+1;i++){
				nvgStrokeWidth(args.vg, (i % 4 == 0) ? 2 : 1);
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, 0, i * CELLH);
				nvgLineTo(args.vg, box.size.x, i * CELLH);
				nvgStroke(args.vg);
			}

			if(module == NULL) return;

			//cells
			for(int i=0;i<CELLS;i++){
				if(module->cells[i]){
					int x = module->xFromI(i);
					int y = module->yFromI(i);
					nvgFillColor(args.vg, module->colors[y%4]);
					nvgBeginPath(args.vg);
					nvgRect(args.vg, x * CELLW + 2, y * CELLH + 2, CELLW - 4, CELLH - 4);
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

struct PlayModeKnob2 : JwSmallSnapKnob {
	PlayModeKnob2(){}
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

struct RndModeKnob2 : JwSmallSnapKnob {
	RndModeKnob2(){}
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

struct RowTextField : LedDisplayTextField {
	Arrange* module;
	int i = -1;

	void step() override {
		LedDisplayTextField::step();
		if (module && module->dirtyNames) {
			setText(module->rowNames[i]);
			module->dirtyNames[i] = false;
        }
	}

	void onChange(const ChangeEvent& e) override {
		if (module) {
			module->rowNames[i].assign(getText());
			module->getOutputInfo(Arrange::MAIN_OUTPUT + i)->description = getText();
        }
	}
};
	
struct RowDisplay : LedDisplay {
    RowTextField* textField;
	void setModule(Arrange* module, int i) {
		textField = createWidget<RowTextField>(Vec(0, 0));
		textField->module = module;
		textField->i = i;
		textField->text = "";
		textField->box.size = box.size;
		textField->multiline = false;
		if(module)textField->color = module->colors[i%4];
		textField->textOffset = Vec(-1, -2);
		addChild(textField);
	}
    ~RowDisplay(){
		if(textField){	
			textField = nullptr;
		}
    }
};

struct ArrangeAbsolutePosModeItem : MenuItem {
	Arrange *arrange;
	void onAction(const event::Action &e) override {
		arrange->absolutePosMode = !arrange->absolutePosMode;
	}
	void step() override {
		rightText = (arrange->absolutePosMode) ? "✔" : "";
		MenuItem::step();
	}
};

struct DefaultWhenNoInputModeItem : MenuItem {
	Arrange *arrange;
	void onAction(const event::Action &e) override {
		arrange->defaultToTenVoltsWhenNoInput = !arrange->defaultToTenVoltsWhenNoInput;
	}
	void step() override {
		rightText = (arrange->defaultToTenVoltsWhenNoInput) ? "✔" : "";
		MenuItem::step();
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
	display->box.pos = Vec(60, 42);
	display->box.size = Vec(ARRANGE_COLS*CELLW, ARRANGE_ROWS*CELLH);
	addChild(display);
	if(module != NULL){
		module->displayWidth = display->box.size.x;
		module->displayHeight = display->box.size.y;
	}

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	// top row //
	float topPort = 20.0;
	float topKnob = 15.0;

	addInput(createInput<TinyPJ301MPort>(Vec(90, topPort), module, Arrange::CLOCK_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(120, topPort), module, Arrange::RESET_INPUT));
	addParam(createParam<SmallButton>(Vec(140, topKnob), module, Arrange::RESET_BTN_PARAM));

	addInput(createInput<TinyPJ301MPort>(Vec(180, topPort), module, Arrange::START_INPUT));
	addParam(createParam<JwSmallSnapKnob>(Vec(200, topKnob), module, Arrange::START_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(245, topPort), module, Arrange::LENGTH_INPUT));
	addParam(createParam<JwSmallSnapKnob>(Vec(265, topKnob), module, Arrange::LENGTH_KNOB_PARAM));

	addInput(createInput<TinyPJ301MPort>(Vec(300, 20), module, Arrange::MODE_INPUT));
	PlayModeKnob2 *playModeKnob2 = dynamic_cast<PlayModeKnob2*>(createParam<PlayModeKnob2>(Vec(320, topKnob), module, Arrange::PLAY_MODE_KNOB_PARAM));
	CenteredLabel* const playModeLabel = new CenteredLabel;
	playModeLabel->box.pos = Vec(161, 7);
	playModeLabel->text = "";
	playModeKnob2->connectLabel(playModeLabel, module);
	addChild(playModeLabel);
	addParam(playModeKnob2);

	addInput(createInput<TinyPJ301MPort>(Vec(360, topPort), module, Arrange::CLEAR_INPUT));
	addParam(createParam<SmallButton>(Vec(380, topKnob), module, Arrange::CLEAR_BTN_PARAM));

	addInput(createInput<TinyPJ301MPort>(Vec(420, topPort), module, Arrange::RND_TRIG_INPUT));
	addParam(createParam<SmallButton>(Vec(440, topKnob), module, Arrange::RND_TRIG_BTN_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(480, topPort), module, Arrange::RND_AMT_INPUT));
	addParam(createParam<SmallWhiteKnob>(Vec(500, topKnob), module, Arrange::RND_AMT_KNOB_PARAM));

	RndModeKnob2 *rndModeKnob2 = dynamic_cast<RndModeKnob2*>(createParam<RndModeKnob2>(Vec(538, topKnob), module, Arrange::RND_MODE_KNOB_PARAM));
	CenteredLabel* const rndModeLabel = new CenteredLabel;
	rndModeLabel->box.pos = Vec(275, 7);
	rndModeLabel->text = "";
	rndModeKnob2->connectLabel(rndModeLabel, module);
	addChild(rndModeLabel);
	addParam(rndModeKnob2);

	addOutput(createOutput<TinyPJ301MPort>(Vec(580, topPort), module, Arrange::POS_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(610, topPort), module, Arrange::INTENSITY_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(640, topPort), module, Arrange::EOC_OUTPUT));

	///////////////////////////////////////////////////// MAIN PORTS /////////////////////////////////////////////////////

	float outputRowTop = 43.0;
	float outputRowDist = 20.0;
	for(int i=0;i<ARRANGE_ROWS;i++){
		if (i % 4 == 0) {
			addInput(createInput<Orange_TinyPJ301MPort>(Vec(3, outputRowTop + i * outputRowDist), module, Arrange::MAIN_INPUT + i));
		} else if (i % 4 == 1) {
			addInput(createInput<Yellow_TinyPJ301MPort>(Vec(3, outputRowTop + i * outputRowDist), module, Arrange::MAIN_INPUT + i));
		} else if (i % 4 == 2) {
			addInput(createInput<Purple_TinyPJ301MPort>(Vec(3, outputRowTop + i * outputRowDist), module, Arrange::MAIN_INPUT + i));
		} else if (i % 4 == 3) {
			addInput(createInput<Blue_TinyPJ301MPort>(Vec(3, outputRowTop + i * outputRowDist), module, Arrange::MAIN_INPUT + i));
		}
		if (i % 4 == 0) {
			addOutput(createOutput<Orange_TinyPJ301MPort>(Vec(702, outputRowTop + i * outputRowDist), module, Arrange::MAIN_OUTPUT + i));
		} else if (i % 4 == 1) {
			addOutput(createOutput<Yellow_TinyPJ301MPort>(Vec(702, outputRowTop + i * outputRowDist), module, Arrange::MAIN_OUTPUT + i));
		} else if (i % 4 == 2) {
			addOutput(createOutput<Purple_TinyPJ301MPort>(Vec(702, outputRowTop + i * outputRowDist), module, Arrange::MAIN_OUTPUT + i));
		} else if (i % 4 == 3) {
			addOutput(createOutput<Blue_TinyPJ301MPort>(Vec(702, outputRowTop + i * outputRowDist), module, Arrange::MAIN_OUTPUT + i));
		}

		RowDisplay* rowDisplay = createWidget<RowDisplay>(Vec(20, outputRowTop + i * outputRowDist));
		rowDisplay->box.size = Vec(36, 16);
		rowDisplay->setModule(module, i);
		addChild(rowDisplay);
	}
}

void ArrangeWidget::appendContextMenu(Menu *menu) {
	MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);

	Arrange *arrange = dynamic_cast<Arrange*>(module);

	ArrangeAbsolutePosModeItem *absolutePosMode = new ArrangeAbsolutePosModeItem();
	absolutePosMode->text = "Absolute Position";
	absolutePosMode->arrange = arrange;
	menu->addChild(absolutePosMode);

	DefaultWhenNoInputModeItem *def = new DefaultWhenNoInputModeItem();
	def->text = "Default to 10V out when no input";
	def->arrange = arrange;
	menu->addChild(def);
}

Model *modelArrange = createModel<Arrange, ArrangeWidget>("Arrange");
