FROM ubuntu:noble

ARG SHC_ARCHIVE=shc-v5.1r03
ARG WIBO_VERSION=1.2.0

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        ca-certificates \
        curl && \
    mkdir -p /opt/shc && \
    curl -fsSL "https://github.com/decompme/compilers/releases/download/compilers/${SHC_ARCHIVE}.tar.gz" -o /tmp/shc.tar.gz && \
    tar -xzf /tmp/shc.tar.gz -C /opt/shc && \
    test -f /opt/shc/bin/shc.exe && \
    curl -fsSL "https://github.com/decompals/wibo/releases/download/${WIBO_VERSION}/wibo-x86_64" -o /usr/local/bin/wibo && \
    chmod +x /usr/local/bin/wibo && \
    rm -f /tmp/shc.tar.gz && \
    rm -rf /var/lib/apt/lists/*

COPY compile.sh /usr/local/bin/compile-shc

ENTRYPOINT ["/usr/local/bin/compile-shc"]
