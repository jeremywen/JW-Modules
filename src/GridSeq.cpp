#include "JWModules.hpp"
#include "dsp/digital.hpp"

struct GridSeq : Module {
	enum ParamIds {
		RUN_PARAM,
		CLOCK_PARAM,
		RESET_PARAM,
		CELL_PARAM,
		GATE_PARAM = CELL_PARAM + 16,
		RND_NOTES_INPUT = GATE_PARAM + 16,
		ROOT_NOTE_PARAM,
		SCALE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		EXT_CLOCK_INPUT,
		RESET_INPUT,
		RIGHT_INPUT, LEFT_INPUT, DOWN_INPUT, UP_INPUT,
		REPEAT_INPUT,
		RND_DIR_INPUT,		
		NUM_INPUTS
	};
	enum OutputIds {
		GATES_OUTPUT,
		CELL_OUTPUT,
		NUM_OUTPUTS
	};

	SchmittTrigger rightTrigger;
	SchmittTrigger leftTrigger;
	SchmittTrigger downTrigger;
	SchmittTrigger upTrigger;

	SchmittTrigger repeatTrigger;
	SchmittTrigger rndPosTrigger;

	SchmittTrigger runningTrigger;
	SchmittTrigger resetTrigger;
	SchmittTrigger rndScaleTrigger;
	SchmittTrigger gateTriggers[16];

	int index = 0;
	int posX = 0;
	int posY = 0;
	float phase = 0.0;
	bool gateState[16] = {};
	bool running = true;

	enum GateMode { TRIGGER, RETRIGGER, CONTINUOUS };
	GateMode gateMode = TRIGGER;
	PulseGenerator gatePulse;

	// Lights
	float resetLight = 0.0;
	float rndNotesLight = 0.0;
	float runningLight = 0.0;
	float stepLights[16] = {};
	float gateLights[16] = {};

	GridSeq() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {}
	void step();

	json_t *toJson() {
		json_t *rootJ = json_object();

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));

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

	void fromJson(json_t *rootJ) {
		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);

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

	void initialize() {
		for (int i = 0; i < 16; i++) {
			gateState[i] = true;
		}
	}

	void randomize() {
		for (int i = 0; i < 16; i++) {
			gateState[i] = (randomf() > 0.25);
		}
	}
};

void GridSeq::step() {
	const float lightLambda = 0.075;
	// Run
	if (runningTrigger.process(params[RUN_PARAM].value)) {
		running = !running;
	}
	runningLight = running ? 1.0 : 0.0;

	bool nextStep = false;

	// Reset
	if (resetTrigger.process(params[RESET_PARAM].value + inputs[RESET_INPUT].value)) {
		phase = 0.0;
		posX = 0;
		posY = 0;
		nextStep = true;
		resetLight = 1.0;
	}

	if(running){
		if (repeatTrigger.process(inputs[REPEAT_INPUT].value)) {
			nextStep = true;
		} 
		
		if (rndPosTrigger.process(inputs[RND_DIR_INPUT].value)) {
			nextStep = true;
			switch(int(4 * randomf())){
				case 0:posX = posX == 3 ? 0 : posX + 1;break;
				case 1:posX = posX == 0 ? 3 : posX - 1;break;
				case 2:posY = posY == 3 ? 0 : posY + 1;break;
				case 3:posY = posY == 0 ? 3 : posY - 1;break;
			}
		} 

		if (rightTrigger.process(inputs[RIGHT_INPUT].value)) {
			nextStep = true;
			posX = posX == 3 ? 0 : posX + 1;
		} 
		if (leftTrigger.process(inputs[LEFT_INPUT].value)) {
			nextStep = true;
			posX = posX == 0 ? 3 : posX - 1;
		} 
		if (downTrigger.process(inputs[DOWN_INPUT].value)) {
			nextStep = true;
			posY = posY == 3 ? 0 : posY + 1;
		} 
		if (upTrigger.process(inputs[UP_INPUT].value)) {
			nextStep = true;
			posY = posY == 0 ? 3 : posY - 1;
		}
	}
	
	if (nextStep) {
		index = posX + (posY * 4);
		stepLights[index] = 1.0;
		gatePulse.trigger(1e-3);
	}

	resetLight -= resetLight / lightLambda / gSampleRate;
	bool pulse = gatePulse.process(1.0 / gSampleRate);

	// Gate buttons
	for (int i = 0; i < 16; i++) {
		if (gateTriggers[i].process(params[GATE_PARAM + i].value)) {
			gateState[i] = !gateState[i];
		}
		bool gateOn = (running && i == index && gateState[i]);
		if (gateMode == TRIGGER)
			gateOn = gateOn && pulse;
		else if (gateMode == RETRIGGER)
			gateOn = gateOn && !pulse;

		stepLights[i] -= stepLights[i] / lightLambda / gSampleRate;
		gateLights[i] = gateState[i] ? 1.0 - stepLights[i] : stepLights[i];
	}

	// Cells
	bool gatesOn = (running && gateState[index]);
	if (gateMode == TRIGGER)
		gatesOn = gatesOn && pulse;
	else if (gateMode == RETRIGGER)
		gatesOn = gatesOn && !pulse;

	// Outputs
	float cellVal = params[CELL_PARAM + index].value;
//TODO QUANTIZE TO SCALE!!!!!!!!!!!
	outputs[CELL_OUTPUT].value = cellVal;
	outputs[GATES_OUTPUT].value = gatesOn ? 10.0 : 0.0;
}

