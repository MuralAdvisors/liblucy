#pragma once

#include "../node.h"
#include "../state.h"

int parser_consume_inline_context(State*, TransitionNode*);
int parser_consume_context(State*);