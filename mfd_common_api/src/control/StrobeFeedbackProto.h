/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal Protocol Buffers marshalling for `StrobeStatusFeedback`,
 * shared by the strobe feedback codec and the generic feedback envelope.
 */

#include "mfd/control/StrobeFeedback.h"
#include "mfd_feedback.pb.h"

namespace mfd::detail
{
/** @brief Fills a Protocol Buffers strobe status message from its public representation. */
void FillStrobeStatusFeedbackProto(const StrobeStatusFeedback& feedback,
                                    ::mfd::transport::StrobeStatusFeedback* target);

/**
 * @brief Parses a Protocol Buffers strobe status message into its public representation.
 * @throws std::runtime_error if the message carries an invalid feedback identifier.
 */
StrobeStatusFeedback ParseStrobeStatusFeedbackProto(const ::mfd::transport::StrobeStatusFeedback& message);
} // namespace mfd::detail
