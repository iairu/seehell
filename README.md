Ondrej Spanik (c) iairu.com 2022

# Program parts

## syscall.S, syscall.h

- Custom `syscall` calling using Assembly `0x80` interrupt method in `syscall.S` and interface with C using `syscall.h` alongside helpful comments regarding origin of other system calls.

## main.c

Contents of the `main` function explain the flow pretty well:

1. Process any external arguments using `processArgs`
2. Prepare the socket if requested in arguments
3. Inform the user about how the shell is ran (on socket as client or server / without socket)
4. Prepare buffer for user input
5. Interactive shell loop consisting of:
   1. Print up-to-date prompt
   2. Retrieve user input until new line
   3. If the user input contains a built-in command, execute it internally (note: no support for arguments for now as it wasn't deemed necessary)
   4. Else proceed to argument parsing for external commands / programs
   5. Handle program execution (forking, waiting, ...)
   6. Free processed arguments from dynamic memory
6. Free buffers from dynamic memory

# Additional documentation

## processArgs

External arguments handling. Defines internal behavior.

## parseArgs

Internal user input argument parsing into array.

### Special characters

| Character | Expected behavior | Implementation notes                             |
| --------- | ----------------- | ------------------------------------------------ |
| `#`       | comment           | rest of the input is ignored                     |
| `;`       | next input        |                                                  |
| `<`       |                   |                                                  |
| `>`       |                   |                                                  |
| `|`       | pipe              |                                                  |
| `\`       | escape            | literal treatment of `"`, space, `#`, others NYI |

Additional processing behavior:

- Double quotes `"` are treated as special characters unless escaped with `\`. They allow for (multiple) spaces to be inserted without the need for individual escapes.
- Spaces are allowed to be escaped for literal treatment.
- Check for matching pair of double quotes.
- Processing of `n>` (stream redirection) is not considered because it is viewed as a separate operator from `>`

# Improvement suggestions

- Major improvements are flagged with `// todo` within code 
- Minor improvements are flagged with `// _todo` within code
