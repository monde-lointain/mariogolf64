#!/usr/bin/env python3
"""Fetch scratches from decomp.me and save them as a combined JSON array."""

from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUTPUT_FILE = ROOT / "tools" / "scratches.json"
SECRET_FILE = ROOT / ".decomp_dev_secret"
# The compiler filter name (gcc2.7.2kmc) is unverified against the decomp.me API.
BASE_URL = "https://decomp.me/api/scratch?compiler=gcc2.7.2kmc&page_size=100&has_owner=true&ordering=-last_updated"


def repo_rel(path: Path, root: Path = ROOT) -> str:
    """Return path relative to root as POSIX, or absolute POSIX if outside root."""
    resolved = path.resolve()
    try:
        return resolved.relative_to(root).as_posix()
    except ValueError:
        return resolved.as_posix()


def log(message: str) -> None:
    """Write progress and diagnostics to stderr."""
    print(message, file=sys.stderr)


def get_user_agent() -> str | None:
    """Read the User-Agent from the first line of SECRET_FILE."""
    if SECRET_FILE.exists():
        with SECRET_FILE.open("r") as f:
            return f.readline().strip()
    return None


def fetch_page(cursor: str | None = None, user_agent: str | None = None) -> dict:
    """Fetch a single page of scratches from the API."""
    url = BASE_URL
    if cursor:
        url = f"{BASE_URL}&cursor={urllib.parse.quote(cursor)}"

    log(f"Fetching: {url}")

    headers = {"Accept": "application/json"}
    if user_agent:
        headers["User-Agent"] = user_agent

    req = urllib.request.Request(url, headers=headers)

    with urllib.request.urlopen(req, timeout=30) as response:
        return json.loads(response.read().decode("utf-8"))


def load_existing_scratches() -> tuple[list, dict]:
    """Load existing scratches from OUTPUT_FILE.
    Returns (scratches_list, slug_to_scratch_dict).
    """
    if not OUTPUT_FILE.exists():
        return [], {}

    with OUTPUT_FILE.open("r") as f:
        scratches = json.load(f)

    scratches_by_slug = {}
    for scratch in scratches:
        slug = scratch.get("slug") or scratch.get("id")
        if slug:
            scratches_by_slug[slug] = scratch

    return scratches, scratches_by_slug


def main():
    parser = argparse.ArgumentParser(description="Fetch scratches from decomp.me")
    parser.add_argument(
        "--cursor", "-c",
        help="Resume from a specific cursor (printed on error exit)"
    )
    parser.add_argument(
        "--update", "-u",
        action="store_true",
        help="Only fetch new or changed scratches (updates existing scratches.json)"
    )
    args = parser.parse_args()

    if args.update and args.cursor is not None:
        print("Error: --update and --cursor are mutually exclusive", file=sys.stderr)
        sys.exit(1)

    existing_scratches = []
    existing_by_slug = {}
    if args.update:
        existing_scratches, existing_by_slug = load_existing_scratches()
        if existing_scratches:
            log(f"Loaded {len(existing_scratches)} existing scratches from {repo_rel(OUTPUT_FILE)}")
        else:
            log(f"No existing {repo_rel(OUTPUT_FILE)} found, fetching all scratches")

    all_results = []
    new_or_updated = []
    cursor = args.cursor
    page_num = 1
    user_agent = get_user_agent()

    if user_agent:
        log(f"Using User-Agent from {repo_rel(SECRET_FILE)}")

    try:
        while True:
            data = fetch_page(cursor, user_agent)
            results = data.get("results", [])
            all_results.extend(results)
            log(f"Page {page_num}: fetched {len(results)} scratches (total: {len(all_results)})")

            if args.update:
                page_has_unchanged = True
                for scratch in results:
                    slug = scratch.get("slug") or scratch.get("id")
                    if not slug:
                        continue

                    score = scratch.get("score")
                    last_updated = scratch.get("last_updated")

                    if slug in existing_by_slug:
                        local_scratch = existing_by_slug[slug]
                        local_score = local_scratch.get("score")
                        local_last_updated = local_scratch.get("last_updated")

                        if score != local_score or last_updated != local_last_updated:
                            new_or_updated.append(scratch)
                            page_has_unchanged = False
                            log(f"  Changed: {slug} (score: {local_score} -> {score}, last_updated changed)")
                        else:
                            log(f"  Unchanged: {slug} (score={score}, last_updated={last_updated})")
                    else:
                        new_or_updated.append(scratch)
                        page_has_unchanged = False
                        log(f"  New: {slug}")

                if page_has_unchanged:
                    log("All scratches on this page were unchanged, stopping.")
                    break

            next_url = data.get("next")
            if not next_url:
                log("No more pages.")
                break

            parsed = urllib.parse.urlparse(next_url)
            params = urllib.parse.parse_qs(parsed.query)
            cursor = params.get("cursor", [None])[0]

            if not cursor:
                log("No cursor in next URL, stopping.")
                break

            page_num += 1
            time.sleep(10)

    except (OSError, ValueError) as e:
        log(f"\nError: {e}")
        if cursor:
            log("\nTo resume, run:")
            log(f"  python3 {sys.argv[0]} --cursor '{cursor}'")
        sys.exit(1)

    if args.update and new_or_updated:
        new_or_updated_slugs = {s.get("slug") or s.get("id") for s in new_or_updated}
        unchanged = [s for s in existing_scratches if (s.get("slug") or s.get("id")) not in new_or_updated_slugs]
        merged = new_or_updated + unchanged

        with OUTPUT_FILE.open("w") as f:
            json.dump(merged, f, indent=2)

        log(f"\nSaved {len(merged)} scratches to {repo_rel(OUTPUT_FILE)} ({len(new_or_updated)} new/updated)")
    elif args.update:
        log(f"\nNo new or updated scratches found. {repo_rel(OUTPUT_FILE)} unchanged.")
    else:
        with OUTPUT_FILE.open("w") as f:
            json.dump(all_results, f, indent=2)

        log(f"\nSaved {len(all_results)} scratches to {repo_rel(OUTPUT_FILE)}")


if __name__ == "__main__":
    main()
