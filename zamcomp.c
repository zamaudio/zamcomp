#include <stdlib.h>
#include <string.h>

#define         _ISOC9X_SOURCE  1
#define         _ISOC99_SOURCE  1
#define         __USE_ISOC99    1
#define         __USE_ISOC9X    1

#include <math.h>

#include "ladspa.h"

#define ZAMCOMP_ATTACK  0
#define ZAMCOMP_RELEASE 1
#define ZAMCOMP_KNEE    2
#define ZAMCOMP_RATIO   3
#define ZAMCOMP_THRESH  4
#define ZAMCOMP_MAKEUP  5
#define ZAMCOMP_GAINR   6
#define ZAMCOMP_INPUT   7
#define ZAMCOMP_OUTPUT  8

static LADSPA_Descriptor *zamcompDescriptor = NULL;

typedef struct {
	LADSPA_Data *attack;
	LADSPA_Data *release;
	LADSPA_Data *knee;
	LADSPA_Data *ratio;
	LADSPA_Data *thresh;
	LADSPA_Data *makeup;
	LADSPA_Data *gainr;
	LADSPA_Data *input;
	LADSPA_Data *output;
	LADSPA_Data srate;
        float old_yl;
	float old_y1;
} ZamComp;

const LADSPA_Descriptor *ladspa_descriptor(unsigned long index) {

	switch (index) {
	case 0:
		return zamcompDescriptor;
	default:
		return NULL;
	}
}

static void cleanupZamComp(LADSPA_Handle instance) {
	free(instance);
}

static void connectPortZamComp(
 LADSPA_Handle instance,
 unsigned long port,
 LADSPA_Data *data) {
	ZamComp *plugin;

	plugin = (ZamComp *)instance;
	switch (port) {
	case ZAMCOMP_ATTACK:
		plugin->attack = data;
		break;
	case ZAMCOMP_RELEASE:
		plugin->release = data;
		break;
	case ZAMCOMP_KNEE:
		plugin->knee = data;
		break;
	case ZAMCOMP_RATIO:
		plugin->ratio = data;
		break;
	case ZAMCOMP_THRESH:
		plugin->thresh = data;
		break;
	case ZAMCOMP_MAKEUP:
		plugin->makeup = data;
		break;
	case ZAMCOMP_GAINR:
		plugin->gainr = data;
		break;
	case ZAMCOMP_INPUT:
		plugin->input = data;
		break;
	case ZAMCOMP_OUTPUT:
		plugin->output = data;
		break;
	}
}

static LADSPA_Handle instantiateZamComp(
 const LADSPA_Descriptor *descriptor,
 unsigned long s_rate) {
	ZamComp *plugin_data = (ZamComp *)malloc(sizeof(ZamComp));
	plugin_data->srate = (LADSPA_Data) s_rate;
	plugin_data->old_yl = 0.f;
	plugin_data->old_y1 = 0.f;

	return (LADSPA_Handle)plugin_data;
}

static inline float
sanitize_denormal(float v) {
        if(!isnormal(v))
                return 0.f;
        return v;
}

static inline float
from_dB(float gdb) {
        return (exp(gdb/20.f*log(10.f)));
}

static inline float
to_dB(float g) {
        return (20.f*log10(g));
}

static void runZamComp(LADSPA_Handle instance, unsigned long sample_count) {
	ZamComp *plugin_data = (ZamComp *)instance;

	const LADSPA_Data * const input = plugin_data->input;
	LADSPA_Data * const output = plugin_data->output;

	const LADSPA_Data attack = *(plugin_data->attack);
	const LADSPA_Data release = *(plugin_data->release);
	const LADSPA_Data knee = *(plugin_data->knee);
	const LADSPA_Data ratio = *(plugin_data->ratio);
	const LADSPA_Data thresdb = *(plugin_data->thresh);
	const LADSPA_Data makeup = from_dB(*(plugin_data->makeup));
	LADSPA_Data* gainr = (plugin_data->gainr);
	const LADSPA_Data srate = plugin_data->srate;

        float width=(knee-0.99f)*6.f;
        float cdb=0.f;
        float attack_coeff = exp(-1000.f/(attack * srate));
        float release_coeff = exp(-1000.f/(release * srate));

        float gain = 1.f;
        float xg, xl, yg, yl, y1;
	int i;

	for (i = 0; i < sample_count; i++) {
                yg=0.f;
                xg = (input[i]==0.f) ? -160.f : to_dB(fabs(input[i]));
                xg = sanitize_denormal(xg);


                if (2.f*(xg-thresdb)<-width) {
                        yg = xg;
                } else if (2.f*fabs(xg-thresdb)<=width) {
                        yg = xg + (1.f/ratio-1.f)*(xg-thresdb+width/2.f)*(xg-thresdb+width/2.f)/(2.f*width);
                } else if (2.f*(xg-thresdb)>width) {
                        yg = thresdb + (xg-thresdb)/ratio;
                }
                yg = sanitize_denormal(yg);

                xl = xg - yg;
                plugin_data->old_y1 = sanitize_denormal(plugin_data->old_y1);
                plugin_data->old_yl = sanitize_denormal(plugin_data->old_yl);

                y1 = fmaxf(xl, release_coeff * plugin_data->old_y1+(1.f-release_coeff)*xl);
                yl = attack_coeff * plugin_data->old_yl+(1.f-attack_coeff)*y1;
                y1 = sanitize_denormal(y1);
                yl = sanitize_denormal(yl);

                cdb = -yl;
                gain = from_dB(cdb);

                *gainr = yl;

                output[i] = input[i];
                output[i] *= gain * makeup;

                plugin_data->old_yl = yl;
                plugin_data->old_y1 = y1;
	}
}

