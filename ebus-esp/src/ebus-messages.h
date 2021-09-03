#pragma once

#include "Ebus.h"

struct identification_t {
   char device[6];
   char sw_version[6];
   char hw_version[6];
};

struct info_t {
  identification_t self;
  identification_t heater;
};

extern struct info_t* system_info;

void handleMessage(Ebus::Telegram telegram);
