import inspect, os, sys

# From http://stackoverflow.com/questions/279237/python-import-a-module-from-a-folder
cmd_subfolder = os.path.realpath(os.path.abspath(os.path.join(os.path.split(inspect.getfile( inspect.currentframe() ))[0],"..")))
if cmd_subfolder not in sys.path:
    sys.path.insert(0, cmd_subfolder)

import mosq_test
import mqtt5_opts
import mqtt5_props
import mqtt5_rc

import socket
import ssl
import struct
import subprocess
import time
import errno
from pathlib import Path

source_dir = Path(__file__).resolve().parent
ssl_dir = source_dir.parent / "ssl"

import importlib

def persist_module():
    if len(sys.argv) > 1:
        mod = sys.argv.pop(1)
    else:
        raise RuntimeError("Not enough command line arguments - need persist module")
    return importlib.import_module(mod)

def do_test_broker_failure(conf_file : str, config : list, rc_expected : int, error_log_entry : str = None, cmd_args : list = None):
    rc = 1

    if len(conf_file) and len(config):
        with open(conf_file, 'w') as f:
            f.write("\n".join(config))
            f.write("\n")
    try:
        broker = mosq_test.start_broker(conf_file, use_conf=True, expect_fail=True, cmd_args=cmd_args)
        broker.wait(2)
        if broker.returncode != rc_expected:
            (stdo, stde) = broker.communicate()
            print(stde.decode('utf-8'))
            return rc

        if error_log_entry is not None:
            (_, stde) = broker.communicate()
            error_log = stde.decode('utf-8')
            if error_log_entry not in error_log:
                print(f"Error log entry: '{error_log_entry}' not found in '{error_log}'")
                return rc
        rc = 0
    except subprocess.TimeoutExpired:
        broker.terminate()
        mosq_test.wait_for_subprocess(broker)
        return rc
    except Exception as e:
        print(e)
        return rc
    finally:
        if len(conf_file) and len(config):
            try:
                os.remove(conf_file)
            except FileNotFoundError:
                pass
        if rc:
            print(f"While testing 'config {chr(10).join(config) if len(config) else ''}'{', args'+ ' '.join(cmd_args) if cmd_args is not None else ''}")
            exit(rc)

    return rc;

