//
//  RNBO_DataRef.h
//
//

#ifndef _RNBO_DATAREF_H_
#define _RNBO_DATAREF_H_

#include "RNBO_Types.h"

#ifndef RNBO_NOSTDLIB
#include <atomic>
#endif

namespace RNBO {

	// it is hard coded for now
	#define RNBO_DEFAULT_SAMPLERATE 44100

	/**
	 * @brief Metadata info for an UntypedBuffer (empty)
	 *
	 * Required for compatibility between untyped and audio buffers
	 */
	struct UntypedBufferInfo {

	};

	/**
	 * @brief Stores metadata info for an audio buffer
	 */
	struct AudioBufferInfo {
		size_t channels;
		number samplerate;
	};

	/**
	  * @brief A RNBO external data reference type
	  *
	  * There are two categories of data references - audio buffers and memory buffers.
	  * Audio buffers are specialized for handling audio and contain information about
	  * channel count and samplerate, for example. Memory buffers are intended for all other
	  * data storage and transfer uses.
	  */
	struct DataType {
        /** Valid types of external data refs */
		enum Type {
			Untyped,  ///< generic, untyped memory buffer
			Float32AudioBuffer,  ///< 32-bit float audio buffer
			Float64AudioBuffer,  ///< 64-bit float audio buffer
            SampleAudioBuffer,  ///< 64 or 32-bit float audio buffer
			TypedArray  ///< typed memory buffer
		} type = Untyped;


		union {
			UntypedBufferInfo	untypedBufferInfo;
			AudioBufferInfo		audioBufferInfo;
		};

		bool operator==(const DataType& rhs) const
		{
			if (type != rhs.type) return false;
			switch (type) {
				case Untyped: return true;
				case TypedArray: return true;
				case Float32AudioBuffer:
				case Float64AudioBuffer:
                case SampleAudioBuffer:
					return (audioBufferInfo.channels == rhs.audioBufferInfo.channels
							&& audioBufferInfo.samplerate == rhs.audioBufferInfo.samplerate);
			}

			return false;
		}

		bool operator!=(const DataType& rhs) const
		{
			return !operator==(rhs);
		}

		bool matches(const DataType& rhs) const
		{
			if (type == Untyped) return false;
			return type == rhs.type;
		}
	};

	/**
	 * @private
	 */
	class DataRef {
	public:

		DataRef()
		: _name(nullptr)
		, _file(nullptr)
		, _tag(nullptr)
		, _sizeInBytes(0)
		, _data(nullptr)
		, _touched(false)
		{}

		DataRef(DataRef&& other)
		{
			_name = other._name;
			_file = other._file;
			_sizeInBytes = other._sizeInBytes;
			_data = other._data;
			_type = other._type;
			_touched = other._touched;
			_tag = other._tag;
			_internal = other._internal;
			_index = other._index;
			_requestedSizeInBytes = other._requestedSizeInBytes;
			_allocatedSizeInBytes = other._allocatedSizeInBytes;
			_deAlloc = other._deAlloc;

			// now reset the other one
			other._data = nullptr;
			other._sizeInBytes = 0;
			other._deAlloc = false;
			other._name = nullptr;
			other._file = nullptr;
			other._requestedSizeInBytes = 0;
			other._index = -1;
			other._tag = nullptr;
		}

		void operator=(const DataRef& ref ) = delete;

		/**
		 * @brief Move assignment operator
		 *
		 * @param other DataRef to move from
		 * @return DataRef&
		 */
		DataRef& operator=(DataRef&& other)
		{
			if (this != &other) {
				_name = other._name;
				_file = other._file;
				_sizeInBytes = other._sizeInBytes;
				_data = other._data;
				_type = other._type;
				_touched = other._touched;
				_deAlloc = other._deAlloc;
				_tag = other._tag;
				_internal = other._internal;
				_index = other._index;
				_requestedSizeInBytes = other._requestedSizeInBytes;
				_allocatedSizeInBytes = other._allocatedSizeInBytes;

				// now reset the other one
				other._data = nullptr;
				other._sizeInBytes = 0;
				other._deAlloc = false;
				other._name = nullptr;
				other._file = nullptr;
				other._requestedSizeInBytes = 0;
				other._allocatedSizeInBytes = 0;
				other._index = -1;
				other._tag = nullptr;
			}

			return *this;
		}

