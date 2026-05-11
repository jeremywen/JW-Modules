//
//  EngineCoreU.cpp
//  RNBOUnitTests
//
//  Created by Stefan Brunner on 20.05.25.
//

#if defined(C74_UNIT_TESTS) && defined(RNBO_TEST_COREOBJECT)
#include <JuceHeader.h>
#include <stdexcept>
#include <vector>

#include "RNBO.h"
#include "RNBO_EngineCore.h"

#define STR_IMPL(A) #A
#define STR(A) STR_IMPL(A)
#include STR(RNBO_ENGINECORE_CLASS_FILE)

namespace EngineCoreTests {
    using namespace RNBO;

    constexpr number SAMPLERATE = 48000;
    constexpr Index VECTORSIZE = 64;

    static MillisecondTime sampsToMs(SampleIndex samps) {
        return samps * (1/SAMPLERATE * 1000);
    }

    class TestEngine : public EngineCore {
    public:
        size_t getQueueSize() {
            return _scheduledEvents.size();
        }
    };

    class TestHandler : public EventHandler {
    public:
        void handleParameterEvent(const ParameterEvent& pe) override {
            handledParamEvents.push_back({ pe.getIndex(), pe.getTime(), pe.getValue(), pe.getSource() });
        }

        void eventsAvailable() override {
            drainEvents();
        }

        struct HandledParamevent {
            ParameterIndex parameterIndex;
            MillisecondTime eventTime;
            ParameterValue value;
            ParameterInterfaceId source;
        };

        std::vector<HandledParamevent> handledParamEvents;
    };

    class TestPatcher : public rnbomaticEngineCore<EXTERNALENGINE> {
    public:
        void processClockEvent(MillisecondTime time, ClockId index, bool hasValue, ParameterValue value) override
        {
            processedClocks.push_back({ time, index, hasValue, value });
        }

        struct ProcessedClock {
            MillisecondTime time;
            ClockId index;
            bool hasValue;
            ParameterValue value;
        };
        std::vector<ProcessedClock> processedClocks;

        MillisecondTime getPatcherTimePublic() const {
            return this->getPatcherTime();
        }

//        void processParameterEvent(ParameterIndex index, ParameterValue value, MillisecondTime time) override
//        {
//            processedParameters.push_back({ index, value, time });
//        }
//
//        struct ProcessedParameter {
//            ParameterIndex index;
//            ParameterValue value;
//            MillisecondTime time;
//        };
//        std::vector<ProcessedClock> processedParameters;

    };

