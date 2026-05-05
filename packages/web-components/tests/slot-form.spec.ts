import { test, expect } from '@playwright/test';

const PAGE = 'http://localhost:7373/tests/fixtures/slot-form.html';

const SLOTS_JSON = JSON.stringify([
  { name: 'camera_id', required: true,  value_json: null },
  { name: 'resolution', required: false, value_json: '"1080p"' },
]);

test.describe('aui-slot-form', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto(PAGE);
    await page.evaluate((json) => {
      document.getElementById('form')!.setAttribute('slots-json', json);
    }, SLOTS_JSON);
  });

  test('renders required and optional slots', async ({ page }) => {
    const host = page.locator('#form');
    await expect(host.locator('css=input[data-slot="camera_id"]')).toBeVisible();
    await expect(host.locator('css=input[data-slot="resolution"]')).toBeVisible();
  });

  test('required slot has asterisk label', async ({ page }) => {
    const label = page.locator('#form').locator('css=.slot').first().locator('css=label');
    await expect(label).toContainText('*');
  });

  test('submit disabled while required slot empty', async ({ page }) => {
    const btn = page.locator('#form').locator('css=button[type="submit"]');
    await expect(btn).toBeDisabled();
  });

  test('submit enabled after required slot filled', async ({ page }) => {
    const host = page.locator('#form');
    await host.locator('css=input[data-slot="camera_id"]').fill('cam-01');
    await host.locator('css=input[data-slot="camera_id"]').dispatchEvent('change');

    const btn = host.locator('css=button[type="submit"]');
    await expect(btn).toBeEnabled();
  });

  test('fires aui:slot-change with correct detail on input change', async ({ page }) => {
    const host = page.locator('#form');
    await host.locator('css=input[data-slot="camera_id"]').fill('cam-99');
    await host.locator('css=input[data-slot="camera_id"]').dispatchEvent('change');

    const changes = await page.evaluate(() => (window as any).__events.slotChanges);
    expect(changes.length).toBeGreaterThan(0);
    expect(changes[changes.length - 1]).toMatchObject({
      slot_name: 'camera_id',
      value_json: '"cam-99"',
    });
  });

  test('fires aui:submit when form submitted', async ({ page }) => {
    const host = page.locator('#form');
    await host.locator('css=input[data-slot="camera_id"]').fill('cam-01');
    await host.locator('css=input[data-slot="camera_id"]').dispatchEvent('change');
    await host.locator('css=button[type="submit"]').click();

    const submits = await page.evaluate(() => (window as any).__events.submits);
    expect(submits).toBe(1);
  });

  test('pre-filled slot shows filled value', async ({ page }) => {
    const input = page.locator('#form').locator('css=input[data-slot="resolution"]');
    await expect(input).toHaveValue('1080p');
  });

  test('disabled attribute disables all inputs', async ({ page }) => {
    await page.evaluate(() => {
      document.getElementById('form')!.setAttribute('disabled', '');
    });
    const inputs = page.locator('#form').locator('css=input');
    for (const input of await inputs.all()) {
      await expect(input).toBeDisabled();
    }
  });
});
