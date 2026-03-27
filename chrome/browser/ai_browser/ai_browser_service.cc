// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/ai_browser_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ai_browser/ai_browser_prefs.h"
#include "chrome/browser/ai_browser/ai_client.h"
#include "chrome/browser/ai_browser/auto_login_manager.h"
#include "chrome/browser/ai_browser/page_reader.h"
#include "chrome/browser/ai_browser/parallel_tab_manager.h"
#include "chrome/browser/ai_browser/search_dispatcher.h"
#include "chrome/browser/ai_browser/session_persistence.h"
#include "chrome/browser/ai_browser/smart_scroller.h"
#include "chrome/browser/ai_browser/stealth_injector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace ai_browser {

namespace {

// Observer that fires callback when navigation finishes.
class NavigationWaiter : public content::WebContentsObserver {
 public:
  NavigationWaiter(content::WebContents* contents, base::OnceClosure on_done)
      : content::WebContentsObserver(contents),
        on_done_(std::move(on_done)) {}

  void DidFinishLoad(content::RenderFrameHost* rfh,
                     const GURL& url) override {
    if (rfh->IsInPrimaryMainFrame() && on_done_) {
      // Wait a bit for dynamic content to render.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, std::move(on_done_), base::Seconds(1));
    }
  }

  void DidFailLoad(content::RenderFrameHost* rfh,
                   const GURL& url,
                   int error_code) override {
    if (rfh->IsInPrimaryMainFrame() && on_done_) {
      std::move(on_done_).Run();
    }
  }

 private:
  base::OnceClosure on_done_;
};

}  // namespace

AIBrowserService::AIBrowserService(content::BrowserContext* context)
    : browser_context_(context) {
  Profile* profile = Profile::FromBrowserContext(context);
  PrefService* prefs = profile->GetPrefs();

  auto url_loader_factory =
      context->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess();

  ai_client_ = std::make_unique<AIClient>(prefs, url_loader_factory);
  session_mgr_ = std::make_unique<SessionPersistence>(prefs);
  stealth_ = std::make_unique<StealthInjector>();
  scroller_ = std::make_unique<SmartScroller>();
  page_reader_ = std::make_unique<PageReader>(ai_client_.get());
  login_manager_ =
      std::make_unique<AutoLoginManager>(session_mgr_.get(), ai_client_.get());
  search_dispatcher_ =
      std::make_unique<SearchDispatcher>(context, ai_client_.get());
  tab_manager_ = std::make_unique<ParallelTabManager>(context);
}

AIBrowserService::~AIBrowserService() = default;

void AIBrowserService::Shutdown() {
  active_contents_.clear();
  tab_manager_.reset();
  search_dispatcher_.reset();
  login_manager_.reset();
  page_reader_.reset();
  scroller_.reset();
  stealth_.reset();
  session_mgr_.reset();
  ai_client_.reset();
}

void AIBrowserService::AnalyzePage(const GURL& url,
                                   const std::string& question,
                                   AnalyzeCallback callback) {
  // Create an off-screen WebContents.
  content::WebContents::CreateParams params(browser_context_);
  auto web_contents = content::WebContents::Create(params);
  content::WebContents* wc = web_contents.get();
  active_contents_.push_back(std::move(web_contents));

  // Apply stealth.
  stealth_->InjectStealth(wc);

  // Set up navigation waiter.
  auto waiter = std::make_unique<NavigationWaiter>(
      wc, base::BindOnce(&AIBrowserService::ProcessLoadedTab,
                         weak_factory_.GetWeakPtr(), wc, url, question,
                         std::move(callback)));

  // Navigate.
  content::NavigationController::LoadURLParams load_params(url);
  load_params.transition_type = ui::PAGE_TRANSITION_TYPED;
  wc->GetController().LoadURLWithParams(load_params);
}

void AIBrowserService::ProcessLoadedTab(content::WebContents* web_contents,
                                        const GURL& url,
                                        const std::string& question,
                                        AnalyzeCallback callback) {
  // Check if login is needed.
  login_manager_->CheckNeedsLogin(
      web_contents,
      base::BindOnce(
          [](base::WeakPtr<AIBrowserService> self,
             content::WebContents* wc, GURL url, std::string question,
             AnalyzeCallback cb, bool needs_login) {
            if (!self) return;

            auto continue_after_login = base::BindOnce(
                [](base::WeakPtr<AIBrowserService> self2,
                   content::WebContents* wc2, GURL url2,
                   std::string question2, AnalyzeCallback cb2) {
                  if (!self2) return;

                  // Smart scroll to trigger lazy loading.
                  self2->scroller_->ScrollToBottom(
                      wc2,
                      base::BindOnce(
                          [](base::WeakPtr<AIBrowserService> self3,
                             content::WebContents* wc3, GURL url3,
                             std::string question3, AnalyzeCallback cb3) {
                            if (!self3) return;
                            // Read page content.
                            self3->page_reader_->ReadPage(
                                wc3,
                                base::BindOnce(
                                    &AIBrowserService::OnPageContentReady,
                                    self3, url3, question3, std::move(cb3)));
                          },
                          self2, wc2, url2, question2, std::move(cb2)));
                },
                self, wc, url, question, std::move(cb));

            if (needs_login) {
              std::string domain = url.host();
              self->login_manager_->PerformLogin(
                  wc, domain,
                  base::BindOnce(
                      [](base::OnceClosure next_step, bool success,
                         std::string error) {
                        if (!success) {
                          LOG(WARNING) << "Auto-login failed: " << error;
                        }
                        std::move(next_step).Run();
                      },
                      std::move(continue_after_login)));
            } else {
              std::move(continue_after_login).Run();
            }
          },
          weak_factory_.GetWeakPtr(), web_contents, url, question,
          std::move(callback)));
}

