#!/usr/bin/env python3
# Tuxchii branding: renames the app to "Tuxchii" in everything the user sees (window titles,
# dialogs, menus, settings text, the desktop entry), after the Linux patches are applied.
#
# Only user-visible text changes. Internal names stay as they are, so nothing breaks and existing
# data keeps working: data folders (~/.local/share/fritchii, ~/.cache/fritchii), settings keys,
# environment variables (FRITCHII_CLASSIC_UI...), Qt resource paths and object names, the build
# name compared in code (g_build_name == "Fritchii"), log lines, and upstream's update checker.
#
# Usage: tuxchii-branding.py [--dry-run] <source dir>     (prints every change it makes)

import pathlib
import re
import sys

NEW = "Tuxchii"

# Proper names of the app, as whole words, in string literals.
# (also right after an escape such as "\nFritchii")
NAMES = re.compile(r"(?:(?<=\\[nt])|(?<![A-Za-z0-9_]))(?:Fritchii|Lichii)\b")

# Text that names another project, an older build or a folder on disk, not this app.
PROTECT = [
    r"Frizti/Lichii", r"zortos293/Lichii", r"Lichii \d", r"Lichii's `?%APPDATA%",
    r"Earlier Lichii builds", r"earlier Fritchii", r"Fritchii or Eden folder",
    r"Fritchii-v", r"Fritchii #\d", r"Lichii#\d", r"Lichii contributor",
    # The community Discord and the emulators a profile can be imported from are other projects.
    r"Fritchii &?Discord",
]

# Files whose strings are identifiers or protocol data, never shown as the app's name.
SKIP_FILES = {
    "src/frontend_common/update_checker.cpp", "src/frontend_common/update_checker.h",
    "src/common/net/net.cpp", "src/common/settings.cpp", "src/yuzu/fritchii/fritchii_ui.h",
}
SKIP_DIRS = ("src/tests/", "src/dynarmic/", "src/android/", "src/web_service/")

# A line with one of these handles names as identifiers (or only logs them): left alone.
SKIP_LINE = re.compile(
    r"g_build_name|LOG_[A-Z]+\(|getenv|qEnvironmentVariable|setObjectName|setProperty|"
    r"property\(|QFile|\":/|QIcon::fromTheme|setDesktopFileName|setOrganizationName|"
    r"setApplicationName|objectName|HiddenReleaseMarker|RepoNames|ReleaseRepo|User-Agent|"
    # The other emulators' folders a profile or saves can be imported from keep their names.
    r"LichiiDir|LegacyEdenDir")

