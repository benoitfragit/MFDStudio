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

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "mfd/runtime/DocumentSemanticValidator.h"
#include "mfd/model/RuntimeBudgets.h"

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
    strobe.name = "Default";
    strobe.normalizedName = "default";
    strobe.reticle = MakeReticle("Cursor");
    page.strobes.push_back(std::move(strobe));
    page.activeStrobeName = "Default";
    page.normalizedActiveStrobeName = "default";

    mfd::MfdDocument document;
    document.pages.push_back(std::move(page));

    const std::vector<mfd::SemanticValidationDiagnostic> diagnostics =
        mfd::DocumentSemanticValidator {}.Validate(document);

    EXPECT_EQ(DiagnosticCodes(diagnostics), std::vector<std::string> {"MFD018"});
}

TEST(DocumentSemanticValidatorTests, AcceptsAndRejectsAggregateCardinalityBoundaries)
{
    mfd::MfdDocument document;
    document.pages.resize(mfd::runtime_validation::kMaxDocumentPages);
    EXPECT_TRUE(mfd::DocumentSemanticValidator {}.Validate(document).empty());

    document.pages.emplace_back();
    EXPECT_EQ(DiagnosticCodes(mfd::DocumentSemanticValidator {}.Validate(document)),
              std::vector<std::string> {"MFD019"});

    document = {};
    mfd::PageDefinition page;
    page.layers.resize(mfd::runtime_validation::kMaxDocumentLayers);
    document.pages.push_back(std::move(page));
    EXPECT_TRUE(mfd::DocumentSemanticValidator {}.Validate(document).empty());

    document.pages.front().layers.emplace_back();
    EXPECT_EQ(DiagnosticCodes(mfd::DocumentSemanticValidator {}.Validate(document)),
              std::vector<std::string> {"MFD022"});
}

TEST(DocumentSemanticValidatorTests, CountsEditorLayersInAggregateLayerBudget)
{
    mfd::MfdDocument document;
    document.pages.emplace_back();
    document.pages.front().layers.resize(mfd::runtime_validation::kMaxDocumentLayers / 2U);
    document.pages.front().editor.layers.resize(
        mfd::runtime_validation::kMaxDocumentLayers - document.pages.front().layers.size());
    EXPECT_TRUE(mfd::DocumentSemanticValidator {}.Validate(document).empty());

    document.pages.front().editor.layers.emplace_back();
    EXPECT_EQ(DiagnosticCodes(mfd::DocumentSemanticValidator {}.Validate(document)),
              std::vector<std::string> {"MFD022"});
}

TEST(DocumentSemanticValidatorTests, AcceptsAndRejectsAggregatePrimitiveProductBoundary)
{
    const mfd::Primitive validPrimitive = MakePrimitive({}, mfd::PrimitiveType::Line);
    mfd::ReticleGroup first = MakeReticle("first");
    first.visible = false;
    first.primitives.assign(mfd::runtime_validation::kMaxDocumentPrimitives / 2U, validPrimitive);
    mfd::ReticleGroup second = MakeReticle("second");
    second.visible = false;
    second.primitives.assign(
        mfd::runtime_validation::kMaxDocumentPrimitives - first.primitives.size(), validPrimitive);

    mfd::MfdDocument document = MakeDocumentWithReticle(std::move(first));
    document.pages.front().staticReticles.push_back(std::move(second));
    EXPECT_TRUE(mfd::DocumentSemanticValidator {}.Validate(document).empty());

    document.pages.front().staticReticles.back().primitives.push_back(validPrimitive);
    EXPECT_EQ(DiagnosticCodes(mfd::DocumentSemanticValidator {}.Validate(document)),
              std::vector<std::string> {"MFD023"});
}

TEST(DocumentSemanticValidatorTests, AcceptsAndRejectsVisiblePrimitiveBoundary)
{
    const mfd::Primitive validPrimitive = MakePrimitive({}, mfd::PrimitiveType::Line);
    mfd::ReticleGroup reticle = MakeReticle("visible");
    reticle.primitives.assign(mfd::runtime_validation::kMaxDocumentVisiblePrimitives, validPrimitive);
    mfd::MfdDocument document = MakeDocumentWithReticle(std::move(reticle));
    EXPECT_TRUE(mfd::DocumentSemanticValidator {}.Validate(document).empty());

    document.pages.front().staticReticles.front().primitives.push_back(validPrimitive);
    EXPECT_EQ(DiagnosticCodes(mfd::DocumentSemanticValidator {}.Validate(document)),
              std::vector<std::string> {"MFD024"});
}

TEST(DocumentSemanticValidatorTests, AcceptsAndRejectsAggregateStringByteBoundary)
{
    mfd::MfdDocument document;
    document.pages.emplace_back();
    document.pages.front().title.resize(mfd::runtime_validation::kMaxDocumentStringBytes, 'x');
    EXPECT_TRUE(mfd::DocumentSemanticValidator {}.Validate(document).empty());

    document.pages.front().title.push_back('x');
    EXPECT_EQ(DiagnosticCodes(mfd::DocumentSemanticValidator {}.Validate(document)),
              std::vector<std::string> {"MFD025"});
}

TEST(DocumentSemanticValidatorTests, CountsDocumentPathsInAggregateStringBudget)
{
    mfd::MfdDocument document;
    document.sourceFile = std::filesystem::path(
        std::string(mfd::runtime_validation::kMaxDocumentStringBytes + 1U, 'x'));

    EXPECT_EQ(DiagnosticCodes(mfd::DocumentSemanticValidator {}.Validate(document)),
              std::vector<std::string> {"MFD025"});
}

TEST(DocumentSemanticValidatorTests, StopsSemanticChecksAfterCardinalityBudgetFailure)
{
    mfd::MfdDocument document;
    document.pages.resize(mfd::runtime_validation::kMaxDocumentPages + 1U);
    document.pages.front().staticReticles.push_back(MakeReticle("duplicate"));
    document.pages.front().staticReticles.push_back(MakeReticle("DUPLICATE"));

    EXPECT_EQ(DiagnosticCodes(mfd::DocumentSemanticValidator {}.Validate(document)),
              std::vector<std::string> {"MFD019"});
}

TEST(DocumentSemanticValidatorTests, RejectsTemplateAndReticleBoundaryPlusOne)
{
    mfd::MfdDocument templates;
    for (std::size_t index = 0; index < mfd::runtime_validation::kMaxDocumentTemplates; ++index)
    {
        const std::string id = "template_" + std::to_string(index);
        templates.reticleLibrary.emplace(id, MakeReticle(id));
    }
    EXPECT_TRUE(mfd::DocumentSemanticValidator {}.Validate(templates).empty());

    templates.reticleLibrary.emplace("template_over_budget", MakeReticle("template_over_budget"));
    EXPECT_EQ(DiagnosticCodes(mfd::DocumentSemanticValidator {}.Validate(templates)),
              std::vector<std::string> {"MFD020"});

    mfd::MfdDocument reticles;
    reticles.pages.emplace_back();
    reticles.pages.front().staticReticles.resize(
        mfd::runtime_validation::kMaxDocumentReticles);
    EXPECT_TRUE(mfd::DocumentSemanticValidator {}.Validate(reticles).empty());

    reticles.pages.front().staticReticles.emplace_back();
    EXPECT_EQ(DiagnosticCodes(mfd::DocumentSemanticValidator {}.Validate(reticles)),
              std::vector<std::string> {"MFD021"});
}
