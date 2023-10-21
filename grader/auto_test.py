import subprocess

base_cmd = ['./grader_controller',  '-c', 'grader.cfg', '-s' '../genjebek_pa1.tar', '-t'] 

tests = [
    ["startup", 5.0],
    ["author", 0.0],
    ["ip", 5.0],
    ["port", 2.5],
    ["_list", 10.0],
    ["refresh", 5.0],
    ["send", 15.0],
    ["broadcast", 10.0],
    ["block", 5.0],
    ["blocked", 5.0],
    ["unblock", 2.5],
    ["logout", 2.5],
    ["buffer", 5.0],
    ["exit", 2.5],
    ["statistics", 5.0],
    ["exception_login", 2.0],
    ["exception_send", 2.0],
    ["exception_block", 2.0],
    ["exception_blocked", 2.0],
    ["exception_unblock", 2.0],
]

for test in tests:
    base_cmd.append(test[0])
    res = subprocess.run(base_cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL).stdout.decode('utf-8').splitlines()
    j = 0
    for i in res[-2:]:
        if j == 1:
            print(f'{i}/{test[1]}')
        else:
            print(i)
        j+=1
    base_cmd.pop()
