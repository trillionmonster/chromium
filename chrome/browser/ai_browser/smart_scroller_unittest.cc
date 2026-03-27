// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/smart_scroller.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ai_browser {

// SmartScroller tests focus on configuration and state management.
// Full scrolling behavior requires a live RenderFrameHost so is better
// suited for browser_tests.

class SmartScrollerTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SmartScrollerTest, DefaultConfiguration) {
  SmartScroller scroller;
  // Verify default constants.
  EXPECT_EQ(SmartScroller::kDefaultScrollPause, base::Milliseconds(500));
  EXPECT_EQ(SmartScroller::kDefaultMaxIterations, 100);
  EXPECT_EQ(SmartScroller::kDefaultMaxIdleScrolls, 3);
}

TEST_F(SmartScrollerTest, InitiallyNotScrolling) {
  SmartScroller scroller;
  EXPECT_FALSE(scroller.IsScrolling());
}

TEST_F(SmartScrollerTest, CancelWhenNotScrollingIsNoOp) {
  SmartScroller scroller;
  // Should not crash.
  scroller.Cancel();
  EXPECT_FALSE(scroller.IsScrolling());
}

TEST_F(SmartScrollerTest, SetScrollPause) {
  SmartScroller scroller;
  scroller.set_scroll_pause(base::Milliseconds(200));
  // No direct getter, but setting should not crash.
  // The effect is tested in integration/browser_tests.
}

TEST_F(SmartScrollerTest, SetMaxIterations) {
  SmartScroller scroller;
  scroller.set_max_iterations(50);
  // Verified indirectly via scroll behavior.
}

TEST_F(SmartScrollerTest, SetMaxIdleScrolls) {
  SmartScroller scroller;
  scroller.set_max_idle_scrolls(5);
}

TEST_F(SmartScrollerTest, DestructorDuringIdleIsSafe) {
  // Create and immediately destroy — no crash.
  { SmartScroller scroller; }
}

}  // namespace ai_browser
