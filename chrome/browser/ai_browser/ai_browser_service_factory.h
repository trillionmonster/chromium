// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_BROWSER_AI_BROWSER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_AI_BROWSER_AI_BROWSER_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace ai_browser {

class AIBrowserService;

// Factory for creating AIBrowserService instances per profile.
// Usage:
//   auto* service = AIBrowserServiceFactory::GetForProfile(profile);
class AIBrowserServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static AIBrowserService* GetForProfile(content::BrowserContext* context);
  static AIBrowserServiceFactory* GetInstance();

  AIBrowserServiceFactory(const AIBrowserServiceFactory&) = delete;
  AIBrowserServiceFactory& operator=(const AIBrowserServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<AIBrowserServiceFactory>;

  AIBrowserServiceFactory();
  ~AIBrowserServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ai_browser

#endif  // CHROME_BROWSER_AI_BROWSER_AI_BROWSER_SERVICE_FACTORY_H_
