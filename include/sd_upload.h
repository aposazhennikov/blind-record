#pragma once
#include <Arduino.h>
#include <WebServer.h>

// SD Upload module.
// Handles file uploads via HTTP multipart/form-data.

// Initialize upload handlers on the web server.
void sdUploadBegin(WebServer& server);

// Get upload status message.
String sdUploadGetStatus();
