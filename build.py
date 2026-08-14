import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent
CORE = ROOT / "Source" / "FloorPlanUnreal" / "FloorPlanCore"
TESTS = ROOT / "Tests"
BUILD = ROOT / "build"
PROBE = BUILD / "probe"

STANDARD = "c++17"
WARNINGS = [
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-Wconversion",
    "-Wshadow",
    "-Wold-style-cast",
    "-Wnon-virtual-dtor",
    "-Werror",
]
SANITIZE = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]

WINGET_PACKAGES = Path(os.environ.get("LOCALAPPDATA", "")) / "Microsoft" / "WinGet" / "Packages"


def candidate_compilers():
    found = []
    for name in ("g++", "clang++"):
        resolved = shutil.which(name)
        if resolved:
            found.append(resolved)
    if WINGET_PACKAGES.is_dir():
        for pattern in ("**/mingw64/bin/g++.exe", "**/mingw32/bin/g++.exe"):
            found.extend(str(p) for p in WINGET_PACKAGES.glob(pattern))
    for fixed in (r"C:\mingw64\bin\g++.exe", r"C:\Program Files\LLVM\bin\clang++.exe"):
        if Path(fixed).exists():
            found.append(fixed)
    return found


PROBE_SOURCE = """#include <cstdint>
#include <string>
#include <vector>
int main() {
    std::vector<std::string> v{"a"};
    return static_cast<int>(v[0].size()) - 1;
}
"""


def environment(compiler):
    env = dict(os.environ)
    env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
    return env


def works(compiler, extra):
    PROBE.mkdir(parents=True, exist_ok=True)
    source = PROBE / "probe.cpp"
    source.write_text(PROBE_SOURCE, encoding="utf-8")
    binary = PROBE / ("probe" + (".exe" if os.name == "nt" else ""))
    command = [compiler, "-std=" + STANDARD, *extra, str(source), "-o", str(binary)]
    env = environment(compiler)
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=300, env=env)
    except (OSError, subprocess.TimeoutExpired):
        return False
    if result.returncode != 0:
        return False
    try:
        return subprocess.run(
            [str(binary)], capture_output=True, timeout=120, env=env
        ).returncode == 0
    except (OSError, subprocess.TimeoutExpired):
        return False


def select_toolchain():
    for compiler in candidate_compilers():
        if not works(compiler, []):
            continue
        sanitize = works(compiler, SANITIZE)
        return compiler, sanitize
    return None, False


def sources():
    core = [p for p in sorted(CORE.rglob("*.cpp")) if not p.name.endswith("Module.cpp")]
    return core + sorted(TESTS.rglob("*.cpp"))


def main():
    compiler, sanitize = select_toolchain()
    if compiler is None:
        print("No working C++ toolchain found. Candidates tried:")
        for candidate in candidate_compilers() or ["(none on PATH)"]:
            print(f"  {candidate}")
        return 2

    if "--no-sanitize" in sys.argv:
        sanitize = False

    BUILD.mkdir(exist_ok=True)
    files = sources()
    if not files:
        print("No sources found.")
        return 2

    flags = [compiler, "-std=" + STANDARD, *WARNINGS, "-g", "-O1"]
    if sanitize:
        flags += SANITIZE

    print(f"compiler   {compiler}")
    print(f"sanitizers {'address,undefined' if sanitize else 'UNAVAILABLE on this toolchain'}")
    print(f"sources    {len(files)}")

    env = environment(compiler)
    started = time.time()
    objects = []
    failed = 0
    for source in files:
        obj = BUILD / (source.stem + ".o")
        command = flags + ["-I", str(CORE), "-I", str(TESTS), "-c", str(source), "-o", str(obj)]
        result = subprocess.run(command, capture_output=True, text=True, env=env)
        if result.returncode != 0:
            failed += 1
            print(f"\nFAILED {source.relative_to(ROOT)}")
            print(result.stderr.rstrip())
        else:
            if result.stderr.strip():
                print(f"\nWARNINGS {source.relative_to(ROOT)}")
                print(result.stderr.rstrip())
            objects.append(obj)

    if failed:
        print(f"\n{failed} file(s) failed to compile")
        return 1

    binary = BUILD / ("FloorPlanCoreTests" + (".exe" if os.name == "nt" else ""))
    link = [compiler, *[str(o) for o in objects], "-o", str(binary)]
    if sanitize:
        link += SANITIZE
    result = subprocess.run(link, capture_output=True, text=True, env=env)
    if result.returncode != 0:
        print("\nLINK FAILED")
        print(result.stderr.rstrip())
        return 1

    print(f"built      {binary.name}  ({time.time() - started:.1f}s)")

    if "--no-run" in sys.argv:
        return 0

    print()
    passthrough = [a for a in sys.argv[1:] if not a.startswith("--")]
    return subprocess.run([str(binary)] + passthrough, env=env).returncode


if __name__ == "__main__":
    sys.exit(main())
