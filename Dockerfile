FROM ubuntu:18.04 

RUN apt-get update
RUN apt-get install -y vim git gcc g++ doxygen wget sudo curl autoconf gettext xutils-dev libdrm-dev m4 pkg-config libtool bison flex zlib1g-dev cmake libgl1-mesa-dev

RUN apt-get install -y python python-pip
RUN pip install Pillow

RUN apt-get install -y python3 python3-pip
RUN pip3 install --upgrade pip
RUN pip3 install mako lxml untangle

ENV LLVM_INSTALL_DIR /opt/llvm
RUN mkdir $LLVM_INSTALL_DIR

ENV LLVM_INSTALL_VER 9.0.0
RUN wget http://releases.llvm.org/$LLVM_INSTALL_VER/llvm-$LLVM_INSTALL_VER.src.tar.xz && tar -xf llvm-$LLVM_INSTALL_VER.src.tar.xz && rm llvm-$LLVM_INSTALL_VER.src.tar.xz
RUN wget http://releases.llvm.org/$LLVM_INSTALL_VER/cfe-$LLVM_INSTALL_VER.src.tar.xz && tar -xf cfe-$LLVM_INSTALL_VER.src.tar.xz && rm cfe-$LLVM_INSTALL_VER.src.tar.xz && mv cfe-$LLVM_INSTALL_VER.src llvm-$LLVM_INSTALL_VER.src/tools/clang
RUN wget http://releases.llvm.org/$LLVM_INSTALL_VER/lld-$LLVM_INSTALL_VER.src.tar.xz && tar -xf lld-$LLVM_INSTALL_VER.src.tar.xz && rm lld-$LLVM_INSTALL_VER.src.tar.xz && mv lld-$LLVM_INSTALL_VER.src llvm-$LLVM_INSTALL_VER.src/tools/lld
RUN cd llvm-$LLVM_INSTALL_VER.src && mkdir build && cd build && cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD=X86 -DLLVM_BUILD_LLVM_DYLIB=1 -DLLVM_LINK_LLVM_DYLIB=1 -DLLVM_ENABLE_RTTI=1 -DCMAKE_INSTALL_PREFIX=$LLVM_INSTALL_DIR/$LLVM_INSTALL_VER .. && make install -j`nproc`
RUN rm -rf llvm-$LLVM_INSTALL_VER.src
RUN ln -s $LLVM_INSTALL_DIR/$LLVM_INSTALL_VER/bin/ld.lld $LLVM_INSTALL_DIR/$LLVM_INSTALL_VER/bin/ld

RUN pip3 install meson
RUN apt-get install -y ninja-build

ARG mesa_version=20.0.4
RUN curl -L https://mesa.freedesktop.org/archive/mesa-${mesa_version}.tar.xz -o /tmp/mesa-${mesa_version}.tar.xz && \
    tar xf /tmp/mesa-${mesa_version}.tar.xz -C /opt/ && \
    rm -f /tmp/mesa-${mesa_version}.tar.xz
WORKDIR /opt/mesa-${mesa_version}

ENV MESA_BUILD_DIR "/opt/mesa"
RUN mkdir $MESA_BUILD_DIR

RUN printf "[binaries]\nllvm-config = '/opt/llvm/9.0.0/bin/llvm-config'" > config.ini
RUN meson $MESA_BUILD_DIR \
	--native-file=config.ini \
	--buildtype=release \
	-Dglx=gallium-xlib \
	-Dvulkan-drivers= \
	-Ddri-drivers= \
	-Dgallium-drivers=swrast,swr \
	-Dplatforms=x11 \
	-Dgallium-omx=disabled \
	-Dosmesa=gallium \
	-Dprefix=$MESA_BUILD_DIR
RUN ninja -C $MESA_BUILD_DIR  && \
	meson install -C $MESA_BUILD_DIR
ENV LD_LIBRARY_PATH=/opt/mesa/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH
ENV GALLIUM_DRIVER=swr

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
	vim \
	# vim's needed for xxd
    && rm -rf /var/lib/apt/lists/*


WORKDIR /app
COPY Makefile /app
COPY src /app/src
ENV PKG_CONFIG_PATH=/app/contrib/lib/pkgconfig
RUN ls -lahR /app/ && \
	make \
		INCLUDE='-I/opt/mesa/include'

CMD ["/app/build/server"]
