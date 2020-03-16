#include <lv2.h>
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "math.h"

#include "serial.c++"

#define TEST_URI "http://sammons.io/plugins/test"

typedef enum {
	TEST_INPUT  = 0,
	TEST_OUTPUT = 1,
  DURATION_INPUT = 2
} PortIndex;

typedef struct {
	// Port buffers
	const float* input;
	float*       output;
  float*       duration;
  int controller_magnitude;
  int controller_enable;
} Converter;



int inverter = 1;
double sample_rate = 0;
SerialDevice controller("usb-Adafruit_Industries_LLC_Trinket_M0_410DD1F1536425050213E273046171FF-if00");


static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
            double                    rate,
            const char*               bundle_path,
            const LV2_Feature* const* features)
{
	Converter* converter = (Converter*)calloc(1, sizeof(Converter));
  sample_rate = rate;
  controller.initiate(); // only call this once plz
  controller.configure_lv2_state(sample_rate);
	return (LV2_Handle)converter;
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
  case DURATION_INPUT:
    // converter->duration = (float*)data;
    break;
	}
}



/** reset everything except connect_port */
static void
activate(LV2_Handle instance)
{
  const Converter* converter = (const Converter*)instance;
  fprintf(stderr, "Successfully activated\n");
}

static void
run(LV2_Handle instance, uint32_t n_samples)
{
  if (controller.last_frame.enabled == 0) {
    return;
  }
	const Converter* converter = (const Converter*)instance;
  controller.samples = round((controller.last_frame.magnitude/100.0 * 2000.0)/1000.0 * sample_rate);
	const float* const input  = converter->input;
	float* const       output = converter->output;
  if (controller.sustain_init) {
    
    // for (int i = samples - 1; i > samples - 1000; --i) {
    //   if (sample_buffer[i] < sample_buffer[back_zero]) {
    //     back_zero = i;
    //   }
    // }
    for (uint32_t pos = 0; pos < n_samples; pos++) {
      if (controller.sustain_counter >= controller.samples) {
        inverter = inverter * -1;
        controller.sustain_counter = 0;
      }
      if (controller.sustain_counter <= 0) {
        inverter = inverter * -1;
        controller.sustain_counter = 0;
      }
      output[pos] = controller.sample_buffer[controller.sustain_counter] + input[pos];
      controller.sustain_counter += inverter;
    }
  } else {
    if (controller.sustain_counter == 0) {
      std::cout << "start recording sample" << std::endl;
    }
    uint32_t pos = 0;
    while (pos < n_samples && controller.sustain_counter < controller.samples) {
      controller.sample_buffer[controller.sustain_counter] = input[pos];
      ++controller.sustain_counter;
      ++pos;
    }
    if (controller.sustain_counter == controller.samples) {
      fprintf(stdout, "Sample ready. Samples = %f /1000.0 * %f\n",
        (controller.last_frame.magnitude/100.0 * 2000.0), sample_rate);
      controller.sustain_init = true;
      controller.sustain_counter = 0;
    }
  }
}

static void
deactivate(LV2_Handle instance)
{
  memset(controller.sample_buffer, 0, controller.samples * sizeof(float));
  controller.sustain_init = false;
  controller.sustain_counter = 0;
  fprintf(stderr, "Successfully deactivated\n");
}

static void
cleanup(LV2_Handle instance)
{
	controller.reset_lv2_state();
  fprintf(stderr, "Successfully cleaning up\n");
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