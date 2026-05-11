//
//  RNBO_PatcherState.h
//  Created: 27 Jan 2016 1:43:53pm
//  Author:  Stefan Brunner
//
//

#ifndef _RNBO_Patcherstate_H_
#define _RNBO_Patcherstate_H_

#include <unordered_map>
#include <vector>
#include <memory>

#include "RNBO_Debug.h"
#include "RNBO_String.h"
#include "RNBO_Types.h"
#include "RNBO_ExternalPtr.h"
#include "RNBO_PatcherEventTarget.h"
#include "RNBO_PatcherStateInterface.h"
#include "RNBO_List.h"
#include "RNBO_DataRef.h"

#include "3rdparty/MPark_variant/variant.hpp"

namespace RNBO {

	class ValueHolder;
	class PatcherState;
	using StateMapPtr = std::shared_ptr<PatcherState>;
	using SubStateMapType = std::unordered_map<Index,std::shared_ptr<ValueHolder> >;
	using SubStateMapPtr = std::shared_ptr<SubStateMapType>;

	/**
	 * @brief A class to hold a RNBO value (e.g. number, list, etc.)
	 */
	class ValueHolder {
	public:

		/**
		 * @brief A RNBO value type variant
		 */
		enum Type {
			NONE,           ///< no value
			FLOAT,          ///< floating point number
			DOUBLE,         ///< double precision floating point number
			EXTERNAL,       ///< @private
			SUBSTATE,       ///< subpatcher state
			SUBSTATEMAP,    ///< subpatcher state map
			EVENTTARGET,    ///< @private
			LIST,           ///< list of numbers
			DATAREF,        ///< @private
			MULTIREF,       ///< @private
			SIGNAL,         ///< @private
			STRING,         ///< @private
			UINT32,         ///< @private
			BOOLEAN,        ///< @private
			INTVALUE,       ///< @private
			UINT64,        	///< @private
            BUFFER          ///< serialized content of a data buffer
		};

		/**
		 * @brief Construct a ValueHolder from a float
		 */
		ValueHolder(float val);

		/**
		 * @brief Construct a ValueHolder from a float
		 */
		ValueHolder(double val);

		/**
		 * @brief Construct a ValueHolder from an integer
		 */
		ValueHolder(Int val);

		/**
		 * @brief Construct a ValueHolder from an Index
		 */
		ValueHolder(UInt32 val);

		/**
		 * @brief Construct a ValueHolder from an Index
		 */
		ValueHolder(UInt64 val);

		/**
		 * @brief Construct a ValueHolder from a bool
		 */
		ValueHolder(bool val);

		/**
		 * @private
		 */
		ValueHolder(ExternalPtr& ext);

		/**
		 * @brief Construct a ValueHolder from a PatcherState
		 *
		 * @param substate a std::shared_pointer<PatcherState>
		 */
		ValueHolder(StateMapPtr substate);

		/**
		 * @private
		 */
		ValueHolder(PatcherEventTarget* patcherEventTarget);

		/**
		 * @brief Construct a ValueHolder from a list
		 */
		ValueHolder(const list& theList);

		/**
		 * @private
		 */
		ValueHolder(DataRef&& dataRef);

		/**
		 * @private
		 */
		ValueHolder(MultiDataRef&& dataRef);

		/**
		 * @private
		 */
		ValueHolder(const signal sig);

		/**
		 * @private
		 */
		ValueHolder(const char* str);

		/**
		 * @private
		 */
		ValueHolder(SerializedBuffer&& data);

		/**
		 * @brief Construct a value without a type
		 */
		ValueHolder();

		~ValueHolder();

		//no copy constructor/assign
		ValueHolder(const ValueHolder&) = delete;
		ValueHolder& operator=(ValueHolder&) = delete;

		//default move constructor
		ValueHolder(ValueHolder&&) = default;
		//explicit delete move assignment, mpark variant does so it would be implicitly deleted anyway
		ValueHolder& operator=(ValueHolder&&) = delete;

		explicit operator float() const;
		explicit operator double() const;
		explicit operator Int() const;
		explicit operator UInt32() const;
		explicit operator UInt64() const;
		explicit operator bool() const;
		explicit operator PatcherEventTarget*();
		explicit operator signal();

