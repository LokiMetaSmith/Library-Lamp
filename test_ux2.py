def test_ux2():
    with open('main/web_assets/ereader.html', 'r') as f:
        content = f.read()

    assert "main library" in content, "Missing CTA in ereader.html"
    assert "aria-label" in content and "readLink" in content, "Missing aria-label in ereader.html"

    print("All assertions passed. Modifications are successfully present in ereader.html.")

if __name__ == "__main__":
    test_ux2()
