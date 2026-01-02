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

	// Traversal pattern modes
	enum PatternMode { ROW_MAJOR, COL_MAJOR, SNAKE_ROWS, SNAKE_COLS };
	PatternMode patternMode = ROW_MAJOR;

	dsp::PulseGenerator gatePulse;

	// Gate pulse length in seconds (for TRIGGER/RETRIGGER modes)
	float gatePulseLenSec = 0.005f;

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
		configInput(RIGHT_INPUT, "Clock");
		configInput(LENGTH_INPUT, "Length");
		configInput(RESET_INPUT, "Reset");
		configInput(ROOT_INPUT, "Root Note");
		configInput(OCTAVE_INPUT, "Octave");
		configInput(SCALE_INPUT, "Scale");
		configInput(VOLT_MAX_INPUT, "Range");
		configInput(RND_GATES_INPUT, "Random Gates");
		configInput(RND_PROBS_INPUT, "Random Probabilities");
		configInput(RND_NOTES_INPUT, "Random Notes");
		
		configOutput(GATES_OUTPUT, "Gate");
		configOutput(CELL_OUTPUT, "V/Oct");
		configOutput(EOC_OUTPUT, "End of Cycle");
		configOutput(PROB_OUTPUT, "Probability");
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

		// gate pulse length
		json_object_set_new(rootJ, "gatePulseLenSec", json_real(gatePulseLenSec));

		// pattern mode
		json_object_set_new(rootJ, "patternMode", json_integer((int)patternMode));

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

		// gate pulse length
		json_t *gatePulseLenSecJ = json_object_get(rootJ, "gatePulseLenSec");
		if (gatePulseLenSecJ) {
			gatePulseLenSec = (float) json_number_value(gatePulseLenSecJ);
			gatePulseLenSec = clampfjw(gatePulseLenSec, 0.001f, 10.0f);
		}

		// pattern mode
		json_t *patternModeJ = json_object_get(rootJ, "patternMode");
		if (patternModeJ) {
			int pm = (int)json_integer_value(patternModeJ);
			pm = clampijw(pm, ROW_MAJOR, SNAKE_COLS);
			patternMode = (PatternMode)pm;
		}
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

		if (rndNotesTrigger.process(inputs[RND_NOTES_INPUT].getVoltage() + params[RND_NOTES_PARAM].getValue())) {
			randomizeNotesOnly();
			params[RND_NOTES_PARAM].setValue(0.f);
		}

		if (rndGatesTrigger.process(inputs[RND_GATES_INPUT].getVoltage() + params[RND_GATES_PARAM].getValue())) {
			randomizeGateStates();
			params[RND_GATES_PARAM].setValue(0.f);
		}

		if (rndProbsTrigger.process(inputs[RND_PROBS_INPUT].getVoltage() + params[RND_PROBS_PARAM].getValue())) {
			randomizeProbsOnly();
			params[RND_PROBS_PARAM].setValue(0.f);
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
		// Light the currently addressed step according to traversal pattern
		int order[8];
		// Build traversal order once per step; small fixed size
		// ROW_MAJOR: 0..7
		order[0]=0; order[1]=1; order[2]=2; order[3]=3; order[4]=4; order[5]=5; order[6]=6; order[7]=7;
		if (patternMode == COL_MAJOR) {
			// columns first: 0,4,1,5,2,6,3,7
			order[0]=0; order[1]=4; order[2]=1; order[3]=5; order[4]=2; order[5]=6; order[6]=3; order[7]=7;
		} else if (patternMode == SNAKE_ROWS) {
			// row0 L->R then row1 R->L: 0,1,2,3,7,6,5,4
			order[0]=0; order[1]=1; order[2]=2; order[3]=3; order[4]=7; order[5]=6; order[6]=5; order[7]=4;
		} else if (patternMode == SNAKE_COLS) {
			// col0 T->B, col1 B->T, ...: 0,4,5,1,2,6,7,3
			order[0]=0; order[1]=4; order[2]=5; order[3]=1; order[4]=2; order[5]=6; order[6]=7; order[7]=3;
		}
		int playIdx = order[index];
		lights[STEPS_LIGHT + playIdx].value = 1.0;
		gatePulse.trigger(gatePulseLenSec);
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
	// Map index through traversal order to get actual cell position
	int order[8];
	order[0]=0; order[1]=1; order[2]=2; order[3]=3; order[4]=4; order[5]=5; order[6]=6; order[7]=7;
	if (patternMode == COL_MAJOR) {
		order[0]=0; order[1]=4; order[2]=1; order[3]=5; order[4]=2; order[5]=6; order[6]=3; order[7]=7;
	} else if (patternMode == SNAKE_ROWS) {
		order[0]=0; order[1]=1; order[2]=2; order[3]=3; order[4]=7; order[5]=6; order[6]=5; order[7]=4;
	} else if (patternMode == SNAKE_COLS) {
		order[0]=0; order[1]=4; order[2]=5; order[3]=1; order[4]=2; order[5]=6; order[6]=7; order[7]=3;
	}
	int playIdx = order[index];
	bool prob = params[CELL_PROB_PARAM + playIdx].getValue() > rndFloat0to1AtClockStep;
	if(!params[PROB_ON_SWITCH_PARAM].getValue()){
		prob = true;
	}

	bool gatesOn = (running && gateState[playIdx] && prob);
	if (gateMode == TRIGGER) gatesOn = gatesOn && pulse;
	else if (gateMode == RETRIGGER) gatesOn = gatesOn && !pulse;

	// Outputs
	if(gatesOn || ignoreGateOnPitchOut)	{
		outputs[CELL_OUTPUT].setVoltage(closestVoltageInScaleWrapper(params[CELL_NOTE_PARAM + playIdx].getValue()));
	}
	outputs[GATES_OUTPUT].setVoltage(gatesOn ? 10.0 : 0.0);
	outputs[PROB_OUTPUT].setVoltage(rescalefjw(params[CELL_PROB_PARAM + playIdx].getValue(), 0.0, 1.0, 1.0, 10.0));
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

// Randomize buttons now handled in process() via triggers; use plain TinyButton widgets

EightSeqWidget::EightSeqWidget(EightSeq *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*20, RACK_GRID_HEIGHT);

	setPanel(createPanel(
		asset::plugin(pluginInstance, "res/EightSeq.svg"), 
		asset::plugin(pluginInstance, "res/EightSeq.svg")
	));

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

	addParam(createParam<TinyButton>(Vec(230, paramY+10), module, EightSeq::RND_GATES_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(230, 345), module, EightSeq::RND_GATES_INPUT));

	addParam(createParam<TinyButton>(Vec(255, paramY+10), module, EightSeq::RND_PROBS_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(255, 345), module, EightSeq::RND_PROBS_INPUT));

	addParam(createParam<TinyButton>(Vec(279, paramY+10), module, EightSeq::RND_NOTES_PARAM));
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

struct EightSeqPatternModeItem : MenuItem {
	EightSeq *eightSeq;
	EightSeq::PatternMode patternMode;
	void onAction(const event::Action &e) override {
		eightSeq->patternMode = patternMode;
	}
	void step() override {
		rightText = (eightSeq->patternMode == patternMode) ? "✔" : "";
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

	// Traversal pattern menu
	MenuLabel *patternLabel = new MenuLabel();
	patternLabel->text = "Traversal Pattern";
	menu->addChild(patternLabel);

	EightSeqPatternModeItem *rowMajorItem = new EightSeqPatternModeItem();
	rowMajorItem->text = "Row-Major";
	rowMajorItem->eightSeq = eightSeq;
	rowMajorItem->patternMode = EightSeq::ROW_MAJOR;
	menu->addChild(rowMajorItem);

	EightSeqPatternModeItem *colMajorItem = new EightSeqPatternModeItem();
	colMajorItem->text = "Col-Major";
	colMajorItem->eightSeq = eightSeq;
	colMajorItem->patternMode = EightSeq::COL_MAJOR;
	menu->addChild(colMajorItem);

	EightSeqPatternModeItem *snakeRowsItem = new EightSeqPatternModeItem();
	snakeRowsItem->text = "Snake Rows";
	snakeRowsItem->eightSeq = eightSeq;
	snakeRowsItem->patternMode = EightSeq::SNAKE_ROWS;
	menu->addChild(snakeRowsItem);

	EightSeqPatternModeItem *snakeColsItem = new EightSeqPatternModeItem();
	snakeColsItem->text = "Snake Cols";
	snakeColsItem->eightSeq = eightSeq;
	snakeColsItem->patternMode = EightSeq::SNAKE_COLS;
	menu->addChild(snakeColsItem);

	// Gate pulse length slider
	MenuLabel *spacerLabelGate = new MenuLabel();
	menu->addChild(spacerLabelGate);
	MenuLabel *gatePulseLabel = new MenuLabel();
	gatePulseLabel->text = "Gate Length";
	menu->addChild(gatePulseLabel);

	GatePulseMsSlider* gateSlider = new GatePulseMsSlider();
	{
		auto qp = static_cast<GatePulseMsQuantity*>(gateSlider->quantity);
		qp->getSeconds = [eightSeq](){ return eightSeq->gatePulseLenSec; };
		qp->setSeconds = [eightSeq](float v){ eightSeq->gatePulseLenSec = v; };
		qp->defaultSeconds = 0.005f;
		qp->label = "Gate Length";
	}
	gateSlider->box.size.x = 175.0f;
	menu->addChild(gateSlider);
}

Model *modelEightSeq = createModel<EightSeq, EightSeqWidget>("8Seq");
