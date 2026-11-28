#!/usr/bin/env python3
"""Push the wiki page source in this directory to the Stm32World wiki.

    ./push.py diff                 show the differences between the wiki and the local file
    ./push.py push -m "summary"    upload the local file as a new revision

Login uses a MediaWiki bot password (Special:BotPasswords, grant "Edit
existing pages" and "Create, edit, and move pages"), read from
~/.config/stm32world-wiki.ini, never from this repository:

    [wiki]
    user = YourName@botname
    password = the-generated-bot-password

push refuses to overwrite the page if it changed on the wiki since the last
push (a copy of the pushed text is kept in pushed/, commit it): merge those
edits into the local file first, then push with --force.
"""
import argparse
import configparser
import difflib
import http.cookiejar
import json
import os
import sys
import urllib.parse
import urllib.request

API = "https://stm32world.com/w/api.php"
HERE = os.path.dirname(os.path.abspath(__file__))
PAGES = {"Hazard3 + FPU on FPGA": "Hazard3_+_FPU_on_FPGA.wiki"}
CONFIG = os.path.expanduser("~/.config/stm32world-wiki.ini")

opener = urllib.request.build_opener(
    urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()))
opener.addheaders = [("User-Agent", "hazard3_fpu-wiki-sync/1.0")]


def api(params, post=False):
    params = dict(params, format="json", formatversion="2")
    data = urllib.parse.urlencode(params).encode()
    if post:
        req = urllib.request.Request(API, data=data)
    else:
        req = urllib.request.Request(API + "?" + data.decode())
    with opener.open(req, timeout=60) as r:
        res = json.load(r)
    if "error" in res:
        sys.exit(f"API error: {res['error'].get('code')}: {res['error'].get('info')}")
    return res


def latest(title):
    """(text, user) of the latest revision, or (None, None) if the page is missing."""
    res = api({"action": "query", "prop": "revisions", "titles": title,
               "rvprop": "content|user", "rvslots": "main"})
    page = res["query"]["pages"][0]
    if page.get("missing"):
        return None, None
    rev = page["revisions"][0]
    return rev["slots"]["main"]["content"], rev["user"]


def local(fname):
    with open(os.path.join(HERE, fname), encoding="utf-8") as f:
        return f.read().rstrip("\n")


def pushed(fname):
    try:
        return local(os.path.join("pushed", fname))
    except FileNotFoundError:
        return None


def login():
    cfg = configparser.ConfigParser()
    if not cfg.read(CONFIG):
        sys.exit(f"No credentials: create {CONFIG} (see the top of this script)")
    user, password = cfg["wiki"]["user"], cfg["wiki"]["password"]
    token = api({"action": "query", "meta": "tokens", "type": "login"})["query"]["tokens"]["logintoken"]
    res = api({"action": "login", "lgname": user, "lgpassword": password, "lgtoken": token}, post=True)
    if res["login"]["result"] != "Success":
        sys.exit(f"Login failed: {res['login']}")
    return res["login"]["lgusername"]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=["diff", "push"])
    ap.add_argument("-m", "--summary", default="Sync with the hazard3_fpu repository")
    ap.add_argument("--force", action="store_true", help="overwrite edits made on the wiki by others")
    args = ap.parse_args()

    if args.cmd == "push":
        login()
    for title, fname in PAGES.items():
        new = local(fname)
        old, user = latest(title)
        if args.cmd == "diff":
            if old is None:
                print(f"{title}: does not exist on the wiki yet")
                continue
            d = list(difflib.unified_diff(old.splitlines(), new.splitlines(),
                                          "wiki", "local", lineterm=""))
            print("\n".join(d) if d else f"{title}: no differences (last edit by {user})")
            continue
        if old is not None and old.rstrip("\n") == new:
            print(f"{title}: unchanged")
            continue
        if old is not None and not args.force and old.rstrip("\n") != pushed(fname):
            sys.exit(f"{title}: changed on the wiki since the last push (last edit by {user}); "
                     "run './push.py diff', merge those changes into the local file, "
                     "then push with --force")
        csrf = api({"action": "query", "meta": "tokens"})["query"]["tokens"]["csrftoken"]
        res = api({"action": "edit", "title": title, "text": new, "summary": args.summary,
                   "token": csrf}, post=True)
        e = res["edit"]
        print(f"{title}: {e['result']}, revision {e.get('newrevid', '-')}")
        if e["result"] == "Success":
            os.makedirs(os.path.join(HERE, "pushed"), exist_ok=True)
            with open(os.path.join(HERE, "pushed", fname), "w", encoding="utf-8") as f:
                f.write(new + "\n")
        print("https://stm32world.com/wiki/" + urllib.parse.quote(title.replace(" ", "_")))


if __name__ == "__main__":
    main()
