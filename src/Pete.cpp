#include "JWModules.hpp"

struct Pete : Module {
	enum ParamIds {
		BOWL_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	int catY = 0;
	bool goingDown = true;

	Pete() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
	}
	
};

struct PeteWidget : ModuleWidget {
	PeteWidget(Pete *module);
};

PeteWidget::PeteWidget(Pete *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*12, RACK_GRID_HEIGHT);
	
	setPanel(createPanel(
		asset::plugin(pluginInstance, "res/PT.svg"), 
		asset::plugin(pluginInstance, "res/dark/PT.svg")
	));

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));
}

Model *modelPete = createModel<Pete, PeteWidget>("Pete");