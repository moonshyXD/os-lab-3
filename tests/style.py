import ast
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def c_functions(path):
    text = path.read_text(encoding="utf-8")
    clean = re.sub(r'"(?:\\.|[^"\\])*"|\x27(?:\\.|[^\x27\\])*\x27',
                   lambda match: re.sub(r"[^\n]", " ", match[0]), text)
    assert not re.search(r"\bgoto\b", clean), f"goto in {path}"
    pattern = r"(?m)^[A-Za-z_][\w *]*\b(\w+)\s*\([^;{}]*\)\s*\{"
    for match in re.finditer(pattern, clean):
        depth, end = 1, match.end()
        while depth:
            depth += (clean[end] == "{") - (clean[end] == "}")
            end += 1
        yield match[1], clean.count("\n", match.start(), end) + 1


def main():
    functions = []
    for path in sorted(ROOT.glob("src/*.c")) + sorted(ROOT.glob("tests/*.c")):
        for name, length in c_functions(path):
            assert length <= 40, (path, name, length)
            if name == "main":
                assert length <= 20, (path, name, length)
            functions.append((path.name, name, length))
    for path in ROOT.glob("tests/*.py"):
        tree = ast.parse(path.read_text(encoding="utf-8"))
        for node in ast.walk(tree):
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                length = node.end_lineno - node.lineno + 1
                assert length <= 40, (path, node.name, length)
    assert functions, "No C functions detected"
    for path, name, length in functions:
        print(f"{path}: {name}: {length} lines")
    print("PASS: no goto; C/Python functions <=40 lines; main <=20 lines")


if __name__ == "__main__":
    main()
