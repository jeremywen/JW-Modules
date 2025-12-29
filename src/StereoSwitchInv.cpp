#include <string.h>
#include <algorithm>
#include "JWModules.hpp"

struct StereoSwitchInv : Module {
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
		UP_TRIG_INPUT,
		DOWN_TRIG_INPUT,
		RESET_INPUT,
		IN_L, IN_R,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_L1, OUT_R1,
		OUT_L2, OUT_R2,
		OUT_L3, OUT_R3,
		OUT_L4, OUT_R4,
		OUT_L5, OUT_R5,
		OUT_L6, OUT_R6,
		OUT_L7, OUT_R7,
		OUT_L8, OUT_R8,
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
	dsp::SchmittTrigger upTrig;
	dsp::SchmittTrigger downTrig;
	dsp::SchmittTrigger resetTrig;
	int currentPair = 0; // 0..7

	StereoSwitchInv() {
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
		configInput(TRIGGER_INPUT, "Trigger (random)");
		configInput(UP_TRIG_INPUT, "Up trigger");
		configInput(DOWN_TRIG_INPUT, "Down trigger");
		configInput(RESET_INPUT, "Reset");
		// Single stereo input
		configInput(IN_L, "In L");
		configInput(IN_R, "In R");
		// Stereo output pairs
		configOutput(OUT_L1, "Out L1"); configOutput(OUT_R1, "Out R1");
		configOutput(OUT_L2, "Out L2"); configOutput(OUT_R2, "Out R2");
		configOutput(OUT_L3, "Out L3"); configOutput(OUT_R3, "Out R3");
		configOutput(OUT_L4, "Out L4"); configOutput(OUT_R4, "Out R4");
		configOutput(OUT_L5, "Out L5"); configOutput(OUT_R5, "Out R5");
		configOutput(OUT_L6, "Out L6"); configOutput(OUT_R6, "Out R6");
		configOutput(OUT_L7, "Out L7"); configOutput(OUT_R7, "Out R7");
		configOutput(OUT_L8, "Out L8"); configOutput(OUT_R8, "Out R8");
	}

	~StereoSwitchInv() {
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
		// Directional triggers take precedence
		bool didMove = false;
		// Reset: go to last step unless UP is connected, then first
		if (resetTrig.process(inputs[RESET_INPUT].getVoltage())) {
			if (inputs[DOWN_TRIG_INPUT].isConnected()) {
				currentPair = 0;
			}
			else {
				currentPair = 7;
			}
			didMove = true;
		}
		if (upTrig.process(inputs[UP_TRIG_INPUT].getVoltage())) {
			currentPair = (currentPair + 1) % 8;
			didMove = true;
		}
		if (downTrig.process(inputs[DOWN_TRIG_INPUT].getVoltage())) {
			currentPair = (currentPair + 7) % 8; // -1 mod 8
			didMove = true;
		}
		// If no directional trigger, use weighted random trigger
		if (!didMove && trig.process(inputs[TRIGGER_INPUT].getVoltage())) {
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
		// Map pair index to output IDs
		int lOutIds[8] = {OUT_L1, OUT_L2, OUT_L3, OUT_L4, OUT_L5, OUT_L6, OUT_L7, OUT_L8};
		int rOutIds[8] = {OUT_R1, OUT_R2, OUT_R3, OUT_R4, OUT_R5, OUT_R6, OUT_R7, OUT_R8};
		float l = inputs[IN_L].getVoltage();
		float r = inputs[IN_R].getVoltage();
		for (int i = 0; i < 8; i++) {
			outputs[lOutIds[i]].setVoltage(i == currentPair ? l : 0.f);
			outputs[rOutIds[i]].setVoltage(i == currentPair ? r : 0.f);
		}
	}

};

struct StereoSwitchInvWidget : ModuleWidget { 
	StereoSwitchInvWidget(StereoSwitchInv *module); 
	void appendContextMenu(Menu *menu) override;
};

StereoSwitchInvWidget::StereoSwitchInvWidget(StereoSwitchInv *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*6, RACK_GRID_HEIGHT);