        void operator=(DataRef& other)
        {
            if (this != &other) {
                _name = other._name;
				_file = other._file;
                _sizeInBytes = other._sizeInBytes;
                _data = other._data;
                _type = other._type;
                _touched = other._touched;
                _deAlloc = other._deAlloc;
				_tag = other._tag;
				_internal = other._internal;
				_index = other._index;
				_requestedSizeInBytes = other._requestedSizeInBytes;
				_allocatedSizeInBytes = other._allocatedSizeInBytes;

                // now reset the other one
                other._data = nullptr;
                other._sizeInBytes = 0;
                other._deAlloc = false;
                other._name = nullptr;
				other._file = nullptr;
				other._requestedSizeInBytes = 0;
				other._allocatedSizeInBytes = 0;
				other._index = -1;
				other._tag = nullptr;
            }
        }

		DataRef* operator->() {
			return this;
		}

		~DataRef() {
			freeIfNeeded();
		}

		inline DataType getType() const {
			return _type;
		}

		void setType(DataType type) {
			_type = type;
		}

		inline const char *getName() const {
			return _name;
		}

		void setName(const char *name) {
			if (name && Platform::strlen(name)) _name = name;
			else _name = nullptr;
		}

		inline const char *getFile() const {
			return _file;
		}

		void setFile(const char *file) {
			if (file && Platform::strlen(file)) _file = file;
			else _file = nullptr;
		}

		inline char *getData() {
			return _data;
		}

		const char *getData() const {
			return _data;
		}

		void setData(char *data, size_t sizeInBytes, bool deAlloc = false) {
			freeIfNeeded();

			_data = data;
			_sizeInBytes = _allocatedSizeInBytes = sizeInBytes;
			// normally we now hold external data, but in the WASM case
			// we want to de-allocate
			_deAlloc = deAlloc;
		}

		inline size_t getSizeInBytes() const {
			return _sizeInBytes;
		}

		void requestSizeInBytes(size_t size, bool force) {
			if (size > _requestedSizeInBytes || force) _requestedSizeInBytes = size;
		}

		bool hasRequestedSize() const {
			return _requestedSizeInBytes > 0;
		}

		void resetRequestedSizeInByte() {
			_requestedSizeInBytes = 0;
		}

		inline bool getTouched() const {
			return _touched;
		}

		void setTouched(bool value) {
			_touched = value;
		}

		void allocateIfNeeded() {
			if (_requestedSizeInBytes) {
				if (_requestedSizeInBytes > _allocatedSizeInBytes || !_deAlloc) {
					auto oldData = _data;
					_data = static_cast<char *>(Platform::calloc(_requestedSizeInBytes, 1));
					if (_deAlloc && oldData) {
						// we own the old data, so copy and free it
						Platform::memcpy(_data, oldData, _sizeInBytes);
						Platform::free(oldData);
						_wantsFill = false;
					}
					else {
						_wantsFill = true;
					}
					_deAlloc = true;

					_sizeInBytes = _allocatedSizeInBytes = _requestedSizeInBytes;
				}
				else {
					// zero out padding if we grow
					if (_requestedSizeInBytes > _sizeInBytes) {
						Platform::memset(_data + _sizeInBytes, 0, _requestedSizeInBytes - _sizeInBytes);
					}
					// if we didn't need to allocate, we just reset our size to the requested value
					_sizeInBytes = _requestedSizeInBytes;
				}
			}
		}

		void freeIfNeeded() {
			if (_deAlloc && _data != nullptr) {
				Platform::free(_data);
				_data = nullptr;
				_deAlloc = false;
			}
		}

		inline bool wantsFill() const {
			return _wantsFill;
		}

		void setWantsFill(bool value) {
			_wantsFill = value;
		}

		inline const char *getTag() const {
			return _tag;
		}

		void setTag(const char *tag) {
			if (tag && Platform::strlen(tag)) _tag = tag;
			else _tag = nullptr;
		}

		void setInternal(bool value) {
			_internal = value;
		}

		inline bool isInternal() const {
			return _internal;
		}

		void setZero() {
			Platform::memset(_data, 0, _sizeInBytes);
		}

