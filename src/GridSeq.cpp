#include "JWModules.hpp"

struct GridSeq : Module,QuantizeUtils {
	enum ParamIds {
		RUN_PARAM,
		CLOCK_PARAM,
		RESET_PARAM,
		CELL_NOTE_PARAM,
		CELL_GATE_PARAM = CELL_NOTE_PARAM + 16,
		RND_NOTES_PARAM = CELL_GATE_PARAM + 16,
		ROOT_NOTE_PARAM,
		SCALE_PARAM,
		RND_GATES_PARAM,
		RIGHT_MOVE_BTN_PARAM,
		LEFT_MOVE_BTN_PARAM,
		DOWN_MOVE_BTN_PARAM,
		UP_MOVE_BTN_PARAM,
		RND_MOVE_BTN_PARAM,
		REP_MOVE_BTN_PARAM,
		VOLT_MAX_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		EXT_CLOCK_INPUT,
		RESET_INPUT,
		RIGHT_INPUT, LEFT_INPUT, DOWN_INPUT, UP_INPUT,
		REPEAT_INPUT,
		RND_DIR_INPUT,		
		RND_NOTES_INPUT,
		RND_GATES_INPUT,
		VOLT_MAX_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		GATES_OUTPUT,
		CELL_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		RUNNING_LIGHT,
		RESET_LIGHT,
		GATES_LIGHT,
		STEPS_LIGHT = GATES_LIGHT+ 16,
		NUM_LIGHTS = STEPS_LIGHT + 16
	};

	dsp::SchmittTrigger rightTrigger;
	dsp::SchmittTrigger leftTrigger;
	dsp::SchmittTrigger downTrigger;
	dsp::SchmittTrigger upTrigger;

	dsp::SchmittTrigger repeatTrigger;
	dsp::SchmittTrigger rndPosTrigger;

	dsp::SchmittTrigger runningTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::SchmittTrigger rndNotesTrigger;
	dsp::SchmittTrigger rndGatesTrigger;
	dsp::SchmittTrigger gateTriggers[16];

	int index = 0;
	int posX = 0;
	int posY = 0;
	float phase = 0.0;
	float noteParamMax = 10.0;
	bool gateState[16] = {};
	bool running = true;
	bool ignoreGateOnPitchOut = false;
	bool resetMode = false;

	enum GateMode { TRIGGER, RETRIGGER, CONTINUOUS };
	GateMode gateMode = TRIGGER;
	dsp::PulseGenerator gatePulse;

