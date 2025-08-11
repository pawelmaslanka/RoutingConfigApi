echo "Startup config"
curl -s -X GET http://localhost:8001/config/running \
   -H 'Content-Type: application/json' | jq

SESSION_TOKEN="HelloWorld!"
echo "Create new session token"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/session/token \
   -H 'Content-Type: application/text' \
   -d ${SESSION_TOKEN}`

if [ ${HTTP_STATUS} -eq 201 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Get diff of valid config - change router-id"
JSON_DIFF_OUTPUT=`curl -s -X GET http://localhost:8001/config/running/diff \
   -H 'Content-Type: application/json' \
   -d '
{
  "router-id": "127.0.0.1",
  "bgp": {
    "sessions": {
      "peer1": {
        "peer": {
          "address": {
            "range": "192.0.2.0/29"
          },
          "as": "external",
          "port": 179
        },
        "address-family": {
          "ipv4": {
            "next-hop-self": true,
            "policy-in": "MAIN_POLICY",
            "policy-out": "OUT_POLICY"
          }
        },
        "local": {
          "address": "192.0.2.2",
          "as": 65000
        },
        "ibgp": {}
      },
      "peer2": {
        "peer": {
          "address": "192.0.2.3",
          "as": 65200
        },
        "address-family": {
          "ipv6": {
            "policy-in": "SUB_POLICY",
            "policy-out": "OUT_POLICY"
          }
        },
        "local": {
          "address": "2001:db8:abcd:0012:0000:0000:0000:0001",
          "as": 65000
        },
        "ibgp": {}
      },
      "peer3": {
        "peer": {
          "link-local": {
            "address": "fe80::1",
            "interface": "eth0"
          },
          "as": 65201
        },
        "address-family": {
          "ipv6": {
            "policy-in": "SUB_POLICY",
            "policy-out": "OUT_POLICY"
          }
        },
        "local": {
          "address": "2001:db8:abcd:0012:0000:0000:0000:0001",
          "as": 65000
        },
        "ibgp": {}
      },
      "peer4": {
        "peer": {
          "address": "2001::1",
          "as": 65201
        },
        "address-family": {
          "ipv6": {
            "policy-in": "SUB_POLICY",
            "policy-out": "OUT_POLICY"
          }
        },
        "local": {
          "address": "2001:db8:abcd:0012:0000:0000:0000:0001",
          "as": 65000
        },
        "ebgp": {
          "multihop": {
            "ttl": 4
          }
        }
      },
      "peer5": {
        "peer": {
          "link-local": {
            "address": "fe80::1",
            "interface": "eth0"
          },
          "as": 65201
        },
        "address-family": {
          "ipv6": {
            "policy-in": "SUB_POLICY",
            "policy-out": "OUT_POLICY"
          }
        },
        "local": {
          "address": "2001:db8:abcd:0012:0000:0000:0000:0001",
          "as": 65000
        },
        "ibgp": {}
      },
      "peer6": {
        "peer": {
          "address": "3001::2",
          "as": 65201
        },
        "address-family": {
          "ipv6": {
            "policy-in": "SUB_POLICY",
            "policy-out": "OUT_POLICY"
          }
        },
        "local": {
          "address": "2001:db8:abcd:0012:0000:0000:0000:0001",
          "as": 65000
        },
        "ebgp": {
          "multihop": {}
        }
      }
    },
    "policy-list": {
      "MAIN_POLICY": {
        "term-10": {
          "if-match": {
            "ext-community-eq": [ "65535:4294967295:4294967295" ],
            "net-eq": {
              "prefix-v6": {
                "2001:db8::/32": {}
              }
            },
            "match-type": "ALL"
          },
          "then": {
            "community-remove": [ "65001:125", "65001:126" ],
            "action": "call-next"
          }
        },
        "term-20": {
          "if-match": {
            "as-path-in": [ 7000, 8000 ],
            "large-community-eq": [ "65535:4294967295:4294967295" ],
            "net-in": {
              "prefix-v6": {
                "::/0": {},
                "::1/128": {}
              }
            },
            "match-type": "ANY"
          },
          "then": {
            "action": "permit",
            "local-preference-set": 200,
            "community-add": "65000:124"
          }
        },
        "term-30": {
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
        "term-40": {
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
            "action": "deny"
          }
        }
      },
      "SUB_POLICY": {
        "term-10": {
          "if-match": {
            "community-in": [ "65000:100", "65001:101" ]
          },
          "then": {
            "action": "permit",
            "med-set": 100
          }
        }
      },
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
    },
    "prefix-v4-list": {
      "LOCAL_PREFIXES": {
        "10.0.0.0/8": {},
        "192.168.0.0/16": { "ge": 20 },
        "172.168.0.0/18": {
          "ge": 19,
          "le": 22
        },
        "192.0.2.0/24": {
          "le": 32
        }
      }
    },
    "prefix-v6-list": {
      "LOCAL_PREFIXES_V6": {
        "2001:db8::/32": {
          "ge": 32
        }
      }
    },
    "community-list": {
      "STANDARD_COMMS": [ "65010:1000", "65010:2000" ],
      "TEST_COMMS": [ "65110:2200" ],
      "TEST_COMMUNITY_LIST": [ "65010:4000", "65010:3000" ]
    },
    "ext-community-list": {
      "EXT_COMMS": [ "65000:123:456", "65000:123:456" ]
    },
    "large-community-list": {
      "LARGE_COMMS": [ "4294967295:4294967295:4294967295" ]
    },
    "as-path-list": {
      "STRICT_AS_PATH": [ 65002 ],
      "EXTERNAL_AS_PATH": [ 65003, 65003, 65003 ]
    }
  },
  "static": {
    "route": {
      "ipv4": {
        "10.10.10.0/24": {
          "next-hop": "blackhole"
        },
        "1.1.1.0/24": {
          "next-hop": {
            "1.1.2.1":{}
          }
        },
        "2.2.2.0/24": {
          "ifname": "eth0"
        },
        "3.3.3.0/24": {
          "next-hop": {
            "1.1.2.1": {
              "ifname": "eth0",
              "onlink": true
            }
          }
        },
        "33.33.33.0/24": {
          "next-hop": {
            "1.1.2.1": {
              "ifname": "eth0",
              "onlink": true
            }
          }
        },
        "4.4.4.0/24": {
          "next-hop": {
            "1.1.2.1": {
              "ifname": "eth0",
              "onlink": true
            },
            "2.2.2.1": {
              "ifname": "eth0",
              "onlink": true
            }
          }
        }
      },
      "ipv6": {
        "2001:db8::/32": {
          "next-hop": {
            "1.1.2.1": {},
            "2.2.2.1": {}
          }
        }
      }
    }
  }
}' | jq`

