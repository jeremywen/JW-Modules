#include "rnbo_data_loader.h"
#include <atomic>
#include <3rdparty/readerwriterqueue/readerwriterqueue.h>
#include <ext_sysfile.h>

namespace {
	t_class *s_rnbo_data_loader_class = nullptr;

	struct loader_data {
		size_t channels = 0;
		size_t frames = 0;
		RNBO::number samplerate = 0.0;
		size_t bytes = 0;
		char * data = nullptr;

		loader_data(size_t cs, size_t fms, RNBO::number sr) : channels(cs), frames(fms), samplerate(sr) {
			data = reinterpret_cast<char *>(new float[channels * frames]);
		}

		loader_data(size_t cs) : bytes(cs) {
			data = new char[cs];
		}

		~loader_data() {
			if (data) {
				delete [] data;
			}
		}
	};

	t_symbol *ps_close;
	t_symbol *ps_error;
	t_symbol *ps_open;

	const t_fourcc s_types[] = {
		FOUR_CHAR_CODE('AIFF'),
		FOUR_CHAR_CODE('AIFC'),
		FOUR_CHAR_CODE('ULAW'),	//next/sun typed by peak and others
		FOUR_CHAR_CODE('WAVE'),
		FOUR_CHAR_CODE('FLAC'),
		FOUR_CHAR_CODE('Sd2f'),
		FOUR_CHAR_CODE('QQQQ'),	// or whatever IRCAM snd files are saved as...
		FOUR_CHAR_CODE('BINA'),	// or whatever IRCAM snd files are saved as...
		FOUR_CHAR_CODE('NxTS'),	//next/sun typed by soundhack
		FOUR_CHAR_CODE('Mp3 '),
		FOUR_CHAR_CODE('DATA'), // our own 'raw' files
		FOUR_CHAR_CODE('M4a '),
		FOUR_CHAR_CODE('CAF '),
		FOUR_CHAR_CODE('wv64'),
		FOUR_CHAR_CODE('Midi'), //MIDI
		FOUR_CHAR_CODE('BMP ') //bitmap
	};
}

using RNBO::DataType;

