#!/usr/bin/env python3
"""Pre-build check for a new Fritchii release, run BEFORE starting the AppImage build.

Usage:  python3 precheck.py NEW_TAG [BASE_TAG] [--install-deps]
  NEW_TAG   the upstream tag about to be built, e.g. fritchii-v5.4
  BASE_TAG  the last version that built fine (default: the upstream fritchii-* tag before NEW_TAG)
Run it from a checkout of this repo (xTopher1049/Fritchii-linux): it reads the workflow's
"Apply Linux build patches" step and .github/linux-patches/ from there.

What it checks, on Frizti's source at NEW_TAG:
  1. PATCHES   the workflow's patch step still applies (same commands the build runs).
  2. WINDOWS   code that only exists in Windows builds but is used by code Linux compiles
               (the kind of bug that broke v5: g_committed_bytes, GetExeDirectory).
  3. UI        changes that can break the classic-interface default / Settings > UI checkbox:
               new or changed FritchiiUi::Enabled() call sites, calls made before the settings
               file is loaded, and upstream edits inside the code the classic-UI patch touches.
  4. COMPILE   syntax-only compile (with the patches applied) of every file the patches touch and
               every changed .cpp in the Qt frontend and common code. Needs Qt 6 headers, uic and
               g++; --install-deps installs them (apt) and fetches fmt/SimpleIni headers.
Exit code 0 = safe to build; 1 = something needs fixing first (details printed).
"""
import json, os, re, shutil, subprocess, sys, tempfile

UPSTREAM = "https://github.com/Frizti/Fritchii.git"
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
WORKFLOW = os.path.join(REPO_ROOT, ".github/workflows/appimage-auto-release.yml")
PATCH_DIR = os.path.join(REPO_ROOT, ".github/linux-patches")
WORK = os.environ.get("PRECHECK_WORK", os.path.join(tempfile.gettempdir(), "fritchii-precheck"))
SRC = os.path.join(WORK, "src")
problems, warnings = [], []


def sh(cmd, cwd=None, check=True, env=None):
    r = subprocess.run(cmd, cwd=cwd, shell=isinstance(cmd, str), capture_output=True, text=True,
                       env=env)
    if check and r.returncode != 0:
        raise RuntimeError(f"command failed ({r.returncode}): {cmd}\n{r.stdout}\n{r.stderr}")
    return r


def section(title):
    print(f"\n=== {title} ===")


# ---------------------------------------------------------------- source
def fetch_source(new, base):
    os.makedirs(WORK, exist_ok=True)
    if not os.path.isdir(os.path.join(SRC, ".git")):
        sh(["git", "init", "-q", SRC])
        sh(["git", "remote", "add", "origin", UPSTREAM], cwd=SRC)
    for tag in (new, base):
        sh(["git", "fetch", "-q", "--depth", "1", "origin", f"refs/tags/{tag}:refs/tags/{tag}"],
           cwd=SRC)
    checkout(new)


def checkout(tag):
    sh(["git", "checkout", "-q", "-f", tag], cwd=SRC)
    sh(["git", "clean", "-qfdx", "-e", "_deps"], cwd=SRC)


def upstream_tags_before(new):
    out = sh(["git", "ls-remote", "--tags", UPSTREAM]).stdout
    tags = sorted({l.split("refs/tags/")[1].removesuffix("^{}") for l in out.splitlines()
                   if "refs/tags/fritchii-" in l}, key=version_key)
    older = [t for t in tags if version_key(t) < version_key(new)]
    return older[-1] if older else None


def version_key(tag):
    return [int(x) if x.isdigit() else x for x in re.split(r"[.\-v]", tag.removeprefix("fritchii-")) if x]


# ---------------------------------------------------------------- 1. patches
def patch_step_script():
    text = open(WORKFLOW).read()
    try:
        import yaml
        d = yaml.safe_load(text)
        for step in d["jobs"]["build"]["steps"]:
            if step.get("name") == "Apply Linux build patches":
                return step["run"]
    except ImportError:
        m = re.search(r"- name: Apply Linux build patches\n\s+run: \|\n((?:\s{10}.*\n|\n)+)", text)
        if m:
            return "\n".join(l[10:] for l in m.group(1).splitlines()) + "\n"
    raise RuntimeError("could not find the 'Apply Linux build patches' step in the workflow")


