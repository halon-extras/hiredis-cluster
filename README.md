# hiredis-cluster

This plugin is a wrapper around [hiredis-cluster](https://github.com/Nordix/hiredis-cluster). 
Hiredis-cluster is a client library for cluster deployments of the Redis database,
using [Hiredis](https://github.com/redis/hiredis).
It is used and sponsored by Ericsson.

## redisClusterCommand(...)

Run a cluster command.

### Returns

An associative array with a `reply` key or a `error` key (if an error occurred). Values are converted to HSL types, types defined in RESP2 and partly the RESP3 format is supported.

### Example usage

```
echo redisClusterCommand("GET", "foo");
```
