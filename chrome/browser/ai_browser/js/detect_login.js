// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// detect_login.js — Injected into pages to detect login forms.
// Returns a JSON string describing any login forms found.

(function() {
  'use strict';

  var results = {hasLoginForm: false, forms: []};
  var forms = document.querySelectorAll('form');

  for (var i = 0; i < forms.length; i++) {
    var form = forms[i];
    var pwdFields = form.querySelectorAll('input[type="password"]');
    if (pwdFields.length === 0) continue;

    var pwdField = pwdFields[0];
    var pwdSelector = '';
    if (pwdField.id) {
      pwdSelector = '#' + CSS.escape(pwdField.id);
    } else if (pwdField.name) {
      pwdSelector = 'input[name="' + CSS.escape(pwdField.name) + '"]';
    } else {
      pwdSelector = 'input[type="password"]';
    }

    // Find associated username/email field before the password field.
    var textInputs = form.querySelectorAll(
        'input[type="text"], input[type="email"], input:not([type])');
    var userField = null;
    for (var j = 0; j < textInputs.length; j++) {
      var inp = textInputs[j];
      // Check that the text input comes before the password field in DOM order.
      if (inp.compareDocumentPosition(pwdField) &
          Node.DOCUMENT_POSITION_FOLLOWING) {
        userField = inp;
      }
    }

    var userSelector = '';
    if (userField) {
      if (userField.id) {
        userSelector = '#' + CSS.escape(userField.id);
      } else if (userField.name) {
        userSelector = 'input[name="' + CSS.escape(userField.name) + '"]';
      } else {
        userSelector = 'input[type="text"], input[type="email"]';
      }
    }

    // Find the submit button.
    var submitBtn = form.querySelector(
        'button[type="submit"], input[type="submit"]');
    if (!submitBtn) {
      submitBtn = form.querySelector('button:not([type="button"])');
    }

    var submitSelector = '';
    if (submitBtn) {
      if (submitBtn.id) {
        submitSelector = '#' + CSS.escape(submitBtn.id);
      } else if (submitBtn.name) {
        submitSelector = '[name="' + CSS.escape(submitBtn.name) + '"]';
      } else {
        submitSelector = 'button[type="submit"], input[type="submit"]';
      }
    }

    // Check for CAPTCHA presence.
    var hasCaptcha = !!(
      form.querySelector('[class*="captcha" i]') ||
      form.querySelector('[id*="captcha" i]') ||
      form.querySelector('iframe[src*="captcha"]') ||
      form.querySelector('iframe[src*="recaptcha"]') ||
      form.querySelector('[class*="recaptcha"]') ||
      form.querySelector('[class*="hcaptcha"]')
    );

    results.hasLoginForm = true;
    results.forms.push({
      usernameSelector: userSelector,
      passwordSelector: pwdSelector,
      submitSelector: submitSelector,
      formAction: form.action || '',
      hasCaptcha: hasCaptcha
    });
  }

  // Check for standalone password fields not inside a <form>.
  if (!results.hasLoginForm) {
    var standalonePasswords = document.querySelectorAll(
        'input[type="password"]');
    if (standalonePasswords.length > 0) {
      results.hasLoginForm = true;
      results.forms.push({
        usernameSelector: '',
        passwordSelector: 'input[type="password"]',
        submitSelector: '',
        formAction: '',
        hasCaptcha: false
      });
    }
  }

  return JSON.stringify(results);
})();