	GridSeq() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(RUN_PARAM, 0.0, 1.0, 0.0);
		configParam(RESET_PARAM, 0.0, 1.0, 0.0);
		configParam(RIGHT_MOVE_BTN_PARAM, 0.0, 1.0, 0.0);
		configParam(LEFT_MOVE_BTN_PARAM, 0.0, 1.0, 0.0);
		configParam(DOWN_MOVE_BTN_PARAM, 0.0, 1.0, 0.0);
		configParam(UP_MOVE_BTN_PARAM, 0.0, 1.0, 0.0);
		configParam(RND_MOVE_BTN_PARAM, 0.0, 1.0, 0.0);
		configParam(REP_MOVE_BTN_PARAM, 0.0, 1.0, 0.0);
		configParam(ROOT_NOTE_PARAM, 0.0, QuantizeUtils::NUM_NOTES-1, QuantizeUtils::NOTE_C);
		configParam(SCALE_PARAM, 0.0, QuantizeUtils::NUM_SCALES-1, QuantizeUtils::MINOR);
		configParam(RND_GATES_PARAM, 0.0, 1.0, 0.0);
		configParam(RND_NOTES_PARAM, 0.0, 1.0, 0.0);
		configParam(VOLT_MAX_PARAM, 0.0, 10.0, 5.0);
		for (int y = 0; y < 4; y++) {
			for (int x = 0; x < 4; x++) {
				int idx = (x+(y*4));
				configParam(CELL_NOTE_PARAM + idx, 0.0, noteParamMax, 3.0);
				configParam(CELL_GATE_PARAM + idx, 0.0, 1.0, 0.0);
			}
		}
	}

	void process(const ProcessArgs &args) override;

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "running", json_boolean(running));
		json_object_set_new(rootJ, "ignoreGateOnPitchOut", json_boolean(ignoreGateOnPitchOut));

		// gates
		json_t *gatesJ = json_array();
		for (int i = 0; i < 16; i++) {
			json_t *gateJ = json_integer((int) gateState[i]);
			json_array_append_new(gatesJ, gateJ);
		}
		json_object_set_new(rootJ, "gates", gatesJ);

		// gateMode
		json_t *gateModeJ = json_integer((int) gateMode);
		json_object_set_new(rootJ, "gateMode", gateModeJ);

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);

		json_t *ignoreGateOnPitchOutJ = json_object_get(rootJ, "ignoreGateOnPitchOut");
		if (ignoreGateOnPitchOutJ)
			ignoreGateOnPitchOut = json_is_true(ignoreGateOnPitchOutJ);

		// gates
		json_t *gatesJ = json_object_get(rootJ, "gates");
		if (gatesJ) {
			for (int i = 0; i < 16; i++) {
				json_t *gateJ = json_array_get(gatesJ, i);
				if (gateJ)
					gateState[i] = !!json_integer_value(gateJ);
			}
		}

		// gateMode
		json_t *gateModeJ = json_object_get(rootJ, "gateMode");
		if (gateModeJ)
			gateMode = (GateMode)json_integer_value(gateModeJ);
	}

	void onReset() override {
		for (int i = 0; i < 16; i++) {
			gateState[i] = true;
		}
	}

	void onRandomize() override {
		randomizeGateStates();
	}

	void randomizeGateStates() {
		for (int i = 0; i < 16; i++) {
			gateState[i] = (random::uniform() > 0.50);
		}
	}

	float getOneRandomNote(){
		return random::uniform() * noteParamMax;
	}

	void randomizeNotesOnly(){
		for (int i = 0; i < 16; i++) {
			params[CELL_NOTE_PARAM + i].setValue(getOneRandomNote());
		}
	}

	float closestVoltageInScaleWrapper(float voltsIn){
		float totalMax = clampfjw(params[VOLT_MAX_PARAM].getValue()+inputs[VOLT_MAX_INPUT].getVoltage(), 0.0, 10.0);
		float voltsScaled = rescalefjw(voltsIn, 0, noteParamMax, 0, totalMax);
		int rootNote = params[ROOT_NOTE_PARAM].getValue();
		int scale = params[SCALE_PARAM].getValue();
		return closestVoltageInScale(voltsScaled, rootNote, scale);
	}

	void handleMoveRight(){ posX = posX == 3 ? 0 : posX + 1; }
	void handleMoveLeft(){ posX = posX == 0 ? 3 : posX - 1; }
	void handleMoveDown(){ posY = posY == 3 ? 0 : posY + 1; }
	void handleMoveUp(){ posY = posY == 0 ? 3 : posY - 1; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// STEP
///////////////////////////////////////////////////////////////////////////////////////////////////
void GridSeq::process(const ProcessArgs &args) {
	const float lightLambda = 0.05;
	// Run
	if (runningTrigger.process(params[RUN_PARAM].getValue())) {
		running = !running;
	}
	lights[RUNNING_LIGHT].value = running ? 1.0 : 0.0;
	
	bool nextStep = false;
	if (resetTrigger.process(params[RESET_PARAM].getValue() + inputs[RESET_INPUT].getVoltage())) {
		resetMode = true;
	}

	if(running){

		if (rndNotesTrigger.process(inputs[RND_NOTES_INPUT].getVoltage())) {
			randomizeNotesOnly();
		}

		if (rndGatesTrigger.process(inputs[RND_GATES_INPUT].getVoltage())) {
			randomizeGateStates();
		}

		if (repeatTrigger.process(inputs[REPEAT_INPUT].getVoltage() + params[REP_MOVE_BTN_PARAM].getValue())) {
			nextStep = true;
		} 

		if (rndPosTrigger.process(inputs[RND_DIR_INPUT].getVoltage() + params[RND_MOVE_BTN_PARAM].getValue())) {
			nextStep = true;
			switch(int(4 * random::uniform())){
				case 0:handleMoveRight();break;
				case 1:handleMoveLeft();break;
				case 2:handleMoveDown();break;
				case 3:handleMoveUp();break;
			}
		} 
		if (rightTrigger.process(inputs[RIGHT_INPUT].getVoltage() + params[RIGHT_MOVE_BTN_PARAM].getValue())) {
			nextStep = true;
			handleMoveRight();
		} 
		if (leftTrigger.process(inputs[LEFT_INPUT].getVoltage() + params[LEFT_MOVE_BTN_PARAM].getValue())) {
			nextStep = true;
			handleMoveLeft();
		} 
		if (downTrigger.process(inputs[DOWN_INPUT].getVoltage() + params[DOWN_MOVE_BTN_PARAM].getValue())) {
			nextStep = true;
			handleMoveDown();
		} 
		if (upTrigger.process(inputs[UP_INPUT].getVoltage() + params[UP_MOVE_BTN_PARAM].getValue())) {
			nextStep = true;
			handleMoveUp();
		}
	}
	
	if (nextStep) {
		if(resetMode){
			resetMode = false;
			phase = 0.0;
			posX = 0;
			posY = 0;
			index = 0;
			lights[RESET_LIGHT].value =  1.0;
		}
		index = posX + (posY * 4);
		lights[STEPS_LIGHT + index].value = 1.0;
		gatePulse.trigger(1e-1);
	}

	lights[RESET_LIGHT].value -= lights[RESET_LIGHT].value / lightLambda / args.sampleRate;
	bool pulse = gatePulse.process(1.0 / args.sampleRate);

	// Gate buttons
	for (int i = 0; i < 16; i++) {
		if (gateTriggers[i].process(params[CELL_GATE_PARAM + i].getValue())) {
			gateState[i] = !gateState[i];
		}
		bool gateOn = (running && i == index && gateState[i]);
		if (gateMode == TRIGGER)
			gateOn = gateOn && pulse;
		else if (gateMode == RETRIGGER)
			gateOn = gateOn && !pulse;

		if(lights[STEPS_LIGHT + i].value > 0){ lights[STEPS_LIGHT + i].value -= lights[STEPS_LIGHT + i].value / lightLambda / args.sampleRate; }
		lights[GATES_LIGHT + i].value = gateState[i] ? 1.0 - lights[STEPS_LIGHT + i].value : lights[STEPS_LIGHT + i].value;
	}

	// Cells
	bool gatesOn = (running && gateState[index]);
	if (gateMode == TRIGGER)
		gatesOn = gatesOn && pulse;
	else if (gateMode == RETRIGGER)
		gatesOn = gatesOn && !pulse;

	// Outputs
	if(gatesOn || ignoreGateOnPitchOut)	{
		outputs[CELL_OUTPUT].setVoltage(closestVoltageInScaleWrapper(params[CELL_NOTE_PARAM + index].getValue()));
	}
	outputs[GATES_OUTPUT].setVoltage(gatesOn ? 10.0 : 0.0);
}

struct GridSeqWidget : ModuleWidget {
	std::vector<ParamWidget*> seqKnobs;
	std::vector<ParamWidget*> gateButtons;
	GridSeqWidget(GridSeq *module);
	~GridSeqWidget(){ 
		seqKnobs.clear(); 
		gateButtons.clear(); 
	}
	void appendContextMenu(Menu *menu) override;
};

struct RandomizeNotesOnlyButton : SmallButton {
	void onButton(const event::Button &e) override {
		SmallButton::onButton(e);
		GridSeqWidget *gsw = this->getAncestorOfType<GridSeqWidget>();
		GridSeq *gs = dynamic_cast<GridSeq*>(gsw->module);
		for (int i = 0; i < 16; i++) {
			if(e.action == GLFW_PRESS && e.button == 0){
				gsw->seqKnobs[i]->paramQuantity->setValue(gs->getOneRandomNote());
			} else if(e.action == GLFW_PRESS && e.button == 1){
				//right click this to update the knobs (if randomized by cv in)
				gsw->seqKnobs[i]->paramQuantity->setValue(gs->params[GridSeq::CELL_NOTE_PARAM + i].getValue());
			}
		}
	}
};

struct RandomizeGatesOnlyButton : SmallButton {
	void onButton(const event::Button &e) override {
		SmallButton::onButton(e);
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			GridSeqWidget *gsw = this->getAncestorOfType<GridSeqWidget>();
			for (int i = 0; i < 16; i++) {
				gsw->gateButtons[i]->paramQuantity->setValue(random::uniform() > 0.5);
			}
		}
	}
};

