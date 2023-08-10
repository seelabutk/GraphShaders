FROM nvidia/opengl:1.2-glvnd-runtime-ubuntu20.04

ARG NVIDIA_VISIBLE_DEVICES=all
ARG DEBIAN_FRONTEND=noninteractive
ENV NVIDIA_DRIVER_CAPABILITIES all

RUN apt-get update && \
    apt-get install -y \
        cmake \
        build-essential \
        gcc \
        git \
        libegl1-mesa-dev \
        freeglut3-dev \
        libegl-mesa0 \
        nouveau-firmware \
        mesa-utils \
        libosmesa6-dev \
        zlib1g-dev \
        autotools-dev \
        automake \
        autopoint \
        libtool \
        texinfo \
        git \
        python3.8 \
        python3-pip \
        python3-virtualenv \
        python3.8-dev \
        pkg-config \
        # vim's needed for xxd
        vim \
        uuid-dev \
        gdb \
    && rm -rf /var/lib/apt/lists/*

# WORKDIR /opt/pyglsl_parser
# RUN git clone --recurse-submodules https://github.com/player1537-forks/pyglsl_parser.git .
# 
# WORKDIR /
# RUN python3.8 -m pip --no-cache-dir install \
#         cython \
#     && \
#     python3.8 -m pip --no-cache-dir install \
#         /opt/pyglsl_parser \
#         pcpp \
#         jinja2

# WORKDIR /opt/libmicrohttpd
# COPY libmicrohttpd-0.9.70 /opt/libmicrohttpd
# RUN autoreconf --install && \
#     ./configure --prefix=/app/contrib && \
#     make && \
#     make install

# WORKDIR /app
# COPY Makefile /app
# COPY src /app/src
# ENV PKG_CONFIG_PATH=/app/contrib/lib/pkgconfig
# RUN ls -lahR /app/ && make

# WORKDIR /opt/fgl
# COPY fgl ./

# WORKDIR /app

# CMD ["/app/build/server"]
# # CMD ["tail","-f", "/dev/null"]
