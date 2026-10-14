FROM ubuntu:noble

ARG TARGETARCH
ARG SATURN_COMPILERS_REV=af27f4aa6566c1b0fa5332b488e63d6ffd28bd48
ARG GCCSH_DIR=cygnus-2.7-96Q3-bin

RUN test "$TARGETARCH" = "amd64" && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        libbsd0 \
        libelf1t64 \
        libreadline8t64 \
        unzip \
        xfonts-utils && \
    curl -fsSL https://github.com/sozud/dosemu-deb/raw/main/fdpp_1.6-1_amd64.deb -o /tmp/fdpp.deb && \
    curl -fsSL https://github.com/sozud/dosemu-deb/raw/main/fdpp-dev_1.6-1_amd64.deb -o /tmp/fdpp-dev.deb && \
    curl -fsSL https://github.com/sozud/dosemu-deb/raw/main/comcom32_0.1~alpha3-1_all.deb -o /tmp/comcom32.deb && \
    curl -fsSL https://github.com/sozud/dosemu-deb/raw/main/dosemu2_2.0~pre9-1_amd64.deb -o /tmp/dosemu2.deb && \
    dpkg -i /tmp/fdpp.deb /tmp/fdpp-dev.deb /tmp/comcom32.deb /tmp/dosemu2.deb && \
    rm -f /tmp/*.deb && \
    mkdir -p /opt/gccsh && \
    curl -fsSL "https://github.com/sozud/saturn-compilers/archive/${SATURN_COMPILERS_REV}.zip" -o /tmp/gccsh.zip && \
    unzip -q /tmp/gccsh.zip -d /tmp/gccsh && \
    cp -a "/tmp/gccsh/saturn-compilers-${SATURN_COMPILERS_REV}/${GCCSH_DIR}/." /opt/gccsh/ && \
    test -f /opt/gccsh/GCC.EXE && \
    test -f /opt/gccsh/CPP.EXE && \
    test -f /opt/gccsh/CC1.EXE && \
    test -f /opt/gccsh/GO32.EXE && \
    rm -rf /tmp/gccsh.zip /tmp/gccsh /var/lib/apt/lists/*

COPY compile.sh /usr/local/bin/compile-sh

ENTRYPOINT ["/usr/local/bin/compile-sh"]