extern "C" {

	using moodycamel::ReaderWriterQueue;

	// Simplified "loader" object that can load audio data from a file or URL
	// without using a Max buffer.
	struct _rnbo_data_loader {
		t_object 				obj;
		RNBO::DataType::Type	_type;
		t_symbol				*_last_requested;
		t_object				*_remote_resource;

		std::atomic<loader_data *> _newinfo;
		loader_data * _activeinfo;

		ReaderWriterQueue<loader_data *, 32> * _cleanup;
		void * _cleanupqelem;
	};

	t_rnbo_data_loader *rnbo_data_loader_new(RNBO::DataType::Type type);
	void rnbo_data_loader_free(t_rnbo_data_loader *x);
	void rnbo_data_loader_drain(t_rnbo_data_loader *x);
	t_max_err rnbo_data_loader_notify(t_rnbo_data_loader *loader, t_symbol *s, t_symbol *msg, void *sender, void *data);

	static t_max_err rnbo_data_loader_storefile(
		t_rnbo_data_loader *loader,
		const char *key,
		short vol,
		const char *filename,
		const t_fourcc filetype
	) {
		t_max_err err = MAX_ERR_NONE;
		if (loader->_type == DataType::TypedArray) {
			t_filehandle fh;
			err = path_opensysfile(filename, vol, &fh, READ_PERM);
			if (err == MAX_ERR_NONE) {
				t_ptr_size bytes = 0;
				err = sysfile_geteof(fh, &bytes);
				if (err == MAX_ERR_NONE && bytes > 0) {
					loader_data * info = new loader_data(bytes);
					err = sysfile_read(fh, &bytes, info->data);
					if (err == MAX_ERR_NONE) {
						info->bytes = bytes;
						info = loader->_newinfo.exchange(info);
						if (info != nullptr) {
							delete info;
						}
					} else {
						delete info;
					}
				}
				sysfile_close(fh);
			}
			if (err != MAX_ERR_NONE) {
				object_error(nullptr, "%s: failed to read", filename);
			}
			return err;
		}

		t_object *reader = (t_object *) object_new(CLASS_NOBOX, gensym("jsoundfile"));

		err = (t_max_err) object_method(reader, ps_open, vol, filename, 0, 0);
		if (err != MAX_ERR_NONE) {
			return err;
		}

		loader_data * info = new loader_data(
				(t_atom_long) object_method(reader, gensym("getchannelcount")),
				(t_atom_long) object_method(reader, gensym("getlength")),
				(t_atom_long) object_method(reader, gensym("getsr")));

		// Get the info
		long sampleLength = info->channels * info->frames;
		float *fdata = reinterpret_cast<float *>(info->data);

		// Read in the data
		object_method(reader, gensym("readfloats"), fdata, 0, info->frames, info->channels);

		// Close it up
		object_method(reader, ps_close);
		object_free(reader);

		// If we're 64 bit, then copy the data over
		if (loader->_type == DataType::Float64AudioBuffer || (loader->_type == DataType::SampleAudioBuffer && sizeof(RNBO::SampleValue) == sizeof(double))) {
			double *ddata = new double[info->channels * info->frames];
			for (unsigned long i = 0; i < sampleLength; i++) {
				ddata[i] = fdata[i];
			}
			delete [] fdata;
			info->data = reinterpret_cast<char *>(ddata);
			info->bytes = sampleLength * sizeof(double);
		} else if (loader->_type == DataType::Float32AudioBuffer || (loader->_type == DataType::SampleAudioBuffer && sizeof(RNBO::SampleValue) == sizeof(float))) {
			info->bytes = sampleLength * sizeof(float);
		} else {
			object_error(nullptr, "%s: don't know how to load for datatype", filename);
			delete info;
			return MAX_ERR_GENERIC;
		}

		info = loader->_newinfo.exchange(info);
		if (info != nullptr) {
			delete info;
		}

		return MAX_ERR_NONE;
	}

	void rnbo_data_loader_register()
	{
		ps_close = gensym("close");
		ps_error = gensym("error");
		ps_open = gensym("open");

		if (s_rnbo_data_loader_class)
			return;


		auto c = class_new("rnbo_data_loader", (method)rnbo_data_loader_new, (method)rnbo_data_loader_free, (long)sizeof(t_rnbo_data_loader), 0L, A_CANT, 0);

		class_addmethod(c, (method)rnbo_data_loader_notify, "notify", A_CANT, 0);

		class_register(CLASS_NOBOX, c);
		s_rnbo_data_loader_class = c;
	}

	t_rnbo_data_loader *rnbo_data_loader_new(RNBO::DataType::Type type)
	{
		t_rnbo_data_loader *loader = (t_rnbo_data_loader *)object_alloc(s_rnbo_data_loader_class);
		if (loader) {
			loader->_type = type;
			loader->_last_requested = nullptr;
			loader->_activeinfo = nullptr;
			loader->_newinfo = nullptr;
			loader->_cleanup = new ReaderWriterQueue<loader_data *, 32>(32);
			loader->_cleanupqelem = qelem_new(loader, (method) rnbo_data_loader_drain);
		}

		return loader;
	}

	void rnbo_data_loader_drain(t_rnbo_data_loader *loader) {
		if (loader->_cleanup) {
			loader_data * info;
			while (loader->_cleanup->try_dequeue(info)) {
				delete info;
			}
		}
	}

	void rnbo_data_loader_free(t_rnbo_data_loader *loader)
	{
		auto info = loader->_newinfo.exchange(nullptr);
		if (info != nullptr) {
			delete info;
		}

		if (loader->_activeinfo != nullptr) {
			delete loader->_activeinfo;
		}


		if (loader->_remote_resource) {
			object_free(loader->_remote_resource);
			loader->_remote_resource = nullptr;
		}

		qelem_free(loader->_cleanupqelem);
		rnbo_data_loader_drain(loader);
		if (loader->_cleanup) {
			delete loader->_cleanup;
		}
	}

	static t_max_err rnbo_data_locatefile(const char *in_filename, short *out_path, char *out_filename, t_fourcc& filetype, RNBO::DataType::Type rnbotype) {
		t_max_err err;
		char pathBuffer[MAX_PATH_CHARS];
		const t_fourcc * filter = s_types;
		size_t filterlen = sizeof(s_types) / sizeof(s_types[0]);

		//don't filter based on fourcc for TypedArray
		if (rnbotype == DataType::Type::TypedArray) {
			filter = nullptr;
			filterlen = 0;
		}

		strncpy_zero(pathBuffer, in_filename, MAX_PATH_CHARS);
		err = locatefile_extended(pathBuffer, out_path, &filetype, filter, filterlen);

		if (err == MAX_ERR_NONE) {
			err = path_toabsolutesystempath(*out_path, pathBuffer, pathBuffer);
		}

		if (err == MAX_ERR_NONE) {
			err = path_frompathname(pathBuffer , out_path, out_filename);
		}

		if (err != MAX_ERR_NONE) {
			out_path = 0;
			out_filename[0] = '\0';
		}

		return err;
	}

	// Attempt to read the audio file from disk and load it into memory, so that
	// the contents of the audio file can be passed to RNBO
	void rnbo_data_loader_file(t_rnbo_data_loader *loader, const char *filename)
	{
		// If there were a mechanism to cancel an HTTP request, we could do it here
		loader->_last_requested = gensym(filename);

		// Locate the file
		char out_filename[MAX_PATH_CHARS];
		short out_path;
		t_fourcc out_filetype;
		t_max_err err = rnbo_data_locatefile(filename, &out_path, out_filename, out_filetype, loader->_type);

		if (err == MAX_ERR_NONE) {
			err = rnbo_data_loader_storefile(loader, filename, out_path, out_filename, out_filetype);
		} else {
			object_error(nullptr, "%s: can't open file", filename);
		}
	}

	// Attempt to download the audio file from a remote URL, then load it so that
	// the contents of the audio file can be passed to RNBO
	void rnbo_data_loader_url(t_rnbo_data_loader *loader, const char *url)
	{
		static t_symbol *sym_download = gensym("download");

		loader->_last_requested = gensym(url);

		if (!loader->_remote_resource) {
			loader->_remote_resource = (t_object *) object_new(CLASS_NOBOX, gensym("remote_resource"));
			object_attach_byptr_register(loader, loader->_remote_resource, CLASS_NOBOX);
		}

		object_method(loader->_remote_resource, sym_download, loader->_last_requested);
	}

	t_max_err rnbo_data_loader_notify(t_rnbo_data_loader *loader, t_symbol *s, t_symbol *msg, void *sender, void *data)
	{
		static t_symbol *sym_complete = gensym("complete");
		static t_symbol *sym_geturl = gensym("geturl");
		static t_symbol *sym_filename = gensym("filename");
		static t_symbol *sym_vol = gensym("vol");
		static t_symbol *sym_remote_resource = gensym("remote_resource");

		if (sender && object_classname_compare(sender, sym_remote_resource)) {
			t_symbol *url = (t_symbol *) object_method(sender, sym_geturl);

			bool showError = false;

			// If the url doesn't match _last_requested, it means that RNBO has changed
			// its mind about what resource should be loaded into the given data buffer,
			// and we can just move on
			if (url == loader->_last_requested) {
				if (msg == sym_complete) {
					t_symbol *filename = (t_symbol *) object_method(sender, sym_filename);
					const short vol = (t_atom_long) object_method(sender, sym_vol);

					if (filename != NULL && vol != -1) {
						t_fourcc filetype = 0; //don't care
						rnbo_data_loader_storefile(loader, url->s_name, vol, filename->s_name, filetype);
					} else {
						showError = true;
					}
				} else if (msg == ps_error) {
					showError = true;
				}
			}

			if (showError) object_error(nullptr, "Couldn't download file from %s", url ? url->s_name : "unknown url");
		}

		return MAX_ERR_NONE;
	}

	t_symbol *rnbo_data_loader_get_last_requested(t_rnbo_data_loader *loader)
	{
		return loader->_last_requested;
	}


	bool rnbo_path_is_url(const char *pathOrURL) {
		return strstr(pathOrURL, "://") != nullptr;
	}

	void rnbo_data_loader_load(t_rnbo_data_loader *x, const char *pathorurl) {
		if (rnbo_path_is_url(pathorurl)) {
			rnbo_data_loader_url(x, pathorurl);
		} else {
			rnbo_data_loader_file(x, pathorurl);
		}
	}
}

