# Neo Workstation news data model

`search-results.json` is intentionally shaped like a small news database rather
than a scene-specific transcription file.

The logical tables are:

- `publishers`
- `articles`
- `search_sessions`
- `search_sessions[].results`
- `ui_strings`

Film-reconstruction metadata lives under `provenance`; the core article fields
are generic enough for future RSS/Atom/API/live-news adapters.

A future live record should use the same `articles` shape, for example:

```json
{
  "id": "provider:stable-id",
  "record_type": "article",
  "origin": "live",
  "provider": "rss",
  "external_id": "stable-id",
  "publisher_id": "publisher-id",
  "language": "en",
  "country_code": "US",
  "title": "Example headline",
  "subtitle": null,
  "authors": ["Example Author"],
  "dateline": null,
  "published_at": "2026-08-11T20:00:00Z",
  "updated_at": null,
  "url": "https://example.invalid/article",
  "canonical_url": "https://example.invalid/article",
  "summary": "Short summary.",
  "content_status": "full",
  "body": {
    "format": "segments",
    "segments": [
      {
        "sequence": 1,
        "kind": "paragraph",
        "text": "Article paragraph.",
        "summary": null,
        "content_status": "full",
        "confidence": "source",
        "evidence_source_ids": []
      }
    ]
  },
  "media": [],
  "topics": [],
  "entities": [],
  "locations": [],
  "provenance": {
    "confidence": "source",
    "field_evidence": {}
  }
}
```

For larger live-news caches, JSON should remain the interchange/export format
while SQLite is a natural future persistence layer using the same logical
tables.
