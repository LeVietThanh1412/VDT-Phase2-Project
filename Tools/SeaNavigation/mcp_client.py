"""Small serialized SpecialAgent client used for reproducible editor automation."""
import argparse
import json
import pathlib
import sys
import urllib.request

ENDPOINT = "http://127.0.0.1:8767/mcp"


def call_tool(name, arguments=None, timeout=120):
    body = json.dumps({"jsonrpc": "2.0", "id": 1, "method": "tools/call",
                       "params": {"name": name, "arguments": arguments or {}}}).encode()
    request = urllib.request.Request(ENDPOINT, data=body,
                                     headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(request, timeout=timeout) as response:
        envelope = json.load(response)
    if "error" in envelope:
        raise RuntimeError(envelope["error"])
    result = envelope["result"]
    if result.get("isError"):
        raise RuntimeError(result)
    texts = [json.loads(c["text"]) for c in result.get("content", []) if c["type"] == "text"]
    return texts[0] if len(texts) == 1 else texts


def execute(code):
    result = call_tool("python/execute", {"code": code})
    if result.get("stdout"):
        print(result["stdout"], end="")
    if not result.get("success"):
        raise RuntimeError(result.get("stderr", result))
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("script", nargs="?", help="Editor Python script; otherwise read stdin")
    args = parser.parse_args()
    execute(pathlib.Path(args.script).read_text(encoding="utf-8") if args.script else sys.stdin.read())
