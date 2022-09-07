// Copyright 2022 The SiliFuzz Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_SILIFUZZ_UTIL_PLATFORM_H_
#define THIRD_PARTY_SILIFUZZ_UTIL_PLATFORM_H_

#include "./util/enum_flag.h"
#include "./util/itoa.h"
#include "./util/misc_util.h"

namespace silifuzz {

// Corresponds to proto::PlatformId including the storage-stable enum values.
// silifuzz/common/snapshot_proto.cc checks this.
//
// We add this instead of just using base::CPUType from //base/cpuid/cpuid.h
// because we need stable int values for usage in proto.Snapshot. This can
// also come-in handy when open-sourcing Silifuzz. There's also the chance that
// we'll discover visible microarchitectural behavior differences beyond
// the granularity of base::CPUType.
enum class PlatformId {
  kUndefined = 0,
  kIntelSkylake = 1,
  kIntelHaswell = 2,
  kIntelBroadwell = 3,
  kIntelIvybridge = 4,
  kIntelCascadelake = 5,
  kAmdRome = 6,
  kIntelIcelake = 7,
  kAmdMilan = 8,
  kIntelSapphireRapids = 9,
  kAmdGenoa = 10,
  // The values below are meta-values that don't have proto::PlatformId
  // representation. Never persisted and can be renumbered as needed.
  kAny = 11,          // any platform for platform selectors
  kNonExistent = 12,  // for tests only
};

DECLARE_ENUM_FLAG(PlatformId);

// Max valid value of PlatformId, min being kUndefined.
constexpr PlatformId kMaxPlatformId = PlatformId::kNonExistent;

// EnumStr() works for PlatformId.
template <>
extern const char* EnumNameMap<PlatformId>[ToInt(kMaxPlatformId) + 1];

// A short name for PlatformId that is unique and still human-readable.
const char* ShortPlatformName(PlatformId platform);

// Returns the PlatformId of where this code runs on or kUndefined if
// we don't have a needed PlatformId value defined or it can't be determined.
PlatformId CurrentPlatformId();

}  // namespace silifuzz

#endif  // THIRD_PARTY_SILIFUZZ_UTIL_PLATFORM_H_
