#include "rack.hpp"


using namespace rack;


extern Plugin *plugin;

////////////////////
// module widgets
////////////////////

struct SimpleClockWidget : ModuleWidget {
	SimpleClockWidget();
};

struct GridSeqWidget : ModuleWidget {
	GridSeqWidget();
	Menu *createContextMenu();
};

struct SmallWhiteKnob : RoundKnob {
	SmallWhiteKnob() {
		setSVG(SVG::load(assetPlugin(plugin, "res/SmallWhiteKnob.svg")));
	}
};

struct MyBlueValueLight : ColorValueLight {
	MyBlueValueLight() {
		baseColor = nvgRGB(25, 150, 252);
	}
};

