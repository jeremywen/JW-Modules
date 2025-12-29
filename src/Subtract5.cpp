#include <string.h>
#include <algorithm>
#include "JWModules.hpp"

struct Subtract5 : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		VOLT_INPUT,
		NUM_INPUTS = VOLT_INPUT + 16
	};
	enum OutputIds {
		VOLT_OUTPUT,
		NUM_OUTPUTS = VOLT_INPUT + 16
	};
	enum LightIds {
		NUM_LIGHTS
	};

	Subtract5() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for(int i=0;i<NUM_INPUTS;i++){
			configInput(VOLT_INPUT + i, "Voltage" + std::to_string(i+1));
			configOutput(VOLT_OUTPUT + i, "Voltage" + std::to_string(i+1));
			configBypass(VOLT_INPUT + i, VOLT_OUTPUT + i);
		}
	}

	~Subtract5() {
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
		for(int i=0;i<NUM_INPUTS;i++){
			int channels = inputs[i].getChannels();
			for (int c = 0; c < channels; c++) {
				outputs[i].setVoltage(clampfjw(inputs[i].getVoltage(c) - 5.0, -10.0, 10.0), c);
			}
			outputs[i].setChannels(channels);
		}
	}

};

struct Subtract5Widget : ModuleWidget { 
	Subtract5Widget(Subtract5 *module); 
	void appendContextMenu(Menu *menu) override;
};

Subtract5Widget::Subtract5Widget(Subtract5 *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*3, RACK_GRID_HEIGHT);

	setPanel(createPanel(
		asset::plugin(pluginInstance, "res/Subtract5.svg"), 
		asset::plugin(pluginInstance, "res/Subtract5.svg")
	));

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	for(int i=0;i<Subtract5::NUM_INPUTS;i++){
		addInput(createInput<TinyPJ301MPort>(Vec(4, i*19.5+33.5), module, i));
		addOutput(createOutput<TinyPJ301MPort>(Vec(27, i*19.5+33.5), module, i));
	}

}

void Subtract5Widget::appendContextMenu(Menu *menu) {
}

Model *modelSubtract5 = createModel<Subtract5, Subtract5Widget>("Subtract5");
