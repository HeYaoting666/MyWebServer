cmake_minimum_required(VERSION 3.16)
project(MyWebServer)

set(CMAKE_CXX_STANDARD 11)

add_executable(MyWebServer main.cpp
        lock/locker.h
        log/block_queue.h
        log/log.h
        log/log.cpp
        pool/sql_conn_pool.h
        pool/sql_conn_pool.cpp
        http/http_conn.h
        http/http_conn.cpp
        pool/thread_pool.h
        timer/lst_timer.h
        timer/lst_timer.cpp
        commend/utils.h
        commend/utils.cpp
        webserver/webserver.h
        webserver/webserver.cpp
        config.h
        config.cpp)

find_package(Threads)
target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_THREAD_LIBS_INIT} libmysqlclient.so)
