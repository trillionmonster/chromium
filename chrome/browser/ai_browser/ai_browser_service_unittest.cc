// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/ai_browser_service.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ai_browser {

// === Data Structure Tests ===

TEST(AnalyzeResultTest, DefaultValues) {
  AnalyzeResult result;
  EXPECT_FALSE(result.success);
  EXPECT_TRUE(result.answer.empty());
  EXPECT_TRUE(result.url.empty());
  EXPECT_TRUE(result.title.empty());
  EXPECT_TRUE(result.extraction_method.empty());
  EXPECT_TRUE(result.error.empty());
}

TEST(SearchAnalyzeResultTest, DefaultValues) {
  SearchAnalyzeResult result;
  EXPECT_FALSE(result.success);
  EXPECT_TRUE(result.answer.empty());
  EXPECT_TRUE(result.sources.empty());
  EXPECT_TRUE(result.error.empty());
}

TEST(AnalyzeResultTest, SetFields) {
  AnalyzeResult result;
  result.success = true;
  result.answer = "The answer";
  result.url = "https://example.com";
  result.title = "Example";
  result.extraction_method = "dom";

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.answer, "The answer");
  EXPECT_EQ(result.url, "https://example.com");
  EXPECT_EQ(result.title, "Example");
  EXPECT_EQ(result.extraction_method, "dom");
}

TEST(SearchAnalyzeResultTest, SetFieldsWithSources) {
  SearchAnalyzeResult result;
  result.success = true;
  result.answer = "Summary";

  AnalyzeResult source1;
  source1.success = true;
  source1.url = "https://a.com";
  source1.extraction_method = "dom";

  AnalyzeResult source2;
  source2.success = false;
  source2.url = "https://b.com";
  source2.error = "timeout";

  result.sources.push_back(std::move(source1));
  result.sources.push_back(std::move(source2));

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.sources.size(), 2u);
  EXPECT_TRUE(result.sources[0].success);
  EXPECT_FALSE(result.sources[1].success);
  EXPECT_EQ(result.sources[1].error, "timeout");
}

}  // namespace ai_browser