struct NoteKnob : SmallWhiteKnob {
	std::string formatCurrentValue() override {
		switch(int(value)){
			case GridSeqWidget::NOTE_C:       return "C";
			case GridSeqWidget::NOTE_C_SHARP: return "C#";
			case GridSeqWidget::NOTE_D:       return "D";
			case GridSeqWidget::NOTE_D_SHARP: return "D#";
			case GridSeqWidget::NOTE_E:       return "E";
			case GridSeqWidget::NOTE_F:       return "F";
			case GridSeqWidget::NOTE_F_SHARP: return "F#";
			case GridSeqWidget::NOTE_G:       return "G";
			case GridSeqWidget::NOTE_G_SHARP: return "G#";
			case GridSeqWidget::NOTE_A:       return "A";
			case GridSeqWidget::NOTE_A_SHARP: return "A#";
			case GridSeqWidget::NOTE_B:       return "B";
			default: return "";
		}
	}
};

struct ScaleKnob : NoteKnob {
	std::string formatCurrentValue() override {
		switch(int(value)){
			case GridSeqWidget::AEOLIAN:        return "Aeolian";
			case GridSeqWidget::BLUES:          return "Blues";
			case GridSeqWidget::CHROMATIC:      return "Chromatic";
			case GridSeqWidget::DIATONIC_MINOR: return "Diatonic Minor";
			case GridSeqWidget::DORIAN:         return "Dorian";
			case GridSeqWidget::HARMONIC_MINOR: return "Harmonic Minor";
			case GridSeqWidget::INDIAN:         return "Indian";
			case GridSeqWidget::LOCRIAN:        return "Locrian";
			case GridSeqWidget::LYDIAN:         return "Lydian";
			case GridSeqWidget::MAJOR:          return "Major";
			case GridSeqWidget::MELODIC_MINOR:  return "Melodic Minor";
			case GridSeqWidget::MINOR:          return "Minor";
			case GridSeqWidget::MIXOLYDIAN:     return "Mixolydian";
			case GridSeqWidget::NATURAL_MINOR:  return "Natural Minor";
			case GridSeqWidget::PENTATONIC:     return "Pentatonic";
			case GridSeqWidget::PHRYGIAN:       return "Phrygian";
			case GridSeqWidget::TURKISH:        return "Turkish";
			default: return "";
		}
	}
};

