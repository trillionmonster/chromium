// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Smart-scroll helper injected into pages by SmartScroller.
//
// Sets up a MutationObserver on document.body to detect lazy-loaded content,
// and exposes a scrollStep(pixels) function that the C++ side calls via
// ExecuteJavaScript.

(function() {
  'use strict';

  // Guard against double-injection.
  if (window.__aiSmartScroller) {
    window.__aiSmartScroller.mutationCount = 0;
    return;
  }

  window.__aiSmartScroller = {
    mutationCount: 0,
    observer: null
  };

  // Observe childList and subtree mutations on the document body.
  window.__aiSmartScroller.observer = new MutationObserver(function(mutations) {
    window.__aiSmartScroller.mutationCount += mutations.length;
  });

  window.__aiSmartScroller.observer.observe(document.body, {
    childList: true,
    subtree: true
  });

  /**
   * Scrolls the page by |pixels| and returns a result object describing the
   * current scroll state plus how many DOM mutations have occurred since the
   * last call.
   *
   * @param {number} pixels - Number of pixels to scroll down.
   * @return {{scrollY: number, scrollMax: number, mutationCount: number,
   *           done: boolean}}
   */
  window.__aiSmartScroller.scrollStep = function(pixels) {
    window.scrollBy(0, pixels);

    var scrollY = window.scrollY || window.pageYOffset;
    var scrollMax = document.body.scrollHeight;
    var viewportHeight = window.innerHeight;

    var result = {
      scrollY: scrollY,
      scrollMax: scrollMax,
      mutationCount: window.__aiSmartScroller.mutationCount,
      done: scrollY + viewportHeight >= scrollMax
    };

    // Reset the mutation count after each read so the caller sees only
    // mutations that occurred between consecutive scroll steps.
    window.__aiSmartScroller.mutationCount = 0;

    return result;
  };
})();
