#ifndef _RNBO_DATA_LOADER_H
#define _RNBO_DATA_LOADER_H

//RNBO.h seems to have to come first in visual studio or std::numeric_limits<_>::max() breaks..
#include <RNBO.h>
#include <memory>

#include <ext.h>
#include <ext_obex.h>
#include <ext_path.h>
#include <ext_systhread.h>
#include "commonsyms.h"

extern "C" {
	typedef struct _rnbo_data_loader t_rnbo_data_loader;

	void rnbo_data_loader_register();

	//path or url, it handles it
	void rnbo_data_loader_load(t_rnbo_data_loader *x, const char *pathorurl);
	t_symbol *rnbo_data_loader_get_last_requested(t_rnbo_data_loader *x);

	bool rnbo_path_is_url(const char *pathOrURL);
}

namespace RNBO {
	void DataLoaderHandoffData(
			ExternalDataIndex dataRefIndex,
			const ExternalDataRef* ref,
			t_rnbo_data_loader *loader,
			UpdateRefCallback updateDataRef,
			ReleaseRefCallback releaseDataRef
	);
};

#endif