struct RandomizeNotesOnlyButton : LEDButton {
	void onMouseUpOpaque(int b){
		GridSeqWidget *gsw = this->getAncestorOfType<GridSeqWidget>();

		int rootNote = dynamic_cast<NoteKnob*>(gsw->noteKnob)->value;
		int curScaleVal = dynamic_cast<ScaleKnob*>(gsw->scaleKnob)->value;
		int *curScaleArr;
		int notesInScale = 0;
		switch(curScaleVal){
			case GridSeqWidget::AEOLIAN:        curScaleArr = gsw->SCALE_AEOLIAN;       notesInScale=LENGTHOF(gsw->SCALE_AEOLIAN); break;
			case GridSeqWidget::BLUES:          curScaleArr = gsw->SCALE_BLUES;         notesInScale=LENGTHOF(gsw->SCALE_BLUES); break;
			case GridSeqWidget::CHROMATIC:      curScaleArr = gsw->SCALE_CHROMATIC;     notesInScale=LENGTHOF(gsw->SCALE_CHROMATIC); break;
			case GridSeqWidget::DIATONIC_MINOR: curScaleArr = gsw->SCALE_DIATONIC_MINOR;notesInScale=LENGTHOF(gsw->SCALE_DIATONIC_MINOR); break;
			case GridSeqWidget::DORIAN:         curScaleArr = gsw->SCALE_DORIAN;        notesInScale=LENGTHOF(gsw->SCALE_DORIAN); break;
			case GridSeqWidget::HARMONIC_MINOR: curScaleArr = gsw->SCALE_HARMONIC_MINOR;notesInScale=LENGTHOF(gsw->SCALE_HARMONIC_MINOR); break;
			case GridSeqWidget::INDIAN:         curScaleArr = gsw->SCALE_INDIAN;        notesInScale=LENGTHOF(gsw->SCALE_INDIAN); break;
			case GridSeqWidget::LOCRIAN:        curScaleArr = gsw->SCALE_LOCRIAN;       notesInScale=LENGTHOF(gsw->SCALE_LOCRIAN); break;
			case GridSeqWidget::LYDIAN:         curScaleArr = gsw->SCALE_LYDIAN;        notesInScale=LENGTHOF(gsw->SCALE_LYDIAN); break;
			case GridSeqWidget::MAJOR:          curScaleArr = gsw->SCALE_MAJOR;         notesInScale=LENGTHOF(gsw->SCALE_MAJOR); break;
			case GridSeqWidget::MELODIC_MINOR:  curScaleArr = gsw->SCALE_MELODIC_MINOR; notesInScale=LENGTHOF(gsw->SCALE_MELODIC_MINOR); break;
			case GridSeqWidget::MINOR:          curScaleArr = gsw->SCALE_MINOR;         notesInScale=LENGTHOF(gsw->SCALE_MINOR); break;
			case GridSeqWidget::MIXOLYDIAN:     curScaleArr = gsw->SCALE_MIXOLYDIAN;    notesInScale=LENGTHOF(gsw->SCALE_MIXOLYDIAN); break;
			case GridSeqWidget::NATURAL_MINOR:  curScaleArr = gsw->SCALE_NATURAL_MINOR; notesInScale=LENGTHOF(gsw->SCALE_NATURAL_MINOR); break;
			case GridSeqWidget::PENTATONIC:     curScaleArr = gsw->SCALE_PENTATONIC;    notesInScale=LENGTHOF(gsw->SCALE_PENTATONIC); break;
			case GridSeqWidget::PHRYGIAN:       curScaleArr = gsw->SCALE_PHRYGIAN;      notesInScale=LENGTHOF(gsw->SCALE_PHRYGIAN); break;
			case GridSeqWidget::TURKISH:        curScaleArr = gsw->SCALE_TURKISH;       notesInScale=LENGTHOF(gsw->SCALE_TURKISH); break;
		}

		for (int i = 0; i < 16; i++) {
			float voltsOut = 0;
			int rndOctaveInVolts = int(5 * randomf());
			voltsOut += rndOctaveInVolts;
			voltsOut += rootNote / 12.0;
			voltsOut += curScaleArr[int(notesInScale * randomf())] / 12.0;
			gsw->seqKnobs[i]->setValue(voltsOut);
		}		

	}
};

