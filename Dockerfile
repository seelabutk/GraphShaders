FROM ubuntu:bionic

RUN apt-get update && \
    apt-get install -y \
        cmake \
	build-essential \
	gcc \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y \
        git \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y \
        libegl1-mesa-dev \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y \
        freeglut3-dev \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y \
        libegl-mesa0 \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y \
        nouveau-firmware \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y \
        mesa-utils \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y \
        autotools-dev \
        automake \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y \
        autopoint \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y \
        libtool \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y \
        texinfo \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/libmicrohttpd
COPY libmicrohttpd-0.9.70 /opt/libmicrohttpd
RUN autoreconf --install && \
    ./configure --prefix=/app/contrib && \
    make && \
    make install

WORKDIR /opt/zorder-lib
COPY zorder-lib /opt/zorder-lib
RUN make && \
    make install PREFIX=/app/contrib

RUN apt-get update && \
    apt-get install -y \
        pkg-config \
	# vim's needed for xxd
	vim \
    && rm -rf /var/lib/apt/lists/*


WORKDIR /app
COPY Makefile /app
COPY src /app/src
ENV PKG_CONFIG_PATH=/app/contrib/lib/pkgconfig
RUN ls -lahR /app/ && make

CMD ["/app/build/render"]