    class Tests : public UnitTest {
        public:
            Tests()
                : UnitTest("Core Engine Tests")
            {}
        void runTest() override {
            {
                beginTest("scheduleClockEvent");
                UniquePtr<PatcherInterface> patcher(new TestPatcher());
                TestEngine testEngine;
                testEngine.setPatcher(std::move(patcher));
                TestPatcher& testPatcher = static_cast<TestPatcher&>(testEngine.getPatcher());

                testEngine.prepareToProcess(SAMPLERATE, VECTORSIZE);

                // process once to get all startup events out of the way
                testEngine.process(nullptr, 0, nullptr, 0, VECTORSIZE);

                testEngine.scheduleClockEvent(nullptr, 74, sampsToMs(VECTORSIZE) * 1.5);
                expectEquals(testEngine.getQueueSize(), (size_t)1);

                testEngine.scheduleClockEventWithValue(nullptr, 23, sampsToMs(VECTORSIZE) * 2.5, 0.74);
                expectEquals(testEngine.getQueueSize(), (size_t)2);

                testEngine.scheduleClockEvent(nullptr, 23, sampsToMs(VECTORSIZE) * 1.2);
                expectEquals(testEngine.getQueueSize(), (size_t)3);

                testEngine.process(nullptr, 0, nullptr, 0, VECTORSIZE);

                expectEquals(testPatcher.processedClocks.size(), (size_t)2);
                expectEquals(testPatcher.processedClocks[0].index, (ClockId)23);
                expectEquals(testPatcher.processedClocks[1].index, (ClockId)74);
                expect(!testPatcher.processedClocks[0].hasValue);
                expect(!testPatcher.processedClocks[1].hasValue);

                testEngine.process(nullptr, 0, nullptr, 0, VECTORSIZE);
                expectEquals(testPatcher.processedClocks.size(), (size_t)3);
                expectEquals(testPatcher.processedClocks[2].index, (ClockId)23);
                expect(testPatcher.processedClocks[2].hasValue);
                expectEquals(testPatcher.processedClocks[2].value, 0.74);
            }

            {
                beginTest("flushClockEvents");
                UniquePtr<PatcherInterface> patcher(new TestPatcher());
                TestEngine testEngine;
                testEngine.setPatcher(std::move(patcher));
                TestPatcher& testPatcher = static_cast<TestPatcher&>(testEngine.getPatcher());

                testEngine.prepareToProcess(SAMPLERATE, VECTORSIZE);

                // process once to get all initialization events out of the way
                testEngine.process(nullptr, 0, nullptr, 0, VECTORSIZE);

                size_t processed = testPatcher.processedClocks.size();

                testEngine.scheduleClockEventWithValue(nullptr, 72, sampsToMs(VECTORSIZE) * 1.5, 7.4);
                testEngine.scheduleClockEventWithValue(nullptr, 73, sampsToMs(VECTORSIZE) * 1.5, 7.4);
                testEngine.scheduleClockEventWithValue(nullptr, 73, sampsToMs(VECTORSIZE) * 1.5, 23);
                testEngine.scheduleClockEventWithValue(nullptr, 74, sampsToMs(VECTORSIZE) * 1.5, 7.4);

                testEngine.scheduleClockEvent(nullptr, 72, sampsToMs(VECTORSIZE) * 1.5);
                testEngine.scheduleClockEvent(nullptr, 72, sampsToMs(VECTORSIZE) * 1.5);
                testEngine.scheduleClockEvent(nullptr, 73, sampsToMs(VECTORSIZE) * 1.5);
                testEngine.scheduleClockEvent(nullptr, 74, sampsToMs(VECTORSIZE) * 1.5);

                expectEquals(testEngine.getQueueSize(), (size_t)8);
                testEngine.flushClockEventsWithValue(nullptr, 73, 23, false);
                expectEquals(testEngine.getQueueSize(), (size_t)7);
                testEngine.flushClockEvents(nullptr, 72, false);
                expectEquals(testEngine.getQueueSize(), (size_t)4);

                testEngine.flushClockEvents(nullptr, 74, true);
                expectEquals(testPatcher.processedClocks.size(), processed + 2);
                expectEquals(testPatcher.processedClocks[processed].index, (ClockId)74);
                expect(testPatcher.processedClocks[processed].hasValue);
                expectEquals(testPatcher.processedClocks[processed].value, 7.4);
                expect(!testPatcher.processedClocks[processed + 1].hasValue);

                // now process the rest
                testEngine.process(nullptr, 0, nullptr, 0, VECTORSIZE);
                expectEquals(testPatcher.processedClocks.size(), processed + 2 + 2);
                expectEquals(testPatcher.processedClocks[processed + 2].index, (ClockId)73);
                expectEquals(testPatcher.processedClocks[processed + 3].index, (ClockId)73);
            }

            {
                beginTest("Param Change");
                UniquePtr<PatcherInterface> patcher(new TestPatcher());
                TestEngine testEngine;
                TestHandler testHandler;
                auto paramInterface = testEngine.createParameterInterface(RNBO::ParameterEventInterface::NotThreadSafe, &testHandler);
                testEngine.setPatcher(std::move(patcher));
                TestPatcher& testPatcher = static_cast<TestPatcher&>(testEngine.getPatcher());

                testEngine.prepareToProcess(SAMPLERATE, VECTORSIZE);

                // process once to get all initialization events out of the way
                testEngine.process(nullptr, 0, nullptr, 0, VECTORSIZE);

                size_t handled = testHandler.handledParamEvents.size();
                testPatcher.setParameterValue(0, 0.74, 5);
                expectEquals(testHandler.handledParamEvents.size(), handled + 1);
                expectEquals(testEngine.getParameterValue(0), 0.74);
                expectEquals(testPatcher.getPatcherTimePublic(), (MillisecondTime)5);
            }

            {
                beginTest("parameterEvents");
                UniquePtr<PatcherInterface> patcher(new TestPatcher());
                TestEngine testEngine;
                TestHandler testHandler;
                auto paramInterface = testEngine.createParameterInterface(RNBO::ParameterEventInterface::NotThreadSafe, &testHandler);
                testEngine.setPatcher(std::move(patcher));

                testEngine.prepareToProcess(SAMPLERATE, VECTORSIZE);

                // process once to get all initialization events out of the way
                testEngine.process(nullptr, 0, nullptr, 0, VECTORSIZE);

                size_t handled = testHandler.handledParamEvents.size();
                size_t sizeBefore = testEngine.getQueueSize();

                testEngine.scheduleParameterChange(0, 0.74, sampsToMs(VECTORSIZE) * 2.5);
                expectEquals(testEngine.getQueueSize(), sizeBefore + 1);

                testEngine.process(nullptr, 0, nullptr, 0, VECTORSIZE);
                expectEquals(testHandler.handledParamEvents.size(), handled);
                testEngine.process(nullptr, 0, nullptr, 0, VECTORSIZE);
                expectEquals(testHandler.handledParamEvents.size(), handled);
                testEngine.process(nullptr, 0, nullptr, 0, VECTORSIZE);
                expectEquals(testHandler.handledParamEvents.size(), handled + 1);
                expectEquals(testHandler.handledParamEvents[handled].value, 0.74);
            }

            {
                beginTest("paramInit");
                UniquePtr<PatcherInterface> patcher(new TestPatcher());
                TestEngine testEngine;
                TestHandler testHandler;
                auto paramInterface = testEngine.createParameterInterface(RNBO::ParameterEventInterface::NotThreadSafe, &testHandler);
                testEngine.setPatcher(std::move(patcher));

                testEngine.prepareToProcess(SAMPLERATE, VECTORSIZE);

                testEngine.process(nullptr, 0, nullptr, 0, VECTORSIZE);
                expectEquals(testHandler.handledParamEvents.size(), (size_t)1);
                expectEquals(testHandler.handledParamEvents[0].value, 12.);
            }

            {
                beginTest("Process Time");
                UniquePtr<PatcherInterface> patcher(new TestPatcher());
                TestEngine testEngine;
                testEngine.setPatcher(std::move(patcher));
                testEngine.prepareToProcess(SAMPLERATE, VECTORSIZE);

                expectEquals(testEngine.getCurrentTime(), (MillisecondTime)0);
                testEngine.process(nullptr, 0, nullptr, 0, VECTORSIZE);
                expectEquals(testEngine.getCurrentTime(), sampsToMs(VECTORSIZE));
            }
        }
    };
    Tests tests;
}
#endif
