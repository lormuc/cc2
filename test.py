import subprocess
import tempfile
import os
import sys

my_cc = "./build/program"

success_cnt = 0
failure_cnt = 0

def run_my_cc(_file):
    llvm = os.path.join(tempfile.mkdtemp(), "llvm")
    t = subprocess.run([my_cc, _file, llvm], capture_output=True)
    if t.returncode == 0:
        w = subprocess.run(["lli", llvm], capture_output=True)
        return (w.returncode, w.stdout)
    else:
        return None

def run_cc(_file):
    cc_out = os.path.join(tempfile.mkdtemp(), "cc_out")
    t = subprocess.run(["gcc", _file, "-o", cc_out], capture_output=True)
    if t.returncode == 0:
        w = subprocess.run([cc_out], capture_output=True)
        return (w.returncode, w.stdout)
    else:
        return None

def test_file(_file):
    my_cc_res = run_my_cc(_file)
    cc_res = run_cc(_file)
    return my_cc_res == cc_res

def test(filename):
    if os.path.isdir(filename):
        for _file in os.listdir(filename):
            test(os.path.join(filename, _file))
    else:
        global success_cnt
        global failure_cnt
        b = filename
        print(b, end="")
        print((50 - len(b)) * ".", end="")
        success = test_file(filename)
        if success:
            print("ok");
            success_cnt += 1
        else:
            print("fail")
            failure_cnt += 1

if len(sys.argv) == 2:
    test(sys.argv[1])
print("===================summary==========================")
print(f"{success_cnt} successes, {failure_cnt} failures")
