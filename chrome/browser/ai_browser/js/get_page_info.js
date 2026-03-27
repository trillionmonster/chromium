// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// get_page_info.js — Extract comprehensive page metadata.

(function() {
  'use strict';
  try {
    // Title and URL.
    var info = {
      title: document.title || '',
      url: window.location.href,
      language: document.documentElement.lang || '',
    };

    // Meta tags.
    var metas = document.querySelectorAll('meta');
    var metaData = {};
    for (var i = 0; i < metas.length; i++) {
      var m = metas[i];
      var name = m.getAttribute('name') || m.getAttribute('property') || '';
      var content = m.getAttribute('content') || '';
      if (name && content) {
        metaData[name.toLowerCase()] = content;
      }
    }
    info.description = metaData['description'] || metaData['og:description'] || '';
    info.keywords = metaData['keywords'] || '';
    info.ogTitle = metaData['og:title'] || '';
    info.ogImage = metaData['og:image'] || '';

    // Forms.
    var forms = [];
    var formEls = document.querySelectorAll('form');
    for (var j = 0; j < formEls.length && j < 20; j++) {
      var form = formEls[j];
      var fields = [];
      var inputs = form.querySelectorAll('input, select, textarea');
      for (var k = 0; k < inputs.length && k < 50; k++) {
        var inp = inputs[k];
        fields.push({
          tag: inp.tagName.toLowerCase(),
          type: inp.type || '',
          name: inp.name || '',
          id: inp.id || '',
          placeholder: inp.placeholder || ''
        });
      }
      forms.push({
        action: form.action || '',
        method: form.method || 'get',
        id: form.id || '',
        fields: fields
      });
    }
    info.forms = forms;

    // Images.
    var images = [];
    var imgEls = document.querySelectorAll('img[src]');
    for (var l = 0; l < imgEls.length && l < 50; l++) {
      var img = imgEls[l];
      images.push({
        src: img.src,
        alt: img.alt || '',
        width: img.naturalWidth || img.width || 0,
        height: img.naturalHeight || img.height || 0
      });
    }
    info.images = images;

    // Headings structure.
    var headings = [];
    var hEls = document.querySelectorAll('h1, h2, h3');
    for (var m2 = 0; m2 < hEls.length && m2 < 30; m2++) {
      headings.push({
        level: parseInt(hEls[m2].tagName.charAt(1)),
        text: (hEls[m2].innerText || '').trim().substring(0, 200)
      });
    }
    info.headings = headings;

    // Page dimensions.
    info.pageWidth = document.documentElement.scrollWidth;
    info.pageHeight = document.documentElement.scrollHeight;
    info.viewportWidth = window.innerWidth;
    info.viewportHeight = window.innerHeight;

    return JSON.stringify(info);
  } catch (e) {
    return JSON.stringify({error: e.message});
  }
})();