		void setIndex(DataRefIndex index) {
			_index = index;
		}

		DataRefIndex getIndex() {
			return _index;
		}

	private:
		const char*		_name = nullptr;
		const char*		_file = nullptr;
		const char*		_tag = nullptr;

		size_t			_sizeInBytes = 0;
		size_t			_requestedSizeInBytes = 0;
		size_t			_allocatedSizeInBytes = 0;
		char			*_data = nullptr;
		bool			_touched = false;
		bool			_deAlloc = false;
		bool			_wantsFill = false;
		bool			_internal = false;
		DataRefIndex	_index = -1;

		DataType		_type;
	};

	// set the name of the DataRef, this is needed for Javascript and C to be similar
	// might go away in the future
	ATTRIBUTE_UNUSED
	static inline DataRef& initDataRef(DataRef& ref, const char *name, bool internal, const char* file, const char* tag) {
		ref.setName(name);
		ref.setInternal(internal);
		ref.setFile(file);
		ref.setTag(tag);
		return ref;
	}

	/**
	 * @private
	 */
	class MultiDataRef
	{
	public:
		template<typename... Ts> MultiDataRef(Ts & ... args)
		: _count(sizeof ... (args))
		{
			_refs = static_cast<DataRef**>(Platform::malloc(_count * sizeof(DataRef *)));
			DataRef* refs[sizeof...(args)] = {static_cast<DataRef*>(&args)...};
			for (size_t i = 0; i < _count; i++) {
				_refs[i] = refs[i];
			}
		}

		MultiDataRef() = default;

		MultiDataRef(MultiDataRef&& other)
		{
			_count = other._count;
			_current = other._current;
			if (!_refs) {
				_refs = other._refs;
				other._refs = nullptr;
			}
			_index = other._index;

		}

		void operator=(const MultiDataRef& ref ) = delete;

		MultiDataRef& operator=(MultiDataRef&& other)
		{
			if (this != &other) {
				_count = other._count;
				_current = other._current;
				if (!_refs) {
					_refs = other._refs;
					other._refs = nullptr;
				}
				_index = other._index;
			}

			return *this;
		}

		void operator=(MultiDataRef& other)
		{
			if (this != &other) {
				_count = other._count;
				_current = other._current;
				if (!_refs) {
					_refs = other._refs;
					other._refs = nullptr;
				}
				_index = other._index;
			}
		}


		~MultiDataRef() {
			if (_refs) {
				Platform::free(_refs);
			}
		}

		MultiDataRef* operator->() {
			return this;
		}

		void setCurrent(DataRefIndex current) {
			if (current >= 0 && current < static_cast<DataRefIndex>(_count)) {
				_current = current;
			}
		}

		DataRef& getCurrent() const {
			return *_refs[_current];
		}

        DataRefIndex getCurrentIndex() const {
            return _current;
        }

		void setIndex(DataRefIndex index) {
			_index = index;
		}

		DataRefIndex getIndex() const {
			return _index;
		}

	private:
		Index			_count = 0;
		DataRefIndex 	_current = 0;
		DataRef**		_refs = nullptr;
		DataRefIndex	_index = 0;
	};

	template<typename ... Ts> MultiDataRef initMultiRef(Ts & ... args)
	{
		MultiDataRef ref(args...);
		return ref;
	}

