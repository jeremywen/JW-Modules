#include <string.h>
#include <algorithm>
#include "JWModules.hpp"

struct StereoSwitch : Module {
	enum ParamIds {
		WEIGHT_1,
		WEIGHT_2,
		WEIGHT_3,
		WEIGHT_4,
		WEIGHT_5,
		WEIGHT_6,
		WEIGHT_7,
		WEIGHT_8,
		NUM_PARAMS
	};
	enum InputIds {
		TRIGGER_INPUT,
		IN_L1, IN_R1,
		IN_L2, IN_R2,
		IN_L3, IN_R3,
		IN_L4, IN_R4,
		IN_L5, IN_R5,
		IN_L6, IN_R6,
		IN_L7, IN_R7,
		IN_L8, IN_R8,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_L,
		OUT_R,
		NUM_OUTPUTS
	};
	enum LightIds {
		PAIR_LIGHT_1,
		PAIR_LIGHT_2,
		PAIR_LIGHT_3,
		PAIR_LIGHT_4,
		PAIR_LIGHT_5,
		PAIR_LIGHT_6,
		PAIR_LIGHT_7,
		PAIR_LIGHT_8,
		NUM_LIGHTS
	};
	dsp::SchmittTrigger trig;
	int currentPair = 0; // 0..7

	StereoSwitch() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		// Weight knobs (0..1)
		configParam(WEIGHT_1, 0.f, 1.f, 0.125f, "Weight 1");
		configParam(WEIGHT_2, 0.f, 1.f, 0.125f, "Weight 2");
		configParam(WEIGHT_3, 0.f, 1.f, 0.125f, "Weight 3");
		configParam(WEIGHT_4, 0.f, 1.f, 0.125f, "Weight 4");
		configParam(WEIGHT_5, 0.f, 1.f, 0.125f, "Weight 5");
		configParam(WEIGHT_6, 0.f, 1.f, 0.125f, "Weight 6");
		configParam(WEIGHT_7, 0.f, 1.f, 0.125f, "Weight 7");
		configParam(WEIGHT_8, 0.f, 1.f, 0.125f, "Weight 8");
		configInput(TRIGGER_INPUT, "Trigger");
		// Stereo input pairs
		configInput(IN_L1, "In L1"); configInput(IN_R1, "In R1");
		configInput(IN_L2, "In L2"); configInput(IN_R2, "In R2");
		configInput(IN_L3, "In L3"); configInput(IN_R3, "In R3");
		configInput(IN_L4, "In L4"); configInput(IN_R4, "In R4");
		configInput(IN_L5, "In L5"); configInput(IN_R5, "In R5");
		configInput(IN_L6, "In L6"); configInput(IN_R6, "In R6");
		configInput(IN_L7, "In L7"); configInput(IN_R7, "In R7");
		configInput(IN_L8, "In L8"); configInput(IN_R8, "In R8");
		configOutput(OUT_L, "Out L");
		configOutput(OUT_R, "Out R");
	}

	~StereoSwitch() {
	}

	void onRandomize() override {
	}

	void onReset() override {
	}

	void onSampleRateChange() override {
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
	}

	void process(const ProcessArgs &args) override {
				// Update lights: only selected pair lit
				for (int i = 0; i < 8; i++) {
					lights[PAIR_LIGHT_1 + i].setBrightness(i == currentPair ? 1.0f : 0.0f);
				}
		// On trigger, choose a weighted random stereo pair (0..7)
		if (trig.process(inputs[TRIGGER_INPUT].getVoltage())) {
			float weights[8] = {
				params[WEIGHT_1].getValue(),
				params[WEIGHT_2].getValue(),
				params[WEIGHT_3].getValue(),
				params[WEIGHT_4].getValue(),
				params[WEIGHT_5].getValue(),
				params[WEIGHT_6].getValue(),
				params[WEIGHT_7].getValue(),
				params[WEIGHT_8].getValue()
			};
			// Compute cumulative distribution
			float sum = 0.f;
			for (int i = 0; i < 8; i++) sum += weights[i];
			if (sum <= 0.f) {
				// Fallback to uniform if all weights zero
				currentPair = (int) std::floor(random::uniform() * 8.0f);
				if (currentPair < 0) currentPair = 0;
				if (currentPair > 7) currentPair = 7;
			} else {
				float r = random::uniform() * sum;
				float acc = 0.f;
				int chosen = 0;
				for (int i = 0; i < 8; i++) {
					acc += weights[i];
					if (r <= acc) { chosen = i; break; }
				}
				currentPair = chosen;
			}
		}
		// Map pair index to input IDs
		int lIds[8] = {IN_L1, IN_L2, IN_L3, IN_L4, IN_L5, IN_L6, IN_L7, IN_L8};
		int rIds[8] = {IN_R1, IN_R2, IN_R3, IN_R4, IN_R5, IN_R6, IN_R7, IN_R8};
		float l = inputs[lIds[currentPair]].getVoltage();
		float r = inputs[rIds[currentPair]].getVoltage();
		outputs[OUT_L].setVoltage(l);
		outputs[OUT_R].setVoltage(r);
	}

};

