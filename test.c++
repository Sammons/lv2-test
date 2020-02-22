#include <lv2.h>
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "math.h"

#define TEST_URI "http://sammons.io/plugins/test"

typedef enum {
	TEST_INPUT  = 0,
	TEST_OUTPUT = 1
} PortIndex;

typedef struct {
	// Port buffers
	const float* input;
	float*       output;
} Converter;

float *sample_buffer = nullptr;
int sustain_counter = 0;
int inverter = 1;
bool sustain_init = false;
int samples = 0;

static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
            double                    rate,
            const char*               bundle_path,
            const LV2_Feature* const* features)
{
	Converter* data = (Converter*)calloc(1, sizeof(Converter));
  samples = round(rate * 2);
  sample_buffer = (float*)calloc(1, samples * sizeof(float));
  fprintf(stderr, "Successfully allocated\n");
	return (LV2_Handle)data;
}

static void
connect_port(LV2_Handle instance,
             uint32_t   port,
             void*      data)
{
	Converter* converter = (Converter*)instance;

	switch ((PortIndex)port) {
	case TEST_INPUT:
		converter->input = (const float*)data;
		break;
	case TEST_OUTPUT:
		converter->output = (float*)data;
		break;
	}
}

/** reset everything except connect_port */
static void
activate(LV2_Handle instance)
{
  fprintf(stderr, "Successfully activated\n");
}

static void
run(LV2_Handle instance, uint32_t n_samples)
{
	const Converter* converter = (const Converter*)instance;

	const float* const input  = converter->input;
	float* const       output = converter->output;
  if (sustain_init) {
    for (uint32_t pos = 0; pos < n_samples; pos++) {
      output[pos] = sample_buffer[sustain_counter] + input[pos];
      sustain_counter = sustain_counter + inverter;
      if (sustain_counter >= samples || sustain_counter < 0) {
        inverter = inverter * -1;
        sustain_counter += inverter;
      }
    }
  } else {
    uint32_t pos = 0;
    while (pos < n_samples && sustain_counter < samples) {
      sample_buffer[sustain_counter] = input[pos];
      ++sustain_counter;
      ++pos;
    }
    if (sustain_counter == samples) {
      sustain_init = true;
      sustain_counter = 0;
    }
  }
}

static void
deactivate(LV2_Handle instance)
{
  memset(&sample_buffer[0], 0, samples);
  sustain_init = false;
  sustain_counter = 0;
  fprintf(stderr, "Successfully deactivated\n");
}

static void
cleanup(LV2_Handle instance)
{
  fprintf(stderr, "Successfully cleaning up\n");
	free(instance);
  free(sample_buffer);
}

static const void*
extension_data(const char* uri)
{
  fprintf(stderr, "Getting extension data\n");
	return nullptr;
}

static const LV2_Descriptor descriptor = {
	TEST_URI,
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
  switch(index) {
    case 0: return &descriptor;
    default: return nullptr;
  }
}