	/**
	 * @private
	 */
	template<typename T, typename DR> class DataView {
	public:
		DataView(DR& dataRef)
		: _usage(0)
		, _dataRef(&dataRef)
		{
			updateCachedSize();
		}

		virtual ~DataView() {}

		inline T& operator[](size_t i) {
			T *values = reinterpret_cast<T*>(_dataRef->getData());
			return values[i];
		}

		inline T operator[](size_t i) const {
			T *values = reinterpret_cast<T*>(_dataRef->getData());
			return values[i];
		}

		inline bool typeislockfree() const {
#if defined(_MSC_VER)
				//TODO
#elif defined(__clang__) || defined(__GNUC__)
			T dummy = 0;
			return __atomic_is_lock_free(sizeof(T), &dummy);
#endif
#ifndef RNBO_NOSTDLIB
			return std::atomic<T>{}.is_lock_free();
#else
			return false; //unknown
#endif
		}

		inline T atomicload(const Index index, int order = 5) const { //order = 5 -> memory_order_seq_cst
			if (index < getSize()) {
				T *loc = reinterpret_cast<T*>(_dataRef->getData()) + index;
#if defined(__clang__) || defined(__GNUC__)
				//https://gcc.gnu.org/wiki/Atomic/GCCMM/LIbrary
				T ret;
				int order = 5;
				//llvm's docs show a size arg as first arg but also says they use gcc's format which doesn't have the size
				__atomic_load(loc, &ret, order);
				return ret;
#elif defined(_MSC_VER)
				switch (sizeof(T)) {
					case 8:
						return InterlockedOr64(loc, 0);
					case 4:
						return InterlockedOr(loc, 0);
					case 2:
						return InterlockedOr16(loc, 0);
					case 1:
						return InterlockedOr8(loc, 0);
					default:
						return 0; //error
				}
#else
				//TODO?
				return *loc;
#endif
			}
			return 0;
		}

		inline void atomicstore(const Index index, T value, int order = 5) {   //order = 5 -> memory_order_seq_cst
			if (index < DataView<T, DR>::getSize()) {
				T *loc = reinterpret_cast<T*>(_dataRef->getData()) + index;
#if defined(__clang__) || defined(__GNUC__)
				//https://gcc.gnu.org/wiki/Atomic/GCCMM/LIbrary
				//llvm's docs show a size arg as first arg but also says they use gcc's format which doesn't have the size
				__atomic_store(loc, &value, order);
#elif defined(_MSC_VER)
				switch (sizeof(T)) {
					case 8:
						InterlockedExchange64(loc, value);
						break;
					case 4:
						InterlockedExchange(loc, value);
						break;
					case 2:
						InterlockedExchange16(loc, value);
						break;
					case 1:
						InterlockedExchange8(loc, value);
						break;
					default:
						return; //error
				}
#else
				//TODO?
				*loc = value;
#endif
			}
		}

		DataView* operator->() {
			return this;
		}

		const DataView* operator->() const {
			return this;
		}

		inline size_t getSize() const {
			return _size;
		}

		virtual void updateCachedSize() {
			_size = _dataRef->getSizeInBytes() / sizeof(T);
		}

		inline DataType getType() const {
			return _dataRef->getType();
		}

		void setType(DataType type) {
			_dataRef->setType(type);
			updateCachedSize();
		}

		inline size_t getSizeInBytes() const {
			return _dataRef->getSizeInBytes();
		}

		void setTouched(bool value) {
			_dataRef->setTouched(value);
		}

		bool getTouched() {
			return _dataRef->getTouched();
		}

		virtual DataView<T, DR>* allocateIfNeeded() {
			_dataRef->allocateIfNeeded();
			updateCachedSize();
			return this;
		}

		void setWantsFill(bool value) {
			_dataRef->setWantsFill(value);
		}

		void clear() {
			_dataRef->setData(nullptr, 0);
		}

		void setZero() {
			_dataRef->setZero();
		}

		DataRefIndex getIndex() const {
			return _dataRef->getIndex();
		}

		int			_usage;

		virtual void reInit() {
			reInit(*_dataRef);
		}

	protected:
		// the derived view class has to implement the correct version for its data format
		void requestSize(size_t size) {
			size_t sizeInBytes = size * sizeof(T);
			_dataRef->requestSizeInBytes(sizeInBytes, false);
		}

		// the derive view class has to implement the correct version for its data format
		virtual DataView<T, DR>* setSize(size_t size) {
			size_t sizeInBytes = size * sizeof(T);
			_dataRef->requestSizeInBytes(sizeInBytes, true);
			_dataRef->allocateIfNeeded();
			updateCachedSize();
			return this;
		}

		void reInit(DR& dataRef) {
			_dataRef = &dataRef;
			updateCachedSize();
		}

		size_t		_size;
		DR*		_dataRef;
	};

