/**************************************************************************/
/*  ai_model_catalog.h                                                    */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

class AIModelCatalog {
public:
	static Array list_provider_presets();
	static Dictionary get_provider_preset(const String &p_provider_id);
};
