

# Two execution modes

The EMP interpreter has 2 modes:
- immediate, where actions are directly performed
- VM, where the statements are compiled and then executed later 

The immediate mode is used whenever all quantities in the statement are
explicitly given. As soon as a set or condition or loop is parsed, then
the interpreter is switched to VM mode. 

The rational for it is to perform any trivial action directly, and only use
the VM mode when more complex operations needs to be performed. This keeps
the bytecode small and reduces the need to store data that would not be useful.

The drawback is that one has to be careful when switching from immediate to VM.
In particular, the daguid of the parent MP has to be stored in the VM data.
