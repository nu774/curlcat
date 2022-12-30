# curlcat

netcat for HTTP/1.1, based on libcurl.

# options

```
-h, --help              print help message
-X, --request <method>  HTTP method
-H, --header <header>   HTTP header
-k, --insecure          don't verify SSL certificate
-v, --verbose           write headers to stderr
-u, --unbuffered        disable stdout/stderr buffering
--json                  shorthand for -H "Content-Type: application/json"
```

Option -X, -H and -z are the same as curl. Other than that, lots of curl's options are not implemented.   

Unlike curl, but similarly to netcat, curlcat always reads request body from stdin when PATCH/POST/PUT method is specified with -X.

Also, curlcat supports full-duplex streaming correctly. You can test it with HTTP echo server such as:  
 https://github.com/nu774/httpechoserver-go

It's unfortunate that curl (used by everybody) doesn't seem to support full-duplex HTTP streaming properly. By curl, you can communicate with a HTTP echo server like this:
```
$ curl -sNT- http://server:port/path
```
You enter a line, curl sends them to the server, then the server immediately echo backs. However, you will see that curl doesn't print server's resposene until you enter a next line.

For this to work correctly, we need two pipelines (stdin to server, and server to stdout) to work concurrently. In other words, we need either two threads or some kind of I/O multiplex mechanism.

Thanks to libcurl's multi interface, we can easily multiplex stdin and libcurl's easy handles, and this is how curlcat is implemented.
