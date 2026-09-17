#pragma once
#include <ESP8266WebServer.h>

class OtaService {
public:
    void begin(void (*serviceDuringUpload)());
    void update();
    bool isBusy() const { return uploadStarted_ || restartPending_; }
private:
    void handleUpload();
    void finishUpload();
    void fail(const char* message, int status = 400);
    void resetUpload();
    ESP8266WebServer server_{80};
    void (*serviceDuringUpload_)() = nullptr;
    bool serverStarted_ = false;
    bool uploadStarted_ = false;
    bool uploadEnded_ = false;
    bool restartPending_ = false;
    size_t receivedBytes_ = 0;
    size_t expectedBytes_ = 0;
    size_t maximumBytes_ = 0;
    const char* error_ = nullptr;
    int errorStatus_ = 400;
    uint32_t restartScheduledMs_ = 0;
    uint32_t lastServiceMs_ = 0;
};
