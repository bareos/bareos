# Assignment Submission

## Compiling and running tests

- compiling

![Compiling](/screenshots/compiled.png?raw=true "Compiling")

- running test

![Running_tests](/screenshots/single_test.png?raw=true "Running_tests")

- starting bareos

![Starting](/screenshots/run.png?raw=true "Starting")

## Explaining how to improve the behavior of setdebug : 
Right now the interactive set debug command doesn’t take trace value as input from the user. it rather uses the trace value existing in the context which is defined by the last trace value entered from the extended command : 

    - setdebug level=100 dir trace=1

One way to fix this issue is by prompting the user to enter a trace value the same as for the debug level and daemon.

## Finding the functions that handle the setdebug functionality

We can find the functions responsible for this behavior at the 
ua_cmds.cc file

the SetDebugCmd() static function is the entry point for the execution of the setdebug command 

in this function we get the command params: level, trace_flag, hangup_flag, timestamp_flag, and daemon either by searching for them using FindArgWithValue() function or by prompting the user to enter them using GetPint() function in case they don’t exist in the args ( for the required args: level, trace flag, and daemon )

after that we do a switch...case on the selected daemon param we got using DoPrompt() and call the appropriate functions according to the user input :
    - DoDirectorSetDebug()
    - DoClientSetDebug()
    - DoStorageSetDebug()
    - DoAllSetDebug()
  
## Behavior implementation proposal

all we need to do is instead of setting trace_flag value to -1 when there is no parameter entered from the user, we just prompt the user 
to enter it.

    - i = FindArgWithValue(ua, NT_("trace"));
    -  if (i >= 0) {
    -    trace_flag = atoi(ua->argv[i]);
    -    if (trace_flag > 0) { trace_flag = 1; }
    -  } else {
    -    //Solution goes here
    -  }
 
### Implementing the behavior and testing 

- Solution 

    - i = FindArgWithValue(ua, NT_("trace"));
    -   if (i >= 0) {
    -     trace_flag = atoi(ua->argv[i]);
    -     if (trace_flag > 0) { trace_flag = 1; }
    -   } else {
    -      if (!GetPint(ua, _("Enter new trace value: "))) { return true; }
    -      trace_flag = ua->pint32_val;
    -   }

- Testing
 
![Testing](/screenshots/testing.png?raw=true "Testing")

## Additional bugs proposals

- Backspace bug
while entering params for the interactive setdebug command, if you type a number and hit backspace the input text and value disappear.

- Incomplete log in bareos-dir.trace
a trace of the full setdebug command with inline args is thorough while a trace of the interactive setdebug command doesn’t give details about the arguments.


- missing argument’s entries
arguments such as hangup_flag and timestamp_flag aren’t mentioned while executing the setdebug command this could be resolved by adding prompts for those params in the interactive command with the possibility of skipping by pressing enter and setting them as -1. this can also be solved by adding a command similar to the “man” command for Linux which will show instructions about the setdebug command 

- timestamp value 
setting the timestamp param to 1 doesn’t add the command’s execution timestamp to the log message 


## [If there are issues with the screenshots, you'll find the same submission at this google doc](https://docs.google.com/document/d/1VCvlVHyug8k2PSue7wRwys4v91Rhw7zP_Xe4-sFaw4E/edit?usp=sharing) :

