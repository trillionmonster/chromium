// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/search_dispatcher.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ai_browser/ai_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/escape.h"
#include "url/gurl.h"

namespace ai_browser {

namespace {

constexpr char kGoogleSearchURL[] = "https://www.google.com/search?q=";

// JavaScript to extract search results from Google's search page.
constexpr char kExtractSearchResultsScript[] = R"js(
(function() {
  var results = [];
  // Google search result links are typically in <div class="g"> blocks.
  var items = document.querySelectorAll('div.g, div[data-sokoban-container]');
  for (var i = 0; i < items.length; i++) {
    var link = items[i].querySelector('a[href]');
    if (!link) continue;
    var href = link.href;
    // Filter out Google internal links.
    if (!href || href.includes('google.com/search') ||
        href.startsWith('javascript:') || href.includes('#')) {
      continue;
    }
    var titleEl = items[i].querySelector('h3');
    var title = titleEl ? titleEl.innerText : (link.innerText || '');
    var snippetEl = items[i].querySelector(
        'div[data-sncf], span.aCOpRe, div.VwiC3b');
    var snippet = snippetEl ? snippetEl.innerText : '';

    if (title && href) {
      results.push({url: href, title: title.trim(), snippet: snippet.trim()});
    }
  }

  // Fallback: look for any search result link patterns.
  if (results.length === 0) {
    var allLinks = document.querySelectorAll('a[href]');
    for (var j = 0; j < allLinks.length; j++) {
      var a = allLinks[j];
      var h = a.href;
      if (h && !h.includes('google.') && !h.startsWith('javascript:') &&
          h.startsWith('http') && a.innerText.trim().length > 5) {
        results.push({url: h, title: a.innerText.trim(), snippet: ''});
      }
    }
  }

  return JSON.stringify(results);
})();
)js";

// Observer that waits for a page to finish loading.
class LoadWaiter : public content::WebContentsObserver {
 public:
  LoadWaiter(content::WebContents* contents, base::OnceClosure on_loaded)
      : content::WebContentsObserver(contents),
        on_loaded_(std::move(on_loaded)) {}

  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    if (render_frame_host->IsInPrimaryMainFrame() && on_loaded_) {
      // Small delay to let JS render.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, std::move(on_loaded_), base::Seconds(2));
    }
  }

 private:
  base::OnceClosure on_loaded_;
};

}  // namespace

SearchDispatcher::SearchDispatcher(content::BrowserContext* context,
                                   AIClient* ai_client)
    : browser_context_(context), ai_client_(ai_client) {}

SearchDispatcher::~SearchDispatcher() = default;

void SearchDispatcher::Search(const std::string& query,
                              int max_results,
                              SearchCallback callback) {
  // Build search URL.
  std::string escaped_query = net::EscapeQueryParamValue(query, true);
  GURL search_url(kGoogleSearchURL + escaped_query);

  // Create an off-screen WebContents for the search.
  content::WebContents::CreateParams params(browser_context_);
  auto web_contents = content::WebContents::Create(params);
  content::WebContents* wc_ptr = web_contents.get();
  active_tabs_.push_back(std::move(web_contents));

  // Set up load observer.
  auto waiter = std::make_unique<LoadWaiter>(
      wc_ptr,
      base::BindOnce(&SearchDispatcher::OnSearchPageLoaded,
                     weak_factory_.GetWeakPtr(), wc_ptr, max_results,
                     std::move(callback)));

  // Navigate to search page.
  content::NavigationController::LoadURLParams load_params(search_url);
  load_params.transition_type = ui::PAGE_TRANSITION_TYPED;
  wc_ptr->GetController().LoadURLWithParams(load_params);
}

void SearchDispatcher::OnSearchPageLoaded(content::WebContents* web_contents,
                                          int max_results,
                                          SearchCallback callback) {
  ParseSearchResultsViaJS(web_contents, max_results, std::move(callback));
}

void SearchDispatcher::ParseSearchResultsViaJS(
    content::WebContents* web_contents,
    int max_results,
    SearchCallback callback) {
  content::RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  if (!frame) {
    std::move(callback).Run(false, {});
    return;
  }

  frame->ExecuteJavaScript(
      base::UTF8ToUTF16(kExtractSearchResultsScript),
      base::BindOnce(
          [](int max_results, SearchCallback cb, base::Value result) {
            std::vector<SearchResult> results;

            if (!result.is_string()) {
              std::move(cb).Run(false, results);
              return;
            }

            auto parsed = base::JSONReader::Read(result.GetString());
            if (!parsed || !parsed->is_list()) {
              std::move(cb).Run(false, results);
              return;
            }

            for (const auto& item : parsed->GetList()) {
              if (!item.is_dict()) continue;
              const auto& dict = item.GetDict();
              SearchResult sr;
              if (const auto* url = dict.FindString("url")) {
                sr.url = GURL(*url);
              }
              if (const auto* title = dict.FindString("title")) {
                sr.title = *title;
              }
              if (const auto* snippet = dict.FindString("snippet")) {
                sr.snippet = *snippet;
              }
              if (sr.url.is_valid()) {
                results.push_back(std::move(sr));
              }
              if (static_cast<int>(results.size()) >= max_results) break;
            }

            std::move(cb).Run(!results.empty(), std::move(results));
          },
          max_results, std::move(callback)));

  // Clean up the search tab after a delay.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<SearchDispatcher> self,
             content::WebContents* wc) {
            if (!self) return;
            auto it = std::find_if(
                self->active_tabs_.begin(), self->active_tabs_.end(),
                [wc](const auto& ptr) { return ptr.get() == wc; });
            if (it != self->active_tabs_.end()) {
              self->active_tabs_.erase(it);
            }
          },
          weak_factory_.GetWeakPtr(), web_contents),
      base::Seconds(10));
}

}  // namespace ai_browser
