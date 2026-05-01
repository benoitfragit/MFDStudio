/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "PackageSmokeUi.h"

#include <cstdint>
#include <vector>

namespace
{
void CompileAgainstClientPackage()
{
    client_api_package_smoke::PackageSmokeUi ui;
    std::vector<mfd::UserCommand> commands = ui.BuildBatch();
    const mfd::CommandBatch batch = ui.BuildCommandBatch(7U);

    mfd::client::WindowDisplay display;
    display.SetBrightness(0.5f);
    display.SetColorInverted(false);
    display.AppendCommands(commands);

    (void)client_api_package_smoke::PackageSmokeUi::WindowFile();
    (void)client_api_package_smoke::PackageSmokeUi::MappingHash();
    (void)batch;
}
} // namespace
