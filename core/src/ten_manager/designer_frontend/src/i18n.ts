//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import i18n from "i18next";
import { initReactI18next } from "react-i18next";
import HttpBackend from "i18next-http-backend";
import LanguageDetector from "i18next-browser-languagedetector";

import { EPreferencesLocale } from "@/types/apps";

i18n
  // Use backend plugin to load translation files.
  .use(HttpBackend)
  // Detect user language.
  .use(LanguageDetector)
  // Pass the i18n instance to react-i18next.
  .use(initReactI18next)
  .init({
    fallbackLng: EPreferencesLocale.EN_US, // Default language.
    debug: true, // Enable debug mode, can be turned off in production.

    ns: ["common"], // Define namespace.
    defaultNS: "common", // Default namespace.

    interpolation: {
      escapeValue: false, // React already handles XSS safely.
    },

    backend: {
      // Path to load translation files.
      loadPath: "/locales/{{lng}}/{{ns}}.json",
    },
  });

export default i18n;
