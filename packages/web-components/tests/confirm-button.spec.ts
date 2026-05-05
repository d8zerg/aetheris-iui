import { test, expect } from '@playwright/test';

const PAGE = 'http://localhost:7373/tests/fixtures/confirm-button.html';

async function shadowBtn(page: ReturnType<typeof test.extend>, selector: string) {
  return page.locator(selector).locator('css=button');
}

test.describe('aui-confirm-button', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto(PAGE);
  });

  // ── single mode ──────────────────────────────────────────────────────────
  test('single: renders Confirm button', async ({ page }) => {
    const btn = page.locator('#single').locator('css=button');
    await expect(btn).toBeVisible();
    await expect(btn).toHaveText('Confirm');
    await expect(btn).toBeEnabled();
  });

  test('single: fires aui:confirm on click', async ({ page }) => {
    await page.locator('#single').locator('css=button').click();
    const events = await page.evaluate(() => (window as any).__events['single']);
    expect(events).toContain('confirm');
  });

  // ── typed mode ────────────────────────────────────────────────────────────
  test('typed: Confirm button disabled until phrase typed', async ({ page }) => {
    const host = page.locator('#typed');
    const btn = host.locator('css=button');
    await expect(btn).toBeDisabled();
  });

  test('typed: Confirm button enables after correct phrase', async ({ page }) => {
    const host = page.locator('#typed');
    const input = host.locator('css=input');
    const btn = host.locator('css=button');

    await input.fill('DELETE');
    await expect(btn).toBeEnabled();
  });

  test('typed: partial phrase keeps button disabled', async ({ page }) => {
    const host = page.locator('#typed');
    const input = host.locator('css=input');
    const btn = host.locator('css=button');

    await input.fill('DELET');
    await expect(btn).toBeDisabled();
  });

  test('typed: fires aui:confirm after correct phrase and click', async ({ page }) => {
    const host = page.locator('#typed');
    await host.locator('css=input').fill('DELETE');
    await host.locator('css=button').click();
    const events = await page.evaluate(() => (window as any).__events['typed']);
    expect(events).toContain('confirm');
  });

  // ── multi_party mode ──────────────────────────────────────────────────────
  test('multi_party: renders Send for approval button', async ({ page }) => {
    const btn = page.locator('#multi').locator('css=button');
    await expect(btn).toHaveText('Send for approval');
  });

  test('multi_party: fires aui:send-for-approval and shows pending state', async ({ page }) => {
    const host = page.locator('#multi');
    await host.locator('css=button').click();

    const events = await page.evaluate(() => (window as any).__events['multi']);
    expect(events).toContain('send-for-approval');

    const pendingText = host.locator('css=span.pending');
    await expect(pendingText).toBeVisible();
    await expect(pendingText).toContainText('Awaiting co-approval');
  });

  // ── cooling_off mode ──────────────────────────────────────────────────────
  test('cooling_off: button disabled during countdown', async ({ page }) => {
    const btn = page.locator('#cooling').locator('css=button');
    await expect(btn).toBeDisabled();
    await expect(btn).toContainText(/Confirm \(\d+s\)/);
  });

  test('cooling_off: button enables after countdown and fires aui:confirm', async ({ page }) => {
    // cooling-seconds="3" so wait 4s
    const btn = page.locator('#cooling').locator('css=button');
    await expect(btn).toBeEnabled({ timeout: 5000 });
    await btn.click();
    const events = await page.evaluate(() => (window as any).__events['cooling']);
    expect(events).toContain('confirm');
  });

  // ── automatic mode ────────────────────────────────────────────────────────
  test('automatic: fires aui:confirm without user interaction', async ({ page }) => {
    // Give microtask time to resolve
    await page.waitForTimeout(100);
    const events = await page.evaluate(() => (window as any).__events['auto']);
    expect(events).toContain('confirm');
  });

  // ── disabled ──────────────────────────────────────────────────────────────
  test('disabled: button is not clickable', async ({ page }) => {
    const btn = page.locator('#disabled').locator('css=button');
    await expect(btn).toBeDisabled();
  });
});