	/**
	 * @private
	 * A helper class to make a pointer to a DataView look more like JavaScript
	 */
	template<typename T, typename U> class DataViewRef {
	public:
		DataViewRef()
		{}

		virtual ~DataViewRef()
		{
			freeView();
		}

		DataViewRef(const DataViewRef<T, U>& other)
		{
			other._view->_usage++;
			freeView();
			_view = other._view;
		}

		DataViewRef(U* view)
		{
			view->_usage++;
			freeView();
			_view = view;
		}

		void operator=(U* view )
		{
			view->_usage++;
			freeView();
			_view = view;
		}

		void operator=(const DataViewRef<T, U>& other)
		{
			if (&other != this) {
				other._view->_usage++;
				freeView();
				_view = other._view;
			}
		}

		inline T& operator[](size_t i) {
			return (*_view)[i];
		}

		inline T operator[](size_t i) const {
			return (*_view)[i];
		}

		U* operator->() {
			return _view;
		}

		void freeView() {
			if (_view) {
				_view->_usage--;
				if (!_view->_usage) delete _view;
				_view = nullptr;
			}
		}

	private:
		U*	_view = nullptr;
	};

	/**
	 * @private
	 */
	template<typename T, typename DR> class OneDimensionalArray : public DataView<T, DR>
	{
	public:
		OneDimensionalArray(DR& dataRef)
		: DataView<T, DR>(dataRef)
		{
			DataType info;
			info.type = DataType::TypedArray;
			DataView<T, DR>::setType(info);
		}

		void requestSize(size_t size) {
			DataView<T, DR>::requestSize(size);
		}

		inline size_t getChannels() const {
			return 1;
		}

		inline number getSampleRate() const {
			return 0.0;
		}

		virtual OneDimensionalArray<T, DR>* allocateIfNeeded() override {
			DataView<T, DR>::allocateIfNeeded();
			return this;
		}

		inline T getSample(const size_t /*channel*/, const size_t index) const {
			return DataView<T, DR>::operator[](index);
		}

		inline T getSampleSafe(const long channel, const long index) const {
			if (channel == 0 && index < DataView<T, DR>::getSize()) {
				return DataView<T, DR>::operator[](index);
			}
			return 0;
		}

		// channel numbers are 0 based
		inline void setSample(const size_t /*channel*/, const size_t index, const T value) {
			DataView<T, DR>::operator[](index) = value;
		}

		inline void setSampleSafe(const long channel, const long index, const T value) {
			if (channel == 0 && index < DataView<T, DR>::getSize()) {
				DataView<T, DR>::operator[](index) = value;
			}
		}

		OneDimensionalArray<T, DR>* setSize(size_t size) override {
			DataView<T, DR>::setSize(size);
			return this;
		}
	};

