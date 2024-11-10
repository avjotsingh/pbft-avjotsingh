#pragma once

namespace types {
    enum RequestTypes {
        TRANSFER,
        PRE_PREPARE,
        PRE_PREPARE_MSG,
        PRE_PREPARE_OK,
        PREPARE,
        PREPARE_OK,
        COMMIT,
        TRIGGER_VIEW_CHANGE,
        VIEW_CHANGE,
        NEW_VIEW,
        NOTIFY,
        PROCESS
    };
}
