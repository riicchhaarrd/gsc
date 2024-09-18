#include "script.h"
#include <core/asset.h>
#include <core/parse/lexer.h>

void asset_manager_fsm_script(struct Asset_s *asset, Stream *stream)
{
    Lexer l = {0};
    lexer_init(&l, NULL, stream);
	l.flags |= LEXER_FLAG_SKIP_COMMENTS;
    l.flags |= LEXER_FLAG_PRINT_SOURCE_ON_ERROR;
    if(setjmp(l.jmp_error))
    {
        asset->state = ASSET_STATE_FAILED;
        asset->function = NULL;
        return;
	}
	Token t;
    char str[256];
	while(!lexer_step(&l, &t))
    {
    }

	asset->state = ASSET_STATE_LOADED;
	asset->function = NULL;
}

AssetHandle load_script(const char *path)
{
    return asset_load(path, "script", asset_manager_fsm_script, NULL);
}