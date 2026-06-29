/**************************************************************************/
/*  test_version.cpp                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "core/next_version.h"
#include "core/version.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_version)

TEST_CASE("[Version] NEXT display version combines NEXT and Godot versions") {
	CHECK(String(NEXT_VERSION_NUMBER) == "0.1.4.7.1");
	CHECK(String(NEXT_VERSION_FULL_CONFIG) == "0.1.4.7.1-preview");
	CHECK(String(NEXT_VERSION_STATUS) == "preview");

	CHECK(GODOT_VERSION_MAJOR == 4);
	CHECK(GODOT_VERSION_MINOR == 7);
	CHECK(GODOT_VERSION_PATCH == 1);
	CHECK(String(GODOT_VERSION_NUMBER) == "4.7.1");
}

TEST_CASE("[Version] NEXT update comparison accepts GitHub release tags") {
	CHECK(is_next_engine_version_newer("v0.1.4.7.2-preview", "0.1.4.7.1-preview"));
	CHECK(is_next_engine_version_newer("0.2.4.7.1-preview", "0.1.4.7.1-preview"));
	CHECK(is_next_engine_version_newer("0.1.4.8.0-preview", "0.1.4.7.1-preview"));
	CHECK(is_next_engine_version_newer("0.1.4.7.1", "0.1.4.7.1-preview"));

	CHECK_FALSE(is_next_engine_version_newer("v0.1.4.7.1-preview", "0.1.4.7.1-preview"));
	CHECK_FALSE(is_next_engine_version_newer("not-a-version", "0.1.4.7.1-preview"));
	CHECK_FALSE(is_next_engine_version_newer("0.1.4.7.1-preview", "not-a-version"));
}
