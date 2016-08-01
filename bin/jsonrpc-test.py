#!/usr/bin/env python
# coding: utf-8

from   pprint import pprint
import pyjsonrpc

http_client = pyjsonrpc.HttpClient(
    url = "http://localhost:8080",
    #username = "Username",
    #password = "Password"
)

#try:
    #print http_client.call("a2")
    ## Result: 3
#except:
    #pass

# It is also possible to use the *method* name as *attribute* name.
#print http_client.add(1, 2)

result = http_client.call( "call", "list jobs last" )
try:
    pprint(result['result'])
except:
    pprint( result )

#print http_client.add(2,"abc")
