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

#ifndef THIRD_PARTY_SILIFUZZ_SNAP_TESTING_SNAP_TEST_SNAPS_H_
#define THIRD_PARTY_SILIFUZZ_SNAP_TESTING_SNAP_TEST_SNAPS_H_

#include "./snap/snap.h"
#include "./snap/testing/snap_test_types.h"
#include "./util/checks.h"
#include "./util/misc_util.h"

namespace silifuzz {

// Generated by snap_test_snaps_gen for test types defined in snap_test_types.
extern const Snap::Array<const Snap*> kSnapGeneratorTestSnaps;
extern const Snap::Array<const Snap*> kSnapRunnerTestSnaps;

// Returns a Snap generator test Snap of the given type.
inline const Snap& GetSnapGeneratorTestSnap(SnapGeneratorTestType type) {
  int idx = ToInt(type);
  CHECK_GE(idx, 0);
  CHECK_LT(idx, kSnapGeneratorTestSnaps.size);
  return *kSnapGeneratorTestSnaps.elements[idx];
}

// Returns a Snap runner test Snap of the given type.
inline const Snap& GetSnapRunnerTestSnap(SnapRunnerTestType type) {
  int idx = ToInt(type);
  CHECK_GE(idx, 0);
  CHECK_LT(idx, kSnapRunnerTestSnaps.size);
  return *kSnapRunnerTestSnaps.elements[idx];
}

}  // namespace silifuzz

#endif  // THIRD_PARTY_SILIFUZZ_SNAP_TESTING_SNAP_TEST_SNAPS_H_
