#include "JWModules.hpp"


Plugin *plugin;

void init(rack::Plugin *p) {
	plugin = p;
	p->slug = "JW-Modules";
#ifdef VERSION
	p->version = TOSTRING(VERSION);
#endif
	p->addModel(createModel<SimpleClockWidget>("JW-Modules", "JW-Modules", "SimpleClock", "SimpleClock"));
	p->addModel(createModel<GridSeqWidget>("JW-Modules", "JW-Modules", "GridSeq", "GridSeq"));
	p->addModel(createModel<RMSWidget>("JW-Modules", "JW-Modules", "RMS", "RMS"));
	p->addModel(createModel<FullScopeWidget>("JW-Modules", "JW-Modules", "FullScope", "FullScope"));
	p->addModel(createModel<XYPadWidget>("JW-Modules", "JW-Modules", "XYPad", "XYPad"));
	p->addModel(createModel<QuantizerWidget>("JW-Modules", "JW-Modules", "Quantizer", "Quantizer"));
	// p->addModel(createModel<BouncyBallWidget>("JW-Modules", "JW-Modules", "BouncyBall", "BouncyBall"));
}
