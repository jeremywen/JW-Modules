//
//  RNBO_MinimalEngineU.cpp
//  RNBOUnitTests
//
//  Created by Stefan Brunner on 15.05.25.
//
#if defined(C74_UNIT_TESTS) && defined(RNBO_TEST_COREOBJECT)
#include <JuceHeader.h>
#include <stdexcept>
#include <vector>

#define RNBO_MINENGINEQUEUESIZE 8

#include "RNBO.h"
#include "RNBO_MinimalEngine.h"

namespace RNBO {
    extern "C" PatcherFactoryFunctionPtr rnbomaticFactoryFunction();
}

#define STR_IMPL(A) #A
#define STR(A) STR_IMPL(A)
#include STR(RNBO_MINIMAL_CLASS_FILE)

namespace MinimalEngineTests {
    using namespace RNBO;

    constexpr number SAMPLERATE = 48000;
    constexpr Index VECTORSIZE = 64;

    class TestEngine : public MinimalEngine<> {
    public:
        TestEngine(PatcherInterface* patcher)
        : MinimalEngine<>(patcher)
        {}

        void executeEvent(const InternalEvent& event) override {
            MinimalEngine<>::executeEvent(event);
            executedEvents.push_back(event);
        }

        void notifyParameterValueChanged(ParameterIndex index, ParameterValue value, bool /*ignoreSource*/) override {
            notifiedParams.push_back({ index, value });
        }

        std::vector<InternalEvent>  executedEvents;
        std::vector<std::pair<ParameterIndex, ParameterValue>> notifiedParams;
    };

    static MillisecondTime sampsToMs(SampleIndex samps) {
        return samps * (1/SAMPLERATE * 1000);
    }

