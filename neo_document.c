#include "neo_document.h"

#include <string.h>

static int neo_document_add_block(neo_document *document,
                                  neo_document_block_type type,
                                  const char *text,
                                  neo_document_font_role font_role,
                                  unsigned int source_order)
{
    neo_document_block *block;

    if (!document || !text || text[0] == '\0')
        return 1;

    if (document->block_count >= NEO_DOCUMENT_MAX_BLOCKS)
        return 0;

    block = &document->blocks[document->block_count++];
    memset(block, 0, sizeof(*block));
    block->type = type;
    block->text = text;
    block->asset_id = NULL;
    block->style.font_role = font_role;
    block->style.alignment = NEO_DOC_ALIGN_LEFT;
    block->style.bold = (type == NEO_DOC_HEADING) ? 1U : 0U;
    block->style.italic = 0U;
    block->source_order = source_order;
    return 1;
}

int neo_document_from_article(const neo_news_article *article,
                              neo_document *document)
{
    size_t i;
    unsigned int order = 0U;

    if (!article || !document)
        return 0;

    memset(document, 0, sizeof(*document));

    document->version = NEO_DOCUMENT_VERSION;
    document->source.article_id = article->id;
    document->source.origin = article->origin;
    document->source.provider = article->provider;
    document->source.source_url = article->url;
    document->source.retrieved_at = article->published_at;

    document->headline_text = article->title;
    document->lead_text = article->summary;
    document->dateline_text = article->dateline;
    document->body_text = article->display_body;

    document->passive_only = 1U;
    document->remote_resources_allowed = 0U;
    document->executable_content_allowed = 0U;

    if (!neo_document_add_block(document, NEO_DOC_HEADING,
                                article->title,
                                NEO_DOC_FONT_HEADLINE, order++))
        return 0;

    if (!neo_document_add_block(document, NEO_DOC_DATELINE,
                                article->dateline,
                                NEO_DOC_FONT_METADATA, order++))
        return 0;

    if (!neo_document_add_block(document, NEO_DOC_LEAD,
                                article->summary,
                                NEO_DOC_FONT_LEAD, order++))
        return 0;

    for (i = 0U; i < article->segment_count; i++) {
        const neo_news_segment *segment = &article->segments[i];
        const char *text = segment->text;

        if (!text || text[0] == '\0')
            text = segment->summary;

        if (!neo_document_add_block(document, NEO_DOC_PARAGRAPH,
                                    text, NEO_DOC_FONT_BODY, order++))
            return 0;
    }

    /*
     * display_body is generated from segment.text first and segment.summary as
     * a fallback.  If a provider supplied only an article-level summary, keep
     * that as the body convenience view too.
     */
    if (!document->body_text || document->body_text[0] == '\0')
        document->body_text = article->summary;

    return 1;
}

const neo_document_block *
neo_document_first_block(const neo_document *document,
                         neo_document_block_type type)
{
    size_t i;

    if (!document)
        return NULL;

    for (i = 0U; i < document->block_count; i++) {
        if (document->blocks[i].type == type)
            return &document->blocks[i];
    }

    return NULL;
}
