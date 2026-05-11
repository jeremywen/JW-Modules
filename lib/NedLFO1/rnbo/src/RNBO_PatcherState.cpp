//
//  RNBO_PatcherState.cpp
//  Created: 17 Feb 2016 2:02:52pm
//  Author:  stb
//
//

#include "RNBO_PatcherState.h"
#include "RNBO_AudioSignal.h"

namespace RNBO {

	ValueHolder::ValueHolder(float val)
	: _value(val)
	{}

	ValueHolder::ValueHolder(double val)
	: _value(val)
	{}

	ValueHolder::ValueHolder(Int val)
	: _value((Int)val)
	{}

	ValueHolder::ValueHolder(UInt32 val)
	: _value(val)
	{}

	ValueHolder::ValueHolder(UInt64 val)
	: _value(val)
	{}

	ValueHolder::ValueHolder(bool val)
	: _value(val)
	{}

	ValueHolder::ValueHolder(ExternalPtr& ext)
	: _value(std::move(ext))
	{}

	ValueHolder::ValueHolder(StateMapPtr substate)
	: _value(substate)
	{}

	ValueHolder::ValueHolder(PatcherEventTarget* patcherEventTarget)
	: _value(patcherEventTarget)
	{}

	ValueHolder::ValueHolder(const list& theList)
	: _value(theList)
	{}

	ValueHolder::ValueHolder(DataRef&& dataRef)
	: _value(std::move(dataRef))
	{}

	ValueHolder::ValueHolder(MultiDataRef&& dataRef)
	: _value(std::move(dataRef))
	{}

    ValueHolder::ValueHolder(SerializedBuffer&& data)
    : _value(std::move(data))
    {}

	ValueHolder::ValueHolder(const signal sig)
	: _value(sig)
	{}

	ValueHolder::ValueHolder(const char* str)
	: _value(String(str))
	{}

	ValueHolder::ValueHolder()
	{}

    ValueHolder::~ValueHolder() {
        if (getType() == ValueHolder::SIGNAL) {
            signal _sig = mpark::get<signal>(_value);
            if (_sig) {
                freeSignal(_sig);
                _value = (signal) nullptr;
            }
        }
    }

	ValueHolder::operator float() const { return mpark::get<float>(_value); }
	ValueHolder::operator double() const { return mpark::get<double>(_value); }
	ValueHolder::operator Int() const { return mpark::get<Int>(_value); }
	ValueHolder::operator UInt32() const { return mpark::get<UInt32>(_value); }
	ValueHolder::operator UInt64() const { return mpark::get<UInt64>(_value); }
	ValueHolder::operator bool() const { return mpark::get<bool>(_value); }
	ValueHolder::operator PatcherEventTarget*() { return mpark::get<PatcherEventTarget*>(_value); }
    ValueHolder::operator signal() {
        signal _sig = mpark::get<signal>(_value);
        _value = (signal)nullptr;
        return _sig;
    }

	ValueHolder::operator ExternalPtr() { return std::move(mpark::get<ExternalPtr>(_value)); }
	ValueHolder::operator const list&() const { return mpark::get<list>(_value); }
	ValueHolder::operator DataRef&() { return mpark::get<DataRef>(_value); }
	ValueHolder::operator MultiDataRef&() { return mpark::get<MultiDataRef>(_value); }
	ValueHolder::operator const char*() const { return mpark::get<String>(_value).c_str(); }
    ValueHolder::operator SerializedBuffer&() { return mpark::get<SerializedBuffer>(_value); }

	ValueHolder::operator PatcherState&() {
		if (!mpark::holds_alternative<StateMapPtr>(_value)) allocateSubState();
		return *mpark::get<StateMapPtr>(_value);
	}
	PatcherState& ValueHolder::operator[](Index i) {
		if (!mpark::holds_alternative<SubStateMapPtr>(_value)) allocateSubStateMap();
		SubStateMapPtr subPtr = mpark::get<SubStateMapPtr>(_value);
		if (subPtr->find(i) == subPtr->end()) {
			auto ps = std::make_shared<ValueHolder>();
			subPtr->insert({ i, ps });
		}
		return *(subPtr->at(i));
	}

	ValueHolder::operator const PatcherState&() const
	{
		return *mpark::get<StateMapPtr>(_value);
	}

	const PatcherState& ValueHolder::operator[](Index i) const
	{
		std::shared_ptr<ValueHolder> ps = mpark::get<SubStateMapPtr>(_value)->at(i);
		if (!ps) {
			ps = std::make_shared<ValueHolder>();
			mpark::get<SubStateMapPtr>(_value)->at(i) = ps;
		}
		return *mpark::get<SubStateMapPtr>(_value)->at(i);
	}

	Index ValueHolder::getSubStateMapSize() const
	{
		return mpark::holds_alternative<SubStateMapPtr>(_value) ? mpark::get<SubStateMapPtr>(_value)->size() : 0;
	}

	void ValueHolder::allocateSubState()
	{
		RNBO_ASSERT(getType() == ValueHolder::NONE);
		_value = std::make_shared<PatcherState>();
	}

	void ValueHolder::allocateSubStateMap()
	{
		RNBO_ASSERT(getType() == ValueHolder::NONE);
		_value = std::make_shared<SubStateMapType>();
	}

