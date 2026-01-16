#pragma once
#include <Arduino.h>

using RestartAudioFn = void (*)();

void webPanelBegin(RestartAudioFn restartCb);
void webPanelHandleClient();
