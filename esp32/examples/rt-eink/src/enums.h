#ifndef ENUMS
#define ENUMS

// The state in which the device can be. This mainly affects what
// is drawn on the display.
enum DEVICE_STATE {
  CONNECTING_WIFI,
  CONNECTING_MQTT,
  FETCHING_TIME,
  FETCHING_SENS,
  UP_AUTONOMOUS,
  UP,
};

// Place to store all the variables that need to be displayed.
// All other functions should update these!
struct Values {
  double t_amb;
  double t_exc;
  double t_sau;
  double t_in;
  double t_out;
  double p_sys;
  
  bool   b_on;
  int8_t wifi_strength;
  DEVICE_STATE currentState;
  String time = "xx:xx";
};

#endif
