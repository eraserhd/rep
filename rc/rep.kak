declare-user-mode rep
declare-option -hidden str rep_evaluate_output

define-command \
    -docstring %{rep-evaluate-selection: Evaluate selected code in REPL and echo result.} \
    rep-evaluate-selection %{
    evaluate-commands %{
        set-option global rep_evaluate_output ''
        evaluate-commands -draft %sh{
            eval set -- $kak_selections
            error_file=$(mktemp)
            while [ $# -gt 0 ]; do
                value=$(rep "$1" 2>"$error_file" |sed -e "s/'/''/g")
                error=$(sed "s/'/''/g" <"$error_file")
                rm -f "$error_file"
                printf "set-option -add global rep_evaluate_output '%s'\n" "$value"
                if [ -n "$error" ]; then
                    printf "fail '%s'\n" "$error"
                fi
                shift
            done
        }
        echo "%opt{rep_evaluate_output}"
    }
}

define-command \
    -docstring %{rep-evaluate-file: Evaluate the entire file in the REPL.} \
    rep-evaluate-file %{
    evaluate-commands -draft %{
        execute-keys 'Z%'
        rep-evaluate-selection
        execute-keys 'z'
    }
    echo "%opt{rep_evaluate_output}"
}

map global user e ': enter-user-mode rep<ret>'
map global rep e ': rep-evaluate-selection<ret>'
map global rep f ': rep-evaluate-file<ret>'
