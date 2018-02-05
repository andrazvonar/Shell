# Shell
This was a homework assignment for my Operating systems course at FRI, University of Ljubljana.

## Functionality
This shell implements some of the most common UNIX shell commands such as:
* dirmake (mkdir)
* remove (rm)
* linkhard (ln)
* linksoft (ln -l)
* ...

But also executes UNIX shell commands such as (ln, cp, cat, echo, ...)

### Background execution
Commands can be executed in the background by appending `&` at the end of the line

### Input and output redirection
Input and output can be redirected with `<` and `>` at the end of the line.

### Pipeline execution
Pipeline execution can be achieved with the `pipes` command.
For example: `pipes "cat /etc/passwd" "cut -d: -f7" "sort" "uniq -c"`
