import re

def test_ux():
    with open('main/web_assets/index.html', 'r') as f:
        content = f.read()

    assert "Upload a book above to get started!" in content, "Missing CTA in local empty state"
    assert "Transfer a book from your local library to start reading!" in content, "Missing CTA in ereader empty state"
    assert "Upload in progress..." in content, "Missing title tooltip"
    assert "Connect an E-Reader to transfer books" in content, "Missing title tooltip"

    print("All assertions passed. Modifications are successfully present in main/web_assets/index.html.")

if __name__ == "__main__":
    test_ux()
