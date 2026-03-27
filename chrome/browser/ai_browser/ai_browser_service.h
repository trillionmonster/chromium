// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_BROWSER_AI_BROWSER_SERVICE_H_
#define CHROME_BROWSER_AI_BROWSER_AI_BROWSER_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace ai_browser {

class AIClient;
class AutoLoginManager;
class PageReader;
class ParallelTabManager;
class SearchDispatcher;
class SessionPersistence;
class SmartScroller;
class StealthInjector;

struct AnalyzeResult {
  bool success = false;
  std::string answer;
  std::string url;
  std::string title;
  std::string extraction_method;
  std::string error;
};

struct SearchAnalyzeResult {
  bool success = false;
  std::string answer;
  std::vector<AnalyzeResult> sources;
  std::string error;
};

using AnalyzeCallback = base::OnceCallback<void(AnalyzeResult)>;
using SearchAnalyzeCallback = base::OnceCallback<void(SearchAnalyzeResult)>;
using BatchCallback = base::OnceCallback<void(std::vector<AnalyzeResult>)>;

// Central orchestration service for the AI Browser. Coordinates all
// sub-components to provide end-to-end page analysis and search+summarize.
class AIBrowserService : public KeyedService {
 public:
  explicit AIBrowserService(content::BrowserContext* context);
  ~AIBrowserService() override;

  AIBrowserService(const AIBrowserService&) = delete;
  AIBrowserService& operator=(const AIBrowserService&) = delete;

  // Analyze a single page: navigate → read → answer.
  void AnalyzePage(const GURL& url,
                   const std::string& question,
                   AnalyzeCallback callback);

  // Search + multi-page analysis: search → parallel open → read all → summarize.
  void SearchAndAnalyze(const std::string& query,
                        int max_results,
                        SearchAnalyzeCallback callback);

  // Batch: analyze multiple URLs with the same question.
  void BatchAnalyze(const std::vector<GURL>& urls,
                    const std::string& question,
                    BatchCallback callback);

  AIClient* ai_client() { return ai_client_.get(); }
  SessionPersistence* session_persistence() { return session_mgr_.get(); }

 private:
  void Shutdown() override;

  // Internal: process a single tab that has finished loading.
  void ProcessLoadedTab(content::WebContents* web_contents,
                        const GURL& url,
                        const std::string& question,
                        AnalyzeCallback callback);

  // Called after page content is read.
  void OnPageContentReady(const GURL& url,
                          const std::string& question,
                          AnalyzeCallback callback,
                          PageContent content);

  // Called after LLM answers the question.
  void OnLLMAnswer(const GURL& url,
                   const std::string& extraction_method,
                   AnalyzeCallback callback,
                   LLMResult result);

  // Called when search results are ready for SearchAndAnalyze.
  void OnSearchResultsReady(const std::string& query,
                            const std::string& original_question,
                            SearchAnalyzeCallback callback,
                            bool success,
                            std::vector<SearchResult> results);

  // Called when all batch pages are analyzed.
  void OnAllBatchPagesReady(const std::string& question,
                            SearchAnalyzeCallback callback,
                            std::vector<AnalyzeResult> all_results);

  raw_ptr<content::BrowserContext> browser_context_;
  std::unique_ptr<AIClient> ai_client_;
  std::unique_ptr<SearchDispatcher> search_dispatcher_;
  std::unique_ptr<ParallelTabManager> tab_manager_;
  std::unique_ptr<PageReader> page_reader_;
  std::unique_ptr<AutoLoginManager> login_manager_;
  std::unique_ptr<StealthInjector> stealth_;
  std::unique_ptr<SmartScroller> scroller_;
  std::unique_ptr<SessionPersistence> session_mgr_;

  // Track active WebContents for cleanup.
  std::vector<std::unique_ptr<content::WebContents>> active_contents_;

  base::WeakPtrFactory<AIBrowserService> weak_factory_{this};
};

}  // namespace ai_browser

#endif  // CHROME_BROWSER_AI_BROWSER_AI_BROWSER_SERVICE_H_
