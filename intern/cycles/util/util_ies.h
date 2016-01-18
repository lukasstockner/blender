/*
 * Copyright 2011-2015 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __UTIL_IES_H__
#define __UTIL_IES_H__

#include "util_vector.h"
#include "util_string.h"

CCL_NAMESPACE_BEGIN

class IESLight {
public:
	IESLight(const string& ies);
	~IESLight();
	bool pack(vector<float> &data);

protected:
	string ies;

	int v_angles_num, h_angles_num;
	vector<float> v_angles, h_angles;
	vector<float*> intensity;

	bool parse();
	bool process();
};

CCL_NAMESPACE_END

#endif /* __UTIL_IES_H__ */
