// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/stealth_injector.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ai_browser {

// StealthInjector tests. The actual JS injection requires a RenderFrameHost
// (tested in browser_tests), but we can verify construction and that the
// injector doesn't crash without a live WebContents.

TEST(StealthInjectorTest, ConstructionAndDestruction) {
  StealthInjector injector;
  // Should not crash.
}

TEST(StealthInjectorTest, InjectOnNullWebContentsDoesNotCrash) {
  StealthInjector injector;
  // Passing nullptr — should handle gracefully and not crash.
  injector.InjectStealth(nullptr);
}

}  // namespace ai_browser
