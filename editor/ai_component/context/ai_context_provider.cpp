/**************************************************************************/
/*  ai_context_provider.cpp                                                */
/**************************************************************************/

#include "ai_context_provider.h"

#include "core/object/class_db.h"

void AIContextProvider::_bind_methods() {
	ClassDB::bind_method(D_METHOD("collect_context"), &AIContextProvider::collect_context);
}

Array AIContextProvider::collect_context() {
	return Array();
}
