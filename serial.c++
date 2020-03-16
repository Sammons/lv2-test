#include <string>
#include <iostream>
#include <experimental/filesystem>
#include <unordered_map>
#include <functional>
#include <vector>
#include <thread>
#include <sstream>
#include "string.h"
#include "termios.h" // serial stuff
#include "unistd.h"  // close
#include "fcntl.h"   // open

namespace fs = std::experimental::filesystem;

class InputFrame
{
public:
  uint32_t id;
  uint32_t magnitude;
  uint32_t enabled;
};

// id device will be broadcasting
// this refers to a specific device we built
// with a button and a potentiometer
#define DEVICE_FIRST 327

#define MAX_DURATION 10000

class SerialDevice
{
  std::string name;
  std::thread* watcher = nullptr;

public:
  float *sample_buffer = nullptr;
  int sustain_counter = 0;
  bool sustain_init = false;
  int samples = 0;

  ~SerialDevice()
  {
    if (this->watcher != nullptr && this->watcher->joinable())
    {
      this->watcher->join();
    }
  }
  InputFrame last_frame;
  bool ready = false;

  void configure_lv2_state(double sample_rate)
  {
    this->samples = round(MAX_DURATION / 1000.0 * sample_rate);
    this->sample_buffer = (float *)calloc(1, this->samples * sizeof(float));
    if (this->sample_buffer == nullptr)
    {
      fprintf(stderr, "Failed to allocate plugin buffer");
    }
    this->sustain_counter = 0;
    this->sustain_init = false;
  }
  void reset_lv2_state()
  {
    if (this->sample_buffer != nullptr) {
      free(this->sample_buffer);
      this->sample_buffer = nullptr;
    }
    sustain_init = false;
    sustain_counter = 0;
  }

  // usb-Adafruit_Industries_LLC_Trinket_M0_410DD1F1536425050213E273046171FF-if00
  SerialDevice(const std::string &name)
  {
    this->name = name;
    std::vector<std::string> trinkets;
    bool found = false;
    for (const auto &entry : fs::directory_iterator(fs::path("/dev/serial/by-id")))
    {
      // to be concatted + /<name>/device exists
      const std::string path = entry.path().string();
      if (path.find(name) != path.npos)
      {
        std::cout << "Found: " << entry.path() << std::endl;
        found = true;
        break;
      }
    }

    if (!found)
    {
      std::cout << "Device not found" << std::endl;
      return;
    }
  }

  //bool initiate(const std::function<void(const InputFrame&)> &func)
  bool initiate()
  {
    const std::string path_string = "/dev/serial/by-id/" + this->name;
    auto file_descriptor = open(path_string.c_str(), O_RDONLY | O_NDELAY);
    if (file_descriptor == -1)
    {
      std::cout << "Failed to open trinket serial for device " << path_string << std::endl;
      return false;
    }
    struct termios config;
    if (tcgetattr((long)file_descriptor, &config) < 0)
    {
      std::cout << "Failed to get serial attributes for device " << path_string << std::endl;
      close(file_descriptor);
      return false;
    }
    // https://en.wikibooks.org/wiki/Serial_Programming/termios
    config.c_iflag = 0;
    config.c_lflag &= ~(ECHO | ECHONL);
    if (cfsetspeed(&config, B9600) < 0)
    {
      std::cout << "Failed to set input speed" << std::endl;
      close(file_descriptor);
      return false;
    }
    if (tcsetattr(file_descriptor, TCSAFLUSH, &config) < 0)
    {
      std::cout << "Failed to apply termios configuration" << std::endl;
      close(file_descriptor);
      return false;
    }
    // TODO: handle proper destruction, currently no such thing
    const auto watch_input = [file_descriptor, this]() -> void {
      std::cout << "reading" << std::endl;
      char buf[128] = { 0 };
      std::string tmp = "";
      while (true)
      {
        int num = 0;
        memset(buf, 0, 128);
        if ((num = read(file_descriptor, &buf[0], 128)) > 0)
        {
          // std::cout << "appending: [" << buf << "]" << std::endl;
          tmp += std::string(buf);
        }
        auto first_message_end = tmp.find_first_of("\n");
        if (first_message_end != tmp.npos)
        {
          std::string msg = tmp.substr(0, first_message_end);
          tmp = tmp.substr(first_message_end + 1, tmp.size() - first_message_end - 1);
          if (msg.size() > 0)
          {
            std::cout << "read msg " << msg << std::endl;
            auto stream = std::basic_stringstream(msg);
            InputFrame new_frame;
            stream >> new_frame.id;
            stream >> new_frame.magnitude;
            stream >> new_frame.enabled;
            if (new_frame.id != 327 || new_frame.enabled > 1 || new_frame.magnitude > 100) {
              std::cout << "discard frame" << std::endl;
              continue;
            }
            if (new_frame.enabled == 0 && this->last_frame.enabled != 0) {
              std::cout << "unpress" << std::endl;
              if (this->sample_buffer != nullptr) {
                memset(this->sample_buffer, 0, this->samples * sizeof(float));
                this->sustain_init = false;
                this->sustain_counter = 0;
              }
            }
            this->last_frame.enabled = new_frame.enabled;
            this->last_frame.id = new_frame.id;
            this->last_frame.magnitude = new_frame.magnitude;
          }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
    };

    new (&this->watcher) std::thread(watch_input);
    this->ready = true;
    return true;
  }
};