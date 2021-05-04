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
		OCTAVE_PARAM,
		CELL_PROB_PARAM,
		RND_PROBS_PARAM = CELL_PROB_PARAM + 16,
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
		ROOT_INPUT,
		SCALE_INPUT,
		OCTAVE_INPUT,
		RND_PROBS_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		GATES_OUTPUT,
		CELL_OUTPUT,
		GATES_YX_OUTPUT,
		CELL_YX_OUTPUT,
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
	dsp::SchmittTrigger rndProbsTrigger;
	dsp::SchmittTrigger gateTriggers[16];

	int index = 0;
	int indexYX = 0;
	int posX = 0;
	int posY = 0;
	float phase = 0.0;
	float noteParamMax = 10.0;
	bool gateState[16] = {};
	bool running = true;
	bool ignoreGateOnPitchOut = false;
	bool resetMode = false;
	float rndFloat0to1AtClockStep = random::uniform();

	enum GateMode { TRIGGER, RETRIGGER, CONTINUOUS };
	GateMode gateMode = TRIGGER;
	dsp::PulseGenerator gatePulse;

	GridSeq() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(RUN_PARAM, 0.0, 1.0, 0.0, "Run");
		configParam(RESET_PARAM, 0.0, 1.0, 0.0, "Reset");
		configParam(RIGHT_MOVE_BTN_PARAM, 0.0, 1.0, 0.0, "Click to Move Right");
		configParam(LEFT_MOVE_BTN_PARAM, 0.0, 1.0, 0.0, "Click to Move Left");
		configParam(DOWN_MOVE_BTN_PARAM, 0.0, 1.0, 0.0, "Click to Move Down");
		configParam(UP_MOVE_BTN_PARAM, 0.0, 1.0, 0.0, "Click to Move Up");
		configParam(RND_MOVE_BTN_PARAM, 0.0, 1.0, 0.0, "Click to Move Random");
		configParam(REP_MOVE_BTN_PARAM, 0.0, 1.0, 0.0, "Click to Repeat");
		configParam(ROOT_NOTE_PARAM, 0.0, QuantizeUtils::NUM_NOTES-1, QuantizeUtils::NOTE_C, "Root Note");
		configParam(SCALE_PARAM, 0.0, QuantizeUtils::NUM_SCALES-1, QuantizeUtils::MINOR, "Scale");
		configParam(RND_GATES_PARAM, 0.0, 1.0, 0.0, "Random Gates (Shift + Click to Init Defaults)");
		configParam(RND_NOTES_PARAM, 0.0, 1.0, 0.0, "Random Notes\n(Shift + Click to Init Defaults)\n(Alt + Click to use first knob as max)");
		configParam(RND_PROBS_PARAM, 0.0, 1.0, 0.0, "Random Probabilities\n(Shift + Click to Init Defaults)\n(Alt + Click to use first knob as max)");
		configParam(VOLT_MAX_PARAM, 0.0, 10.0, 2.0, "Range");
		configParam(OCTAVE_PARAM, -5.0, 7.0, -1.0, "Octave");
		for (int y = 0; y < 4; y++) {
			for (int x = 0; x < 4; x++) {
				int idx = (x+(y*4));
				configParam(CELL_NOTE_PARAM + idx, 0.0, noteParamMax, 3.0, "Voltage");
				configParam(CELL_GATE_PARAM + idx, 0.0, 1.0, 0.0, "Gate");
				configParam(CELL_PROB_PARAM + idx, 0.0, 1.0, 1.0, "Probability");
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

	void randomizeProbsOnly(){
		for (int i = 0; i < 16; i++) {
			params[CELL_PROB_PARAM + i].setValue(random::uniform());
		}
	}

	float closestVoltageInScaleWrapper(float voltsIn){
		int octaveInputOffset = inputs[OCTAVE_INPUT].isConnected() ? int(inputs[OCTAVE_INPUT].getVoltage()) : 0;
		int octave = clampijw(params[OCTAVE_PARAM].getValue() + octaveInputOffset, -5.0, 7.0);

		int rootInputOffset = inputs[ROOT_INPUT].isConnected() ? rescalefjw(inputs[ROOT_INPUT].getVoltage(), 0, 10, 0, QuantizeUtils::NUM_NOTES-1) : 0;
		int rootNote = clampijw(params[ROOT_NOTE_PARAM].getValue() + rootInputOffset, 0, QuantizeUtils::NUM_NOTES-1);

		int scaleInputOffset = inputs[SCALE_INPUT].isConnected() ? rescalefjw(inputs[SCALE_INPUT].getVoltage(), 0, 10, 0, QuantizeUtils::NUM_SCALES-1) : 0;
		int scale = clampijw(params[SCALE_PARAM].getValue() + scaleInputOffset, 0, QuantizeUtils::NUM_SCALES-1);

		float totalMax = clampfjw(params[VOLT_MAX_PARAM].getValue()+inputs[VOLT_MAX_INPUT].getVoltage(), 0.0, 10.0);
		float voltsScaled = rescalefjw(voltsIn, 0, noteParamMax, 0, totalMax);
		return closestVoltageInScale(octave + voltsScaled, rootNote, scale);
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
	const float lightLambda = 0.10;
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

		if (rndProbsTrigger.process(inputs[RND_PROBS_INPUT].getVoltage())) {
			randomizeProbsOnly();
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
			indexYX = 0;
			lights[RESET_LIGHT].value =  1.0;
		}
		rndFloat0to1AtClockStep = random::uniform();
		index = posX + (posY * 4);
		indexYX = posY + (posX * 4);
		lights[STEPS_LIGHT + index].value = 1.0;
		gatePulse.trigger(1e-1);
	}

	lights[RESET_LIGHT].value -= lights[RESET_LIGHT].value / lightLambda / args.sampleRate;
	bool pulse = gatePulse.process(1.0 / args.sampleRate);

	//////////////////////////////////////////////////////////////////////////////////////////	
	// MAIN XY OUT
	//////////////////////////////////////////////////////////////////////////////////////////	

	// Gate buttons
	for (int i = 0; i < 16; i++) {
		if (gateTriggers[i].process(params[CELL_GATE_PARAM + i].getValue())) {
			gateState[i] = !gateState[i];
		}
		bool gateOn = (running && i == index && gateState[i]);
		if (gateMode == TRIGGER) gateOn = gateOn && pulse;
		else if (gateMode == RETRIGGER) gateOn = gateOn && !pulse;

		if(lights[STEPS_LIGHT + i].value > 0){ 
			lights[STEPS_LIGHT + i].value -= lights[STEPS_LIGHT + i].value / lightLambda / args.sampleRate; 
		}
		float gateOnVal = params[CELL_PROB_PARAM + i].getValue();
		lights[GATES_LIGHT + i].value = gateState[i] ? gateOnVal : lights[STEPS_LIGHT + i].value;
	}

	// Cells
	bool prob = params[CELL_PROB_PARAM + index].getValue() > rndFloat0to1AtClockStep;
	bool gatesOn = (running && gateState[index] && prob);
	if (gateMode == TRIGGER) gatesOn = gatesOn && pulse;
	else if (gateMode == RETRIGGER) gatesOn = gatesOn && !pulse;

	// Outputs
	if(gatesOn || ignoreGateOnPitchOut)	{
		outputs[CELL_OUTPUT].setVoltage(closestVoltageInScaleWrapper(params[CELL_NOTE_PARAM + index].getValue()));
	}
	outputs[GATES_OUTPUT].setVoltage(gatesOn ? 10.0 : 0.0);

	//////////////////////////////////////////////////////////////////////////////////////////	
	// INVERTED YX OUT
	//////////////////////////////////////////////////////////////////////////////////////////	
	bool probYX = params[CELL_PROB_PARAM + indexYX].getValue() > rndFloat0to1AtClockStep;
	bool gatesOnYX = (running && gateState[indexYX] && probYX);
	if (gateMode == TRIGGER) gatesOnYX = gatesOnYX && pulse;
	else if (gateMode == RETRIGGER) gatesOnYX = gatesOnYX && !pulse;
	if(gatesOnYX || ignoreGateOnPitchOut)	{
		outputs[CELL_YX_OUTPUT].setVoltage(closestVoltageInScaleWrapper(params[CELL_NOTE_PARAM + indexYX].getValue()));
	}
	outputs[GATES_YX_OUTPUT].setVoltage(gatesOnYX ? 10.0 : 0.0);
}

struct GridSeqWidget : ModuleWidget {
	std::vector<ParamWidget*> seqKnobs;
	std::vector<ParamWidget*> probKnobs;
	std::vector<ParamWidget*> gateButtons;
	GridSeqWidget(GridSeq *module);
	~GridSeqWidget(){ 
		seqKnobs.clear(); 
		probKnobs.clear(); 
		gateButtons.clear(); 
	}
	void appendContextMenu(Menu *menu) override;
};

struct RandomizeNotesOnlyButton : TinyButton {
	void onButton(const event::Button &e) override {
		TinyButton::onButton(e);
		if(e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT){
			GridSeqWidget *gsw = this->getAncestorOfType<GridSeqWidget>();
			GridSeq *gs = dynamic_cast<GridSeq*>(gsw->module);
			float firstKnobVal = gsw->seqKnobs[0]->paramQuantity->getValue();
			for (int i = 0; i < 16; i++) {
				if ((e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT) {
					gsw->seqKnobs[i]->paramQuantity->setValue(3);
				} else if ((e.mods & RACK_MOD_MASK) == GLFW_MOD_ALT) {
					if(i != 0){
						gsw->seqKnobs[i]->paramQuantity->setValue(random::uniform() * firstKnobVal);
					}
				} else {
					gsw->seqKnobs[i]->paramQuantity->setValue(gs->getOneRandomNote());
				}
			}
		}
	}
};

struct RandomizeProbsOnlyButton : TinyButton {
	void onButton(const event::Button &e) override {
		TinyButton::onButton(e);
		if(e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT){
			GridSeqWidget *gsw = this->getAncestorOfType<GridSeqWidget>();
			float firstKnobVal = gsw->probKnobs[0]->paramQuantity->getValue();
			for (int i = 0; i < 16; i++) {
				if ((e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT) {
					gsw->probKnobs[i]->paramQuantity->setValue(1);
				} else if ((e.mods & RACK_MOD_MASK) == GLFW_MOD_ALT) {
					if(i != 0){
						gsw->probKnobs[i]->paramQuantity->setValue(random::uniform() * firstKnobVal);
					}
				} else {
					gsw->probKnobs[i]->paramQuantity->setValue(random::uniform());
				}
			}
		}
	}
};

struct RandomizeGatesOnlyButton : TinyButton {
	void onButton(const event::Button &e) override {
		TinyButton::onButton(e);
		if(e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT){
			GridSeqWidget *gsw = this->getAncestorOfType<GridSeqWidget>();
			GridSeq *gs = dynamic_cast<GridSeq*>(gsw->module);
			for (int i = 0; i < 16; i++) {
				if ((e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT) {
					gs->gateState[i] = true;
				} else {
					bool active = random::uniform() > 0.5;
					gs->gateState[i] = active;
				}
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

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	///// RUN /////
	addParam(createParam<TinyButton>(Vec(27, 90), module, GridSeq::RUN_PARAM));
	addChild(createLight<SmallLight<MyBlueValueLight>>(Vec(27.3+3.75, 90+3.75), module, GridSeq::RUNNING_LIGHT));

	///// RESET /////
	addParam(createParam<TinyButton>(Vec(27, 138), module, GridSeq::RESET_PARAM));
	addChild(createLight<SmallLight<MyBlueValueLight>>(Vec(27.3+3.75, 138+3.75), module, GridSeq::RESET_LIGHT));
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
	float paramY = 313;
	NoteKnob *noteKnob = dynamic_cast<NoteKnob*>(createParam<NoteKnob>(Vec(70, paramY), module, GridSeq::ROOT_NOTE_PARAM));
	CenteredLabel* const noteLabel = new CenteredLabel;
	noteLabel->box.pos = Vec(41, 175);
	noteLabel->text = "C";
	noteKnob->connectLabel(noteLabel, module);
	addChild(noteLabel);
	addParam(noteKnob);
	addInput(createInput<TinyPJ301MPort>(Vec(76, 355), module, GridSeq::ROOT_INPUT));

	addParam(createParam<JwSmallSnapKnob>(Vec(111, paramY), module, GridSeq::OCTAVE_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(117, 355), module, GridSeq::OCTAVE_INPUT));

	ScaleKnob *scaleKnob = dynamic_cast<ScaleKnob*>(createParam<ScaleKnob>(Vec(150, paramY), module, GridSeq::SCALE_PARAM));
	CenteredLabel* const scaleLabel = new CenteredLabel;
	scaleLabel->box.pos = Vec(81, 175);
	scaleLabel->text = "Minor";
	scaleKnob->connectLabel(scaleLabel, module);
	addChild(scaleLabel);
	addParam(scaleKnob);
	addInput(createInput<TinyPJ301MPort>(Vec(155, 355), module, GridSeq::SCALE_INPUT));


	addParam(createParam<JwSmallSnapKnob>(Vec(189, paramY), module, GridSeq::VOLT_MAX_PARAM));//RANGE
	addInput(createInput<TinyPJ301MPort>(Vec(194, 345), module, GridSeq::VOLT_MAX_INPUT));//RANGE

	addParam(createParam<RandomizeGatesOnlyButton>(Vec(230, paramY+10), module, GridSeq::RND_GATES_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(230, 345), module, GridSeq::RND_GATES_INPUT));

	addParam(createParam<RandomizeProbsOnlyButton>(Vec(255, paramY+10), module, GridSeq::RND_PROBS_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(255, 345), module, GridSeq::RND_PROBS_INPUT));

	addParam(createParam<RandomizeNotesOnlyButton>(Vec(279, paramY+10), module, GridSeq::RND_NOTES_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(279, 345), module, GridSeq::RND_NOTES_INPUT));

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
			// float noteParamMax = 0;
			// if(module != NULL){
			// 	noteParamMax = module->noteParamMax;
			// }
			ParamWidget *cellNoteKnob = createParam<SmallWhiteKnob>(Vec(knobX-2, knobY), module, GridSeq::CELL_NOTE_PARAM + idx);
			addParam(cellNoteKnob);
			seqKnobs.push_back(cellNoteKnob);

			ParamWidget *cellProbKnob = createParam<JwTinyGrayKnob>(Vec(knobX+27, knobY+7), module, GridSeq::CELL_PROB_PARAM + idx);
			addParam(cellProbKnob);
			probKnobs.push_back(cellProbKnob);

			ParamWidget *cellGateButton = createParam<LEDButton>(Vec(knobX+22, knobY-15), module, GridSeq::CELL_GATE_PARAM + idx);
			addParam(cellGateButton);
			gateButtons.push_back(cellGateButton);

			addChild(createLight<MediumLight<MyOrangeValueLight>>(Vec(knobX+5, knobY-13.6), module, GridSeq::STEPS_LIGHT + idx));
			addChild(createLight<LargeLight<MyBlueValueLight>>(Vec(knobX+23.5, knobY-13.6), module, GridSeq::GATES_LIGHT + idx));
		}
	}

	///// OUTPUTS /////
	addOutput(createOutput<PJ301MPort>(Vec(10, 233), module, GridSeq::GATES_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(10, 295), module, GridSeq::CELL_OUTPUT));

	addOutput(createOutput<TinyPJ301MPort>(Vec(40, 241), module, GridSeq::GATES_YX_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(40, 303), module, GridSeq::CELL_YX_OUTPUT));
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