# Places where upstream still calls the app "Eden" (the emulator it is built on) as its own name.
EDEN_SELF = [
    ("src/qt_common/util/content.cpp", '"Eden has detected', '"Tuxchii has detected'),
    ("src/qt_common/util/content.cpp", "restart Eden before", "restart Tuxchii before"),
    ("src/qt_common/qt_string_lookup.h", '"Eden or re-install firmware', '"Tuxchii or re-install firmware'),
    ("src/qt_common/util/game.cpp", "with the Eden Emulator", "with Tuxchii"),
    ("src/yuzu/configuration/system/new_user_dialog.cpp", '"Eden", tr("New User")', '"Tuxchii", tr("New User")'),
    ("src/core/hle/service/acc/profile_manager.cpp", 'CreateNewUser(UUID::MakeRandom(), "Eden")',
     'CreateNewUser(UUID::MakeRandom(), "Tuxchii")'),
    ("src/core/hle/service/acc/acc.cpp", 'CreateNewUser(user_id, "Eden")', 'CreateNewUser(user_id, "Tuxchii")'),
    ("src/common/settings.h", 'linkage, "Eden", "device_name"', 'linkage, "Tuxchii", "device_name"'),
    ("src/common/settings.h", 'eden_username{linkage, "Eden"', 'eden_username{linkage, "Tuxchii"'),
    # The app name in the desktop's volume mixer.
    ("src/audio_core/sink/cubeb_sink.cpp", 'cubeb_init(&ctx, "Eden"', 'cubeb_init(&ctx, "Tuxchii"'),
    ("src/input_common/drivers/sdl_driver.cpp", 'SDL_HINT_APP_NAME, "Eden"', 'SDL_HINT_APP_NAME, "Tuxchii"'),
    # eden-cli's window titles: "Eden Fritchii | ..." -> "Tuxchii | ...".
    ("src/yuzu_cmd/emu_window/emu_window_sdl3_vk.cpp", 'fmt::format("Eden {} | {}-{} (Vulkan)",',
     'fmt::format("{} | {}-{} (Vulkan)",'),
    ("src/yuzu_cmd/emu_window/emu_window_sdl3_null.cpp", 'fmt::format("Eden {} | {}-{} (Vulkan)",',
     'fmt::format("{} | {}-{} (Vulkan)",'),
    ("src/yuzu_cmd/emu_window/emu_window_sdl3_null.cpp", '(Vulkan)", Common::g_build_name,',
     '(Vulkan)", "Tuxchii",'),
    # Linux problems are reported on this repository's GitHub issues.
    ("src/yuzu/util/export_log.cpp",
     '"Drag this file into your post in the Fritchii Discord issues, or send it to Frizti."',
     '"Attach this file to an issue at github.com/xTopher1049/Tuxchii/issues."'),
    # The import dialogs name the folder the data comes from: upstream calls a Fritchii-used Eden
    # folder "Fritchii"; it is the Eden folder (or Lichii's) on disk.
    ("src/yuzu/user_data_migration.cpp", 'best->name = QStringLiteral("Fritchii");',
     '// Tuxchii: keep the folder\'s own name (Eden or Lichii).'),
]

# Where the build name ("Fritchii", also used as an identifier) is shown: show "Tuxchii".
BUILD_NAME_SHOWN = [
    (r"std::string\{Common::g_build_name\}", 'std::string{"Tuxchii"}'),
    (r"QString::from(?:Latin1|Utf8)\(Common::g_build_name\)", 'QStringLiteral("Tuxchii")'),
    (r"^(\s*)Common::g_build_name,", r'\1"Tuxchii",'),
]

# Removed outright: the Discord entries (Help menu and the home screen of Fritchii's interface).
REMOVE_LINES = [
    ("src/yuzu/fritchii/fritchii_ui.cpp", r"^.*\bauto\* discord = new QAction\(.*\n"),
    ("src/yuzu/fritchii/fritchii_ui.cpp", r"^.*QObject::connect\(discord,.*\n"),
    ("src/yuzu/fritchii/fritchii_ui.cpp", r"^.*insertAction\([^,]+, discord\);\n"),
    ("src/yuzu/fritchii/home.cpp", r"^.*addWidget\(link\([^\n]*&OpenDiscord\)\);\n"),
]

# The About dialog keeps one line crediting the projects Tuxchii is built from.
ABOUT_CREDIT = (
    "src/yuzu/about_dialog.cpp",
    "\n}\n\nAboutDialog::~AboutDialog",
    '\n    ui->labelBuildInfo->setText(ui->labelBuildInfo->text() + QStringLiteral("<br>") +\n'
    '                                tr("Based on Fritchii by Frizti and Lichii by zortos293."));\n'
    "}\n\nAboutDialog::~AboutDialog",
)

STRING = re.compile(r'(?<![A-Za-z0-9_])"((?:[^"\\\n]|\\.)*)"')


def protect(text, fn):
    """Apply fn to text, leaving the PROTECT phrases untouched."""
    pattern = re.compile("|".join(f"(?:{p})" for p in PROTECT))
    out, last = [], 0
    for m in pattern.finditer(text):
        out.append(fn(text[last:m.start()]))
        out.append(m.group(0))
        last = m.end()
    out.append(fn(text[last:]))
    return "".join(out)


def rename(text):
    return protect(text, lambda s: NAMES.sub(NEW, s))


