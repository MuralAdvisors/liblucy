#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "../parser/parser.h"
#include "../program.h"
#include "assignment.h"
#include "compiler.h"
#include "core.h"
#include "import.h"
#include "machine.h"
#include "state.h"
#include "ts_printer.h"
#include "../error.h"

static void destroy_state(PrintState *state) {
  xs_destroy_state_refs(state);
  set_destroy(state->events);
  set_destroy(state->action_names);
  set_destroy(state->guard_names);
  set_destroy(state->delay_names);
  set_destroy(state->service_names);
}

CompileResult* xs_create() {
  CompileResult* result = malloc(sizeof(*result));
  return result;
}

void xs_init(CompileResult* result, bool use_remote_source, bool include_dts) {
  result->success = false;
  result->js = NULL;
  result->dts = NULL;
  result->flags = 0;
  
  if(use_remote_source) {
    result->flags |= XS_FLAG_USE_REMOTE;
  }
  if(include_dts) {
    result->flags |= XS_FLAG_DTS;
  }
}

void pre_compile_context (PrintState* state, Node* node, JSBuilder* jsb) {
  // pre-traverse the tree for context assignments and helper functions

  while(node != NULL) {
    unsigned short type = node->type;
    switch(type) {
      case NODE_ASSIGNMENT_TYPE: {
        Assignment* assignment = (Assignment*)node;
        if (assignment->binding_type == ASSIGNMENT_CONTEXT) {
          error_message ("Before xs_enter_assignment");
          xs_enter_assignment(state, jsb, node);
          break;
        }
      }
    }
    // Node has a child
    if (node->child) {
      pre_compile_context(state, node->child, jsb);
    } 
    
    if(node->next) {
      node = node->next;
    } else if(node->parent) {
      return;
    } else {
      node = NULL;
    }
  }

}