struct StereoSwitchWidget : ModuleWidget { 
	StereoSwitchWidget(StereoSwitch *module); 
	void appendContextMenu(Menu *menu) override;
};

StereoSwitchWidget::StereoSwitchWidget(StereoSwitch *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*10, RACK_GRID_HEIGHT);

	setPanel(createPanel(
		asset::plugin(pluginInstance, "res/StereoSwitch.svg"), 
		asset::plugin(pluginInstance, "res/dark/StereoSwitch.svg")
	));

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	// Left column (L), Right column (R)
	// Indicator LEDs to the left of each pair
	addChild(createLight<SmallLight<RedLight>>(Vec(20, 68), module, StereoSwitch::PAIR_LIGHT_1));
	addInput(createInput<PJ301MPort>(Vec(38, 55),  module, StereoSwitch::IN_L1));
	addInput(createInput<PJ301MPort>(Vec(78, 55), module, StereoSwitch::IN_R1));
	addParam(createParam<SmallWhiteKnob>(Vec(118, 56), module, StereoSwitch::WEIGHT_1));
	addChild(createLight<SmallLight<RedLight>>(Vec(20, 98), module, StereoSwitch::PAIR_LIGHT_2));
	addInput(createInput<PJ301MPort>(Vec(38, 85),  module, StereoSwitch::IN_L2));
	addInput(createInput<PJ301MPort>(Vec(78, 85), module, StereoSwitch::IN_R2));
	addParam(createParam<SmallWhiteKnob>(Vec(118, 86), module, StereoSwitch::WEIGHT_2));
	addChild(createLight<SmallLight<RedLight>>(Vec(20, 128), module, StereoSwitch::PAIR_LIGHT_3));
	addInput(createInput<PJ301MPort>(Vec(38, 115), module, StereoSwitch::IN_L3));
	addInput(createInput<PJ301MPort>(Vec(78, 115),module, StereoSwitch::IN_R3));
	addParam(createParam<SmallWhiteKnob>(Vec(118, 116), module, StereoSwitch::WEIGHT_3));
	addChild(createLight<SmallLight<RedLight>>(Vec(20, 158), module, StereoSwitch::PAIR_LIGHT_4));
	addInput(createInput<PJ301MPort>(Vec(38, 145), module, StereoSwitch::IN_L4));
	addInput(createInput<PJ301MPort>(Vec(78, 145),module, StereoSwitch::IN_R4));
	addParam(createParam<SmallWhiteKnob>(Vec(118, 146), module, StereoSwitch::WEIGHT_4));
	addChild(createLight<SmallLight<RedLight>>(Vec(20, 188), module, StereoSwitch::PAIR_LIGHT_5));
	addInput(createInput<PJ301MPort>(Vec(38, 175), module, StereoSwitch::IN_L5));
	addInput(createInput<PJ301MPort>(Vec(78, 175),module, StereoSwitch::IN_R5));
	addParam(createParam<SmallWhiteKnob>(Vec(118, 176), module, StereoSwitch::WEIGHT_5));
	addChild(createLight<SmallLight<RedLight>>(Vec(20, 218), module, StereoSwitch::PAIR_LIGHT_6));
	addInput(createInput<PJ301MPort>(Vec(38, 205), module, StereoSwitch::IN_L6));
	addInput(createInput<PJ301MPort>(Vec(78, 205),module, StereoSwitch::IN_R6));
	addParam(createParam<SmallWhiteKnob>(Vec(118, 206), module, StereoSwitch::WEIGHT_6));
	addChild(createLight<SmallLight<RedLight>>(Vec(20, 248), module, StereoSwitch::PAIR_LIGHT_7));
	addInput(createInput<PJ301MPort>(Vec(38, 235), module, StereoSwitch::IN_L7));
	addInput(createInput<PJ301MPort>(Vec(78, 235),module, StereoSwitch::IN_R7));
	addParam(createParam<SmallWhiteKnob>(Vec(118, 236), module, StereoSwitch::WEIGHT_7));
	addChild(createLight<SmallLight<RedLight>>(Vec(20, 278), module, StereoSwitch::PAIR_LIGHT_8));
	addInput(createInput<PJ301MPort>(Vec(38, 265), module, StereoSwitch::IN_L8));
	addInput(createInput<PJ301MPort>(Vec(78, 265),module, StereoSwitch::IN_R8));
	addParam(createParam<SmallWhiteKnob>(Vec(118, 266), module, StereoSwitch::WEIGHT_8));
	
	addInput(createInput<PJ301MPort>(Vec(10, 310), module, StereoSwitch::TRIGGER_INPUT));
	addOutput(createOutput<PJ301MPort>(Vec(58, 310), module, StereoSwitch::OUT_L));
	addOutput(createOutput<PJ301MPort>(Vec(98, 310), module, StereoSwitch::OUT_R));

}

void StereoSwitchWidget::appendContextMenu(Menu *menu) {
}

Model *modelStereoSwitch = createModel<StereoSwitch, StereoSwitchWidget>("StereoSwitch");
