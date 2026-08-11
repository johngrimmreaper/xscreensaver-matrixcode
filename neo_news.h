#ifndef MATRIXCODE_NEO_NEWS_H
#define MATRIXCODE_NEO_NEWS_H

#include <stddef.h>

typedef struct neo_news_segment {
    unsigned int sequence;
    const char *kind;
    const char *text;
    const char *summary;
    const char *content_status;
} neo_news_segment;

typedef struct neo_news_article {
    const char *id;
    const char *origin;
    const char *provider;
    const char *language;
    const char *title;
    const char *subtitle;
    const char *dateline;
    const char *published_at;
    const char *url;
    const char *summary;
    const char *content_status;
    const neo_news_segment *segments;
    size_t segment_count;

    /*
     * Renderer-ready body assembled by the corpus adapter.
     *
     * Preference order is segment.text, then segment.summary, then the article
     * summary.  This lets today's curated Matrix corpus and a future live-news
     * provider expose the same C model without coupling the renderer to JSON.
     */
    const char *display_body;
    int display_body_uses_summary;
} neo_news_article;

const char *neo_news_query_text(void);
size_t neo_news_result_count(void);
const neo_news_article *neo_news_search_result(size_t index);
const neo_news_article *neo_news_find_article(const char *id);

#endif