	struct GetTypeVisitor {
		constexpr ValueHolder::Type operator()(const mpark::monostate&) const { return ValueHolder::NONE; }
		constexpr ValueHolder::Type operator()(const float&) const { return ValueHolder::FLOAT; }
		constexpr ValueHolder::Type operator()(const double&) const { return ValueHolder::DOUBLE; }
		constexpr ValueHolder::Type operator()(const Int&) const { return ValueHolder::INTVALUE; }
		constexpr ValueHolder::Type operator()(const UInt32&) const { return ValueHolder::UINT32; }
		constexpr ValueHolder::Type operator()(const UInt64&) const { return ValueHolder::UINT64; }
		constexpr ValueHolder::Type operator()(const bool&) const { return ValueHolder::BOOLEAN; }
		constexpr ValueHolder::Type operator()(const ExternalPtr&) const { return ValueHolder::EXTERNAL; }
		constexpr ValueHolder::Type operator()(const StateMapPtr&) const { return ValueHolder::SUBSTATE; }
		constexpr ValueHolder::Type operator()(const PatcherEventTarget&) const { return ValueHolder::EVENTTARGET; }
		constexpr ValueHolder::Type operator()(const SubStateMapPtr&) const { return ValueHolder::SUBSTATEMAP; }
		constexpr ValueHolder::Type operator()(const list&) const { return ValueHolder::LIST; }
		constexpr ValueHolder::Type operator()(const DataRef&) const { return ValueHolder::DATAREF; }
		constexpr ValueHolder::Type operator()(const MultiDataRef&) const { return ValueHolder::MULTIREF; }
		constexpr ValueHolder::Type operator()(const signal&) const { return ValueHolder::SIGNAL; }
		constexpr ValueHolder::Type operator()(const String&) const { return ValueHolder::STRING; }
        constexpr ValueHolder::Type operator()(const SerializedBuffer&) const { return ValueHolder::BUFFER; }
	};

	ValueHolder::Type ValueHolder::getType() const
	{
		return mpark::visit(GetTypeVisitor{}, _value);
	}

	void PatcherState::add(const char* key, float val)
	{
		_map.emplace(key, val);
	}

	void PatcherState::add(const char* key, double val)
	{
		_map.emplace(key, val);
	}

	void PatcherState::add(const char* key, Int val)
	{
		_map.emplace(key, val);
	}

	void PatcherState::add(const char* key, UInt32 val)
	{
		_map.emplace(key, val);
	}

	void PatcherState::add(const char* key, UInt64 val)
	{
		_map.emplace(key, val);
	}

	void PatcherState::add(const char* key, bool val)
	{
		_map.emplace(key, val);
	}

	void PatcherState::add(const char* key, ExternalPtr& ext)
	{
		_map.emplace(key, ext);
	}

	void PatcherState::add(const char* key, PatcherEventTarget* patcherEventTarget)
	{
		_map.emplace(key, patcherEventTarget);
	}

	void PatcherState::add(const char* key, const list& theList)
	{
		_map.emplace(key, theList);
	}

	void PatcherState::add(const char* key, DataRef& dataRef)
	{
		_map.emplace(key, std::move(dataRef));
	}

	void PatcherState::add(const char* key, MultiDataRef& dataRef)
	{
		_map.emplace(key, std::move(dataRef));
	}

	void PatcherState::add(const char* key, signal sig)
	{
		_map.emplace(key, sig);
	}

	void PatcherState::add(const char* key, const char* str)
	{
		_map.emplace(key, str);
	}

    void PatcherState::add(const char* key, SerializedBuffer& data)
    {
        _map.emplace(key, std::move(data));
    }

	float PatcherState::getFloat(const char* key)
	{
		return (float)_map[key];
	}

	double PatcherState::getDouble(const char* key)
	{
		return (double)_map[key];
	}

	Int PatcherState::getInt(const char* key)
	{
		return static_cast<Int>(_map[key]);
	}

	UInt32 PatcherState::getUInt32(const char* key)
	{
		return static_cast<UInt32>(_map[key]);
	}

	UInt64 PatcherState::getUInt64(const char* key)
	{
		return static_cast<UInt64>(_map[key]);
	}

	bool PatcherState::getBool(const char* key)
	{
		return static_cast<bool>(_map[key]);
	}

	ExternalPtr PatcherState::getExternalPtr(const char* key)
	{
		return _map[key];
	}

	PatcherEventTarget* PatcherState::getEventTarget(const char* key)
	{
		return (PatcherEventTarget*)_map[key];
	}

	PatcherState& PatcherState::getSubState(const char* key)
	{
		return _map[key];
	}

	PatcherState& PatcherState::getSubStateAt(const char* key, Index i)
	{
		return _map[key][i];
	}

	const PatcherState& PatcherState::getSubState(const char* key) const
	{
		return _map.at(key);
	}

	const PatcherState& PatcherState::getSubStateAt(const char* key, Index i) const
	{
		return _map.at(key)[i];
	}

	bool PatcherState::isEmpty() const {
		return _map.empty();
	}

	list PatcherState::getList(const char* key)
	{
		const ValueHolder& value = _map[key];
		const list& list = value;
		return list;
	}

	DataRef& PatcherState::getDataRef(const char* key)
	{
		return _map[key];
	}

	MultiDataRef& PatcherState::getMultiDataRef(const char* key)
	{
		return _map[key];
	}

	signal PatcherState::getSignal(const char* key)
	{
		return (signal)_map[key];
	}

	const char* PatcherState::getString(const char* key)
	{
		return _map[key];
	}

    SerializedBuffer& PatcherState::getBuffer(const char *key)
    {
        return _map[key];
    }

	bool PatcherState::containsValue(const char* key) const
	{
		return _map.find(key) != _map.end();
	}
}
