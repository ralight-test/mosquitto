#!/usr/bin/env python3

#

from mosq_test_helper import *

def do_test(proto_ver):
    rc = 1

    port = mosq_test.get_port()

    if proto_ver == 5:
        V = 'mqttv5'
    elif proto_ver == 4:
        V = 'mqttv311'
    else:
        V = 'mqttv31'

    env = {
        'XDG_CONFIG_HOME':'/tmp/missing'
    }
    env = mosq_test.env_add_ld_library_path(env)
    cmd = [f'{mosq_test.get_build_root()}/client/mosquitto_sub',
            '-p', str(port),
            '-q', '0',
            '-t', '02/sub/filter-out/#',
            '-T', '02/sub/filter-out/filtered',
            '-V', V,
            '-C', '2'
            ]

    publish_packet1 = mosq_test.gen_publish("02/sub/filter-out/recv", qos=0, payload="recv", proto_ver=proto_ver)
    publish_packet2 = mosq_test.gen_publish("02/sub/filter-out/filtered", qos=0, payload="filtered", proto_ver=proto_ver)

    broker = mosq_test.start_broker(filename=os.path.basename(__file__), port=port)

    try:
        sock = mosq_test.pub_helper(port=port, proto_ver=proto_ver)

        sub = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
        time.sleep(0.1)
        sock.send(publish_packet1)
        sock.send(publish_packet2)
        sock.send(publish_packet1)
        sock.send(publish_packet2)
        sub_terminate_rc = 0
        if mosq_test.wait_for_subprocess(sub):
            print("sub not terminated")
            sub_terminate_rc = 1
        (stdo, stde) = sub.communicate()
        if stdo.decode('utf-8') == 'recv\nrecv\n':
            rc = sub_terminate_rc
        else:
            print(stdo.decode('utf-8'))
        sock.close()
    except mosq_test.TestError:
        pass
    except Exception as e:
        print(e)
    finally:
        broker.terminate()
        if mosq_test.wait_for_subprocess(broker):
            print("broker not terminated")
            if rc == 0: rc=1
        (stdo, stde) = broker.communicate()
        if rc:
            print(stde.decode('utf-8'))
            print("proto_ver=%d" % (proto_ver))
            exit(rc)


do_test(proto_ver=3)
do_test(proto_ver=4)
do_test(proto_ver=5)
