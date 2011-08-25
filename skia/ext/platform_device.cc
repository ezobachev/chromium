// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/platform_device.h"

#include "third_party/skia/include/core/SkMetaData.h"

namespace skia {

namespace {
const char* kDevicePlatformBehaviour = "CrDevicePlatformBehaviour";
}

void SetPlatformDevice(SkDevice* device, PlatformDevice* platform_behaviour) {
  SkMetaData& meta_data = device->getMetaData();
  meta_data.setPtr(kDevicePlatformBehaviour, platform_behaviour);
}

PlatformDevice* GetPlatformDevice(SkDevice* device) {
  SkMetaData& meta_data = device->getMetaData();
  PlatformDevice* device_behaviour = NULL;
  if (meta_data.findPtr(kDevicePlatformBehaviour,
                        reinterpret_cast<void**>(&device_behaviour)))
    return device_behaviour;

  return NULL;
}

}  // namespace skia

