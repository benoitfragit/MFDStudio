/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Umbrella header consumed by generated client UI code.
 *
 * @details The code generator emits this header as its single direct include so
 * generated sources stay coupled only to the `mfd_client_api` SDK surface.
 *
 * @note Generated source files should not add direct includes to lower-level
 * repository modules. The consuming client target links only `mfd_client_api`.
 */

#include "mfd/client/Animation.h"
#include "mfd/client/LatestBatchPublisher.h"
#include "mfd/control/CommandClient.h"
#include "mfd/control/CommandTypes.h"
#include "mfd/control/StrobeFeedback.h"
#include "mfd/ipc/ExchangeChannel.h"
#include "mfd/model/Types.h"
