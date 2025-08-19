#!/bin/bash

mkdir -p build && pushd build && cmake .. && make \
    && cp ../Config/Test/bgp-config-test.json ./bgp-config-test.json \
    && ./RoutingConfigApi \
            --config ./bgp-config-test.json \
            --schema ../Config/Schemas/bgp-main-config.json \
            --address localhost \
            --port 8001 \
            --exec "/opt/podman/bin/podman exec -it bird birdc" \
            --target "./bird.conf" \
    && popd
