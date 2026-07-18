from playwright.sync_api import sync_playwright
import os

def test_setup_page():
    with sync_playwright() as p:
        browser = p.chromium.launch()
        page = browser.new_page()

        # Open the setup page
        base_dir = os.path.dirname(os.path.abspath(__file__))
        file_url = f"file://{base_dir}/main/web_assets/setup.html"
        page.goto(file_url)

        # Verify initial state
        password_input = page.locator("#password")
        assert password_input.get_attribute("type") == "password"

        show_btn = page.locator("button[aria-label='Show password']")
        assert show_btn.text_content() == "Show"
        assert show_btn.get_attribute("aria-pressed") == "false"

        # Click show button
        show_btn.click()

        # Verify unmasked state
        assert password_input.get_attribute("type") == "text"
        assert show_btn.text_content() == "Hide"
        assert show_btn.get_attribute("aria-pressed") == "true"

        # Click hide button
        show_btn.click()

        # Verify masked state
        assert password_input.get_attribute("type") == "password"
        assert show_btn.text_content() == "Show"
        assert show_btn.get_attribute("aria-pressed") == "false"

        print("Setup page UX verified successfully.")

        browser.close()

if __name__ == "__main__":
    test_setup_page()
