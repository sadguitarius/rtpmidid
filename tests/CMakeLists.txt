add_executable(test_rtppeer test_rtppeer.cpp test_utils.cpp)
target_link_libraries(test_rtppeer rtpmidid-shared -lfmt)
add_test(NAME test_rtppeer COMMAND test_rtppeer)

add_executable(test_rtpserver test_rtpserver.cpp test_utils.cpp)
target_link_libraries(test_rtpserver rtpmidid-shared -lfmt -pthread)
add_test(NAME test_rtpserver COMMAND test_rtpserver)

add_executable(test_poller test_poller.cpp test_utils.cpp)
target_link_libraries(test_poller rtpmidid-shared -lfmt -pthread)
add_test(NAME test_poller COMMAND test_poller)


add_executable(test_misc test_misc.cpp test_utils.cpp)
target_link_libraries(test_misc rtpmidid-shared -lfmt -pthread)
add_test(NAME test_misc COMMAND test_poller)

# add_executable(test_rtpmidid
#     test_rtpmidid.cpp test_utils.cpp
#     ../src/aseq.cpp  ../src/config.cpp ../src/control_socket.cpp ../src/rtpmidid.cpp ../src/stringpp.cpp
# )
# target_link_libraries(test_rtpmidid rtpmidid-shared -lfmt -pthread)

# target_link_libraries(test_rtpmidid ${AVAHI_LIBRARIES})
# target_include_directories(test_rtpmidid PUBLIC ${AVAHI_INCLUDE_DIRS})
# target_compile_options(test_rtpmidid PUBLIC ${AVAHI_CFLAGS_OTHER})

# target_link_libraries(test_rtpmidid ${FMT_LIBRARIES})
# target_include_directories(test_rtpmidid PUBLIC ${FMT_INCLUDE_DIRS})
# target_compile_options(test_rtpmidid PUBLIC ${FMT_CFLAGS_OTHER})

# target_link_libraries(test_rtpmidid ${ALSA_LIBRARIES})
# target_include_directories(test_rtpmidid PUBLIC ${ALSA_INCLUDE_DIRS})
# target_compile_options(test_rtpmidid PUBLIC ${ALSA_CFLAGS_OTHER})

add_executable(test_signals
    test_signals.cpp
)
target_link_libraries(test_signals -lfmt -pthread rtpmidid-shared)
add_test(NAME test_signals COMMAND test_signals)


#
# MIDIROUTER
#

if (NOT LIBS_ONLY)
  add_executable(test_midirouter
      test_midirouter.cpp test_utils.cpp
  )
  target_link_libraries(test_midirouter -lfmt -pthread rtpmidid-shared rtpmidid2-static)
  add_test(NAME test_midirouter COMMAND test_midirouter)
  target_link_libraries(test_midirouter ${ALSA_LIBRARIES})
  target_include_directories(test_midirouter PUBLIC ${ALSA_INCLUDE_DIRS})
  target_compile_options(test_midirouter PUBLIC ${ALSA_CFLAGS_OTHER})
endif (NOT LIBS_ONLY)

#
# MIDIROUTER2
#

if (NOT LIBS_ONLY)
  add_executable(test_midirouter2
      test_midirouter2.cpp test_utils.cpp
  )
  target_link_libraries(test_midirouter2 -lfmt -pthread rtpmidid-shared rtpmidid2-static)
  add_test(NAME test_midirouter2 COMMAND test_midirouter2)
  target_link_libraries(test_midirouter2 ${ALSA_LIBRARIES})
  target_include_directories(test_midirouter2 PUBLIC ${ALSA_INCLUDE_DIRS})
  target_compile_options(test_midirouter2 PUBLIC ${ALSA_CFLAGS_OTHER})
endif (NOT LIBS_ONLY)


# disabled as failing
# add_test(NAME test_rtpmidid COMMAND test_rtpmidid)

#
# RTPCLIENT
#

if (NOT LIBS_ONLY)
    add_executable(test_rtpclient
        test_rtpclient.cpp test_utils.cpp
    )
    target_link_libraries(test_rtpclient -lfmt -pthread rtpmidid-shared)
    add_test(NAME test_rtpclient COMMAND test_rtpclient)
    target_link_libraries(test_rtpclient ${ALSA_LIBRARIES})
    target_include_directories(test_rtpclient PUBLIC ${ALSA_INCLUDE_DIRS})
    target_compile_options(test_rtpclient PUBLIC ${ALSA_CFLAGS_OTHER})
endif (NOT LIBS_ONLY)
