#include "JWModules.hpp"

#if defined(METAMODULE_BUILTIN)
extern Plugin *pluginInstance;
#else
Plugin *pluginInstance;
#endif

#if defined(METAMODULE_BUILTIN)
void init_JWModules(rack::Plugin *p) {
#else 
void init(rack::Plugin *p) {
#endif

	pluginInstance = p;
	p->addModel(modelAdd5);
	p->addModel(modelAbcdSeq);
	p->addModel(modelArrange);
	p->addModel(modelArrange16);
	p->addModel(modelBouncyBalls);
	p->addModel(modelCat);
	p->addModel(modelTree);
	p->addModel(modelFullScope);
	p->addModel(modelGridSeq);
	p->addModel(modelEightSeq);
	p->addModel(modelDivSeq);
	p->addModel(modelMinMax);
	p->addModel(modelNoteSeq);
	p->addModel(modelNoteSeqFu);
	p->addModel(modelNoteSeq16);
	p->addModel(modelTrigs);
	p->addModel(modelOnePattern);
	p->addModel(modelPatterns);
	p->addModel(modelQuantizer);
	p->addModel(modelSimpleClock);
	p->addModel(modelStr1ker);
	p->addModel(modelD1v1de);
	p->addModel(modelPres1t);
	p->addModel(modelThingThing);
	p->addModel(modelWavHead);
	p->addModel(modelXYPad);
	p->addModel(modelBlankPanel1hp);
	p->addModel(modelBlankPanelSmall);
	p->addModel(modelBlankPanelMedium);
	p->addModel(modelBlankPanelLarge);
	p->addModel(modelCoolBreeze);
	p->addModel(modelPete);
	p->addModel(modelTimer);
	p->addModel(modelShiftRegRnd);
	p->addModel(modelBuffer);
	p->addModel(modelRandomSound);
	p->addModel(modelStereoSwitch);
}
