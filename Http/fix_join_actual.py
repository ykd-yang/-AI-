from pathlib import Path
path = Path("HttpServer.py")
text = path.read_text(encoding="utf-8")
text = text.replace('    return "\\n".join(', '    return "\n".join(')
path.write_text(text, encoding="utf-8")
