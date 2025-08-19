# RoutingConfigApi
Configuration management service for routing protocols with unified configuration model. With backend agents, it is possible to convert, validate and load via multiple target daemons such as BIRD or FRR.

## Motivation
### Missing features
At mid-2025 the BIRD project did not have an API remote management of configuration. Additionally there is not native tool to compare (make diff changes) of two configuration files based on BIRD-style syntax.
### Unified configuration model
I evaluated both __YANG__ and __JSON Schema__ as potential approaches for modeling the configuration syntax of routing protocols. While comprehensive configuration models are already available through the [OpenConfig](https://github.com/openconfig/public) project, my initial work was focused solely on BGP. In this context, adopting the complete set of OpenConfig BGP models and parameters would have introduced unnecessary complexity.

## Goals
### Unified configuration model
The project provides common configuration syntax for multiple routing daemons (e.g. BIRD or FRR). This capability is provided by a transaltion layer abstraction which allows to implement a converter to transform the __JSON Schema__ based configuration model into target configuration syntax. It can also handle the case where the target configuration is not stored in a flat file and can instead make single or multiple gRPC-based calls.

### Focus on limited set of routing protocol
Initially, the project aimed to support only the BGP protocol.

### Remote API for configuration management


## Architecture
### Model-based configuration
The configuration structure (syntax) is based on the JSON schema specification. Why not YANG? That was just my personal preference. Personally, I found the JSON schema-based configuration more readable and simpler.
### Network-based API
Management of configuration instance is based on the HTTP protocol. Via the built-in methods like GET, PATCH, POST AND DELETE you can manage the configuration. Why not a more modern approach like gRPC? That was just my personal preference. Personally, I found the HTTP-based API more simpler.

## Design
### Running & Candidate config
The running config is configuration instance loaded on startup. Any new changes provides to running configurtion is stored at candidate configuration instance (copy of the running configuration). If user commit changes stored at candidate configuration they become the new running configuration. There is only single instance of the running configuration and the candidate configuration. It means that only one user at time can modify and manage the candidate configuration.
### Session token
If the user want to make a new changes to the running configration, he has to acquire its own session token. The session token is just identifier of the user/session. The session token has to be send with such operations (requests) like: __send new changes__, __apply new changes__, __delete the changes__ or __delete session (token)__. By default the session token expires after 10 minutes and all changes, if any, are discarded. Then you have to obtain a new session token.

### Commit-confirm operation
The commit-confirm operation provides mechanism of a two-stage process for commit the candidate configuration. In the first step (stage) the changes are applied with some timeout. If the user send the **confirm** request before the timeout period expires, the changes will be permanent. This is second step (stage) of the process. If this timeout will expiry before the **confirm** request from the user then the changes are automatically rolled back. This is very helpful mechanism when a new changes will cut of an access to the target device then the __rollback__ mechanism should restore the __old__ working configuration.

### Translation layer
It is responsible for converting the __JSON__-based configuration format into the target-style configuration format. The abstraction layer allows to implement backend agent that converts __JSON__-based configuration into target format.

For more details, see the **IConfigConverting** interface declared in the __Source/IConfigConverting.hpp__ file. The reference implementation of this interface can be found in the __Source/BirdConfigConverter.hpp__. It converts __JSON__-based configuration to a **BIRD** configuration format.

### Backend agent for configuration management
There is provided abstraction layer that allows for implement agent responsible for validate, load and rollback target-style configuration. For example, to manage of BIRD-style configuration, the backend agent uses **birdc** program to perform these operations.

For more details, see the **IConfigExecuting** interface declared in the __Source/IConfigExecuting.hpp__ file. The reference implementation of this interface can be found in the __Source/BirdConfigExecutor.hpp__. It manages the **BIRD** daemon configuration file.

## How to?
### Step-by-step session management
1. Run program

    1.1. Build the project

    Please run the __run.sh__ script.

    1.2. Run the service

    Please refer to the program's help message:
    ```bash
    ./BgpConfigApi --help
    
        Configuration Management System
    
      OPTIONS:
    
          -h, --help                        Show this help menu
          -a[ADDRESS], --address=[ADDRESS]  The host binding address (hostname or IP
                                            address)
          -b[BIRDC], --birdc=[BIRDC]        Path to 'birdc' executable program for
                                            validation and load config purpose
          -c[CONFIG], --config=[CONFIG]     The configuration file
          -e[EXEC], --exec=[EXEC]           Path to the executable program to verify
                                            and load the config
          -s[SCHEMA], --schema=[SCHEMA]     The schema file
          -p[PORT], --port=[PORT]           The host binding port
          -t[TARGET], --target=[TARGET]     The target config file
    ```

    Let's take a closer look at the specific parameters:
    * --address=[ADDRESS] - specifies the address of the host on which the service is available
    * --config=[CONFIG] - specifies the filename (path) to the JSON based configuration file
    * --exec=[EXEC] - specifies the path to the executable program that allows validation and loading the target-style file. For instance, to validate and load the BIRD-style configuration file, you have to pass path to the **birdc** program
    * --schema=[SCHEMA] - specifies the filename (path) to the JSON schema (configuration) file. This schema models the configuration structure
    * --port=[PORT] - specifies the port number on which the service is listens for requests
    * --target=[TARGET] - specifies the filename (path) to the target configuration file. This file stores an result of translating a JSON-based configuration into the target-style configuration structure (syntax)

    1.3. Run basic test

    To quickly check capabilities of the program, please run the program and execute the __sample_request.sh__ script.
    
2. Log messages

    2.1. To get N-latest log messages, please send the following request:
    ```bash
    N_LATEST=2
    # Endpoint: logs/latest/:limit
    # HTTP method: GET
    curl -s -X GET http://localhost:8001/logs/latest/${N_LATEST} \
      -H 'Content-Type: application/json'
    ```
3. Get running configuration

    Example request:
    ```bash
    # Endpoint: config/running
    # HTTP method: GET
    curl -s -X GET http://localhost:8001/config/running \
       -H 'Content-Type: application/json'
    ```

    Example output:
    ```json
    {
      "router-id": "192.168.0.1"
    }
    ```

4. Make a changes

    4.1. Create new configuration based on the running configuration and provide to it your own changes. Let's retrieve the changes made in the new configuration:
    ```bash
    # Endpoint: config/running/diff
    # HTTP method: GET
    curl -s -X GET http://localhost:8001/config/running/diff \
      -H 'Content-Type: application/json' \
      -d '
      {
        "router-id": "127.0.0.1"
      }'
    ```

    Output:
    ```json
    [
        {
            "op": "replace",
            "path": "/router-id",
            "value": "127.0.0.1"
        }
    ]
    ```

    4.2. If everything looks good then you can create your own session token to continue operations on the remote configuration instance:

    ```bash
    SESSION_TOKEN="HelloWorld!"
    # Endpoint: session/token/create
    # HTTP method: POST
    # HTTP status code:
    #   - CREATED: 201
    #   - CONFLICT: 409
    curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/session/token/create \
      -H 'Content-Type: application/text' \
      -d ${SESSION_TOKEN}
    ```

    If the session token has been created successfully then server should return status code **201**.

    4.3. Create candidate configuration with requested changes:
    ```bash
    # Endpoint: config/running/update
    # HTTP method: PATCH
    # HTTP status code:
    #   - SUCCESS: 200
    curl -s -o /dev/null -w "%{http_code}" -X PATCH http://localhost:8001/config/running/update \
      -H 'Content-Type: application/json' \
      -d '
      [
        {
          "op": "replace",
          "path": "/router-id",
          "value": "127.0.0.1"
        }
      ]'
    ```

    4.4. You can get the candidate configuration with the requested changes:
    ```bash
    # Endpoint: config/candidate
    # HTTP method: GET
    # HTTP status code:
    #   - SUCCESS: 200
    curl -s -X GET http://localhost:8001/config/candidate \
      -H "Authorization: Bearer ${SESSION_TOKEN}" \
      -H 'Content-Type: application/json'
    ```

    Example output:
    ```json
    {
      "router-id": "127.0.0.1"
    }
    ```

54. Apply the candidate configuration

    There are two ways to apply candidate changes: immediately (one-step process) or commit-confirm (two-step process).

    5.A. Commit
    You can apply changes immediately using the following request:
    ```bash
    # Endpoint: config/candidate/commit
    # HTTP method: POST
    # HTTP status code:
    #   - SUCCESS: 200
    curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/candidate/commit \
      -H 'Content-Type: application/json' \
      -H "Authorization: Bearer ${SESSION_TOKEN}" \
      -d ''
    ```

    5.B. Commit-confirm

    5.B.1. Commit changes and specify a timeout for the user's **confirm** request:

    ```bash
    TIMEOUT=60 # a 1 minute timeout
    # Endpoint: config/candidate/commit/timeout/:timeout
    # HTTP method: POST
    # HTTP status code:
    #   - SUCCESS: 200
    curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/candidate/commit/timeout/${TIMEOUT} \
      -H 'Content-Type: application/json' \
      -H "Authorization: Bearer ${SESSION_TOKEN}" \
      -d ''
    ```

    5.B.2. Confirm or rollback the candidate configuration

    5.B.2.A. Confirm
    To permanently apply changes from your candidate configuration, please send a **confirm** request:
    ```bash
    # Endpoint: config/candidate/commit/confirm
    # HTTP method: POST
    # HTTP status code:
    #   - SUCCESS: 200
    curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/candidate/commit/confirm \
      -H 'Content-Type: application/json' \
      -H "Authorization: Bearer ${SESSION_TOKEN}" \
      -d ''
    ```

    5.B.2.B. Cancel

    If you want to revert changes before a time-out, please send the following request:

    ```bash
    # Endpoint: config/candidate/commit/cancel
    # HTTP method: POST
    # HTTP status code:
    #   - SUCCESS: 200
    curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/config/candidate/commit/cancel \
      -H 'Content-Type: application/json' \
      -H "Authorization: Bearer ${SESSION_TOKEN}" \
      -d ''
    ```

6. End a session

    To finish a session and/or remove your changes before commiting(-confirm) them, please send the following request:
    ```bash
    # Endpoint: session/token
    # HTTP method: DELETE
    # HTTP status code:
    #   - SUCCESS: 200
    curl -s -o /dev/null -w "%{http_code}" -X DELETE http://localhost:8001/session/token \
      -H 'Content-Type: application/json' \
      -H "Authorization: Bearer ${SESSION_TOKEN}" \
      -d ''
    ```

### Run a sample test
1. Build and run the service by executing the __run.sh__ script.
2. In another console window run the sample test by executing the __sample_request.sh__ script. The script will perform all the operations described in the previous section.

### Understand configuration model constructs
1. Pre-defined sets
- Autonomous System Number Path list

    Schema defined under the xpath **/bgp/as-path-list**.

    Example:
    ```json
    "as-path-list": {
      "STRICT_AS_PATH": [ 65002 ],
      "EXTERNAL_AS_PATH": [ 65003, 65003, 65003 ]
    }
    ```
    Example translation into the **BIRD** configuration syntax:
    ```console
    define STRICT_AS_PATH = [65002];
    define EXTERNAL_AS_PATH = [65003, 65003, 65003];
    ```

- Community list

    Schema defined under the xpath **/bgp/community-list**.

    Example:
    ```json
    "community-list": {
      "STANDARD_COMMS": [ "65010:1000", "65010:2000" ],
      "TEST_COMM": [ "65110:2200" ]
    }
    ```
    Example translation into the **BIRD** configuration syntax:
    ```console
    define STANDARD_COMMS = [(65010,1000),(65010,2000)];
    define TEST_COMM = (65110,2200);
    ```

    Similarly, extended and large community lists are available.

- Prefix list

    Schema defined under the xpath **/bgp/prefix-v4-list** and **/bgp/prefix-v6-list**.

    Example:
    ```json
    "prefix-v4-list": {
      "LOCAL_PREFIXES": {
        "10.0.0.0/8": {},
        "172.168.0.0/18": {
          "ge": 19,
          "le": 22
        }
      }
    },
    "prefix-v6-list": {
      "LOCAL_PREFIXES_V6": {
        "2001:cb8::/32": {
          "ge": 32
        }
      }
    }
    ```
    Example translation into the **BIRD** configuration syntax:
    ```console
    define LOCAL_PREFIXES = [
        10.0.0.0/8,
        172.168.0.0/18{19,22}
    ];
    define LOCAL_PREFIXES_V6 = [
        2001:cb8::/32{32,128}
    ];
    ```

- Policy list

    Schema defined under the xpath **/bgp/policy-list**.

    Example:
    ```json
    "policy-list": {
      "OUT_POLICY": {
        "term-10": {
          "if-match": {
            "community-in": {
              "community-list": "TEST_COMMUNITY_LIST"
            }
          },
          "then": {
            "action": "permit"
          }
        }
      }
    }
    ```

    Example translation into the **BIRD** configuration syntax:

    ```console
    filter OUT_POLICY {
        if ((bgp_community ~ TEST_COMMUNITY_LIST)) then {
            accept;
        }
        reject;
    }
    ```

2. Conditional statements

    2.1 Chained conditional statements

    To define chained conditional statements please, use the **term-XX** property in the policy node.

    Example:
    ```json
    "policy-list": {
      "MAIN_POLICY": {
        "term-10": {
          "if-match": {
            "as-path-eq": {
              "as-path-list": "STRICT_AS_PATH"
            },
            "community-eq": [ "65000:200", "65001:201" ],
            "net-in": {
              "prefix-v4-list": "LOCAL_PREFIXES"
            },
            "match-type": "ANY"
          },
          "then": {
            "community-add": {
              "community-list": "TEST_COMMS"
            },
            "as-path-prepend": {
              "asn": 65000,
              "n-times": 2
            },
            "action": "call-next"
          }
        },
        "term-20": {
          "if-match": {
            "ext-community-in": {
              "ext-community-list": "EXT_COMMS"
            },
            "net-type-eq": "ipv4",
            "net-in": {
              "prefix-v4": {
                "192.0.2.0/24": { "le": 32 }
              }
            },
            "source-protocol-eq": "BGP"
          },
          "then": {
            "community-remove": {
              "community-list": "TEST_COMMS"
            },
            "action": "permit"
          }
        }
      }
    }
    ```

    Example translation into the **BIRD** configuration syntax:

    ```console
    filter MAIN_POLICY {
        if ((bgp_path = STRICT_AS_PATH) || (bgp_community = [(65000,200),(65001,201)])) then {
            bgp_path.prepend(65000); bgp_path.prepend(65000); 
            bgp_community.add(TEST_COMMS);
        }
        if ((bgp_ext_community ~ EXT_COMMS) && (net.type = NET_IP4) && (source = RTS_BGP)) then {
            bgp_community.delete(TEST_COMMS);
            accept;
        }
        reject;
    }
    ```

    2.2 Multiple conditional statements

    If the conditional statement successfully evaluates to a logical value of **true**, then the **"then": {}** statement will be executed.

    2.3 Actions

    Actions are put in the **"then": {}** property and are executed when the conditional statement is met.


3. An individual BGP session

    Example:
    ```json
    "bgp": {
      "sessions": {
        "peer4": {
          "peer": {
            "address": "5001::1",
            "as": 65201
          },
          "address-family": {
            "ipv6": {
              "policy-in": "SUB_POLICY",
              "policy-out": "OUT_POLICY"
            }
          },
          "local": {
            "address": "2001:cb8:abcd:ef12:0000:0000:0000:0001",
            "as": 65000
          },
          "ebgp": {
            "multihop": {
              "ttl": 4
            }
          }
        }
      }
    }
    ```

    Example translation into the **BIRD** configuration syntax:

    ```console
    protocol bgp 'peer4' {
        neighbor 5001::1 as 65201;
        local 2001:cb8:abcd:ef12:0000:0000:0000:0001 as 65000;
        ipv6 {
            import filter SUB_POLICY;
            export filter OUT_POLICY;
        };
        multihop 4;
    }
    ```

4. Static route

    Example:
    ```json
    "static": {
      "route": {
        "ipv4": {
          "10.10.10.0/24": {
            "next-hop": "blackhole"
          }
        },
        "ipv6": {
          "2001:cb8::/32": {
            "next-hop": {
              "1.1.2.1": {},
              "2.2.2.1": {}
            }
          }
        }
      }
    }
    ```

    Example translation into the **BIRD** configuration syntax:

    ```console
    protocol static 'STATIC_IPv4' {
        ipv4;
        route 10.10.10.0/24 blackhole;
    }
    protocol static 'STATIC_IPv6' {
        ipv6;
        route 2001:cb8::/32 via 1.1.2.1
            via 2.2.2.1;
    }
    ```

#### Reference
A quick example of a configuration file based on the __JSON Schema__ can be found in the __Config/Test/bgp-config-test.json__ file. For the corresponding configuration format based on the __BIRD__ syntax, see the file in __Source/Demo/bird.conf__.

## Third-party dependencies
The project depends on the following external libraries:
* [args](https://github.com/Taywee/args) - A simple header-only C++ argument parser library
* [defer](https://github.com/mattkretz/defer) - Defer a function/lambda until the end of the scope
* [cpp-httplib](https://github.com/yhirose/cpp-httplib) - A C++ header-only HTTP/HTTPS server and client library
* [subprocess.h](https://github.com/sheredom/subprocess.h) - A simple one header solution to launching processes and interacting with them for C/C++

Above dependencies are put into the project source code structure (please look at __Source/Lib/ThirdParty__).

The following external libraries have to be installed (provided) manually:
* [json](https://github.com/nlohmann/json) - JSON for Modern C++
* [json-schema-validator](https://github.com/pboettch/json-schema-validator) - JSON schema validator for JSON for Modern C++
* [spdlog](https://github.com/gabime/spdlog) - Fast C++ logging library
