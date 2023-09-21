# RPC API

## Github branch

https://github.com/alaaeddineelamri/bareos/tree/dev/alaaeddineelamri/master/libjsonrpccpp-websockets

## Tools

You can dowlonad `wscat` and use `wscat --connect ws://localhost:<port>` to connect and execute command on the command line.

## Example commands

You can see how the commands should be formulated in the spec file (currently `core/src/dird/rpc_spec.json`).

### List client(s)

{"jsonrpc":"2.0","id":1,"method":"list_client","params":{"name":"bareos-fd"}}
{"jsonrpc":"2.0","id":1,"method":"list_clients"}


### List fileset(s)

{"jsonrpc":"2.0","id":1,"method":"list_fileset","params":{"name":"bareos-fd"}}
{"jsonrpc":"2.0","id":1,"method":"list_filesets"}

### Restore

{"jsonrpc":"2.0","id":1,"method":"restore","params":{"jobid":1,"restoreid":1,"client":"bareos-fd"}}
{"jsonrpc":"2.0","id":2,"method":"ls","params":{"offset":0,"limit":100,"path":"","restoreid":1}}
{"jsonrpc":"2.0","id":3,"method":"cd","params":{"path":"/home","restoreid":1}}
{"jsonrpc":"2.0","id":4,"method":"mark","params":{"files":["*"],"restoreid":1}}
{"jsonrpc":"2.0","id":5,"method":"done","params":{"restoreid":1}}
{"jsonrpc":"2.0","id":6,"method":"commitrestore","params":{"restoreid":1}}


## Tests

There is a systemtest called `json-rpc` that shows basic usage, along with some unit tests.

## Stub Gen

The stub generator is built during the Bareos build phase, and will be located (as can be seen in `third-party/libjsonrpccpp.cmake`) in `build/third-party/libjson-rpc-cpp/build/bin/jsonrpcstub`.

To generate the stub, you have to create/update a spec file (currently named `rpc_spec.json`), `cd` into the folder where you want to create your stub server (for the moment in `core/src/dird`) and then run the following command

`~/build/third-party/libjson-rpc-cpp/build/bin/jsonrpcstub rpc_spec.json --cpp-server=StubServer`

The parameter given to `--cpp-server=` is the name of the class, which is for now `StubServer`

Run `~/bareosprojects/build-bareos/third-party/libjson-rpc-cpp/build/bin/jsonrpcstub --help` for more options.

