#pragma once
#include "core/AppState.h"

class BoilerService {
public:
    void begin();
    void update(AppState& state);

    // TODO: po analizie NexaPanel dodać dokładnie te same komendy,
    // które są rzeczywiście używane w panelu kuchennym.
};
