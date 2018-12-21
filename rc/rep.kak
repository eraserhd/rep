declare-option -hidden str rep_evaluate_output
declare-option -hidden str rep_namespace

define-command -hidden rep-find-namespace %{
    evaluate-commands -draft %{
        set-option buffer rep_namespace ''
        # Probably will get messed up if the file starts with comments
        # containing parens.
        execute-keys 'gkm'
        evaluate-commands %sh{
            ns=$(rep -- "(second '$kak_selection)" 2>/dev/null)
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
        try %{
            rep-find-namespace
        } catch %{
        }
        set-option global rep_evaluate_output ''
        evaluate-commands -draft %sh{
            ns="$kak_opt_rep_namespace"
            while [ $# -gt 0 ]; do
                case "$1" in
                    -namespace) shift; ns="$1";;
                esac
                shift
            done
            rep_opts=''
            if [ -n "$ns" ]; then
                rep_opts="$rep_opts -n $ns"
            fi
            eval set -- $kak_selections
            error_file=$(mktemp)
            while [ $# -gt 0 ]; do
                value=$(rep $rep_opts -- "$1" 2>"$error_file" |sed -e "s/'/''/g")
                error=$(sed "s/'/''/g" <"$error_file")
                rm -f "$error_file"
                printf "set-option -add global rep_evaluate_output '%s'\n" "$value"
                if [ -n "$error" ]; then
                    printf "fail '%s'\n" "$error"
                fi
                shift
            done
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

declare-user-mode rep
map global user e ': enter-user-mode rep<ret>'
map -docstring 'evaluate the selection in the REPL' global rep e ': rep-evaluate-selection<ret>'
map -docstring 'evaluate this file in the REPL' global rep f ': rep-evaluate-file<ret>'
