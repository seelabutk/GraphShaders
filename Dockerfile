FROM nvidia/opengl:1.2-glvnd-runtime-ubuntu20.04

ARG NVIDIA_VISIBLE_DEVICES=all
ARG DEBIAN_FRONTEND=noninteractive
ENV NVIDIA_DRIVER_CAPABILITIES all
RUN echo "bb\b"

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
        libosmesa6-dev \
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

RUN apt-get update && \
    apt-get install -y \
        git \
        python3.8 \
        python3-pip \
        python3.8-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/pyglsl_parser
RUN git clone --recurse-submodules https://github.com/player1537-forks/pyglsl_parser.git .

WORKDIR /
RUN python3.8 -m pip --no-cache-dir install \
        cython \
    && \
    python3.8 -m pip --no-cache-dir install \
        /opt/pyglsl_parser \
        pcpp \
        jinja2

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

RUN apt-get update && \
    apt-get install -y \
        uuid-dev \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y \
        gdb \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY Makefile /app
COPY src /app/src
ENV PKG_CONFIG_PATH=/app/contrib/lib/pkgconfig
RUN ls -lahR /app/ && make

WORKDIR /opt/fgl
COPY fgl ./

WORKDIR /app

CMD ["/app/build/server"]
# CMD ["tail","-f", "/dev/null"]