namespace RNBO {
	void DataLoaderHandoffData(
			ExternalDataIndex dataRefIndex,
			const ExternalDataRef* ref,
			t_rnbo_data_loader *loader,
			UpdateRefCallback updateDataRef,
			ReleaseRefCallback releaseDataRef
	) {

		auto info = loader->_newinfo.exchange(nullptr);
		if (info != nullptr && info != loader->_activeinfo) {
			if (loader->_type == DataType::Float64AudioBuffer || (loader->_type == DataType::SampleAudioBuffer && sizeof(RNBO::SampleValue) == sizeof(double))) {
				Float64AudioBuffer newType(info->channels, info->samplerate);
				updateDataRef(dataRefIndex, info->data, info->bytes, newType);
			} else if (loader->_type == DataType::TypedArray) {
				DataType newType;
				newType.type = loader->_type;
				updateDataRef(dataRefIndex, info->data, info->bytes, newType);
			} else if (loader->_type == DataType::Float32AudioBuffer || (loader->_type == DataType::SampleAudioBuffer && sizeof(RNBO::SampleValue) == sizeof(float))) {
				Float32AudioBuffer newType(info->channels, info->samplerate);
				updateDataRef(dataRefIndex, info->data, info->bytes, newType);
			} else {
				//don't know how to handle
				return;
			}

			if (loader->_activeinfo != nullptr) {
				loader->_cleanup->try_enqueue(loader->_activeinfo);
				qelem_set(loader->_cleanupqelem);
			}
			loader->_activeinfo = info;
		}
	}
};
