//
//  RNBO_BBUEvent.h
//
//

#ifndef _RNBO_BBUEvent_H_
#define _RNBO_BBUEvent_H_

#include "RNBO_Types.h"

namespace RNBO {

	class PatcherEventTarget;

    /**
     * An event representing bars beats and units
     */
	class BBUEvent {

	public:

		BBUEvent()
		: _eventTime(0)
		{
		}

		~BBUEvent()
		{
		}

		BBUEvent(
			MillisecondTime eventTime,
			number bars,
			number beats,
			number units
		)
		: _eventTime(eventTime)
		, _bars(bars)
		, _beats(beats)
		, _units(units)
		{
		}

		BBUEvent(const BBUEvent& other)
		: _eventTime(other._eventTime)
		{
			_bars = other._bars;
			_beats = other._beats;
			_units = other._units;
		}

		BBUEvent(BBUEvent&& other)
		: _eventTime(other._eventTime)
		{
			_bars = other._bars;
			_beats = other._beats;
			_units = other._units;
		}

		BBUEvent& operator = (const BBUEvent& other)
		{
			_eventTime = other._eventTime;
			_bars = other._bars;
			_beats = other._beats;
			_units = other._units;

			return *this;
		}

		bool operator==(const BBUEvent& rhs) const
		{
			return rhs.getTime() == getTime()
				&& rhs.getBars() == getBars()
				&& rhs.getBeats() == getBeats()
				&& rhs.getUnits() == getUnits();
		}

		MillisecondTime getTime() const { return _eventTime; }
		number getBars() const { return _bars; }
		number getBeats() const { return _beats; }
		number getUnits() const { return _units; }

		// we will always use the default event target (the top level patcher)
		PatcherEventTarget* getEventTarget() const { return nullptr; }

		// debugging
		void dumpEvent() const {
			// disabling for now to avoid requiring fprintf support in generated code
//			fprintf(stdout, "BBUEvent: time=%.3f state=%d", _eventTime, _state);
		}

	protected:

		MillisecondTime		_eventTime;

		number _bars = 0;
		number _beats = 0;
		number _units = 0;

		friend class EventVariant;

		void setTime(MillisecondTime eventTime) { _eventTime = eventTime; }
	};

} // namespace RNBO

#endif // #ifndef _RNBO_BBUEvent_H_