def apply_patches(tag):
    dest = os.path.join(SRC, "_linux-patches/.github/linux-patches")
    shutil.rmtree(os.path.join(SRC, "_linux-patches"), ignore_errors=True)
    shutil.copytree(PATCH_DIR, dest)
    env = dict(os.environ, TAG=tag, GITHUB_ENV=os.path.join(WORK, "github_env"))
    r = sh(["bash", "-e", "-c", patch_step_script()], cwd=SRC, check=False, env=env)
    shutil.rmtree(os.path.join(SRC, "_linux-patches"), ignore_errors=True)
    return r


# ---------------------------------------------------------------- 2. windows-only code
COND = re.compile(r"^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)")


def regions(lines):
    """Yield (line_no, line, windows_only) for each line of a C/C++ file."""
    stack = []
    for i, line in enumerate(lines, 1):
        m = COND.match(line)
        if m:
            kind, rest = m.groups()
            win = bool(re.search(r"_WIN32|_MSC_VER|\bWIN32\b", rest)) and kind != "ifndef" and "!" not in rest
            if kind in ("if", "ifdef", "ifndef"):
                stack.append(win)
            elif kind in ("elif", "else") and stack:
                stack[-1] = False
            elif kind == "endif" and stack:
                stack.pop()
            continue
        yield i, line, any(stack)


def read(path):
    try:
        return open(os.path.join(SRC, path), errors="ignore").read().split("\n")
    except OSError:
        return []


def changed_files(base, new, pattern):
    out = sh(["git", "diff", "--name-only", "--diff-filter=AM", base, new, "--", *pattern], cwd=SRC).stdout
    return [f for f in out.split() if f]


SKIP = ("/windows/", "_win", "win32", "/android/", "apple", "msvc", "streamline", "dlss")
CALL = re.compile(r"\b([A-Z][A-Za-z0-9_]{5,})\s*\(")
GLOBAL = re.compile(r"^\s*(?:static\s+|inline\s+|constexpr\s+|const\s+)*[\w:<>,\s\*&]+?\b(g_\w+|s_\w+)\s*(?:\{|=|;|\[)")
LINUX_OK = {"ToU8String"}  # declared Windows-only in one header but available on Linux elsewhere


def scan_windows_only(base, new):
    headers = sh(["git", "ls-files", "src/*.h"], cwd=SRC).stdout.split()
    win_funcs, everywhere_all = {}, set()
    for h in headers:
        everywhere, win = set(), set()
        for _, line, w in regions(read(h)):
            (win if w else everywhere).update(CALL.findall(line))
        everywhere_all |= everywhere
        for name in win - everywhere - LINUX_OK:
            win_funcs.setdefault(name, h)
    # A name declared outside Windows-only code in any header (e.g. HostMemory::TrimZeroPages vs
    # the Windows-only DeviceMemory::TrimZeroPages) is available on Linux too.
    for name in everywhere_all:
        win_funcs.pop(name, None)
    for f in changed_files(base, new, ["src/*.cpp", "src/*.h"]):
        if any(s in f.lower() for s in SKIP):
            continue
        lines = list(regions(read(f)))
        for i, line, w in lines:
            if w or line.strip().startswith("//"):
                continue
            for name in CALL.findall(line):
                if name in win_funcs and win_funcs[name] != f:
                    problems.append(f"WINDOWS {f}:{i}: calls {name}(), declared only for Windows in "
                                    f"{win_funcs[name]}")
        win_globals = {m.group(1) for _, l, w in lines if w and (m := GLOBAL.match(l))}
        all_globals = {m.group(1) for _, l, w in lines if not w and (m := GLOBAL.match(l))}
        for name in win_globals - all_globals:
            for i, line, w in lines:
                if not w and re.search(rf"\b{name}\b", line) and not line.strip().startswith("//"):
                    problems.append(f"WINDOWS {f}:{i}: uses {name}, defined only for Windows")


# ---------------------------------------------------------------- 3. interface switch
# Files that switch on the build name for reasons unrelated to the interface (reviewed by hand).
BUILD_NAME_OK = {
    "src/yuzu/fritchii/fritchii_ui.cpp",          # Enabled() itself
    "src/frontend_common/update_checker.cpp",     # Fritchii skips update checks on Linux
    "src/qt_common/qt_common.cpp",                # "Fritchii vN" text in the window title
}