    class Tests : public UnitTest {
        public:
            Tests()
                : UnitTest("Minimal Engine Tests")
            {}
        void runTest() override {
            {
                beginTest("scheduleClockEvent");
                UniquePtr<PatcherInterface> patcher(rnbomaticFactoryFunction()());
                TestEngine testEngine(patcher.get());
                testEngine.initialize();

                size_t qsize = testEngine.getQueueSize();
                testEngine.scheduleClockEvent(nullptr, 74, 10);
                expectEquals(testEngine.getQueueSize(), (size_t)qsize + 1);
                testEngine.scheduleClockEvent(nullptr, 23, 8);
                expectEquals(testEngine.getQueueSize(), (size_t)qsize + 2);
                testEngine.scheduleClockEvent(nullptr, 23, 12);
                testEngine.processEventsUntil(10);
                expectEquals(testEngine.executedEvents.size(), (size_t)qsize + 2);
                expectEquals(testEngine.executedEvents[testEngine.executedEvents.size() - 2].clockId, (ClockId)23);
                expectEquals(testEngine.executedEvents[testEngine.executedEvents.size() - 1].clockId, (ClockId)74);
            }

            {
                beginTest("scheduleClockEvent2");
                rnbomaticMinimal<TestEngine> patcher;
                expectGreaterOrEqual(patcher.getNumParameters(), (ParameterIndex)1);
                patcher.initialize();
                patcher.prepareToProcess(SAMPLERATE, VECTORSIZE, true);
                TestEngine* testEngine = static_cast<TestEngine*>(patcher.getEngine());

                // schedule an event into the second audio buffer
                patcher.getEngine()->scheduleClockEventWithValue(nullptr, 74, sampsToMs(VECTORSIZE) * 1.5, 7.4);
                patcher.process(nullptr, 0, nullptr, 0, VECTORSIZE);
                // after the first process call we just want to have the bang for the param initialization
                expectEquals(testEngine->executedEvents.size(), (size_t)1);
                expectEquals(testEngine->executedEvents[0].type, TestEngine::ParameterBang);
                patcher.process(nullptr, 0, nullptr, 0, VECTORSIZE);
                // now we expect the clock event to have happened
                expectEquals(testEngine->executedEvents.size(), (size_t)2);
                expectEquals(testEngine->executedEvents[1].type, TestEngine::Clock);
                expectEquals(testEngine->executedEvents[1].clockId, (ClockId)74);
                expectEquals(testEngine->executedEvents[1].value, 7.4);
            }

            {
                beginTest("flushClockEvents");
                rnbomaticMinimal<TestEngine> patcher;
                expectGreaterOrEqual(patcher.getNumParameters(), (ParameterIndex)1);
                patcher.initialize();
                patcher.prepareToProcess(SAMPLERATE, VECTORSIZE, true);
                TestEngine* testEngine = static_cast<TestEngine*>(patcher.getEngine());

                // process once to get all initialization events out of the way
                patcher.process(nullptr, 0, nullptr, 0, VECTORSIZE);

                size_t executed = testEngine->executedEvents.size();

                patcher.getEngine()->scheduleClockEventWithValue(nullptr, 72, sampsToMs(VECTORSIZE) * 1.5, 7.4);
                patcher.getEngine()->scheduleClockEventWithValue(nullptr, 73, sampsToMs(VECTORSIZE) * 1.5, 7.4);
                patcher.getEngine()->scheduleClockEventWithValue(nullptr, 73, sampsToMs(VECTORSIZE) * 1.5, 23);
                patcher.getEngine()->scheduleClockEventWithValue(nullptr, 74, sampsToMs(VECTORSIZE) * 1.5, 7.4);

                patcher.getEngine()->scheduleClockEvent(nullptr, 72, sampsToMs(VECTORSIZE) * 1.5);
                patcher.getEngine()->scheduleClockEvent(nullptr, 72, sampsToMs(VECTORSIZE) * 1.5);
                patcher.getEngine()->scheduleClockEvent(nullptr, 73, sampsToMs(VECTORSIZE) * 1.5);
                patcher.getEngine()->scheduleClockEvent(nullptr, 74, sampsToMs(VECTORSIZE) * 1.5);

                expectEquals(testEngine->getQueueSize(), (size_t)8);
                testEngine->flushClockEventsWithValue(nullptr, 73, 23, false);
                expectEquals(testEngine->getQueueSize(), (size_t)7);
                testEngine->flushClockEvents(nullptr, 72, false);
                expectEquals(testEngine->getQueueSize(), (size_t)4);
                
                testEngine->flushClockEvents(nullptr, 74, true);
                expectEquals(testEngine->executedEvents.size(), executed + 2);
                expectEquals(testEngine->executedEvents[executed].clockId, (ClockId)74);
                expect(testEngine->executedEvents[executed].clockHasValue);
                expectEquals(testEngine->executedEvents[executed].value, 7.4);
                expect(!testEngine->executedEvents[executed + 1].clockHasValue);

                // now process the rest
                patcher.process(nullptr, 0, nullptr, 0, VECTORSIZE);

                expectEquals(testEngine->executedEvents.size(), executed + 2 + 2);
                expectEquals(testEngine->executedEvents[executed + 2].clockId, (ClockId)73);
                expectEquals(testEngine->executedEvents[executed + 3].clockId, (ClockId)73);
            }

            {
                beginTest("Queue Overflow");
                UniquePtr<PatcherInterface> patcher(rnbomaticFactoryFunction()());
                TestEngine testEngine(patcher.get());
                testEngine.initialize();

                size_t qsize = testEngine.getQueueSize();
                for (size_t i = 0; i < RNBO_MINENGINEQUEUESIZE - qsize; i++) {
                    expectDoesNotThrow(testEngine.scheduleParameterBang(0, i));
                }
                expectThrowsType(testEngine.scheduleParameterBang(0, 0), std::overflow_error);
            }

            {
                beginTest("Param Change");
                rnbomaticMinimal<TestEngine> patcher;
                expectGreaterOrEqual(patcher.getNumParameters(), (ParameterIndex)1);
                patcher.initialize();
                patcher.prepareToProcess(SAMPLERATE, VECTORSIZE, true);
                TestEngine* testEngine = static_cast<TestEngine*>(patcher.getEngine());

                // process once to get all initialization events out of the way
                patcher.process(nullptr, 0, nullptr, 0, VECTORSIZE);

                size_t notified = testEngine->notifiedParams.size();
                patcher.processParameterEvent(0, 0.74, 5);
                expectEquals(testEngine->notifiedParams.size(), notified + 1);
                expectEquals(patcher.getParameterValue(0), 0.74);
                expectEquals(testEngine->getPatcherTime(), (MillisecondTime)5);
            }

            {
                beginTest("parameterEvents");
                rnbomaticMinimal<TestEngine> patcher;
                expectGreaterOrEqual(patcher.getNumParameters(), (ParameterIndex)1);
                patcher.initialize();
                patcher.prepareToProcess(SAMPLERATE, VECTORSIZE, true);
                TestEngine* testEngine = static_cast<TestEngine*>(patcher.getEngine());

                // process once to get all initialization events out of the way
                patcher.process(nullptr, 0, nullptr, 0, VECTORSIZE);

                size_t executed = testEngine->executedEvents.size();
                size_t notified = testEngine->notifiedParams.size();
                size_t sizeBefore = testEngine->getQueueSize();

                testEngine->scheduleParameterChange(0, 0.74, sampsToMs(VECTORSIZE) * 2.5);
                expectEquals(testEngine->getQueueSize(), sizeBefore + 1);

                patcher.process(nullptr, 0, nullptr, 0, VECTORSIZE);
                expectEquals(testEngine->notifiedParams.size(), notified);
                patcher.process(nullptr, 0, nullptr, 0, VECTORSIZE);
                expectEquals(testEngine->notifiedParams.size(), notified);
                patcher.process(nullptr, 0, nullptr, 0, VECTORSIZE);
                expectEquals(testEngine->executedEvents.size(), executed + 1);
                expectEquals(testEngine->notifiedParams.size(), notified + 1);
            }

            {
                beginTest("Process Time");
                rnbomaticMinimal<TestEngine> patcher;
                patcher.initialize();
                patcher.prepareToProcess(SAMPLERATE, VECTORSIZE, true);
                expectEquals(patcher.getEngine()->getCurrentTime(), (MillisecondTime)0);
                patcher.process(nullptr, 0, nullptr, 0, VECTORSIZE);
                expectEquals(patcher.getEngine()->getCurrentTime(), sampsToMs(VECTORSIZE));
            }
        }
    };
    Tests tests;
}
#endif