GridSeqWidget::GridSeqWidget() {
	GridSeq *module = new GridSeq();
	setModule(module);
	box.size = Vec(15*20, 380);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/GridSeq.svg")));
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 365)));

	///// RUN AND RESET /////
	addParam(createParam<LEDButton>(Vec(23, 90), module, GridSeq::RUN_PARAM, 0.0, 1.0, 0.0));
	addChild(createValueLight<SmallLight<MyBlueValueLight>>(Vec(23+5, 90+5), &module->runningLight));

	addInput(createInput<PJ301MPort>(Vec(20, 160), module, GridSeq::RESET_INPUT));
	addParam(createParam<LEDButton>(Vec(23, 130), module, GridSeq::RESET_PARAM, 0.0, 1.0, 0.0));
	addChild(createValueLight<SmallLight<MyBlueValueLight>>(Vec(23+5, 130+5), &module->resetLight));

	///// DIR CONTROLS /////
	addInput(createInput<PJ301MPort>(Vec(70, 55), module, GridSeq::RIGHT_INPUT));
	addInput(createInput<PJ301MPort>(Vec(103, 55), module, GridSeq::LEFT_INPUT));
	addInput(createInput<PJ301MPort>(Vec(137, 55), module, GridSeq::DOWN_INPUT));
	addInput(createInput<PJ301MPort>(Vec(172, 55), module, GridSeq::UP_INPUT));

	addInput(createInput<PJ301MPort>(Vec(212, 55), module, GridSeq::RND_DIR_INPUT));
	addInput(createInput<PJ301MPort>(Vec(253, 55), module, GridSeq::REPEAT_INPUT));

	///// NOTE AND SCALE CONTROLS /////
	noteKnob = createParam<NoteKnob>(Vec(70, 335), module, GridSeq::ROOT_NOTE_PARAM, 0.0, NUM_NOTES-1, NOTE_C);
	rack::Label* const noteLabel = new rack::Label;
	noteLabel->box.pos = Vec(70+25, 335+5);
	noteLabel->text = "note here";
	dynamic_cast<NoteKnob*>(noteKnob)->connectLabel(noteLabel);
	addChild(noteLabel);
	addParam(noteKnob);

	scaleKnob = createParam<ScaleKnob>(Vec(130, 335), module, GridSeq::SCALE_PARAM, 0.0, NUM_SCALES-1, MINOR);
	rack::Label* const scaleLabel = new rack::Label;
	scaleLabel->box.pos = Vec(130+25, 335+5);
	scaleLabel->text = "scale here";
	dynamic_cast<ScaleKnob*>(scaleKnob)->connectLabel(scaleLabel);
	addChild(scaleLabel);
	addParam(scaleKnob);

	addParam(createParam<RandomizeNotesOnlyButton>(Vec(245, 335), module, GridSeq::RND_NOTES_INPUT, 0.0, 1.0, 0.0));
	addChild(createValueLight<SmallLight<MyBlueValueLight>>(Vec(245+5, 335+5), &module->rndNotesLight));

	//// MAIN SEQUENCER KNOBS ////
	int boxSize = 55;
	for (int y = 0; y < 4; y++) {
		for (int x = 0; x < 4; x++) {
			int knobX = x * boxSize + 76;
			int knobY = y * boxSize + 110;
			int idx = (x+(y*4));
			//TODO someday put note labels in each cell
			ParamWidget *paramWidget = createParam<SmallWhiteKnob>(Vec(knobX, knobY), module, GridSeq::CELL_PARAM + idx, 0.0, 6.0, 0.0);
			addParam(paramWidget);
			seqKnobs.push_back(paramWidget);
			addParam(createParam<LEDButton>(Vec(knobX+22, knobY-15), module, GridSeq::GATE_PARAM + idx, 0.0, 1.0, 0.0));
			addChild(createValueLight<SmallLight<MyBlueValueLight>>(Vec(knobX+27, knobY-10), &module->gateLights[idx]));			
		}
	}

	///// OUTPUTS /////
	addOutput(createOutput<PJ301MPort>(Vec(20, 238), module, GridSeq::GATES_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(20, 299), module, GridSeq::CELL_OUTPUT));
}

GridSeqWidget::~GridSeqWidget(){
	seqKnobs.clear();
}

struct GridSeqGateModeItem : MenuItem {
	GridSeq *gridSeq;
	GridSeq::GateMode gateMode;
	void onAction() {
		gridSeq->gateMode = gateMode;
	}
	void step() {
		rightText = (gridSeq->gateMode == gateMode) ? "✔" : "";
	}
};

Menu *GridSeqWidget::createContextMenu() {
	Menu *menu = ModuleWidget::createContextMenu();

	MenuLabel *spacerLabel = new MenuLabel();
	menu->pushChild(spacerLabel);

	GridSeq *gridSeq = dynamic_cast<GridSeq*>(module);
	assert(gridSeq);

	MenuLabel *modeLabel = new MenuLabel();
	modeLabel->text = "Gate Mode";
	menu->pushChild(modeLabel);

	GridSeqGateModeItem *triggerItem = new GridSeqGateModeItem();
	triggerItem->text = "Trigger";
	triggerItem->gridSeq = gridSeq;
	triggerItem->gateMode = GridSeq::TRIGGER;
	menu->pushChild(triggerItem);

	GridSeqGateModeItem *retriggerItem = new GridSeqGateModeItem();
	retriggerItem->text = "Retrigger";
	retriggerItem->gridSeq = gridSeq;
	retriggerItem->gateMode = GridSeq::RETRIGGER;
	menu->pushChild(retriggerItem);

	GridSeqGateModeItem *continuousItem = new GridSeqGateModeItem();
	continuousItem->text = "Continuous";
	continuousItem->gridSeq = gridSeq;
	continuousItem->gateMode = GridSeq::CONTINUOUS;
	menu->pushChild(continuousItem);

	return menu;
}

