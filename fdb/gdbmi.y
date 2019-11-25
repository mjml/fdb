%{
/*
output →
( out-of-band-record )* [ result-record ] "(gdb)" nl

result-record →
[ token ] "^" result-class ( "," result )* nl

out-of-band-record →
async-record | stream-record

async-record →
exec-async-output | status-async-output | notify-async-output

exec-async-output →
[ token ] "*" async-output nl

status-async-output →
[ token ] "+" async-output nl

notify-async-output →
[ token ] "=" async-output nl

async-output →
async-class ( "," result )*

result-class →
"done" | "running" | "connected" | "error" | "exit"

async-class →
"stopped" | others (where others will be added depending on the needs—this is still in development).

result →
variable "=" value

variable →
string

value →
const | tuple | list

const →
c-string

tuple →
"{}" | "{" result ( "," result )* "}"

list →
"[]" | "[" value ( "," value )* "]" | "[" result ( "," result )* "]"

stream-record →
console-stream-output | target-stream-output | log-stream-output

console-stream-output →
"~" c-string nl

target-stream-output →
"@" c-string nl

log-stream-output →
"&" c-string nl

nl →
CR | CR-LF

token →
any sequence of digits.
*/


%}

%require "3.4"
%define api.pure full
%define api.push-pull push

%union {
  QString qs;
	int			nToken;
}

%token <qs> IDENTIFIER
%token <qs> CSTRING
%token LF        '\n'
%token CR				 '\r'
%token <nToken> NUMTOKEN
%token END       "(gdb)"

%%
line_input: result_record
| oob_record;

result_record:
  NUMTOKEN '^' result_class "," result_list nl;

result_list: result
| result "," result_list;

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

t_notify_async_output: NUMTOKEN '=' async_output nl;

u_notify_async_output: '=' async_output nl;

async_output: async_class "," result_list;

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
