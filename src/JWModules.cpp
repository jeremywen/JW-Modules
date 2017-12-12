#include "JWModules.hpp"

Plugin *plugin;

void init(rack::Plugin *p)
{
	plugin = p;
	p->slug = "JW-Modules";
#ifdef VERSION
	p->version = TOSTRING(VERSION);
#endif
	std::string me = "JW-Modules";
	// NOTE: First module string is the slug, don't change it.
	p->addModel(createModel<BouncyBallsWidget>(me, "BouncyBalls", "Bouncy Balls",  SEQUENCER_TAG, VISUAL_TAG));
	p->addModel(createModel<FullScopeWidget>(me, "FullScope", "Full Scope", VISUAL_TAG));
	p->addModel(createModel<GridSeqWidget>(me, "GridSeq", "GridSeq", SEQUENCER_TAG));
	p->addModel(createModel<QuantizerWidget>(me, "Quantizer", "Quantizer", QUANTIZER_TAG));
	p->addModel(createModel<MinMaxWidget>(me, "MinMax", "Min Max", UTILITY_TAG));
	p->addModel(createModel<SimpleClockWidget>(me, "SimpleClock", "Simple Clock", CLOCK_TAG, RANDOM_TAG));
	p->addModel(createModel<WavHeadWidget>(me, "WavHead", "Wav Head", VISUAL_TAG));
	p->addModel(createModel<XYPadWidget>(me, "XYPad", "XY Pad", LFO_TAG, ENVELOPE_GENERATOR_TAG, RANDOM_TAG, OSCILLATOR_TAG, SAMPLE_AND_HOLD_TAG));
}
