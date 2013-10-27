/* zamcompx2.c  ZamCompX2 stereo compressor
 * Copyright (C) 2013  Damien Zammit
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <math.h>
#include <stdlib.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#define ZAMCOMPX2_URI "http://zamaudio.com/lv2/zamcompx2"

#define STEREOLINK_UNCOUPLED 0
#define STEREOLINK_AVERAGE 1
#define STEREOLINK_MAX 2


typedef enum {
	ZAMCOMP_INPUT_L = 0,
	ZAMCOMP_INPUT_R = 1,
	ZAMCOMP_OUTPUT_L = 2,
	ZAMCOMP_OUTPUT_R = 3,

	ZAMCOMP_ATTACK = 4,
	ZAMCOMP_RELEASE = 5,
	ZAMCOMP_KNEE = 6,
	ZAMCOMP_RATIO = 7,
	ZAMCOMP_THRESHOLD = 8,
	ZAMCOMP_MAKEUP = 9,
	
	ZAMCOMP_GAINR_L = 10,
	ZAMCOMP_GAINR_R = 11,

	ZAMCOMP_STEREOLINK = 12
} PortIndex;


typedef struct {
	float* input_l;
	float* input_r;
	float* output_l;
	float* output_r;
  
	float* attack;
	float* release;
	float* knee;
	float* ratio;
	float* threshold;
	float* makeup;

	float* gainr_l;
	float* gainr_r;

	float* stereolink;

	float srate;
	float oldL_yl;
	float oldR_yl;
	float oldL_y1;
	float oldR_y1;
 
} ZamCOMP;

static LV2_Handle
instantiate(const LV2_Descriptor* descriptor,
            double rate,
            const char* bundle_path,
            const LV2_Feature* const* features)
{
	ZamCOMP* zamcomp = (ZamCOMP*)malloc(sizeof(ZamCOMP));
	zamcomp->srate = rate;
  
	zamcomp->oldL_yl=zamcomp->oldL_y1=0.f;
	zamcomp->oldR_yl=zamcomp->oldR_y1=0.f;
  
	return (LV2_Handle)zamcomp;
}

static void
connect_port(LV2_Handle instance,
             uint32_t port,
             void* data)
{
	ZamCOMP* zamcomp = (ZamCOMP*)instance;
  
	switch ((PortIndex)port) {
	case ZAMCOMP_INPUT_L:
		zamcomp->input_l = (float*)data;
  	break;
	case ZAMCOMP_INPUT_R:
		zamcomp->input_r = (float*)data;
  	break;
	case ZAMCOMP_OUTPUT_L:
		zamcomp->output_l = (float*)data;
  	break;
	case ZAMCOMP_OUTPUT_R:
		zamcomp->output_r = (float*)data;
  	break;
	case ZAMCOMP_ATTACK:
		zamcomp->attack = (float*)data;
	break;
	case ZAMCOMP_RELEASE:
		zamcomp->release = (float*)data;
	break;
	case ZAMCOMP_KNEE:
		zamcomp->knee = (float*)data;
	break;
	case ZAMCOMP_RATIO:
		zamcomp->ratio = (float*)data;
	break;
	case ZAMCOMP_THRESHOLD:
		zamcomp->threshold = (float*)data;
	break;
	case ZAMCOMP_MAKEUP:
		zamcomp->makeup = (float*)data;
	break;
	case ZAMCOMP_GAINR_L:
		zamcomp->gainr_l = (float*)data;
	break;
	case ZAMCOMP_GAINR_R:
		zamcomp->gainr_r = (float*)data;
	break;
	case ZAMCOMP_STEREOLINK:
		zamcomp->stereolink = (float*)data;
	break;
	}
}

// Works on little-endian machines only
static inline bool
is_nan(float& value ) {
	if (((*(uint32_t *) &value) & 0x7fffffff) > 0x7f800000) {
		return true;
	}
return false;
}

// Force already-denormal float value to zero
static inline void
sanitize_denormal(float& value) {
	if (is_nan(value)) {
		value = 0.f;
	}
}

static inline float
from_dB(float gdb) {
	return (exp(gdb/20.f*log(10.f)));
};

static inline float
to_dB(float g) {
	return (20.f*log10(g));
}

static void
activate(LV2_Handle instance)
{
}

static void
run(LV2_Handle instance, uint32_t n_samples)
{
	ZamCOMP* zamcomp = (ZamCOMP*)instance;
  
	const float* const input_l = zamcomp->input_l;
	const float* const input_r = zamcomp->input_r;
	float* const output_l = zamcomp->output_l;
	float* const output_r = zamcomp->output_r;
  
	float attack = *(zamcomp->attack);
	float release = *(zamcomp->release);
	float knee = *(zamcomp->knee);
	float ratio = *(zamcomp->ratio);
	float threshold = from_dB(*(zamcomp->threshold));
	float makeup = from_dB(*(zamcomp->makeup));
	float* const gainr_l =  zamcomp->gainr_l;
	float* const gainr_r =  zamcomp->gainr_r;
	int stereolink = (*(zamcomp->stereolink) > 1.f) ? STEREOLINK_MAX : (*(zamcomp->stereolink) > 0.f) ? STEREOLINK_AVERAGE : STEREOLINK_UNCOUPLED;
	float width=(knee-0.99f)*6.f;
	float cdb=0.f;
	float attack_coeff = exp(-1000.f/(attack * zamcomp->srate));
	float release_coeff = exp(-1000.f/(release * zamcomp->srate));
	float thresdb= to_dB(threshold);
 
	float Lgain = 1.f;
	float Rgain = 1.f;
	float Lxg, Lyg;
	float Rxg, Ryg;
	float Lxl, Lyl, Ly1;
	float Rxl, Ryl, Ry1;
 
	for (uint32_t i = 0; i < n_samples; ++i) {
		Lyg = Ryg = 0.f;
		Lxg = (input_l[i]==0.f) ? -160.f : to_dB(fabs(input_l[i]));
		Rxg = (input_r[i]==0.f) ? -160.f : to_dB(fabs(input_r[i]));
		sanitize_denormal(Lxg);
		sanitize_denormal(Rxg);
    
    
		if (2.f*(Lxg-thresdb)<-width) {
			Lyg = Lxg;
		} else if (2.f*fabs(Lxg-thresdb)<=width) {
			Lyg = Lxg + (1.f/ratio-1.f)*(Lxg-thresdb+width/2.f)*(Lxg-thresdb+width/2.f)/(2.f*width);
		} else if (2.f*(Lxg-thresdb)>width) {
			Lyg = thresdb + (Lxg-thresdb)/ratio;
		}
    
		sanitize_denormal(Lyg);
    
		if (2.f*(Rxg-thresdb)<-width) {
			Ryg = Rxg;
		} else if (2.f*fabs(Rxg-thresdb)<=width) {
			Ryg = Rxg + (1.f/ratio-1.f)*(Rxg-thresdb+width/2.f)*(Rxg-thresdb+width/2.f)/(2.f*width);
		} else if (2.f*(Rxg-thresdb)>width) {
			Ryg = thresdb + (Rxg-thresdb)/ratio;
		}
    
		sanitize_denormal(Ryg);

		if (stereolink == STEREOLINK_UNCOUPLED) {
			Lxl = Lxg - Lyg;
			Rxl = Rxg - Ryg;
		} else if (stereolink == STEREOLINK_MAX) {
			Lxl = Rxl = fmaxf(Lxg - Lyg, Rxg - Ryg);
		} else {
			Lxl = Rxl = (Lxg - Lyg + Rxg - Ryg) / 2.f;
		}

		sanitize_denormal(zamcomp->oldL_y1);
		sanitize_denormal(zamcomp->oldR_y1);
		sanitize_denormal(zamcomp->oldL_yl);
		sanitize_denormal(zamcomp->oldR_yl);


		Ly1 = fmaxf(Lxl, release_coeff * zamcomp->oldL_y1+(1.f-release_coeff)*Lxl);
		Lyl = attack_coeff * zamcomp->oldL_yl+(1.f-attack_coeff)*Ly1;
		sanitize_denormal(Ly1);
		sanitize_denormal(Lyl);
    
		cdb = -Lyl;
		Lgain = from_dB(cdb);

		*gainr_l = Lyl;


		Ry1 = fmaxf(Rxl, release_coeff * zamcomp->oldR_y1+(1.f-release_coeff)*Rxl);
		Ryl = attack_coeff * zamcomp->oldR_yl+(1.f-attack_coeff)*Ry1;
		sanitize_denormal(Ry1);
		sanitize_denormal(Ryl);
    
		cdb = -Ryl;
		Rgain = from_dB(cdb);

		*gainr_r = Ryl;

		output_l[i] = input_l[i];
		output_l[i] *= Lgain * makeup;
		output_r[i] = input_r[i];
		output_r[i] *= Rgain * makeup;
    
		zamcomp->oldL_yl = Lyl;
		zamcomp->oldR_yl = Ryl;
		zamcomp->oldL_y1 = Ly1;
		zamcomp->oldR_y1 = Ry1;
		}
}

static void
deactivate(LV2_Handle instance)
{
}

static void
cleanup(LV2_Handle instance)
{
	free(instance);
}

const void*
extension_data(const char* uri)
{
	return NULL;
}

static const LV2_Descriptor descriptor = {
	ZAMCOMPX2_URI,
	instantiate,
	connect_port,
	activate,
	run,
	deactivate,
	cleanup,
	extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
	switch (index) {
	case 0:
		return &descriptor;
	default:
		return NULL;
	}
}
