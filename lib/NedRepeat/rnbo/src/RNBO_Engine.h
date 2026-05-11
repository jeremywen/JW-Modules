//
//  RNBO_Engine.h
//
//  Created by stb on 07/08/15.
//
//

#ifndef _RNBO_Engine_h
#define _RNBO_Engine_h

#include "RNBO.h"

#include "RNBO_EngineCore.h"
#include "RNBO_ParameterEventQueue.h"
#include "RNBO_PatcherState.h"
#include "RNBO_ParameterInterfaceAsync.h"
#include "RNBO_HardwareDevice.h"

#include <mutex>
#include <vector>
#include <thread>
#include <map>

namespace RNBO {

	using ExternalDataQueue = EventQueue<ExternalDataEvent, moodycamel::ReaderWriterQueue<ExternalDataEvent>>;
	using ExternalDataRefMap = std::unordered_map<String, std::shared_ptr<ExternalDataRef>, StringHasher >;
	using ExternalDataRefList = Vector<ExternalDataRef*>;

	/**
	 * @private
	 * The Engine is used to drive the RNBO patcher. It holds common code for
	 * feeding data to, driving, and getting data back from the RNBO patcher.
	 */
	class Engine : public EngineCore
	{
	public:

		/**
		 * @brief Construct a new Engine instance to wrap/execute a RNBO
		 * patcher.
		 *
		 * This patcher is generated C++ code taht you can interact with via a
		 * PatcherInterface. All interactions with this code must go through
		 * this Engine interface.
		 */
		Engine();
		~Engine() override;

		PatcherInterface& getPatcher() const override { return *_patcher; }

		/**
		 * @brief Replace the currently running patcher
		 * @return true replacment was successful
		 * @return false otherwise
		 */
		bool setPatcher(UniquePtr<PatcherInterface> patcher) override;

		/**
		 * @brief update the targets for already scheduled events when a new
		 * patcher is set
		 */
		void updatePatcherEventTarget(const EventTarget *oldTarget, PatcherEventTarget *newTarget) override;
		void rescheduleEventTarget(const EventTarget *target) override;

		ParameterEventInterfaceUniquePtr createParameterInterface(ParameterEventInterface::Type type, EventHandler* handler) override;

		bool prepareToProcess(number sampleRate, Index maxBlockSize, bool force = false) override;

		void process(const SampleValue* const* audioInputs, Index numInputs,
					 SampleValue* const* audioOutputs, Index numOutputs,
					 Index sampleFrames,
					 const MidiEventList* midiInput, MidiEventList* midiOutput) override;

		// used by ParameterInterfaceAsync and ParameterInterfaceAsyncImpl
		void queueServiceEvent(const ServiceNotification& event);
		void registerAsyncParameterInterface(ParameterEventInterfaceImplPtr pi);

		/**
		 * @brief Check whether we are within the processing thread
		 *
		 * Mostly used for debugging purposes.
		 *
		 * @return true if called from inside the process() call on the thread
		 * calling process()
		 * @return false otherwise
		 */
		bool insideProcessThread();

		ExternalDataIndex getNumExternalDataRefs() const;
		ExternalDataId getExternalDataId(ExternalDataIndex index) const;
		const ExternalDataInfo getExternalDataInfo(ExternalDataIndex index) const;

		void setExternalData(ExternalDataId memoryId, char *data, size_t sizeInBytes, DataType type, ReleaseCallback callback);
		void releaseExternalData(ExternalDataId memoryId);
		void setExternalDataHandler(ExternalDataHandler* handler);

		void setPresetSync(UniquePresetPtr preset) override;
		ConstPresetPtr getPresetSync() override;

        void scheduleClockEvent(EventTarget* eventTarget, ClockId clockIndex, MillisecondTime time) override;
        void scheduleClockEventWithValue(EventTarget* eventTarget, ClockId clockIndex, MillisecondTime time, ParameterValue value) override;

		void beginProcessDataRefs() override;
		void endProcessDataRefs() override;

 private:

		void handleServiceQueue();
		void clearInactiveParameterInterfaces();
		size_t InitialActiveParameterInterfaces() override;

		void updateExternalDataRefs();

		// list of all currently existing parameter interfaces, might contain inactive ones, that
		// will be deleted when a new one is requested
		std::mutex											_registeredParameterInterfacesMutex;
		std::vector<ParameterEventInterfaceImplPtr>			_registeredParameterInterfaces;

		ExternalDataRefMap									_externalDataRefMap;
		ExternalDataRefList									_externalDataRefs;

		ExternalDataQueue									_externalDataQueueIn;
		ExternalDataQueue									_externalDataQueueOut;
		ExternalDataHandler*								_externalDataHandler = nullptr;

		std::mutex											_serviceQueueMutex;
		ServiceNotificationQueue							_serviceQueue;

		// suppress any processing while we are setting a new patcher
		std::atomic<bool>									_inSetPatcher;
		std::atomic<bool>									_inProcess;

		std::thread::id										_processThreadID;

		friend class SetPatcherLocker;
		friend class ProcessLocker;

	};

}  // namespace RNBO


#endif // ifndef _RNBO_Engine_h