GridSeqWidget::GridSeqWidget(GridSeq *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*20, RACK_GRID_HEIGHT);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/GridSeq.svg")));
		addChild(panel);
	}

	addChild(createWidget<Screw_J>(Vec(16, 1)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 1)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	///// RUN /////
	addParam(createParam<TinyButton>(Vec(27, 90), module, GridSeq::RUN_PARAM));
	addChild(createLight<SmallLight<MyBlueValueLight>>(Vec(27+3.75, 90+3.75), module, GridSeq::RUNNING_LIGHT));

	///// RESET /////
	addParam(createParam<TinyButton>(Vec(27, 138), module, GridSeq::RESET_PARAM));
	addChild(createLight<SmallLight<MyBlueValueLight>>(Vec(27+3.75, 138+3.75), module, GridSeq::RESET_LIGHT));
	addInput(createInput<PJ301MPort>(Vec(22, 160), module, GridSeq::RESET_INPUT));

	///// DIR CONTROLS /////
	addParam(createParam<RightMoveButton>(Vec(70, 30), module, GridSeq::RIGHT_MOVE_BTN_PARAM));
	addParam(createParam<LeftMoveButton>(Vec(103, 30), module, GridSeq::LEFT_MOVE_BTN_PARAM));
	addParam(createParam<DownMoveButton>(Vec(137, 30), module, GridSeq::DOWN_MOVE_BTN_PARAM));
	addParam(createParam<UpMoveButton>(Vec(172, 30), module, GridSeq::UP_MOVE_BTN_PARAM));
	addParam(createParam<RndMoveButton>(Vec(215, 30), module, GridSeq::RND_MOVE_BTN_PARAM));
	addParam(createParam<RepMoveButton>(Vec(255, 30), module, GridSeq::REP_MOVE_BTN_PARAM));

	addInput(createInput<PJ301MPort>(Vec(70, 52), module, GridSeq::RIGHT_INPUT));
	addInput(createInput<PJ301MPort>(Vec(103, 52), module, GridSeq::LEFT_INPUT));
	addInput(createInput<PJ301MPort>(Vec(137, 52), module, GridSeq::DOWN_INPUT));
	addInput(createInput<PJ301MPort>(Vec(172, 52), module, GridSeq::UP_INPUT));
	addInput(createInput<PJ301MPort>(Vec(212, 52), module, GridSeq::RND_DIR_INPUT));
	addInput(createInput<PJ301MPort>(Vec(253, 52), module, GridSeq::REPEAT_INPUT));

	///// NOTE AND SCALE CONTROLS /////
	NoteKnob *noteKnob = dynamic_cast<NoteKnob*>(createParam<NoteKnob>(Vec(70, 315), module, GridSeq::ROOT_NOTE_PARAM));
	CenteredLabel* const noteLabel = new CenteredLabel;
	noteLabel->box.pos = Vec(41, 178);
	noteLabel->text = "note here";
	noteKnob->connectLabel(noteLabel, module);
	addChild(noteLabel);
	addParam(noteKnob);

	ScaleKnob *scaleKnob = dynamic_cast<ScaleKnob*>(createParam<ScaleKnob>(Vec(108, 315), module, GridSeq::SCALE_PARAM));
	CenteredLabel* const scaleLabel = new CenteredLabel;
	scaleLabel->box.pos = Vec(61, 178);
	scaleLabel->text = "scale here";
	scaleKnob->connectLabel(scaleLabel, module);
	addChild(scaleLabel);
	addParam(scaleKnob);

	addParam(createParam<RandomizeGatesOnlyButton>(Vec(196, 315), module, GridSeq::RND_GATES_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(201, 345), module, GridSeq::RND_GATES_INPUT));

	addParam(createParam<RandomizeNotesOnlyButton>(Vec(250, 315), module, GridSeq::RND_NOTES_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(255, 345), module, GridSeq::RND_NOTES_INPUT));

	addParam(createParam<JwSmallSnapKnob>(Vec(146, 315), module, GridSeq::VOLT_MAX_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(152, 345), module, GridSeq::VOLT_MAX_INPUT));

	//// MAIN SEQUENCER KNOBS ////
	int boxSize = 55;
	for (int y = 0; y < 4; y++) {
		for (int x = 0; x < 4; x++) {
			int knobX = x * boxSize + 75;
			int knobY = y * boxSize + 105;
			int idx = (x+(y*4));
			if(module != NULL){
				module->gateState[idx] = true; //start with all gates on
			}

			//maybe someday put note labels in each cell
			float noteParamMax = 0;
			if(module != NULL){
				noteParamMax = module->noteParamMax;
			}
			ParamWidget *cellNoteKnob = createParam<SmallWhiteKnob>(Vec(knobX, knobY), module, GridSeq::CELL_NOTE_PARAM + idx);
			addParam(cellNoteKnob);
			seqKnobs.push_back(cellNoteKnob);

			ParamWidget *cellGateButton = createParam<LEDButton>(Vec(knobX+22, knobY-15), module, GridSeq::CELL_GATE_PARAM + idx);
			addParam(cellGateButton);
			gateButtons.push_back(cellGateButton);

			addChild(createLight<SmallLight<MyBlueValueLight>>(Vec(knobX+27.5, knobY-9.5), module, GridSeq::GATES_LIGHT + idx));
		}
	}

	///// OUTPUTS /////
	addOutput(createOutput<PJ301MPort>(Vec(22, 233), module, GridSeq::GATES_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(22, 295), module, GridSeq::CELL_OUTPUT));
}

