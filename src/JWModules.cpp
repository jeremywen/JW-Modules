#include "JWModules.hpp"

Plugin *pluginInstance;

void init(rack::Plugin *p)
{
	pluginInstance = p;
	p->addModel(modelBouncyBalls);
	p->addModel(modelCat);
	p->addModel(modelFullScope);
	p->addModel(modelGridSeq);
	p->addModel(modelMinMax);
	p->addModel(modelNoteSeq);
	p->addModel(modelNoteSeq16);
	p->addModel(modelPatterns);
	p->addModel(modelQuantizer);
	p->addModel(modelSimpleClock);
	p->addModel(modelStr1ker);
	p->addModel(modelThingThing);
	p->addModel(modelWavHead);
	p->addModel(modelXYPad);
	p->addModel(modelBlankPanelSmall);
	p->addModel(modelBlankPanelMedium);
	p->addModel(modelBlankPanelLarge);
	p->addModel(modelCoolBreeze);
}
