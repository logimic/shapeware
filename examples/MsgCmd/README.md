# MsgCmd example
To build examples use:
- buildWithExamples64.bat on Win
- buildMakeWithExamples.sh on Lin
- or `-DBUILD_EXAMPLES:BOOL=true in` `cmake -g ...` command

This example demonstrates usage of MqMessageService component. MsgCmd and MsgCmdEcho instances are connected by two MqMessageServce instances providing duplex communication channel. It is demonstrated in scope of one process, but in fact both MsgCmd and MsgCmdEcho instances and their referenced MqMessageService instances can be separated to two processes as MqMessageService provides IPC.

MsgCmd instance provides a simple command line. From the command line it is possible to send messages to MsgCmdEcho instance via MqMessageService instance. MsgCmdEcho repeats the message back


## Used components

- shape::CommandLineService
- shape::CommandService
- shape::LauncherService
- shape::MqMessageService
- shape::TraceFileService
- shape::TraceFormatService
- shape::MsgCmd
- shape::MsgCmdEcho

### StartUp

cd `<buidDir>/examples/MsgCmd`
`<pathToSturtup>/startup ./configuration/config.json`

For example on Windows:
```
c:\projects\shapeware\build\VS14_2015_x64\examples\MsgCmd>..\..\bin\Debug\startup ./configuration/config.json
startup ...
Running on Shape component system https://github.com/logimic/shape

Launcher: Loading configuration file: ./configuration/config.json
Configuration directory set to: configuration
cmd> h
c         to communicate with msg. Type c h for help
h         for help
q         for quit

cmd> c h
c h            show help
c f file       send content of file
c t "text"     send text


cmd> c t "message"

cmd> received:
"message"
q
quit command invoked
```

Note on Linux use of message queues (see http://man7.org/linux/man-pages/man0/mqueue.h.0p.html) need permissions to use it. To test this example use **sudo** to start up.
