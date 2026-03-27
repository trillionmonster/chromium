// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_BROWSER_SMART_SCROLLER_H_
#define CHROME_BROWSER_AI_BROWSER_SMART_SCROLLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "content/public/browser/web_contents.h"

namespace ai_browser {

// Scrolls a page incrementally to trigger lazy-loaded content, using a
// MutationObserver injected into the page to detect new DOM nodes. Scrolling
// stops when either:
//   - No new mutations are detected for |kMaxIdleScrolls| consecutive scrolls.
//   - The maximum number of scroll iterations is reached.
//   - The bottom of the page has been reached.
class SmartScroller {
 public:
  // Called when the scrolling session completes. |success| is true if the page
  // was scrolled to the bottom (or close to it) without error.
  using ScrollDoneCallback = base::OnceCallback<void(bool success)>;

  SmartScroller();
  ~SmartScroller();

  SmartScroller(const SmartScroller&) = delete;
  SmartScroller& operator=(const SmartScroller&) = delete;

  // Begins incrementally scrolling |web_contents| towards the bottom.
  // |callback| is invoked once scrolling finishes.
  void ScrollToBottom(content::WebContents* web_contents,
                      ScrollDoneCallback callback);

  // Cancels any in-progress scrolling session.
  void Cancel();

  // Returns true if a scrolling session is currently active.
  bool IsScrolling() const;

  // Configuration setters (call before ScrollToBottom).
  void set_scroll_pause(base::TimeDelta pause) { scroll_pause_ = pause; }
  void set_max_iterations(int max) { max_iterations_ = max; }
  void set_max_idle_scrolls(int max) { max_idle_scrolls_ = max; }

  // Default configuration values.
  static constexpr base::TimeDelta kDefaultScrollPause =
      base::Milliseconds(500);
  static constexpr int kDefaultMaxIterations = 100;
  static constexpr int kDefaultMaxIdleScrolls = 3;

 private:
  // Injects the MutationObserver setup script and starts the first scroll.
  void InjectScriptAndStart();

  // Called once the setup script injection completes.
  void OnSetupScriptInjected(base::Value result);

  // Executes a single scroll step via JavaScript.
  void PerformScrollStep();

  // Called with the result of the scroll step script.
  void OnScrollStepResult(base::Value result);

  // Finishes the session and invokes the done callback.
  void Finish(bool success);

  // The WebContents currently being scrolled.
  raw_ptr<content::WebContents> web_contents_ = nullptr;

  // Callback to invoke when scrolling is complete.
  ScrollDoneCallback done_callback_;

  // Timer used to pause between scroll steps.
  base::OneShotTimer scroll_timer_;

  // Configuration.
  base::TimeDelta scroll_pause_ = kDefaultScrollPause;
  int max_iterations_ = kDefaultMaxIterations;
  int max_idle_scrolls_ = kDefaultMaxIdleScrolls;

  // Session state.
  int current_iteration_ = 0;
  int consecutive_idle_scrolls_ = 0;
  bool is_scrolling_ = false;

  base::WeakPtrFactory<SmartScroller> weak_factory_{this};
};

}  // namespace ai_browser

#endif  // CHROME_BROWSER_AI_BROWSER_SMART_SCROLLER_H_