PATCHED_SPOTS = {  # code the classic-UI patch edits or depends on
    "src/yuzu/fritchii/fritchii_ui.cpp": ["bool Enabled()"],
    "src/yuzu/configuration/configure_ui.cpp": ["ConfigureUi::ConfigureUi", "ConfigureUi::ApplyConfiguration",
                                                "ConfigureUi::SetConfiguration", "ConfigureUi::RetranslateUI"],
    "src/qt_common/config/uisettings.h": ["fritchii_whats_new_seen"],
}


def scan_interface(base, new):
    diff = sh(["git", "diff", "-U0", base, new, "--", "src"], cwd=SRC).stdout
    current = None
    for line in diff.splitlines():
        if line.startswith("+++ b/"):
            current = line[6:]
        elif line.startswith("+") and "FritchiiUi::Enabled()" in line:
            warnings.append(f"UI  new/changed interface check in {current}: {line[1:].strip()[:110]}"
                            " -> verify it runs after the settings file is loaded")
    # Calls in main() before QApplication exists see the default, not the user's setting. The
    # classic-UI patch answers those from FRITCHII_CLASSIC_UI only and doesn't cache; check it.
    main = read("src/yuzu/main.cpp")
    app_line = next((i for i, l in enumerate(main, 1) if re.search(r"QApplication\s+app\s*[\(\{]", l)), None)
    early = [i for i, l in enumerate(main, 1) if "FritchiiUi::Enabled()" in l and app_line and i < app_line]
    patched = "QCoreApplication::instance() == nullptr" in "\n".join(read("src/yuzu/fritchii/fritchii_ui.cpp"))
    if early and not patched:
        problems.append(f"UI  main.cpp:{early} calls FritchiiUi::Enabled() before QApplication exists; "
                        "the classic-UI patch must not cache the answer there")
    elif early:
        print(f"UI  main.cpp:{early} checks the interface before settings load (handled by the patch)")
    # Upstream edits inside the functions the patch touches: the patch may still apply but be wrong.
    for path, anchors in PATCHED_SPOTS.items():
        hunks = sh(["git", "diff", "-U0", base, new, "--", path], cwd=SRC).stdout
        for anchor in anchors:
            if re.search(rf"^@@.*{re.escape(anchor)}", hunks, re.M) or \
               any(anchor in l for l in hunks.splitlines() if l[:1] in "+-" and not l.startswith(("+++", "---"))):
                warnings.append(f"UI  upstream changed {path} near '{anchor}' -> re-read the classic-UI patch there")
    # Other files that decide the interface without going through Enabled().
    for f in changed_files(base, new, ["src/yuzu/*.cpp", "src/qt_common/*.cpp"]):
        for i, line, _ in regions(read(f)):
            if "FRITCHII_CLASSIC_UI" in line and f != "src/yuzu/fritchii/fritchii_ui.cpp":
                warnings.append(f"UI  {f}:{i} reads FRITCHII_CLASSIC_UI directly -> make it follow the setting")
            if re.search(r'g_build_name\}?\s*==\s*"Fritchii"', line) and f not in BUILD_NAME_OK:
                warnings.append(f"UI  {f}:{i} switches on the build name, not FritchiiUi::Enabled() -> "
                                "check it in the classic interface")


# ---------------------------------------------------------------- 4. compile check
def install_deps():
    pkgs = "g++ qt6-base-dev qt6-base-private-dev qt6-tools-dev-tools libboost-dev libzstd-dev liblz4-dev " \
           "nlohmann-json3-dev libssl-dev python3-yaml"
    sudo = [] if os.geteuid() == 0 else ["sudo"]
    sh(sudo + ["apt-get", "update", "-q"], check=False)
    sh(sudo + ["apt-get", "install", "-y", "-q", *pkgs.split()], check=False)


