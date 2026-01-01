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
	p->addModel(modelAbcdSeq);
	p->addModel(modelAdd5);
	p->addModel(modelArrange);
	p->addModel(modelArrange16);
	p->addModel(modelBlankPanel1hp);
	p->addModel(modelBlankPanel2hp);
	p->addModel(modelBlankPanel4hp);
	p->addModel(modelBlankPanelLarge);
	p->addModel(modelBlankPanelMedium);
	p->addModel(modelBlankPanelSmall);
	p->addModel(modelBouncyBalls);
	p->addModel(modelBuffer);
	p->addModel(modelCat);
	p->addModel(modelCoolBreeze);
	p->addModel(modelD1v1de);
	p->addModel(modelDivSeq);
	p->addModel(modelEightSeq);
	p->addModel(modelFullScope);
	p->addModel(modelGrains);
	p->addModel(modelGridSeq);
	p->addModel(modelMinMax);
	p->addModel(modelNoteSeq);
	p->addModel(modelNoteSeq16);
	p->addModel(modelNoteSeqFu);
	p->addModel(modelOnePattern);
	p->addModel(modelPatterns);
	p->addModel(modelPete);
	p->addModel(modelPres1t);
	p->addModel(modelQuantizer);
	p->addModel(modelRandomSound);
	p->addModel(modelSampleGrid);
	p->addModel(modelShiftRegRnd);
	p->addModel(modelSimpleClock);
	p->addModel(modelStereoSwitch);
	p->addModel(modelStereoSwitchInv);
	p->addModel(modelStr1ker);
	p->addModel(modelSubtract5);
	p->addModel(modelThingThing);
	p->addModel(modelTimer);
	p->addModel(modelTree);
	p->addModel(modelTrigs);
	p->addModel(modelWavHead);
	p->addModel(modelXYPad);
}