		operator ExternalPtr();
		operator const list&() const;
		operator DataRef&();
		operator MultiDataRef&();
		operator const char*() const;
		operator SerializedBuffer&();

		operator PatcherState&();
		PatcherState& operator[](Index i);

		operator const PatcherState&() const;
		const PatcherState& operator[](Index i) const;

		/**
		 * @brief Get the number of substates in the map
		 */
		Index getSubStateMapSize() const;

		/**
		 * @brief Get the type of the value variant
		 */
		Type getType() const;

	private:

		void allocateSubState();
		void allocateSubStateMap();

		mpark::variant<
			mpark::monostate,
			float,
			double,
			Int,
			UInt32,
			bool,
			ExternalPtr,
			StateMapPtr,
			PatcherEventTarget*,
			SubStateMapPtr,
			list,
			DataRef,
			MultiDataRef,
			signal,
			String,
			UInt64,
            SerializedBuffer
		> _value;
	};

	using StateMap = std::unordered_map<String, ValueHolder, StringHasher>;

	/**
	 * @brief A representation of the RNBO patcher graph
	 */
	class PatcherState : public PatcherStateInterface {
	public:

        PatcherState() {}

        PatcherState (const PatcherState&) = delete;
        PatcherState& operator= (const PatcherState&) = delete;

		using Iterator = typename StateMap::iterator;
		using ConstIterator = typename StateMap::const_iterator;

		Iterator begin()
		{
			return _map.begin();
		}

		Iterator end()
		{
			return _map.end();
		}

		ConstIterator begin() const
		{
			return _map.begin();
		}

		ConstIterator end() const
		{
			return _map.end();
		}

		/**
		 * @brief Get a substate (e.g. a subpatcher)
		 *
		 * @param key the substate name
		 * @return PatcherState&
		 */
		PatcherState& getSubState(const char* key) override;

		/**
		 * @brief Get a substate (e.g. a subpatcher) from an array of multiple substates
		 *
		 * @param key the substate name
		 * @param i the index of the substate in the array
		 * @return PatcherState&
		 */
		PatcherState& getSubStateAt(const char* key, Index i) override;

		StateHelper<PatcherState> operator[](const char* key) {
			StateHelper<PatcherState> helper(key, *this);
			return helper;
		}

        virtual bool isDummy() const { return false; }

	private:

		friend class StateHelper<PatcherState>;

		void add(const char* key, float val) override;
		void add(const char* key, double val) override;
		void add(const char* key, Int val) override;
		void add(const char* key, UInt32 val) override;
		void add(const char* key, UInt64 val) override;
		void add(const char* key, bool val) override;
		void add(const char* key, ExternalPtr& ext) override;
		void add(const char* key, PatcherEventTarget* patcherEventTarget) override;
		void add(const char* key, const list& theList) override;
		void add(const char* key, DataRef& dataRef) override;
		void add(const char* key, MultiDataRef& dataRef) override;
		void add(const char* key, signal sig) override;
		void add(const char* key, const char* str) override;
        void add(const char* key, SerializedBuffer& data) override;

		float getFloat(const char* key) override;
		double getDouble(const char* key) override;
		Int getInt(const char* key) override;
		UInt32 getUInt32(const char* key) override;
		UInt64 getUInt64(const char* key) override;
		bool getBool(const char* key) override;
		ExternalPtr getExternalPtr(const char* key) override;
		PatcherEventTarget* getEventTarget(const char* key) override;
		const PatcherState& getSubState(const char* key) const override;
		const PatcherState& getSubStateAt(const char* key, Index i) const override;
		bool isEmpty() const override;
		list getList(const char* key) override;
		DataRef& getDataRef(const char *key) override;
		MultiDataRef& getMultiDataRef(const char *key) override;
		signal getSignal(const char *key) override;
		const char* getString(const char *key) override;
        SerializedBuffer& getBuffer(const char *key) override;

		bool containsValue(const char* key) const override;

		StateMap _map;
	};

}

#endif  // _RNBO_Patcherstate_H_
