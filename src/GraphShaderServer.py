#!/usr/bin/env python3
"""

"""

from __future__ import annotations
from dataclasses import dataclass
import logging
import logging.config
from pathlib import Path
from subprocess import run, PIPE, DEVNULL
from base64 import b64decode
from threading import Lock

from flask import Flask, request
from flask_cors import CORS


#--- Custom Logging

class TidyRequestFormatter(logging.Formatter):
    def format(self, record):
        from urllib.parse import urlsplit, urlunsplit, parse_qsl, urlencode
        from hashlib import md5

        print(f'{record=!r}')

        if hasattr(record, "url"):
            url = record.url

            # Split the URL into components
            scheme, netloc, path, query, fragment = urlsplit(url)
            query = parse_sql(query)

            for i, (k, v) in enumerate(query[:]):
                print(f'{k=!r}: {v=!r}')
                if k == 'shader':
                    v = v.encode('utf-8')
                    v = md5(v).hexdigest()
                    query[i] = (k, v)

            query = urlencode(query)
            url = urlunsplit((scheme, netloc, path, query, fragment))
            
            record.url = url

        return super().format(record)

logging.config.dictConfig({
    'version': 1,
    'formatters': {'default': {
        'format': '[%(asctime)s] %(levelname)s in %(module)s: %(message)s',
    }},
    'handlers': {'wsgi': {
        'class': 'logging.StreamHandler',
        'stream': 'ext://flask.logging.wsgi_errors_stream',
        'formatter': 'default'
    }},
    'root': {
        'level': 'INFO',
        'handlers': ['wsgi']
    }
})


#--- Global Variables

app = Flask(__name__)
CORS(app)

_g_index_path: Path = None
_g_graph_shader_transpiler_executable: str = None
_g_graph_shader_engine_executable: str = None
_g_lock: Lock = Lock()
_g_shader_path: Path = None


#--- Graph Shader Classes

@dataclass
class RenderingResponse:
    info: Any
    jpeg: bytes


@dataclass
class RenderingRequest:
    w: int
    h: int
    z: float
    x: float
    y: float
    shader: bytes

    def __call__(self):
        with _g_lock:
            process = run([
                '/usr/bin/env',
                _g_graph_shader_transpiler_executable,
                '-x', _g_graph_shader_engine_executable,
                '-i', '/dev/stdin',
                '-e', 'GS_OUTPUT', '/dev/stdout',
                '-e', 'GS_TILE_Z', str(self.z),
                '-e', 'GS_TILE_X', str(self.x),
                '-e', 'GS_TILE_Y', str(self.y),
                '-e', 'GS_TILE_WIDTH', str(self.w),
                '-e', 'GS_TILE_HEIGHT', str(self.h),
            ], input=self.shader, stderr=DEVNULL, stdout=PIPE)

            output = process.stdout

        index = output.find(b'\xFF\xD8\xFF')
        info, jpeg = output[:index], output[index:]

        return RenderingResponse(
            info=info,
            jpeg=jpeg,
        )


#--- Routing

@app.route('/')
def index():
    with open(_g_index_path, 'rt') as f:
        return f.read(), 200, { 'Content-Type': 'text/html' }


@app.route('/graph/<int:z>/<int:x>/<int:y>/<int:w>x<int:h>.jpg')
def graph(z, x, y, w, h):
    shader: 'base64' = request.args.get('shader')
    shader: bytes = b64decode(shader)

    req = RenderingRequest(
        w=w,
        h=h,
        z=z,
        x=x,
        y=y,
        shader=shader,
    )

    res = req()

    return res.jpeg, 200, { 'Content-Type': 'image/jpeg' }


#--- Main Entrypoint

def main(
    *,
    graph_shader_transpiler_executable: str,
    graph_shader_engine_executable: str,
):
    global _g_index_path
    _g_index_path = Path(__file__).parent / 'index.html'

    global _g_graph_shader_transpiler_executable
    _g_graph_shader_transpiler_executable = graph_shader_transpiler_executable

    global _g_graph_shader_engine_executable
    _g_graph_shader_engine_executable = graph_shader_engine_executable

    # from werkzeug._internal import _log
    # _log('info', 'dummy')

    # logger = logging.getLogger('werkzeug')
    # formatter = TidyRequestFormatter(
    #     "[%(asctime)s] %(levelname)s in %(module)s: %(message)s"
    # )
    # for handler in logger.handlers:
    #     handler.setFormatter(formatter)

    app.run(host='0.0.0.0', port=8080, debug=False)


def cli():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('--gst-executable', dest='graph_shader_transpiler_executable', default='GraphShaderTranspiler.py')
    parser.add_argument('--gse-executable', dest='graph_shader_engine_executable', default='GraphShaderEngine')
    args = vars(parser.parse_args())

    main(**args)


if __name__ == '__main__':
    cli()
