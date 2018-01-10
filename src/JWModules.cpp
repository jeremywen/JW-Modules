#include "JWModules.hpp"

Plugin *plugin;

void init(rack::Plugin *p)
{
	std::string me = "JW-Modules"; //DONT CHANGE THIS EVER!
	plugin = p;
	p->slug = "JW-Modules";
	p->version = TOSTRING(VERSION);
	// NOTE: First module string is the slug, don't change it.
	p->addModel(createModel<CatWidget>(me, "0Cat", "0Cat", VISUAL_TAG));
	p->addModel(createModel<BouncyBallsWidget>(me, "BouncyBalls", "Bouncy Balls",  SEQUENCER_TAG, VISUAL_TAG));
	p->addModel(createModel<FullScopeWidget>(me, "FullScope", "Full Scope", VISUAL_TAG));
	p->addModel(createModel<GridSeqWidget>(me, "GridSeq", "GridSeq", SEQUENCER_TAG));
	p->addModel(createModel<QuantizerWidget>(me, "Quantizer", "Quantizer", QUANTIZER_TAG));
	p->addModel(createModel<MinMaxWidget>(me, "MinMax", "Min Max", UTILITY_TAG));
	p->addModel(createModel<NoteSeqWidget>(me, "NoteSeq", "NoteSeq", SEQUENCER_TAG));
	p->addModel(createModel<SimpleClockWidget>(me, "SimpleClock", "Simple Clock", CLOCK_TAG, RANDOM_TAG));
	p->addModel(createModel<ThingThingWidget>(me, "ThingThing", "Thing Thing", VISUAL_TAG));
	p->addModel(createModel<WavHeadWidget>(me, "WavHead", "Wav Head", VISUAL_TAG));
	p->addModel(createModel<XYPadWidget>(me, "XYPad", "XY Pad", LFO_TAG, ENVELOPE_GENERATOR_TAG, RANDOM_TAG, OSCILLATOR_TAG, SAMPLE_AND_HOLD_TAG));
}
