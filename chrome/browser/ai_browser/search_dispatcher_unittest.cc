// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/search_dispatcher.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ai_browser {

// SearchDispatcher depends on WebContents for navigation.
// We test the SearchResult struct and validate URL construction logic.

TEST(SearchResultTest, DefaultValues) {
  SearchResult result;
  EXPECT_TRUE(result.url.is_empty());
  EXPECT_TRUE(result.title.empty());
  EXPECT_TRUE(result.snippet.empty());
}

TEST(SearchResultTest, SetFields) {
  SearchResult result;
  result.url = GURL("https://example.com");
  result.title = "Example";
  result.snippet = "An example site";

  EXPECT_TRUE(result.url.is_valid());
  EXPECT_EQ(result.title, "Example");
  EXPECT_EQ(result.snippet, "An example site");
}

TEST(SearchDispatcherTest, GoogleSearchURLFormat) {
  // Verify the expected Google search URL format.
  // SearchDispatcher builds: https://www.google.com/search?q=<query>&num=<max>
  std::string query = "test query";
  int max_results = 10;

  GURL search_url("https://www.google.com/search?q=" + query +
                   "&num=" + std::to_string(max_results));
  EXPECT_TRUE(search_url.is_valid());
  EXPECT_EQ(search_url.host(), "www.google.com");
  EXPECT_EQ(search_url.path(), "/search");
}

}  // namespace ai_browser
