import asyncio, sys
from playwright.async_api import async_playwright

URL=sys.argv[1] if len(sys.argv)>1 else "http://epaper42.local/"

async def main():
    async with async_playwright() as p:
        b=await p.firefox.launch(headless=True)
        page=await b.new_page(viewport={"width":1200,"height":1100})
        errs=[]
        page.on("pageerror", lambda e: errs.append("pageerror: "+str(e)))
        page.on("console", lambda m: errs.append("console: "+m.text) if m.type=="error" else None)
        await page.goto(URL, wait_until="load")
        await page.wait_for_function("document.getElementById('uiState').innerText.includes('当前')", timeout=15000)
        print("state:", await page.inner_text("#uiState"))
        await page.screenshot(path="/tmp/remote_home.png", full_page=True)

        await page.click("[data-key=down]")
        await page.wait_for_function("document.getElementById('uiState').innerText.includes('选中 2')", timeout=10000)
        print("down:", await page.inner_text("#uiState"))

        await page.click("[data-app=clock]")
        await page.wait_for_function("document.getElementById('uiState').innerText.includes('时钟')", timeout=10000)
        await page.wait_for_timeout(2200)
        print("clock:", await page.inner_text("#uiState"))
        await page.screenshot(path="/tmp/remote_clock.png", full_page=True)

        await page.fill("#noteText", "网页遥控写入的记事\n第二行：ESP32 本地渲染中文。\n第三行：端到端测试通过。")
        await page.click("#btnNote")
        await page.wait_for_function("document.getElementById('uiState').innerText.includes('记事')", timeout=10000)
        print("notes:", await page.inner_text("#uiState"))
        await page.screenshot(path="/tmp/remote_notes.png", full_page=True)

        await page.click("[data-app=home]")
        await page.wait_for_function("document.getElementById('uiState').innerText.includes('主菜单')", timeout=10000)

        async with page.expect_file_chooser(timeout=5000) as fc:
            await page.click("#drop")
        ch=await fc.value
        await ch.set_files("docs/images/demo.png")
        await page.wait_for_function("document.getElementById('log').innerText.includes('已载入')", timeout=10000)
        await page.select_option("#fullMode","fast")
        await page.click("#btnFull")
        await page.wait_for_function("document.getElementById('log').innerText.includes('全屏完成')", timeout=20000)
        await page.wait_for_function("document.getElementById('uiState').innerText.includes('图片')", timeout=10000)
        print("image:", await page.inner_text("#uiState"))
        await page.screenshot(path="/tmp/remote_image.png", full_page=True)

        await page.click("[data-key=back]")
        await page.wait_for_function("document.getElementById('uiState').innerText.includes('主菜单')", timeout=10000)
        print("back:", await page.inner_text("#uiState"))
        print("errors:", errs)
        await b.close()

asyncio.run(main())
