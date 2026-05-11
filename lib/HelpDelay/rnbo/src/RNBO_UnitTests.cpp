//a place for for small misc tests

#ifdef C74_UNIT_TESTS

#include "RNBO.h"
#include <JuceHeader.h>
#include <stdexcept>

namespace RNBOUnitTests {
	using namespace RNBO;
	class GeneralTests : public UnitTest {
		public:
			GeneralTests()
				: UnitTest("General Tests")
			{}
			void runTest() override {
				beginTest("array bounds");
				RNBO::array<number, 1> l = { 1.0 };
				expectDoesNotThrow(l[0]);
				expectThrowsType(l[1], std::out_of_range);
				expectThrowsType(l[100], std::out_of_range);

				beginTest("truc");
				for (double v: { 2.0, 2.1, -1.0, -1.1, 6.999999999999998 }) {
					expectEquals(rnbo_trunc(v), (RNBO::Int)std::trunc(v));
				}
			}
	};
	GeneralTests tests;
}

#if RNBO_TEST_COREOBJECT

//forward decl
namespace RNBO {
	extern "C" PatcherFactoryFunctionPtr rnbomaticFactoryFunction();
}

namespace RNBOPresetTests {

	using namespace RNBO;

	class Handler : public RNBO::EventHandler {
		public:
			virtual void eventsAvailable() override {}
			void processEvents() {
				drainEvents();
			}
	};

	class PresetTests : public UnitTest {
		public:
			PresetTests()
				: UnitTest("Preset Tests")
			{}
			void runTest() override {
				Index blocksize = 64;
				number samplerate = 44100;

				ParameterInfo info;
				expectEquals(info.min, 0.0);
				Handler handler;

				UniquePtr<PatcherInterface> patcher(rnbomaticFactoryFunction()());
				CoreObject core(std::move(patcher));

				expectNotEquals(core.getNumParameters(), (RNBO::ParameterIndex)0);

				auto interface = core.createParameterInterface(ParameterEventInterface::NotThreadSafe, &handler);

				auto update = [&core, &handler, this, blocksize]() {
					const std::array<const SampleValue *, 0> ins = { };
					const std::array<SampleValue *, 0> outs = { };
					expectDoesNotThrow(core.process(ins.data(), ins.size(), outs.data(), outs.size(), blocksize));
					expectDoesNotThrow(handler.processEvents());
					expectDoesNotThrow(core.process(ins.data(), ins.size(), outs.data(), outs.size(), blocksize));
					expectDoesNotThrow(handler.processEvents());
				};

				core.prepareToProcess(samplerate, blocksize);

				core.getParameterInfo(0, &info);

				expectEquals(info.initialValue, interface->getParameterValue(0));
				expectNotEquals(info.max, info.initialValue);

				{
					beginTest("sync");
					interface->setParameterValue(0, info.max);
					update();
					expectEquals(info.max, interface->getParameterValue(0));

					update();
					auto preset = core.getPresetSync();

					interface->setParameterValue(0, info.initialValue);
					update();
					expectEquals(info.initialValue, interface->getParameterValue(0));

					//can copy
					RNBO::UniquePresetPtr unique = RNBO::make_unique<RNBO::Preset>();
					RNBO::copyPreset(*preset, *unique);

					//can load
					core.setPresetSync(std::move(unique));
					update();
					expectEquals(info.max, interface->getParameterValue(0), "sync to equal max");
				}

				{
					beginTest("async");

					RNBO::UniquePresetPtr unique;
					expect(unique.get() == nullptr);

					interface->setParameterValue(0, info.max);
					update();
					expectEquals(info.max, interface->getParameterValue(0));

					core.getPreset([&unique, this](ConstPresetPtr preset) {
							unique = RNBO::make_unique<RNBO::Preset>();
							//can copy
							expectDoesNotThrow(RNBO::copyPreset(*preset, *unique));
					});
					update();
					expect(unique.get() != nullptr);

					interface->setParameterValue(0, info.initialValue);
					update();
					expectEquals(info.initialValue, interface->getParameterValue(0));

					//can load
					core.setPreset(std::move(unique));
					update();
					expectEquals(info.max, interface->getParameterValue(0), "async to equal max");
				}

				{
					beginTest("json");

					RNBO::UniquePresetPtr unique;
					expect(unique.get() == nullptr);

					interface->setParameterValue(0, info.max);
					update();
					expectEquals(info.max, interface->getParameterValue(0));

					core.getPreset([&unique, this](ConstPresetPtr preset) {
							unique = RNBO::make_unique<RNBO::Preset>();
							//can copy
							expectDoesNotThrow(RNBO::copyPreset(*preset, *unique));
					});
					update();
					expect(unique.get() != nullptr);

					interface->setParameterValue(0, info.initialValue);
					update();
					expectEquals(info.initialValue, interface->getParameterValue(0));

					auto j = convertPresetToJSONObj(*unique);

					RNBO::UniquePresetPtr copy = RNBO::make_unique<RNBO::Preset>();
					convertJSONObjToPreset(j, *copy);

					//can load
					core.setPreset(std::move(copy));
					update();
					expectEquals(info.max, interface->getParameterValue(0), "json to equal max");
				}
			}
	};
	PresetTests tests;
}
#endif

#endif

