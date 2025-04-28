//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { test, expect } from "@playwright/test";

const BASE_URL = process.env.CI
  ? "http://127.0.0.1:49483"
  : "http://127.0.0.1:4173";

console.log(`Using base URL: ${BASE_URL}`);

test.describe("first load", () => {
  test.beforeEach(async ({ page }) => {
    // Go to the starting url before each test.

    await page.goto(BASE_URL, {
      timeout: 30000,
      waitUntil: "load",
    });
  });

  test("should load the homepage and display the correct content", async ({
    page,
  }) => {
    // Check if the page title is correct.
    await expect(page).toHaveTitle(/TEN Manager/);
  });

  test("header is visible", async ({ page }) => {
    // Check if the header is visible.
    await expect(page.locator("header").first()).toBeVisible();
  });

  test("react-flow is visible", async ({ page }) => {
    // Check if the react-flow is visible.
    await expect(page.locator(".react-flow").first()).toBeVisible();
  });
});
