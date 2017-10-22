#include "JWModules.hpp"
#include "dsp/digital.hpp"

struct GridSeq : Module {
	enum ParamIds {
		RUN_PARAM,
		CLOCK_PARAM,
		RESET_PARAM,
		CELL_PARAM,
		GATE_PARAM = CELL_PARAM + 16,
		NUM_PARAMS = GATE_PARAM + 16,
	};
	enum InputIds {
		CLOCK_INPUT,
		EXT_CLOCK_INPUT,
		RESET_INPUT,
		RIGHT_INPUT, LEFT_INPUT, DOWN_INPUT, UP_INPUT,
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

	SchmittTrigger clockTrigger; // for external clock
	SchmittTrigger runningTrigger;
	SchmittTrigger resetTrigger;
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
		for (int i = 0; i < 8; i++) {
			gateState[i] = true;
		}
	}

	void randomize() {
		for (int i = 0; i < 8; i++) {
			gateState[i] = (randomf() > 0.5);
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

	// // Reset
	// if (resetTrigger.process(params[RESET_PARAM].value + inputs[RESET_INPUT].value)) {
	// 	phase = 0.0;
	// 	index = 8;
	// 	nextStep = true;
	// 	resetLight = 1.0;
	// }

	if(running){
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
		index = posX + (posY * 4); //this is the only place index should be updated
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
	outputs[CELL_OUTPUT].value = cellVal;
	outputs[GATES_OUTPUT].value = gatesOn ? 10.0 : 0.0;
}

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

	// addInput(createInput<PJ301MPort>(Vec(83, 90), module, GridSeq::RESET_INPUT));

	addInput(createInput<PJ301MPort>(Vec(83, 90), module, GridSeq::RIGHT_INPUT));
	addInput(createInput<PJ301MPort>(Vec(138, 90), module, GridSeq::LEFT_INPUT));
	addInput(createInput<PJ301MPort>(Vec(193, 90), module, GridSeq::DOWN_INPUT));
	addInput(createInput<PJ301MPort>(Vec(248, 90), module, GridSeq::UP_INPUT));

	addOutput(createOutput<PJ301MPort>(Vec(21, 202), module, GridSeq::GATES_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(21, 274), module, GridSeq::CELL_OUTPUT));

	addParam(createParam<LEDButton>(Vec(23, 112), module, GridSeq::RUN_PARAM, 0.0, 1.0, 0.0));
	addChild(createValueLight<SmallLight<MyBlueValueLight>>(Vec(23+5, 112+5), &module->runningLight));

	int boxSize = 55;
	for (int x = 0; x < 4; x++) {
		for (int y = 0; y < 4; y++) {
			int knobX = x * boxSize + 76;
			int knobY = y * boxSize + 149;
			int idx = (x+(y*4));
			addParam(createParam<SmallWhiteKnob>(Vec(knobX, knobY), module, GridSeq::CELL_PARAM + idx, 0.0, 6.0, 0.0));
			addParam(createParam<LEDButton>(Vec(knobX+22, knobY-15), module, GridSeq::GATE_PARAM + idx, 0.0, 1.0, 0.0));
			addChild(createValueLight<SmallLight<MyBlueValueLight>>(Vec(knobX+27, knobY-10), &module->gateLights[idx]));			
		}
	}
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
