# bridge-topic-test
add_library(bridge-topic-obj
	OBJECT
	../../../src/bridge_topic.c
	../../../lib/memory_mosq.c
	../../../lib/util_topic.c
)
target_compile_definitions(bridge-topic-obj PRIVATE WITH_BRIDGE WITH_BROKER)
target_link_libraries(bridge-topic-obj PUBLIC common-unit-test-header)

add_executable(bridge-topic-test
    bridge_topic_test.c
    stubs.c
)
target_compile_definitions(bridge-topic-test PRIVATE WITH_BRIDGE WITH_BROKER)
target_link_libraries(bridge-topic-test PRIVATE bridge-topic-obj common-unit-test-header)
add_test(NAME unit-bridge-topic-test COMMAND bridge-topic-test)

# keepalive-test
add_library(keepalive-obj
	OBJECT
	../../../lib/memory_mosq.c
)
target_compile_definitions(keepalive-obj PRIVATE WITH_BROKER)
target_link_libraries(keepalive-obj PUBLIC common-unit-test-header)

add_executable(keepalive-test
    keepalive_test.c
    keepalive_stubs.c
)
target_compile_definitions(keepalive-test PRIVATE WITH_BROKER)
target_link_libraries(keepalive-test PRIVATE keepalive-obj common-unit-test-header)
add_test(NAME unit-keepalive-test COMMAND keepalive-test)

# persist-read-test
add_library(persistence-read-obj
    OBJECT
		../../../lib/memory_mosq.c
		../../../src/memory_public.c
        ../../../common/misc_mosq.c
        ../../../src/persist_read_v234.c
        ../../../src/persist_read_v5.c
        ../../../src/persist_read.c
        ../../../src/retain.c
        ../../../src/topic_tok.c
        ../../../lib/utf8_mosq.c
)
target_compile_definitions(persistence-read-obj PRIVATE WITH_PERSISTENCE WITH_BROKER)
target_link_libraries(persistence-read-obj PUBLIC common-unit-test-header OpenSSL::SSL)

add_executable(persist-read-test
    persist_read_test.c
    persist_read_stubs.c
    ../../../lib/packet_datatypes.c
    ../../../lib/property_mosq.c
    ../../../lib/util_mosq.c
)
target_compile_definitions(persist-read-test PRIVATE TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" WITH_PERSISTENCE WITH_BROKER)
target_link_libraries(persist-read-test PRIVATE persistence-read-obj)
add_test(NAME unit-persist-read-test COMMAND persist-read-test)

# persist-write-test
add_library(persistence-write-obj
    OBJECT
        ../../../src/database.c
		../../../lib/memory_mosq.c
		../../../src/memory_public.c
        ../../../common/misc_mosq.c
        ../../../src/persist_read_v234.c
        ../../../src/persist_read_v5.c
        ../../../src/persist_read.c
        ../../../src/persist_write_v5.c
        ../../../src/persist_write.c
        ../../../src/retain.c
        ../../../src/subs.c
        ../../../src/topic_tok.c
        ../../../lib/utf8_mosq.c
)
target_compile_definitions(persistence-write-obj PRIVATE WITH_PERSISTENCE WITH_BROKER)
target_link_libraries(persistence-write-obj PUBLIC common-unit-test-header)

add_executable(persist-write-test
    persist_write_test.c
    persist_write_stubs.c
    ../../../lib/packet_datatypes.c
    ../../../lib/property_mosq.c
    ../../../lib/util_mosq.c
    ../../../lib/packet_mosq.c
)
target_compile_definitions(persist-write-test PRIVATE TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" WITH_PERSISTENCE WITH_BROKER WITH_SYS_TREE)
target_link_libraries(persist-write-test PRIVATE persistence-write-obj OpenSSL::SSL)
add_test(NAME unit-persist-write-test COMMAND persist-write-test)

# subs-test
add_library(subs-obj
    OBJECT
        ../../../lib/memory_mosq.c
        ../../../src/memory_public.c
        ../../../lib/property_mosq.c
        ../../../lib/packet_datatypes.c
        ../../../src/database.c
        ../../../src/subs.c
        ../../../src/topic_tok.c
        ../../../lib/utf8_mosq.c
)
target_compile_definitions(subs-obj PRIVATE WITH_BROKER)
target_link_libraries(subs-obj PUBLIC common-unit-test-header)

add_executable(subs-test
    subs_stubs.c
    subs_test.c
)

target_compile_definitions(subs-test PRIVATE WITH_PERSISTENCE WITH_BROKER WITH_SYS_TREE)
target_link_libraries(subs-test PRIVATE common-unit-test-header subs-obj)
add_test(NAME unit-subs-test COMMAND subs-test)
