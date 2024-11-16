#pragma once

namespace types {
    enum RequestTypes {
        TRANSFER,
        PRE_PREPARE,
        PRE_PREPARE_OK,
        PREPARE,
        PREPARE_OK,
        COMMIT,
        VIEW_CHANGE,
        NEW_VIEW,
        NOTIFY,
        PROCESS,
        CHECKPOINT,
        SYNC,
        SYNC_OK,
        GET_LOG,
        GET_DB,
        GET_STATUS,
        GET_VIEW_CHANGES,
        GET_PERFORMANCE
    };
}
