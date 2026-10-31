"""Engine-free PR checks. Run from any directory with Python 3.12+."""

import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
from urllib.parse import unquote, urlsplit

import yaml


ROOT = Path(__file__).resolve().parents[2]
TEXT_SUFFIXES = {
    ".md", ".py", ".ps1", ".yml", ".yaml", ".ini", ".cs", ".cpp",
    ".h", ".hpp", ".uproject", ".uplugin", ".json", ".lua", ".txt",
}
CHECK_ROOTS = {".github", "Build", "Config", "Docs", "Source"}
CONFLICT = re.compile(r"^(?:<{7}|={7}|>{7})(?:\s.*)?$", re.MULTILINE)
LINK = re.compile(r"!?\[[^\]]*\]\((<[^>]+>|[^\s)]+)(?:\s+\"[^\"]*\")?\)")


def validate_file(path: Path, root: Path) -> list[str]:
    """Validate project-owned text without running project or game code."""
    label = path.relative_to(root).as_posix()
    try:
        text = path.read_text(encoding="utf-8-sig")
    except (UnicodeError, OSError) as error:
        return [f"{label}: cannot read UTF-8 text: {error}"]

    errors = []
    if CONFLICT.search(text):
        errors.append(f"{label}: unresolved merge conflict marker")

    try:
        if path.suffix in {".json", ".uproject", ".uplugin"}:
            value = json.loads(text)
            if path.suffix == ".uproject" and not isinstance(value, dict):
                errors.append(f"{label}: project descriptor must be an object")
        elif path.suffix in {".yaml", ".yml"}:
            list(yaml.safe_load_all(text))
    except (ValueError, yaml.YAMLError) as error:
        errors.append(f"{label}: invalid JSON/YAML: {error}")

    if path.suffix == ".md":
        fence = None
        prose = []
        for line in text.splitlines():
            match = re.match(r"^\s{0,3}(`{3,}|~{3,})(.*)$", line)
            if match:
                marker, tail = match.groups()
                if fence is None:
                    fence = marker
                elif marker[0] == fence[0] and len(marker) >= len(fence) and not tail.strip():
                    fence = None
                continue
            if fence is None:
                prose.append(line)
        if fence is not None:
            errors.append(f"{label}: unclosed fenced code block")
        for match in LINK.finditer("\n".join(prose)):
            target = match.group(1).strip("<>")
            parts = urlsplit(target)
            if parts.scheme or parts.netloc or not parts.path:
                continue
            destination = (path.parent / unquote(parts.path)).resolve()
            if not destination.is_relative_to(root.resolve()):
                errors.append(f"{label}: local link leaves repository: {target}")
            elif not destination.exists():
                errors.append(f"{label}: broken local link: {target}")
    return errors


def main() -> int:
    result = subprocess.run(
        ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
        cwd=ROOT, check=True, capture_output=True,
    )
    paths = sorted(set(result.stdout.decode("utf-8").split("\0")) - {""})
    errors = []
    checked = 0
    lua_files = []
    for relative in paths:
        path = ROOT / relative
        owned = path.parts[len(ROOT.parts)] in CHECK_ROOTS or "/" not in relative
        is_lua = relative.startswith("Content/Script/") and path.suffix == ".lua"
        if path.is_file() and path.suffix in TEXT_SUFFIXES and (owned or is_lua):
            checked += 1
            errors.extend(validate_file(path, ROOT))
            if is_lua:
                lua_files.append(path)

    if lua_files:
        settings = ROOT / "Config/DefaultUnLuaEditor.ini"
        if settings.exists():
            version = re.search(r"^\s*LuaVersion\s*=\s*\"?([^\"\r\n]+)", settings.read_text(encoding="utf-8-sig"), re.MULTILINE)
            if version and not version.group(1).strip().startswith("lua-5.4."):
                errors.append("UnLua LuaVersion differs from CI Lua 5.4; update both together.")
        compiler = shutil.which("luac5.4") or shutil.which("luac")
        if not compiler:
            errors.append("Lua files exist but luac5.4 is not installed.")
        else:
            version = subprocess.run([compiler, "-v"], capture_output=True, text=True)
            if version.returncode or "Lua 5.4" not in version.stdout + version.stderr:
                errors.append("Lua syntax checker must use Lua 5.4.")
            else:
                for path in lua_files:
                    syntax = subprocess.run([compiler, "-p", str(path)], capture_output=True, text=True)
                    if syntax.returncode:
                        errors.append(f"{path.relative_to(ROOT)}: {syntax.stderr.strip()}")
    else:
        print("Lua syntax: skipped (no project scripts in Content/Script yet).")

    for error in errors:
        print(f"ERROR: {error}", file=sys.stderr)
    print(f"Checked {checked} project text files and {len(lua_files)} Lua scripts; {len(errors)} error(s).")
    print("These checks do not compile UE C++, cook assets, or validate multiplayer/rendering.")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