JSON_DIFF_EXPECTED='[
  {
    "op": "replace",
    "path": "/router-id",
    "value": "127.0.0.1"
  }
]'

JSON_DIFF_EXPECTED=`echo $JSON_DIFF_EXPECTED | jq "."`

if test "${JSON_DIFF_OUTPUT}" = "${JSON_DIFF_EXPECTED}"; then
    echo "Successfully received expected JSON diff"
else
    echo "Unexpected JSON diff"
    echo "Received:"
    echo "${JSON_DIFF_OUTPUT}"
    echo "Expected:"
    echo "${JSON_DIFF_EXPECTED}"
fi

echo "Post update good config without session token"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X PATCH http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -d '
[
  {
    "op": "replace",
    "path": "/router-id",
    "value": "193.0.2.2"
  }
]'`

if [ ${HTTP_STATUS} -eq 499 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Post update good config with bad session token"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X PATCH http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer INVALID_TOKEN" \
   -d '
[
  {
    "op": "replace",
    "path": "/router-id",
    "value": "193.0.2.2"
  }
]'`

if [ ${HTTP_STATUS} -eq 498 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Get latest 1 log message"
curl -s -X GET http://localhost:8001/logs/latest/1 \
   -H 'Content-Type: application/json'

echo "Post update good config [1]"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X PATCH http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d '
[
  {
    "op": "replace",
    "path": "/router-id",
    "value": "192.0.2.22"
  }
]'`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Apply candidate config"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X PUT http://localhost:8001/config/candidate \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d ''`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Get updated running config"
curl -s -X GET http://localhost:8001/config/running \
   -H 'Content-Type: application/json' | jq

echo "Get candidate config which should be empty"
curl -s -X GET http://localhost:8001/config/candidate \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -H 'Content-Type: application/json' | jq

echo "Delete current session token"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X DELETE http://localhost:8001/session/token \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d ''`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Check rollback capability"

SESSION_TOKEN="HelloWorld2!"
echo "Create next new session token"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:8001/session/token \
   -H 'Content-Type: application/text' \
   -d ${SESSION_TOKEN}`

if [ ${HTTP_STATUS} -eq 201 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Post update good config [2]"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X PATCH http://localhost:8001/config/running/update \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d '
[
  {
    "op": "replace",
    "path": "/router-id",
    "value": "192.1.2.3"
  }
]'`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

curl -s -X GET http://localhost:8001/config/candidate \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -H 'Content-Type: application/json' | jq

echo "Rollback the changes"
HTTP_STATUS=`curl -s -o /dev/null -w "%{http_code}" -X DELETE http://localhost:8001/config/candidate \
   -H 'Content-Type: application/json' \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -d ''`

if [ ${HTTP_STATUS} -eq 200 ] 
then 
    echo "Successfully processed the request" 
else 
    echo "Failed to process the request (${HTTP_STATUS})"
    exit 1
fi

echo "Get candidate config after rollback the changes"
HTTP_RETURN_DATA=`curl -s -X GET http://localhost:8001/config/candidate \
   -H "Authorization: Bearer ${SESSION_TOKEN}" \
   -H 'Content-Type: application/json'`

if test "${HTTP_RETURN_DATA}" != "Failed"; then
    echo "The candidate config should be pruned after rollback operation"
    exit 1
fi

echo "Get running config which should be untouched with the previous candidate changes"
curl -s -X GET http://localhost:8001/config/running \
   -H 'Content-Type: application/json' | jq

echo "Successfully passed the test!"
