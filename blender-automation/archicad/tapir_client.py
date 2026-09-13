"""Minimal JSON client for Archicad's HTTP interface (official + Tapir commands).

Standalone copy of the helper used by archicad-addon/Examples/aclib so that the
Blender pipeline does not depend on the example folder layout.
"""
import json
import urllib.request


class TapirClient:
    def __init__(self, host='http://127.0.0.1', port=19723):
        self.url = f'{host}:{port}'

    def run(self, command, parameters=None):
        req = urllib.request.Request(self.url)
        req.add_header('Content-Type', 'application/json')
        body = json.dumps({'command': command, 'parameters': parameters or {}}).encode('utf8')
        with urllib.request.urlopen(req, body) as resp:
            data = json.loads(resp.read())
        if not data.get('succeeded'):
            raise RuntimeError(f'{command} failed: {json.dumps(data.get("error"), ensure_ascii=False)}')
        return data.get('result')

    def run_tapir(self, command, parameters=None):
        result = self.run('API.ExecuteAddOnCommand', {
            'addOnCommandId': {'commandNamespace': 'TapirCommand', 'commandName': command},
            'addOnCommandParameters': parameters or {},
        })
        response = result['addOnCommandResponse']
        if isinstance(response, dict) and 'error' in response:
            raise RuntimeError(f'Tapir {command} failed: {json.dumps(response["error"], ensure_ascii=False)}')
        return response
