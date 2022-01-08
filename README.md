# Redis cluster client plugin

This plugin is a wrapper around [hiredis-cluster](https://github.com/Nordix/hiredis-cluster). 
Hiredis-cluster is a client library for cluster deployments of the Redis database,
using [Hiredis](https://github.com/redis/hiredis).
It is used and sponsored by Ericsson.

## Installation

Follow the [instructions](https://docs.halon.io/manual/comp_install.html#installation) in our manual to add our package repository and then run the below command.

### Ubuntu

```
apt-get install halon-extras-hiredis-cluster
```

### RHEL

```
yum install halon-extras-hiredis-cluster
```

## Configuration
For the configuration schema, see [hiredis-cluster.schema.json](hiredis-cluster.schema.json). Below is a sample configuration.

**smtpd.yaml**

```
plugins:
  - id: hiredis-cluster
    config:
      pool_size: 32
      nodes: 127.0.0.1:7000,127.0.0.1:7001
```

## Exported functions

### redisClusterCommand(...)

Run a cluster command.

**Returns**

An associative array with a `reply` key or a `error` key (if an error occurred). Values are converted to HSL types, types defined in RESP2 and partly the RESP3 format is supported.

**Example**

```
echo redisClusterCommand("GET", "foo");
```
