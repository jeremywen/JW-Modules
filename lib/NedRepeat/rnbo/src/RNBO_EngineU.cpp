//
//  RNBO_EngineU.cpp
//  RNBOUnitTests
//
//  Created by Stefan Brunner on 21.05.25.
//

#ifdef C74_UNIT_TESTS

#include <JuceHeader.h>
#include "RNBO_Engine.h"

#define STR_IMPL(A) #A
#define STR(A) STR_IMPL(A)
#include STR(RNBO_ENGINE_CLASS_FILE)

namespace RNBO {

class EngineTests : public UnitTest, public EventHandler {

    public:

        EngineTests()
        : UnitTest("Engine")
        {}

        void eventsAvailable() override {
            drainEvents();
        }

        void runTest() override {
            beginTest ("Set Parameter");

            UniquePtr<PatcherInterface> patcher(creaternbomaticEngine());
            _engine.setPatcher(std::move(patcher));
            _engine.prepareToProcess(44100, 1024);

            ParameterEventInterfaceUniquePtr pi =
            _engine.createParameterInterface(ParameterEventInterface::SingleProducer, nullptr);

            ParameterEventInterfaceUniquePtr pi2 =
            _engine.createParameterInterface(ParameterEventInterface::SingleProducer, this);

            _engine.process(nullptr, 0, nullptr, 0, 1024, nullptr, nullptr);

            pi->setParameterValue(0, 0.3);
            pi->setParameterValue(1, 0.5);

            _engine.process(nullptr, 0, nullptr, 0, 1024, nullptr, nullptr);
            
            expectEquals(pi->getParameterValue(0), 0.3);
            expectEquals(pi->getParameterValue(1), 0.5);
            expectEquals(pi2->getParameterValue(0), 0.3);
            expectEquals(pi2->getParameterValue(1), 0.5);
        }

    private:

        RNBO::Engine    _engine;

    };

    static EngineTests engineTests;

} // namespace RNBO

#endif // C74_UNIT_TESTS
