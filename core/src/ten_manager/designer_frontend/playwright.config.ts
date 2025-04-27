//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
/* eslint-disable max-len */
import { defineConfig, devices } from "@playwright/test";

/**
 * Read environment variables from file.
 * https://github.com/motdotla/dotenv
 */
// import dotenv from 'dotenv';
// import path from 'path';
// dotenv.config({ path: path.resolve(__dirname, '.env') });

/**
 * Playwright Configuration
 *
 * This file configures the Playwright test runner for end-to-end testing.
 * See https://playwright.dev/docs/test-configuration for complete documentation.
 */
export default defineConfig({
  // Directory where test files are located.
  testDir: "./tests/e2e",

  // Run tests in files in parallel for faster execution.
  fullyParallel: true,

  // Prevents tests marked with test.only from being the only ones run in CI environments.
  forbidOnly: !!process.env.CI,

  // Retry failed tests on CI environments to handle flaky tests.
  retries: process.env.CI ? 2 : 1,

  // Limit parallel test execution on CI to prevent resource contention.
  workers: process.env.CI ? 2 : undefined,

  // Set global timeout for tests.
  // timeout: 60000, // 60 seconds for entire test.

  // HTML reporter provides detailed test results with screenshots and traces.
  reporter: "html",

  // Set global timeout for expectations/assertions.
  expect: {
    timeout: 30000, // 30 seconds
  },

  // Global settings applied to all test projects.
  use: {
    // Base URL to use in actions like `await page.goto('/')`.
    // baseURL: 'http://127.0.0.1:3000',

    // Take screenshots on test failures.
    screenshot: "only-on-failure",

    // Record video for failed tests.
    // video: "on-first-retry",

    // Trace collection settings - collect traces on failure for better
    // debugging.
    trace: {
      mode: "on-first-retry",
      snapshots: true,
      screenshots: true,
      sources: true,
      attachments: true,
    },
  },

  // Browser configurations for running tests.
  projects: [
    {
      // Chrome/Chromium is the primary testing browser.
      name: "chromium",
      use: { ...devices["Desktop Chrome"] },
    },

    // Firefox configuration - currently disabled.
    // {
    //   name: "firefox",
    //   use: { ...devices["Desktop Firefox"] },
    // },

    {
      // Safari/WebKit for testing Apple browser compatibility.
      name: "webkit",
      use: { ...devices["Desktop Safari"] },
    },

    // Mobile viewport testing configurations - currently disabled.
    // {
    //   name: 'Mobile Chrome',
    //   use: { ...devices['Pixel 5'] },
    // },
    // {
    //   name: 'Mobile Safari',
    //   use: { ...devices['iPhone 12'] },
    // },

    // Branded browser testing - uncomment when specific browser testing is needed.
    // {
    //   name: 'Microsoft Edge',
    //   use: { ...devices['Desktop Edge'], channel: 'msedge' },
    // },
    // {
    //   name: 'Google Chrome',
    //   use: { ...devices['Desktop Chrome'], channel: 'chrome' },
    // },
  ],

  // Local development server configuration - uncomment when needed.
  // webServer: {
  //   command: "npm run preview",
  //   url: "http://127.0.0.1:4173",
  //   reuseExistingServer: !process.env.CI,
  // },
});
