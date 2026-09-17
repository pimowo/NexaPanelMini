#include "services/OtaService.h"
#include <Updater.h>
#include "version.h"

namespace {
const char UPDATE_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="pl"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>NexaPanel Mini — OTA</title>
<style>body{font:16px sans-serif;max-width:480px;margin:40px auto;padding:0 16px;background:#f5f5f5;color:#222}button,input{margin:12px 0;padding:10px;max-width:100%;box-sizing:border-box}progress{width:100%;height:24px}#status{white-space:pre-wrap}#filename{overflow-wrap:anywhere}</style>
<h1>NexaPanel Mini</h1><p>Firmware: )HTML" FW_VERSION R"HTML(</p>
<h2>Aktualizacja firmware</h2>
<form id="form"><input id="file" type="file" accept=".bin" aria-label="Plik firmware">
<p id="filename">Wybierz plik .bin</p><button id="button" disabled>Aktualizuj</button></form>
<progress id="progress" max="100" value="0" aria-label="Postęp uploadu"></progress>
<p id="percent">0%</p><p id="status" role="status" aria-live="polite"></p>
<script>
const file=document.getElementById('file'),button=document.getElementById('button'),
status=document.getElementById('status'),progress=document.getElementById('progress'),
percent=document.getElementById('percent');
file.onchange=()=>{const f=file.files[0];document.getElementById('filename').textContent=f?f.name:'Wybierz plik .bin';button.disabled=!f;};
document.getElementById('form').onsubmit=e=>{
 e.preventDefault();const f=file.files[0];if(!f)return;
 if(!f.size){status.textContent='Błąd aktualizacji: pusty plik.';return;}
 if(!f.name.toLowerCase().endsWith('.bin')){status.textContent='Wybierz plik .bin.';return;}
 button.disabled=true;file.disabled=true;progress.value=0;percent.textContent='0%';status.textContent='Wysyłanie firmware...';
 const xhr=new XMLHttpRequest(),data=new FormData();data.append('firmware',f);
 xhr.open('POST','/update');xhr.setRequestHeader('X-Firmware-Size',String(f.size));xhr.timeout=120000;
 const failure=message=>{status.textContent='Błąd aktualizacji. '+message;button.disabled=false;file.disabled=false;};
 xhr.upload.onprogress=e=>{if(e.lengthComputable){const p=Math.floor(e.loaded/e.total*100);progress.value=p;percent.textContent=p+'%';if(p===100)status.textContent='Upload 100%. Weryfikacja firmware...';}};
 xhr.onload=()=>{if(xhr.status===200){status.textContent=xhr.responseText;}else failure(xhr.responseText||('HTTP '+xhr.status));};
 xhr.onerror=()=>failure('Brak odpowiedzi urządzenia. Sprawdź połączenie i /update.');
 xhr.ontimeout=()=>failure('Przekroczono czas oczekiwania. Sprawdź stan urządzenia.');
 xhr.onabort=()=>failure('Upload przerwany.');xhr.send(data);
};
</script></html>)HTML";
constexpr uint32_t RESTART_DELAY_MS = 1500;
}

void OtaService::begin(void (*serviceDuringUpload)()) {
    serviceDuringUpload_ = serviceDuringUpload;
    server_.collectHeaders("X-Firmware-Size");
    server_.on("/update", HTTP_GET, [this]() {
        server_.sendHeader("Cache-Control", "no-store");
        server_.send_P(200, PSTR("text/html; charset=utf-8"), UPDATE_PAGE);
    });
    server_.on("/update", HTTP_POST, [this]() { finishUpload(); },
               [this]() { handleUpload(); });
    server_.onNotFound([this]() {
        server_.send(404, "text/plain; charset=utf-8", "Nie znaleziono strony.");
    });
}

void OtaService::update() {
    if (restartPending_ && millis() - restartScheduledMs_ >= RESTART_DELAY_MS) {
        Serial.println("OTA: controlled restart");
        ESP.restart();
        return;
    }
    if (WiFi.status() != WL_CONNECTED) {
        if (serverStarted_) {
            server_.stop();
            serverStarted_ = false;
            Serial.println("OTA: server stopped (Wi-Fi offline)");
        }
        return;
    }
    if (!serverStarted_) {
        server_.begin();
        serverStarted_ = true;
        Serial.printf("OTA: http://%s/update\n", WiFi.localIP().toString().c_str());
    }
    server_.handleClient();
    // Native parsing handles a whole multipart request synchronously.
    // Clean up also if parsing fails without calling the POST handler.
    if (uploadStarted_ || error_) {
        if (Update.isRunning()) fail("Upload nie został ukończony.");
        resetUpload();
    }
}

