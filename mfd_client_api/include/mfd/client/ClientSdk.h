/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Umbrella header for standalone client applications built against `mfd_client_api`.
 *
 * @details This header gathers the supported standalone loading, transport,
 * feedback, and publication helpers so examples and user code can stay on the
 * `mfd_client_api` SDK surface without spelling lower-level module headers
 * directly. Generated window-specific headers keep including
 * `mfd/client/GeneratedUiSupport.h` directly.
 *
 * @note Standalone clients and shipped examples should link only
 * `mfd_client_api`.
 */

#include "mfd/client/LatestBatchPublisher.h"
#include "mfd/client/WindowLivenessMonitor.h"
#include "mfd/control/FeedbackTransport.h"
#include "mfd/io/JsonLoader.h"
#include "mfd/model/PageDefinition.h"
#include "mfd/model/PageName.h"
#include "mfd/model/Reticle.h"
