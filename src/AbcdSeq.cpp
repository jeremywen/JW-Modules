#include "JWModules.hpp"

std::string DEFAULT_TEXT = "AAAB";
std::string POSSIBLE_CHARS = "ABCDORSabcdors";

struct AbcdSeq : Module,QuantizeUtils {
	enum ParamIds {
		CELL_NOTE_PARAM,
		CELL_GATE_PARAM = CELL_NOTE_PARAM + 32,
		CELL_VEL_PARAM = CELL_GATE_PARAM + 32,
		RND_CV_PARAM = CELL_VEL_PARAM + 32,
		ROOT_NOTE_PARAM,
        RND_TEXT_PARAM,
		SCALE_PARAM,
		RND_GATES_PARAM,
		VOLT_MAX_PARAM,
		OCTAVE_PARAM,
		RND_VELS_PARAM,
		LENGTH_KNOB_PARAM,
		RESET_PARAM = LENGTH_KNOB_PARAM + 4,
		RND_LENGTHS_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		RESET_INPUT,
		CLOCK_INPUT,
		RND_CV_INPUT,
		RND_GATES_INPUT,
		RND_TEXT_INPUT,
		VOLT_MAX_INPUT,
		ROOT_INPUT,
		SCALE_INPUT,
		OCTAVE_INPUT,
		RND_VELS_INPUT,
		LENGTH_INPUT,
		RND_LENGTHS_INPUT = LENGTH_INPUT + 4,
		NUM_INPUTS
	};
	enum OutputIds {
		GATES_OUTPUT,
		CV_OUTPUT,
		VEL_OUTPUT,
		EOC_OUTPUT,
		A_GATE_OUTPUT,
		B_GATE_OUTPUT,
		C_GATE_OUTPUT,
		D_GATE_OUTPUT,
		A_CV_OUTPUT,
		B_CV_OUTPUT,
		C_CV_OUTPUT,
		D_CV_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		GATES_LIGHT,
		STEPS_LIGHT = GATES_LIGHT + 32,
		VEL_LIGHT = STEPS_LIGHT + 32,
		NUM_LIGHTS = VEL_LIGHT + 32
	};
	dsp::SchmittTrigger rightTrigger;
	dsp::SchmittTrigger runningTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::SchmittTrigger rndNotesTrigger;
	dsp::SchmittTrigger rndTextTrigger;
	dsp::SchmittTrigger rndGatesTrigger;
	dsp::SchmittTrigger rndVelsTrigger;
	dsp::SchmittTrigger rndLengthsTrigger;
	dsp::SchmittTrigger gateTriggers[32];

	std::string text = DEFAULT_TEXT;
	bool dirty = false;
    int col = 0;
    int row = 0;
    int index = 0;
    int charIdx = 0;
	float phase = 0.0;
	float noteParamMax = 10.0;
	bool gateState[32] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
	bool running = true;
	bool ignoreGateOnPitchOut = false;
	bool resetMode = false;
    bool initialRowSet = false;
	bool eocOn = false;
	bool velocityAsProbability = false;
	float clipboard[8] = {3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0};
	float rndNumOnClockTick = 0.0f;

	enum GateMode { TRIGGER, RETRIGGER, CONTINUOUS };
	GateMode gateMode = TRIGGER;
    dsp::PulseGenerator gatePulse;

