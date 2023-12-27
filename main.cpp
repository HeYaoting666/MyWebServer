#include "lock/locker.h"
#include "pool/sql_conn_pool.h"
#include "pool/thread_pool.h"
#include "http/http_conn.h"
#include "timer/lst_timer.h"
#include "webserver/webserver.h"
#include "config.h"

using namespace std;

int main( int argc, char* argv[] ) {

    //命令行解析
    Config config;
    config.parse_arg(argc, argv);

    WebServer server(
            config.port,
            "./ServerLog",
            "root",
            "HE2000319",
            "yourdb",
            config.asyn_log_write,
            config.OPT_LINGER,
            config.trig_mode,
            config.sql_num,
            config.thread_num,
            config.close_log,
            config.actor_model
            );
    server.event_listen();
    server.event_loop();

    return 0;
}
