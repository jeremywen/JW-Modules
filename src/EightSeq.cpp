#include "JWModules.hpp"

struct EightSeq : Module,QuantizeUtils {
	enum ParamIds {
		CLOCK_PARAM,
		CELL_NOTE_PARAM,
		CELL_GATE_PARAM = CELL_NOTE_PARAM + 16,
		RND_NOTES_PARAM = CELL_GATE_PARAM + 16,
		ROOT_NOTE_PARAM,
		SCALE_PARAM,
		RND_GATES_PARAM,
		VOLT_MAX_PARAM,
		OCTAVE_PARAM,
		CELL_PROB_PARAM,
		RND_PROBS_PARAM = CELL_PROB_PARAM + 16,
		PROB_ON_SWITCH_PARAM,
		LENGTH_KNOB_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		EXT_CLOCK_INPUT,
		RESET_INPUT,
		RIGHT_INPUT,
		RND_NOTES_INPUT,
		RND_GATES_INPUT,
		VOLT_MAX_INPUT,
		ROOT_INPUT,
		SCALE_INPUT,
		OCTAVE_INPUT,
		RND_PROBS_INPUT,
		LENGTH_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		GATES_OUTPUT,
		CELL_OUTPUT,
		EOC_OUTPUT,
		PROB_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		GATES_LIGHT,
		STEPS_LIGHT = GATES_LIGHT + 16,
		NUM_LIGHTS = STEPS_LIGHT + 16
	};

	dsp::SchmittTrigger rightTrigger;
	dsp::SchmittTrigger runningTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::SchmittTrigger rndNotesTrigger;
	dsp::SchmittTrigger rndGatesTrigger;
	dsp::SchmittTrigger rndProbsTrigger;
	dsp::SchmittTrigger gateTriggers[8];

	int index = 0;
	float phase = 0.0;
	float noteParamMax = 10.0;
	bool gateState[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };
	bool running = true;
	bool ignoreGateOnPitchOut = false;
	bool resetMode = false;
	float rndFloat0to1AtClockStep = random::uniform();
	bool hitEnd = false;

	enum GateMode { TRIGGER, RETRIGGER, CONTINUOUS };
	GateMode gateMode = TRIGGER;

	enum RandomMode { RANDOM, FIRST_MIN, FIRST_MAX };
	RandomMode randomMode = RANDOM;

	dsp::PulseGenerator gatePulse;

