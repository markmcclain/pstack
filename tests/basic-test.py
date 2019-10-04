#!/usr/bin/python

import subprocess,json
import coremonitor

cm = coremonitor.CoreMonitor(["tests/basic"])

print("core is %s" % cm.core())
text = subprocess.check_output(["./pstack", "-j", cm.core()])
threads = json.loads(text)
assert len(threads) == 1
thread = threads[0]
stack = thread["ti_stack"]

if stack[0]["function"] == "__kernel_vsyscall":
    stack = stack[1:]
assert stack[0]["function"] == "raise" or stack[0]["function"] == "__GI_raise" or stack[0]["function"] == "gsignal"
assert stack[1]["function"] == "abort" or stack[1]["function"] == "__GI_abort"
assert stack[2]["function"] == "g"
assert stack[3]["function"] == "f"
