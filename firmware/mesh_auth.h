// Authenticated framing for the ESP-NOW link between the mesh hub and the
// garage node.
//
// ESPHome's espnow component sends frames unencrypted, and anyone nearby can
// sniff a frame, replay it, or forge one with a spoofed MAC. For a garage
// door that isn't acceptable, so every frame carries a counter and a
// truncated HMAC-SHA256 over it with a shared key:
//
//     <counter>|<payload>|<tag>      tag = first 8 bytes of HMAC, as 16 hex chars
//
// The receiver accepts a frame only if the tag matches and the counter is
// higher than the last one it accepted from that sender (stops replays). The
// payload is still readable over the air; it's the same text the mesh
// carries anyway.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "mbedtls/md.h"

namespace mesh_auth {

static constexpr size_t TAG_HEX_LEN = 16;

inline std::string tag(const std::string &key, const std::string &signed_part) {
  uint8_t mac[32];
  const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  mbedtls_md_hmac(info, reinterpret_cast<const uint8_t *>(key.data()), key.size(),
                  reinterpret_cast<const uint8_t *>(signed_part.data()), signed_part.size(), mac);
  char hex[TAG_HEX_LEN + 1];
  for (size_t i = 0; i < TAG_HEX_LEN / 2; i++)
    snprintf(hex + 2 * i, 3, "%02x", mac[i]);
  return std::string(hex, TAG_HEX_LEN);
}

// Builds "<counter>|<payload>|<tag>".
inline std::string seal(const std::string &key, uint32_t counter, const std::string &payload) {
  std::string signed_part = std::to_string(counter) + "|" + payload;
  return signed_part + "|" + tag(key, signed_part);
}

// Constant-time string compare, so tag checks don't leak timing.
inline bool equal_ct(const std::string &a, const std::string &b) {
  if (a.size() != b.size())
    return false;
  uint8_t diff = 0;
  for (size_t i = 0; i < a.size(); i++)
    diff |= static_cast<uint8_t>(a[i] ^ b[i]);
  return diff == 0;
}

// Checks a frame. On success fills payload and counter and returns true. The
// caller still has to compare counter against the last accepted one.
inline bool open(const std::string &key, const uint8_t *data, size_t size, std::string &payload,
                 uint32_t &counter) {
  std::string frame(reinterpret_cast<const char *>(data), size);
  size_t first = frame.find('|');
  size_t last = frame.rfind('|');
  if (first == std::string::npos || last == first || frame.size() - last - 1 != TAG_HEX_LEN)
    return false;
  std::string signed_part = frame.substr(0, last);
  if (!equal_ct(tag(key, signed_part), frame.substr(last + 1)))
    return false;
  char *end = nullptr;
  unsigned long c = strtoul(frame.c_str(), &end, 10);
  if (end != frame.c_str() + first)
    return false;
  counter = static_cast<uint32_t>(c);
  payload = frame.substr(first + 1, last - first - 1);
  return true;
}

}  // namespace mesh_auth
