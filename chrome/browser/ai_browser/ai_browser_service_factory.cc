// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/ai_browser_service_factory.h"

#include "chrome/browser/ai_browser/ai_browser_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace ai_browser {

// static
AIBrowserService* AIBrowserServiceFactory::GetForProfile(
    content::BrowserContext* context) {
  return static_cast<AIBrowserService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
AIBrowserServiceFactory* AIBrowserServiceFactory::GetInstance() {
  static base::NoDestructor<AIBrowserServiceFactory> instance;
  return instance.get();
}

AIBrowserServiceFactory::AIBrowserServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "AIBrowserService",
          BrowserContextDependencyManager::GetInstance()) {}

AIBrowserServiceFactory::~AIBrowserServiceFactory() = default;

std::unique_ptr<KeyedService>
AIBrowserServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<AIBrowserService>(profile);
}

}  // namespace ai_browser
