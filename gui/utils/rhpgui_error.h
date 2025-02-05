#ifndef RHPGUI_ERROR_H 
#define RHPGUI_ERROR_H 

#include "rhp_gui_data.h"

#define RHP_SOCKET_FATAL(...) error_init(__VA_ARGS__)
#define RHP_SOCKET_LOGGER(...) loggerfmt(ErrorGui, __VA_ARGS__)

#include "rhp_socket_error.h"
#endif // RHPGUI_ERROR_H 
