import os

def test_ux2():
    base_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

    with open(os.path.join(base_dir, 'main/web_assets/ereader.html'), 'r') as f:
        content = f.read()

    assert "main library" in content, "Missing CTA in ereader.html"
    assert "aria-label" in content and "readLink" in content, "Missing aria-label in ereader.html"

    print("All assertions passed. Modifications are successfully present in ereader.html.")

if __name__ == "__main__":
    test_ux2()
