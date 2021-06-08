# smallsh
Author: Mitch Campbell

## About
A project completed for OSU's CS344 Operating Systems, smallsh implements a bash-like shell. It can run abitrary commands that could otherwise be run using a typical shell.

Here it is in action

![Example of smallsh in use](/images/example.PNG)

## Instructions
### Compile
```bash
$ gcc -o smallsh main.c --std=gnu99
```

### Run
```bash
$ ./smallsh
```

### Status
to see the most recent background child process exit status
```bash
: status
```

### Exit
to exit smallsh
```bash
: exit
```