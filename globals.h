// globals.h
#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>

// Variables globales partagées entre .ino et .cpp
//extern bool intrusion_detectee;
extern volatile bool surveillanceActive;

// Prototypes partagés
String getTimeString();

#endif // GLOBALS_H
#pragma once
extern volatile bool intrusion_detectee;