	/**
	 * @private
	 */
	template<typename T, typename DR> class InterleavedAudioBuffer : public DataView<T, DR>
	{
	public:
		InterleavedAudioBuffer(DR& dataRef, DataType::Type type)
		: DataView<T, DR>(dataRef)
		{
			if (DataView<T, DR>::getType().type == DataType::Untyped) {
				DataType info;
				info.type = type;
				info.audioBufferInfo.channels = 0;
				info.audioBufferInfo.samplerate = RNBO_DEFAULT_SAMPLERATE;
				DataView<T, DR>::setType(info);
			}

			updateCachedSize();
		}

		virtual ~InterleavedAudioBuffer() override {}

        InterleavedAudioBuffer* operator->() {
            return this;
        }

		inline T getSample(const size_t channel, const size_t index) const {
			if (!_audioData) return 0;
			return _audioData[_channels * index + channel];
		}

		inline T getSampleSafe(const long channel, const long index) const {
			if (!_audioData) return 0;
			const auto ind = _channels * index + channel;
			if (ind < 0 || static_cast<size_t>(ind) >= DataView<T, DR>::getSize() * _channels) {
				return static_cast<T>(0);
			}
			return _audioData[ind];
		}

		// channel numbers are 0 based
		inline void setSample(const size_t channel, const size_t index, const T value) {
			if (_audioData)
				_audioData[_channels * index + channel] = value;
		}

		inline void setSampleSafe(const long channel, const long index, const T value) {
			if (channel < 0 || static_cast<size_t>(channel) >= _channels || index < 0 || static_cast<size_t>(index) >= DataView<T, DR>::getSize()) {
				return;
			}
			_audioData[_channels * index + channel] = value;
		}

		inline size_t getChannels() const {
			return _channels;
		}

		inline number getSampleRate() const {
			DataType info = DataView<T, DR>::getType();
			return info.audioBufferInfo.samplerate;
		}

		void setSampleRate(number sampleRate) {
			DataType info = DataView<T, DR>::getType();
			info.audioBufferInfo.samplerate = sampleRate;
			DataView<T, DR>::setType(info);
		}

		bool isInterleaved() {
			// for now we only support interleaved audio
			return true;
		}

		void requestSize(size_t size, size_t channels) {
			DataView<T, DR>::requestSize(size * channels);
			_requestedChannels = channels;
		}

		InterleavedAudioBuffer<T, DR>* setSize(size_t size) override {
			DataView<T, DR>::setSize(size * _channels);
			return this;
		}

		void updateCachedSize() override {
			DataType info = DataView<T, DR>::getType();
			_channels = info.audioBufferInfo.channels;
			_audioData = reinterpret_cast<T*>(DataView<T, DR>::_dataRef->getData());
			DataView<T, DR>::_size = _channels ? (DataView<T, DR>::getSizeInBytes() / sizeof(T)) / _channels : 0;
		}

		virtual InterleavedAudioBuffer<T, DR>* setChannels(size_t channels) {
			if (channels != _channels) {
				size_t currentSize = DataView<T, DR>::getSize();

				DataType info = DataView<T, DR>::getType();
				info.audioBufferInfo.channels = channels;
				DataView<T, DR>::setType(info);

				// update local cache
				_channels = channels;
				_audioData = reinterpret_cast<T*>(DataView<T, DR>::_dataRef->getData());

				// we might want to implement preserving the data in the future
				DataView<T, DR>::clear();

				DataView<T, DR>::setSize(currentSize * _channels);
			}

			return this;
		}

		InterleavedAudioBuffer<T, DR>* allocateIfNeeded() override {
			if (_requestedChannels > 0) {
				if (_channels != 0 && _requestedChannels != _channels) {
					DataView<T, DR>::setZero();
				}

				DataType info = DataView<T, DR>::getType();
				info.audioBufferInfo.channels = _requestedChannels;
				DataView<T, DR>::setType(info);

				DataView<T, DR>::allocateIfNeeded();
			}

			return this;
		}

        DataRefIndex getCurrentIndex() const {
            return 0;
        }

	private:
		size_t	_requestedChannels = 0;
		size_t	_channels = 0;
		T*		_audioData = nullptr;

	};

	/**
	 * @private
	 * specialized versions for floats - you will always need to define DataView
	 * and a DataViewRef (might also change)
	 */
	class Float32Buffer : public InterleavedAudioBuffer<float, DataRef>
	{
	public:
		Float32Buffer(DataRef& dataRef)
		: InterleavedAudioBuffer<float, DataRef>(dataRef, DataType::Float32AudioBuffer)
		{}

		virtual Float32Buffer* allocateIfNeeded() override {
			InterleavedAudioBuffer<float, DataRef>::allocateIfNeeded();
			return this;
		}

		virtual Float32Buffer* setChannels(size_t channels) override {
			InterleavedAudioBuffer<float, DataRef>::setChannels(channels);
			return this;
		}

		virtual Float32Buffer* setSize(size_t size) override {
			InterleavedAudioBuffer<float, DataRef>::setSize(size);
			return this;
		}
	};

	using Float32BufferRef = DataViewRef<float, Float32Buffer >;

	/**
	 * @private
	 */
	class Float64Buffer : public InterleavedAudioBuffer<double, DataRef>
	{
	public:
		Float64Buffer(DataRef& dataRef)
		: InterleavedAudioBuffer<double, DataRef>(dataRef, DataType::Float64AudioBuffer)
		{}

		virtual Float64Buffer* allocateIfNeeded() override {
			InterleavedAudioBuffer<double, DataRef>::allocateIfNeeded();
			return this;
		}

		virtual Float64Buffer* setChannels(size_t channels) override {
			InterleavedAudioBuffer<double, DataRef>::setChannels(channels);
			return this;
		}

		virtual Float64Buffer* setSize(size_t size) override {
			InterleavedAudioBuffer<double, DataRef>::setSize(size);
			return this;
		}
	};

	using Float64BufferRef = DataViewRef<double, Float64Buffer>;


