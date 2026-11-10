#!/usr/bin/env python3
"""Turns docs/ into GitHub wiki pages.

    python3 .github/scripts/docs_to_wiki.py <repo root> <wiki folder> <owner/repo>

The wiki is flat, so docs/plugins/Swarm.md becomes the page "Swarm" and docs/README.md becomes "Home".
Links between docs pages become wiki links, and links to pictures or other files in the repository
become full github.com links. A sidebar (_Sidebar.md) and a footer (_Footer.md) are written too.
Pages the script made before but that no longer exist in docs/ are removed; other wiki pages are kept.
"""
import os, re, sys, glob

root, wiki, repo = sys.argv[1], sys.argv[2], sys.argv[3]
docs = os.path.join(root, "docs")
marker = "<!-- made from docs/ by docs_to_wiki.py: edit the file in the repository, not here -->"

pages = {}                                   # absolute path of a docs .md -> wiki page name
for path in glob.glob(os.path.join(docs, "**", "*.md"), recursive=True):
    rel = os.path.relpath(path, docs)
    if rel.split(os.sep)[0] == "reference":
        continue
    stem = os.path.splitext(os.path.basename(path))[0]
    pages[os.path.abspath(path)] = "Home" if rel == "README.md" else stem

names = list(pages.values())
assert len(names) == len(set(names)), "two docs pages would get the same wiki name"

images = (".png", ".jpg", ".jpeg", ".gif", ".svg", ".webp")


def target(src_dir, link):
    """The wiki form of one link found in a page that lives in src_dir."""
    if re.match(r"^[a-z]+:", link) or link.startswith("#") or link.startswith("/"):
        return link
    path, _, anchor = link.partition("#")
    if not path:
        return link
    full = os.path.abspath(os.path.normpath(os.path.join(src_dir, path)))
    if full in pages:
        return pages[full] + ("#" + anchor if anchor else "")
    rel = os.path.relpath(full, root).replace(os.sep, "/")
    if rel.startswith(".."):
        return link
    kind = "raw" if rel.lower().endswith(images) else ("tree" if os.path.isdir(full) else "blob")
    return f"https://github.com/{repo}/{kind}/main/{rel}" + ("#" + anchor if anchor else "")


def convert(path):
    src_dir = os.path.dirname(path)
    text = open(path, encoding="utf-8").read()
    text = re.sub(r"(\]\()([^)\s]+)(\))", lambda m: m.group(1) + target(src_dir, m.group(2)) + m.group(3), text)
    text = re.sub(r'((?:src|href)=")([^"]+)(")', lambda m: m.group(1) + target(src_dir, m.group(2)) + m.group(3), text)
    return marker + "\n\n" + text


os.makedirs(wiki, exist_ok=True)
for old in glob.glob(os.path.join(wiki, "*.md")):           # pages this script made earlier
    with open(old, encoding="utf-8") as f:
        if f.readline().strip() == marker:
            os.remove(old)

for path, name in pages.items():
    with open(os.path.join(wiki, name + ".md"), "w", encoding="utf-8") as f:
        f.write(convert(path))


def group(sub):
    return sorted(n for p, n in pages.items() if os.path.dirname(p) == os.path.abspath(os.path.join(docs, sub)))


guides = [n for n in ["Install", "DAW", "Every-Cat", "Cat-Link", "Composer", "FAQ", "Building", "ROADMAP"] if n in names]
titles = {"DAW": "Using it in your DAW", "Every-Cat": "Every cat at a glance", "ROADMAP": "Roadmap"}
side = [marker, "", "**[Home](Home)**", "", "**Start**", ""]
side += [f"- [{titles.get(n, n.replace('-', ' '))}]({n})" for n in guides]
side += ["", "**Plug-ins**", ""] + [f"- [{n}]({n})" for n in group("plugins")]
side += ["", "**Apps**", ""] + [f"- [{n.replace('-', ' ')}]({n})" for n in group("apps")]
side += ["- [Cat Box](https://github.com/%s/blob/main/CatBox/README.md)" % repo]
side += ["", f"[Download](https://github.com/{repo}/releases/latest)"]
open(os.path.join(wiki, "_Sidebar.md"), "w", encoding="utf-8").write("\n".join(side) + "\n")
open(os.path.join(wiki, "_Footer.md"), "w", encoding="utf-8").write(
    marker + f"\n\n[Cat Suite](https://github.com/{repo}) · free, GPL v3 · these pages come from the "
    f"[docs folder](https://github.com/{repo}/tree/main/docs): fix them there.\n")
print(len(pages), "pages")