void _init() {
	char **port_names;
	LADSPA_PortDescriptor *port_descriptors;
	LADSPA_PortRangeHint *port_range_hints;

	zamcompDescriptor =
	 (LADSPA_Descriptor *)malloc(sizeof(LADSPA_Descriptor));

	if (zamcompDescriptor) {
		zamcompDescriptor->UniqueID = 666;
		zamcompDescriptor->Label = "ZamComp";
		zamcompDescriptor->Properties =
		 LADSPA_PROPERTY_HARD_RT_CAPABLE;
		zamcompDescriptor->Name =
		 "ZamComp";
		zamcompDescriptor->Maker =
		 "Damien Zammit <damien@zamaudio.com>";
		zamcompDescriptor->Copyright =
		 "GPL";
		zamcompDescriptor->PortCount = 9;

		port_descriptors = (LADSPA_PortDescriptor *)calloc(9,
		 sizeof(LADSPA_PortDescriptor));
		zamcompDescriptor->PortDescriptors =
		 (const LADSPA_PortDescriptor *)port_descriptors;

		port_range_hints = (LADSPA_PortRangeHint *)calloc(9,
		 sizeof(LADSPA_PortRangeHint));
		zamcompDescriptor->PortRangeHints =
		 (const LADSPA_PortRangeHint *)port_range_hints;

		port_names = (char **)calloc(9, sizeof(char*));
		zamcompDescriptor->PortNames =
		 (const char **)port_names;

		port_descriptors[ZAMCOMP_ATTACK] =
		 LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
		port_names[ZAMCOMP_ATTACK] =
		 "Attack (ms)";
		port_range_hints[ZAMCOMP_ATTACK].HintDescriptor =
		 LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_LOW;
		port_range_hints[ZAMCOMP_ATTACK].LowerBound = +1;
		port_range_hints[ZAMCOMP_ATTACK].UpperBound = +200;

		port_descriptors[ZAMCOMP_RELEASE] =
		 LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
		port_names[ZAMCOMP_RELEASE] =
		 "Release (ms)";
		port_range_hints[ZAMCOMP_RELEASE].HintDescriptor =
		 LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_LOW;
		port_range_hints[ZAMCOMP_RELEASE].LowerBound = +1;
		port_range_hints[ZAMCOMP_RELEASE].UpperBound = +500;

		port_descriptors[ZAMCOMP_KNEE] =
		 LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
		port_names[ZAMCOMP_KNEE] =
		 "Knee (dB)";
		port_range_hints[ZAMCOMP_KNEE].HintDescriptor =
		 LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_0;
		port_range_hints[ZAMCOMP_KNEE].LowerBound = 0;
		port_range_hints[ZAMCOMP_KNEE].UpperBound = +9;

		port_descriptors[ZAMCOMP_RATIO] =
		 LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
		port_names[ZAMCOMP_RATIO] =
		 "Ratio";
		port_range_hints[ZAMCOMP_RATIO].HintDescriptor =
		 LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_LOW;
		port_range_hints[ZAMCOMP_RATIO].LowerBound = +1;
		port_range_hints[ZAMCOMP_RATIO].UpperBound = +20;

		port_descriptors[ZAMCOMP_THRESH] =
		 LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
		port_names[ZAMCOMP_THRESH] =
		 "Threshold (dB)";
		port_range_hints[ZAMCOMP_THRESH].HintDescriptor =
		 LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_0;
		port_range_hints[ZAMCOMP_THRESH].LowerBound = -80;
		port_range_hints[ZAMCOMP_THRESH].UpperBound = 0;

		port_descriptors[ZAMCOMP_MAKEUP] =
		 LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
		port_names[ZAMCOMP_MAKEUP] =
		 "Makeup Gain (dB)";
		port_range_hints[ZAMCOMP_MAKEUP].HintDescriptor =
		 LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_0;
		port_range_hints[ZAMCOMP_MAKEUP].LowerBound = 0;
		port_range_hints[ZAMCOMP_MAKEUP].UpperBound = +30;

		port_descriptors[ZAMCOMP_GAINR] =
		 LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL;
		port_names[ZAMCOMP_GAINR] =
		 "Gain Reduction (dB)";
		port_range_hints[ZAMCOMP_GAINR].HintDescriptor =
		 LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_0;
		port_range_hints[ZAMCOMP_GAINR].LowerBound = 0;
		port_range_hints[ZAMCOMP_GAINR].UpperBound = +20;

		/* Parameters for Input */
		port_descriptors[ZAMCOMP_INPUT] =
		 LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
		port_names[ZAMCOMP_INPUT] =
		 "Input";
		port_range_hints[ZAMCOMP_INPUT].HintDescriptor = 0;

		/* Parameters for Output */
		port_descriptors[ZAMCOMP_OUTPUT] =
		 LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
		port_names[ZAMCOMP_OUTPUT] =
		 "Output";
		port_range_hints[ZAMCOMP_OUTPUT].HintDescriptor = 0;

		zamcompDescriptor->activate = NULL;
		zamcompDescriptor->cleanup = cleanupZamComp;
		zamcompDescriptor->connect_port = connectPortZamComp;
		zamcompDescriptor->deactivate = NULL;
		zamcompDescriptor->instantiate = instantiateZamComp;
		zamcompDescriptor->run = runZamComp;
	}
}

void _fini() {
	if (zamcompDescriptor) {
		free((LADSPA_PortDescriptor *)zamcompDescriptor->PortDescriptors);
		free((char **)zamcompDescriptor->PortNames);
		free((LADSPA_PortRangeHint *)zamcompDescriptor->PortRangeHints);
		free(zamcompDescriptor);
	}

}
