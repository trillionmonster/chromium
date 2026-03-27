// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/parallel_tab_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/page_transition_types.h"

namespace ai_browser {

// ---------------------------------------------------------------------------
// ParallelTabManager::LoadObserver
// ---------------------------------------------------------------------------

// Observes a single off-screen WebContents and notifies the owning
// ParallelTabManager when the page finishes loading or a navigation error
// occurs.
class ParallelTabManager::LoadObserver
    : public content::WebContentsObserver {
 public:
  LoadObserver(std::unique_ptr<content::WebContents> web_contents,
               const GURL& url,
               ParallelTabManager* manager)
      : content::WebContentsObserver(web_contents.get()),
        owned_web_contents_(std::move(web_contents)),
        url_(url),
        manager_(manager) {}

  ~LoadObserver() override = default;

  LoadObserver(const LoadObserver&) = delete;
  LoadObserver& operator=(const LoadObserver&) = delete;

  content::WebContents* web_contents() const {
    return owned_web_contents_.get();
  }

  const GURL& url() const { return url_; }

  // Starts the per-tab timeout timer.
  void StartTimeout(base::TimeDelta timeout) {
    if (timeout.is_zero()) {
      return;
    }
    timeout_timer_.Start(
        FROM_HERE, timeout,
        base::BindOnce(&ParallelTabManager::OnTabTimeout,
                       manager_->weak_factory_.GetWeakPtr(), this));
  }

  // Cancels the timeout timer (e.g. on successful load).
  void CancelTimeout() { timeout_timer_.Stop(); }

 private:
  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    // Only care about main-frame navigations.
    if (!navigation_handle->IsInMainFrame()) {
      return;
    }

    if (navigation_handle->IsErrorPage() ||
        !navigation_handle->HasCommitted()) {
      LOG(WARNING) << "ParallelTabManager: navigation error for " << url_;
      CancelTimeout();
      manager_->OnTabFinished(this, /*success=*/false);
    }
    // Wait for DidFinishLoad for successful navigations.
  }

  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    // Only fire for the main frame.
    if (render_frame_host != web_contents()->GetPrimaryMainFrame()) {
      return;
    }

    DVLOG(1) << "ParallelTabManager: finished loading " << url_;
    CancelTimeout();
    manager_->OnTabFinished(this, /*success=*/true);
  }

  std::unique_ptr<content::WebContents> owned_web_contents_;
  GURL url_;
  raw_ptr<ParallelTabManager> manager_;
  base::OneShotTimer timeout_timer_;
};

// ---------------------------------------------------------------------------
// ParallelTabManager
// ---------------------------------------------------------------------------

ParallelTabManager::ParallelTabManager(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK(browser_context_);
}

ParallelTabManager::~ParallelTabManager() {
  CleanUp();
}

void ParallelTabManager::OpenMultiple(const std::vector<GURL>& urls,
                                      size_t max_concurrent,
                                      TabReadyCallback per_tab_callback,
                                      AllDoneCallback all_done_callback,
                                      base::TimeDelta timeout) {
  DCHECK(!urls.empty());
  DCHECK_GT(max_concurrent, 0u);
  DCHECK(per_tab_callback);
  DCHECK(all_done_callback);

  // Clean up any previous run.
  CleanUp();

  max_concurrent_ = max_concurrent;
  per_tab_callback_ = std::move(per_tab_callback);
  all_done_callback_ = std::move(all_done_callback);
  timeout_ = timeout;
  total_urls_ = urls.size();
  completed_count_ = 0;
  had_error_ = false;

  for (const auto& url : urls) {
    pending_urls_.push(url);
  }

  // Kick off the first batch.
  MaybeStartNext();
}

void ParallelTabManager::CleanUp() {
  // Destroy active observers (and their owned WebContents).
  active_observers_.clear();

  // Drain the queue.
  std::queue<GURL> empty;
  pending_urls_.swap(empty);

  per_tab_callback_.Reset();
  all_done_callback_.Reset();

  total_urls_ = 0;
  completed_count_ = 0;
}

size_t ParallelTabManager::ActiveTabCount() const {
  return active_observers_.size();
}

size_t ParallelTabManager::PendingUrlCount() const {
  return pending_urls_.size();
}

void ParallelTabManager::MaybeStartNext() {
  while (active_observers_.size() < max_concurrent_ && !pending_urls_.empty()) {
    GURL url = pending_urls_.front();
    pending_urls_.pop();

    // Create an off-screen WebContents.
    content::WebContents::CreateParams create_params(browser_context_);
    create_params.initially_hidden = true;
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContents::Create(create_params);

    // Navigate to the target URL.
    content::NavigationController::LoadURLParams load_params(url);
    load_params.transition_type = ui::PAGE_TRANSITION_TYPED;
    web_contents->GetController().LoadURLWithParams(load_params);

    DVLOG(1) << "ParallelTabManager: started loading " << url;

    // Create the observer and start its timeout.
    auto observer = std::make_unique<LoadObserver>(std::move(web_contents),
                                                   url, this);
    observer->StartTimeout(timeout_);
    active_observers_.push_back(std::move(observer));
  }
}

void ParallelTabManager::OnTabFinished(LoadObserver* observer, bool success) {
  if (!success) {
    had_error_ = true;
  }

  // Notify the caller for successful loads.
  if (success && per_tab_callback_) {
    per_tab_callback_.Run(observer->web_contents(), observer->url());
  }

  ++completed_count_;

  // Remove the observer from the active set. We iterate to find a matching
  // raw pointer.
  for (auto it = active_observers_.begin(); it != active_observers_.end();
       ++it) {
    if (it->get() == observer) {
      active_observers_.erase(it);
      break;
    }
  }

  // Check if all URLs have been processed.
  if (completed_count_ == total_urls_) {
    if (all_done_callback_) {
      std::move(all_done_callback_).Run(!had_error_);
    }
    return;
  }

  // Start the next queued URL.
  MaybeStartNext();
}

void ParallelTabManager::OnTabTimeout(LoadObserver* observer) {
  LOG(WARNING) << "ParallelTabManager: timeout loading " << observer->url();
  OnTabFinished(observer, /*success=*/false);
}

}  // namespace ai_browser
