#include "JWModules.hpp"

struct DivSeq : Module,QuantizeUtils {
	enum ParamIds {
		CELL_NOTE_PARAM,
		CELL_GATE_PARAM = CELL_NOTE_PARAM + 16,
		CELL_DIV_PARAM = CELL_GATE_PARAM + 16,
		RND_NOTES_PARAM = CELL_DIV_PARAM + 16,
		ROOT_NOTE_PARAM,
		SCALE_PARAM,
		RND_GATES_PARAM,
		VOLT_MAX_PARAM,
		OCTAVE_PARAM,
		RND_DIVS_PARAM,
		LENGTH_KNOB_PARAM,
		RESET_PARAM,
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
		RND_DIVS_INPUT,
		LENGTH_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		GATES_OUTPUT,
		CELL_OUTPUT,
		EOC_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		GATES_LIGHT,
		STEPS_LIGHT = GATES_LIGHT + 16,
		DIV_LIGHT = STEPS_LIGHT + 16,
		NUM_LIGHTS = DIV_LIGHT + 16
	};

	dsp::SchmittTrigger rightTrigger;
	dsp::SchmittTrigger runningTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::SchmittTrigger rndNotesTrigger;
	dsp::SchmittTrigger rndGatesTrigger;
	dsp::SchmittTrigger rndDivsTrigger;
	dsp::SchmittTrigger gateTriggers[16];

	int index = 0;
	bool playDiv = false;
	float phase = 0.0;
	float noteParamMax = 10.0;
	bool gateState[16] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
	bool running = true;
	bool ignoreGateOnPitchOut = false;
	bool resetMode = false;
	float rndFloat0to1AtClockStep = random::uniform();
	int counters[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int divMax = 64;
	bool hitEnd = false;

	enum GateMode { TRIGGER, RETRIGGER, CONTINUOUS };
	GateMode gateMode = TRIGGER;

	enum RandomMode { RANDOM, FIRST_MIN, FIRST_MAX };
	RandomMode randomMode = RANDOM;

	dsp::PulseGenerator gatePulse;

	DivSeq() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ROOT_NOTE_PARAM, 0.0, QuantizeUtils::NUM_NOTES-1, QuantizeUtils::NOTE_C, "Root Note");
		configParam(SCALE_PARAM, 0.0, QuantizeUtils::NUM_SCALES-1, QuantizeUtils::MINOR, "Scale");
		configParam(LENGTH_KNOB_PARAM, 1.0, 16.0, 16.0, "Length");
		configParam(RND_GATES_PARAM, 0.0, 1.0, 0.0, "Random Gates (Shift + Click to Init Defaults)");
		configParam(RND_NOTES_PARAM, 0.0, 1.0, 0.0, "Random Notes\n(Shift + Click to Init Defaults)");
		configParam(RND_DIVS_PARAM, 0.0, 1.0, 0.0, "Random Divisions\n(Shift + Click to Init Defaults)");
		configParam(VOLT_MAX_PARAM, 0.0, 10.0, 2.0, "Range");
		configParam(OCTAVE_PARAM, -5.0, 7.0, -1.0, "Octave");
		for (int y = 0; y < 4; y++) {
			for (int x = 0; x < 4; x++) {
				int idx = (x+(y*4));
				configParam(CELL_NOTE_PARAM + idx, 0.0, noteParamMax, 3.0, "Voltage");
				configParam(CELL_GATE_PARAM + idx, 0.0, 1.0, 0.0, "Gate");
				configParam(CELL_DIV_PARAM + idx, 1.0, divMax, 1.0, "Division");
			}
		}
		configInput(RIGHT_INPUT, "Clock");
		configInput(LENGTH_INPUT, "Length");
		configInput(RESET_INPUT, "Reset");
		configInput(ROOT_INPUT, "Root Note");
		configInput(OCTAVE_INPUT, "Octave");
		configInput(SCALE_INPUT, "Scale");
		configInput(VOLT_MAX_INPUT, "Range");
		configInput(RND_GATES_INPUT, "Random Gates");
		configInput(RND_DIVS_INPUT, "Random Divisions");
		configInput(RND_NOTES_INPUT, "Random Notes");
		
		configOutput(GATES_OUTPUT, "Gate");
		configOutput(CELL_OUTPUT, "V/Oct");
		configOutput(EOC_OUTPUT, "End of Cycle");
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

