// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_BROWSER_SEARCH_DISPATCHER_H_
#define CHROME_BROWSER_AI_BROWSER_SEARCH_DISPATCHER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace ai_browser {

class AIClient;

struct SearchResult {
  GURL url;
  std::string title;
  std::string snippet;
};

using SearchCallback =
    base::OnceCallback<void(bool success, std::vector<SearchResult> results)>;

// Dispatches a search query to a search engine, extracts result URLs.
class SearchDispatcher {
 public:
  SearchDispatcher(content::BrowserContext* context, AIClient* ai_client);
  ~SearchDispatcher();

  SearchDispatcher(const SearchDispatcher&) = delete;
  SearchDispatcher& operator=(const SearchDispatcher&) = delete;

  void Search(const std::string& query,
              int max_results,
              SearchCallback callback);

 private:
  void OnSearchPageLoaded(content::WebContents* web_contents,
                          int max_results,
                          SearchCallback callback);

  void ParseSearchResultsViaJS(content::WebContents* web_contents,
                               int max_results,
                               SearchCallback callback);

  raw_ptr<content::BrowserContext> browser_context_;
  raw_ptr<AIClient> ai_client_;
  std::vector<std::unique_ptr<content::WebContents>> active_tabs_;

  base::WeakPtrFactory<SearchDispatcher> weak_factory_{this};
};

}  // namespace ai_browser

#endif  // CHROME_BROWSER_AI_BROWSER_SEARCH_DISPATCHER_H_
