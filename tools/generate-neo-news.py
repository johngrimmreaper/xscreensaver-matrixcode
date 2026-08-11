#!/usr/bin/env python3
import argparse
import json
from pathlib import Path


def c_string(value):
    if value is None:
        return "NULL"
    # JSON string syntax is valid for the ASCII/UTF-8 metadata emitted here and
    # provides the escaping needed for C string literals.
    return json.dumps(str(value), ensure_ascii=True)


def c_ident(value):
    out = []
    for ch in value:
        if ch.isalnum():
            out.append(ch.lower())
        else:
            out.append("_")
    ident = "".join(out).strip("_")
    while "__" in ident:
        ident = ident.replace("__", "_")
    return ident or "article"


def display_body(article):
    parts = []
    used_summary = False

    segments = article.get("body", {}).get("segments", [])
    for segment in sorted(segments, key=lambda s: s.get("sequence", 0)):
        text = segment.get("text")
        if isinstance(text, str) and text.strip():
            parts.append(text.strip())
            continue

        summary = segment.get("summary")
        if isinstance(summary, str) and summary.strip():
            parts.append(summary.strip())
            used_summary = True

    if not parts:
        summary = article.get("summary")
        if isinstance(summary, str) and summary.strip():
            parts.append(summary.strip())
            used_summary = True

    return "\n\n".join(parts), used_summary


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input", type=Path)
    ap.add_argument("output", type=Path)
    args = ap.parse_args()

    data = json.loads(args.input.read_text())
    articles = data.get("articles", [])
    article_by_id = {a["id"]: a for a in articles}

    session_id = data.get("dataset", {}).get("default_search_session_id")
    sessions = data.get("search_sessions", [])
    session = next((s for s in sessions if s.get("id") == session_id), None)
    if session is None:
        raise SystemExit(f"default search session not found: {session_id!r}")

    query_text = session.get("query", {}).get("text") or ""
    results = sorted(session.get("results", []),
                     key=lambda r: r.get("appearance_order",
                                         r.get("rank", 0)))
    result_ids = [r["article_id"] for r in results]

    for article_id in result_ids:
        if article_id not in article_by_id:
            raise SystemExit(f"search result references missing article: {article_id}")

    lines = []
    lines.append("/* Generated file. Do not edit by hand. */")
    lines.append("/* Source: data/neo-workstation/search-results.json */")
    lines.append("")
    lines.append(f"static const char neo_news_seed_query_text[] = {c_string(query_text)};")
    lines.append("")

    segment_symbols = {}
    for article in articles:
        segments = article.get("body", {}).get("segments", [])
        if not segments:
            continue

        symbol = f"neo_news_segments_{c_ident(article['id'])}"
        segment_symbols[article["id"]] = symbol
        lines.append(f"static const neo_news_segment {symbol}[] = {{")
        for segment in sorted(segments, key=lambda s: s.get("sequence", 0)):
            lines.append("    {")
            lines.append(f"        {int(segment.get('sequence', 0))}U,")
            lines.append(f"        {c_string(segment.get('kind'))},")
            lines.append(f"        {c_string(segment.get('text'))},")
            lines.append(f"        {c_string(segment.get('summary'))},")
            lines.append(f"        {c_string(segment.get('content_status'))}")
            lines.append("    },")
        lines.append("};")
        lines.append("")

    lines.append("static const neo_news_article neo_news_seed_articles[] = {")
    for article in articles:
        body, uses_summary = display_body(article)
        segments = article.get("body", {}).get("segments", [])
        symbol = segment_symbols.get(article["id"], "NULL")
        segment_ptr = symbol if segments else "NULL"
        lines.append("    {")
        lines.append(f"        {c_string(article.get('id'))},")
        lines.append(f"        {c_string(article.get('origin'))},")
        lines.append(f"        {c_string(article.get('provider'))},")
        lines.append(f"        {c_string(article.get('language'))},")
        lines.append(f"        {c_string(article.get('title'))},")
        lines.append(f"        {c_string(article.get('subtitle'))},")
        lines.append(f"        {c_string(article.get('dateline'))},")
        lines.append(f"        {c_string(article.get('published_at'))},")
        lines.append(f"        {c_string(article.get('url'))},")
        lines.append(f"        {c_string(article.get('summary'))},")
        lines.append(f"        {c_string(article.get('content_status'))},")
        lines.append(f"        {segment_ptr},")
        lines.append(f"        {len(segments)}U,")
        lines.append(f"        {c_string(body)},")
        lines.append(f"        {1 if uses_summary else 0}")
        lines.append("    },")
    lines.append("};")
    lines.append("")

    lines.append("static const char *const neo_news_seed_result_ids[] = {")
    for article_id in result_ids:
        lines.append(f"    {c_string(article_id)},")
    lines.append("};")
    lines.append("")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines))


if __name__ == "__main__":
    main()
