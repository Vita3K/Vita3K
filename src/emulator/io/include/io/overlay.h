#pragma once

#include <map>

// This header defines Overlay types

typedef int SceFiosOverlayID;

enum SceFiosOverlayType 
{
   SCE_FIOS_OVERLAY_TYPE_OPAQUE = 0,
   SCE_FIOS_OVERLAY_TYPE_TRANSLUCENT = 1,
   SCE_FIOS_OVERLAY_TYPE_NEWER = 2,
   SCE_FIOS_OVERLAY_TYPE_WRITABLE = 3
} SceFiosOverlayType;

struct SceFiosOverlay
{
  std::uint8_t type;
  std::uint8_t order;
  std::uint16_t dst_len;
  std::uint16_t src_len;
  std::uint16_t unk2;
  SceUID pid;
  SceFiosOverlayID id;
  char dst[292];
  char src[292];
};

struct OverlayState {
    std::map<SceFiosOverlayID, SceFiosOverlay> overlays; 
}
