#include "neo_news.h"

#include <string.h>

#include "generated/neo_news_seed.h"

const char *neo_news_query_text(void)
{
    return neo_news_seed_query_text;
}

size_t neo_news_result_count(void)
{
    return sizeof(neo_news_seed_result_ids) /
           sizeof(neo_news_seed_result_ids[0]);
}

const neo_news_article *neo_news_find_article(const char *id)
{
    size_t i;

    if (!id)
        return NULL;

    for (i = 0U;
         i < sizeof(neo_news_seed_articles) /
             sizeof(neo_news_seed_articles[0]);
         i++) {
        if (strcmp(neo_news_seed_articles[i].id, id) == 0)
            return &neo_news_seed_articles[i];
    }

    return NULL;
}

const neo_news_article *neo_news_search_result(size_t index)
{
    if (index >= neo_news_result_count())
        return NULL;

    return neo_news_find_article(neo_news_seed_result_ids[index]);
}
