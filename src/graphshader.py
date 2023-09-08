#!/usr/bin/env python3
from __future__ import annotations
import ctypes
import os
import typing
import dataclasses
import contextlib

import numpy as np


__all__ = [
    'load_library',
]

def load_library(name: str, /) -> ctypes.CDLL:
    lib = ctypes.CDLL(
        name,
        mode=os.RTLD_LOCAL | os.RTLD_DEEPBIND,
    )

    def Declare(name: str, restype: type, /, *argtypes: type):
        getattr(lib, name).restype = restype
        getattr(lib, name).argtypes = argtypes
    
    def DeclareInitDone(init_name: str, done_name: str, /):
        Declare(init_name, None,
            ctypes.c_void_p,
        )
        Declare(done_name, None,
            ctypes.c_void_p,
        )
    
    Declare('gsNewApplication', ctypes.c_void_p,
    )

    DeclareInitDone('gsEnvironmentInit', 'gsEnvironmentDone')
    Declare('gsEnvironmentSet', None,
        ctypes.c_void_p,
        ctypes.c_char_p,
        ctypes.c_char_p,
    )

    DeclareInitDone('gsEGLInit', 'gsEGLDone')
    DeclareInitDone('gsGLInit', 'gsGLDone')

    Declare('gsDataSet', None,
        ctypes.c_void_p,
        ctypes.c_char_p,
        ctypes.c_void_p,
        ctypes.c_size_t,
    )
    DeclareInitDone('gsDataInit', 'gsDataDone')

    DeclareInitDone('gsShaderInit', 'gsShaderDone')

    DeclareInitDone('gsRenderInit', 'gsRenderDone')
    Declare('gsRenderGet', None,
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_void_p),
        ctypes.POINTER(ctypes.c_size_t),
    )

    return lib


@dataclasses.dataclass
class GS:
    lib: ctypes.CDLL
    app: ctypes.c_void_p

    @classmethod    
    def load(cls: typing.Self, name: str, /) -> typing.Self:
        lib = load_library(name)
        app = lib.gsNewApplication()
        return cls(lib, app)
    
    def __setitem__(self: typing.Self, name: str, value: str | bytes, /) -> typing.Literal[None]:
        name = name.encode('ascii')
        
        if isinstance(value, str):
            value = value.encode('ascii')
            self.lib.gsEnvironmentSet(self.app, name, value)

        elif isinstance(value, np.ndarray):
            buffer = value.ctypes.data_as(ctypes.c_void_p)
            buflen = value.nbytes
            self.lib.gsDataSet(self.app, name, buffer, buflen)
        
        elif isinstance(value, bytes):
            self.lib.gsDataSet(self.app, name, value, len(value))

        else:
            raise NotImplementedError(f"{type(value)=!r}")

    @contextlib.contextmanager
    def environment(self: typing.Self, /):
        self.lib.gsEnvironmentInit(self.app)
        yield self
        self.lib.gsEnvironmentDone(self.app)

    @contextlib.contextmanager
    def egl(self: typing.Self, /):
        self.lib.gsEGLInit(self.app)
        yield self
        self.lib.gsEGLDone(self.app)

    @contextlib.contextmanager
    def gl(self: typing.Self, /):
        self.lib.gsGLInit(self.app)
        yield self
        self.lib.gsGLDone(self.app)

    @contextlib.contextmanager
    def data(self: typing.Self, /):
        self.lib.gsDataInit(self.app)
        yield self
        self.lib.gsDataDone(self.app)

    @contextlib.contextmanager
    def shader(self: typing.Self, /):
        self.lib.gsShaderInit(self.app)
        yield self
        self.lib.gsShaderDone(self.app)

    @contextlib.contextmanager
    def render(self: typing.Self, /) -> iter[bytes]:
        self.lib.gsRenderInit(self.app)

        jpeg = ctypes.c_void_p(0)
        jpeglen = ctypes.c_size_t(0)
        self.lib.gsRenderGet(self.app, ctypes.byref(jpeg), ctypes.byref(jpeglen))

        jpeg = ctypes.string_at(jpeg.value, jpeglen.value)
        yield jpeg

        self.lib.gsRenderDone(self.app)


def main():
    gs = GS.load('build/libgraphshader.so')
    with gs.environment():
        for k, v in os.environ.items():
            gs[k] = v
        
        with gs.egl():
            with gs.gl():
                with gs.data():
                    with gs.shader():
                        with gs.render():
                            pass


if __name__ == '__main__':
    main()
