%{

#include "GdbMI.h"

namespace GdbMI {

%}

%require "3.4"
%define api.pure full
%define api.push-pull push

/* Provisional data type until I read more about the bison c++ variant. */
%union {
	int			nToken; // token type
	int			nValue; // integer literal
	QString qs;     // either an identifier or quoted literal
}

/* These tokens will have to be defined by reflex/flex/whatever. */
%token <qs>      IDENTIFIER
%token <qs>      QUOTEDSTRING
%token <nToken>  NUMTOKEN
%token <nValue>  QUOTEDNUM

/* The END token is the only explicit string literal, and we have two explicit character token types. */
%token END       "(gdb)"
%token LF        '\n'
%token CR				 '\r'

%%
output: oob_record_seq END nl
| oob_record_seq result_record END nl

line_input: result_record
| oob_record;

result_record:
  NUMTOKEN '^' result_class ',' result_list nl;

result_list: result
| result "," result_list;

oob_record_seq:	%empty
| oob_record nl
| oob_record nl oob_record_seq

oob_record: async_record
| stream_record;

async_record: exec_async_output
| status_async_output
| notify_async_output;

exec_async_output: t_exec_async_output
| u_exec_async_output;

t_exec_async_output: NUMTOKEN '*' async_output nl;

u_exec_async_output: '*' async_output nl;

status_async_output: t_status_async_output
| u_status_async_output;

t_status_async_output: NUMTOKEN '+' async_output nl;

u_status_async_output: '+' async_output nl;

notify_async_output: t_notify_async_output
| u_notify_async_output;

t_notify_async_output: NUMTOKEN '=' notify_async_output nl;

u_notify_async_output: '=' async_output nl;

notify_async_output:
| '=' thread_group_added
| '=' thread_group_removed
| '=' thread_group_exited
| '=' thread_group_started
| '=' thread_started
| '=' thread_exited
| '=' thread_created
| '=' thread_selected
| '=' library_loaded
| '=' library_unloaded
| '=' traceframe_changed
| '=' tsv_created
| '=' tsv_deleted
| '=' tsv_modified
| '=' breakpoint_created
| '=' breakpoint_modified
| '=' breakpoint_deleted
| '=' record_started
| '=' record_stopped
| '=' cmd_param_changed
| '=' memory_changed;

thread_group_added: "thread-group-added" "," result_list;

thread_group_removed: "thread-group-removed" "," result_list;

thread_group_exited: "thread-group-exited" "," result_list;

thread_group_started: "thread-group-started" "," result_list;

thread_started: "thread-started" "," result_list;

thread_exited: "thread-exited" "," result_list;

thread_selected: "thread-selected" "," result_list;

library_loaded: "library-loaded" "," result_list;

library_unloaded: "library-unloaded" "," result_list;

traceframe_changed: "traceframe-changed" "," result_list;

tsv_created: "tsv-created" "," result_list;

tsv_deleted: "tsv-deleted" "," result_list;

tsv_modified: "tsv-modified" "," result_list;

breakpoint_created: "breakpoint-created" "," result_list;

breakpoint_modified: "breakpoint-modified" "," result_list;

breakpoint_deleted: "breakpoint-deleted" "," result_list;

record_started: "record-started" "," result_list;

record_stopped: "record-stopped" "," result_list;

cmd_param_changed: "cmd-param-changed" "," result_list;

memory_changed: "memory-changed" "," result_list;

async_output: async_class ',' result_list;

verb: "created"
| "deleted"
| "exited"
| "modified"
| "changed"
| "started"
| "stopped"
| "loaded"
| "unloaded"
| "selected"
| "added"
| "removed";

result_class: "done"
| "running"
| "connected"
| "error"
| "exit";

async_class: "stopped"
| IDENTIFIER;

result: variable '=' value;

variable: IDENTIFIER;

value: const
| tuple
| list;

const: CSTRING;

tuple: "{}"
| "{" result_list "}";

list: "[]"
| "[" value "]"
| "[" value_list "}";

value_list: value
| value "," value_list;

stream_record: console_stream_output
| target_stream_output
| log_stream_output;

console_stream_output: "~" CSTRING nl;

target_stream_output: "@" CSTRING nl;

log_stream_output: "&" CSTRING nl;

nl: CR | CR LF

%%


%{

} // namespace GdbMI

%}
