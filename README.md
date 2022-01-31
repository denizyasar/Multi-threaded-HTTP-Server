# Multi-threaded HTTP-Server (Linux)
A small web server in C that supports a subset of the HTTP 1.0 specifications.

**Port used:** 6789

Server takes too many requests but it can return response only 10 requests. More than 10 requests are refused. In this state, server returns a “Server is busy” message

Server accept the requests that will be html and jpeg files. The other types will be not accepting.

Returns:
- **200 OK:** request succeeded, requested object later in this message
- **301 Moved Permanently:** requested object moved, new location specified later in this message (Location:)
- **400 Bad Request:** request message not understood by server
- **404 Not Found:** requested document not found on this server

## Compiling

```cmd
gcc main.c –o main –lpthread
```

## Usage

```url
http://mydomain.com:6789/index.html
```
