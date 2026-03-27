// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_BROWSER_AI_BROWSER_TAB_HELPER_H_
#define CHROME_BROWSER_AI_BROWSER_AI_BROWSER_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace ai_browser {

class AIBrowserService;

// Per-tab helper that provides AI functionality for individual tabs.
// Attached to each WebContents via WebContentsUserData pattern.
class AIBrowserTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<AIBrowserTabHelper> {
 public:
  ~AIBrowserTabHelper() override;

  AIBrowserTabHelper(const AIBrowserTabHelper&) = delete;
  AIBrowserTabHelper& operator=(const AIBrowserTabHelper&) = delete;

  // Trigger analysis of the current page with the given question.
  void AnalyzeCurrentPage(const std::string& question,
                          base::OnceCallback<void(std::string)> callback);

 private:
  friend class content::WebContentsUserData<AIBrowserTabHelper>;
  explicit AIBrowserTabHelper(content::WebContents* web_contents);

  // WebContentsObserver overrides.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace ai_browser

#endif  // CHROME_BROWSER_AI_BROWSER_AI_BROWSER_TAB_HELPER_H_