def rewrite_code(rel, text):
    lines = text.split("\n")
    for i, line in enumerate(lines):
        if rel.startswith(("src/yuzu/", "src/yuzu_cmd/", "src/qt_common/")) and "LOG_" not in line \
                and not re.search(r"g_build_name\}?\s*[!=]=", line):
            for a, b in BUILD_NAME_SHOWN:
                line = re.sub(a, b, line)
            lines[i] = line
        if SKIP_LINE.search(line) or line.lstrip().startswith(("//", "*", "/*", "#include")):
            continue
        lines[i] = STRING.sub(lambda m: '"' + rename(m.group(1)) + '"', line)
    return "\n".join(lines)


def rewrite_ui(rel, text):
    # Only <string> elements (titles, labels, tooltips); never resources, names or properties.
    return re.sub(r"(<string[^>]*>)(.*?)(</string>)",
                  lambda m: m.group(1) + rename(m.group(2)) + m.group(3), text, flags=re.S)


def main():
    args = sys.argv[1:]
    dry = "--dry-run" in args
    root = pathlib.Path([a for a in args if a != "--dry-run"][0])
    changed = 0
    for path in sorted(root.glob("src/**/*")):
        rel = path.relative_to(root).as_posix()
        if path.suffix not in (".cpp", ".h", ".ui", ".in", ".md") or rel in SKIP_FILES or rel.startswith(SKIP_DIRS):
            continue
        if path.suffix == ".md" and not rel.startswith("src/yuzu/fritchii/"):
            continue
        old = path.read_text(encoding="utf-8", errors="surrogateescape")
        new = old
        for f, a, b in EDEN_SELF:
            if rel == f:
                new = new.replace(a, b)
        for f, pattern in REMOVE_LINES:
            if rel == f:
                new = re.sub(pattern, "", new, flags=re.M)
        if path.suffix == ".md":  # the "What's new" window
            new = rename(new)
        elif path.suffix == ".ui":
            new = rewrite_ui(rel, new)
        else:
            new = rewrite_code(rel, new)
        if rel == ABOUT_CREDIT[0] and ABOUT_CREDIT[1] in new:
            new = new.replace(ABOUT_CREDIT[1], ABOUT_CREDIT[2], 1)
        if new != old:
            changed += 1
            for n, (a, b) in enumerate(zip(old.split("\n"), new.split("\n")), 1):
                if a != b:
                    print(f"{rel}:{n}: {a.strip()[:150]}\n{' ' * (len(rel) + len(str(n)) + 3)}-> {b.strip()[:150]}")
            if len(old.split("\n")) != len(new.split("\n")):
                print(f"{rel}: (lines added)")
            if not dry:
                path.write_text(new, encoding="utf-8", errors="surrogateescape")
    # The full build name shown in eden-cli titles and the log export ("Fritchii v5.8 ").
    cmake = root / "CMakeModules/GenerateSCMRev.cmake"
    if cmake.is_file():
        old = cmake.read_text()
        new = old.replace('set(BUILD_FULLNAME "${REPO_NAME} ${BUILD_VERSION} ")',
                          f'set(BUILD_FULLNAME "{NEW} ${{BUILD_VERSION}} ")')
        if new != old:
            changed += 1
            print(f"{cmake.relative_to(root)}: BUILD_FULLNAME -> {NEW}")
            if not dry:
                cmake.write_text(new)
    # The desktop entry: the name shown in app menus and the taskbar.
    for desktop in root.glob("dist/*.desktop"):
        old = desktop.read_text()
        new = re.sub(r"^Name=(Lichii|Fritchii)$", f"Name={NEW}", old, flags=re.M)
        if new != old:
            changed += 1
            print(f"{desktop.relative_to(root)}: Name= -> {NEW}")
            if not dry:
                desktop.write_text(new)
    print(f"Tuxchii branding: {changed} files {'would change' if dry else 'changed'}")


if __name__ == "__main__":
    main()
