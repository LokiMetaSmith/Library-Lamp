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

    print("All assertions passed. Modifications are successfully present.")

if __name__ == "__main__":
    test_ux()
