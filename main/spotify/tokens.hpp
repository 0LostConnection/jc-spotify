#pragma once

#include "esp_err.h"

#include <cstddef>
#include <cstdint>

namespace spotify {

struct Tokens {
    char access[512]{};
    char refresh[512]{};
    int64_t expires_at_ms = 0; /* esp_timer absolute ms when access expires */
};

esp_err_t tokens_load(Tokens *out);
esp_err_t tokens_save(const Tokens &tokens);
esp_err_t tokens_clear();

bool tokens_access_valid(const Tokens &tokens, int skew_ms = 60'000);

}  // namespace spotify
