// #include "JWModules.hpp"

// struct Temp123 : Module {
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

// 	Temp123() {
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
// void Temp123::process(const ProcessArgs &args) {
// };

// struct Temp123Widget : ModuleWidget {
// 	Temp123Widget(Temp123 *module);
// 	~Temp123Widget(){ 
// 	}
// 	void appendContextMenu(Menu *menu) override;
// };

// Temp123Widget::Temp123Widget(Temp123 *module) {
// 	setModule(module);
// 	box.size = Vec(RACK_GRID_WIDTH*40, RACK_GRID_HEIGHT);

// 	setPanel(createPanel(
// 		asset::plugin(pluginInstance, "res/Temp123.svg"), 
// 		asset::plugin(pluginInstance, "res/Temp123.svg")
// 	));

// 	addChild(createWidget<Screw_J>(Vec(16, 2)));
// 	addChild(createWidget<Screw_J>(Vec(16, 365)));
// 	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
// 	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

// };

// void Temp123Widget::appendContextMenu(Menu *menu) {
// 	MenuLabel *spacerLabel = new MenuLabel();
// 	menu->addChild(spacerLabel);

// 	Temp123 *temp123 = dynamic_cast<Temp123*>(module);
// }



// Model *modelTemp123 = createModel<Temp123, Temp123Widget>("Temp123");