    /**
     * @private
     */
    class SampleBuffer : public InterleavedAudioBuffer<SampleValue, DataRef>
    {
    public:
        SampleBuffer(DataRef& dataRef)
        : InterleavedAudioBuffer<SampleValue, DataRef>(dataRef, DataType::SampleAudioBuffer)
        {}

        virtual SampleBuffer* allocateIfNeeded() override {
            InterleavedAudioBuffer<SampleValue, DataRef>::allocateIfNeeded();
            return this;
        }

        virtual SampleBuffer* setChannels(size_t channels) override {
            InterleavedAudioBuffer<SampleValue, DataRef>::setChannels(channels);
            return this;
        }

        virtual SampleBuffer* setSize(size_t size) override {
            InterleavedAudioBuffer<SampleValue, DataRef>::setSize(size);
            return this;
        }
    };

    using SampleBufferRef = DataViewRef<SampleValue, SampleBuffer>;

	/**
	 * @private
	 *
	 * @brief Base class for multi buffers
	 *
	 * Since we always re-create the view when the multi buffer changes, we can
	 * here simply construct a normal data view with the currently selected
	 * underlying DataRef
	 *
	 * @tparam T
	 */
	template <typename T> class MultiBuffer : public InterleavedAudioBuffer<T, DataRef>
	{
	public:
		MultiBuffer(MultiDataRef& multiRef, DataType::Type type)
		: InterleavedAudioBuffer<T, DataRef>(multiRef.getCurrent(), type)
		, _multiRef(multiRef)
		{}

		void setCurrent(DataRefIndex current) {
			_multiRef.setCurrent(current);
		}

		DataRefIndex getIndex() const {
			return _multiRef.getIndex();
		}

		void reInit() override {
			InterleavedAudioBuffer<T, DataRef>::reInit(_multiRef.getCurrent());
		}

        DataRefIndex getCurrentIndex() const {
            return _multiRef.getCurrentIndex();
        }

	private:
		MultiDataRef&	_multiRef;
	};

	/**
	 * @private
	 */
	class Float32MultiBuffer : public MultiBuffer<float>
	{
	public:
		Float32MultiBuffer(MultiDataRef& dataRef)
		: MultiBuffer<float>(dataRef, DataType::Float32AudioBuffer)
		{}
	};

	using Float32MultiBufferRef = DataViewRef<float, Float32MultiBuffer >;

	/**
	 * @private
	 */
	class Float64MultiBuffer : public MultiBuffer<double>
	{
	public:
		Float64MultiBuffer(MultiDataRef& dataRef)
		: MultiBuffer<double>(dataRef, DataType::Float64AudioBuffer)
		{}
	};

	using Float64MultiBufferRef = DataViewRef<double, Float64MultiBuffer>;


	using Int8Buffer = OneDimensionalArray<int8_t, DataRef>;
	using Int8BufferRef = DataViewRef<int8_t, Int8Buffer>;

	using UInt8Buffer = OneDimensionalArray<uint8_t, DataRef>;
	using UInt8BufferRef = DataViewRef<uint8_t, UInt8Buffer>;

	using Int32Buffer = OneDimensionalArray<int32_t, DataRef>;
	using Int32BufferRef = DataViewRef<int32_t, Int32Buffer>;

	using UInt32Buffer = OneDimensionalArray<uint32_t, DataRef>;
	using UInt32BufferRef = DataViewRef<uint32_t, UInt32Buffer>;

	using Int64Buffer = OneDimensionalArray<int64_t, DataRef>;
	using Int64BufferRef = DataViewRef<int64_t, Int64Buffer>;

	using UInt64Buffer = OneDimensionalArray<uint64_t, DataRef>;
	using UInt64BufferRef = DataViewRef<uint64_t, UInt64Buffer>;

	using IntBuffer = OneDimensionalArray<Int, DataRef>;
	using IntBufferRef = DataViewRef<Int, IntBuffer>;

	using UIntBuffer = OneDimensionalArray<UInt, DataRef>;
	using UIntBufferRef = DataViewRef<UInt, UIntBuffer>;


    /**
     * @brief A DataBuffer that has no type
     */
    struct UntypedDataBuffer : public DataType {
        UntypedDataBuffer() {
            type = DataType::Untyped;
        }
    };

