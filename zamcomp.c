#include <math.h>
#include <stdlib.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#define ZAMCOMP_URI "http://zamaudio.com/lv2/zamcomp"


typedef enum {
	ZAMCOMP_INPUT = 0,
	ZAMCOMP_OUTPUT = 1,

	ZAMCOMP_ATTACK = 2,
	ZAMCOMP_RELEASE = 3,
	ZAMCOMP_KNEE = 4,
	ZAMCOMP_RATIO = 5,
	ZAMCOMP_THRESHOLD = 6,
	ZAMCOMP_MAKEUP = 7,
  
	ZAMCOMP_GAINR = 8
} PortIndex;


typedef struct {
	float* input;
	float* output;
  
	float* attack;
	float* release;
	float* knee;
	float* ratio;
	float* threshold;
	float* makeup;
	float* gainr;
 
	float srate;
	float old_yl; 
	float old_y1;
  
} ZamCOMP;

static LV2_Handle
instantiate(const LV2_Descriptor* descriptor,
            double rate,
            const char* bundle_path,
            const LV2_Feature* const* features)
{
	ZamCOMP* zamcomp = (ZamCOMP*)malloc(sizeof(ZamCOMP));
	zamcomp->srate = rate;
  
	zamcomp->old_yl=zamcomp->old_y1=0.f;
  
	return (LV2_Handle)zamcomp;
}

static void
connect_port(LV2_Handle instance,
             uint32_t port,
             void* data)
{
	ZamCOMP* zamcomp = (ZamCOMP*)instance;
  
	switch ((PortIndex)port) {
	case ZAMCOMP_INPUT:
		zamcomp->input = (float*)data;
  	break;
	case ZAMCOMP_OUTPUT:
		zamcomp->output = (float*)data;
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
	case ZAMCOMP_GAINR:
	zamcomp->gainr = (float*)data;
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
  
	const float* const input = zamcomp->input;
	float* const output = zamcomp->output;
  
	float attack = *(zamcomp->attack);
	float release = *(zamcomp->release);
	float knee = *(zamcomp->knee);
	float ratio = *(zamcomp->ratio);
	float threshold = from_dB(*(zamcomp->threshold));
	float makeup = from_dB(*(zamcomp->makeup));
	float* const gainr =  zamcomp->gainr;
 
	float width=(knee-0.99f)*6.f;
	float cdb=0.f;
	float attack_coeff = exp(-1000.f/(attack * zamcomp->srate));
	float release_coeff = exp(-1000.f/(release * zamcomp->srate));
	float thresdb= to_dB(threshold);
 
	float gain = 1.f;
	float xg, xl, yg, yl, y1;
 
	for (uint32_t i = 0; i < n_samples; ++i) {
		yg=0.f;
		xg = (input[i]==0.f) ? -160.f : to_dB(fabs(input[i]));
		sanitize_denormal(xg);
    
    
		if (2.f*(xg-thresdb)<-width) {
			yg = xg;
		} else if (2.f*fabs(xg-thresdb)<=width) {
			yg = xg + (1.f/ratio-1.f)*(xg-thresdb+width/2.f)*(xg-thresdb+width/2.f)/(2.f*width);
		} else if (2.f*(xg-thresdb)>width) {
			yg = thresdb + (xg-thresdb)/ratio;
		}
    
		sanitize_denormal(yg);
    
		xl = xg - yg;
		sanitize_denormal(zamcomp->old_y1);
		sanitize_denormal(zamcomp->old_yl);
    
		y1 = fmaxf(xl, release_coeff * zamcomp->old_y1+(1.f-release_coeff)*xl);
		yl = attack_coeff * zamcomp->old_yl+(1.f-attack_coeff)*y1;
		sanitize_denormal(y1);
		sanitize_denormal(yl);
    
		cdb = -yl;
		gain = from_dB(cdb);

		*gainr = yl;
    
		output[i] = input[i];
		output[i] *= gain * makeup;
    
		zamcomp->old_yl = yl; 
		zamcomp->old_y1 = y1;
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
	ZAMCOMP_URI,
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