void OtaService::handleUpload() {
    HTTPUpload& upload = server_.upload();
    if (restartPending_) return;
    switch (upload.status) {
    case UPLOAD_FILE_START: {
        if (uploadStarted_) {
            fail("Wyślij dokładnie jeden plik firmware.");
            return;
        }
        uploadStarted_ = true;
        const String& size = server_.header("X-Firmware-Size");
        const uint32_t freeSpace = ESP.getFreeSketchSpace();
        maximumBytes_ = freeSpace > 0x1000 ? (freeSpace - 0x1000) & 0xFFFFF000 : 0;
        // Stay below begin()'s limit: end(false) must always cancel failed
        // requests, even when all declared file bytes have already arrived.
        for (size_t i = 0; i < size.length(); ++i) {
            if (size[i] < '0' || size[i] > '9' ||
                expectedBytes_ > maximumBytes_ / 10) {
                fail("Nieprawidłowy rozmiar firmware.");
                return;
            }
            expectedBytes_ = expectedBytes_ * 10 + size[i] - '0';
            if (expectedBytes_ >= maximumBytes_) {
                fail("Firmware jest zbyt duży.");
                return;
            }
        }
        String filename = upload.filename;
        filename.toLowerCase();
        if (!expectedBytes_ || upload.name != "firmware" ||
            !filename.endsWith(".bin")) {
            fail("Wymagany niepusty plik .bin oraz X-Firmware-Size.");
            return;
        }
        Serial.printf("OTA: upload started, expected=%lu bytes\n",
                      static_cast<unsigned long>(expectedBytes_));
        if (!Update.begin(maximumBytes_, U_FLASH)) {
            fail("Nie można rozpocząć aktualizacji.", 500);
        }
        break;
    }
    case UPLOAD_FILE_WRITE:
        if (error_ || !uploadStarted_) return;
        if (!upload.currentSize) break;
        if (!receivedBytes_ && upload.buf[0] != 0xE9) {
            fail("Plik nie jest obrazem firmware ESP8266.");
            return;
        }
        if (upload.currentSize > expectedBytes_ - receivedBytes_) {
            fail("Rozmiar pliku nie zgadza się z deklaracją.");
            return;
        }
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            fail("Błąd zapisu firmware.", 500);
            return;
        }
        receivedBytes_ += upload.currentSize;
        if (serviceDuringUpload_ && millis() - lastServiceMs_ >= 25) {
            lastServiceMs_ = millis();
            serviceDuringUpload_();
        }
        // Also yield when a block stays in Update's RAM buffer.
        yield();
        break;
    case UPLOAD_FILE_END:
        if (!error_) {
            if (!receivedBytes_ || receivedBytes_ != expectedBytes_ ||
                receivedBytes_ != upload.totalSize) {
                fail("Pusty lub niekompletny firmware.");
            } else {
                uploadEnded_ = true;
            }
        }
        break;
    case UPLOAD_FILE_ABORTED:
        fail("Upload przerwany.");
        break;
    }
}

void OtaService::finishUpload() {
    server_.keepAlive(false);
    server_.sendHeader("Cache-Control", "no-store");
    if (restartPending_) {
        server_.send(409, "text/plain; charset=utf-8", "Trwa restart po aktualizacji.");
        return;
    }
    if (!error_ && (!uploadStarted_ || !uploadEnded_ || !Update.isRunning())) {
        fail("Brak kompletnego uploadu firmware.");
    }
    // Commit only after the entire multipart request has been parsed.
    if (!error_ && !Update.end(true)) fail("Weryfikacja firmware nie powiodła się.", 500);
    if (error_) {
        server_.send(errorStatus_, "text/plain; charset=utf-8", error_);
    } else {
        server_.send(200, "text/plain; charset=utf-8",
                     "Aktualizacja zakończona pomyślnie.\nUrządzenie uruchomi się ponownie...");
        restartPending_ = true;
        restartScheduledMs_ = millis();
        Serial.printf("OTA: success, %lu bytes; restart in 1500 ms\n",
                      static_cast<unsigned long>(receivedBytes_));
    }
}

void OtaService::fail(const char* message, int status) {
    if (!error_) {
        error_ = message;
        errorStatus_ = status;
        Serial.printf("OTA ERROR: %s\n", message);
        if (Update.hasError()) Update.printError(Serial);
    }
    if (Update.isRunning()) Update.end(false);
}

void OtaService::resetUpload() {
    uploadStarted_ = false;
    uploadEnded_ = false;
    receivedBytes_ = 0;
    expectedBytes_ = 0;
    maximumBytes_ = 0;
    error_ = nullptr;
    errorStatus_ = 400;
}
