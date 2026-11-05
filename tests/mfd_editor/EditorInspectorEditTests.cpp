/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "internal/application/EditorInspectorStringEdit.h"

#include <gtest/gtest.h>

TEST(EditorInspectorEdit, OnlyValidEffectiveChangesCreateUndoHistory)
{
    using editor::app::CheckInspectorStringEdit;
    using editor::app::StringEditDecision;
    EXPECT_EQ(CheckInspectorStringEdit("%H", "%H", "%H", true), StringEditDecision::Skip);
    EXPECT_EQ(CheckInspectorStringEdit("%H", "%H", "%", true), StringEditDecision::RejectFormat);
    EXPECT_EQ(CheckInspectorStringEdit("%H", "%H", "%H:%M", true), StringEditDecision::Commit);
    EXPECT_EQ(CheckInspectorStringEdit("changed", "old", "new", false), StringEditDecision::Skip);
    EXPECT_EQ(CheckInspectorStringEdit("title", "title", "50%", false), StringEditDecision::Commit);
}
