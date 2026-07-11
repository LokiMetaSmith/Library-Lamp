import re
import os

def test_ux():
    base_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

    with open(os.path.join(base_dir, 'main/web_assets/index.html'), 'r') as f:
        content = f.read()

    assert "Upload a book above to get started!" in content, "Missing CTA in local empty state"
    assert "Transfer a book from your local library to start reading!" in content, "Missing CTA in ereader empty state"
    assert "Upload in progress..." in content, "Missing title tooltip"
    assert "Connect an E-Reader to transfer books" in content, "Missing title tooltip"

    assert ":aria-label=\"'Transfer ' + file.title + ' to E-Reader'\"" in content, "Missing Transfer aria-label"
    assert ":aria-label=\"'Transfer ' + file.title + ' to Library'\"" in content, "Missing Transfer to Library aria-label"
    assert ":aria-label=\"'Read ' + file.title\"" in content, "Missing Read aria-label"
    assert ":aria-label=\"'Delete ' + file.title\"" in content, "Missing Delete aria-label"
    assert ":aria-label=\"'Cancel transfer for ' + file.title\"" in content, "Missing Cancel transfer aria-label"

    with open(os.path.join(base_dir, 'main/web_assets/audio.html'), 'r') as f:
        audio_content = f.read()

    assert ":aria-label=\"'Remove ' + track + ' from queue'\"" in audio_content, "Missing Remove from queue aria-label"
    assert ":aria-label=\"'Add ' + (file.title || file.name) + ' to queue'\"" in audio_content, "Missing Add to queue aria-label"
    assert ":aria-label=\"'Skip ' + currentTrack\"" in audio_content, "Missing Skip Track aria-label"

    with open(os.path.join(base_dir, 'main/web_assets/style.css'), 'r') as f:
        css_content = f.read()

    assert ":focus-visible" in css_content, "Missing focus-visible styles in style.css"

    with open(os.path.join(base_dir, 'main/web_assets/viewer.html'), 'r') as f:
        viewer_content = f.read()

    assert "title=\"Loading book...\"" in viewer_content, "Missing disabled state tooltip in viewer.html"
    assert "aria-label=\"Previous Page\"" in viewer_content, "Missing Previous Page aria-label in viewer.html"
    assert "aria-label=\"Next Page\"" in viewer_content, "Missing Next Page aria-label in viewer.html"
    assert "title = \"Previous Page (Left Arrow)\"" in viewer_content, "Missing keyboard hint tooltip in viewer.html"

    with open(os.path.join(base_dir, 'main/web_assets/admin.html'), 'r') as f:
        admin_content = f.read()

    assert "onclick=\"confirmFormatSD(this)\"" in admin_content, "Missing 'this' parameter in confirmFormatSD call"
    assert "btn.disabled = true" in admin_content, "Missing disabled state in confirmFormatSD"
    assert "Formatting..." in admin_content, "Missing loading text in confirmFormatSD"
    assert ".finally" in admin_content, "Missing finally block in confirmFormatSD"

    print("All assertions passed. Modifications are successfully present.")

if __name__ == "__main__":
    test_ux()
