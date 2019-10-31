import subprocess
from subprocess import PIPE
from tempfile import TemporaryDirectory
import os
import sys

my_cc = "./build/program"

success_cnt = 0
failure_cnt = 0

def run_my_cc(_file):
    with TemporaryDirectory() as temp_dir:
        llvm = os.path.join(temp_dir, "llvm")
        t = subprocess.run([my_cc, "-o", llvm, _file],
                           stdout=PIPE, stderr=PIPE)
        if t.returncode == 0:
            w = subprocess.run(["lli", llvm], stdout=PIPE, stderr=PIPE)
            return (w.returncode, w.stdout)
        else:
            return None

def run_cc(_file):
    with TemporaryDirectory() as temp_dir:
        cc_out = os.path.join(temp_dir, "cc_out")
        t = subprocess.run(["gcc", _file, "-o", cc_out],
                           stdout=PIPE, stderr=PIPE)
        if t.returncode == 0:
            w = subprocess.run([cc_out], stdout=PIPE, stderr=PIPE)
            return (w.returncode, w.stdout)
        else:
            return None

def test_file(_file):
    my_cc_res = run_my_cc(_file)
    cc_res = run_cc(_file)
    return cc_res and my_cc_res == cc_res

def test(filename):
    if os.path.isdir(filename):
        for _file in os.listdir(filename):
            test(os.path.join(filename, _file))
    else:
        global success_cnt
        global failure_cnt
        msg = filename
        msg += (50 - len(filename)) * "."
        success = test_file(filename)
        if success:
            msg += "ok";
            success_cnt += 1
        else:
            msg += "fail"
            failure_cnt += 1
        print(msg)

try:
    if len(sys.argv) == 2:
        test(sys.argv[1])
    print("===================summary==========================")
    print(f"{success_cnt} successes, {failure_cnt} failures")
except KeyboardInterrupt:
    sys.exit(0)
