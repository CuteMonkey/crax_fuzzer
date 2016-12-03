#!/usr/bin/python
import sys
import os.path
import time
import threading

if len(sys.argv) != 4:
    sys.stderr.write("Usage: craxfuzzer.py <loadvm Name> <Guest Exec Command> <Verify Exec Command>\n")
    sys.stderr.write("<Verify Exec Command> use 'TestCase' to be the crash file \n")
    sys.exit()

if not "TestCase" in sys.argv[3]:
    sys.exit("\"" + sys.argv[3] + "\" doesn't have 'TestCase' this word")

#check the verify command
print "testing command ---" ,
print sys.argv[3]
cmd = "ssh littlepunpun@127.0.0.1 -p 2222 'cd crax-fuzzer/verify;" + sys.argv[3] + "'"
retcode = os.system(cmd)
#if retcode != 0:
#    sys.exit("error\n")

cmd = "./crax-fuzzer/host/automated/s2erun.exp " + sys.argv[1] + " '" + sys.argv[2] + "'"
fuzzing = threading.Thread(target=os.system, args=(cmd,))
fuzzing.start()

time.sleep(3)

num = 1

while 1 == 1:
    dir_path = "/home/littlepunpun/sqlab/CRAXFuzzer/s2e-last/"
    dir_path = os.path.realpath(dir_path)
    folder = os.path.basename(os.path.normpath(dir_path))
    file_name = "TestCase_" + str(num) + ".bin"
    file_path = dir_path + "/" + file_name
    remote_file_path = folder + "/" + file_name
    verify_cmd = sys.argv[3].replace("TestCase", "~/" + remote_file_path)

    while not os.path.exists(file_path):
        time.sleep(2)

    if os.path.isfile(file_path):
        if num == 1:
            #create folder to save TestCase
            cmd = "ssh littlepunpun@127.0.0.1 -p 2222 'mkdir " + folder + "'"
            retcode = os.system(cmd)
        
        #send the TestCase to the folder
        cmd = "scp -P 2222 " + file_path + " littlepunpun@127.0.0.1:~/" + folder
        retcode = os.system(cmd)
        if retcode != 0:
            sys.exit()

        #exec verify command
        cmd = "ssh littlepunpun@127.0.0.1 -p 2222 'cd crax-fuzzer/verify;" + verify_cmd + "'"
        retcode = os.system(cmd)
        #if retcode != 0:
        #    sys.exit()

        #try to retrive the result
        cmd = "scp -P 2222 littlepunpun@127.0.0.1:crax-fuzzer/verify/result.txt " + dir_path
        retcode = os.system(cmd)

        num += 1

        if os.path.exists(folder + "/result.txt"):
            print file_name , "Verify done"
            cmd = "ssh littlepunpun@127.0.0.1 -p 2222 'rm -f crax-fuzzer/verify/result.txt'"
            retcode = os.system(cmd)
            break
    else:
        raise ValueError("%s isn't a file!" % file_path)
