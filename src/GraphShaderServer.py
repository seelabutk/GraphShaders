#!/usr/bin/env python3
"""

"""

from __future__ import annotations
from pathlib import Path
from subprocess import run, PIPE, DEVNULL
from base64 import b64decode
from threading import Lock

from flask import Flask, request
from flask_cors import CORS


app = Flask(__name__)
CORS(app)

_g_index_path: Path = None
_g_graph_shader_transpiler_executable: str = None
_g_graph_shader_engine_executable: str = None
_g_lock: Lock = Lock()
_g_shader_path: Path = None


@app.route('/')
def index():
    with open(_g_index_path, 'rt') as f:
        return f.read(), 200, { 'Content-Type': 'text/html' }


@app.route('/graph/<int:z>/<int:x>/<int:y>/<int:w>x<int:h>.jpg')
def graph(z, x, y, w, h):
    shader: 'base64' = request.args.get('shader')
    shader: bytes = b64decode(shader)
    
    with _g_lock:
        process = run([
            '/usr/bin/env',
            _g_graph_shader_transpiler_executable,
            '-x', _g_graph_shader_engine_executable,
            '-i', '/dev/stdin',
            '-e', 'GS_OUTPUT', '/dev/stdout',
            '-e', 'GS_TILE_Z', str(z),
            '-e', 'GS_TILE_X', str(x),
            '-e', 'GS_TILE_Y', str(y),
            '-e', 'GS_TILE_WIDTH', str(w),
            '-e', 'GS_TILE_HEIGHT', str(h),
        ], input=shader, stderr=DEVNULL, stdout=PIPE)

        output = process.stdout

    index = output.find(b'\xFF\xD8\xFF')
    info, jpeg = output[:index], output[index:]

    return jpeg, 200, { 'Content-Type': 'image/jpeg' }


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

    app.run(host='0.0.0.0', port=8080, debug=True)


def cli():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('--gst-executable', dest='graph_shader_transpiler_executable', default='GraphShaderTranspiler.py')
    parser.add_argument('--gse-executable', dest='graph_shader_engine_executable', default='GraphShaderEngine')
    args = vars(parser.parse_args())

    main(**args)


if __name__ == '__main__':
    cli()
