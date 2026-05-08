import { test, expect } from '@playwright/test';

// 매 실행마다 새 사용자를 만들어 격리. seed 데이터에 의존하지 않음.
const stamp = () => Date.now().toString(36);

test('회원가입 → 로그인 상태 → 피드 페이지에 헤더 노출', async ({ page }) => {
  const id = stamp();
  const email = `e2e-${id}@monggle.local`;
  const pw    = `e2epw-${id}!`;
  const name  = `E2E ${id}`;

  await page.goto('/signup');

  await page.getByLabel(/email|이메일/i).fill(email);
  await page.getByLabel(/password|비밀번호/i).fill(pw);
  await page.getByLabel(/이름|name|displayname/i).fill(name);
  await page.getByRole('button', { name: /가입|sign\s*up/i }).click();

  await expect(page).toHaveURL(/\/feed|\/$/, { timeout: 10_000 });
  // 헤더에 표시 이름 또는 로그아웃 버튼이 보여야 = 로그인됨
  await expect(page.getByRole('button', { name: /로그아웃|logout/i })).toBeVisible();
});

test('로그인 후 새 글 작성 → 피드에 즉시 노출', async ({ page, request }) => {
  // 백엔드 직접 호출로 사용자 + 토큰 미리 만들고, 로컬스토리지에 주입해서 빠르게 진입.
  const id = stamp();
  const r = await request.post('http://127.0.0.1:8080/auth/signup', {
    data: { email: `e2e-${id}@monggle.local`, password: `pw-${id}!`, display_name: `E2E ${id}` },
  });
  expect(r.ok()).toBeTruthy();
  const tokens = await r.json() as {
    access_token: string; refresh_token: string;
    access_expires_at: number; refresh_expires_at: number; user_id: number;
  };

  await page.addInitScript((t) => {
    localStorage.setItem('monggle.auth', JSON.stringify({
      accessToken:  t.access_token,
      refreshToken: t.refresh_token,
      accessExpiresAt:  t.access_expires_at,
      refreshExpiresAt: t.refresh_expires_at,
      userId:       t.user_id,
    }));
  }, tokens);

  await page.goto('/feed');

  // 컴포저 열고 본문 입력
  await page.getByRole('button', { name: /^새\s*글$/ }).click();
  const body = `E2E 첫 글 ${id}`;
  await page.getByPlaceholder(/오늘 무엇을 했나요/).fill(body);
  await page.getByRole('button', { name: /^발행/ }).click();

  // 발행 후 피드에 본문이 노출되어야 함
  await expect(page.getByText(body)).toBeVisible({ timeout: 10_000 });
});

test('/healthz 와 /readyz 가 200 으로 응답', async ({ request }) => {
  const h = await request.get('http://127.0.0.1:8080/healthz');
  expect(h.ok()).toBeTruthy();
  const r = await request.get('http://127.0.0.1:8080/readyz');
  expect(r.ok()).toBeTruthy();
  const body = await r.json();
  expect(body.db).toBe('ok');
});
