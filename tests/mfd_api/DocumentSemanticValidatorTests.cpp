/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for semantic document validation.
 */

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include "mfd/runtime/DocumentSemanticValidator.h"

namespace
{
mfd::Primitive MakePrimitive(std::string id, const mfd::PrimitiveType type)
{
    mfd::Primitive primitive;
    primitive.id = std::move(id);
    primitive.type = type;

    switch (type)
    {
    case mfd::PrimitiveType::Circle:
        primitive.geometry = mfd::CircleGeometry {};
        break;
    case mfd::PrimitiveType::Rectangle:
        primitive.geometry = mfd::RectangleGeometry {};
        break;
    case mfd::PrimitiveType::Ellipse:
        primitive.geometry = mfd::EllipseGeometry {};
        break;
    case mfd::PrimitiveType::Square:
        primitive.geometry = mfd::SquareGeometry {};
        break;
    case mfd::PrimitiveType::Triangle:
        primitive.geometry = mfd::TriangleGeometry {};
        break;
    case mfd::PrimitiveType::Text:
        primitive.geometry = mfd::TextGeometry {};
        break;
    default:
        primitive.geometry = mfd::LineGeometry {};
        break;
    }

    return primitive;
}

mfd::ReticleGroup MakeReticle(std::string id)
{
    mfd::ReticleGroup reticle;
    reticle.id = std::move(id);
    reticle.layerId = "default";
    return reticle;
}

mfd::MfdDocument MakeDocumentWithReticle(mfd::ReticleGroup reticle)
{
    mfd::PageDefinition page;
    page.name = "Main";
    page.normalizedName = "main";
    page.layers.push_back(mfd::PageLayerDefinition {"default"});
    page.staticReticles.push_back(std::move(reticle));

    mfd::MfdDocument document;
    document.pages.push_back(std::move(page));
    return document;
}

std::vector<std::string> DiagnosticCodes(const std::vector<mfd::SemanticValidationDiagnostic>& diagnostics)
{
    std::vector<std::string> codes;
    codes.reserve(diagnostics.size());
    for (const mfd::SemanticValidationDiagnostic& diagnostic : diagnostics)
    {
        codes.push_back(diagnostic.code);
    }

    return codes;
}
} // namespace

TEST(DocumentSemanticValidatorTests, ReportsEmptyClippingPrimitiveId)
{
    mfd::ReticleGroup reticle = MakeReticle("adi_group");
    reticle.clipping.mode = mfd::ReticleClipMode::Inner;
    reticle.primitives.push_back(MakePrimitive("adi_circle", mfd::PrimitiveType::Circle));

    const std::vector<mfd::SemanticValidationDiagnostic> diagnostics =
        mfd::DocumentSemanticValidator {}.Validate(MakeDocumentWithReticle(std::move(reticle)));

    ASSERT_EQ(diagnostics.size(), 1U);
    EXPECT_EQ(diagnostics.front().code, "MFD016");
    EXPECT_NE(diagnostics.front().message.find("has no primitive id"), std::string::npos);
}

TEST(DocumentSemanticValidatorTests, ReportsUnknownClippingPrimitiveId)
{
    mfd::ReticleGroup reticle = MakeReticle("adi_group");
    reticle.clipping.mode = mfd::ReticleClipMode::Outer;
    reticle.clipping.primitiveId = "adi_circle";
    reticle.primitives.push_back(MakePrimitive("body", mfd::PrimitiveType::Circle));

    const std::vector<mfd::SemanticValidationDiagnostic> diagnostics =
        mfd::DocumentSemanticValidator {}.Validate(MakeDocumentWithReticle(std::move(reticle)));

    ASSERT_EQ(diagnostics.size(), 1U);
    EXPECT_EQ(diagnostics.front().code, "MFD014");
    EXPECT_NE(diagnostics.front().message.find("unknown clipping primitive"), std::string::npos);
}