void compile_xstate(CompileResult* result, char* source, char* filename) {
  ParseResult *parse_result = parse(source, filename);

  if(parse_result->success == false) {
    result->success = false;
    result->js = NULL;
    return;
  }

  error_message ("After Parse.");

  Program *program = parse_result->program;
  char* xstate_specifier;
  if(result->flags & XS_FLAG_USE_REMOTE) {
    xstate_specifier = "https://cdn.skypack.dev/xstate";
  } else {
    xstate_specifier = "xstate";
  }

  JSBuilder *jsb;
  jsb = js_builder_create();

  PrintState state = {
    .flags = result->flags,
    .source = source,
    .program = program,
    .guard = NULL,
    .context = NULL,
    .action = NULL,
    .delay = NULL,
    .events = malloc(sizeof(SimpleSet)),
    .guard_names = malloc(sizeof(SimpleSet)),
    .context_names = malloc(sizeof(SimpleSet)),
    .action_names = malloc(sizeof(SimpleSet)),
    .delay_names = malloc(sizeof(SimpleSet)),
    .service_names = malloc(sizeof(SimpleSet)),
    .tsprinter = NULL,
    .cur_event_name = NULL,
    .cur_state_start = 0,
    .cur_state_end = 0,
    .in_entry = false
  };
  set_init(state.events);
  set_init(state.guard_names);
  set_init(state.context_names);  
  set_init(state.action_names);
  set_init(state.delay_names);
  set_init(state.service_names);

  if(state.flags & XS_FLAG_DTS) {
    state.tsprinter = ts_printer_alloc();
    ts_printer_init(state.tsprinter);
  }

  Node* node;
  node = program->body;

  if(node != NULL) {
    js_builder_add_str(jsb, "import { createMachine");

    if(program->flags & PROGRAM_USES_ASSIGN) {
      js_builder_add_str(jsb, ", assign");
    }
    if(program->flags & PROGRAM_USES_SEND) {
      js_builder_add_str(jsb, ", send");
    }
    if(program->flags & PROGRAM_USES_SPAWN) {
      js_builder_add_str(jsb, ", spawn");
    }

    js_builder_add_str(jsb, " } from '");

    js_builder_add_str(jsb, xstate_specifier);
    js_builder_add_str(jsb, "';\n");
    if(result->flags & XS_FLAG_DTS) {
      state.tsprinter->xstate_specifier = xstate_specifier;
    }
  }

  // pre compile for context and other
  pre_compile_context (&state, node, jsb);

  // back to the start
  node = program->body;

  error_message ("Before Traversing Tree.");
  bool exit = false;
  while(node != NULL) {
    unsigned short type = node->type;

    if(exit) {
      switch(type) {
        case NODE_STATE_TYPE: {
          error_message ("State Node.");
          xs_exit_state(&state, jsb, node);
          node_destroy_state((StateNode*)node);
          break;
        }
        case NODE_TRANSITION_TYPE: {
          error_message ("Transition Node.");
          node_destroy_transition((TransitionNode*)node);
          break;
        }
        case NODE_ASSIGNMENT_TYPE: {
          error_message ("Assignment Node."); 
          node_destroy_assignment((Assignment*)node);
          break;
        }
        case NODE_IMPORT_SPECIFIER_TYPE: {
          error_message ("Specifier Node.");
          node_destroy_import_specifier((ImportSpecifier*)node);
          break;
        }
        case NODE_IMPORT_TYPE: {
          error_message ("Import Node.");
          node_destroy_import((ImportNode*)node);
          break;
        }
        case NODE_MACHINE_TYPE: {
          error_message ("Machine Node.");
          xs_exit_machine(&state, jsb, node);
          node_destroy_machine((MachineNode*)node);
          break;
        }
        case NODE_INVOKE_TYPE: {
          error_message ("Invoke Node.");
          node_destroy_invoke((InvokeNode*)node);
          break;
        }
        default: {
          error_message ("Unrecognized Node Type.");
          printf("Node type %hu not torn down (this is a compiler bug)\n", type);
          break;
        }
      }
    } else {
      switch(type) {
        case NODE_IMPORT_TYPE: {
          xs_enter_import(&state, jsb, node);
          break;
        }
        case NODE_STATE_TYPE: {
          xs_enter_state(&state, jsb, node);
          break;
        }
        case NODE_TRANSITION_TYPE: {
          break;
        }
        case NODE_ASSIGNMENT_TYPE: {
          error_message ("Before xs_enter_assignment");
          xs_enter_assignment(&state, jsb, node);
          break;
        }
        case NODE_INVOKE_TYPE: {
          break;
        }
        case NODE_LOCAL_TYPE: {
          // Do not walk down local nodes.
          exit = true;
          node_destroy_local((LocalNode*)node);
          break;
        }
        case NODE_MACHINE_TYPE: {
          xs_enter_machine(&state, jsb, node);
          break;
        }
      }
    }

    // Node has no children, so go again for the exit.
    if(!exit && !node->child) {
      exit = true;
      continue;
    }
    // Node has a child
    else if(!exit && node->child) {
      node = node->child;
    } else if(node->next) {
      exit = false;
      node = node->next;
    } else if(node->parent) {
      exit = true;
      node_destroy(node);
      node = node->parent;
    } else {
      // Reached the end.
      node_destroy(node);
      break;
    }
  }

  char* js = js_builder_dump(jsb);
  result->success = true;
  result->js = js;

  if(result->flags & XS_FLAG_DTS) {
    char* dts = ts_printer_dump(state.tsprinter);
    result->dts = dts;
    ts_printer_destroy(state.tsprinter);
  }

  // Teardown
  destroy_state(&state);
  js_builder_destroy(jsb);

  return;
}

char* xs_get_js(CompileResult* result) {
  return result->js;
}

char* xs_get_dts(CompileResult* result) {
  return result->dts;
}

void destroy_xstate_result(CompileResult* result) {
  if(result->js != NULL) {
    free(result->js);
  }
  if(result->dts != NULL) {
    free(result->dts);
  }
  free(result);
}