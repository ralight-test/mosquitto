#!/usr/bin/env python3

from mosq_test_helper import *
import json
import shutil

def write_config(filename, port):
    with open(filename, 'w') as f:
        f.write("listener %d\n" % (port))
        f.write("allow_anonymous true\n")
        f.write(f"plugin {mosq_test.get_build_root()}/plugins/dynamic-security/mosquitto_dynamic_security.so\n")
        f.write("plugin_opt_config_file %d/dynamic-security.json\n" % (port))

def command_check(sock, command_payload, expected_response, msg=""):
    command_packet = mosq_test.gen_publish(topic="$CONTROL/dynamic-security/v1", qos=0, payload=json.dumps(command_payload))
    sock.send(command_packet)
    response = json.loads(mosq_test.read_publish(sock))
    if response != expected_response:
        print(msg)
        print(expected_response)
        print(response)
        raise ValueError(response)



port = mosq_test.get_port()
conf_file = os.path.basename(__file__).replace('.py', '.conf')
write_config(conf_file, port)

create_client_command = { "commands": [{
            "command": "createClient", "username": "user_one",
            "password": "password", "clientid": "cid",
            "textname": "Name", "textdescription": "Description",
            "correlationData": "2" }]
}
create_client_response = {'responses': [{'command': 'createClient', 'correlationData': '2'}]}

create_groups_command = { "commands": [
    {
        "command": "createGroup", "groupname": "group_one",
        "textname": "Name", "textdescription": "Description",
        "correlationData": "12"
    },
    {
        "command": "createGroup", "groupname": "group_two",
        "textname": "Name", "textdescription": "Description",
        "correlationData": "13"
    }
    ]
}
create_groups_response = {'responses': [
    {'command': 'createGroup', 'correlationData': '12'},
    {'command': 'createGroup', 'correlationData': '13'}
    ]}

create_roles_command = { "commands": [
    {
        "command": "createRole", "rolename": "role_one",
        "textname": "Name", "textdescription": "Description",
        "acls":[], "correlationData": "21"
    },
    {
        "command": "createRole", "rolename": "role_two",
        "textname": "Name", "textdescription": "Description",
        "acls":[], "correlationData": "22"
    },
    {
        "command": "createRole", "rolename": "role_three",
        "textname": "Name", "textdescription": "Description",
        "acls":[], "correlationData": "23"
    }
    ]
}
create_roles_response = {'responses': [
    {'command': 'createRole', 'correlationData': '21'},
    {'command': 'createRole', 'correlationData': '22'},
    {'command': 'createRole', 'correlationData': '23'}
    ]}

modify_client_command1 = { "commands": [{
    "command": "modifyClient", "username": "user_one",
    "textname": "Modified name", "textdescription": "Modified description",
    "clientid": "",
    "roles":[
        {'rolename':'role_one', 'priority':2},
        {'rolename':'role_two'},
        {'rolename':'role_three', 'priority':10}
    ],
    "groups":[
        {'groupname':'group_one', 'priority':3},
        {'groupname':'group_two', 'priority':8}
    ],
    "correlationData": "3" }]
}
modify_client_response1 = {'responses': [{'command': 'modifyClient', 'correlationData': '3'}]}

modify_client_command2 = { "commands": [{
    "command": "modifyClient", "username": "user_one",
    "textname": "Modified name", "textdescription": "Modified description",
    "groups":[],
    "correlationData": "4" }]
}
modify_client_response2 = {'responses': [{'command': 'modifyClient', 'correlationData': '4'}]}


get_client_command1 = { "commands": [{
    "command": "getClient", "username": "user_one"}]}
get_client_response1 = {'responses':[{'command': 'getClient', 'data': {'client': {'username': 'user_one', 'clientid': 'cid',
    'textname': 'Name', 'textdescription': 'Description',
    'roles': [],
    'groups': [],
    'connections': []
    }}}]}

get_client_command2 = { "commands": [{
    "command": "getClient", "username": "user_one"}]}
get_client_response2 = {'responses':[{'command': 'getClient', 'data': {'client': {'username': 'user_one',
    'textname': 'Modified name', 'textdescription': 'Modified description',
    'roles': [
        {'rolename':'role_three', 'priority':10},
        {'rolename':'role_one', 'priority':2},
        {'rolename':'role_two'}
    ],
    'groups': [
        {'groupname':'group_two', 'priority':8},
        {'groupname':'group_one', 'priority':3}],
    'connections': []
    }}}]}

get_client_command3 = { "commands": [{
            "command": "getClient", "username": "user_one"}]}
get_client_response3 = {'responses':[{'command': 'getClient', 'data': {'client': {'username': 'user_one',
    'textname': 'Modified name', 'textdescription': 'Modified description',
    'groups': [],
    'roles': [
        {'rolename':'role_three', 'priority':10},
        {'rolename':'role_one', 'priority':2},
        {'rolename':'role_two'}],
     'connections': []
    }}}]}



rc = 1
connect_packet = mosq_test.gen_connect("ctrl-test", username="admin", password="admin")
connack_packet = mosq_test.gen_connack(rc=0)

mid = 2
subscribe_packet = mosq_test.gen_subscribe(mid, "$CONTROL/dynamic-security/#", 1)
suback_packet = mosq_test.gen_suback(mid, 1)

try:
    os.mkdir(str(port))
    shutil.copyfile(str(Path(__file__).resolve().parent / "dynamic-security-init.json"), "%d/dynamic-security.json" % (port))
except FileExistsError:
    pass

broker = mosq_test.start_broker(filename=os.path.basename(__file__), use_conf=True, port=port)

try:
    sock = mosq_test.do_client_connect(connect_packet, connack_packet, timeout=5, port=port)
    mosq_test.do_send_receive(sock, subscribe_packet, suback_packet, "suback")

    # Create client
    command_check(sock, create_client_command, create_client_response)

    # Create groups
    command_check(sock, create_groups_command, create_groups_response)

    # Create role
    command_check(sock, create_roles_command, create_roles_response)

    # Get client
    command_check(sock, get_client_command1, get_client_response1, "get client 1")

    # Modify client - with groups
    command_check(sock, modify_client_command1, modify_client_response1)

    # Get client
    command_check(sock, get_client_command2, get_client_response2, "get client 2a")

    # Kill broker and restart, checking whether our changes were saved.
    broker.terminate()
    broker_terminate_rc = 0
    if mosq_test.wait_for_subprocess(broker):
        print("broker not terminated")
        broker_terminate_rc = 1
    broker = mosq_test.start_broker(filename=os.path.basename(__file__), use_conf=True, port=port)

    sock = mosq_test.do_client_connect(connect_packet, connack_packet, timeout=5, port=port)
    mosq_test.do_send_receive(sock, subscribe_packet, suback_packet, "suback")

    # Get client
    command_check(sock, get_client_command2, get_client_response2, "get client 2b")

    # Modify client - without groups
    command_check(sock, modify_client_command2, modify_client_response2)

    # Get client
    command_check(sock, get_client_command3, get_client_response3, "get client 3")

    rc = broker_terminate_rc

    sock.close()
except mosq_test.TestError:
    pass
finally:
    os.remove(conf_file)
    try:
        os.remove(f"{port}/dynamic-security.json")
        pass
    except FileNotFoundError:
        pass
    os.rmdir(f"{port}")
    broker.terminate()
    if mosq_test.wait_for_subprocess(broker):
        print("broker not terminated")
        if rc == 0: rc=1
    (stdo, stde) = broker.communicate()
    if rc:
        print(stde.decode('utf-8'))


exit(rc)
