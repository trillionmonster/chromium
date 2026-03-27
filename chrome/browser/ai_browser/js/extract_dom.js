// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// extract_dom.js — Injected into pages to extract clean text content.
// Returns a JSON string with text, links, title, and url.

(function() {
  'use strict';
  try {
    // Clone body to avoid mutating the live DOM.
    var clone = document.body.cloneNode(true);

    // Remove noise elements that don't contribute to main content.
    var noiseSelectors = [
      'script', 'style', 'noscript', 'iframe', 'nav', 'footer',
      'header', 'aside', 'svg', '[role="banner"]', '[role="navigation"]',
      '[role="complementary"]', '[role="contentinfo"]',
      '.ad', '.ads', '.advertisement', '.adsbygoogle',
      '[class*="cookie"]', '[class*="popup"]', '[class*="modal"]',
      '[class*="sidebar"]', '[class*="menu"]', '[class*="share"]',
      '[class*="social"]', '[class*="comment-form"]',
      '[aria-hidden="true"]'
    ];

    for (var i = 0; i < noiseSelectors.length; i++) {
      var els = clone.querySelectorAll(noiseSelectors[i]);
      for (var j = 0; j < els.length; j++) {
        els[j].remove();
      }
    }

    // Extract cleaned text content.
    var text = (clone.innerText || clone.textContent || '');
    // Collapse excessive whitespace.
    text = text.replace(/[ \t]+/g, ' ');
    text = text.replace(/\n{3,}/g, '\n\n');
    text = text.trim();

    // Extract links with their text.
    var links = [];
    var anchors = document.querySelectorAll('a[href]');
    for (var k = 0; k < anchors.length && k < 200; k++) {
      var a = anchors[k];
      var href = a.href;
      var linkText = (a.innerText || a.textContent || '').trim();
      if (href && linkText && linkText.length > 0 &&
          !href.startsWith('javascript:') &&
          !href.startsWith('mailto:')) {
        links.push({
          url: href,
          text: linkText.substring(0, 200)
        });
      }
    }

    return JSON.stringify({
      text: text.substring(0, 500000),
      links: links,
      title: document.title || '',
      url: window.location.href
    });
  } catch (e) {
    return JSON.stringify({
      text: '',
      links: [],
      title: document.title || '',
      url: window.location.href,
      error: e.message
    });
  }
})();
