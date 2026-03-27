// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/smart_scroller.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace ai_browser {

namespace {

// JavaScript source that sets up the MutationObserver and defines the
// scrollStep() function. This is kept inline to avoid a runtime file read;
// the canonical version lives in js/scroll_page.js.
constexpr char kSetupScript[] = R"js(
(function() {
  if (window.__aiSmartScroller) {
    // Already initialised — reset mutation counter only.
    window.__aiSmartScroller.mutationCount = 0;
    return true;
  }

  window.__aiSmartScroller = {
    mutationCount: 0,
    observer: null
  };

  window.__aiSmartScroller.observer = new MutationObserver(function(mutations) {
    window.__aiSmartScroller.mutationCount += mutations.length;
  });

  window.__aiSmartScroller.observer.observe(document.body, {
    childList: true,
    subtree: true
  });

  return true;
})();
)js";

// JavaScript that performs one scroll step. Scrolls down by one viewport
// height and returns a JSON object with current scroll state.
constexpr char kScrollStepScript[] = R"js(
(function() {
  var viewportHeight = window.innerHeight;
  window.scrollBy(0, viewportHeight);

  var result = {
    scrollY: window.scrollY || window.pageYOffset,
    scrollMax: document.body.scrollHeight,
    viewportHeight: viewportHeight,
    mutationCount: window.__aiSmartScroller
        ? window.__aiSmartScroller.mutationCount
        : 0,
    done: (window.scrollY || window.pageYOffset) + viewportHeight >=
          document.body.scrollHeight
  };

  // Reset mutation count after reading.
  if (window.__aiSmartScroller) {
    window.__aiSmartScroller.mutationCount = 0;
  }

  return JSON.stringify(result);
})();
)js";

}  // namespace

SmartScroller::SmartScroller() = default;

SmartScroller::~SmartScroller() {
  Cancel();
}

void SmartScroller::ScrollToBottom(content::WebContents* web_contents,
                                   ScrollDoneCallback callback) {
  DCHECK(web_contents);
  DCHECK(callback);
  DCHECK(!is_scrolling_) << "ScrollToBottom called while already scrolling";

  web_contents_ = web_contents;
  done_callback_ = std::move(callback);
  current_iteration_ = 0;
  consecutive_idle_scrolls_ = 0;
  is_scrolling_ = true;

  InjectScriptAndStart();
}

void SmartScroller::Cancel() {
  if (!is_scrolling_) {
    return;
  }

  scroll_timer_.Stop();
  is_scrolling_ = false;
  web_contents_ = nullptr;

  if (done_callback_) {
    std::move(done_callback_).Run(/*success=*/false);
  }
}

bool SmartScroller::IsScrolling() const {
  return is_scrolling_;
}

void SmartScroller::InjectScriptAndStart() {
  if (!web_contents_ || !web_contents_->GetPrimaryMainFrame()) {
    Finish(/*success=*/false);
    return;
  }

  web_contents_->GetPrimaryMainFrame()->ExecuteJavaScriptInIsolatedWorld(
      base::UTF8ToUTF16(std::string(kSetupScript)),
      base::BindOnce(&SmartScroller::OnSetupScriptInjected,
                      weak_factory_.GetWeakPtr()),
      content::ISOLATED_WORLD_ID_CONTENT_END);
}

void SmartScroller::OnSetupScriptInjected(base::Value result) {
  if (!is_scrolling_) {
    return;
  }

  if (!result.is_bool() || !result.GetBool()) {
    LOG(WARNING) << "SmartScroller: setup script injection failed";
    Finish(/*success=*/false);
    return;
  }

  DVLOG(1) << "SmartScroller: MutationObserver installed, starting scroll";
  PerformScrollStep();
}

void SmartScroller::PerformScrollStep() {
  if (!is_scrolling_) {
    return;
  }

  if (!web_contents_ || !web_contents_->GetPrimaryMainFrame()) {
    Finish(/*success=*/false);
    return;
  }

  ++current_iteration_;
  if (current_iteration_ > max_iterations_) {
    DVLOG(1) << "SmartScroller: reached max iterations (" << max_iterations_
             << ")";
    Finish(/*success=*/true);
    return;
  }

  web_contents_->GetPrimaryMainFrame()->ExecuteJavaScriptInIsolatedWorld(
      base::UTF8ToUTF16(std::string(kScrollStepScript)),
      base::BindOnce(&SmartScroller::OnScrollStepResult,
                      weak_factory_.GetWeakPtr()),
      content::ISOLATED_WORLD_ID_CONTENT_END);
}

void SmartScroller::OnScrollStepResult(base::Value result) {
  if (!is_scrolling_) {
    return;
  }

  // The script returns a JSON string; parse it.
  if (!result.is_string()) {
    LOG(WARNING) << "SmartScroller: scroll step returned non-string";
    Finish(/*success=*/false);
    return;
  }

  std::optional<base::Value> parsed =
      base::JSONReader::Read(result.GetString());
  if (!parsed || !parsed->is_dict()) {
    LOG(WARNING) << "SmartScroller: failed to parse scroll step result";
    Finish(/*success=*/false);
    return;
  }

  const base::Value::Dict& dict = parsed->GetDict();

  std::optional<double> scroll_y = dict.FindDouble("scrollY");
  std::optional<double> scroll_max = dict.FindDouble("scrollMax");
  std::optional<int> mutation_count = dict.FindInt("mutationCount");
  std::optional<bool> done = dict.FindBool("done");

  if (!scroll_y || !scroll_max || !mutation_count || !done) {
    LOG(WARNING) << "SmartScroller: incomplete scroll step result";
    Finish(/*success=*/false);
    return;
  }

  DVLOG(2) << "SmartScroller: step " << current_iteration_
           << " scrollY=" << *scroll_y << " scrollMax=" << *scroll_max
           << " mutations=" << *mutation_count << " done=" << *done;

  // Check if we've reached the bottom of the page.
  if (*done) {
    DVLOG(1) << "SmartScroller: reached page bottom";
    Finish(/*success=*/true);
    return;
  }

  // Track consecutive scrolls with no new DOM mutations.
  if (*mutation_count == 0) {
    ++consecutive_idle_scrolls_;
    if (consecutive_idle_scrolls_ >= max_idle_scrolls_) {
      DVLOG(1) << "SmartScroller: no new content for "
               << consecutive_idle_scrolls_ << " consecutive scrolls, stopping";
      Finish(/*success=*/true);
      return;
    }
  } else {
    consecutive_idle_scrolls_ = 0;
  }

  // Schedule the next scroll after a pause to allow lazy content to load.
  scroll_timer_.Start(
      FROM_HERE, scroll_pause_,
      base::BindOnce(&SmartScroller::PerformScrollStep,
                     weak_factory_.GetWeakPtr()));
}

void SmartScroller::Finish(bool success) {
  scroll_timer_.Stop();
  is_scrolling_ = false;
  web_contents_ = nullptr;

  if (done_callback_) {
    std::move(done_callback_).Run(success);
  }
}

}  // namespace ai_browser