		// randomMode
		json_t *randomModeJ = json_object_get(rootJ, "randomMode");
		if (randomModeJ)
			randomMode = (RandomMode)json_integer_value(randomModeJ);
	}

	void onReset() override {
		index = 0;
		resetMode = true;
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
		int firstKnobVal = params[CELL_NOTE_PARAM].getValue();
		float firstKnobMaxVal = noteParamMax;
		for (int i = 0; i < 16; i++) {
			if (randomMode == DivSeq::FIRST_MIN) {
				if(i != 0){
					params[CELL_NOTE_PARAM + i].setValue(firstKnobVal + (random::uniform() * (firstKnobMaxVal - firstKnobVal)));
				}
			} else if (randomMode == DivSeq::FIRST_MAX) {
				if(i != 0){
					params[CELL_NOTE_PARAM + i].setValue(random::uniform() * firstKnobVal);
				}
			} else {
				params[CELL_NOTE_PARAM + i].setValue(getOneRandomNote());
			}
		}
	}

	void randomizeDivsOnly(){
		int firstKnobVal = params[CELL_DIV_PARAM].getValue();
		float firstKnobMaxVal = divMax;

		for (int i = 0; i < 16; i++) {
				if (randomMode == DivSeq::FIRST_MIN) {
					if(i != 0){
						params[CELL_DIV_PARAM + i].setValue((int)(firstKnobVal + (random::uniform() * (firstKnobMaxVal - firstKnobVal))));
					}
				} else if (randomMode == DivSeq::FIRST_MAX) {
					if(i != 0){
						params[CELL_DIV_PARAM + i].setValue((int)(random::uniform()*firstKnobVal+1));
					}
				} else {
					params[CELL_DIV_PARAM + i].setValue((int)(random::uniform()*64+1));
				}
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
void DivSeq::process(const ProcessArgs &args) {
	const float lightLambda = 0.10;
	// Run
	
	bool nextStep = false;
	if (resetTrigger.process(inputs[RESET_INPUT].getVoltage()) || resetTrigger.process(params[RESET_PARAM].getValue())) {
		resetMode = true;
	}

	int inputOffset = int(rescalefjw(inputs[LENGTH_INPUT].getVoltage(), 0, 10.0, 0.0, 15.0));
	int len = clampijw(params[LENGTH_KNOB_PARAM].getValue() + inputOffset, 1.0, 16.0);
	if(running){

		if (rndNotesTrigger.process(inputs[RND_NOTES_INPUT].getVoltage())) {
			randomizeNotesOnly();
		}

		if (rndGatesTrigger.process(inputs[RND_GATES_INPUT].getVoltage())) {
			randomizeGateStates();
		}

		if (rndDivsTrigger.process(inputs[RND_DIVS_INPUT].getVoltage())) {
			randomizeDivsOnly();
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
			for (int i = 0; i < 16; i++) {
				counters[i] = 0;
			}
		}
		rndFloat0to1AtClockStep = random::uniform();
		lights[STEPS_LIGHT + index].value = 1.0;
		gatePulse.trigger(1e-1);

		counters[index]++;
		int playDivInt = params[CELL_DIV_PARAM + index].getValue();
		playDiv = counters[index] == playDivInt;
		if(counters[index] >= playDivInt){
			counters[index] = 0;
		}
	}

	bool pulse = gatePulse.process(1.0 / args.sampleRate);

	//////////////////////////////////////////////////////////////////////////////////////////	
	// MAIN XY OUT
	//////////////////////////////////////////////////////////////////////////////////////////	

	// Gate buttons
	for (int i = 0; i < 16; i++) {
		if (gateTriggers[i].process(params[CELL_GATE_PARAM + i].getValue())) {
			gateState[i] = !gateState[i];
		}

		if(lights[STEPS_LIGHT + i].value > 0){ 
			lights[STEPS_LIGHT + i].value -= lights[STEPS_LIGHT + i].value / lightLambda / args.sampleRate; 
		}
		lights[DIV_LIGHT + i].value = counters[i] / params[CELL_DIV_PARAM + i].getValue();
		lights[GATES_LIGHT + i].value = (int)gateState[i];
	}

	bool gatesOn = (running && gateState[index] && playDiv);
	if (gateMode == TRIGGER) gatesOn = gatesOn && pulse;
	else if (gateMode == RETRIGGER) gatesOn = gatesOn && !pulse;

	// Outputs
	if(gatesOn || ignoreGateOnPitchOut)	{
		outputs[CELL_OUTPUT].setVoltage(closestVoltageInScaleWrapper(params[CELL_NOTE_PARAM + index].getValue()));
	}
	outputs[GATES_OUTPUT].setVoltage(gatesOn ? 10.0 : 0.0);
	outputs[EOC_OUTPUT].setVoltage((pulse && hitEnd && index == 0) ? 10.0 : 0.0);
	if(index == len-1){
		hitEnd = true;
	}
}

struct DivSeqWidget : ModuleWidget {
	std::vector<ParamWidget*> seqKnobs;
	std::vector<ParamWidget*> divKnobs;
	std::vector<ParamWidget*> gateButtons;
	DivSeqWidget(DivSeq *module);
	~DivSeqWidget(){ 
		seqKnobs.clear(); 
		divKnobs.clear(); 
		gateButtons.clear(); 
	}
	void appendContextMenu(Menu *menu) override;
};

struct RandomizeNotes16SeqOnlyButton : TinyButton {
	void onButton(const event::Button &e) override {
		TinyButton::onButton(e);
		if(e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT){
			DivSeqWidget *wid = this->getAncestorOfType<DivSeqWidget>();
			DivSeq *mod = dynamic_cast<DivSeq*>(wid->module);
			float firstKnobVal = wid->seqKnobs[0]->getParamQuantity()->getDisplayValue();
			float firstKnobMaxVal = mod->noteParamMax;
			bool shiftDown = (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT;
			for (int i = 0; i < 16; i++) {
				if (mod->randomMode == DivSeq::FIRST_MIN) {
					if(i != 0){
						wid->seqKnobs[i]->getParamQuantity()->setValue(firstKnobVal + (random::uniform() * (firstKnobMaxVal - firstKnobVal)));
					}
				} else if (shiftDown) {
					wid->seqKnobs[i]->getParamQuantity()->setValue(3);
				} else if (mod->randomMode == DivSeq::FIRST_MAX) {
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

struct RandomizeDivs16SeqOnlyButton : TinyButton {
	void onButton(const event::Button &e) override {
		TinyButton::onButton(e);
		if(e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT){
			DivSeqWidget *wid = this->getAncestorOfType<DivSeqWidget>();
			DivSeq *mod = dynamic_cast<DivSeq*>(wid->module);
			int firstKnobVal = wid->divKnobs[0]->getParamQuantity()->getDisplayValue();
			float firstKnobMaxVal = mod->divMax;
			bool shiftDown = (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT;
			for (int i = 0; i < 16; i++) {
				if (mod->randomMode == DivSeq::FIRST_MIN) {
					if(i != 0){
						wid->divKnobs[i]->getParamQuantity()->setValue((int)(firstKnobVal + (random::uniform() * (firstKnobMaxVal - firstKnobVal))));
					}
				} else if (shiftDown) {
					wid->divKnobs[i]->getParamQuantity()->setValue(1);
				} else if (mod->randomMode == DivSeq::FIRST_MAX) {
					if(i != 0){
						wid->divKnobs[i]->getParamQuantity()->setValue((int)(random::uniform()*firstKnobVal+1));
					}
				} else {
					wid->divKnobs[i]->getParamQuantity()->setValue((int)(random::uniform()*64+1));
				}
			}
		}
	}
};

struct RandomizeGates16SeqOnlyButton : TinyButton {
	void onButton(const event::Button &e) override {
		TinyButton::onButton(e);
		if(e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT){
			DivSeqWidget *gsw = this->getAncestorOfType<DivSeqWidget>();
			DivSeq *gs = dynamic_cast<DivSeq*>(gsw->module);
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

DivSeqWidget::DivSeqWidget(DivSeq *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*20, RACK_GRID_HEIGHT);

	setPanel(createPanel(
		asset::plugin(pluginInstance, "res/DivSeq.svg"), 
		asset::plugin(pluginInstance, "res/dark/DivSeq.svg")
	));

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	///// CLOCK /////
	addInput(createInput<PJ301MPort>(Vec(102, 26), module, DivSeq::RIGHT_INPUT));

	addParam(createParam<JwSmallSnapKnob>(Vec(159, 26), module, DivSeq::LENGTH_KNOB_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(188, 31), module, DivSeq::LENGTH_INPUT));

	///// RESET /////
	addInput(createInput<PJ301MPort>(Vec(228, 26), module, DivSeq::RESET_INPUT));
	addParam(createParam<TinyButton>(Vec(257, 31), module, DivSeq::RESET_PARAM));

	///// NOTE AND SCALE CONTROLS /////
	float paramY = 313;
	NoteKnob *noteKnob = dynamic_cast<NoteKnob*>(createParam<NoteKnob>(Vec(70, paramY), module, DivSeq::ROOT_NOTE_PARAM));
	CenteredLabel* const noteLabel = new CenteredLabel;
	noteLabel->box.pos = Vec(41, 175);
	noteLabel->text = "C";
	noteKnob->connectLabel(noteLabel, module);
	addChild(noteLabel);
	addParam(noteKnob);
	addInput(createInput<TinyPJ301MPort>(Vec(76, 355), module, DivSeq::ROOT_INPUT));

	addParam(createParam<JwSmallSnapKnob>(Vec(111, paramY), module, DivSeq::OCTAVE_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(117, 355), module, DivSeq::OCTAVE_INPUT));

	ScaleKnob *scaleKnob = dynamic_cast<ScaleKnob*>(createParam<ScaleKnob>(Vec(150, paramY), module, DivSeq::SCALE_PARAM));
	CenteredLabel* const scaleLabel = new CenteredLabel;
	scaleLabel->box.pos = Vec(81, 175);
	scaleLabel->text = "Minor";
	scaleKnob->connectLabel(scaleLabel, module);
	addChild(scaleLabel);
	addParam(scaleKnob);
	addInput(createInput<TinyPJ301MPort>(Vec(155, 355), module, DivSeq::SCALE_INPUT));


	addParam(createParam<JwSmallSnapKnob>(Vec(189, paramY), module, DivSeq::VOLT_MAX_PARAM));//RANGE
	addInput(createInput<TinyPJ301MPort>(Vec(194, 345), module, DivSeq::VOLT_MAX_INPUT));//RANGE

	addParam(createParam<RandomizeGates16SeqOnlyButton>(Vec(230, paramY+10), module, DivSeq::RND_GATES_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(230, 345), module, DivSeq::RND_GATES_INPUT));

	addParam(createParam<RandomizeDivs16SeqOnlyButton>(Vec(255, paramY+10), module, DivSeq::RND_DIVS_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(255, 345), module, DivSeq::RND_DIVS_INPUT));

	addParam(createParam<RandomizeNotes16SeqOnlyButton>(Vec(279, paramY+10), module, DivSeq::RND_NOTES_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(279, 345), module, DivSeq::RND_NOTES_INPUT));

	//// MAIN SEQUENCER KNOBS ////
	int boxSizeX = 60;
	int boxSizeY = 61;
	for (int y = 0; y < 4; y++) {
		for (int x = 0; x < 4; x++) {
			int knobX = x * boxSizeX + 60;
			int knobY = y * boxSizeY + 80;
			int idx = (x+(y*4));

			//maybe someday put note labels in each cell
			// float noteParamMax = 0;
			// if(module != NULL){
			// 	noteParamMax = module->noteParamMax;
			// }
			ParamWidget *cellNoteKnob = createParam<SmallWhiteKnob>(Vec(knobX-2, knobY), module, DivSeq::CELL_NOTE_PARAM + idx);
			addParam(cellNoteKnob);
			seqKnobs.push_back(cellNoteKnob);

			ParamWidget *cellDivKnob = createParam<JwTinyGraySnapKnob>(Vec(knobX+27, knobY+7), module, DivSeq::CELL_DIV_PARAM + idx);
			addParam(cellDivKnob);
			divKnobs.push_back(cellDivKnob);

			ParamWidget *cellGateButton = createParam<LEDButton>(Vec(knobX+22, knobY-15), module, DivSeq::CELL_GATE_PARAM + idx);
			addParam(cellGateButton);
			gateButtons.push_back(cellGateButton);

			addChild(createLight<MediumLight<MyYellowValueLight>>(Vec(knobX+5, knobY-13.6), module, DivSeq::STEPS_LIGHT + idx));
			addChild(createLight<LargeLight<MyBlueValueLight>>(Vec(knobX+23.5, knobY-13.6), module, DivSeq::GATES_LIGHT + idx));
			addChild(createLight<MediumLight<MyBlueValueLight>>(Vec(knobX+26.5, knobY-10.5), module, DivSeq::DIV_LIGHT + idx));
		}
	}

	///// OUTPUTS /////
	addOutput(createOutput<PJ301MPort>(Vec(14, 110), module, DivSeq::GATES_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(14, 165), module, DivSeq::CELL_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(14, 225), module, DivSeq::EOC_OUTPUT));
}

struct DivSeqPitchMenuItem : MenuItem {
	DivSeq *divSeq;
	void onAction(const event::Action &e) override {
		divSeq->ignoreGateOnPitchOut = !divSeq->ignoreGateOnPitchOut;
	}
	void step() override {
		rightText = (divSeq->ignoreGateOnPitchOut) ? "✔" : "";
		MenuItem::step();
	}
};

struct DivSeqGateModeItem : MenuItem {
	DivSeq *divSeq;
	DivSeq::GateMode gateMode;
	void onAction(const event::Action &e) override {
		divSeq->gateMode = gateMode;
	}
	void step() override {
		rightText = (divSeq->gateMode == gateMode) ? "✔" : "";
		MenuItem::step();
	}
};

struct DivSeqRandomModeItem : MenuItem {
	DivSeq *divSeq;
	DivSeq::RandomMode randomMode;
	void onAction(const event::Action &e) override {
		divSeq->randomMode = randomMode;
	}
	void step() override {
		rightText = (divSeq->randomMode == randomMode) ? "✔" : "";
		MenuItem::step();
	}
};

void DivSeqWidget::appendContextMenu(Menu *menu) {
	MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);

	DivSeq *divSeq = dynamic_cast<DivSeq*>(module);
	assert(divSeq);

	MenuLabel *modeLabel = new MenuLabel();
	modeLabel->text = "Gate Mode";
	menu->addChild(modeLabel);

	DivSeqGateModeItem *triggerItem = new DivSeqGateModeItem();
	triggerItem->text = "Trigger";
	triggerItem->divSeq = divSeq;
	triggerItem->gateMode = DivSeq::TRIGGER;
	menu->addChild(triggerItem);

	DivSeqGateModeItem *retriggerItem = new DivSeqGateModeItem();
	retriggerItem->text = "Retrigger";
	retriggerItem->divSeq = divSeq;
	retriggerItem->gateMode = DivSeq::RETRIGGER;
	menu->addChild(retriggerItem);

	DivSeqGateModeItem *continuousItem = new DivSeqGateModeItem();
	continuousItem->text = "Continuous";
	continuousItem->divSeq = divSeq;
	continuousItem->gateMode = DivSeq::CONTINUOUS;
	menu->addChild(continuousItem);

	DivSeqPitchMenuItem *pitchMenuItem = new DivSeqPitchMenuItem();
	pitchMenuItem->text = "Ignore Gate for V/OCT Out";
	pitchMenuItem->divSeq = divSeq;
	menu->addChild(pitchMenuItem);

	MenuLabel *spacerLabel2 = new MenuLabel();
	menu->addChild(spacerLabel2);

	MenuLabel *randomModeLabel = new MenuLabel();
	randomModeLabel->text = "Random Button Mode";
	menu->addChild(randomModeLabel);

	DivSeqRandomModeItem *randomItem = new DivSeqRandomModeItem();
	randomItem->text = "Random";
	randomItem->divSeq = divSeq;
	randomItem->randomMode = DivSeq::RANDOM;
	menu->addChild(randomItem);

	DivSeqRandomModeItem *randomMinItem = new DivSeqRandomModeItem();
	randomMinItem->text = "First is Minimum";
	randomMinItem->divSeq = divSeq;
	randomMinItem->randomMode = DivSeq::FIRST_MIN;
	menu->addChild(randomMinItem);

	DivSeqRandomModeItem *randomMaxItem = new DivSeqRandomModeItem();
	randomMaxItem->text = "First is Maximum";
	randomMaxItem->divSeq = divSeq;
	randomMaxItem->randomMode = DivSeq::FIRST_MAX;
	menu->addChild(randomMaxItem);
}

Model *modelDivSeq = createModel<DivSeq, DivSeqWidget>("DivSeq");
