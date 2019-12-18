declare-option -hidden str rep_evaluate_output
declare-option -hidden str rep_namespace

define-command -hidden rep-find-namespace %{
    evaluate-commands -draft %{
        set-option buffer rep_namespace ''
        # Probably will get messed up if the file starts with comments
        # containing parens.
        execute-keys 'gkm'
        evaluate-commands %sh{
            ns=$(rep --port="@.nrepl-port@${kak_buffile-.}" -- "(second '$kak_selection)" 2>/dev/null)
            if [ $? -ne 0 ]; then
                printf 'fail "could not parse namespace"\n'
            else
                printf 'set-option buffer rep_namespace %s\n' "$ns"
            fi
        }
    }
}

define-command \
    -params 0.. \
    -docstring %{rep-evaluate-selection: Evaluate selected code in REPL and echo result.
Switches:
  -namespace <ns>   Evaluate in <ns>. Default is the current file's ns or user if not found.} \
    rep-evaluate-selection %{
    evaluate-commands %{
        set-option global rep_evaluate_output ''
        try %{ rep-find-namespace }
        evaluate-commands -itersel -draft %{
            evaluate-commands %sh{
                add_port() {
                    if [ -n "$kak_buffile" ]; then
                        rep_command="$rep_command --port=\"@.nrepl-port@$kak_buffile\""
                    fi
                }
                add_file_line_and_column() {
                    anchor="${kak_selection_desc%,*}"
                    anchor_line="${anchor%.*}"
                    anchor_column="${anchor#*.}"
                    cursor="${kak_selection_desc#*,}"
                    cursor_line="${cursor%.*}"
                    cursor_column="${cursor#*.}"
                    if [ $anchor_line -lt $cursor_line ]; then
                        start="$anchor_line:$anchor_column"
                    elif [ $anchor_line -eq $cursor_line ] && [ $anchor_column -lt $cursor_column ]; then
                        start="$anchor_line:$anchor_column"
                    else
                        start="$cursor_line:$cursor_column"
                    fi
                    rep_command="$rep_command --line=\"$kak_buffile:$start\""
                }
                add_namespace() {
                    ns="$kak_opt_rep_namespace"
                    while [ $# -gt 0 ]; do
                        case "$1" in
                            -namespace) shift; ns="$1";;
                        esac
                        shift
                    done
                    if [ -n "$ns" ]; then
                        rep_command="$rep_command --namespace=$ns"
                    fi
                }
                error_file=$(mktemp)
                rep_command='value=$(rep'
                add_port
                add_file_line_and_column
                add_namespace "$@"
                rep_command="$rep_command"' -- "$kak_selection" 2>"$error_file" |sed -e "s/'"'"'/'"''"'/g")'
                eval "$rep_command"
                error=$(sed "s/'/''/g" <"$error_file")
                rm -f "$error_file"
                printf "set-option -add global rep_evaluate_output '%s'\n" "$value"
                [ -n "$error" ] && printf "fail '%s'\n" "$error"
            }
        }
        echo -- "%opt{rep_evaluate_output}"
    }
}

define-command \
    -docstring %{rep-evaluate-file: Evaluate the entire file in the REPL.} \
    rep-evaluate-file %{
    evaluate-commands -draft %{
        execute-keys '%'
        rep-evaluate-selection -namespace user
    }
    echo -- "%opt{rep_evaluate_output}"
}

define-command \
    -docstring %{rep-replace-selection: Evaluate selection and replace with result.} \
    rep-replace-selection %{
    rep-evaluate-selection -namespace user
    evaluate-commands -save-regs r %{
        set-register r "%opt{rep_evaluate_output}"
        execute-keys '"rR'
    }
}

declare-user-mode rep
map global user e ': enter-user-mode rep<ret>'
map -docstring 'evaluate the selection in the REPL' global rep e ': rep-evaluate-selection<ret>'
map -docstring 'evaluate this file in the REPL'     global rep f ': rep-evaluate-file<ret>'
map -docstring 'replace selection with result'      global rep R ': rep-replace-selection<ret>'