struct GridSeqPitchMenuItem : MenuItem {
	GridSeq *gridSeq;
	void onAction(const event::Action &e) override {
		gridSeq->ignoreGateOnPitchOut = !gridSeq->ignoreGateOnPitchOut;
	}
	void step() override {
		rightText = (gridSeq->ignoreGateOnPitchOut) ? "✔" : "";
		MenuItem::step();
	}
};

struct GridSeqGateModeItem : MenuItem {
	GridSeq *gridSeq;
	GridSeq::GateMode gateMode;
	void onAction(const event::Action &e) override {
		gridSeq->gateMode = gateMode;
	}
	void step() override {
		rightText = (gridSeq->gateMode == gateMode) ? "✔" : "";
		MenuItem::step();
	}
};

void GridSeqWidget::appendContextMenu(Menu *menu) {
	MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);

	GridSeq *gridSeq = dynamic_cast<GridSeq*>(module);
	assert(gridSeq);

	MenuLabel *modeLabel = new MenuLabel();
	modeLabel->text = "Gate Mode";
	menu->addChild(modeLabel);

	GridSeqGateModeItem *triggerItem = new GridSeqGateModeItem();
	triggerItem->text = "Trigger";
	triggerItem->gridSeq = gridSeq;
	triggerItem->gateMode = GridSeq::TRIGGER;
	menu->addChild(triggerItem);

	GridSeqGateModeItem *retriggerItem = new GridSeqGateModeItem();
	retriggerItem->text = "Retrigger";
	retriggerItem->gridSeq = gridSeq;
	retriggerItem->gateMode = GridSeq::RETRIGGER;
	menu->addChild(retriggerItem);

	GridSeqGateModeItem *continuousItem = new GridSeqGateModeItem();
	continuousItem->text = "Continuous";
	continuousItem->gridSeq = gridSeq;
	continuousItem->gateMode = GridSeq::CONTINUOUS;
	menu->addChild(continuousItem);

	MenuLabel *spacerLabel2 = new MenuLabel();
	menu->addChild(spacerLabel2);

	GridSeqPitchMenuItem *pitchMenuItem = new GridSeqPitchMenuItem();
	pitchMenuItem->text = "Ignore Gate for V/OCT Out";
	pitchMenuItem->gridSeq = gridSeq;
	menu->addChild(pitchMenuItem);
}

Model *modelGridSeq = createModel<GridSeq, GridSeqWidget>("GridSeq");