	AbcdSeq() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ROOT_NOTE_PARAM, 0.0, QuantizeUtils::NUM_NOTES-1, QuantizeUtils::NOTE_C, "Root Note");
		configParam(SCALE_PARAM, 0.0, QuantizeUtils::NUM_SCALES-1, QuantizeUtils::MINOR, "Scale");
		configParam(RND_GATES_PARAM, 0.0, 1.0, 0.0, "Random Gates (Shift + Click to Init Defaults)");
		configParam(RND_CV_PARAM, 0.0, 1.0, 0.0, "Random CV\n(Shift + Click to Init Defaults)");
		configParam(RND_VELS_PARAM, 0.0, 1.0, 0.0, "Random Velocities\n(Shift + Click to Init Defaults)");
		configParam(RND_TEXT_PARAM, 0.0, 1.0, 0.0, "Random Text\n(Shift + Click to Init Defaults)");
		configParam(RND_LENGTHS_PARAM, 0.0, 1.0, 0.0, "Random Lengths\n(Shift + Click to Init Defaults)");
		configParam(VOLT_MAX_PARAM, 0.0, 10.0, 2.0, "Range");
		configParam(OCTAVE_PARAM, -5.0, 7.0, -1.0, "Octave");
		configParam(RESET_PARAM, 0.0, 1.0, 0.0, "Reset");
        int idx = 0;
		for (int y = 0; y < 4; y++) {
			for (int x = 0; x < 8; x++) {
				configParam(CELL_NOTE_PARAM + idx, 0.0, noteParamMax, 3.0, "CV");
				configParam(CELL_GATE_PARAM + idx, 0.0, 1.0, 0.0, "Gate");
				configParam(CELL_VEL_PARAM + idx, 0.0, 10.0, 5.0, "Velocity");
				idx++;
			}
		}
		for (int y = 0; y < 4; y++) {
    		configParam(LENGTH_KNOB_PARAM + y, 1.0, 8.0, 4.0, "Length");
		    configInput(LENGTH_INPUT + y, "Length");
        }
		configInput(CLOCK_INPUT, "Clock");
		configInput(RESET_INPUT, "Reset");
		configInput(ROOT_INPUT, "Root Note");
		configInput(OCTAVE_INPUT, "Octave");
		configInput(SCALE_INPUT, "Scale");
		configInput(VOLT_MAX_INPUT, "Range");
		configInput(RND_GATES_INPUT, "Random Gates");
		configInput(RND_VELS_INPUT, "Random Velocities");
		configInput(RND_CV_INPUT, "Random Notes");
		configInput(RND_TEXT_INPUT, "Random Text");
		configInput(RND_LENGTHS_INPUT, "Random Lengths");
		
		configOutput(GATES_OUTPUT, "Gate");
		configOutput(CV_OUTPUT, "CV");
		configOutput(VEL_OUTPUT, "Velocity");
		configOutput(A_GATE_OUTPUT, "A Gate");
		configOutput(B_GATE_OUTPUT, "B Gate");
		configOutput(C_GATE_OUTPUT, "C Gate");
		configOutput(D_GATE_OUTPUT, "D Gate");
		configOutput(A_CV_OUTPUT, "A CV");
		configOutput(B_CV_OUTPUT, "B CV");
		configOutput(C_CV_OUTPUT, "C CV");
		configOutput(D_CV_OUTPUT, "D CV");
	}

	void process(const ProcessArgs &args) override;

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "running", json_boolean(running));
		json_object_set_new(rootJ, "ignoreGateOnPitchOut", json_boolean(ignoreGateOnPitchOut));
		json_object_set_new(rootJ, "velocityAsProbability", json_boolean(velocityAsProbability));

		// gates
		json_t *gatesJ = json_array();
		for (int i = 0; i < 32; i++) {
			json_t *gateJ = json_integer((int) gateState[i]);
			json_array_append_new(gatesJ, gateJ);
		}
		json_object_set_new(rootJ, "gates", gatesJ);

		// gateMode
		json_t *gateModeJ = json_integer((int) gateMode);
		json_object_set_new(rootJ, "gateMode", gateModeJ);

		json_object_set_new(rootJ, "text", json_stringn(text.c_str(), text.size()));

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);

		json_t *ignoreGateOnPitchOutJ = json_object_get(rootJ, "ignoreGateOnPitchOut");
		if (ignoreGateOnPitchOutJ)
			ignoreGateOnPitchOut = json_is_true(ignoreGateOnPitchOutJ);

		json_t *velocityAsProbabilityJ = json_object_get(rootJ, "velocityAsProbability");
		if (velocityAsProbabilityJ)
			velocityAsProbability = json_is_true(velocityAsProbabilityJ);

		// gates
		json_t *gatesJ = json_object_get(rootJ, "gates");
		if (gatesJ) {
			for (int i = 0; i < 32; i++) {
				json_t *gateJ = json_array_get(gatesJ, i);
				if (gateJ)
					gateState[i] = !!json_integer_value(gateJ);
			}
		}

		// gateMode
		json_t *gateModeJ = json_object_get(rootJ, "gateMode");
		if (gateModeJ)
			gateMode = (GateMode)json_integer_value(gateModeJ);

		json_t* textJ = json_object_get(rootJ, "text");
		if (textJ)
			text = json_string_value(textJ);
		dirty = true;
	}

	void onReset() override {
		text = DEFAULT_TEXT;
		dirty = true;
        charIdx = 0;
        resetRow();
		resetMode = true;
		for (int i = 0; i < 32; i++) {
			gateState[i] = true;
		}
	}

	void onRandomize() override {
		randomizeGateStates();
        randomizeTextOnly();
	}

	void randomizeGateStates() {
		for (int i = 0; i < 32; i++) {
			gateState[i] = (random::uniform() > 0.50);
		}
	}

	float getOneRandomNote(){
		return random::uniform() * noteParamMax;
	}

	void randomizeNotesOnly(){
		for (int i = 0; i < 32; i++) {
            params[CELL_NOTE_PARAM + i].setValue(getOneRandomNote());
		}
	}

	void randomizeVelsOnly(){
		for (int i = 0; i < 32; i++) {
            params[CELL_VEL_PARAM + i].setValue(random::uniform() * 10);
        }
	}

	void randomizeLengthsOnly(){
		for (int i = 0; i < 4; i++) {
			int rndLen = (random::uniform() * 8) + 1;
			params[LENGTH_KNOB_PARAM + i].setValue(rndLen);
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

    int getCurrentRowLength(){
        double lenVolts = clampfjw(inputs[LENGTH_INPUT + row].getVoltage(), 0.0, 10.0);
        int inputOffset = int(rescalefjw(lenVolts, 0, 10.0, 0.0, 7.0));
        int rowLen = clampijw(params[LENGTH_KNOB_PARAM + row].getValue() + inputOffset, 1, 8);
        return rowLen;
    }

    void moveToNextStep(){
		if(text.size() == 0){
			//just default so we don't have to deal with the empty string
			text = DEFAULT_TEXT;
		}
        int rowLen = getCurrentRowLength();
        char currChar = text[charIdx];
        bool goingForward = isupper(currChar);
        bool endOfRow = false;
		eocOn = false;

        if(goingForward){
            if(col+1 < rowLen){
                col++;
            } else {
                endOfRow = true;
            }
        } else {
            if(col > 0){
                col--;
            } else {
                endOfRow = true;
            }
        }
        if(endOfRow){
            char nextChar = 'A';
            bool nextCharGoingForward =  true;
			eocOn = charIdx == (int)text.size() - 1;
			nextChar = text[(charIdx + 1) % text.size()];
			nextCharGoingForward = isupper(nextChar);
			row = getRowForChar(nextChar);
            col = nextCharGoingForward ? 0 : getCurrentRowLength() - 1;
			charIdx++; 
			if(charIdx >= ((int)text.size())){
				charIdx = 0;
				eocOn = true;
				// DEBUG("AAA eocOn=%i, charIdx=%i", eocOn, charIdx);
			}
        }
    }

    void randomizeTextOnly(){
        dirty = true;
        text = "";
        int rndLen = (random::uniform() * 16) + 1;
        for(int i=0;i<rndLen;i++){
            int n = (int)(random::uniform() * POSSIBLE_CHARS.size());
            text += POSSIBLE_CHARS[n];
        }
    }

	void randomizeRowNotes(int rowIdx) {
		if (rowIdx < 0 || rowIdx > 3) {
			return; // Invalid row index
		}
		for (int i = 0; i < 8; i++) {
			int idx = rowIdx * 8 + i;
			params[CELL_NOTE_PARAM + idx].setValue(getOneRandomNote());
		}
	}

	void copyRowNotes(int rowIdx) {
		if (rowIdx < 0 || rowIdx > 3) {
			return; // Invalid row index
		}
		for (int i = 0; i < 8; i++) {
			int idx = rowIdx * 8 + i;
			clipboard[i] = params[CELL_NOTE_PARAM + idx].getValue();
		}
	}
	void pasteRowNotes(int rowIdx) {
		if (rowIdx < 0 || rowIdx > 3) {
			return; // Invalid row index
		}
		for (int i = 0; i < 8; i++) {
			int idx = rowIdx * 8 + i;
			params[CELL_NOTE_PARAM + idx].setValue(clipboard[i]);
		}
	}

    int getNewRandomRow(){
        return (int)(random::uniform() * 4); 
    }

    int getNewRandomOtherRow(){
        int prevRow = row;
        int temp = getNewRandomRow(); 
        while(temp == prevRow){
            temp = getNewRandomRow(); 
        }
        return temp;
    }

    int getRowForChar(char c){
		int prevRow = row;
        switch(c){
            case 'A': case 'a': return 0; break;
            case 'B': case 'b': return 1; break;
            case 'C': case 'c': return 2; break;
            case 'D': case 'd': return 3; break;
            case 'O': case 'o': return getNewRandomOtherRow(); break;
            case 'R': case 'r': return getNewRandomRow(); break;
            case 'S': case 's': return prevRow; break;
        }
        return 0;
    }

    void resetRow(){
        charIdx = 0;
        if(text.size() > 0){
            row = getRowForChar(text[charIdx]);
            col = isupper(text[charIdx]) ? 0 : getCurrentRowLength() - 1;
        } else {
            row = 0;
        }
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// STEP
///////////////////////////////////////////////////////////////////////////////////////////////////
void AbcdSeq::process(const ProcessArgs &args) {
    if(!initialRowSet){
        row = getRowForChar(text[charIdx]);
        initialRowSet = true;
    }

	const float lightLambda = 0.10;	
	bool nextStep = false;
	if (resetTrigger.process(inputs[RESET_INPUT].getVoltage()) || resetTrigger.process(params[RESET_PARAM].getValue())) {
		resetMode = true;
	}

	if(running){
		if (rndTextTrigger.process(inputs[RND_TEXT_INPUT].getVoltage())) {
			randomizeTextOnly();
		}

		if (rndNotesTrigger.process(inputs[RND_CV_INPUT].getVoltage())) {
			randomizeNotesOnly();
		}

		if (rndLengthsTrigger.process(inputs[RND_LENGTHS_INPUT].getVoltage())) {
			randomizeLengthsOnly();
		}

		if (rndGatesTrigger.process(inputs[RND_GATES_INPUT].getVoltage())) {
			randomizeGateStates();
		}

		if (rndVelsTrigger.process(inputs[RND_VELS_INPUT].getVoltage())) {
			randomizeVelsOnly();
		}

		if (rightTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
			nextStep = true;
			moveToNextStep();
		} 
	}
    index = col + row * 8;
	if (nextStep) {
		if(resetMode){
			resetMode = false;
			eocOn = false;
			phase = 0.0;
            resetRow();
            index = col + row * 8;
            // DEBUG("col=%i, index=%i", col, index);
		}
		lights[STEPS_LIGHT + index].value = 1.0;
		gatePulse.trigger(1e-1);
		rndNumOnClockTick = random::uniform();
	}

	bool pulse = gatePulse.process(1.0 / args.sampleRate);

	//////////////////////////////////////////////////////////////////////////////////////////	
	// MAIN XY OUT
	//////////////////////////////////////////////////////////////////////////////////////////	
	
	// Gate buttons
	for (int i = 0; i < 32; i++) {
		if (gateTriggers[i].process(params[CELL_GATE_PARAM + i].getValue())) {
			gateState[i] = !gateState[i];
		}
		
		if(lights[STEPS_LIGHT + i].value > 0){ 
			lights[STEPS_LIGHT + i].value -= lights[STEPS_LIGHT + i].value / lightLambda / args.sampleRate; 
		}
		lights[GATES_LIGHT + i].value = (int)gateState[i];
	}
	
	bool gatesOn = (running && gateState[index]);
	if (velocityAsProbability) {
		float vel = params[CELL_VEL_PARAM + index].getValue() / 10.0;
		gatesOn = gatesOn && rndNumOnClockTick < vel;
	}
	if (gateMode == TRIGGER) gatesOn = gatesOn && pulse;
	else if (gateMode == RETRIGGER) gatesOn = gatesOn && !pulse;

	// Outputs
	if(gatesOn || ignoreGateOnPitchOut)	{
		outputs[CV_OUTPUT].setVoltage(closestVoltageInScaleWrapper(params[CELL_NOTE_PARAM + index].getValue()));
		outputs[A_CV_OUTPUT + row].setVoltage(closestVoltageInScaleWrapper(params[CELL_NOTE_PARAM + index].getValue()));
	}
	outputs[GATES_OUTPUT].setVoltage(gatesOn ? 10.0 : 0.0);
	outputs[VEL_OUTPUT].setVoltage(params[CELL_VEL_PARAM + index].getValue());
	outputs[EOC_OUTPUT].setVoltage((pulse && eocOn) ? 10.0 : 0.0);
	outputs[A_GATE_OUTPUT + row].setVoltage(gatesOn ? 10.0 : 0.0);
};

struct OrderTextField : LedDisplayTextField {
	AbcdSeq* module;

	void step() override {
		LedDisplayTextField::step();
		if (module && module->dirty) {
			setText(module->text);
			module->dirty = false;
        // } else if(!module){
        //     setText(DEFAULT_TEXT);
        }
	}

	void onChange(const ChangeEvent& e) override {
		if (module) {
			module->text = getText();
        }
	}
};

struct AbcdSeqWidget : ModuleWidget {
	std::vector<ParamWidget*> seqKnobs;
	std::vector<ParamWidget*> divKnobs;
	std::vector<ParamWidget*> gateButtons;
	std::vector<ParamWidget*> lengthKnobs;
    OrderTextField* orderTextField;
	AbcdSeqWidget(AbcdSeq *module);
	~AbcdSeqWidget(){ 
		seqKnobs.clear(); 
		divKnobs.clear(); 
		gateButtons.clear(); 
        orderTextField = nullptr;
	}
	void appendContextMenu(Menu *menu) override;
};

struct RandomizeNotesButton : TinyButton {
	void onButton(const event::Button &e) override {
		TinyButton::onButton(e);
		if(e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT){
			AbcdSeqWidget *wid = this->getAncestorOfType<AbcdSeqWidget>();
			AbcdSeq *mod = dynamic_cast<AbcdSeq*>(wid->module);
			bool shiftDown = (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT;
			for (int i = 0; i < 32; i++) {
                if(shiftDown){
                    wid->seqKnobs[i]->getParamQuantity()->setValue(3);
                } else {
                    wid->seqKnobs[i]->getParamQuantity()->setValue(mod->getOneRandomNote());
                }
			}
		}
	}
};

struct RandomizeVelButton : TinyButton {
	void onButton(const event::Button &e) override {
		TinyButton::onButton(e);
		if(e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT){
			AbcdSeqWidget *wid = this->getAncestorOfType<AbcdSeqWidget>();
			// AbcdSeq *mod = dynamic_cast<AbcdSeq*>(wid->module);
			bool shiftDown = (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT;
			for (int i = 0; i < 32; i++) {
                if(shiftDown){
                    wid->divKnobs[i]->getParamQuantity()->setValue(5);
                } else {
                    wid->divKnobs[i]->getParamQuantity()->setValue(random::uniform() * 10);
                }
			}
		}
	}
};

struct RandomizeGatesButton : TinyButton {
	void onButton(const event::Button &e) override {
		TinyButton::onButton(e);
		if(e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT){
			AbcdSeqWidget *gsw = this->getAncestorOfType<AbcdSeqWidget>();
			AbcdSeq *gs = dynamic_cast<AbcdSeq*>(gsw->module);
			for (int i = 0; i < 32; i++) {
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

struct RandomizeTextButton : TinyButton {
	void onButton(const event::Button &e) override {
		TinyButton::onButton(e);
		if(e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT){
			AbcdSeqWidget *sw = this->getAncestorOfType<AbcdSeqWidget>();
			AbcdSeq *s = dynamic_cast<AbcdSeq*>(sw->module);
			bool shiftDown = (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT;
            if(shiftDown){
                sw->orderTextField->setText(DEFAULT_TEXT);
            } else {
			    s->randomizeTextOnly();
            }
		}
	}
};

struct RandomizeLengthsButton : TinyButton {
	void onButton(const event::Button &e) override {
		TinyButton::onButton(e);
		if(e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT){
			AbcdSeqWidget *sw = this->getAncestorOfType<AbcdSeqWidget>();
			AbcdSeq *s = dynamic_cast<AbcdSeq*>(sw->module);
			bool shiftDown = (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT;
			for (int i = 0; i < 4; i++) {
				if (shiftDown) {
					sw->lengthKnobs[i]->getParamQuantity()->setValue(4.0);
				} else {
					s->randomizeLengthsOnly();
				}
			}
		}
	}
};

struct OrderDisplay : LedDisplay {
    OrderTextField* textField;
	void setModule(AbcdSeq* module) {
		textField = createWidget<OrderTextField>(Vec(0, 0));
		textField->text = DEFAULT_TEXT;
		textField->box.size = box.size;
		textField->multiline = false;
		textField->module = module;
		textField->color = nvgRGB(25, 150, 252);
		addChild(textField);
	}
    ~OrderDisplay(){
        textField = nullptr;
    }
};

AbcdSeqWidget::AbcdSeqWidget(AbcdSeq *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*40, RACK_GRID_HEIGHT);

	setPanel(createPanel(
		asset::plugin(pluginInstance, "res/AbcdSeq.svg"), 
		asset::plugin(pluginInstance, "res/dark/AbcdSeq.svg")
	));

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	///// CLOCK /////
	addInput(createInput<PJ301MPort>(Vec(130, 26), module, AbcdSeq::CLOCK_INPUT));

	///// RESET /////
	addInput(createInput<PJ301MPort>(Vec(168, 26), module, AbcdSeq::RESET_INPUT));
	addParam(createParam<TinyButton>(Vec(197, 31), module, AbcdSeq::RESET_PARAM));

    OrderDisplay* orderDisplay = createWidget<OrderDisplay>(Vec(250, 20));
    orderDisplay->box.size = Vec(344, 30);
    orderDisplay->setModule(module);
    orderTextField = orderDisplay->textField;
    addChild(orderDisplay);

	///// NOTE AND SCALE CONTROLS /////
	float paramY = 313;
	NoteKnob *noteKnob = dynamic_cast<NoteKnob*>(createParam<NoteKnob>(Vec(70, paramY), module, AbcdSeq::ROOT_NOTE_PARAM));
	CenteredLabel* const noteLabel = new CenteredLabel;
	noteLabel->box.pos = Vec(41, 175);
	noteLabel->text = "C";
	noteKnob->connectLabel(noteLabel, module);
	addChild(noteLabel);
	addParam(noteKnob);
	addInput(createInput<TinyPJ301MPort>(Vec(76, 355), module, AbcdSeq::ROOT_INPUT));

	addParam(createParam<JwSmallSnapKnob>(Vec(111, paramY), module, AbcdSeq::OCTAVE_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(117, 355), module, AbcdSeq::OCTAVE_INPUT));

	ScaleKnob *scaleKnob = dynamic_cast<ScaleKnob*>(createParam<ScaleKnob>(Vec(150, paramY), module, AbcdSeq::SCALE_PARAM));
	CenteredLabel* const scaleLabel = new CenteredLabel;
	scaleLabel->box.pos = Vec(81, 175);
	scaleLabel->text = "Minor";
	scaleKnob->connectLabel(scaleLabel, module);
	addChild(scaleLabel);
	addParam(scaleKnob);
	addInput(createInput<TinyPJ301MPort>(Vec(155, 355), module, AbcdSeq::SCALE_INPUT));

	addParam(createParam<JwSmallSnapKnob>(Vec(189, paramY), module, AbcdSeq::VOLT_MAX_PARAM));//RANGE
	addInput(createInput<TinyPJ301MPort>(Vec(194, 345), module, AbcdSeq::VOLT_MAX_INPUT));//RANGE

	addParam(createParam<RandomizeGatesButton>(Vec(230, paramY+10), module, AbcdSeq::RND_GATES_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(230, 345), module, AbcdSeq::RND_GATES_INPUT));

	addParam(createParam<RandomizeNotesButton>(Vec(255, paramY+10), module, AbcdSeq::RND_CV_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(255, 345), module, AbcdSeq::RND_CV_INPUT));

	addParam(createParam<RandomizeVelButton>(Vec(279, paramY+10), module, AbcdSeq::RND_VELS_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(279, 345), module, AbcdSeq::RND_VELS_INPUT));

	addParam(createParam<RandomizeTextButton>(Vec(304, paramY+10), module, AbcdSeq::RND_TEXT_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(304, 345), module, AbcdSeq::RND_TEXT_INPUT));

	addParam(createParam<RandomizeLengthsButton>(Vec(329, paramY+10), module, AbcdSeq::RND_LENGTHS_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(329, 345), module, AbcdSeq::RND_LENGTHS_INPUT));

	//// MAIN SEQUENCER KNOBS ////
	double boxSizeX = 61.5;
	double boxSizeY = 61;
    int idx = 0;
	for (int y = 0; y < 4; y++) {
        int knobX = 0;
        int knobY = y * boxSizeY + 80;
		for (int x = 0; x < 8; x++) {
			knobX = x * boxSizeX + 60;

			ParamWidget *cellNoteKnob = createParam<SmallWhiteKnob>(Vec(knobX-2, knobY), module, AbcdSeq::CELL_NOTE_PARAM + idx);
			addParam(cellNoteKnob);
			seqKnobs.push_back(cellNoteKnob);

			ParamWidget *cellDivKnob = createParam<JwTinyGrayKnob>(Vec(knobX+27, knobY+7), module, AbcdSeq::CELL_VEL_PARAM + idx);
			addParam(cellDivKnob);
			divKnobs.push_back(cellDivKnob);

			ParamWidget *cellGateButton = createParam<LEDButton>(Vec(knobX+22, knobY-15), module, AbcdSeq::CELL_GATE_PARAM + idx);
			addParam(cellGateButton);
			gateButtons.push_back(cellGateButton);

			addChild(createLight<MediumLight<MyOrangeValueLight>>(Vec(knobX+5, knobY-13.6), module, AbcdSeq::STEPS_LIGHT + idx));
			addChild(createLight<LargeLight<MyBlueValueLight>>(Vec(knobX+23.5, knobY-13.6), module, AbcdSeq::GATES_LIGHT + idx));
			addChild(createLight<MediumLight<MyBlueValueLight>>(Vec(knobX+26.5, knobY-10.5), module, AbcdSeq::VEL_LIGHT + idx));
            idx++;
        }
        ParamWidget *lengthKnob = createParam<JwSmallSnapKnob>(Vec(knobX + 57, knobY-14), module, AbcdSeq::LENGTH_KNOB_PARAM + y);
		addParam(lengthKnob);
		lengthKnobs.push_back(lengthKnob);
	    addInput(createInput<TinyPJ301MPort>(Vec(knobX + 85, knobY-9), module, AbcdSeq::LENGTH_INPUT + y));

		addOutput(createOutput<TinyPJ301MPort>(Vec(knobX + 55, knobY+16), module, AbcdSeq::A_GATE_OUTPUT + y));
		addOutput(createOutput<TinyPJ301MPort>(Vec(knobX + 75, knobY+16), module, AbcdSeq::A_CV_OUTPUT + y));
	}

	///// OUTPUTS /////
	addOutput(createOutput<PJ301MPort>(Vec(357, paramY+12), module, AbcdSeq::GATES_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(413.5, paramY+12), module, AbcdSeq::CV_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(467.5, paramY+12), module, AbcdSeq::VEL_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(521, paramY+12), module, AbcdSeq::EOC_OUTPUT));
};

struct AbcdSeqPitchMenuItem : MenuItem {
	AbcdSeq *abcdSeq;
	void onAction(const event::Action &e) override {
		abcdSeq->ignoreGateOnPitchOut = !abcdSeq->ignoreGateOnPitchOut;
	}
	void step() override {
		rightText = (abcdSeq->ignoreGateOnPitchOut) ? "✔" : "";
		MenuItem::step();
	}
};

struct AbcdSeqGateModeItem : MenuItem {
	AbcdSeq *abcdSeq;
	AbcdSeq::GateMode gateMode;
	void onAction(const event::Action &e) override {
		abcdSeq->gateMode = gateMode;
	}
	void step() override {
		rightText = (abcdSeq->gateMode == gateMode) ? "✔" : "";
		MenuItem::step();
	}
};

struct AbcdSeqPresetItem : MenuItem {
	AbcdSeq *abcdSeq;
	void onAction(const event::Action &e) override {
		abcdSeq->text = text;
		abcdSeq->dirty = true;
	}
};

struct AbcdSeqRowOrderPresetItem : MenuItem {
	AbcdSeq *abcdSeq;
	int presetIndex;
	void onAction(const event::Action &e) override {
		abcdSeq->resetMode = true;
		switch (presetIndex) {
			case 0: abcdSeq->text = "AbCd"; break;
			case 1: abcdSeq->text = "AAAB"; break;
			case 2: abcdSeq->text = "AAABAAACAAAD"; break;
			case 3: abcdSeq->text = "AaBbCcDd"; break;
			case 4: abcdSeq->text = "OSSS"; break;
		}
		abcdSeq->dirty = true;
	}
};

struct PresetChildMenuItem : MenuItem {
	AbcdSeq *abcdSeq;
	PresetChildMenuItem() {
		rightText = "▸";
		text = "Row Order Text Presets";
	}
	Menu* createChildMenu() override {
		Menu *menu = new Menu;
		AbcdSeqPresetItem *presetMenuItem1 = new AbcdSeqPresetItem();
		presetMenuItem1->text = "AbCd";
		presetMenuItem1->abcdSeq = abcdSeq;
		menu->addChild(presetMenuItem1);
		
		AbcdSeqPresetItem *presetMenuItem2 = new AbcdSeqPresetItem();
		presetMenuItem2->text = "AAAB";
		presetMenuItem2->abcdSeq = abcdSeq;
		menu->addChild(presetMenuItem2);
		
		AbcdSeqPresetItem *presetMenuItem3 = new AbcdSeqPresetItem();
		presetMenuItem3->text = "AAABAAACAAAD";
		presetMenuItem3->abcdSeq = abcdSeq;
		menu->addChild(presetMenuItem3);
		
		AbcdSeqPresetItem *presetMenuItem4 = new AbcdSeqPresetItem();
		presetMenuItem4->text = "AaBbCcDd";
		presetMenuItem4->abcdSeq = abcdSeq;
		menu->addChild(presetMenuItem4);
		
		AbcdSeqPresetItem *presetMenuItem5 = new AbcdSeqPresetItem();
		presetMenuItem5->text = "OSSS";
		presetMenuItem5->abcdSeq = abcdSeq;
		menu->addChild(presetMenuItem5);
		return menu;
	}
};

struct PossibleCharsMenuItem : MenuItem {
	AbcdSeq *abcdSeq;
	PossibleCharsMenuItem() {
		rightText = "▸";
		text = "Possible Characters";
	}
	Menu* createChildMenu() override {
		Menu *menu = new Menu;
		MenuLabel *helpLabel2 = new MenuLabel();
		helpLabel2->text = "A, B, C, D will jump to those rows";
		menu->addChild(helpLabel2);

		MenuLabel *helpLabel3 = new MenuLabel();
		helpLabel3->text = "O will jump to any other row";
		menu->addChild(helpLabel3);

		MenuLabel *helpLabel4 = new MenuLabel();
		helpLabel4->text = "R will jump to any row";
		menu->addChild(helpLabel4);

		MenuLabel *helpLabel5 = new MenuLabel();
		helpLabel5->text = "S will repeat last row";
		menu->addChild(helpLabel5);

		MenuLabel *helpLabel6 = new MenuLabel();
		helpLabel6->text = "Upper case forwards and lower case backwards.";
		menu->addChild(helpLabel6);
		return menu;
	}
};

struct RandomizeRowMenuItem : MenuItem {
	AbcdSeq *abcdSeq;
	int row;
	RandomizeRowMenuItem(int rowIdx) {
		this->row = rowIdx;
	}
	void onAction(const event::Action &e) override {
		if (abcdSeq) {
			abcdSeq->randomizeRowNotes(row);
		}
	}
};

struct RandomizeMenuItem : MenuItem {
	AbcdSeq *abcdSeq;
	RandomizeMenuItem() {
		rightText = "▸";
		text = "Randomize Notes";
	}
	Menu* createChildMenu() override {
		Menu *menu = new Menu;
		RandomizeRowMenuItem *item0 = new RandomizeRowMenuItem(0);
		item0->text = "Row A";
		item0->abcdSeq = abcdSeq;
		menu->addChild(item0);

		RandomizeRowMenuItem *item1 = new RandomizeRowMenuItem(1);
		item1->text = "Row B";
		item1->abcdSeq = abcdSeq;
		menu->addChild(item1);

		RandomizeRowMenuItem *item2 = new RandomizeRowMenuItem(2);
		item2->text = "Row C";
		item2->abcdSeq = abcdSeq;
		menu->addChild(item2);

		RandomizeRowMenuItem *item3 = new RandomizeRowMenuItem(3);
		item3->text = "Row D";
		item3->abcdSeq = abcdSeq;
		menu->addChild(item3);
		return menu;
	}
};

struct CopyRowMenuItem : MenuItem {
	AbcdSeq *abcdSeq;
	int row;
	CopyRowMenuItem(int rowIdx) {
		this->row = rowIdx;
	}
	void onAction(const event::Action &e) override {
		if (abcdSeq) {
			abcdSeq->copyRowNotes(row);
		}
	}
};

struct CopyMenuItem : MenuItem {
	AbcdSeq *abcdSeq;
	CopyMenuItem() {
		rightText = "▸";
		text = "Copy Row";
	}
	Menu* createChildMenu() override {
		Menu *menu = new Menu;
		for (int i = 0; i < 4; i++) {
			CopyRowMenuItem *item = new CopyRowMenuItem(i);
			item->text = "Row " + std::string(1, 'A' + i);
			item->abcdSeq = abcdSeq;
			menu->addChild(item);
		}
		return menu;
	}
};

struct PasteRowMenuItem : MenuItem {
	AbcdSeq *abcdSeq;
	int row;
	PasteRowMenuItem(int rowIdx) {
		this->row = rowIdx;
	}
	void onAction(const event::Action &e) override {
		if (abcdSeq) {
			abcdSeq->pasteRowNotes(row);
		}
	}
};

struct PasteMenuItem : MenuItem {
	AbcdSeq *abcdSeq;
	PasteMenuItem() {
		rightText = "▸";
		text = "Paste Row";
	}
	Menu* createChildMenu() override {
		Menu *menu = new Menu;
		for (int i = 0; i < 4; i++) {
			PasteRowMenuItem *item = new PasteRowMenuItem(i);
			item->text = "Row " + std::string(1, 'A' + i);
			item->abcdSeq = abcdSeq;
			menu->addChild(item);
		}
		return menu;
	}
};

struct VelocityAsProbabilitytem : MenuItem {
	AbcdSeq *abcdSeq;
	void onAction(const event::Action &e) override {
		abcdSeq->velocityAsProbability = !abcdSeq->velocityAsProbability;
	}
	void step() override {
		rightText = (abcdSeq->velocityAsProbability) ? "✔" : "";
		MenuItem::step();
	}
};


void AbcdSeqWidget::appendContextMenu(Menu *menu) {
	MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);

	AbcdSeq *abcdSeq = dynamic_cast<AbcdSeq*>(module);
	MenuLabel *modeLabel = new MenuLabel();
	modeLabel->text = "Gate Mode";
	menu->addChild(modeLabel);

	AbcdSeqGateModeItem *triggerItem = new AbcdSeqGateModeItem();
	triggerItem->text = "Trigger";
	triggerItem->abcdSeq = abcdSeq;
	triggerItem->gateMode = AbcdSeq::TRIGGER;
	menu->addChild(triggerItem);

	AbcdSeqGateModeItem *retriggerItem = new AbcdSeqGateModeItem();
	retriggerItem->text = "Retrigger";
	retriggerItem->abcdSeq = abcdSeq;
	retriggerItem->gateMode = AbcdSeq::RETRIGGER;
	menu->addChild(retriggerItem);

	AbcdSeqGateModeItem *continuousItem = new AbcdSeqGateModeItem();
	continuousItem->text = "Continuous";
	continuousItem->abcdSeq = abcdSeq;
	continuousItem->gateMode = AbcdSeq::CONTINUOUS;
	menu->addChild(continuousItem);

	AbcdSeqPitchMenuItem *pitchMenuItem = new AbcdSeqPitchMenuItem();
	pitchMenuItem->text = "Ignore Gate for V/OCT Out";
	pitchMenuItem->abcdSeq = abcdSeq;
	menu->addChild(pitchMenuItem);

	MenuLabel *spacerLabel2 = new MenuLabel();
	menu->addChild(spacerLabel2);

	PossibleCharsMenuItem *possibleCharsMenuItem = new PossibleCharsMenuItem();
	possibleCharsMenuItem->abcdSeq = abcdSeq;
	menu->addChild(possibleCharsMenuItem);

	PresetChildMenuItem *presetMenuItem = new PresetChildMenuItem();
	presetMenuItem->abcdSeq = abcdSeq;
	menu->addChild(presetMenuItem);

	RandomizeMenuItem *randomizeMenuItem = new RandomizeMenuItem();
	randomizeMenuItem->abcdSeq = abcdSeq;
	menu->addChild(randomizeMenuItem);

	CopyMenuItem *copyMenuItem = new CopyMenuItem();
	copyMenuItem->abcdSeq = abcdSeq;
	menu->addChild(copyMenuItem);
	
	PasteMenuItem *pasteMenuItem = new PasteMenuItem();
	pasteMenuItem->abcdSeq = abcdSeq;
	menu->addChild(pasteMenuItem);

	VelocityAsProbabilitytem *velocityAsProbabilityItem = new VelocityAsProbabilitytem();
	velocityAsProbabilityItem->abcdSeq = abcdSeq;
	velocityAsProbabilityItem->text = "Velocity as Probability";
	menu->addChild(velocityAsProbabilityItem);
}



Model *modelAbcdSeq = createModel<AbcdSeq, AbcdSeqWidget>("AbcdSeq");
