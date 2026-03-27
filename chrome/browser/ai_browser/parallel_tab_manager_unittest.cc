// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/parallel_tab_manager.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ai_browser {

// ParallelTabManager requires a BrowserContext to create WebContents.
// Here we test the state management and edge cases without live tabs.

class ParallelTabManagerTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ParallelTabManagerTest, InitialState) {
  // Constructor with nullptr context (only for testing state).
  ParallelTabManager manager(nullptr);
  EXPECT_EQ(manager.ActiveTabCount(), 0u);
  EXPECT_EQ(manager.PendingUrlCount(), 0u);
}

TEST_F(ParallelTabManagerTest, CleanUpWhenEmptyIsNoOp) {
  ParallelTabManager manager(nullptr);
  // Should not crash.
  manager.CleanUp();
  EXPECT_EQ(manager.ActiveTabCount(), 0u);
  EXPECT_EQ(manager.PendingUrlCount(), 0u);
}

TEST_F(ParallelTabManagerTest, DestructionDuringIdleIsSafe) {
  // Create and immediately destroy.
  { ParallelTabManager manager(nullptr); }
}

}  // namespace ai_browser
