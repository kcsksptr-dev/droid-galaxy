"""Run Python inside the open Unreal Editor (needs bRemoteExecution=True, see Config/DefaultEngine.ini).

Usage:
  python3 Tools/ue_remote.py path/to/script.py      # run a file in the editor
  python3 Tools/ue_remote.py -c "print(unreal.SystemLibrary.get_engine_version())"
Prints the editor-side output; exits 1 if the command raised.
"""
import os
import sys
import time

ENGINE = os.environ.get("UE_ROOT", os.path.expanduser("~/unreal/engine/UE_5.2.0"))
sys.path.insert(0, os.path.join(ENGINE, "Engine/Plugins/Experimental/PythonScriptPlugin/Content/Python"))
import remote_execution as rex  # noqa: E402


def main():
    if len(sys.argv) >= 3 and sys.argv[1] == "-c":
        code, mode = sys.argv[2], rex.MODE_EXEC_STATEMENT
    elif len(sys.argv) == 2:
        code, mode = os.path.abspath(sys.argv[1]), rex.MODE_EXEC_FILE
    else:
        sys.exit(__doc__)

    remote = rex.RemoteExecution()
    remote.start()
    try:
        deadline = time.time() + 15
        while not remote.remote_nodes and time.time() < deadline:
            time.sleep(0.25)
        if not remote.remote_nodes:
            sys.exit("no Unreal Editor with remote execution found (is it open?)")
        remote.open_command_connection(remote.remote_nodes[0]["node_id"])
        res = remote.run_command(code, unattended=True, exec_mode=mode, raise_on_failure=False)
        for line in res.get("output", []):
            print(line.get("output", "").rstrip())
        if res.get("result") not in (None, "None", ""):
            print(res["result"])
        sys.exit(0 if res.get("success") else 1)
    finally:
        remote.stop()


if __name__ == "__main__":
    main()