	EightSeq() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ROOT_NOTE_PARAM, 0.0, QuantizeUtils::NUM_NOTES-1, QuantizeUtils::NOTE_C, "Root Note");
		configParam(SCALE_PARAM, 0.0, QuantizeUtils::NUM_SCALES-1, QuantizeUtils::MINOR, "Scale");
		configParam(LENGTH_KNOB_PARAM, 1.0, 8.0, 8.0, "Length");
		configParam(RND_GATES_PARAM, 0.0, 1.0, 0.0, "Random Gates (Shift + Click to Init Defaults)");
		configParam(RND_NOTES_PARAM, 0.0, 1.0, 0.0, "Random Notes\n(Shift + Click to Init Defaults)");
		configParam(RND_PROBS_PARAM, 0.0, 1.0, 0.0, "Random Probabilities\n(Shift + Click to Init Defaults)");
		configParam(VOLT_MAX_PARAM, 0.0, 10.0, 2.0, "Range");
		configParam(OCTAVE_PARAM, -5.0, 7.0, -1.0, "Octave");
		configParam(PROB_ON_SWITCH_PARAM, 0.0, 1.0, 1.0, "Probability Switch");
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
		for (int i = 0; i < 8; i++) {
			json_t *gateJ = json_integer((int) gateState[i]);
			json_array_append_new(gatesJ, gateJ);
		}
		json_object_set_new(rootJ, "gates", gatesJ);

		// gateMode
		json_t *gateModeJ = json_integer((int) gateMode);
		json_object_set_new(rootJ, "gateMode", gateModeJ);

		// randomMode
		json_t *randomModeJ = json_integer((int) randomMode);
		json_object_set_new(rootJ, "randomMode", randomModeJ);

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
			for (int i = 0; i < 8; i++) {
				json_t *gateJ = json_array_get(gatesJ, i);
				if (gateJ)
					gateState[i] = !!json_integer_value(gateJ);
			}
		}

		// gateMode
		json_t *gateModeJ = json_object_get(rootJ, "gateMode");
		if (gateModeJ)
			gateMode = (GateMode)json_integer_value(gateModeJ);

		// randomMode
		json_t *randomModeJ = json_object_get(rootJ, "randomMode");
		if (randomModeJ)
			randomMode = (RandomMode)json_integer_value(randomModeJ);
	}

	void onReset() override {
		index = 0;
		resetMode = true;
		for (int i = 0; i < 8; i++) {
			gateState[i] = true;
		}
	}

	void onRandomize() override {
		randomizeGateStates();
	}

	void randomizeGateStates() {
		for (int i = 0; i < 8; i++) {
			gateState[i] = (random::uniform() > 0.50);
		}
	}

	float getOneRandomNote(){
		return random::uniform() * noteParamMax;
	}

	void randomizeNotesOnly(){
		for (int i = 0; i < 8; i++) {
			params[CELL_NOTE_PARAM + i].setValue(getOneRandomNote());
		}
	}

	void randomizeProbsOnly(){
		for (int i = 0; i < 8; i++) {
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
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// STEP
///////////////////////////////////////////////////////////////////////////////////////////////////
void EightSeq::process(const ProcessArgs &args) {
	const float lightLambda = 0.10;
	// Run
	
	bool nextStep = false;
	if (resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
		resetMode = true;
	}

	int inputOffset = int(rescalefjw(inputs[LENGTH_INPUT].getVoltage(), 0, 10.0, 0.0, 7.0));
	int len = clampijw(params[LENGTH_KNOB_PARAM].getValue() + inputOffset, 1.0, 8.0);
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

		if (rightTrigger.process(inputs[RIGHT_INPUT].getVoltage())) {
			nextStep = true;
			index = (index + 1) % len; 
		} 
	}
	if (nextStep) {
		if(resetMode){
			resetMode = false;
			hitEnd = false;
			phase = 0.0;
			index = 0;
		}
		rndFloat0to1AtClockStep = random::uniform();
		lights[STEPS_LIGHT + index].value = 1.0;
		gatePulse.trigger(1e-1);
	}

	bool pulse = gatePulse.process(1.0 / args.sampleRate);

	//////////////////////////////////////////////////////////////////////////////////////////	
	// MAIN XY OUT
	//////////////////////////////////////////////////////////////////////////////////////////	

	// Gate buttons
	for (int i = 0; i < 8; i++) {
		if (gateTriggers[i].process(params[CELL_GATE_PARAM + i].getValue())) {
			gateState[i] = !gateState[i];
		}

		if(lights[STEPS_LIGHT + i].value > 0){ 
			lights[STEPS_LIGHT + i].value -= lights[STEPS_LIGHT + i].value / lightLambda / args.sampleRate; 
		}
		float gateOnVal = params[CELL_PROB_PARAM + i].getValue();
		lights[GATES_LIGHT + i].value = gateState[i] ? gateOnVal : lights[STEPS_LIGHT + i].value;
	}

	// Cells
	bool prob = params[CELL_PROB_PARAM + index].getValue() > rndFloat0to1AtClockStep;
	if(!params[PROB_ON_SWITCH_PARAM].getValue()){
		prob = true;
	}

	bool gatesOn = (running && gateState[index] && prob);
	if (gateMode == TRIGGER) gatesOn = gatesOn && pulse;
	else if (gateMode == RETRIGGER) gatesOn = gatesOn && !pulse;

	// Outputs
	if(gatesOn || ignoreGateOnPitchOut)	{
		outputs[CELL_OUTPUT].setVoltage(closestVoltageInScaleWrapper(params[CELL_NOTE_PARAM + index].getValue()));
	}
	outputs[GATES_OUTPUT].setVoltage(gatesOn ? 10.0 : 0.0);
	outputs[PROB_OUTPUT].setVoltage(rescalefjw(params[CELL_PROB_PARAM + index].getValue(), 0.0, 1.0, 1.0, 10.0));
	outputs[EOC_OUTPUT].setVoltage((pulse && hitEnd && index == 0) ? 10.0 : 0.0);
	if(index == len-1){
		hitEnd = true;
	}
}

struct EightSeqWidget : ModuleWidget {
	std::vector<ParamWidget*> seqKnobs;
	std::vector<ParamWidget*> probKnobs;
	std::vector<ParamWidget*> gateButtons;
	EightSeqWidget(EightSeq *module);
	~EightSeqWidget(){ 
		seqKnobs.clear(); 
		probKnobs.clear(); 
		gateButtons.clear(); 
	}
	void appendContextMenu(Menu *menu) override;
};

struct RandomizeNotes8SeqOnlyButton : TinyButton {
	void onButton(const event::Button &e) override {
		TinyButton::onButton(e);
		if(e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT){
			EightSeqWidget *wid = this->getAncestorOfType<EightSeqWidget>();
			EightSeq *mod = dynamic_cast<EightSeq*>(wid->module);
			float firstKnobVal = wid->seqKnobs[0]->getParamQuantity()->getDisplayValue();
			float firstKnobMaxVal = mod->noteParamMax;
			bool shiftDown = (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT;
			for (int i = 0; i < 8; i++) {
				if (mod->randomMode == EightSeq::FIRST_MIN) {
					if(i != 0){
						wid->seqKnobs[i]->getParamQuantity()->setValue(firstKnobVal + (random::uniform() * (firstKnobMaxVal - firstKnobVal)));
					}
				} else if (shiftDown) {
					wid->seqKnobs[i]->getParamQuantity()->setValue(3);
				} else if (mod->randomMode == EightSeq::FIRST_MAX) {
					if(i != 0){
						wid->seqKnobs[i]->getParamQuantity()->setValue(random::uniform() * firstKnobVal);
					}
				} else {
					wid->seqKnobs[i]->getParamQuantity()->setValue(mod->getOneRandomNote());
				}
			}
		}
	}
};

struct RandomizeProbs8SeqOnlyButton : TinyButton {
	void onButton(const event::Button &e) override {
		TinyButton::onButton(e);
		if(e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT){
			EightSeqWidget *wid = this->getAncestorOfType<EightSeqWidget>();
			EightSeq *mod = dynamic_cast<EightSeq*>(wid->module);
			float firstKnobVal = wid->probKnobs[0]->getParamQuantity()->getDisplayValue();
			float firstKnobMaxVal = 1.0;
			bool shiftDown = (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT;
			for (int i = 0; i < 8; i++) {
				if (mod->randomMode == EightSeq::FIRST_MIN) {
					if(i != 0){
						wid->probKnobs[i]->getParamQuantity()->setValue(firstKnobVal + (random::uniform() * (firstKnobMaxVal - firstKnobVal)));
					}
				} else if (shiftDown) {
					wid->probKnobs[i]->getParamQuantity()->setValue(1);
				} else if (mod->randomMode == EightSeq::FIRST_MAX) {
					if(i != 0){
						wid->probKnobs[i]->getParamQuantity()->setValue(random::uniform() * firstKnobVal);
					}
				} else {
					wid->probKnobs[i]->getParamQuantity()->setValue(random::uniform());
				}
			}
		}
	}
};

struct RandomizeGates8SeqOnlyButton : TinyButton {
	void onButton(const event::Button &e) override {
		TinyButton::onButton(e);
		if(e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT){
			EightSeqWidget *gsw = this->getAncestorOfType<EightSeqWidget>();
			EightSeq *gs = dynamic_cast<EightSeq*>(gsw->module);
			for (int i = 0; i < 8; i++) {
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

EightSeqWidget::EightSeqWidget(EightSeq *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*20, RACK_GRID_HEIGHT);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/EightSeq.svg")));
		addChild(panel);
	}

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	///// CLOCK /////
	addInput(createInput<PJ301MPort>(Vec(73, 43), module, EightSeq::RIGHT_INPUT));

	addParam(createParam<JwSmallSnapKnob>(Vec(128, 45), module, EightSeq::LENGTH_KNOB_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(157, 50), module, EightSeq::LENGTH_INPUT));

	///// RESET /////
	addInput(createInput<PJ301MPort>(Vec(200, 43), module, EightSeq::RESET_INPUT));

	///// NOTE AND SCALE CONTROLS /////
	float paramY = 313;
	NoteKnob *noteKnob = dynamic_cast<NoteKnob*>(createParam<NoteKnob>(Vec(70, paramY), module, EightSeq::ROOT_NOTE_PARAM));
	CenteredLabel* const noteLabel = new CenteredLabel;
	noteLabel->box.pos = Vec(41, 175);
	noteLabel->text = "C";
	noteKnob->connectLabel(noteLabel, module);
	addChild(noteLabel);
	addParam(noteKnob);
	addInput(createInput<TinyPJ301MPort>(Vec(76, 355), module, EightSeq::ROOT_INPUT));

	addParam(createParam<JwSmallSnapKnob>(Vec(111, paramY), module, EightSeq::OCTAVE_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(117, 355), module, EightSeq::OCTAVE_INPUT));

	ScaleKnob *scaleKnob = dynamic_cast<ScaleKnob*>(createParam<ScaleKnob>(Vec(150, paramY), module, EightSeq::SCALE_PARAM));
	CenteredLabel* const scaleLabel = new CenteredLabel;
	scaleLabel->box.pos = Vec(81, 175);
	scaleLabel->text = "Minor";
	scaleKnob->connectLabel(scaleLabel, module);
	addChild(scaleLabel);
	addParam(scaleKnob);
	addInput(createInput<TinyPJ301MPort>(Vec(155, 355), module, EightSeq::SCALE_INPUT));


	addParam(createParam<JwSmallSnapKnob>(Vec(189, paramY), module, EightSeq::VOLT_MAX_PARAM));//RANGE
	addInput(createInput<TinyPJ301MPort>(Vec(194, 345), module, EightSeq::VOLT_MAX_INPUT));//RANGE

	addParam(createParam<RandomizeGates8SeqOnlyButton>(Vec(230, paramY+10), module, EightSeq::RND_GATES_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(230, 345), module, EightSeq::RND_GATES_INPUT));

	addParam(createParam<RandomizeProbs8SeqOnlyButton>(Vec(255, paramY+10), module, EightSeq::RND_PROBS_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(255, 345), module, EightSeq::RND_PROBS_INPUT));

	addParam(createParam<RandomizeNotes8SeqOnlyButton>(Vec(279, paramY+10), module, EightSeq::RND_NOTES_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(279, 345), module, EightSeq::RND_NOTES_INPUT));

	//// MAIN SEQUENCER KNOBS ////
	int boxSizeX = 60;
	int boxSizeY = 75;
	for (int y = 0; y < 2; y++) {
		for (int x = 0; x < 4; x++) {
			int knobX = x * boxSizeX + 40;
			int knobY = y * boxSizeY + 110;
			int idx = (x+(y*4));

			//maybe someday put note labels in each cell
			// float noteParamMax = 0;
			// if(module != NULL){
			// 	noteParamMax = module->noteParamMax;
			// }
			ParamWidget *cellNoteKnob = createParam<SmallWhiteKnob>(Vec(knobX-2, knobY), module, EightSeq::CELL_NOTE_PARAM + idx);
			addParam(cellNoteKnob);
			seqKnobs.push_back(cellNoteKnob);

			ParamWidget *cellProbKnob = createParam<JwTinyGrayKnob>(Vec(knobX+27, knobY+7), module, EightSeq::CELL_PROB_PARAM + idx);
			addParam(cellProbKnob);
			probKnobs.push_back(cellProbKnob);

			ParamWidget *cellGateButton = createParam<LEDButton>(Vec(knobX+22, knobY-15), module, EightSeq::CELL_GATE_PARAM + idx);
			addParam(cellGateButton);
			gateButtons.push_back(cellGateButton);

			addChild(createLight<MediumLight<MyOrangeValueLight>>(Vec(knobX+5, knobY-13.6), module, EightSeq::STEPS_LIGHT + idx));
			addChild(createLight<LargeLight<MyBlueValueLight>>(Vec(knobX+23.5, knobY-13.6), module, EightSeq::GATES_LIGHT + idx));
		}
	}
	addParam(createParam<JwVerticalSwitch>(Vec(242, 252), module, EightSeq::PROB_ON_SWITCH_PARAM));

	///// OUTPUTS /////
	addOutput(createOutput<PJ301MPort>(Vec(50, 250), module, EightSeq::GATES_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(110, 250), module, EightSeq::CELL_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(165, 250), module, EightSeq::EOC_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(215, 250), module, EightSeq::PROB_OUTPUT));

}

struct EightSeqPitchMenuItem : MenuItem {
	EightSeq *eightSeq;
	void onAction(const event::Action &e) override {
		eightSeq->ignoreGateOnPitchOut = !eightSeq->ignoreGateOnPitchOut;
	}
	void step() override {
		rightText = (eightSeq->ignoreGateOnPitchOut) ? "✔" : "";
		MenuItem::step();
	}
};

struct EightSeqGateModeItem : MenuItem {
	EightSeq *eightSeq;
	EightSeq::GateMode gateMode;
	void onAction(const event::Action &e) override {
		eightSeq->gateMode = gateMode;
	}
	void step() override {
		rightText = (eightSeq->gateMode == gateMode) ? "✔" : "";
		MenuItem::step();
	}
};

struct EightSeqRandomModeItem : MenuItem {
	EightSeq *eightSeq;
	EightSeq::RandomMode randomMode;
	void onAction(const event::Action &e) override {
		eightSeq->randomMode = randomMode;
	}
	void step() override {
		rightText = (eightSeq->randomMode == randomMode) ? "✔" : "";
		MenuItem::step();
	}
};

void EightSeqWidget::appendContextMenu(Menu *menu) {
	MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);

	EightSeq *eightSeq = dynamic_cast<EightSeq*>(module);
	assert(eightSeq);

	MenuLabel *modeLabel = new MenuLabel();
	modeLabel->text = "Gate Mode";
	menu->addChild(modeLabel);

	EightSeqGateModeItem *triggerItem = new EightSeqGateModeItem();
	triggerItem->text = "Trigger";
	triggerItem->eightSeq = eightSeq;
	triggerItem->gateMode = EightSeq::TRIGGER;
	menu->addChild(triggerItem);

	EightSeqGateModeItem *retriggerItem = new EightSeqGateModeItem();
	retriggerItem->text = "Retrigger";
	retriggerItem->eightSeq = eightSeq;
	retriggerItem->gateMode = EightSeq::RETRIGGER;
	menu->addChild(retriggerItem);

	EightSeqGateModeItem *continuousItem = new EightSeqGateModeItem();
	continuousItem->text = "Continuous";
	continuousItem->eightSeq = eightSeq;
	continuousItem->gateMode = EightSeq::CONTINUOUS;
	menu->addChild(continuousItem);

	EightSeqPitchMenuItem *pitchMenuItem = new EightSeqPitchMenuItem();
	pitchMenuItem->text = "Ignore Gate for V/OCT Out";
	pitchMenuItem->eightSeq = eightSeq;
	menu->addChild(pitchMenuItem);

	MenuLabel *spacerLabel2 = new MenuLabel();
	menu->addChild(spacerLabel2);

	MenuLabel *randomModeLabel = new MenuLabel();
	randomModeLabel->text = "Random Button Mode";
	menu->addChild(randomModeLabel);

	EightSeqRandomModeItem *randomItem = new EightSeqRandomModeItem();
	randomItem->text = "Random";
	randomItem->eightSeq = eightSeq;
	randomItem->randomMode = EightSeq::RANDOM;
	menu->addChild(randomItem);

	EightSeqRandomModeItem *randomMinItem = new EightSeqRandomModeItem();
	randomMinItem->text = "First is Minimum";
	randomMinItem->eightSeq = eightSeq;
	randomMinItem->randomMode = EightSeq::FIRST_MIN;
	menu->addChild(randomMinItem);

	EightSeqRandomModeItem *randomMaxItem = new EightSeqRandomModeItem();
	randomMaxItem->text = "First is Maximum";
	randomMaxItem->eightSeq = eightSeq;
	randomMaxItem->randomMode = EightSeq::FIRST_MAX;
	menu->addChild(randomMaxItem);
}

Model *modelEightSeq = createModel<EightSeq, EightSeqWidget>("8Seq");
