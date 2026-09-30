#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <Arduino.h>
#include "hl7_parser.h"

bool send_json_data(const char* endpoint, Metadata* metadata, Parameter* params, int param_count);

#endif