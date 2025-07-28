#!/bin/bash

pushd build && cmake .. && make \
    && cp ../Config/Test/bgp-config-test.json ./bgp-config-test.json \
    && ./BgpConfigApi \
            --config ./bgp-config-test.json \
            --schema ../Config/Schemas/bgp-main-config.json \
            --address localhost \
            --port 8001 \
            --birdc "/opt/podman/bin/podman exec -it bird birdc" \
            --target "./bird.conf" \
    && popd
