// #include "JWModules.hpp"

// struct NewModule : Module {
// 	enum ParamIds {
// 		NUM_PARAMS
// 	};
// 	enum InputIds {
// 		NUM_INPUTS
// 	};
// 	enum OutputIds {
// 		NUM_OUTPUTS
// 	};
// 	enum LightIds {
// 		NUM_LIGHTS
// 	};

// 	NewModule() {
// 		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
// 	}

// 	void process(const ProcessArgs &args) override;

// 	json_t *dataToJson() override {
// 		json_t *rootJ = json_object();

// 		return rootJ;
// 	}

// 	void dataFromJson(json_t *rootJ) override {
// 	}

// 	void onReset() override {
// 	}

// 	void onRandomize() override {
// 	}
// };
// ///////////////////////////////////////////////////////////////////////////////////////////////////
// // STEP
// ///////////////////////////////////////////////////////////////////////////////////////////////////
// void NewModule::process(const ProcessArgs &args) {
// };

// struct NewModuleWidget : ModuleWidget {
// 	NewModuleWidget(NewModule *module);
// 	~NewModuleWidget(){ 
// 	}
// 	void appendContextMenu(Menu *menu) override;
// };

// NewModuleWidget::NewModuleWidget(NewModule *module) {
// 	setModule(module);
// 	box.size = Vec(RACK_GRID_WIDTH*40, RACK_GRID_HEIGHT);

// 	setPanel(createPanel(
// 		asset::plugin(pluginInstance, "res/NewModule.svg"), 
// 		asset::plugin(pluginInstance, "res/NewModule.svg")
// 	));

// 	addChild(createWidget<Screw_J>(Vec(16, 2)));
// 	addChild(createWidget<Screw_J>(Vec(16, 365)));
// 	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
// 	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

// };

// void NewModuleWidget::appendContextMenu(Menu *menu) {
// 	MenuLabel *spacerLabel = new MenuLabel();
// 	menu->addChild(spacerLabel);

// 	NewModule *newModule = dynamic_cast<NewModule*>(module);
// }



// Model *modelNewModule = createModel<NewModule, NewModuleWidget>("NewModule");
