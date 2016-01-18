/*
 * Copyright 2011-2013 Blender Foundation
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

#include "util_ies.h"

CCL_NAMESPACE_BEGIN

IESLight::IESLight(const string& ies_)
{
	ies = ies_;

	if(!parse() || !process()) {
		for(int i = 0; i < intensity.size(); i++)
			delete[] intensity[i];
		intensity.clear();
		v_angles_num = h_angles_num = 0;
	}
}

bool IESLight::parse()
{
	int len = ies.length();
	char *fdata = new char[len+1];
	memcpy(fdata, ies.c_str(), len+1);

	for(int i = 0; i < len; i++)
		if(fdata[i] == ',')
			fdata[i] = ' ';

	char *data = strstr(fdata, "\nTILT=");
	if(!data) {
		delete[] fdata;
		return false;
	}

	if(strncmp(data, "\nTILT=INCLUDE", 13) == 0)
		for(int i = 0; i < 5 && data; i++)
			data = strstr(data+1, "\n");
	else
		data = strstr(data+1, "\n");
	if(!data) {
		delete[] fdata;
		return false;
	}

	data++;
	strtol(data, &data, 10); /* Number of lamps */
	strtod(data, &data); /* Lumens per lamp */
	double factor = strtod(data, &data); /* Candela multiplier */
	v_angles_num = strtol(data, &data, 10); /* Number of vertical angles */
	h_angles_num = strtol(data, &data, 10); /* Number of horizontal angles */
	strtol(data, &data, 10); /* Photometric type (is assumed to be 1 => Type C) */
	strtol(data, &data, 10); /* Unit of the geometry data */
	strtod(data, &data); /* Width */
	strtod(data, &data); /* Length */
	strtod(data, &data); /* Height */
	factor *= strtod(data, &data); /* Ballast factor */
	factor *= strtod(data, &data); /* Ballast-Lamp Photometric factor */
	strtod(data, &data); /* Input Watts */

	/* Intensity values in IES files are specified in candela (lumen/sr), a photometric quantity.
	 * Cycles expects radiometric quantities, though, which requires a conversion.
	 * However, the Luminous efficacy (ratio of lumens per Watt) depends on the spectral distribution
	 * of the light source since lumens take human perception into account.
	 * Since this spectral distribution is not known from the IES file, a typical one must be assumed.
	 * The D65 standard illuminant has a Luminous efficacy of 177.83, which is used here to convert to Watt/sr.
	 * A more advanced approach would be to add a Blackbody Temperature input to the node and numerically
	 * integrate the Luminous efficacy from the resulting spectral distribution.
	 * Also, the Watt/sr value must be multiplied by 4*pi to get the Watt value that Cycles expects
	 * for lamp strength. Therefore, the conversion here uses 4*pi/177.83 as a Candela to Watt factor.
	 */
	factor *= 0.0706650768394;

	for(int i = 0; i < v_angles_num; i++)
		v_angles.push_back(strtod(data, &data));
	for(int i = 0; i < h_angles_num; i++)
		h_angles.push_back(strtod(data, &data));
	for(int i = 0; i < h_angles_num; i++) {
		intensity.push_back(new float[v_angles_num]);
	for(int j = 0; j < v_angles_num; j++)
			intensity[i][j] = factor * strtod(data, &data);
	}
	for(; isspace(*data); data++);
	if(*data == 0 || strncmp(data, "END", 3) == 0) {
		delete[] fdata;
		return true;
	}
	delete[] fdata;
	return false;
}

bool IESLight::process()
{
	if(h_angles_num == 0 || v_angles_num == 0 || h_angles[0] != 0.0f || v_angles[0] != 0.0f)
		return false;

	if(h_angles_num == 1) {
		/* 1D IES */
		h_angles_num = 2;
		h_angles.push_back(360.f);
		intensity.push_back(new float[v_angles_num]);
		memcpy(intensity[1], intensity[0], v_angles_num*sizeof(float));
	}
	else {
		if(!(h_angles[h_angles_num-1] == 90.0f || h_angles[h_angles_num-1] == 180.0f || h_angles[h_angles_num-1] == 360.0f))
			return false;
		/* 2D IES - potential symmetries must be considered here */
		if(h_angles[h_angles_num-1] == 90.0f) {
			/* All 4 quadrants are symmetric */
			for(int i = h_angles_num-2; i >= 0; i--) {
				intensity.push_back(new float[v_angles_num]);
				memcpy(intensity[intensity.size()-1], intensity[i], v_angles_num*sizeof(float));
				h_angles.push_back(180.0f - h_angles[i]);
			}
			h_angles_num = 2*h_angles_num-1;
		}
		if(h_angles[h_angles_num-1] == 180.0f) {
			/* Quadrants 1 and 2 are symmetric with 3 and 4 */
			for(int i = h_angles_num-2; i >= 0; i--) {
				intensity.push_back(new float[v_angles_num]);
				memcpy(intensity[intensity.size()-1], intensity[i], v_angles_num*sizeof(float));
				h_angles.push_back(360.0f - h_angles[i]);
			}
			h_angles_num = 2*h_angles_num-1;
		}
	}

	return true;
}

void IESLight::pack(vector<float> &data)
{
	if((v_angles_num < 2) || (h_angles_num < 2)) {
		/* IES file was not loaded correctly, store fallback instead */
		data.push_back(__int_as_float(2));
		data.push_back(__int_as_float(2));
		data.push_back(0.0f);
		data.push_back(M_2PI_F);
		data.push_back(0.0f);
		data.push_back(M_PI_2_F);
		data.push_back(100.0f);
		data.push_back(100.0f);
		data.push_back(100.0f);
		data.push_back(100.0f);
		return;
	}
	data.push_back(__int_as_float(h_angles_num));
	data.push_back(__int_as_float(v_angles_num));
	for(int h = 0; h < h_angles_num; h++)
		data.push_back(h_angles[h] / 180.f * M_PI_F);
	for(int v = 0; v < v_angles_num; v++)
		data.push_back(v_angles[v] / 180.f * M_PI_F);
	for(int h = 0; h < h_angles_num; h++)
		for(int v = 0; v < v_angles_num; v++)
			data.push_back(intensity[h][v]);
}

IESLight::~IESLight()
{
	for(int i = 0; i < intensity.size(); i++)
		delete[] intensity[i];
}

CCL_NAMESPACE_END