void AIBrowserService::OnPageContentReady(const GURL& url,
                                          const std::string& question,
                                          AnalyzeCallback callback,
                                          PageContent content) {
  if (content.text.empty()) {
    AnalyzeResult result;
    result.url = url.spec();
    result.error = "Failed to extract page content";
    std::move(callback).Run(std::move(result));
    return;
  }

  // Ask LLM.
  ai_client_->AskLLM(
      question, content.text,
      base::BindOnce(&AIBrowserService::OnLLMAnswer,
                     weak_factory_.GetWeakPtr(), url,
                     content.extraction_method, std::move(callback)));
}

void AIBrowserService::OnLLMAnswer(const GURL& url,
                                   const std::string& extraction_method,
                                   AnalyzeCallback callback,
                                   LLMResult result) {
  AnalyzeResult ar;
  ar.url = url.spec();
  ar.extraction_method = extraction_method;
  if (result.success) {
    ar.success = true;
    ar.answer = result.text;
  } else {
    ar.error = result.error;
  }
  std::move(callback).Run(std::move(ar));
}

void AIBrowserService::SearchAndAnalyze(const std::string& query,
                                        int max_results,
                                        SearchAnalyzeCallback callback) {
  search_dispatcher_->Search(
      query, max_results,
      base::BindOnce(&AIBrowserService::OnSearchResultsReady,
                     weak_factory_.GetWeakPtr(), query, query,
                     std::move(callback)));
}

void AIBrowserService::OnSearchResultsReady(
    const std::string& query,
    const std::string& original_question,
    SearchAnalyzeCallback callback,
    bool success,
    std::vector<SearchResult> results) {
  if (!success || results.empty()) {
    SearchAnalyzeResult sar;
    sar.error = "No search results found for: " + query;
    std::move(callback).Run(std::move(sar));
    return;
  }

  // Collect URLs and analyze them in batch.
  std::vector<GURL> urls;
  for (const auto& r : results) {
    urls.push_back(r.url);
  }

  // Use BatchAnalyze with a comprehensive question.
  BatchAnalyze(
      urls, original_question,
      base::BindOnce(&AIBrowserService::OnAllBatchPagesReady,
                     weak_factory_.GetWeakPtr(), original_question,
                     std::move(callback)));
}

void AIBrowserService::OnAllBatchPagesReady(
    const std::string& question,
    SearchAnalyzeCallback callback,
    std::vector<AnalyzeResult> all_results) {
  // Combine all page contents into a single context for final summary.
  std::string combined_context;
  for (size_t i = 0; i < all_results.size(); ++i) {
    if (all_results[i].success && !all_results[i].answer.empty()) {
      combined_context += "=== Source " + std::to_string(i + 1) + ": " +
                          all_results[i].url + " ===\n" +
                          all_results[i].answer + "\n\n";
    }
  }

  if (combined_context.empty()) {
    SearchAnalyzeResult sar;
    sar.error = "Failed to extract content from any search result";
    sar.sources = std::move(all_results);
    std::move(callback).Run(std::move(sar));
    return;
  }

  // Final comprehensive summary.
  ai_client_->AskLLM(
      "Based on the content from multiple web sources below, provide a "
      "comprehensive answer to: " + question +
      "\n\nInclude key facts, different perspectives, and cite sources.",
      combined_context,
      base::BindOnce(
          [](std::vector<AnalyzeResult> sources, SearchAnalyzeCallback cb,
             LLMResult llm_result) {
            SearchAnalyzeResult sar;
            sar.sources = std::move(sources);
            if (llm_result.success) {
              sar.success = true;
              sar.answer = llm_result.text;
            } else {
              sar.error = llm_result.error;
            }
            std::move(cb).Run(std::move(sar));
          },
          std::move(all_results), std::move(callback)));
}

void AIBrowserService::BatchAnalyze(const std::vector<GURL>& urls,
                                    const std::string& question,
                                    BatchCallback callback) {
  if (urls.empty()) {
    std::move(callback).Run({});
    return;
  }

  // Use a shared counter to track completion.
  auto results = std::make_shared<std::vector<AnalyzeResult>>(urls.size());
  auto remaining = std::make_shared<int>(static_cast<int>(urls.size()));

  for (size_t i = 0; i < urls.size(); ++i) {
    AnalyzePage(
        urls[i], question,
        base::BindOnce(
            [](std::shared_ptr<std::vector<AnalyzeResult>> results_ptr,
               std::shared_ptr<int> remaining_ptr,
               BatchCallback batch_cb, size_t index,
               AnalyzeResult result) {
              (*results_ptr)[index] = std::move(result);
              --(*remaining_ptr);
              if (*remaining_ptr <= 0 && batch_cb) {
                std::move(batch_cb).Run(std::move(*results_ptr));
              }
            },
            results, remaining,
            (i == 0) ? std::move(callback) : BatchCallback(),
            i));
  }
}

}  // namespace ai_browser