def third_party_headers():
    cpm = json.load(open(os.path.join(SRC, "cpmfile.json")))
    incs = []
    for name, sub in (("fmt", "include"), ("simpleini", ""), ("vulkan-headers", "include"),
                      ("vulkan-utility-libraries", "include"), ("vulkan-memory-allocator", "include")):
        ver = cpm.get(name, {}).get("version")
        repo = cpm.get(name, {}).get("repo")
        dest = os.path.join(WORK, f"{name}-{ver}")
        if ver and repo and not os.path.isdir(dest):
            sh(["git", "clone", "-q", "--depth", "1", "--branch", ver, f"https://github.com/{repo}.git", dest],
               check=False)
        incs.append(os.path.join(dest, sub))
    return incs


def qt_include():
    for d in ("/usr/include/x86_64-linux-gnu/qt6", "/usr/include/qt6"):
        if os.path.isdir(d):
            return d
    return None


def compile_check(base, new, touched):
    qt, uic = qt_include(), next((p for p in ("/usr/lib/qt6/libexec/uic", "/usr/lib/qt6/bin/uic",
                                             shutil.which("uic") or "") if p and os.path.exists(p)), None)
    if not (qt and uic and shutil.which("g++")):
        warnings.append("COMPILE skipped: needs g++, Qt 6 headers and uic (run with --install-deps)")
        return
    gen = os.path.join(WORK, "gen")
    shutil.rmtree(gen, ignore_errors=True)
    os.makedirs(gen)
    for ui in sh(["git", "ls-files", "src/yuzu/*.ui"], cwd=SRC).stdout.split():
        sh([uic, ui, "-o", os.path.join(gen, "ui_" + os.path.basename(ui)[:-3] + ".h")], cwd=SRC, check=False)
    flags = ["-std=gnu++20", "-fsyntax-only", "-fPIC", "-DARCHITECTURE_x86_64=1", f"-I{gen}", "-Isrc",
             "-Iexternals"] + [f"-I{p}" for p in third_party_headers()] + \
            [f"-I{qt}"] + [f"-I{qt}/{m}" for m in ("QtCore", "QtGui", "QtWidgets", "QtDBus", "QtNetwork")]
    files = set(f for f in touched if f.endswith(".cpp"))
    files |= set(changed_files(base, new, ["src/yuzu/*.cpp", "src/qt_common/*.cpp", "src/common/*.cpp",
                                           "src/frontend_common/*.cpp"]))
    files = sorted(f for f in files if not any(s in f.lower() for s in SKIP) and os.path.exists(os.path.join(SRC, f)))
    skipped = 0
    for f in files:
        r = sh(["g++", *flags, f], cwd=SRC, check=False)
        errs = [l for l in r.stderr.splitlines() if " error:" in l or l.startswith("error:")]
        if any("No such file or directory" in e for e in errs):
            skipped += 1  # needs a generated or third-party header this check doesn't have
            continue
        for e in errs[:4]:
            problems.append(f"COMPILE {e.strip()[:220]}")
    print(f"COMPILE checked {len(files) - skipped} files ({skipped} skipped: headers not available here)")


# ---------------------------------------------------------------- main
def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if not args:
        sys.exit(__doc__)
    new = args[0]
    base = args[1] if len(args) > 1 else upstream_tags_before(new)
    if not base:
        sys.exit(f"no earlier fritchii-* tag than {new}; pass BASE_TAG")
    if "--install-deps" in sys.argv:
        install_deps()
    print(f"Pre-build check: {new} (compared with {base})")
    fetch_source(new, base)

    section("1. PATCHES (the workflow's patch step)")
    r = apply_patches(new)
    print(r.stdout.strip())
    if r.returncode != 0:
        problems.append(f"PATCHES the patch step failed (exit {r.returncode}): {r.stderr.strip()[-600:]}")
    touched = sh(["git", "diff", "--name-only"], cwd=SRC).stdout.split()

    section("2. WINDOWS-ONLY CODE")
    scan_windows_only(base, new)
    section("3. INTERFACE SWITCH")
    scan_interface(base, new)
    section("4. COMPILE")
    compile_check(base, new, touched)

    section("RESULT")
    for w in warnings:
        print("WARN ", w)
    for p in problems:
        print("FIX  ", p)
    if problems:
        print(f"NOT SAFE TO BUILD {new}: {len(problems)} problem(s) above need fixing first.")
    else:
        print(f"OK to build {new}" + (f" ({len(warnings)} warning(s) to review)" if warnings else "") + ".")
    print(f"(Frizti's source, patched, is in {SRC} for inspection)")
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
