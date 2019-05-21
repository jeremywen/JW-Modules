#include "JWModules.hpp"

struct BlankPanel : Module {BlankPanel() : Module(0, 0, 0, 0) {}};
struct BlankPanelSmallWidget : ModuleWidget { BlankPanelSmallWidget(BlankPanel *module); };
struct BlankPanelMediumWidget : ModuleWidget { BlankPanelMediumWidget(BlankPanel *module); };
struct BlankPanelLargeWidget : ModuleWidget { BlankPanelLargeWidget(BlankPanel *module); };

BlankPanelSmallWidget::BlankPanelSmallWidget(BlankPanel *module) {
		setModule(module);
	box.size = Vec(15*3, 380);

	SVGPanel *panel = new SVGPanel();
	panel->box.size = box.size;
	panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/BlankPanelSmall.svg")));
	addChild(panel);

	addChild(createWidget<Screw_J>(Vec(16, 0)));
	addChild(createWidget<Screw_W>(Vec(16, 365)));
}

BlankPanelMediumWidget::BlankPanelMediumWidget(BlankPanel *module) {
		setModule(module);
	box.size = Vec(15*6, 380);

	SVGPanel *panel = new SVGPanel();
	panel->box.size = box.size;
	panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/BlankPanelMedium.svg")));
	addChild(panel);

	addChild(createWidget<Screw_J>(Vec(16, 0)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 0)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));
}

BlankPanelLargeWidget::BlankPanelLargeWidget(BlankPanel *module) {
		setModule(module);
	box.size = Vec(15*12, 380);

	SVGPanel *panel = new SVGPanel();
	panel->box.size = box.size;
	panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/BlankPanelLarge.svg")));
	addChild(panel);

	addChild(createWidget<Screw_J>(Vec(16, 0)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 0)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));
}

Model *modelBlankPanelSmall = createModel<BlankPanel, BlankPanelSmallWidget>("BlankPanel_SM");
Model *modelBlankPanelMedium = createModel<BlankPanel, BlankPanelMediumWidget>("BlankPanel_MD");
Model *modelBlankPanelLarge = createModel<BlankPanel, BlankPanelLargeWidget>("BlankPanel_LG");