TEST(DocumentSemanticValidatorTests, ReportsUnsupportedClippingPrimitive)
{
    mfd::ReticleGroup reticle = MakeReticle("adi_group");
    reticle.clipping.mode = mfd::ReticleClipMode::Inner;
    reticle.clipping.primitiveId = "pitch_ladder";
    reticle.primitives.push_back(MakePrimitive("pitch_ladder", mfd::PrimitiveType::Line));

    const std::vector<mfd::SemanticValidationDiagnostic> diagnostics =
        mfd::DocumentSemanticValidator {}.Validate(MakeDocumentWithReticle(std::move(reticle)));

    ASSERT_EQ(diagnostics.size(), 1U);
    EXPECT_EQ(diagnostics.front().code, "MFD015");
    EXPECT_NE(diagnostics.front().message.find("cannot be used as a clipping mask"), std::string::npos);
}

TEST(DocumentSemanticValidatorTests, AcceptsSupportedClipMaskPrimitives)
{
    const std::vector<mfd::PrimitiveType> supportedTypes {
        mfd::PrimitiveType::Circle,
        mfd::PrimitiveType::Rectangle,
        mfd::PrimitiveType::Ellipse,
        mfd::PrimitiveType::Square,
        mfd::PrimitiveType::Triangle};

    for (const mfd::PrimitiveType type : supportedTypes)
    {
        mfd::ReticleGroup reticle = MakeReticle("valid_mask");
        reticle.clipping.mode = mfd::ReticleClipMode::Outer;
        reticle.clipping.primitiveId = "mask";
        reticle.primitives.push_back(MakePrimitive("mask", type));

        const std::vector<mfd::SemanticValidationDiagnostic> diagnostics =
            mfd::DocumentSemanticValidator {}.Validate(MakeDocumentWithReticle(std::move(reticle)));
        EXPECT_TRUE(diagnostics.empty());
    }
}

TEST(DocumentSemanticValidatorTests, ReportsDuplicatePrimitiveIdsInsideReticle)
{
    mfd::ReticleGroup reticle = MakeReticle("dup_primitives");
    reticle.primitives.push_back(MakePrimitive("same", mfd::PrimitiveType::Circle));
    reticle.primitives.push_back(MakePrimitive("same", mfd::PrimitiveType::Rectangle));

    const std::vector<mfd::SemanticValidationDiagnostic> diagnostics =
        mfd::DocumentSemanticValidator {}.Validate(MakeDocumentWithReticle(std::move(reticle)));

    EXPECT_EQ(DiagnosticCodes(diagnostics), std::vector<std::string> {"MFD017"});
}

TEST(DocumentSemanticValidatorTests, ReportsDuplicateReticleIdsAfterNormalization)
{
    mfd::PageDefinition page;
    page.name = "Main";
    page.normalizedName = "main";
    page.layers.push_back(mfd::PageLayerDefinition {"default"});
    page.staticReticles.push_back(MakeReticle("ADI"));
    page.staticReticles.push_back(MakeReticle("adi"));

    mfd::MfdDocument document;
    document.pages.push_back(std::move(page));

    const std::vector<mfd::SemanticValidationDiagnostic> diagnostics =
        mfd::DocumentSemanticValidator {}.Validate(document);

    EXPECT_EQ(DiagnosticCodes(diagnostics), std::vector<std::string> {"MFD018"});
}

TEST(DocumentSemanticValidatorTests, ReportsDuplicateStaticReticleAndStrobeIdsAfterNormalization)
{
    mfd::PageDefinition page;
    page.name = "Main";
    page.normalizedName = "main";
    page.layers.push_back(mfd::PageLayerDefinition {"default"});
    page.staticReticles.push_back(MakeReticle("cursor"));

    mfd::PageStrobeDefinition strobe;
    strobe.reticle = MakeReticle("Cursor");
    page.strobe = std::move(strobe);

    mfd::MfdDocument document;
    document.pages.push_back(std::move(page));

    const std::vector<mfd::SemanticValidationDiagnostic> diagnostics =
        mfd::DocumentSemanticValidator {}.Validate(document);

    EXPECT_EQ(DiagnosticCodes(diagnostics), std::vector<std::string> {"MFD018"});
}
