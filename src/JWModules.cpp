#include "JWModules.hpp"


Plugin *plugin;

void init(rack::Plugin *p) {
	plugin = p;
	p->slug = "JW-Modules";
#ifdef VERSION
	p->version = TOSTRING(VERSION);
#endif
	std::string me = "JW-Modules";
	p->addModel(createModel<FullScopeWidget>(me, "FullScope", "FullScope", VISUAL_TAG));
	p->addModel(createModel<GridSeqWidget>(me, "GridSeq", "GridSeq", SEQUENCER_TAG));
	p->addModel(createModel<QuantizerWidget>(me, "Quantizer", "Quantizer", QUANTIZER_TAG));
	p->addModel(createModel<MinMaxWidget>(me, "MinMax", "MinMax", UTILITY_TAG));
	p->addModel(createModel<SimpleClockWidget>(me, "SimpleClock", "SimpleClock", CLOCK_TAG, RANDOM_TAG));
	p->addModel(createModel<WavHeadWidget>("JW-Modules", "WavHead", "WavHead", VISUAL_TAG));
	p->addModel(createModel<XYPadWidget>(me, "XYPad", "XYPad", LFO_TAG, ENVELOPE_GENERATOR_TAG, RANDOM_TAG, OSCILLATOR_TAG, SAMPLE_AND_HOLD_TAG));
	// p->addModel(createModel<BouncyBallWidget>("JW-Modules", "JW-Modules", "BouncyBall", "BouncyBall"));
}
