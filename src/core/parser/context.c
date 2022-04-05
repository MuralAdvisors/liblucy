#include "../error.h"
#include "../node.h"
#include "../state.h"
#include "core.h"
#include "token.h"
#include "string.h"

static int consume_inline_context_args(State* state, void* expr, int token, char* arg, int _argi) {
  ContextExpression* context_expression = (ContextExpression*)expr;
  error_message ("Inside consume_inline_context_args.");
  
/*   if(token == TOKEN_STRING) {
    IdentifierExpression* ref = node_create_identifierexpression();
    ref->name = arg;
    context_expression->value = (Expression*)ref;
  } else if(token == TOKEN_INTEGER) {
    SymbolExpression* ref = node_create_symbolexpression();
    ref->name = arg;
    context_expression->value = (Expression*)ref;
  } else {
    error_msg_with_code_block_dec(state, state->token_len, "Unexpected function argument to context()");
    return 1;
  } */

  return 0;
}

int parser_consume_inline_context(State* state, TransitionNode* transition_node) {

  error_message ("Inside consume_inline_context.");

  int err = 0;
/*   ContextExpression* context_expression = node_create_contextexpression();
  
  _check(consume_call_expression(state, "context", context_expression, &consume_inline_context_args)); */

  // TransitionContext* context = node_transition_add_context(transition_node, NULL);
  // context->expression = context_expression;

  // if(context->expression->ref->type == EXPRESSION_SYMBOL) {
  //   MachineNode* parent_machine_node = find_closest_machine_node((Node*)transition_node);
  //   parent_machine_node->flags |= MACHINE_USES_GUARD;
  // }

  return err;
}

/* Public API */
int parser_consume_context(State* state) {

  error_message ("Inside parser_consume_context.");

  Assignment* assignment = node_create_assignment(ASSIGNMENT_CONTEXT);
  Node* node = (Node*)assignment;
  state_node_set(state, node);

  int token;

  token = consume_token(state);

  if(token != TOKEN_IDENTIFIER) {
    error_unexpected_identifier(state, node);
    return 2;
  }

  assignment->binding_name = state_take_word(state);

  token = consume_token(state);

  if(token != TOKEN_ASSIGNMENT) {
    error_msg_with_code_block(state, node, "Expected an assignment operator");
    return 2;
  }

  token = consume_token(state);
  switch(token) {
    case TOKEN_STRING: {
      ContextExpression *expression = node_create_contextexpression();
      expression->key = state_take_word(state);
      // expression->keyValue = state_take_word(state);
      assignment->value = (Expression*)expression;
      break;
    }
    case TOKEN_INTEGER: {
      ContextExpression* expression = node_create_contextexpression();
      // expression->key = strdup(assignment->binding_name);
      expression->key = state_take_word(state);
      // expression->keyValue = state_take_word(state);
      assignment->value = (Expression*)expression;
      break;
    }
    default: {
      error_msg_with_code_block_dec(state, state->token_len, "Expected an string or integer assignment");
      return 2;
    }
  }

  state_add_context(state, assignment->binding_name);
  state_node_up(state);

  return 0;
}