	setPanel(createPanel(
		asset::plugin(pluginInstance, "res/StereoSwitchInv.svg"), 
		asset::plugin(pluginInstance, "res/StereoSwitchInv.svg")
	));

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	// Left column (L), Right column (R)
	// Indicator LEDs to the left of each pair
	addChild(createLight<SmallLight<BlueLight>>(Vec(5, 59), module, StereoSwitchInv::PAIR_LIGHT_1));
	addOutput(createOutput<TinyPJ301MPort>(Vec(18, 55),  module, StereoSwitchInv::OUT_L1));
	addOutput(createOutput<TinyPJ301MPort>(Vec(38, 55), module, StereoSwitchInv::OUT_R1));
	addParam(createParam<JwTinyKnob>(Vec(64, 55), module, StereoSwitchInv::WEIGHT_1));
	addChild(createLight<SmallLight<BlueLight>>(Vec(5, 84), module, StereoSwitchInv::PAIR_LIGHT_2));
	addOutput(createOutput<TinyPJ301MPort>(Vec(18, 80),  module, StereoSwitchInv::OUT_L2));
	addOutput(createOutput<TinyPJ301MPort>(Vec(38, 80), module, StereoSwitchInv::OUT_R2));
	addParam(createParam<JwTinyKnob>(Vec(64, 80), module, StereoSwitchInv::WEIGHT_2));
	addChild(createLight<SmallLight<BlueLight>>(Vec(5, 109), module, StereoSwitchInv::PAIR_LIGHT_3));
	addOutput(createOutput<TinyPJ301MPort>(Vec(18, 105), module, StereoSwitchInv::OUT_L3));
	addOutput(createOutput<TinyPJ301MPort>(Vec(38, 105),module, StereoSwitchInv::OUT_R3));
	addParam(createParam<JwTinyKnob>(Vec(64, 105), module, StereoSwitchInv::WEIGHT_3));
	addChild(createLight<SmallLight<BlueLight>>(Vec(5, 134), module, StereoSwitchInv::PAIR_LIGHT_4));
	addOutput(createOutput<TinyPJ301MPort>(Vec(18, 130), module, StereoSwitchInv::OUT_L4));
	addOutput(createOutput<TinyPJ301MPort>(Vec(38, 130),module, StereoSwitchInv::OUT_R4));
	addParam(createParam<JwTinyKnob>(Vec(64, 130), module, StereoSwitchInv::WEIGHT_4));
	addChild(createLight<SmallLight<BlueLight>>(Vec(5, 159), module, StereoSwitchInv::PAIR_LIGHT_5));
	addOutput(createOutput<TinyPJ301MPort>(Vec(18, 155), module, StereoSwitchInv::OUT_L5));
	addOutput(createOutput<TinyPJ301MPort>(Vec(38, 155),module, StereoSwitchInv::OUT_R5));
	addParam(createParam<JwTinyKnob>(Vec(64, 155), module, StereoSwitchInv::WEIGHT_5));
	addChild(createLight<SmallLight<BlueLight>>(Vec(5, 184), module, StereoSwitchInv::PAIR_LIGHT_6));
	addOutput(createOutput<TinyPJ301MPort>(Vec(18, 180), module, StereoSwitchInv::OUT_L6));
	addOutput(createOutput<TinyPJ301MPort>(Vec(38, 180),module, StereoSwitchInv::OUT_R6));
	addParam(createParam<JwTinyKnob>(Vec(64, 180), module, StereoSwitchInv::WEIGHT_6));
	addChild(createLight<SmallLight<BlueLight>>(Vec(5, 209), module, StereoSwitchInv::PAIR_LIGHT_7));
	addOutput(createOutput<TinyPJ301MPort>(Vec(18, 205), module, StereoSwitchInv::OUT_L7));
	addOutput(createOutput<TinyPJ301MPort>(Vec(38, 205),module, StereoSwitchInv::OUT_R7));
	addParam(createParam<JwTinyKnob>(Vec(64, 205), module, StereoSwitchInv::WEIGHT_7));
	addChild(createLight<SmallLight<BlueLight>>(Vec(5, 234), module, StereoSwitchInv::PAIR_LIGHT_8));
	addOutput(createOutput<TinyPJ301MPort>(Vec(18, 230), module, StereoSwitchInv::OUT_L8));
	addOutput(createOutput<TinyPJ301MPort>(Vec(38, 230),module, StereoSwitchInv::OUT_R8));
	addParam(createParam<JwTinyKnob>(Vec(64, 230), module, StereoSwitchInv::WEIGHT_8));
	
	addInput(createInput<TinyPJ301MPort>(Vec(15, 310), module, StereoSwitchInv::TRIGGER_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(10, 270), module, StereoSwitchInv::UP_TRIG_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(35, 270), module, StereoSwitchInv::DOWN_TRIG_INPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(63, 270), module, StereoSwitchInv::RESET_INPUT));
	// Single stereo input at bottom center
	addInput(createInput<TinyPJ301MPort>(Vec(49.5, 310), module, StereoSwitchInv::IN_L));
	addInput(createInput<TinyPJ301MPort>(Vec(67, 310), module, StereoSwitchInv::IN_R));

}

void StereoSwitchInvWidget::appendContextMenu(Menu *menu) {
}

Model *modelStereoSwitchInv = createModel<StereoSwitchInv, StereoSwitchInvWidget>("StereoSwitchInv");
