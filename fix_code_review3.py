import re

with open('main/main.c', 'r', encoding='utf-8') as f:
    content = f.read()

# Fix the extra bracket
content = content.replace("""        }
    }

    }

    if (g_transfer_progress.active) {""", """        }
    }

    if (g_transfer_progress.active) {""")

with open('main/main.c', 'w', encoding='utf-8') as f:
    f.write(content)
