// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_BROWSER_PARALLEL_TAB_MANAGER_H_
#define CHROME_BROWSER_AI_BROWSER_PARALLEL_TAB_MANAGER_H_

#include <memory>
#include <queue>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ai_browser {

// Manages opening multiple URLs in parallel off-screen tabs with a concurrency
// limit. Tabs are created as off-screen WebContents instances, navigated to
// the target URL, and the caller is notified via callbacks as each tab finishes
// loading. When all URLs have been processed the all-done callback fires.
class ParallelTabManager {
 public:
  // Called for each tab once its page finishes loading. The caller does NOT
  // take ownership of the WebContents pointer — it remains valid until the
  // ParallelTabManager is destroyed or CleanUp() is called.
  using TabReadyCallback =
      base::RepeatingCallback<void(content::WebContents*, const GURL&)>;

  // Called once when every URL has been processed (or an unrecoverable error
  // occurred). |success| is true when all URLs loaded without navigation
  // errors.
  using AllDoneCallback = base::OnceCallback<void(bool success)>;

  explicit ParallelTabManager(content::BrowserContext* browser_context);
  ~ParallelTabManager();

  ParallelTabManager(const ParallelTabManager&) = delete;
  ParallelTabManager& operator=(const ParallelTabManager&) = delete;

  // Opens |urls| in off-screen tabs, keeping at most |max_concurrent| tabs
  // loading at any given time. The remaining URLs are queued and started as
  // earlier tabs complete.
  //
  // |per_tab_callback| fires once for every successfully loaded tab.
  // |all_done_callback| fires once when all URLs have been processed.
  // |timeout| is the per-tab load timeout; a zero value means no timeout.
  void OpenMultiple(const std::vector<GURL>& urls,
                    size_t max_concurrent,
                    TabReadyCallback per_tab_callback,
                    AllDoneCallback all_done_callback,
                    base::TimeDelta timeout = base::Seconds(30));

  // Discards all active and pending tabs. Safe to call at any time.
  void CleanUp();

  // Returns the number of tabs currently loading.
  size_t ActiveTabCount() const;

  // Returns the number of URLs still waiting to be opened.
  size_t PendingUrlCount() const;

 private:
  // Per-tab load observer, created for each active WebContents.
  class LoadObserver;

  // Starts loading the next queued URL if the concurrency limit allows.
  void MaybeStartNext();

  // Called by LoadObserver when a tab finishes loading (success or failure).
  void OnTabFinished(LoadObserver* observer, bool success);

  // Called when a per-tab timeout fires.
  void OnTabTimeout(LoadObserver* observer);

  // Owning browser context used to create WebContents.
  raw_ptr<content::BrowserContext> browser_context_;

  // Callbacks supplied by the caller.
  TabReadyCallback per_tab_callback_;
  AllDoneCallback all_done_callback_;

  // Per-tab load timeout.
  base::TimeDelta timeout_;

  // Maximum number of concurrently loading tabs.
  size_t max_concurrent_ = 1;

  // URLs waiting to be opened.
  std::queue<GURL> pending_urls_;

  // Currently active observers (one per loading tab). Each observer owns its
  // associated WebContents.
  std::vector<std::unique_ptr<LoadObserver>> active_observers_;

  // Tracks whether any tab encountered a navigation error.
  bool had_error_ = false;

  // Total number of URLs requested (used to detect completion).
  size_t total_urls_ = 0;

  // Number of tabs that have completed (successfully or not).
  size_t completed_count_ = 0;

  base::WeakPtrFactory<ParallelTabManager> weak_factory_{this};
};

}  // namespace ai_browser

#endif  // CHROME_BROWSER_AI_BROWSER_PARALLEL_TAB_MANAGER_H_
