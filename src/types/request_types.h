#pragma once

namespace types {
    enum RequestTypes {
        TRANSFER,
        PREPARE,
        ACCEPT,
        COMMIT,
        SYNC,
        GET_BALANCE,
        GET_LOGS,
        GET_DB_LOGS
    };
}
