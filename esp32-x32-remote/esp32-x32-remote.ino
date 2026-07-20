#include "X32Remote.h"

X32Remote remote;

void setup() {
  remote.begin();
}

void loop() {
  remote.update();
}