    static_assert(sizeof(UntypedDataBuffer) == sizeof(DataType), "Do not add any members to the derived class.");

    /**
     * @brief A data buffer for 32-bit floating point audio
     */
    struct Float32AudioBuffer : public DataType
    {
        Float32AudioBuffer(Index channels, number samplerate) {
            type = DataType::Float32AudioBuffer;
            audioBufferInfo.channels = channels;
            audioBufferInfo.samplerate = samplerate;
        }
    };

    static_assert(sizeof(Float32AudioBuffer) == sizeof(DataType), "Do not add any members to the derived class.");

    /**
     * @brief A data buffer for 64-bit floating point audio
     */
    struct Float64AudioBuffer : public DataType
    {
        Float64AudioBuffer(Index channels, number samplerate) {
            type = DataType::Float64AudioBuffer;
            audioBufferInfo.channels = channels;
            audioBufferInfo.samplerate = samplerate;
        }
    };

    static_assert(sizeof(Float64AudioBuffer) == sizeof(DataType), "Do not add any members to the derived class.");

	/**
	 * @private
	 */
	template<class T, typename U> void updateMultiRef(T patcher, U& ref, DataRefIndex current)
	{
		ref->setCurrent(current);
		patcher->getEngine()->sendDataRefUpdated(ref->getIndex());
	}
	template<typename T> void updateMultiRef(T, Float32BufferRef&, DataRefIndex) {}
	template<typename T> void updateMultiRef(T, Float64BufferRef&, DataRefIndex) {}
    template<typename T> void updateMultiRef(T, SampleBufferRef&, DataRefIndex) {}

	template<class T, typename U> void updateDataRef(T patcher, U& ref)
	{
		patcher->getEngine()->sendDataRefUpdated(ref->getIndex());
	}

	ATTRIBUTE_UNUSED
	static DataRef& serializeDataRef(DataRef& ref) {
		ref.resetRequestedSizeInByte();
		return ref;
	}

	template<typename T, typename U> T& reInitDataView(T& bufferRef, U&) {
		bufferRef->reInit();
		return bufferRef;
	}

    struct SerializedBuffer {
        SerializedBuffer() {}

        SerializedBuffer(const DataRef& ref) {
            type = ref.getType();
            if (type.type == DataType::SampleAudioBuffer) {
                if (sizeof(SampleValue) == sizeof(float)) {
                    type.type = DataType::Float32AudioBuffer;
                }
                else {
                    type.type = DataType::Float64AudioBuffer;
                }
            }
            sizeInBytes = ref.getSizeInBytes();
            data = (uint8_t *)Platform::malloc(sizeInBytes);
            Platform::memcpy(data, ref.getData(), sizeInBytes);
        }

        ~SerializedBuffer() {
            if (data) {
                free(data);
                data = nullptr;
            }
        }

        SerializedBuffer(SerializedBuffer&& other)
        {
            type = other.type;
            sizeInBytes = other.sizeInBytes;
            data = other.data;

            other.data = nullptr;
            other.sizeInBytes = 0;
            other.type = DataType();
        }

        // disable copy constructors for development purposes
        SerializedBuffer (const SerializedBuffer& other) {
            type = other.type;
            sizeInBytes = other.sizeInBytes;
            data = (uint8_t*)Platform::malloc(sizeInBytes);
            Platform::memcpy(data, other.data, sizeInBytes);
        }

        SerializedBuffer& operator= (const SerializedBuffer&) = delete;

        DataType    type;
        size_t      sizeInBytes = 0;
        uint8_t*    data = nullptr;
    };

    ATTRIBUTE_UNUSED
    static SerializedBuffer serializeBuffer(const DataRef& ref) {
        return SerializedBuffer(ref);
    }

    template<class T> void deserializeBuffer(T patcher, DataRef& dst, const SerializedBuffer& src) {
        char* data = (char *)Platform::malloc(src.sizeInBytes);
        Platform::memcpy(data, src.data, src.sizeInBytes);
        dst.setData(data, src.sizeInBytes, true);
        dst.setType(src.type);
        patcher->getEngine()->sendDataRefUpdated(dst->getIndex());
    }

} // namespace RNBO

#endif // #ifndef _RNBO_DATAREF_H_
