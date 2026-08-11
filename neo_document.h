#ifndef MATRIXCODE_NEO_DOCUMENT_H
#define MATRIXCODE_NEO_DOCUMENT_H

#include <stddef.h>

#include "neo_news.h"

/*
 * Passive document intermediate representation.
 *
 * SECURITY INVARIANT:
 * MatrixCode never interprets executable web content.  neo_document contains
 * only normalized passive data.  HTML, CSS, JavaScript, WebAssembly, SVG
 * scripting, browser plugins, iframes, forms, event handlers, remote fonts and
 * other active web technologies are outside this type system by design.
 */

#define NEO_DOCUMENT_VERSION 1U
#define NEO_DOCUMENT_MAX_BLOCKS 32U

typedef enum neo_document_block_type {
    NEO_DOC_HEADING = 0,
    NEO_DOC_LEAD,
    NEO_DOC_PARAGRAPH,
    NEO_DOC_BYLINE,
    NEO_DOC_DATELINE,
    NEO_DOC_IMAGE,
    NEO_DOC_CAPTION,
    NEO_DOC_QUOTE,
    NEO_DOC_LIST,
    NEO_DOC_TABLE,
    NEO_DOC_DIVIDER,
    NEO_DOC_METADATA
} neo_document_block_type;

typedef enum neo_document_font_role {
    NEO_DOC_FONT_BODY = 0,
    NEO_DOC_FONT_HEADLINE,
    NEO_DOC_FONT_LEAD,
    NEO_DOC_FONT_METADATA,
    NEO_DOC_FONT_CAPTION,
    NEO_DOC_FONT_MONOSPACE
} neo_document_font_role;

typedef enum neo_document_alignment {
    NEO_DOC_ALIGN_LEFT = 0,
    NEO_DOC_ALIGN_CENTER,
    NEO_DOC_ALIGN_RIGHT
} neo_document_alignment;

typedef struct neo_document_style {
    neo_document_font_role font_role;
    neo_document_alignment alignment;
    unsigned int bold;
    unsigned int italic;
} neo_document_style;

typedef struct neo_document_source_box {
    /*
     * Optional normalized source-layout hint in the range 0..1.
     * This preserves layout intent without preserving or executing CSS.
     */
    float x;
    float y;
    float width;
    float height;
    unsigned int present;
} neo_document_source_box;

typedef struct neo_document_block {
    neo_document_block_type type;
    const char *text;

    /*
     * Images are referenced only by a sanitized local asset identifier.
     * A renderer must never dereference a remote URL from this structure.
     */
    const char *asset_id;

    neo_document_style style;
    neo_document_source_box source_box;
    unsigned int source_order;
} neo_document_block;

typedef struct neo_document_source {
    const char *article_id;
    const char *origin;
    const char *provider;

    /*
     * Provenance-only metadata.  The renderer treats source_url as inert text
     * and never fetches it.
     */
    const char *source_url;
    const char *retrieved_at;
} neo_document_source;

typedef struct neo_document {
    unsigned int version;
    neo_document_source source;

    neo_document_block blocks[NEO_DOCUMENT_MAX_BLOCKS];
    size_t block_count;

    /*
     * Convenience views assembled from passive article data.  These let the
     * current renderer migrate gradually while the block model becomes the
     * canonical layout input.
     */
    const char *headline_text;
    const char *lead_text;
    const char *dateline_text;
    const char *body_text;

    /*
     * These are structural guarantees, not data supplied by the network.
     */
    unsigned int passive_only;
    unsigned int remote_resources_allowed;
    unsigned int executable_content_allowed;
} neo_document;

/*
 * Convert an already-normalized news article into a passive document.
 * No allocation, parsing, networking or code execution occurs here.
 */
int neo_document_from_article(const neo_news_article *article,
                              neo_document *document);

const neo_document_block *
neo_document_first_block(const neo_document *document,
                         neo_document_block_type type);

#endif
